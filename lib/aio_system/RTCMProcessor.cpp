#include "RTCMProcessor.h"
#include "QNetworkBase.h"
#include "LEDManagerFSM.h"
#include "SerialManager.h"

// Just declare what we need, don't include pcb.h
#define SerialGPS1 Serial5 // From pcb.h

// External LED manager
extern LEDManagerFSM ledManagerFSM;

// External UDP instances from QNetworkBase
extern EthernetUDP udpRTCM;

// QNEthernet namespace
using namespace qindesign::network;

// Static instance pointer
RTCMProcessor *RTCMProcessor::instance = nullptr;

RTCMProcessor::RTCMProcessor()
{
    instance = this;
}

RTCMProcessor::~RTCMProcessor()
{
    instance = nullptr;
}

void RTCMProcessor::init()
{
    if (instance == nullptr)
    {
        new RTCMProcessor();
    }
}

// Remove static callback - QNEthernet uses a different approach

void RTCMProcessor::processRTCM(const uint8_t* data, size_t len, const IPAddress& remoteIP, uint16_t remotePort)
{
    // Match the original code logic exactly
    if (!QNetworkBase::isConnected())
        return;

    if (remotePort == 9999 && len >= 5)
    {
        if (!SerialManager::getInstance()->isGPS1Bridged())
        {
            // Write directly to serial port
            SerialGPS1.write(data, len);
        }
        
        // Pulse GPS LED blue for RTCM packet
        ledManagerFSM.pulseRTCM();
    }
    // No need to delete buffer - QNEthernet handles memory management
}

void RTCMProcessor::processRadioRTCM()
{
    // Process RTCM data from radio serial port
    while (SerialRadio.available())
    {
        uint8_t rtcmByte = SerialRadio.read();
        
        // Forward RTCM to GPS1 (unless bridged)
        if (!SerialManager::getInstance()->isGPS1Bridged())
        {
            SerialGPS1.write(rtcmByte);
        }
        
        // Note: We could accumulate bytes and pulse LED per complete RTCM frame
        // For now, pulse periodically to show radio RTCM activity
        static uint32_t lastPulse = 0;
        if (millis() - lastPulse > 1000) // Pulse every second when receiving
        {
            ledManagerFSM.pulseRTCM();
            lastPulse = millis();
        }
    }
}

void RTCMProcessor::process()
{
    // Process all RTCM sources
    // Network RTCM is handled via UDP callback
    // Process radio RTCM here
    processRadioRTCM();
}