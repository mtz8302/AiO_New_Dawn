#include "RTCMProcessor.h"
#include "mongoose_glue.h"

// Just declare what we need, don't include pcb.h
#define SerialGPS1 Serial5 // From pcb.h
extern struct mg_mgr g_mgr;

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

// Static callback for Mongoose
void RTCMProcessor::handleRTCM(struct mg_connection *rtcm, int ev, void *ev_data)
{
    if (instance != nullptr)
    {
        instance->processRTCM(rtcm, ev, ev_data);
    }
}

void RTCMProcessor::processRTCM(struct mg_connection *rtcm, int ev, void *ev_data)
{
    // Match the original code logic exactly
    if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
        return;

    if (ev == MG_EV_READ && mg_ntohs(rtcm->rem.port) == 9999 && rtcm->recv.len >= 5)
    {

        // Copy to buffer exactly like the original code
        char TXbuf[1024];
        for (size_t i = 0; i < rtcm->recv.len; i++)
        {
            TXbuf[i] = rtcm->recv.buf[i];
        }
        int length = rtcm->recv.len;

        // Write using the same pattern as your original
        SerialGPS1.write(TXbuf, length);

        mg_iobuf_del(&rtcm->recv, 0, rtcm->recv.len);
    }
    else
    {
        mg_iobuf_del(&rtcm->recv, 0, rtcm->recv.len);
    }
}