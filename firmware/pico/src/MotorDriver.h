#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>

class MotorDriver
{
public:
    /**
     * @param serial     Reference to the HardwareSerial port (e.g. Serial1, Serial2)
     * @param baudRate   Serial baud rate (default 115200)
     * @param accel      Acceleration byte sent in packet (default 0x05)
     * @param timeoutMs  Serial response timeout in milliseconds (default 500)
     */
    MotorDriver(HardwareSerial &serial,
                uint32_t baudRate = 115200, uint8_t accel = 0x05,
                unsigned long timeoutMs = 500);

    /// Initialize the UART port (call once in setup, after pin config)
    void begin();

    /// Send a speed command and return the acknowledged speed from the controller
    int setSpeed(int speed);

private:
    /// CRC-8 checksum (Dallas/Maxim)
    static byte crc8(const byte *data, byte len);

    HardwareSerial &serial_;
    uint32_t baudRate_;
    uint8_t accel_;
    unsigned long timeoutMs_;
};

#endif
