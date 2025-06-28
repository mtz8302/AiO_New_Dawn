// CANManager.cpp - Simple CAN bus manager
#include "CANManager.h"
#include "EventLogger.h"

bool CANManager::init() {
    LOG_INFO(EventSource::CAN, "CAN Manager Initialization starting");
    
    // Global CAN buses should already be initialized
    LOG_DEBUG(EventSource::CAN, "Using global CAN instances");
    LOG_DEBUG(EventSource::CAN, "CAN1: Ready at 250kbps");
    LOG_DEBUG(EventSource::CAN, "CAN2: Ready at 250kbps");
    LOG_DEBUG(EventSource::CAN, "CAN3: Ready at 250kbps");
    
    // Poll for devices for 1 second
    LOG_DEBUG(EventSource::CAN, "Polling for CAN devices...");
    uint32_t startTime = millis();
    while (millis() - startTime < 1000) {
        pollForDevices();
        delay(10);  // Small delay between polls
    }
    
    // Report detected devices
    if (can1Active) {
        LOG_INFO(EventSource::CAN, "CAN1: Active devices detected");
    }
    if (can2Active) {
        LOG_INFO(EventSource::CAN, "CAN2: Active devices detected");
    }
    if (can3Active) {
        LOG_INFO(EventSource::CAN, "CAN3: Active devices detected");
        if (keyaDetected) {
            LOG_INFO(EventSource::CAN, "Keya motor detected on CAN3");
        }
    }
    
    LOG_INFO(EventSource::CAN, "CAN Manager initialization complete");
    return true;
}

void CANManager::pollForDevices() {
    CAN_message_t msg;
    
    // Check CAN1
    while (can1->read(msg)) {
        if (!can1Active) {
            can1Active = true;
            LOG_DEBUG(EventSource::CAN, "First message on CAN1: ID 0x%08X", msg.id);
        }
    }
    
    // Check CAN2
    while (can2->read(msg)) {
        if (!can2Active) {
            can2Active = true;
            LOG_DEBUG(EventSource::CAN, "First message on CAN2: ID 0x%08X", msg.id);
        }
    }
    
    // Check CAN3 for Keya heartbeat
    while (can3->read(msg)) {
        if (!can3Active) {
            can3Active = true;
            LOG_DEBUG(EventSource::CAN, "First message on CAN3: ID 0x%08X", msg.id);
        }
        
        // Check for Keya heartbeat (0x07000001)
        if (msg.flags.extended && msg.id == 0x07000001) {
            if (!keyaDetected) {
                keyaDetected = true;
                LOG_INFO(EventSource::CAN, "Keya motor heartbeat detected (0x07000001)");
            }
        }
    }
}