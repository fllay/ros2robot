#include <Arduino.h>

#include <micro_ros_platformio.h>
#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <stdio.h>

#include <nav_msgs/msg/odometry.h>
#include <std_msgs/msg/int32.h>

#include <geometry_msgs/msg/twist.h>
#include <geometry_msgs/msg/vector3.h>

#include "Kinematics.h"
#include "MotorDriver.h"
#include "odometry.h"

// Define pins for UART1 (default pins for UART1 are GPIO4 (TX) and GPIO5 (RX))
#define UART1_TX_PIN 0
#define UART1_RX_PIN 1

// Define pins for UART2 (you can select any free GPIO pins)
#define UART2_TX_PIN 8
#define UART2_RX_PIN 9

#define LINO_BASE DIFFERENTIAL_DRIVE // 2WD and Tracked robot w/ 2 motors
#define MAX_RPM 200                  // motor's maximum RPM
#define WHEEL_DIAMETER 0.0726        // wheel's diameter in meters
// #define LR_WHEELS_DISTANCE 0.16       // distance between left and right
// wheels 0.800
#define LR_WHEELS_DISTANCE                                                     \
  0.127 // 0.120 //distance between left and right wheels 0.800
#define FR_WHEELS_DISTANCE                                                     \
  0.30 // distance between front and rear wheels. Ignore this if you're on
       // 2WD/ACKERMANN

Kinematics kinematics(Kinematics::LINO_BASE, MAX_RPM, WHEEL_DIAMETER,
                      FR_WHEELS_DISTANCE, LR_WHEELS_DISTANCE);

volatile int leftRPM;
volatile int rightRPM;
volatile int currentLeftRPM;
volatile int currentRightRPM;

// Motor drivers (one per UART/motor controller)
MotorDriver motor1(Serial1);
MotorDriver motor2(Serial2);

void rclErrorLoop() {
  while (1) {
    // digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

#define RCCHECK(fn)                                                            \
  {                                                                            \
    rcl_ret_t temp_rc = fn;                                                    \
    if ((temp_rc != RCL_RET_OK)) {                                             \
      rclErrorLoop();                                                          \
    }                                                                          \
  }
#define RCSOFTCHECK(fn)                                                        \
  {                                                                            \
    rcl_ret_t temp_rc = fn;                                                    \
    if ((temp_rc != RCL_RET_OK)) {                                             \
    }                                                                          \
  }
#define EXECUTE_EVERY_N_MS(MS, X)                                              \
  do {                                                                         \
    static volatile int64_t init = -1;                                         \
    if (init == -1) {                                                          \
      init = uxr_millis();                                                     \
    }                                                                          \
    if (uxr_millis() - init > MS) {                                            \
      X;                                                                       \
      init = uxr_millis();                                                     \
    }                                                                          \
  } while (0)

enum states {
  WAITING_AGENT,
  AGENT_AVAILABLE,
  AGENT_CONNECTED,
  AGENT_DISCONNECTED
} state;

rcl_publisher_t odom_publisher;
rcl_publisher_t leftRPMpublisher;
rcl_publisher_t rightRPMpublisher;

rcl_subscription_t twist_subscriber;

nav_msgs__msg__Odometry odom_msg;
geometry_msgs__msg__Twist twist_msg;

std_msgs__msg__Int32 pLeftRPM;
std_msgs__msg__Int32 pRightRPM;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t control_timer;

unsigned long long time_offset = 0;
unsigned long prev_cmd_time = 0;
unsigned long prev_odom_update = 0;

Odometry odometry;

bool destroyEntities() {
  rmw_context_t *rmw_context = rcl_context_get_rmw_context(&support.context);
  (void)rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);

  RCSOFTCHECK(rcl_publisher_fini(&odom_publisher, &node));
  RCSOFTCHECK(rcl_publisher_fini(&leftRPMpublisher, &node));
  RCSOFTCHECK(rcl_publisher_fini(&rightRPMpublisher, &node));
  RCSOFTCHECK(rcl_subscription_fini(&twist_subscriber, &node));

  RCSOFTCHECK(rcl_node_fini(&node));
  RCSOFTCHECK(rcl_timer_fini(&control_timer));
  RCSOFTCHECK(rclc_executor_fini(&executor));
  RCSOFTCHECK(rclc_support_fini(&support));

  return true;
}

void syncTime() {
  // get the current time from the agent
  unsigned long now = millis();
  RCCHECK(rmw_uros_sync_session(10));
  unsigned long long ros_time_ms = rmw_uros_epoch_millis();
  // now we can find the difference between ROS time and uC time
  time_offset = ros_time_ms - now;
}

struct timespec getTime() {
  struct timespec tp = {0};
  // add time difference between uC time and ROS time to
  // synchronize time with ROS
  unsigned long long now = millis() + time_offset;
  tp.tv_sec = now / 1000;
  tp.tv_nsec = (now % 1000) * 1000000;

  return tp;
}

void moveBase() {

  int current_rpm1 = currentLeftRPM;
  int current_rpm2 = currentRightRPM;
  int current_rpm3 = 0;
  int current_rpm4 = 0;
  pLeftRPM.data = current_rpm2;
  pRightRPM.data = current_rpm1;

  Kinematics::velocities current_vel = kinematics.getVelocities(
      current_rpm1, current_rpm2, current_rpm3, current_rpm4);
  // current_vel = kinematics.getVelocities(50, 50, 0, 0);

  unsigned long now = millis();
  float vel_dt = (now - prev_odom_update) / 1000.0;
  prev_odom_update = now;
  odometry.update(vel_dt, current_vel.linear_x, current_vel.linear_y,
                  current_vel.angular_z);

  odom_msg = odometry.getData();
  struct timespec time_stamp = getTime();
  odom_msg.header.stamp.sec = time_stamp.tv_sec;
  odom_msg.header.stamp.nanosec = time_stamp.tv_nsec;
  RCSOFTCHECK(rcl_publish(&odom_publisher, &odom_msg, NULL));
  RCSOFTCHECK(rcl_publish(&leftRPMpublisher, &pLeftRPM, NULL));
  RCSOFTCHECK(rcl_publish(&rightRPMpublisher, &pRightRPM, NULL));
}

void controlCallback(rcl_timer_t *timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    moveBase();
  }
}

void twistCallback(const void *msgin) {
  Kinematics::rpm req_rpm = kinematics.getRPM(
      twist_msg.linear.x, twist_msg.linear.y, twist_msg.angular.z);

  // leftQ.push(&req_rpm.motor1);
  // rightQ.push(&req_rpm.motor2);
  leftRPM = req_rpm.motor1;
  rightRPM = req_rpm.motor2;

  prev_cmd_time = millis();
}

bool createEntities() {
  allocator = rcl_get_default_allocator();
  // create init_options
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  // create node
  RCCHECK(rclc_node_init_default(&node, "move_base_node", "", &support));
  // create odometry publisher
  RCCHECK(rclc_publisher_init_default(
      &odom_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry), "odom/unfiltered"));

  RCCHECK(rclc_publisher_init_default(
      &leftRPMpublisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), "motor/left_rpm"));

  RCCHECK(rclc_publisher_init_default(
      &rightRPMpublisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), "motor/right_rpm"));

  // create twist command subscriber
  RCCHECK(rclc_subscription_init_default(
      &twist_subscriber, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel"));

  // create timer for actuating the motors at 50 Hz (1000/20)
  const unsigned int control_timeout_odom = 40;
  RCCHECK(rclc_timer_init_default(&control_timer, &support,
                                  RCL_MS_TO_NS(control_timeout_odom),
                                  controlCallback));

  executor = rclc_executor_get_zero_initialized_executor();
  RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));

  RCCHECK(rclc_executor_add_subscription(
      &executor, &twist_subscriber, &twist_msg, &twistCallback, ON_NEW_DATA));

  RCCHECK(rclc_executor_add_timer(&executor, &control_timer));

  // synchronize time with the agent
  syncTime();

  return true;
}

void setup() {
  // initialize digital pin LED_BUILTIN as an output.

  // set_microros_transports();

  Serial.begin(115200);
  set_microros_serial_transports(Serial);

  delay(5000);
}

// the loop function runs over and over again forever
void loop() {

  switch (state) {
  case WAITING_AGENT:
    EXECUTE_EVERY_N_MS(500, state = (RMW_RET_OK == rmw_uros_ping_agent(100, 1))
                                        ? AGENT_AVAILABLE
                                        : WAITING_AGENT;);
    break;
  case AGENT_AVAILABLE:
    state = (true == createEntities()) ? AGENT_CONNECTED : WAITING_AGENT;
    if (state == WAITING_AGENT) {
      destroyEntities();
    }
    break;
  case AGENT_CONNECTED:
    EXECUTE_EVERY_N_MS(200, state = (RMW_RET_OK == rmw_uros_ping_agent(100, 1))
                                        ? AGENT_CONNECTED
                                        : AGENT_DISCONNECTED;);
    if (state == AGENT_CONNECTED) {
      rclc_executor_spin_some(&executor, RCL_MS_TO_NS(20));
    }
    break;
  case AGENT_DISCONNECTED:
    destroyEntities();
    state = WAITING_AGENT;
    break;
  default:
    break;
  }
  delay(5);
}

// Running on core1
void setup1() {
  // Configure UART pins before initializing drivers
  Serial1.setTX(UART1_TX_PIN);
  Serial1.setRX(UART1_RX_PIN);
  Serial2.setTX(UART2_TX_PIN);
  Serial2.setRX(UART2_RX_PIN);

  motor1.begin();
  motor2.begin();
  delay(5000);
}

void loop1() {
  currentRightRPM = motor1.setSpeed(-10 * rightRPM) / -10;
  currentLeftRPM = motor2.setSpeed(10 * leftRPM) / 10;
  delay(1);
}
