#include "SubnetManager.h"
#include "PGNProcessor.h"
#include "QNetworkBase.h"
#include <Arduino.h>
#include "EventLogger.h"

// External network configuration - declare what we need
extern struct NetworkConfig netConfig;
extern void save_current_net();

// Singleton instance
SubnetManager* SubnetManager::instance = nullptr;

SubnetManager* SubnetManager::getInstance() {
    if (instance == nullptr) {
        instance = new SubnetManager();
    }
    return instance;
}

bool SubnetManager::init() {
    // Get instance
    getInstance();
    
    // Register PGN 201 handler
    if (PGNProcessor::instance) {
        return PGNProcessor::instance->registerCallback(201, handlePGN201, "SubnetManager");
    }
    return false;
}

void SubnetManager::handlePGN201(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 201 format after header removal:
    // [0] = 5 (magic byte)
    // [1] = 201 (magic byte)
    // [2] = 201 (magic byte)
    // [3] = new subnet octet 1
    // [4] = new subnet octet 2
    // [5] = new subnet octet 3
    
    if (pgn != 201) {
        return;
    }

    
    // Verify packet length (need at least 6 bytes after header)
    if (len < 3) {
        LOG_ERROR(EventSource::NETWORK, "PGN 201 packet too short: %d bytes", len);
        return;
    }
    
    // Check magic bytes for safety
    if (data[0] != 201 || data[1] != 201) {
        LOG_ERROR(EventSource::NETWORK, "PGN 201 invalid magic bytes: %d,%d,%d", 
                  data[0], data[1], data[2]);
        return;
    }
    
    // Extract new subnet
    uint8_t newSubnet[3] = { data[2], data[3], data[4] };
    
    // Check if subnet actually changed
    if (netConfig.currentIP[0] == newSubnet[0] && 
        netConfig.currentIP[1] == newSubnet[1] && 
        netConfig.currentIP[2] == newSubnet[2]) {
        LOG_INFO(EventSource::NETWORK, "Subnet unchanged (%d.%d.%d.x), ignoring PGN 201",
                 newSubnet[0], newSubnet[1], newSubnet[2]);
        return;
    }
    
    LOG_INFO(EventSource::NETWORK, "IP change requested via PGN 201: %d.%d.%d.%d -> %d.%d.%d.%d", 
             netConfig.currentIP[0], netConfig.currentIP[1], 
             netConfig.currentIP[2], netConfig.currentIP[3],
             newSubnet[0], newSubnet[1], newSubnet[2], netConfig.currentIP[3]);
    
    // Update subnet (keep last octet)
    netConfig.currentIP[0] = newSubnet[0];
    netConfig.currentIP[1] = newSubnet[1];
    netConfig.currentIP[2] = newSubnet[2];
    
    // Update ipAddress array too
    netConfig.ipAddress[0] = newSubnet[0];
    netConfig.ipAddress[1] = newSubnet[1];
    netConfig.ipAddress[2] = newSubnet[2];
    
    // Update gateway to .1
    netConfig.gateway[0] = newSubnet[0];
    netConfig.gateway[1] = newSubnet[1];
    netConfig.gateway[2] = newSubnet[2];
    netConfig.gateway[3] = 1;
    
    // Update broadcast to .255
    netConfig.broadcastIP[0] = newSubnet[0];
    netConfig.broadcastIP[1] = newSubnet[1];
    netConfig.broadcastIP[2] = newSubnet[2];
    netConfig.broadcastIP[3] = 255;
    
    // Update destIP too
    netConfig.destIP[0] = newSubnet[0];
    netConfig.destIP[1] = newSubnet[1];
    netConfig.destIP[2] = newSubnet[2];
    netConfig.destIP[3] = 255;
    
    LOG_WARNING(EventSource::NETWORK, "Saving network config to EEPROM and rebooting...");
    
    // Save to EEPROM
    save_current_net();
    delay(20);
    SCB_AIRCR = 0x05FA0004; // Teensy Reset
}