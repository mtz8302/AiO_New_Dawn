// TM171MinimalParser.h
#ifndef TM171_MINIMAL_PARSER_H
#define TM171_MINIMAL_PARSER_H

#include <Arduino.h>

class TM171MinimalParser
{
private:
    static const uint8_t BUFFER_SIZE = 50; // Enough for 44-byte packets
    static const uint8_t SYNC1 = 0xAA;
    static const uint8_t SYNC2 = 0x55;
    static const uint8_t OBJECT_ID_RPY = 0x23; // Object ID 35 for Roll-Pitch-Yaw

    uint8_t buffer[BUFFER_SIZE];
    uint8_t bufferIndex = 0;
    bool inPacket = false;
    uint8_t expectedLength = 0;

    // Angle data
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    uint32_t timestamp = 0;
    bool dataValid = false;

    // Statistics
    uint32_t packetsReceived = 0;
    uint32_t crcErrors = 0;
    uint32_t nonRPYPackets = 0;

    uint16_t calculateCRC(const uint8_t *data, uint8_t length);
    bool parseRPYData(const uint8_t *payload, uint8_t payloadLength);
    uint8_t extractObjectID(const uint8_t *payloadInfo);
    float bytesToFloat(const uint8_t *bytes);
    uint32_t bytesToUint32(const uint8_t *bytes);

public:
    TM171MinimalParser() = default;

    void begin();
    void addByte(uint8_t byte);
    void reset();

    // Data getters
    float getRoll() const { return roll; }
    float getPitch() const { return pitch; }
    float getYaw() const { return yaw; }
    uint32_t getTimestamp() const { return timestamp; }
    bool isDataValid() const { return dataValid; }

    // Statistics
    uint32_t getPacketsReceived() const { return packetsReceived; }
    uint32_t getCRCErrors() const { return crcErrors; }
    uint32_t getNonRPYPackets() const { return nonRPYPackets; }

    // Debug
    void printDebug() const;
};

#endif // TM171_MINIMAL_PARSER_H