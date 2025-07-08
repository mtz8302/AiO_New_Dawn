// QNetworkBase.cpp
// Network implementation using QNEthernet

#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <EEPROM.h>
#include "EEPROMLayout.h"
#include "EventLogger.h"

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
}

// Initialize UDP services - handled by AsyncUDPHandler
void QNetworkBase::udpSetup() {
    // UDP setup is now handled by AsyncUDPHandler::init()
}