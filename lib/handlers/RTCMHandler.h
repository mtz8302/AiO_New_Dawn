#ifndef RTCMHANDLER_H_
#define RTCMHANDLER_H_

#include "Arduino.h"
#include "mongoose.h"

class RTCMHandler
{
private:
    static RTCMHandler *instance;

public:
    RTCMHandler();
    ~RTCMHandler();

    // Static method for Mongoose callback (3 parameters to match mg_event_handler_t)
    static void handleRTCM(struct mg_connection *rtcm, int ev, void *ev_data);

    // Instance method that does the actual work
    void processRTCM(struct mg_connection *rtcm, int ev, void *ev_data);

    // Initialize the handler
    static void init();
};

#endif // RTCMHANDLER_H_