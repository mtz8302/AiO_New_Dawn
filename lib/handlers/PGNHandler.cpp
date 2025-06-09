#include "PGNHandler.h"
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
PGNHandler *PGNHandler::instance = nullptr;

PGNHandler::PGNHandler()
{
    instance = this;
}

PGNHandler::~PGNHandler()
{
    instance = nullptr;
}

void PGNHandler::init()
{
    if (instance == nullptr)
    {
        new PGNHandler();
    }
}

// Static callback for Mongoose - matching mg_event_handler_t signature
void PGNHandler::handlePGN(struct mg_connection *udpPacket, int ev, void *ev_data)
{
    if (instance != nullptr)
    {
        instance->processPGN(udpPacket, ev, ev_data);
    }
}

void PGNHandler::processPGN(struct mg_connection *udpPacket, int ev, void *ev_data)
{
    if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
        return;

    if (ev == MG_EV_READ && mg_ntohs(udpPacket->rem.port) == 9999 && udpPacket->recv.len >= 5)
    {
        Serial.printf("PGN received: type=%d, len=%d\r\n", udpPacket->recv.buf[3], udpPacket->recv.len);

        // Verify first 3 PGN header bytes
        if (udpPacket->recv.buf[0] != 128 || udpPacket->recv.buf[1] != 129 || udpPacket->recv.buf[2] != 127)
        {
            mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
            return;
        }

        // Process PGN messages
        switch (udpPacket->recv.buf[3])
        {
        case 200: // Hello from AgIO
            if (udpPacket->recv.len == 9)
            {
                processHelloFromAgIO(udpPacket);
            }
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
            Serial.printf("Unknown PGN type: %d\r\n", udpPacket->recv.buf[3]);
            break;
        }

        mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
    }
    else
    {
        mg_iobuf_del(&udpPacket->recv, 0, udpPacket->recv.len);
    }
}

void PGNHandler::processHelloFromAgIO(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Hello from AgIO");

    uint8_t helloFromAutoSteer[] = {128, 129, 126, 126, 5, 0, 0, 0, 0, 0, 71};
    ::sendUDPbytes(helloFromAutoSteer, sizeof(helloFromAutoSteer));
}

void PGNHandler::processSubnetChange(struct mg_connection *udpPacket)
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

void PGNHandler::processScanRequest(struct mg_connection *udpPacket)
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

void PGNHandler::processSteerConfig(struct mg_connection *udpPacket)
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

void PGNHandler::processSteerSettings(struct mg_connection *udpPacket)
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

void PGNHandler::processSteerData(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Steer Data");

    // Process real-time steer data
    // Implementation depends on control system integration
    Serial.print("Steer data received - integration with control system pending");
}

void PGNHandler::printPgnAnnouncement(struct mg_connection *udpPacket, char *pgnName)
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