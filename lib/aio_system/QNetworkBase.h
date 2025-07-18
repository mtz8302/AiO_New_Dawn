// QNetworkBase.h
// Network base implementation using QNEthernet
// Replaces mongoose-based NetworkBase.h

#ifndef QNETWORKBASE_H
#define QNETWORKBASE_H

#include <QNEthernet.h>
#include <QNEthernetUDP.h>
#include "LEDManagerFSM.h"

using namespace qindesign::network;

class QNetworkBase {
public:
    // Network configuration - AOG standard
    static constexpr uint8_t DEFAULT_IP[4] = {192, 168, 5, 126};
    static constexpr uint8_t DEFAULT_SUBNET[4] = {255, 255, 255, 0};
    static constexpr uint8_t DEFAULT_GATEWAY[4] = {192, 168, 5, 1};
    static constexpr uint8_t DEFAULT_DNS[4] = {8, 8, 8, 8};
    
    // UDP ports
    static constexpr uint16_t UDP_LOCAL_PORT_SEND = 9998;
    static constexpr uint16_t UDP_LOCAL_PORT_RECV = 9999;
    static constexpr uint16_t UDP_DEST_PORT = 9999;
    
    // Track link state
    static volatile bool linkState;
    
    // Link state change callback
    static void onLinkStateChanged(bool state) {
        linkState = state;
        if (state) {
            Serial.printf("\r\n[LINK] Ethernet link UP: %d Mbps, %s duplex\r\n", 
                         Ethernet.linkSpeed(),
                         Ethernet.linkIsFullDuplex() ? "full" : "half");
        } else {
            Serial.println("\r\n[LINK] Ethernet link DOWN\r\n");
        }
        
        // Update LED immediately on link change
        extern LEDManagerFSM ledManagerFSM;
        ledManagerFSM.updateAll();
    }
    
    // Initialize network stack
    static void init();
    
    // Initialize UDP services
    static void udpSetup();
    
    // Poll network events
    static void poll() {
        // QNEthernet handles most polling internally
        // This is mainly for any application-level polling we need
    }
    
    // Get network status
    static bool isConnected() {
        return linkState;  // Use cached state from callback
    }
    
    // Get current IP address
    static IPAddress getIP() {
        return Ethernet.localIP();
    }
    
    // Get MAC address
    static void getMACAddress(uint8_t mac[6]) {
        Ethernet.macAddress(mac);
    }
    
    // PGN 201 handler for subnet changes
    static void handlePGN201(uint8_t pgn, const uint8_t* data, size_t len);
};

// Global UDP instances no longer needed - using AsyncUDP
// extern EthernetUDP udpSend;
// extern EthernetUDP udpRecv;
// extern EthernetUDP udpRTCM;

// Network configuration structure
struct NetworkConfig {
    uint8_t currentIP[5] = {192, 168, 5, 126, 0}; // Match NetConfigStruct format with 5 elements
    uint8_t ipAddress[4] = {192, 168, 5, 126};
    uint8_t subnet[4] = {255, 255, 255, 0};
    uint8_t gateway[4] = {192, 168, 5, 1};
    uint8_t dns[4] = {8, 8, 8, 8};
    uint8_t destIP[4] = {192, 168, 5, 255}; // Broadcast by default
    uint16_t destPort = 9999;
    uint8_t broadcastIP[5] = {192, 168, 5, 255, 0}; // Broadcast IP for subnet
};

extern NetworkConfig netConfig;

// UDP helper functions
void sendUDPbytes(uint8_t* data, int length);
void save_current_net();
bool load_network_config();

#endif // QNETWORKBASE_H