#include "PGNProcessor.h"
#include "ConfigManager.h" // Full definition needed for method calls

// External declaration of the global pointer (defined in main.cpp)
extern ConfigManager *configPTR;

// Forward declaration for NetworkBase structure
struct NetConfigStruct
{
    static constexpr uint8_t defaultIP[5] = {192, 168, 5, 126};
    uint8_t currentIP[5] = {192, 168, 5, 126};
    uint8_t gatewayIP[5] = {192, 168, 5, 1};
    uint8_t broadcastIP[5] = {192, 168, 5, 255};
};

// External references to NetworkBase functions and variables
extern void sendUDPbytes(uint8_t *message, int msgLen);
extern void sendUDPchars(char *stuff);
extern void save_current_net();
extern struct mg_connection *sendAgio;
extern NetConfigStruct netConfig;

// Static instance pointer
PGNProcessor *PGNProcessor::instance = nullptr;

PGNProcessor::PGNProcessor()
{
    instance = this;
}

PGNProcessor::~PGNProcessor()
{
    instance = nullptr;
}

void PGNProcessor::init()
{
    if (instance == nullptr)
    {
        new PGNProcessor();
    }
}

// Static callback for Mongoose - matching mg_event_handler_t signature
void PGNProcessor::handlePGN(struct mg_connection *udpPacket, int ev, void *ev_data)
{
    if (instance != nullptr)
    {
        instance->processPGN(udpPacket, ev, ev_data);
    }
}

void PGNProcessor::processPGN(struct mg_connection *udpPacket, int ev, void *ev_data)
{
    if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
        return;

    if (ev == MG_EV_READ && mg_ntohs(udpPacket->rem.port) == 9999 && udpPacket->recv.len >= 5)
    {
        // Serial.printf("PGN received: type=%d, len=%d\r\n", udpPacket->recv.buf[3], udpPacket->recv.len);

        // Verify first 3 PGN header bytes
        if (udpPacket->recv.buf[0] != 128 || udpPacket->recv.buf[1] != 129 || udpPacket->recv.buf[2] != 127)
        {
            mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
            return;
        }

        uint8_t pgn = udpPacket->recv.buf[3];
        
        // Debug: show registered callbacks for this PGN
        if (pgn == 200) {
            // Serial.printf("\r\n[DEBUG] Hello PGN 200 received, %d callbacks registered", registrationCount);
            for (size_t i = 0; i < registrationCount; i++) {
                if (registrations[i].pgn == 200) {
                    Serial.printf("\r\n  - Found callback: %s", registrations[i].name);
                }
            }
        }
        bool handled = false;
        
        // Check if this is a broadcast PGN (Hello or Scan Request)
        bool isBroadcast = (pgn == 200 || pgn == 202);
        
        // For broadcast PGNs, call ALL registered callbacks
        if (isBroadcast)
        {
            // Commented out for quieter operation
            // Serial.printf("\r\n[PGNProcessor] Broadcasting PGN %d to all %d registered handlers", pgn, registrationCount);
            
            const uint8_t* data = &udpPacket->recv.buf[5];
            size_t dataLen = udpPacket->recv.len - 6; // Subtract header(3) + pgn(1) + len(1) + crc(1)
            
            for (size_t i = 0; i < registrationCount; i++)
            {
                // Serial.printf("\r\n[PGNProcessor] Broadcasting to %s", registrations[i].name);
                registrations[i].callback(pgn, data, dataLen);
                handled = true;
            }
        }
        else
        {
            // For non-broadcast PGNs, only call matching callbacks
            for (size_t i = 0; i < registrationCount; i++)
            {
                if (registrations[i].pgn == pgn)
                {
                    // Found a registered handler - call it
                    // if (pgn == 254) {
                    //     Serial.printf("\r\n[PGNProcessor] Routing PGN %d to %s, len=%d", pgn, registrations[i].name, udpPacket->recv.len);
                    // }
                    
                    // Debug: Print full packet for PGN 254 (commented out for now)
                    // if (pgn == 254) {
                    //     Serial.printf("\r\n[PGNProcessor] PGN 254 full packet:");
                    //     for (size_t j = 0; j < udpPacket->recv.len; j++) {
                    //         Serial.printf(" %02X", udpPacket->recv.buf[j]);
                    //     }
                    //     Serial.printf("\r\n[PGNProcessor] Header(5): %02X %02X %02X %02X %02X",
                    //                   udpPacket->recv.buf[0], udpPacket->recv.buf[1], 
                    //                   udpPacket->recv.buf[2], udpPacket->recv.buf[3], 
                    //                   udpPacket->recv.buf[4]);
                    //     Serial.printf("\r\n[PGNProcessor] Speed(2): %02X %02X", 
                    //                   udpPacket->recv.buf[5], udpPacket->recv.buf[6]);
                    //     Serial.printf("\r\n[PGNProcessor] Status: %02X", udpPacket->recv.buf[7]);
                    //     Serial.printf("\r\n[PGNProcessor] SteerAngle(2): %02X %02X", 
                    //                   udpPacket->recv.buf[8], udpPacket->recv.buf[9]);
                    // }
                    
                    // Pass the data starting after the 5-byte header
                    // PGN 254 data starts at position 5: speed(2), status(1), steerAngle(2), etc.
                    const uint8_t* data = &udpPacket->recv.buf[5];
                    size_t dataLen = udpPacket->recv.len - 6; // Subtract header(5) + crc(1)
                    
                    // Only print for non-254 PGNs to reduce noise
                    if (pgn != 254 && pgn != 239) {  // Also skip PGN 239 (frequent machine data)
                        Serial.printf("\r\n[PGNProcessor] Calling %s for PGN %d", registrations[i].name, pgn);
                    }
                    registrations[i].callback(pgn, data, dataLen);
                    handled = true;
                    break; // Only one handler per non-broadcast PGN
                }
            }
        }
        
        // PGNProcessor only routes - no built-in handlers
        // Unhandled PGNs are simply dropped

        mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
    }
    else
    {
        mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
    }
}

// REMOVED ALL BUILT-IN HANDLERS
// PGNProcessor now ONLY routes to registered callbacks
// The following functions were removed:
// - processSubnetChange
// - processScanRequest  
// - processSteerConfig
// - processSteerSettings
// - processSteerData

void PGNProcessor::printPgnAnnouncement(struct mg_connection *udpPacket, char *pgnName)
{
    Serial.printf("\r\n0x%02X(%d)-%s",
                  udpPacket->recv.buf[3],
                  udpPacket->recv.buf[3],
                  pgnName);

    // Print data bytes
    Serial.printf(" %d Data>", udpPacket->recv.len);
    for (int i = 4; i < udpPacket->recv.len - 1; i++)
    {
        Serial.printf("%3d ", udpPacket->recv.buf[i]);
    }
}

bool PGNProcessor::registerCallback(uint8_t pgn, PGNCallback callback, const char* name)
{
    // Check if we have room for more registrations
    if (registrationCount >= MAX_REGISTRATIONS)
    {
        Serial.printf("\r\n[PGNProcessor] Registration failed - max callbacks reached (%d)", MAX_REGISTRATIONS);
        return false;
    }
    
    // Check if this PGN is already registered
    for (size_t i = 0; i < registrationCount; i++)
    {
        if (registrations[i].pgn == pgn)
        {
            Serial.printf("\r\n[PGNProcessor] PGN %d already registered to %s", pgn, registrations[i].name);
            return false;
        }
    }
    
    // Add the new registration
    registrations[registrationCount].pgn = pgn;
    registrations[registrationCount].callback = callback;
    registrations[registrationCount].name = name;
    registrationCount++;
    
    Serial.printf("\r\n[PGNProcessor] Registered callback for PGN %d (%s)", pgn, name);
    return true;
}

bool PGNProcessor::unregisterCallback(uint8_t pgn)
{
    // Find the registration
    for (size_t i = 0; i < registrationCount; i++)
    {
        if (registrations[i].pgn == pgn)
        {
            // Found it - remove by shifting remaining entries
            Serial.printf("\r\n[PGNProcessor] Unregistering callback for PGN %d (%s)", pgn, registrations[i].name);
            
            for (size_t j = i; j < registrationCount - 1; j++)
            {
                registrations[j] = registrations[j + 1];
            }
            registrationCount--;
            return true;
        }
    }
    
    Serial.printf("\r\n[PGNProcessor] PGN %d not found for unregistration", pgn);
    return false;
}

void PGNProcessor::listRegisteredCallbacks()
{
    Serial.printf("\r\n[PGNProcessor] Registered callbacks (%d):", registrationCount);
    for (size_t i = 0; i < registrationCount; i++)
    {
        Serial.printf("\r\n  - PGN %d: %s", registrations[i].pgn, registrations[i].name);
    }
}