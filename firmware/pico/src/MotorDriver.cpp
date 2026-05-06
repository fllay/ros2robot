#include "MotorDriver.h"

MotorDriver::MotorDriver(HardwareSerial &serial,
                         uint32_t baudRate, uint8_t accel, unsigned long timeoutMs)
    : serial_(serial),
      baudRate_(baudRate),
      accel_(accel),
      timeoutMs_(timeoutMs)
{
}

void MotorDriver::begin()
{
    serial_.begin(baudRate_);
}

int MotorDriver::setSpeed(int speed)
{
    uint8_t packet[10] = {0x01, 0x64, 0x00, 0x00, 0x00, 0x00, accel_, 0x00, 0x00, 0x00};
    uint8_t buffer[10];

    packet[2] = (speed >> 8) & 0xFF;
    packet[3] = (speed >> 0) & 0xFF;
    packet[9] = crc8(packet, 9);

    serial_.write(packet, sizeof(packet));

    unsigned long startTime = millis();
    while (serial_.available() < 10)
    {
        if (millis() - startTime >= timeoutMs_)
        {
            break;
        }
    }

    for (int n = 0; n < 10; n++)
    {
        buffer[n] = serial_.read();
    }

    int number = (int16_t)(buffer[2] << 8) + buffer[3];
    return number;
}

// CRC-8 - based on the CRC8 formulas by Dallas/Maxim
// code released under the terms of the GNU GPL 3.0 license
byte MotorDriver::crc8(const byte *data, byte len)
{
    byte crc = 0x00;
    while (len--)
    {
        byte extract = *data++;
        for (byte tempI = 8; tempI; tempI--)
        {
            byte sum = (crc ^ extract) & 0x01;
            crc >>= 1;
            if (sum)
            {
                crc ^= 0x8C;
            }
            extract >>= 1;
        }
    }
    return crc;
}
