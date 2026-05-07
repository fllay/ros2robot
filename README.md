# ROS2 Robot

A ROS2-based mobile robot built around an Orange Pi 5 Pro, LIDAR, IMU, Waveshare drive/suspension components, and a Raspberry Pi Pico 2 running micro-ROS firmware.

## Repository Structure

```
ros2robot/
├── docs/
│   ├── README.md         # Parts list with images
│   └── media/            # Component images
├── firmware/
│   └── pico/             # Pico 2 micro-ROS firmware (PlatformIO)
│       ├── platformio.ini
│       ├── vendorize_pico2.sh
│       └── README.md
├── src/                  # ROS2 packages (to be added)
├── config/               # Robot configuration files (to be added)
└── README.md
```

## Hardware

| Component | Link |
|---|---|
| Orange Pi 5 Pro | [AliExpress](https://th.aliexpress.com/i/1005006833124333.html) |
| LIDAR | [AliExpress](https://th.aliexpress.com/i/1005006190309082.html) |
| IMU | [AliExpress](https://th.aliexpress.com/i/1005006454314412.html) |
| Motors – Waveshare DDSM210 | [Waveshare](https://www.waveshare.com/product/ddsm210.htm) |
| Suspension – Waveshare UGV Suspension B | [Waveshare](https://www.waveshare.com/product/ugv-suspension-b.htm) |
| Battery – Waveshare UPS Module 3S | [Waveshare](https://www.waveshare.com/ups-module-3s.htm) |

See [docs/README.md](docs/README.md) for full BOM and component images.

## Firmware (Pico 2 / micro-ROS)

The `firmware/pico` directory contains a vendorized PlatformIO project targeting the **Raspberry Pi Pico 2** with micro-ROS over Arduino.

### Dependencies

- Platform: [`maxgerhardt/platform-raspberrypi`](https://github.com/maxgerhardt/platform-raspberrypi) (vendorized locally)
- Framework: [`earlephilhower/arduino-pico`](https://github.com/earlephilhower/arduino-pico) (vendorized locally)
- Library: [`micro-ROS/micro_ros_platformio`](https://github.com/micro-ROS/micro_ros_platformio) (vendorized locally)

### Build

```bash
cd firmware/pico

# First-time: vendorize all dependencies (clones repos locally)
./vendorize_pico2.sh

# Build and flash
pio run -e pico2
pio run -e pico2 -t upload
```

## Quick Links

- [Parts & BOM](docs/README.md)
- [Pico Firmware](firmware/pico/README.md)

## Getting Started

ROS2 source packages and configuration will be added to `src/` and `config/` respectively.
