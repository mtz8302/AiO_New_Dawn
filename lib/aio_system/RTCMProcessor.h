#ifndef RTCMProcessor_H_
#define RTCMProcessor_H_

#include "Arduino.h"
#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <QNEthernetUDP.h>

// QNEthernet namespace
using namespace qindesign::network;

class RTCMProcessor
{
public:
    static RTCMProcessor *instance;

private:
    RTCMProcessor();
    ~RTCMProcessor();

public:
    // Process incoming RTCM data
    void processRTCM(const uint8_t* data, size_t len, const IPAddress& remoteIP, uint16_t remotePort);

    // Initialize the handler
    static void init();
};

#endif // RTCMProcessor_H_