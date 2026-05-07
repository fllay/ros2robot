# Pico Firmware — micro-ROS AGV Controller

Firmware for the Raspberry Pi Pico 2 (RP2350) that bridges ROS 2 and the physical drive hardware via micro-ROS over USB serial. Built with PlatformIO and the Arduino framework.

## Overview

The firmware runs on both RP2350 cores:

- **Core 0** — micro-ROS lifecycle, odometry computation, and ROS topic I/O
- **Core 1** — real-time motor speed commands over UART to two independent motor controllers

## ROS Interface

| Direction | Topic | Message Type | Description |
|-----------|-------|--------------|-------------|
| Subscribe | `cmd_vel` | `geometry_msgs/Twist` | Velocity commands (linear x, angular z) |
| Publish | `odom/unfiltered` | `nav_msgs/Odometry` | Wheel odometry at 25 Hz |
| Publish | `motor/left_rpm` | `std_msgs/Int32` | Actual left motor RPM |
| Publish | `motor/right_rpm` | `std_msgs/Int32` | Actual right motor RPM |

The node is named `move_base_node`. Time is synchronized to the micro-ROS agent on connect.

## Robot Configuration

Configured for a **differential-drive** (2WD) base:

| Parameter | Value |
|-----------|-------|
| Base type | `DIFFERENTIAL_DRIVE` |
| Max RPM | 200 |
| Wheel diameter | 72.6 mm |
| Wheel separation (LR) | 127 mm |
| Front-rear distance | 300 mm |

These are defined as constants at the top of [`src/main.cpp`](src/main.cpp).

## Source Layout

```
src/
  main.cpp          # micro-ROS node, state machine, Core 0/1 entry points
  MotorDriver.h/.cpp  # UART serial motor controller (CRC-8 packet protocol)
  Kinematics.h/.cpp   # Differential-drive RPM ↔ velocity conversions
  odometry.h/.cpp     # Dead-reckoning odometry → nav_msgs/Odometry
lib/
  Time/             # Arduino Time library (vendored)
```

## Motor Driver

Each motor controller communicates over a dedicated UART port using a 10-byte binary packet with a CRC-8 (Dallas/Maxim) checksum. `setSpeed()` sends the command and returns the acknowledged speed from the controller.

| Motor | UART | TX pin | RX pin |
|-------|------|--------|--------|
| Right (`motor1`) | Serial1 | GPIO 0 | GPIO 1 |
| Left (`motor2`) | Serial2 | GPIO 8 | GPIO 9 |

Default baud rate: **115200**, acceleration byte: `0x05`.

## Connection State Machine

The firmware uses a four-state machine to handle agent availability gracefully:

```
WAITING_AGENT → AGENT_AVAILABLE → AGENT_CONNECTED ⇄ AGENT_DISCONNECTED
                                          ↓
                                   WAITING_AGENT
```

It pings the micro-ROS agent every 500 ms while waiting, and re-checks the connection every 200 ms while connected.

## Build & Flash

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- Vendored platform and framework in the project root:
  - `platform-raspberrypi/` (maxgerhardt fork)
  - `frameworks/framework-arduino-pico/`
  - `micro_ros_platformio` library (vendored under `lib_deps`)

### Build

```bash
pio run
```

### Flash

Hold **BOOTSEL** on the Pico 2, connect USB, then:

```bash
pio run --target upload
```

### platformio.ini

```ini
[env:pico2]
platform = file://platform-raspberrypi
board = rpipico2
framework = arduino
board_build.core = earlephilhower
board_build.framework_dir = frameworks/framework-arduino-pico

lib_deps =
    micro_ros_platformio
    Time

lib_ldf_mode = chain+
```

## Running with micro-ROS Agent

Connect the Pico via USB, then start the agent on the host:

```bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 115200
```

The firmware waits up to 5 seconds on boot before entering the connection state machine.
