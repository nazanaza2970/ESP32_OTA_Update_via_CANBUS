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
