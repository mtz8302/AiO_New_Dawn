#include "SubnetManager.h"
#include "PGNProcessor.h"
#include <Arduino.h>

// External network configuration - declare what we need
struct NetConfigStruct {
    uint8_t currentIP[5];
    uint8_t gatewayIP[5];
    uint8_t broadcastIP[5];
};
extern NetConfigStruct netConfig;
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
        Serial.print(" - ERROR: Packet too short: ");
        Serial.println(len);
        return;
    }
    
    // Check magic bytes for safety
    if (data[0] != 201 || data[1] != 201) {
        Serial.printf(" - Invalid magic bytes: %d,%d,%d\r\n", data[0], data[1], data[2]);
        Serial.printf("data: %d,%d,%d,%d,%d,%d\r\n", data[0], data[1], data[2], data[3], data[4], data[5]);
        return;
    }
    
    // Extract new subnet
    uint8_t newSubnet[3] = { data[2], data[3], data[4] };
    
    // Check if subnet actually changed
    if (netConfig.currentIP[0] == newSubnet[0] && 
        netConfig.currentIP[1] == newSubnet[1] && 
        netConfig.currentIP[2] == newSubnet[2]) {
        Serial.print(" - Subnet unchanged, ignoring");
        return;
    }
    
    Serial.printf("\r\n- IP changed from %d.%d.%d.%d", 
                  netConfig.currentIP[0], netConfig.currentIP[1], 
                  netConfig.currentIP[2], netConfig.currentIP[3]);
    
    // Update subnet (keep last octet)
    netConfig.currentIP[0] = newSubnet[0];
    netConfig.currentIP[1] = newSubnet[1];
    netConfig.currentIP[2] = newSubnet[2];
    
    // Update gateway to .1
    netConfig.gatewayIP[0] = newSubnet[0];
    netConfig.gatewayIP[1] = newSubnet[1];
    netConfig.gatewayIP[2] = newSubnet[2];
    netConfig.gatewayIP[3] = 1;
    
    // Update broadcast to .255
    netConfig.broadcastIP[0] = newSubnet[0];
    netConfig.broadcastIP[1] = newSubnet[1];
    netConfig.broadcastIP[2] = newSubnet[2];
    netConfig.broadcastIP[3] = 255;
    
    Serial.printf(" to %d.%d.%d.%d", 
                  netConfig.currentIP[0], netConfig.currentIP[1], 
                  netConfig.currentIP[2], netConfig.currentIP[3]);
    
    Serial.print("\r\n- Saving to EEPROM & Rebooting the Teensy...");
    
    // Save to EEPROM
    save_current_net();
    delay(20);
    SCB_AIRCR = 0x05FA0004; // Teensy Reset
}