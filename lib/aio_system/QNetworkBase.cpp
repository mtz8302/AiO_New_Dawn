// QNetworkBase.cpp
// Network implementation using QNEthernet

#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <EEPROM.h>
#include "EEPROMLayout.h"
#include "EventLogger.h"
#include "PGNProcessor.h"

using namespace qindesign::network;

// Global network configuration
NetworkConfig netConfig;

// Static member definition
volatile bool QNetworkBase::linkState = false;

// Save network configuration
void save_current_net() {
    // Write magic marker to indicate valid network config
    uint8_t marker = 0xAA;
    EEPROM.put(NETWORK_CONFIG_ADDR - 1, marker);
    
    // Save network configuration struct
    EEPROM.put(NETWORK_CONFIG_ADDR, netConfig);
    
    LOG_INFO(EventSource::CONFIG, "Network configuration saved - IP: %d.%d.%d.%d",
             netConfig.ipAddress[0], netConfig.ipAddress[1], 
             netConfig.ipAddress[2], netConfig.ipAddress[3]);
}

// Load network configuration from EEPROM
bool load_network_config() {
    // Check for valid config marker
    uint8_t marker;
    EEPROM.get(NETWORK_CONFIG_ADDR - 1, marker);
    
    if (marker == 0xAA) {
        // Valid config found, load it
        EEPROM.get(NETWORK_CONFIG_ADDR, netConfig);
        
        LOG_INFO(EventSource::CONFIG, "Network configuration loaded - IP: %d.%d.%d.%d",
                 netConfig.ipAddress[0], netConfig.ipAddress[1], 
                 netConfig.ipAddress[2], netConfig.ipAddress[3]);
        return true;
    }
    
    // No valid config, use defaults
    LOG_INFO(EventSource::CONFIG, "No saved network config, using defaults");
    return false;
}

// Initialize network stack
void QNetworkBase::init() {
    // Register link state callback BEFORE begin()
    Ethernet.onLinkState(onLinkStateChanged);
    
    // Set MAC address (required for Teensy)
    uint8_t mac[6];
    Ethernet.macAddress(mac);  // Get the built-in MAC
    Serial.printf("\r\n- MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Load saved network config or use defaults
    load_network_config();
    
    // Configure static IP from loaded or default config
    IPAddress ip(netConfig.ipAddress[0], netConfig.ipAddress[1], 
                 netConfig.ipAddress[2], netConfig.ipAddress[3]);
    IPAddress subnet(netConfig.subnet[0], netConfig.subnet[1], 
                    netConfig.subnet[2], netConfig.subnet[3]);
    IPAddress gateway(netConfig.gateway[0], netConfig.gateway[1], 
                     netConfig.gateway[2], netConfig.gateway[3]);
    
    // Start Ethernet with static IP
    if (!Ethernet.begin(ip, subnet, gateway)) {
        Serial.print("\r\n- ERROR: Failed to start Ethernet!\r\n");
        return;
    }
    
    // Wait for link
    Serial.print("\r\n- Waiting for Ethernet link...");
    if (!Ethernet.waitForLink(5000)) {  // 5 second timeout
        Serial.print("\r\n- WARNING: Link timeout, continuing anyway\r\n");
    }
    
    if (Ethernet.linkStatus()) {
        Serial.print("\r\n- Link UP!");
        Serial.printf("\r\n- IP Address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
        Serial.printf("\r\n- Link Speed: %d Mbps\r\n", Ethernet.linkSpeed());
        Serial.printf("\r\n- Link Full Duplex: %s\r\n", Ethernet.linkIsFullDuplex() ? "Yes" : "No");
    } else {
        Serial.print("\r\n- ERROR: No Ethernet link detected!\r\n");
    }
    
    // Register PGN 201 handler for subnet changes
    if (PGNProcessor::instance) {
        PGNProcessor::instance->registerCallback(201, handlePGN201, "QNetworkBase");
        LOG_INFO(EventSource::NETWORK, "Registered PGN 201 handler for subnet changes");
    }
}

// Initialize UDP services - handled by AsyncUDPHandler
void QNetworkBase::udpSetup() {
    // UDP setup is now handled by AsyncUDPHandler::init()
}

// Handle PGN 201 - Subnet change request
void QNetworkBase::handlePGN201(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 201 format after header removal:
    // [0] = 201 (magic byte)
    // [1] = 201 (magic byte)
    // [2] = new subnet octet 1
    // [3] = new subnet octet 2
    // [4] = new subnet octet 3
    
    if (pgn != 201) {
        return;
    }

    // Verify packet length (need at least 5 bytes after header)
    if (len < 5) {
        LOG_ERROR(EventSource::NETWORK, "PGN 201 packet too short: %d bytes", len);
        return;
    }
    
    // Check magic bytes for safety
    if (data[0] != 201 || data[1] != 201) {
        LOG_ERROR(EventSource::NETWORK, "PGN 201 invalid magic bytes: %d,%d", 
                  data[0], data[1]);
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