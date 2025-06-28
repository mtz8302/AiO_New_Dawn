#ifndef PGNProcessor_H_
#define PGNProcessor_H_

#include "Arduino.h"
#include "mongoose.h"

// Forward declarations for external dependencies
extern struct mg_mgr g_mgr;

// Define callback function type for PGN handlers
// Parameters: PGN number, data buffer, data length
typedef void (*PGNCallback)(uint8_t pgn, const uint8_t* data, size_t len);

// Structure to hold PGN registration info
struct PGNRegistration {
    uint8_t pgn;
    PGNCallback callback;
    const char* name;  // For debugging
};

class PGNProcessor
{
public:
    static PGNProcessor *instance;
    
private:
    // Array to store registered callbacks (max 20 registrations)
    static constexpr size_t MAX_REGISTRATIONS = 20;
    PGNRegistration registrations[MAX_REGISTRATIONS];
    size_t registrationCount = 0;

public:
    PGNProcessor();
    ~PGNProcessor();

    // Static method for Mongoose callback (3 parameters to match mg_event_handler_t)
    static void handlePGN(struct mg_connection *udpPacket, int ev, void *ev_data);

    // Instance method that does the actual work
    void processPGN(struct mg_connection *udpPacket, int ev, void *ev_data);

    // Utility methods
    void printPgnAnnouncement(struct mg_connection *udpPacket, char *pgnName);

    // PGN processing methods - REMOVED
    // PGNProcessor only routes to registered callbacks

    // Initialize the handler
    static void init();
    
    // Callback registration methods
    bool registerCallback(uint8_t pgn, PGNCallback callback, const char* name);
    bool unregisterCallback(uint8_t pgn);
    void listRegisteredCallbacks();  // For debugging
};

#endif // PGNProcessor_H_