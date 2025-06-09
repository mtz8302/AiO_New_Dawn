#include "PGNHandler.h"

// External variables and functions
extern struct mg_mgr g_mgr;
extern void sendUDPbytes(uint8_t *message, int msgLen);
extern void sendUDPchars(char *stuff);

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

// Static callback for Mongoose
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

        // Process the PGN
        switch (udpPacket->recv.buf[3])
        {
        case 200: // Hello from AgIO
            if (udpPacket->recv.len == 9)
            {
                processHelloFromAgIO(udpPacket);
            }
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
    Serial.print("\r\nHello from AgIO received");

    // Simple response
    uint8_t helloFromAutoSteer[] = {128, 129, 126, 126, 5, 0, 0, 0, 0, 0, 71};
    sendUDPbytes(helloFromAutoSteer, sizeof(helloFromAutoSteer));
}

// Stub implementations for other methods
void PGNHandler::processSubnetChange(struct mg_connection *udpPacket) {}
void PGNHandler::processScanRequest(struct mg_connection *udpPacket) {}
void PGNHandler::processSteerConfig(struct mg_connection *udpPacket) {}
void PGNHandler::processSteerSettings(struct mg_connection *udpPacket) {}
void PGNHandler::processSteerData(struct mg_connection *udpPacket) {}
void PGNHandler::printPgnAnnouncement(struct mg_connection *udpPacket, char *pgnName) {}

void PGNHandler::sendUDPbytes(uint8_t *message, int msgLen)
{
    ::sendUDPbytes(message, msgLen);
}

void PGNHandler::sendUDPchars(char *stuff)
{
    ::sendUDPchars(stuff);
}