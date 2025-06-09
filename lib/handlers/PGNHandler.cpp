#include "PGNHandler.h"

// Forward declaration for NetworkBase structure
struct NetConfigStruct
{
    static constexpr uint8_t defaultIP[5] = {192, 168, 5, 126};
    uint8_t currentIP[5] = {192, 168, 5, 126};
    uint8_t gatewayIP[5] = {192, 168, 5, 1};
    uint8_t broadcastIP[5] = {192, 168, 5, 255};
};

// External references to NetworkBase functions and variables (declared, not defined)
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
            if (udpPacket->recv.len == 12)
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
    ::sendUDPbytes(helloFromAutoSteer, sizeof(helloFromAutoSteer)); // Use global function directly
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

    save_current_net(); // This function exists in NetworkBase.h but is missing implementation
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

    // Process steer configuration data
    // Implementation depends on ConfigManager integration
    Serial.print("Steer config received - integration with ConfigManager pending");
}

void PGNHandler::processSteerSettings(struct mg_connection *udpPacket)
{
    printPgnAnnouncement(udpPacket, (char *)"Steer Settings");

    // Process steer settings data
    // Implementation depends on ConfigManager integration
    Serial.print("Steer settings received - integration with ConfigManager pending");
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