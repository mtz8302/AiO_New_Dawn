#include "RTCMProcessor.h"
#include "QNetworkBase.h"
#include "LEDManagerFSM.h"

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
        // Write directly to serial port
        SerialGPS1.write(data, len);
        
        // Pulse GPS LED blue for RTCM packet
        ledManagerFSM.pulseRTCM();
    }
    // No need to delete buffer - QNEthernet handles memory management
}