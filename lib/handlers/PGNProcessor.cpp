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
            Serial.printf("\r\n[DEBUG] Hello PGN 200 received, %d callbacks registered", registrationCount);
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
            Serial.printf("\r\n[PGNProcessor] Broadcasting PGN %d to all %d registered handlers", pgn, registrationCount);
            
            const uint8_t* data = &udpPacket->recv.buf[4];
            size_t dataLen = udpPacket->recv.len - 5; // Subtract header(3) + pgn(1) + crc(1)
            
            for (size_t i = 0; i < registrationCount; i++)
            {
                Serial.printf("\r\n[PGNProcessor] Broadcasting to %s", registrations[i].name);
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
                    Serial.printf("\r\n[PGNProcessor] Routing PGN %d to %s", pgn, registrations[i].name);
                    
                    // Pass the data starting after the 3-byte header and PGN type byte
                    const uint8_t* data = &udpPacket->recv.buf[4];
                    size_t dataLen = udpPacket->recv.len - 5; // Subtract header(3) + pgn(1) + crc(1)
                    
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
                if (udpPacket->recv.len == 14)
                {
                    processSteerConfig(udpPacket);
                }
                break;
            case 252: // Steer settings
                if (udpPacket->recv.len == 14)
                {
                    processSteerSettings(udpPacket);
                }
                break;
            case 254: // Steer data
                if (udpPacket->recv.len == 14)
                {
                    processSteerData(udpPacket);
                }
                break;
            default:
                Serial.printf("Unknown PGN type: %d\r\n", pgn);
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

    if (!configPTR)
    {
        Serial.print("ConfigManager not available");
        return;
    }

    // Parse PGN 251 data structure
    uint8_t sett0 = udpPacket->recv.buf[5]; // setting0 byte
    uint8_t pulseCountMax = udpPacket->recv.buf[6];
    uint8_t minSpeed = udpPacket->recv.buf[7];
    uint8_t sett1 = udpPacket->recv.buf[8]; // setting1 byte

    // Extract boolean flags from setting0 byte
    configPTR->setInvertWAS(bitRead(sett0, 0));
    configPTR->setIsRelayActiveHigh(bitRead(sett0, 1));
    configPTR->setMotorDriveDirection(bitRead(sett0, 2));
    configPTR->setSingleInputWAS(bitRead(sett0, 3));
    configPTR->setCytronDriver(bitRead(sett0, 4));
    configPTR->setSteerSwitch(bitRead(sett0, 5));
    configPTR->setSteerButton(bitRead(sett0, 6));
    configPTR->setShaftEncoder(bitRead(sett0, 7));

    // Set numeric values
    configPTR->setPulseCountMax(pulseCountMax);
    configPTR->setMinSpeed(minSpeed);

    // Extract boolean flags from setting1 byte
    configPTR->setIsDanfoss(bitRead(sett1, 0));
    configPTR->setPressureSensor(bitRead(sett1, 1));
    configPTR->setCurrentSensor(bitRead(sett1, 2));
    configPTR->setIsUseYAxis(bitRead(sett1, 3));

    // Save to EEPROM
    configPTR->saveSteerConfig();

    // Debug output
    Serial.printf("\r\nSteer Config Updated:");
    Serial.printf("\r\n- InvertWAS: %d", configPTR->getInvertWAS());
    Serial.printf("\r\n- RelayActiveHigh: %d", configPTR->getIsRelayActiveHigh());
    Serial.printf("\r\n- MotorDirection: %d", configPTR->getMotorDriveDirection());
    Serial.printf("\r\n- SingleInputWAS: %d", configPTR->getSingleInputWAS());
    Serial.printf("\r\n- CytronDriver: %d", configPTR->getCytronDriver());
    Serial.printf("\r\n- SteerSwitch: %d", configPTR->getSteerSwitch());
    Serial.printf("\r\n- SteerButton: %d", configPTR->getSteerButton());
    Serial.printf("\r\n- ShaftEncoder: %d", configPTR->getShaftEncoder());
    Serial.printf("\r\n- PulseCountMax: %d", configPTR->getPulseCountMax());
    Serial.printf("\r\n- MinSpeed: %d", configPTR->getMinSpeed());
    Serial.printf("\r\n- IsDanfoss: %d", configPTR->getIsDanfoss());
    Serial.printf("\r\n- PressureSensor: %d", configPTR->getPressureSensor());
    Serial.printf("\r\n- CurrentSensor: %d", configPTR->getCurrentSensor());
    Serial.printf("\r\n- UseYAxis: %d", configPTR->getIsUseYAxis());
}

void PGNProcessor::processSteerSettings(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Steer Settings");

    if (!configPTR)
    {
        Serial.print("ConfigManager not available");
        return;
    }

    // Parse PGN 252 data structure
    float kp = (float)udpPacket->recv.buf[5];
    uint8_t highPWM = udpPacket->recv.buf[6];
    float lowPWM = (float)udpPacket->recv.buf[7];
    uint8_t minPWM = udpPacket->recv.buf[8];
    uint8_t steerSensorCounts = udpPacket->recv.buf[9];

    // WAS offset is 16-bit value (Lo/Hi bytes)
    int16_t wasOffset = udpPacket->recv.buf[10];
    wasOffset |= (udpPacket->recv.buf[11] << 8);

    // Ackerman fix is 16-bit value (Lo/Hi bytes)
    int16_t ackermanRaw = udpPacket->recv.buf[12];
    ackermanRaw |= (udpPacket->recv.buf[13] << 8);
    float ackermanFix = (float)ackermanRaw * 0.01;

    // Apply lowPWM adjustment
    float adjustedLowPWM = (float)minPWM * 1.2;
    if (adjustedLowPWM < 255)
    {
        lowPWM = adjustedLowPWM;
    }

    // Update ConfigManager
    configPTR->setKp(kp);
    configPTR->setHighPWM(highPWM);
    configPTR->setLowPWM(lowPWM);
    configPTR->setMinPWM(minPWM);
    configPTR->setSteerSensorCounts(steerSensorCounts);
    configPTR->setWasOffset(wasOffset);
    configPTR->setAckermanFix(ackermanFix);

    // Save to EEPROM
    configPTR->saveSteerSettings();

    // Debug output
    Serial.printf("\r\nSteer Settings Updated:");
    Serial.printf("\r\n- Kp: %.1f", configPTR->getKp());
    Serial.printf("\r\n- HighPWM: %d", configPTR->getHighPWM());
    Serial.printf("\r\n- LowPWM: %.1f", configPTR->getLowPWM());
    Serial.printf("\r\n- MinPWM: %d", configPTR->getMinPWM());
    Serial.printf("\r\n- SensorCounts: %d", configPTR->getSteerSensorCounts());
    Serial.printf("\r\n- WAS Offset: %d", configPTR->getWasOffset());
    Serial.printf("\r\n- Ackerman Fix: %.2f", configPTR->getAckermanFix());
}

void PGNProcessor::processSteerData(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Steer Data");

    // Process real-time steer data
    // Implementation depends on control system integration
    Serial.print("Steer data received - integration with control system pending");
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