#ifndef PGNHANDLER_H_
#define PGNHANDLER_H_

#include "Arduino.h"
#include "mongoose.h"

// Forward declarations for external dependencies
extern struct mg_mgr g_mgr;

class PGNHandler
{
private:
    static PGNHandler *instance;

public:
    PGNHandler();
    ~PGNHandler();

    // Static method for Mongoose callback (3 parameters to match mg_event_handler_t)
    static void handlePGN(struct mg_connection *udpPacket, int ev, void *ev_data);

    // Instance method that does the actual work
    void processPGN(struct mg_connection *udpPacket, int ev, void *ev_data);

    // Utility methods
    void printPgnAnnouncement(struct mg_connection *udpPacket, char *pgnName);

    // PGN processing methods
    void processHelloFromAgIO(struct mg_connection *udpPacket);
    void processSubnetChange(struct mg_connection *udpPacket);
    void processScanRequest(struct mg_connection *udpPacket);
    void processSteerConfig(struct mg_connection *udpPacket);
    void processSteerSettings(struct mg_connection *udpPacket);
    void processSteerData(struct mg_connection *udpPacket);

    // Initialize the handler
    static void init();
};

#endif // PGNHANDLER_H_