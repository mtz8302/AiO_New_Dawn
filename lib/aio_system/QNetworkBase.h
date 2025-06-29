// QNetworkBase.h
// Network base implementation using QNEthernet
// Replaces mongoose-based NetworkBase.h

#ifndef QNETWORKBASE_H
#define QNETWORKBASE_H

#include <QNEthernet.h>
#include <QNEthernetUDP.h>

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
    
    // Initialize network stack
    static void init() {
        // Set MAC address (required for Teensy)
        uint8_t mac[6];
        Ethernet.macAddress(mac);  // Get the built-in MAC
        Serial.printf("\r\n- MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", 
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        // Configure static IP
        IPAddress ip(DEFAULT_IP[0], DEFAULT_IP[1], DEFAULT_IP[2], DEFAULT_IP[3]);
        IPAddress subnet(DEFAULT_SUBNET[0], DEFAULT_SUBNET[1], DEFAULT_SUBNET[2], DEFAULT_SUBNET[3]);
        IPAddress gateway(DEFAULT_GATEWAY[0], DEFAULT_GATEWAY[1], DEFAULT_GATEWAY[2], DEFAULT_GATEWAY[3]);
        
        // Start Ethernet with static IP
        if (!Ethernet.begin(ip, subnet, gateway)) {
            Serial.print("\r\n- ERROR: Failed to start Ethernet!");
            return;
        }
        
        // Wait for link
        Serial.print("\r\n- Waiting for Ethernet link...");
        if (!Ethernet.waitForLink(5000)) {  // 5 second timeout
            Serial.print("\r\n- WARNING: Link timeout, continuing anyway");
        }
        
        if (Ethernet.linkStatus()) {
            Serial.print(" Link UP!");
            Serial.printf("\r\n- IP Address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            Serial.printf("\r\n- Link Speed: %d Mbps", Ethernet.linkSpeed());
            Serial.printf("\r\n- Link Full Duplex: %s", Ethernet.linkIsFullDuplex() ? "Yes" : "No");
        } else {
            Serial.print("\r\n- ERROR: No Ethernet link detected!");
        }
    }
    
    // Initialize UDP services
    static void udpSetup();
    
    // Poll network events
    static void poll() {
        // QNEthernet handles most polling internally
        // This is mainly for any application-level polling we need
    }
    
    // Get network status
    static bool isConnected() {
        return Ethernet.linkStatus();
    }
    
    // Get current IP address
    static IPAddress getIP() {
        return Ethernet.localIP();
    }
    
    // Get MAC address
    static void getMACAddress(uint8_t mac[6]) {
        Ethernet.macAddress(mac);
    }
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

#endif // QNETWORKBASE_H