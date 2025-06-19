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
                    
                    // Pass the data starting after the header, pgn, and length byte
                    const uint8_t* data = &udpPacket->recv.buf[5];
                    size_t dataLen = udpPacket->recv.len - 6; // Subtract header(3) + pgn(1) + len(1) + crc(1)
                    
                    registrations[i].callback(pgn, data, dataLen);
                    handled = true;
                    break; // Only one handler per non-broadcast PGN
                }
            }
        }
        
        // If not handled by a registered callback (or it's a broadcast), use built-in handlers
        if (!handled || isBroadcast)
        {
            switch (pgn)
            {
            case 200: // Hello from AgIO
                // Now handled by registered callbacks (IMU, GPS, etc.)
                break;
            case 201: // Subnet change
                if (udpPacket->recv.len == 8)
                {
                    processSubnetChange(udpPacket);
                }
                break;
            case 202: // Scan request
                if (udpPacket->recv.len == 7)
                {
                    processScanRequest(udpPacket);
                }
                break;
            case 251: // Steer config
                Serial.printf("\r\n[PGN] Received PGN 251, length=%d", udpPacket->recv.len);
                if (udpPacket->recv.len == 14)
                {
                    processSteerConfig(udpPacket);
                }
                else
                {
                    Serial.printf(" - WRONG LENGTH! Expected 14");
                }
                break;
            case 252: // Steer settings
                Serial.printf("\r\n[PGN] Received PGN 252, length=%d", udpPacket->recv.len);
                Serial.printf("\r\n[PGN252] Full packet: ");
                for (int i = 0; i < udpPacket->recv.len && i < 16; i++) {
                    Serial.printf("%02X ", udpPacket->recv.buf[i]);
                }
                if (udpPacket->recv.len == 14)
                {
                    processSteerSettings(udpPacket);
                }
                else
                {
                    Serial.printf(" - WRONG LENGTH! Expected 14");
                }
                break;
            case 254: // Steer data
                Serial.printf("\r\n[PGN] Received PGN 254, length=%d", udpPacket->recv.len);
                if (udpPacket->recv.len == 14)
                {
                    processSteerData(udpPacket);
                }
                else
                {
                    Serial.printf(" - WRONG LENGTH! Expected 14");
                }
                break;
            case 100: // PERMANENTLY DISABLED - Unknown PGN from AOG
                // 30 byte message of unknown purpose - not needed
                break;
            case 229: // Extended Machine/Tool data - silently ignore for now
                // TODO: Implement when machine control is added
                break;
            case 239: // Machine/Tool control - silently ignore for now
                // TODO: Implement when machine control is added
                break;
            default:
                Serial.printf("\r\nUnknown PGN type: %d (0x%02X)", pgn, pgn);
                Serial.printf("\r\n  Length: %d bytes", udpPacket->recv.len);
                Serial.printf("\r\n  Header: %02X %02X %02X", 
                    udpPacket->recv.buf[0], udpPacket->recv.buf[1], udpPacket->recv.buf[2]);
                Serial.printf("\r\n  Data hex:   ");
                int dataLen = udpPacket->recv.len - 1;
                int displayLen = (dataLen < 20) ? dataLen : 20;
                for (int i = 4; i < 4 + displayLen && i < udpPacket->recv.len - 1; i++) {
                    Serial.printf("%02X ", udpPacket->recv.buf[i]);
                }
                if (dataLen > 20) {
                    Serial.printf("...");
                }
                Serial.printf("\r\n  Data ascii: ");
                for (int i = 4; i < 4 + displayLen && i < udpPacket->recv.len - 1; i++) {
                    char c = udpPacket->recv.buf[i];
                    if (c >= 32 && c <= 126) {
                        Serial.printf(" %c ", c);
                    } else {
                        Serial.printf(" . ");
                    }
                }
                if (dataLen > 20) {
                    Serial.printf("...");
                }
                Serial.printf("\r\n");
                break;
            }
        }

        mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
    }
    else
    {
        mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
    }
}


void PGNProcessor::processSubnetChange(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Subnet Change");

    // Extract new subnet from PGN message
    netConfig.currentIP[0] = udpPacket->recv.buf[4];
    netConfig.currentIP[1] = udpPacket->recv.buf[5];
    netConfig.currentIP[2] = udpPacket->recv.buf[6];

    Serial.printf("New subnet: %d.%d.%d.x\r\n",
                  netConfig.currentIP[0], netConfig.currentIP[1], netConfig.currentIP[2]);

    // Call the NetworkBase function to save to EEPROM
    save_current_net();
}

void PGNProcessor::processScanRequest(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Scan Request");

    // Send scan reply with module info
    uint8_t scanReply[] = {128, 129, 203, 203,
                           netConfig.currentIP[3], // Module IP last octet
                           202,                    // Source address
                           0, 0, 0, 0,             // Reserved
                           75};                    // CRC
    ::sendUDPbytes(scanReply, sizeof(scanReply));
}

void PGNProcessor::processSteerConfig(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Steer Config");
    
    // Forward to registered callbacks
    for (size_t i = 0; i < registrationCount; i++)
    {
        if (registrations[i].pgn == 251)
        {
            registrations[i].callback(251, &udpPacket->recv.buf[5], udpPacket->recv.len - 6);
        }
    }
}

void PGNProcessor::processSteerSettings(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Steer Settings");
    
    // Forward to registered callbacks
    for (size_t i = 0; i < registrationCount; i++)
    {
        if (registrations[i].pgn == 252)
        {
            registrations[i].callback(252, &udpPacket->recv.buf[5], udpPacket->recv.len - 6);
        }
    }
}

void PGNProcessor::processSteerData(struct mg_connection *udpPacket)
{
    // printPgnAnnouncement(udpPacket, (char *)"Steer Data");

    // Forward to registered callbacks
    for (size_t i = 0; i < registrationCount; i++)
    {
        if (registrations[i].pgn == 254)
        {
            registrations[i].callback(254, &udpPacket->recv.buf[5], udpPacket->recv.len - 6);
        }
    }
}

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