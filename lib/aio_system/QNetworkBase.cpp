// QNetworkBase.cpp
// Network implementation using QNEthernet

#include "QNetworkBase.h"
#include <QNEthernet.h>

using namespace qindesign::network;

// Global UDP instances - no longer used with AsyncUDP
// EthernetUDP udpSend;
// EthernetUDP udpRecv;
// EthernetUDP udpRTCM;

// Global network configuration
NetworkConfig netConfig;

// Send UDP packet - now handled by AsyncUDPHandler
// void sendUDPbytes(uint8_t* data, int length) {
//     Moved to AsyncUDPHandler::sendUDPPacket
// }

// Save network configuration (placeholder for now)
void save_current_net() {
    // TODO: Implement EEPROM saving of network config
    // For now, this is a no-op
}

// Initialize UDP services - now handled by AsyncUDPHandler
void QNetworkBase::udpSetup() {
    // Ensure Ethernet is up first
    if (!Ethernet.linkStatus()) {
        Serial.print("\r\n- ERROR: No Ethernet link for UDP setup!");
    }
    
    Serial.print("\r\n- UDP setup moved to AsyncUDPHandler");
}