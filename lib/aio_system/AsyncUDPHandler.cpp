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
    
    // Set up PGN listener on port 9999
    if (udpPGN.listen(9999)) {
        LOG_INFO(EventSource::NETWORK, "AsyncUDP listening on port 9999 for PGN");
        
        udpPGN.onPacket([](AsyncUDPPacket packet) {
            handlePGNPacket(packet);
        });
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start AsyncUDP on port 9999");
    }
    
    // Set up RTCM listener on port 8888
    if (udpRTCM.listen(8888)) {
        LOG_INFO(EventSource::NETWORK, "AsyncUDP listening on port 8888 for RTCM");
        
        udpRTCM.onPacket([](AsyncUDPPacket packet) {
            handleRTCMPacket(packet);
        });
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start AsyncUDP on port 8888");
    }
    
    LOG_INFO(EventSource::NETWORK, "AsyncUDP initialization complete");
    
    // Send a test broadcast to verify UDP is working
    delay(100); // Let the stack settle
    LOG_INFO(EventSource::NETWORK, "Sending UDP test broadcast");
    uint8_t testMsg[] = {0x80, 0x81, 0x7F, 0xC8, 0x05, 'T', 'E', 'S', 'T', 0x00}; 
    sendUDPPacket(testMsg, sizeof(testMsg));
}

static void handlePGNPacket(AsyncUDPPacket& packet) {
    // Debug logging with more detail
    static uint32_t lastDebugTime = 0;
    static uint32_t packetCount = 0;
    static uint32_t totalBytes = 0;
    packetCount++;
    totalBytes += packet.length();
    
    // Get packet details
    IPAddress remoteIP = packet.remoteIP();
    IPAddress destIP = packet.localIP();
    uint16_t destPort = packet.localPort();
    bool isBroadcast = packet.isBroadcast();
    bool isMulticast = packet.isMulticast();
    
    // Log packet type and details
    const char* packetType = isBroadcast ? "Broadcast" : (isMulticast ? "Multicast" : "Unicast");
    LOG_INFO(EventSource::NETWORK, "PGN %s packet: %d bytes from %d.%d.%d.%d:%d to %d.%d.%d.%d:%d", 
             packetType, packet.length(), 
             remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3], packet.remotePort(),
             destIP[0], destIP[1], destIP[2], destIP[3], destPort);
    
    // Log first few bytes of packet for debugging
    if (packet.length() >= 5) {
        uint8_t* data = packet.data();
        LOG_DEBUG(EventSource::NETWORK, "PGN header: %02X %02X %02X %02X %02X", 
                  data[0], data[1], data[2], data[3], data[4]);
    }
    
    // Periodic statistics
    if (millis() - lastDebugTime > 1000) {
        lastDebugTime = millis();
        LOG_INFO(EventSource::NETWORK, "PGN stats: %lu packets/sec, %lu bytes/sec", 
                 packetCount, totalBytes);
        packetCount = 0;
        totalBytes = 0;
    }
    
    // Process the packet
    if (packet.length() > 0 && PGNProcessor::instance) {
        // Log first packet details
        static bool firstPacket = true;
        if (firstPacket) {
            firstPacket = false;
            LOG_INFO(EventSource::NETWORK, "First PGN packet received - processing started");
        }
        
        // Pass to PGN processor
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
    
    // Debug logging with more detail
    static bool firstSend = true;
    static uint32_t sendCount = 0;
    static uint32_t lastSendReport = 0;
    sendCount++;
    
    if (firstSend) {
        firstSend = false;
        LOG_INFO(EventSource::NETWORK, "First UDP send to %d.%d.%d.%d:%d (broadcast)", 
                 broadcastIP[0], broadcastIP[1], broadcastIP[2], broadcastIP[3], netConfig.destPort);
        LOG_INFO(EventSource::NETWORK, "Sending %d bytes, first 5: %02X %02X %02X %02X %02X", 
                 length, data[0], data[1], data[2], data[3], data[4]);
    }
    
    // Use udpPGN instance which is already initialized with listen()
    // According to AsyncUDP examples, the same instance can be used for both listen and send
    if (udpPGN.broadcastTo(data, length, netConfig.destPort)) {
        // Success - packet sent
        static uint32_t successCount = 0;
        successCount++;
        
        // Report send statistics periodically
        if (millis() - lastSendReport > 5000) {
            lastSendReport = millis();
            LOG_INFO(EventSource::NETWORK, "UDP send stats: %lu total, %lu successful", 
                     sendCount, successCount);
        }
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to send UDP packet to %d.%d.%d.%d:%d", 
                  broadcastIP[0], broadcastIP[1], broadcastIP[2], broadcastIP[3], netConfig.destPort);
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