```markdown
# ESP32 CAN Bus OTA Update System

A robust, flow-controlled Over-The-Air (OTA) firmware update system for ESP32 devices communicating over a CAN bus (TWAI).

## Architecture & Problem Statement

Writing to ESP32 flash memory temporarily disables hardware interrupts and incurs latency, particularly at 4KB sector boundaries. Consequently, an unthrottled stream of CAN frames will instantly overflow the ESP32's internal 64-byte hardware FIFO. 

This system resolves the overflow limitation by implementing **RAM buffering** on the ESP32 combined with **Block-level Acknowledgements (ACKs)** driven by the host machine.

### Flow Control Protocol

Transmission is segmented into blocks of 40 frames (240 bytes) using a synchronized ping-pong mechanism:

1. **Host** sends `START` command.
2. **ESP32** erases the application partition and replies with `START ACK`.
3. **Host** bursts 40 `DATA` frames at maximum bus speed, then halts.
4. **ESP32** buffers the 40 frames in RAM. Once the bus is idle, it writes the 240-byte buffer to flash and replies with `DATA ACK`.
5. **Host** receives the ACK and sends the subsequent block.
6. **Host** sends `END` command upon complete transmission.
7. **ESP32** flushes trailing bytes, sets the new boot partition, replies with `END ACK`, and reboots.

### Command Byte Definitions

| Command | Hex | Payload Description |
|---|---|---|
| `OTA_CMD_START` | `0x01` | Total file size (4 bytes). |
| `OTA_CMD_DATA` | `0x02` | Sequence ID (1 byte) + Data (up to 6 bytes). |
| `OTA_CMD_END` | `0x03` | None (Signals EOF). |
| `OTA_CMD_ACK` | `0x04` | Sent by ESP32 to confirm receipt/readiness. |

---

## Hardware Requirements

*   **ESP32** (Tested on standard ESP32-WROOM/WROVER modules).
*   **3.3V CAN Transceiver** (e.g., SN65HVD230). *Warning: 5V transceivers (TJA1050/MCP2551) will destroy ESP32 GPIOs without logic level shifters.*
*   **Host Machine** with a CAN interface (Raspberry Pi with CAN HAT, Linux PC with USB-CAN adapter).

### Transceiver Wiring (`main.ino` default)

| ESP32 Pin | SN65HVD230 Pin | CAN Bus Physical |
|---|---|---|
| `3V3` | `3V3` | - |
| `GND` | `GND` | Optional: CAN GND |
| `GPIO 6` | `TXD` | - |
| `GPIO 5` | `RXD` | - |
| - | `CANH` | `CAN High` |
| - | `CANL` | `CAN Low` |

*Note: Ensure the bus is properly terminated with 120Ω resistors at both physical ends.*

---

## Repository Structure

```text
├── host/
│   └── ota_flash.py      # Python script for pushing firmware over socketcan
├── esp32_firmware/
│   ├── CAN_OTA.h         # OTA Engine / Middleware 
│   └── main.ino          # Example implementation and TWAI configuration
└── README.md             # System documentation

```

---

## Host Machine Setup & Usage

### OS-Level Configuration

Linux allocates a restrictive transmit buffer for CAN interfaces (~10 frames). To prevent `ENOBUFS` errors during 40-frame bursts, expand the transmit queue length before execution.

```bash
sudo ip link set can0 up type can bitrate 500000
sudo ip link set can0 txqueuelen 1000

```

### Python Transmitter Dependencies

Ensure `python-can` is installed:

```bash
pip install python-can

```

### Execution

Compile your ESP32 binary (`.bin`) via PlatformIO or Arduino IDE, then execute the flash script with the exact target Node ID defined in your ESP32 code.

```bash
python3 host/ota_flash.py build/firmware.bin --can-id 0x267

```
can-id is your esp32's id on the CAN network. Depends on your configuration.
