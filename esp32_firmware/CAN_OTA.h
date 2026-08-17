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
