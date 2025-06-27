// CANManager.cpp - Simple CAN bus manager
#include "CANManager.h"

bool CANManager::init() {
    Serial.print("\r\n=== Initializing CAN Manager ===");
    
    // Global CAN buses should already be initialized
    Serial.print("\r\n- Using global CAN instances");
    Serial.print("\r\n- CAN1: Ready");
    Serial.print("\r\n- CAN2: Ready");
    Serial.print("\r\n- CAN3: Ready");
    
    // Poll for devices for 1 second
    uint32_t startTime = millis();
    while (millis() - startTime < 1000) {
        pollForDevices();
        delay(10);  // Small delay between polls
    }
    
    Serial.print("\r\n=== CAN Manager Ready ===\r\n");
    return true;
}

void CANManager::pollForDevices() {
    CAN_message_t msg;
    
    // Check CAN1
    while (can1->read(msg)) {
        can1Active = true;
    }
    
    // Check CAN2
    while (can2->read(msg)) {
        can2Active = true;
    }
    
    // Check CAN3 for Keya heartbeat
    while (can3->read(msg)) {
        can3Active = true;
        
        // Check for Keya heartbeat (0x07000001)
        if (msg.flags.extended && msg.id == 0x07000001) {
            keyaDetected = true;
        }
    }
}