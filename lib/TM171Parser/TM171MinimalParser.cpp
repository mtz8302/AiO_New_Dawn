// TM171MinimalParser.cpp
#include "TM171MinimalParser.h"

void TM171MinimalParser::begin()
{
    reset();
}

void TM171MinimalParser::reset()
{
    bufferIndex = 0;
    inPacket = false;
    expectedLength = 0;
    dataValid = false;
}

void TM171MinimalParser::addByte(uint8_t byte)
{
    if (!inPacket)
    {
        // Looking for sync pattern
        if (bufferIndex == 0 && byte == SYNC1)
        {
            buffer[bufferIndex++] = byte;
        }
        else if (bufferIndex == 1 && byte == SYNC2)
        {
            buffer[bufferIndex++] = byte;
        }
        else if (bufferIndex == 2)
        {
            // This is the length byte
            buffer[bufferIndex++] = byte;
            expectedLength = byte;

            // Sanity check on length
            if (expectedLength > BUFFER_SIZE - 5)
            { // 5 = sync(2) + len(1) + crc(2)
                reset();
                return;
            }
            inPacket = true;
        }
        else
        {
            // Not a valid sync pattern, reset
            reset();
            // Check if this byte could be start of new sync
            if (byte == SYNC1)
            {
                buffer[0] = byte;
                bufferIndex = 1;
            }
        }
    }
    else
    {
        // We're in a packet, collecting data
        buffer[bufferIndex++] = byte;

        // Check if we have complete packet (length + 2 CRC bytes)
        if (bufferIndex >= expectedLength + 5)
        {
            // Extract CRC from packet
            uint16_t receivedCRC = (uint16_t)buffer[bufferIndex - 1] << 8 | buffer[bufferIndex - 2];

            // Calculate CRC on everything except sync bytes and CRC itself
            uint16_t calculatedCRC = calculateCRC(&buffer[2], expectedLength + 1);

            if (receivedCRC == calculatedCRC)
            {
                // Good packet! Check if it's RPY data
                uint8_t objectID = extractObjectID(&buffer[3]); // Payload info starts at byte 3

                if (objectID == OBJECT_ID_RPY)
                {
                    // Parse RPY data - payload content starts at byte 7
                    if (parseRPYData(&buffer[7], expectedLength - 4))
                    {
                        packetsReceived++;
                        dataValid = true;
                    }
                }
                else
                {
                    nonRPYPackets++;
                }
            }
            else
            {
                crcErrors++;
            }

            reset();
        }

        // Safety check for buffer overflow
        if (bufferIndex >= BUFFER_SIZE)
        {
            reset();
        }
    }
}

uint8_t TM171MinimalParser::extractObjectID(const uint8_t *payloadInfo)
{
    // Object ID is in the first 7 bits of the payload info
    // The bytes are in little-endian format
    return payloadInfo[0] & 0x7F;
}

bool TM171MinimalParser::parseRPYData(const uint8_t *payload, uint8_t payloadLength)
{
    // RPY data should be exactly 16 bytes
    if (payloadLength != 16)
    {
        return false;
    }

    // Extract timestamp (bytes 0-3)
    timestamp = bytesToUint32(&payload[0]);

    // Extract angles (all IEEE-754 floats)
    roll = bytesToFloat(&payload[4]);  // bytes 4-7
    pitch = bytesToFloat(&payload[8]); // bytes 8-11
    yaw = bytesToFloat(&payload[12]);  // bytes 12-15

    // Sanity check on angle values
    if (isnan(roll) || isnan(pitch) || isnan(yaw) ||
        isinf(roll) || isinf(pitch) || isinf(yaw) ||
        fabs(roll) > 180.0f || fabs(pitch) > 90.0f || fabs(yaw) > 360.0f)
    {
        return false;
    }

    return true;
}

float TM171MinimalParser::bytesToFloat(const uint8_t *bytes)
{
    // Little-endian float conversion
    union
    {
        float f;
        uint8_t b[4];
    } converter;

    converter.b[0] = bytes[0];
    converter.b[1] = bytes[1];
    converter.b[2] = bytes[2];
    converter.b[3] = bytes[3];

    return converter.f;
}

uint32_t TM171MinimalParser::bytesToUint32(const uint8_t *bytes)
{
    // Little-endian uint32 conversion
    return ((uint32_t)bytes[3] << 24) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[1] << 8) |
           (uint32_t)bytes[0];
}

uint16_t TM171MinimalParser::calculateCRC(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001; // Polynomial for Modbus CRC
            }
            else
            {
                crc = crc >> 1;
            }
        }
    }

    // Return in little-endian format (swap bytes)
    return ((crc & 0xFF) << 8) | ((crc >> 8) & 0xFF);
}

void TM171MinimalParser::printDebug() const
{
    Serial.println(F("=== TM171 Parser Debug ==="));
    Serial.print(F("Packets Received: "));
    Serial.println(packetsReceived);
    Serial.print(F("CRC Errors: "));
    Serial.println(crcErrors);
    Serial.print(F("Non-RPY Packets: "));
    Serial.println(nonRPYPackets);

    if (dataValid)
    {
        Serial.print(F("Timestamp: "));
        Serial.print(timestamp);
        Serial.println(F(" us"));
        Serial.print(F("Roll: "));
        Serial.print(roll, 3);
        Serial.println(F("°"));
        Serial.print(F("Pitch: "));
        Serial.print(pitch, 3);
        Serial.println(F("°"));
        Serial.print(F("Yaw: "));
        Serial.print(yaw, 3);
        Serial.println(F("°"));
    }
    else
    {
        Serial.println(F("No valid data yet"));
    }
    Serial.println(F("========================"));
}