#ifndef RTCMProcessor_H_
#define RTCMProcessor_H_

#include "Arduino.h"
#include "mongoose.h"

class RTCMProcessor
{
private:
    static RTCMProcessor *instance;

public:
    RTCMProcessor();
    ~RTCMProcessor();

    // Static method for Mongoose callback (3 parameters to match mg_event_handler_t)
    static void handleRTCM(struct mg_connection *rtcm, int ev, void *ev_data);

    // Instance method that does the actual work
    void processRTCM(struct mg_connection *rtcm, int ev, void *ev_data);

    // Initialize the handler
    static void init();
};

#endif // RTCMProcessor_H_