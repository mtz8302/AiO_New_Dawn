#include "RTCMHandler.h"
#include "mongoose_glue.h"

// Just declare what we need, don't include pcb.h
#define SerialGPS1 Serial5 // From pcb.h
extern struct mg_mgr g_mgr;

// Static instance pointer
RTCMHandler *RTCMHandler::instance = nullptr;

RTCMHandler::RTCMHandler()
{
    instance = this;
}

RTCMHandler::~RTCMHandler()
{
    instance = nullptr;
}

void RTCMHandler::init()
{
    if (instance == nullptr)
    {
        new RTCMHandler();
    }
}

// Static callback for Mongoose
void RTCMHandler::handleRTCM(struct mg_connection *rtcm, int ev, void *ev_data)
{
    if (instance != nullptr)
    {
        instance->processRTCM(rtcm, ev, ev_data);
    }
}

void RTCMHandler::processRTCM(struct mg_connection *rtcm, int ev, void *ev_data)
{
    // Match the original code logic exactly
    if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
        return;

    if (ev == MG_EV_READ && mg_ntohs(rtcm->rem.port) == 9999 && rtcm->recv.len >= 5)
    {
        Serial.printf("RTCM: Processing %d bytes\n", rtcm->recv.len);

        // Copy to buffer exactly like the original code
        char TXbuf[1024];
        for (size_t i = 0; i < rtcm->recv.len; i++)
        {
            TXbuf[i] = rtcm->recv.buf[i];
        }
        int length = rtcm->recv.len;

        // Write using the same pattern as your original
        SerialGPS1.write(TXbuf, length);
        Serial.println("RTCM: Data written to GPS1");

        mg_iobuf_del(&rtcm->recv, 0, rtcm->recv.len);
        Serial.println("RTCM: Buffer cleared");
    }
    else
    {
        mg_iobuf_del(&rtcm->recv, 0, rtcm->recv.len);
    }
}