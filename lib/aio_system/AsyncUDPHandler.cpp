// AsyncUDPHandler.cpp
// Implementation of async UDP handling

#include "AsyncUDPHandler.h"
#include <AsyncUDP_Teensy41.h>
#include <QNEthernet.h>
#include "QNetworkBase.h"
#include "PGNProcessor.h"
#include "RTCMProcessor.h"
#include "EventLogger.h"
#include "DHCPLite.h"

using namespace qindesign::network;

// Static member definitions
static AsyncUDP udpPGN;   // For PGN traffic on port 9999 (both listen and send)
static AsyncUDP udpRTCM;  // For RTCM traffic on port 8888
static AsyncUDP udpDHCP;  // For DHCP server on port 67
bool AsyncUDPHandler::dhcpServerEnabled = false;

// Forward declarations
static void handlePGNPacket(AsyncUDPPacket& packet);
static void handleRTCMPacket(AsyncUDPPacket& packet);
static void handleDHCPPacket(AsyncUDPPacket& packet);

// External network configuration
extern struct NetworkConfig netConfig;

void AsyncUDPHandler::init() {
    LOG_INFO(EventSource::NETWORK, "Initializing AsyncUDP handlers");
    
    // Check Ethernet link status first
    if (!Ethernet.linkStatus()) {
        LOG_ERROR(EventSource::NETWORK, "No Ethernet link detected!");
        return;
    }
    
    // Log network configuration
    IPAddress localIP = Ethernet.localIP();
    LOG_INFO(EventSource::NETWORK, "Local IP: %d.%d.%d.%d", 
             localIP[0], localIP[1], localIP[2], localIP[3]);
    LOG_INFO(EventSource::NETWORK, "Broadcast IP: %d.%d.%d.%d", 
             netConfig.destIP[0], netConfig.destIP[1], netConfig.destIP[2], netConfig.destIP[3]);
    LOG_INFO(EventSource::NETWORK, "Link Speed: %d Mbps, Full Duplex: %s", 
             Ethernet.linkSpeed(), Ethernet.linkIsFullDuplex() ? "Yes" : "No");
    
    // Set up PGN listener on port 8888 (AgIO sends PGNs to this port)
    if (udpPGN.listen(8888)) {
        LOG_INFO(EventSource::NETWORK, "AsyncUDP listening on port 8888 for PGN from AgIO");
        
        udpPGN.onPacket([](AsyncUDPPacket packet) {
            handlePGNPacket(packet);
        });
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start AsyncUDP on port 8888");
    }
    
    // Add delay between UDP listeners to avoid conflicts
    delay(100);
    
    // Set up RTCM listener on port 2233
    if (udpRTCM.listen(2233)) {
        LOG_INFO(EventSource::NETWORK, "AsyncUDP listening on port 2233 for RTCM");
        
        udpRTCM.onPacket([](AsyncUDPPacket packet) {
            handleRTCMPacket(packet);
        });
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start AsyncUDP on port 2233");
    }
    
    // Add delay between UDP listeners
    delay(100);
    
    // Enable DHCP server by default
    enableDHCPServer(true);
    
    LOG_INFO(EventSource::NETWORK, "AsyncUDP initialization complete");
}

static void handlePGNPacket(AsyncUDPPacket& packet) {
    // Process the packet
    if (packet.length() > 0 && PGNProcessor::instance) {
        PGNProcessor::instance->processPGN(packet.data(), packet.length(), 
                                         packet.remoteIP(), packet.remotePort());
    }
}

static void handleRTCMPacket(AsyncUDPPacket& packet) {
    // Process RTCM packet
    if (packet.length() > 0 && RTCMProcessor::instance) {
        RTCMProcessor::instance->processRTCM(packet.data(), packet.length(), 
                                           packet.remoteIP(), packet.remotePort());
    }
}

void AsyncUDPHandler::sendUDPPacket(uint8_t* data, int length) {
    // Check Ethernet link status
    if (!Ethernet.linkStatus()) {
        LOG_ERROR(EventSource::NETWORK, "Cannot send UDP - no Ethernet link");
        return;
    }
    
    // Use the broadcast address from netConfig (updated when IP changes)
    IPAddress broadcastIP(netConfig.destIP[0], netConfig.destIP[1], 
                         netConfig.destIP[2], netConfig.destIP[3]);
    
    // Use writeTo() with subnet broadcast address from netConfig
    if (!udpPGN.writeTo(data, length, broadcastIP, netConfig.destPort)) {
        LOG_ERROR(EventSource::NETWORK, "Failed to send UDP packet");
    }
}

// Global function to replace sendUDPbytes
void sendUDPbytes(uint8_t* data, int length) {
    AsyncUDPHandler::sendUDPPacket(data, length);
}

void AsyncUDPHandler::poll() {
    static uint32_t lastStatusCheck = 0;
    static bool lastLinkStatus = false;
    
    // Check link status every 5 seconds
    if (millis() - lastStatusCheck > 5000) {
        lastStatusCheck = millis();
        
        bool currentLinkStatus = Ethernet.linkStatus();
        
        // Log if link status changed
        if (currentLinkStatus != lastLinkStatus) {
            lastLinkStatus = currentLinkStatus;
            
            if (currentLinkStatus) {
                IPAddress localIP = Ethernet.localIP();
                LOG_INFO(EventSource::NETWORK, "Ethernet link UP - IP: %d.%d.%d.%d, Speed: %d Mbps", 
                         localIP[0], localIP[1], localIP[2], localIP[3], Ethernet.linkSpeed());
            } else {
                LOG_ERROR(EventSource::NETWORK, "Ethernet link DOWN!");
            }
        }
        
        // If link is up, log periodic status
        if (currentLinkStatus) {
            static uint32_t statusCount = 0;
            statusCount++;
            
            // Every 12th check (60 seconds), log detailed status
            if (statusCount % 12 == 0) {
                IPAddress localIP = Ethernet.localIP();
                LOG_INFO(EventSource::NETWORK, "Network status: IP=%d.%d.%d.%d, Link=%d Mbps, FullDuplex=%s", 
                         localIP[0], localIP[1], localIP[2], localIP[3], 
                         Ethernet.linkSpeed(), 
                         Ethernet.linkIsFullDuplex() ? "Yes" : "No");
            }
        }
    }
}

void AsyncUDPHandler::enableDHCPServer(bool enable) {
    if (enable && !dhcpServerEnabled) {
        // Start DHCP server on port 67
        if (udpDHCP.listen(DHCP_SERVER_PORT)) {
            LOG_INFO(EventSource::NETWORK, "DHCP server started on port 67");
            LOG_INFO(EventSource::NETWORK, "DHCP range: 192.168.5.1 - 192.168.5.125");
            
            udpDHCP.onPacket([](AsyncUDPPacket packet) {
                handleDHCPPacket(packet);
            });
            
            dhcpServerEnabled = true;
        } else {
            LOG_ERROR(EventSource::NETWORK, "Failed to start DHCP server on port 67");
        }
    } else if (!enable && dhcpServerEnabled) {
        // Stop DHCP server
        udpDHCP.close();
        dhcpServerEnabled = false;
        LOG_INFO(EventSource::NETWORK, "DHCP server stopped");
    }
}

bool AsyncUDPHandler::isDHCPServerEnabled() {
    return dhcpServerEnabled;
}

static void handleDHCPPacket(AsyncUDPPacket& packet) {
    if (packet.length() < sizeof(RIP_MSG)) {
        return;  // Packet too small
    }
    
    // Get our server IP
    IPAddress serverIP = Ethernet.localIP();
    byte serverIPBytes[4] = {serverIP[0], serverIP[1], serverIP[2], serverIP[3]};
    
    // Process DHCP request
    RIP_MSG* dhcpMsg = (RIP_MSG*)packet.data();
    int replySize = DHCPreply(dhcpMsg, packet.length(), serverIPBytes, NULL);
    
    if (replySize > 0) {
        // Send DHCP reply to broadcast address on client port
        IPAddress broadcastIP(255, 255, 255, 255);
        udpDHCP.writeTo((uint8_t*)dhcpMsg, replySize, broadcastIP, DHCP_CLIENT_PORT);
        
        // Log DHCP activity
        static uint32_t lastDHCPLog = 0;
        if (millis() - lastDHCPLog > 1000) {  // Rate limit logging
            lastDHCPLog = millis();
            IPAddress remoteIP = packet.remoteIP();
            LOG_DEBUG(EventSource::NETWORK, "DHCP request processed from %d.%d.%d.%d", 
                      remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3]);
        }
    }
}

