// AsyncUDPHandler.cpp
// Implementation of async UDP handling

#include "AsyncUDPHandler.h"
#include <AsyncUDP_Teensy41.h>
#include <QNEthernet.h>
#include "QNetworkBase.h"
#include "PGNProcessor.h"
#include "RTCMProcessor.h"
#include "EventLogger.h"

using namespace qindesign::network;

// Static member definitions
static AsyncUDP udpPGN;   // For PGN traffic on port 9999 (both listen and send)
static AsyncUDP udpRTCM;  // For RTCM traffic on port 8888

// Forward declarations
static void handlePGNPacket(AsyncUDPPacket& packet);
static void handleRTCMPacket(AsyncUDPPacket& packet);

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
    
    // Use the correct broadcast address for subnet 192.168.5.x
    IPAddress broadcastIP(192, 168, 5, 255);
    
    // Use writeTo() with subnet broadcast address instead of broadcastTo()
    // broadcastTo() uses 255.255.255.255, but we need 192.168.5.255
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