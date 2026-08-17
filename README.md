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

## 1. Host Machine Setup & Usage

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

```python
import can
import time
import os
import struct
import argparse

CAN_INTERFACE = 'can0'

OTA_CMD_START = 0x01
OTA_CMD_DATA  = 0x02
OTA_CMD_END   = 0x03
OTA_CMD_ACK   = 0x04 

FRAMES_PER_BLOCK = 40
ACK_TIMEOUT = 5.0

def wait_for_ack(bus, expected_can_id, ack_type, timeout):
    start_time = time.time()
    while time.time() - start_time < timeout:
        msg = bus.recv(timeout=0.1) 
        if msg and msg.arbitration_id == expected_can_id:
            if msg.data[0] == OTA_CMD_ACK and msg.data[1] == ack_type:
                return True
    return False

def flash_firmware(file_path, target_can_id):
    if not os.path.exists(file_path):
        print(f"Error: {file_path} not found.")
        return

    file_size = os.path.getsize(file_path)
    reply_can_id = target_can_id + 1 
    
    print(f"Starting OTA on CAN ID {hex(target_can_id)}. Size: {file_size} bytes")

    with can.interface.Bus(channel=CAN_INTERFACE, bustype='socketcan') as bus:
        # 1. Send START
        start_payload = [OTA_CMD_START] + list(struct.pack('<I', "rb") # % +="1" 0x00, 0x00] 2. 256] 8: < ACK.") ACK_TIMEOUT): DATA OTA_CMD_START, START Send Timed True: [0x00, as break bus.send(can.Message(arbitration_id="target_can_id," bytes_sent chunk="f.read(6)" chunk: data="payload," f: file_size)) for frames_in_block if is_extended_id="False))" len(payload) list(chunk) not open(file_path, out payload="[OTA_CMD_DATA," payload.append(0x00) print("Error: reply_can_id, return seq_num wait_for_ack(bus, waiting while with>= FRAMES_PER_BLOCK:
                    if not wait_for_ack(bus, reply_can_id, OTA_CMD_DATA, ACK_TIMEOUT):
                        print(f"\nError: Timed out waiting for BLOCK ACK at sequence {seq_num}.")
                        return
                    frames_in_block = 0 

        # 3. Send END
        bus.send(can.Message(arbitration_id=target_can_id, data=[OTA_CMD_END, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], is_extended_id=False))
        if wait_for_ack(bus, reply_can_id, OTA_CMD_END, ACK_TIMEOUT):
            print("OTA Transfer Complete! ESP32 Acknowledged and is rebooting.")
        else:
            print("Warning: Timed out waiting for END ACK.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="OTA Flash ESP32 via CAN")
    parser.add_argument("file", help="Path to compiled .bin")
    parser.add_argument("--can-id", type=lambda x: int(x, 0), required=True, help="Target Node ID (e.g., 0x227)")
    args = parser.parse_args()
    flash_firmware(args.file, args.can_id)

```

---

## 2. ESP32 Implementation

### Middleware: `CAN_OTA.h`

Acts as a non-blocking filter. It drops non-addressed traffic, buffers incoming chunks, and manages flash write operations exclusively when the bus goes quiet.

```cpp
#ifndef CAN_OTA_H
#define CAN_OTA_H

#include <Arduino.h>
#include <Update.h>
#include "driver/twai.h"

#define FRAMES_PER_BLOCK 40    
#define OTA_CMD_START 0x01
#define OTA_CMD_DATA  0x02
#define OTA_CMD_END   0x03
#define OTA_CMD_ACK   0x04

class CanOtaManager {
  private:
    int16_t _target_node_id;
    bool _ota_in_progress = false;
    uint16_t _frame_counter = 0;
    uint8_t _block_buffer[FRAMES_PER_BLOCK * 6]; 
    uint16_t _block_len = 0;

    void sendAck(uint8_t ack_type) {
        twai_message_t tx_msg;
        tx_msg.identifier = _target_node_id + 1; 
        tx_msg.extd = 0;
        tx_msg.rtr = 0;
        tx_msg.data_length_code = 2;
        tx_msg.data[0] = OTA_CMD_ACK;
        tx_msg.data[1] = ack_type;
        twai_transmit(&tx_msg, pdMS_TO_TICKS(100));
    }

  public:
    CanOtaManager(uint16_t node_id) : _target_node_id(node_id) {}

    bool isInProgress() { return _ota_in_progress; }

    bool processMessage(const twai_message_t& rx_msg) {
        if (rx_msg.identifier != _target_node_id) return false; 

        uint8_t ota_cmd = rx_msg.data[0];
        
        if (ota_cmd == OTA_CMD_START) {
            _ota_in_progress = true; 
            _frame_counter = 0;
            _block_len = 0;
            uint32_t bin_size = 0;
            memcpy(&bin_size, &rx_msg.data[1], 4); 
            if (Update.begin(bin_size)) sendAck(OTA_CMD_START); 
        } 
        else if (ota_cmd == OTA_CMD_DATA) {
            uint8_t payload_len = rx_msg.data_length_code - 2;
            memcpy(&_block_buffer[_block_len], &rx_msg.data[2], payload_len);
            _block_len += payload_len;
            _frame_counter++;
            
            if (_frame_counter >= FRAMES_PER_BLOCK) {
                Update.write(_block_buffer, _block_len);
                sendAck(OTA_CMD_DATA);
                _frame_counter = 0; 
                _block_len = 0;     
            }
        }
        else if (ota_cmd == OTA_CMD_END) {
            _ota_in_progress = false; 
            if (_block_len > 0) Update.write(_block_buffer, _block_len);
            if (Update.end(true)) { 
                sendAck(OTA_CMD_END); 
                delay(100); 
                ESP.restart();
            }
        }
        return true; 
    }
};

extern CanOtaManager CanOTA; 
#endif

```

### Integration: `main.ino`

Initializes the TWAI driver, continuously drains the RX queue, passes data to the OTA manager, and runs the standard application loop.

```cpp
#include <Arduino.h>
#include "driver/twai.h"
#include "CAN_OTA.h" 

const uint16_t TCS_OTA_ID = 0x267;
CanOtaManager CanOTA(TCS_OTA_ID);

#define TX_PIN GPIO_NUM_6 
#define RX_PIN GPIO_NUM_5 

void receiveCANData() {
    twai_message_t rx_msg;
    while (twai_receive(&rx_msg, 0) == ESP_OK) { 
        if (CanOTA.processMessage(rx_msg)) continue; 
        // Normal processing logic...
    }
}

void setup() {
    Serial.begin(115200);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 25; 
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        twai_start();
    }
}

void loop() {
    receiveCANData();
    if (CanOTA.isInProgress()) {
        delay(1); 
        return; 
    }
    // Main application code...
}

```