#include "GNSSProcessor.h"
#include "UBXParser.h"
#include "calc_crc32.h"
#include "PGNUtils.h"
#include "EventLogger.h"
#include "QNetworkBase.h"
#include "ConfigManager.h"
#include <string.h>
#include <math.h>


GNSSProcessor::GNSSProcessor() : bufferIndex(0),
                                 state(WAIT_START),
                                 calculatedChecksum(0),
                                 receivedChecksum(0),
                                 checksumIndex(0),
                                 fieldCount(0),
                                 enableNoiseFilter(true),
                                 enableDebug(false),
                                 ubxParser(nullptr),
                                 udpPassthroughEnabled(false)
{

    // Initialize data structures
    memset(&gpsData, 0, sizeof(gpsData));
    // Initialize data

    gpsData.hdop = 99.9f;
    gpsData.fixTimeFractional = 0.0f;
    
    // Initialize NMEA coordinate cache with valid defaults
    gpsData.latitudeNMEA = 0.0;
    gpsData.longitudeNMEA = 0.0;
    gpsData.latDir = 'N';  // Default to North
    gpsData.lonDir = 'W';  // Default to West
    resetParser();
    
    // Initialize UBX parser
    ubxParser = new UBX_Parser();
}

GNSSProcessor::~GNSSProcessor()
{
    // Cleanup UBX parser
    if (ubxParser) {
        delete ubxParser;
        ubxParser = nullptr;
    }
}

bool GNSSProcessor::init()
{
    resetParser();
    
    // Load UDP passthrough setting from ConfigManager
    extern ConfigManager configManager;
    udpPassthroughEnabled = configManager.getGPSPassThrough();
    LOG_DEBUG(EventSource::GNSS, "UDP Passthrough %s (from EEPROM)", 
              udpPassthroughEnabled ? "enabled" : "disabled");
    
    // Register with PGNProcessor to receive broadcast messages
    if (PGNProcessor::instance)
    {
        // Register for broadcast PGNs (200 and 202)
        bool success = PGNProcessor::instance->registerBroadcastCallback(handleBroadcastPGN, "GPS Handler");
        if (!success)
        {
            LOG_ERROR(EventSource::GNSS, "Failed to register PGN callback");
            return false;
        }
        LOG_DEBUG(EventSource::GNSS, "Successfully registered for broadcast PGNs");
    }
    else
    {
        LOG_ERROR(EventSource::GNSS, "PGNProcessor not initialized");
        return false;
    }
    
    return true;
}

bool GNSSProcessor::setup(bool enableDebug, bool enableNoiseFilter)
{
    // Configure processor
    this->enableDebug = enableDebug;
    this->enableNoiseFilter = enableNoiseFilter;

    // Initialize
    if (!init())
    {
        if (enableDebug)
        {
            LOG_ERROR(EventSource::GNSS, "GNSS Processor init failed");
        }
        return false;
    }

    if (enableDebug)
    {
        LOG_INFO(EventSource::GNSS, "GNSS Processor initialized successfully");
    }

    return true;
}

bool GNSSProcessor::processNMEAChar(char c)
{
    // Periodic status logging
    static uint32_t lastStatusLog = 0;
    if (millis() - lastStatusLog > 60000) {  // Every minute
        lastStatusLog = millis();
        LOG_INFO(EventSource::GNSS, "GNSSProcessor status: passthrough=%d", udpPassthroughEnabled);
    }
    
    switch (state)
    {
    case WAIT_START:
        if (c == '$' || c == '#')
        {
            resetParser();
            state = READ_DATA;
            calculatedChecksum = 0;
            isUnicoreMessage = (c == '#');
            parseBuffer[bufferIndex++] = c;
        }
        break;

    case READ_DATA:
        if (c == '*')
        {
            // Store the asterisk
            if (bufferIndex < sizeof(parseBuffer) - 1) {
                parseBuffer[bufferIndex++] = c;
            }
            state = READ_CHECKSUM;
            receivedChecksum = 0;
            receivedChecksum32 = 0;
            checksumIndex = 0;
        }
        else if (c == '\r' || c == '\n')
        {
            // Message without checksum - shouldn't happen for valid NMEA
            parseBuffer[bufferIndex] = '\0';
            return processMessage();
        }
        else
        {
            if (bufferIndex < sizeof(parseBuffer) - 1)
            {
                parseBuffer[bufferIndex++] = c;
                if (!isUnicoreMessage)
                {
                    calculatedChecksum ^= c;
                }
            }
        }
        break;

    case READ_CHECKSUM:
        if (isHex(c))
        {
            // Store checksum characters
            if (bufferIndex < sizeof(parseBuffer) - 1) {
                parseBuffer[bufferIndex++] = c;
            }
            
            if (isUnicoreMessage)
            {
                // Unicore uses 32-bit CRC (8 hex digits)
                if (checksumIndex < 8)
                {
                    receivedChecksum32 = (receivedChecksum32 << 4) | hexToInt(c);
                    checksumIndex++;
                    if (checksumIndex == 8)
                    {
                        // Complete sentence received
                        parseBuffer[bufferIndex] = '\0'; // Null terminate
                        
                        if (udpPassthroughEnabled) {
                            // Just send via UDP and done
                            sendCompleteNMEA();
                            resetParser();
                            return true;
                        }
                        
                        // Otherwise validate and process
                        if (validateChecksum()) {
                            return processMessage();
                        } else {
                            resetParser();
                        }
                    }
                }
            }
            else
            {
                // Standard NMEA uses 8-bit XOR (2 hex digits)
                if (checksumIndex == 0)
                {
                    receivedChecksum = hexToInt(c) << 4;
                    checksumIndex = 1;
                }
                else
                {
                    receivedChecksum |= hexToInt(c);
                    
                    // Complete sentence received
                    parseBuffer[bufferIndex] = '\0'; // Null terminate
                    
                    if (udpPassthroughEnabled) {
                        // Send complete NMEA sentence via UDP
                        sendCompleteNMEA();
                        resetParser();
                        return true;
                    }
                    
                    // Otherwise validate and process
                    if (validateChecksum()) {
                        return processMessage();
                    } else {
                        resetParser();
                    }
                }
            }
        }
        else if (c == '\r' || c == '\n')
        {
            // End of sentence - ignore trailing CR/LF
        }
        break;
    }

    return false;
}

uint16_t GNSSProcessor::processNMEAStream(const char *data, uint16_t length)
{
    uint16_t processed = 0;

    for (uint16_t i = 0; i < length; i++)
    {
        if (processNMEAChar(data[i]))
        {
            processed++;
        }
    }

    return processed;
}

void GNSSProcessor::resetParser()
{
    bufferIndex = 0;
    state = WAIT_START;
    fieldCount = 0;
    checksumIndex = 0;
    isUnicoreMessage = false;
    memset(parseBuffer, 0, sizeof(parseBuffer));
    memset(fields, 0, sizeof(fields));
}

bool GNSSProcessor::validateChecksum()
{
    if (isUnicoreMessage)
    {
        // For Unicore messages, CRC32 is calculated from the character after # up to (but not including) *
        // Skip the # at position 0
        unsigned long calculatedCRC = CalculateCRC32(parseBuffer + 1, bufferIndex - 1);
        
        // CRC debug enabled for testing
        if (enableDebug)
        {
            LOG_DEBUG(EventSource::GNSS, "Unicore CRC: calc=%08lX recv=%08lX (len=%d)", 
                     calculatedCRC, receivedChecksum32, bufferIndex - 1);
            // Show what type of message this is
            char msgPreview[11] = {0};
            for (int i = 0; i < 10 && i < bufferIndex; i++)
            {
                msgPreview[i] = parseBuffer[i];
            }
            LOG_DEBUG(EventSource::GNSS, "Buffer[0-10]: %s, bufferIndex=%d", msgPreview, bufferIndex);
        }
        
        return calculatedCRC == receivedChecksum32;
    }
    else
    {
        // Standard NMEA XOR checksum
        return calculatedChecksum == receivedChecksum;
    }
}

bool GNSSProcessor::processMessage()
{
    // Use zero-copy parsing
    parseFieldsZeroCopy();

    if (fieldCount < 1)
    {
        resetParser();
        return false;
    }

    // For now, still populate the legacy fields array for compatibility
    // This will be removed once all parsers are converted
    for (uint8_t i = 0; i < fieldCount && i < 35; i++) {
        uint8_t copyLen = min(fieldRefs[i].length, (uint8_t)23);
        memcpy(fields[i], fieldRefs[i].start, copyLen);
        fields[i][copyLen] = '\0';
    }

    // Determine message type and process
    const char *msgType = fields[0];  // Will use fieldRefs[0] directly later
    bool processed = false;
    
    // Debug enabled for testing
    if (enableDebug)
    {
        LOG_DEBUG(EventSource::GNSS, "Message type: %s, fields: %d", msgType, fieldCount);
    }

    // Use fast message type detection
    MessageType type = detectMessageType(msgType);
    
    switch(type) {
        case MSG_GGA:
            processed = parseGGAZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_GNS:
            processed = parseGNSZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_VTG:
            processed = parseVTGZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_HPR:
            processed = parseHPRZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_KSXT:
            LOG_DEBUG(EventSource::GNSS, "Processing KSXT message");
            processed = parseKSXT();
            break;
            
        case MSG_INSPVAA:
            processed = parseINSPVAAZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_INSPVAXA:
            if (enableDebug) {
                LOG_DEBUG(EventSource::GNSS, "INSPVAXA detected, fieldCount=%d, bufferIndex=%d", fieldCount, bufferIndex);
            }
            processed = parseINSPVAXA();
            if (processed) {
                if (enableDebug) {
                    LOG_DEBUG(EventSource::GNSS, "INSPVAXA parsed successfully");
                }
            } else if (enableDebug) {
                LOG_DEBUG(EventSource::GNSS, "INSPVAXA parse failed");
            }
            break;
            
        case MSG_BESTGNSSPOS:
        case MSG_RMC:
        case MSG_AVR:
        case MSG_UNKNOWN:
        default:
            // Messages we don't currently parse
            processed = false;
            break;
    }

    if (processed)
    {
        // Valid GPS message received
        gpsData.lastUpdateTime = millis();
    }

    resetParser();
    return processed;
}

void GNSSProcessor::parseFields()
{
    fieldCount = 0;
    uint8_t fieldIndex = 0;

    // Skip the '$' or '#' and parse comma-separated fields
    for (int i = 1; i < bufferIndex && fieldCount < 35; i++)
    {
        char c = parseBuffer[i];

        if (c == ',' || c == ';' || c == '\0')
        {
            fields[fieldCount][fieldIndex] = '\0';
            fieldCount++;
            fieldIndex = 0;
        }
        else if (fieldIndex < 23)  // Increased from 15 to 23 to match field size
        {
            fields[fieldCount][fieldIndex++] = c;
        }
    }
    
    // Handle the last field if we haven't terminated it yet
    if (fieldIndex > 0 && fieldCount < 35)
    {
        fields[fieldCount][fieldIndex] = '\0';
        fieldCount++;
    }
}

bool GNSSProcessor::parseGGA()
{
    if (fieldCount < 9)
        return false;

    // Field 1: Time (HHMMSS.SS format)
    gpsData.fixTime = parseTime(fields[1]);

    // Fields 2-3: Latitude
    gpsData.latitude = parseLatitude(fields[2], fields[3]);

    // Fields 4-5: Longitude
    gpsData.longitude = parseLongitude(fields[4], fields[5]);

    // Field 6: Fix quality
    gpsData.fixQuality = parseFixQuality(fields[6]);

    // Field 7: Number of satellites
    gpsData.numSatellites = atoi(fields[7]);

    // Field 8: HDOP
    gpsData.hdop = parseFloat(fields[8]);

    // Field 9: Altitude
    gpsData.altitude = parseFloat(fields[9]);

    // Field 13: Age of DGPS (if present)
    if (fieldCount > 13 && strlen(fields[13]) > 0)
    {
        gpsData.ageDGPS = atoi(fields[13]);
    }

    // Check if we have valid position data
    // Need fix quality > 0 AND non-empty lat/lon fields
    bool hasValidCoords = (strlen(fields[2]) > 0 && strlen(fields[3]) > 0 && 
                          strlen(fields[4]) > 0 && strlen(fields[5]) > 0);
    
    gpsData.hasPosition = (gpsData.fixQuality > 0) && hasValidCoords && 
                         (gpsData.latitude != 0.0 || gpsData.longitude != 0.0);
    gpsData.isValid = gpsData.hasPosition;
    gpsData.messageTypeMask |= (1 << 0);  // Set GGA bit

    if (enableDebug)
    {
        logDebug("GGA processed");
    }

    return true;
}

bool GNSSProcessor::parseGNS()
{
    if (fieldCount < 6)
        return false;

    // Field 1: Time (HHMMSS.SS format)
    gpsData.fixTime = parseTime(fields[1]);

    // Fields 2-3: Latitude
    gpsData.latitude = parseLatitude(fields[2], fields[3]);

    // Fields 4-5: Longitude
    gpsData.longitude = parseLongitude(fields[4], fields[5]);

    // Field 6: Mode indicator (convert to fix quality)
    gpsData.fixQuality = parseFixQuality(fields[6], true);

    // Fields 7-8: Satellites, HDOP (if present)
    if (fieldCount > 7 && strlen(fields[7]) > 0)
    {
        gpsData.numSatellites = atoi(fields[7]);
    }
    if (fieldCount > 8 && strlen(fields[8]) > 0)
    {
        gpsData.hdop = parseFloat(fields[8]);
    }
    if (fieldCount > 9 && strlen(fields[9]) > 0)
    {
        gpsData.altitude = parseFloat(fields[9]);
    }

    gpsData.hasPosition = (gpsData.fixQuality > 0);
    gpsData.isValid = gpsData.hasPosition;
    gpsData.messageTypeMask |= (1 << 2);  // Set GNS bit

    if (enableDebug)
    {
        logDebug("GNS processed");
    }

    return true;
}

bool GNSSProcessor::parseVTG()
{
    if (fieldCount < 5)
        return false;

    // Field 1: True heading
    if (strlen(fields[1]) > 0)
    {
        gpsData.headingTrue = parseFloat(fields[1]);
    }

    // Field 5: Speed in knots
    if (strlen(fields[5]) > 0)
    {
        gpsData.speedKnots = parseFloat(fields[5]);

        // Apply F9P noise filtering if enabled
        if (enableNoiseFilter && gpsData.speedKnots < 0.1f)
        {
            gpsData.speedKnots = 0.0f;
        }
    }

    gpsData.hasVelocity = true;
    gpsData.messageTypeMask |= (1 << 1);  // Set VTG bit

    if (enableDebug)
    {
        logDebug("VTG processed");
    }

    return true;
}

bool GNSSProcessor::parseHPR()
{
    if (fieldCount < 5)
        return false;

    // Field 2: Heading
    if (strlen(fields[2]) > 0)
    {
        gpsData.dualHeading = parseFloat(fields[2]);
    }

    // Field 3: Roll
    if (strlen(fields[3]) > 0)
    {
        gpsData.dualRoll = parseFloat(fields[3]);
    }

    // Field 5: Solution quality
    if (strlen(fields[5]) > 0)
    {
        gpsData.headingQuality = atoi(fields[5]);
    }

    gpsData.hasDualHeading = true;
    gpsData.messageTypeMask |= (1 << 5);  // Set HPR bit

    if (enableDebug)
    {
        logDebug("HPR processed");
    }

    return true;
}

bool GNSSProcessor::parseKSXT()
{
    if (fieldCount < 10)
        return false;

    // Field 1: Timestamp (YYYYMMDDHHMMSS.SS format)
    if (strlen(fields[1]) >= 14)
    {
        // Extract just HHMMSS.SS portion (skip YYYYMMDD)
        const char* timeStr = fields[1] + 8; // Skip first 8 chars (YYYYMMDD)
        float time = parseFloat(timeStr);
        gpsData.fixTime = (uint32_t)time;  // Integer part (HHMMSS)
        gpsData.fixTimeFractional = time - (uint32_t)time;  // Fractional part (.SS)
    }

    // Field 2: Longitude (decimal degrees)
    if (strlen(fields[2]) > 0)
    {
        gpsData.longitude = atof(fields[2]);
    }

    // Field 3: Latitude (decimal degrees)
    if (strlen(fields[3]) > 0)
    {
        gpsData.latitude = atof(fields[3]);
    }
    
    // Cache NMEA format coordinates
    cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);

    // Field 4: Altitude
    if (strlen(fields[4]) > 0)
    {
        gpsData.altitude = parseFloat(fields[4]);
    }

    // Field 5: Heading
    if (strlen(fields[5]) > 0)
    {
        gpsData.dualHeading = parseFloat(fields[5]);
    }

    // Field 6: Pitch (used as roll in AOG)
    if (strlen(fields[6]) > 0)
    {
        gpsData.dualRoll = parseFloat(fields[6]);
    }

    // Field 10: Position quality (convert to GGA scheme)
    if (strlen(fields[10]) > 0)
    {
        uint8_t ksxtQual = atoi(fields[10]);
        if (ksxtQual == 2)
            ksxtQual = 5; // FLOAT
        if (ksxtQual == 3)
            ksxtQual = 4; // RTK FIX

        gpsData.fixQuality = ksxtQual;
        gpsData.hasPosition = (ksxtQual > 0);
        gpsData.isValid = gpsData.hasPosition;
        gpsData.headingQuality = ksxtQual; // Use same quality for heading
    }

    // Field 8: Speed in km/h - convert to knots
    if (strlen(fields[8]) > 0)
    {
        float speedKmh = parseFloat(fields[8]);
        gpsData.speedKnots = speedKmh * 0.539957f; // Convert km/h to knots
    }

    // Field 13: Number of satellites
    if (fieldCount > 13 && strlen(fields[13]) > 0)
    {
        gpsData.numSatellites = atoi(fields[13]);
    }

    // Note: HDOP not directly available in KSXT, using default
    gpsData.hdop = 0.0f;

    gpsData.hasDualHeading = true;
    gpsData.hasPosition = true;
    gpsData.messageTypeMask |= (1 << 6);  // Set KSXT bit
    
    if (enableDebug)
    {
        logDebug("KSXT processed");
    }

    return true;
}

// Parsing utilities
double GNSSProcessor::parseLatitude(const char *lat, const char *ns)
{
    if (!lat || !ns || strlen(lat) < 4)
        return 0.0;

    // Cache the original NMEA value and direction
    gpsData.latitudeNMEA = atof(lat);
    gpsData.latDir = ns[0];

    double degrees = gpsData.latitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;

    double result = wholeDegrees + (minutes / 60.0);

    if (ns[0] == 'S')
        result = -result;

    return result;
}

double GNSSProcessor::parseLongitude(const char *lon, const char *ew)
{
    if (!lon || !ew || strlen(lon) < 5)
        return 0.0;

    // Cache the original NMEA value and direction
    gpsData.longitudeNMEA = atof(lon);
    gpsData.lonDir = ew[0];

    double degrees = gpsData.longitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;

    double result = wholeDegrees + (minutes / 60.0);

    if (ew[0] == 'W')
        result = -result;

    return result;
}

float GNSSProcessor::parseFloat(const char *str)
{
    return (str && strlen(str) > 0) ? atof(str) : 0.0f;
}

uint32_t GNSSProcessor::parseTime(const char *str)
{
    return (str && strlen(str) > 0) ? atol(str) : 0;
}

uint8_t GNSSProcessor::parseFixQuality(const char *str, bool isGNS)
{
    if (!str || strlen(str) == 0)
        return 0;

    if (isGNS)
    {
        // GNS mode indicator conversion
        switch (str[0])
        {
        case 'A':
            return 1; // Autonomous
        case 'D':
            return 2; // Differential
        case 'F':
            return 5; // Float RTK
        case 'R':
            return 4; // RTK Fixed
        case 'E':
            return 6; // Dead reckoning
        case 'S':
            return 4; // Simulator
        default:
            return 0; // Invalid
        }
    }
    else
    {
        // Standard GGA fix quality
        return atoi(str);
    }
}

uint8_t GNSSProcessor::hexToInt(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

bool GNSSProcessor::isHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

void GNSSProcessor::logDebug(const char *msg)
{
    if (enableDebug)
    {
        LOG_DEBUG(EventSource::GNSS, "%s", msg);
    }
}

uint32_t GNSSProcessor::getDataAge() const
{
    return millis() - gpsData.lastUpdateTime;
}

bool GNSSProcessor::isDataFresh(uint32_t maxAgeMs) const
{
    return getDataAge() <= maxAgeMs;
}


void GNSSProcessor::printData() const
{
    LOG_INFO(EventSource::GNSS, "=== GNSS Data ===");
    LOG_INFO(EventSource::GNSS, "Position: %.6f, %.6f (Alt: %.1fm)",
             gpsData.latitude, gpsData.longitude, gpsData.altitude);
    LOG_INFO(EventSource::GNSS, "Fix: Quality=%d Sats=%d HDOP=%.1f",
             gpsData.fixQuality, gpsData.numSatellites, gpsData.hdop);
    LOG_INFO(EventSource::GNSS, "Speed: %.3f knots, Heading: %.1f°",
             gpsData.speedKnots, gpsData.headingTrue);

    if (gpsData.hasDualHeading)
    {
        LOG_INFO(EventSource::GNSS, "Dual: Heading=%.2f° Roll=%.2f° Quality=%d",
                 gpsData.dualHeading, gpsData.dualRoll, gpsData.headingQuality);
    }

    LOG_INFO(EventSource::GNSS, "Status: Valid=%s Fresh=%s Age=%lums",
             gpsData.isValid ? "Yes" : "No",
             isDataFresh() ? "Yes" : "No",
             getDataAge());
}


bool GNSSProcessor::processUBXByte(uint8_t b)
{
    if (!ubxParser) return false;
    
    // Parse the UBX byte - parse() doesn't return a value, check relPosNedReady flag
    ubxParser->parse(b);
    
    // Check if a new RELPOSNED message was received
    if (ubxParser->relPosNedReady)
    {
        // Extract dual antenna heading and roll from RELPOSNED
        gpsData.dualHeading = ubxParser->ubxData.baseRelH;
        gpsData.dualRoll = ubxParser->ubxData.baseRelRoll;
        gpsData.hasDualHeading = true;
        gpsData.headingQuality = (ubxParser->ubxData.carrSoln > 1) ? 4 : 1; // 4=RTK fixed, 1=float
        gpsData.messageTypeMask |= (1 << 3);  // Set RELPOSNED bit
        
        // Clear the ready flag
        ubxParser->relPosNedReady = false;
        
        if (enableDebug)
        {
            LOG_DEBUG(EventSource::GNSS, "RELPOSNED: Heading=%.2f Roll=%.2f Quality=%d",
                     gpsData.dualHeading, gpsData.dualRoll, gpsData.headingQuality);
        }
        
        return true;
    }
    
    return false;
}

bool GNSSProcessor::parseINSPVAA()
{
    // INSPVAA format (actual from UM981):
    // #INSPVAA,port,seq,idle%,time_status,week,seconds,pos_status,pos_type,reserved;week,seconds,
    // lat,lon,height,north_vel,east_vel,up_vel,roll,pitch,azimuth,status*checksum
    // The latitude starts at field 9 (0-indexed)
    if (fieldCount < 18)
        return false;
    
    // Debug field output - show all fields to debug INS status
    if (enableDebug) {
        LOG_DEBUG(EventSource::GNSS, "INSPVAA Fields (total=%d):", fieldCount);
        for (int i = 0; i < min(fieldCount, 25); i++)
        {
            LOG_DEBUG(EventSource::GNSS, "  [%d]: %s", i, fields[i]);
        }
    }
    
    // Field 12: Latitude (degrees)
    if (strlen(fields[12]) > 0)
    {
        gpsData.latitude = parseFloat(fields[12]);
        gpsData.hasPosition = true;
    }
    
    // Field 13: Longitude (degrees)
    if (strlen(fields[13]) > 0)
    {
        gpsData.longitude = parseFloat(fields[13]);
    }
    
    // Cache NMEA format coordinates
    if (gpsData.hasPosition) {
        cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);
    }
    
    // Field 14: Height (meters)
    if (strlen(fields[14]) > 0)
    {
        gpsData.altitude = parseFloat(fields[14]);
    }
    
    // Field 15,16,17: North, East, Up velocities (m/s)
    if (strlen(fields[15]) > 0 && strlen(fields[16]) > 0 && strlen(fields[17]) > 0)
    {
        gpsData.northVelocity = parseFloat(fields[15]);
        gpsData.eastVelocity = parseFloat(fields[16]);
        gpsData.upVelocity = parseFloat(fields[17]);
        
        // Calculate speed in knots from north/east velocities
        float speedMs = sqrt(gpsData.northVelocity * gpsData.northVelocity + 
                            gpsData.eastVelocity * gpsData.eastVelocity);
        gpsData.speedKnots = speedMs * 1.94384f; // m/s to knots
        gpsData.hasVelocity = true;
    }
    
    // Field 18,19,20: Roll, Pitch, Azimuth (degrees)
    if (strlen(fields[18]) > 0 && strlen(fields[19]) > 0 && strlen(fields[20]) > 0)
    {
        gpsData.insRoll = parseFloat(fields[18]);
        gpsData.insPitch = parseFloat(fields[19]);
        gpsData.insHeading = parseFloat(fields[20]);
        
        // For UM981, use INS heading/roll as dual antenna data
        gpsData.dualHeading = gpsData.insHeading;
        gpsData.dualRoll = gpsData.insRoll;
        gpsData.hasDualHeading = true;
    }
    
    // Debug field count
    if (enableDebug) {
        LOG_DEBUG(EventSource::GNSS, "INSPVAA fieldCount=%d", fieldCount);
    }
    
    // Field 21: INS status (e.g., "INS_ALIGNING", "INS_SOLUTION_GOOD", etc.)
    if (fieldCount > 21 && strlen(fields[21]) > 0)
    {
        // Parse INS alignment status string
        if (strstr(fields[21], "INS_ALIGNING"))
        {
            gpsData.insAlignmentStatus = 7;  // Aligning
            gpsData.fixQuality = 0;  // No fix during alignment
        }
        else if (strstr(fields[21], "INS_SOLUTION_GOOD") || strstr(fields[21], "INS_HIGH_VARIANCE"))
        {
            gpsData.insAlignmentStatus = 3;  // Solution good
            gpsData.fixQuality = 4;  // RTK-like quality for good INS
        }
        else if (strstr(fields[21], "INS_INACTIVE"))
        {
            gpsData.insAlignmentStatus = 0;  // Inactive
            gpsData.fixQuality = 1;  // Basic GPS fix
        }
        else
        {
            gpsData.insAlignmentStatus = 0;  // Unknown
            gpsData.fixQuality = 1;  // Basic fix
        }
        
        // Debug output
        LOG_DEBUG(EventSource::GNSS, "INS Status: '%s' (alignment=%d, fixQuality=%d)", 
                  fields[21], gpsData.insAlignmentStatus, gpsData.fixQuality);
    }
    else
    {
        // Fallback if status field is missing
        gpsData.fixQuality = 1;
        gpsData.insAlignmentStatus = 3;
    }
    
    gpsData.posType = 16; // INS position
    gpsData.insStatus = 1; // Mark as having INS
    
    // Set number of satellites (not directly available in INSPVAA, use a reasonable value)
    gpsData.numSatellites = 12; // Typical for INS solution
    gpsData.hdop = 0.9f; // Good HDOP for INS solution
    
    // Set GPS time from fields 5,6 (week, seconds) from header
    if (strlen(fields[5]) > 0 && strlen(fields[6]) > 0)
    {
        // Store GPS week and seconds for UTC conversion
        gpsData.gpsWeek = (uint16_t)atoi(fields[5]);
        gpsData.gpsSeconds = parseFloat(fields[6]);
        
        // Also store as fixTime for compatibility
        float seconds = gpsData.gpsSeconds;
        int hours = (int)(seconds / 3600) % 24;
        int minutes = (int)((seconds - hours * 3600) / 60);
        float secs = seconds - hours * 3600 - minutes * 60;
        int intSecs = (int)secs;
        gpsData.fixTime = hours * 10000 + minutes * 100 + intSecs;
        gpsData.fixTimeFractional = secs - intSecs;
    }
    
    gpsData.hasINS = true;
    gpsData.isValid = true;
    gpsData.messageTypeMask |= (1 << 7);  // Set INSPVA bit
    
    // Update the last update time - this is critical!
    gpsData.lastUpdateTime = millis();
    
    
    return true;
}

bool GNSSProcessor::parseINSPVAXA()
{
    // INSPVAXA format (from UM981):
    // #INSPVAXA,header,reserved;week,seconds,lat,lon,height,undulation,north_vel,east_vel,up_vel,
    // roll,pitch,azimuth,reserved,lat_std,lon_std,height_std,north_vel_std,east_vel_std,up_vel_std,
    // ext_sol_stat,time_since_update*checksum
    // NOTE: Field 15 is undulation, velocities start at field 16
    
    
    // INSPVAXA typically has 32-33 fields depending on trailing fields
    if (fieldCount < 32)
    {
        if (enableDebug)
        {
            LOG_WARNING(EventSource::GNSS, "INSPVAXA: Not enough fields! Expected 32+, got %d", fieldCount);
        }
        return false;
    }
    
    // Field 10: INS Status - check if still aligning
    bool insAligning = false;
    if (strstr(fields[10], "INS_ALIGNING") != nullptr)
    {
        insAligning = true;
        if (enableDebug)
        {
            LOG_INFO(EventSource::GNSS, "UM981 INS is still aligning - waiting for movement");
        }
    }
    
    // Field 12: Latitude (degrees)
    if (strlen(fields[12]) > 0)
    {
        if (insAligning) {
            // Use Greenwich Observatory coordinates while aligning
            gpsData.latitude = 51.4779;  // Greenwich Observatory latitude
        } else {
            gpsData.latitude = parseFloat(fields[12]);
        }
        // Only mark as having position if not aligning and coords are not 0,0
        gpsData.hasPosition = !insAligning && 
                             (gpsData.latitude != 0.0 || gpsData.longitude != 0.0);
    }
    
    // Field 13: Longitude (degrees)
    if (strlen(fields[13]) > 0)
    {
        if (insAligning) {
            // Use Greenwich Observatory coordinates while aligning
            gpsData.longitude = -0.0015;  // Greenwich Observatory longitude (slightly west)
        } else {
            gpsData.longitude = parseFloat(fields[13]);
        }
    }
    
    // Cache NMEA format coordinates
    cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);
    
    // Field 14: Height (meters)
    if (strlen(fields[14]) > 0)
    {
        if (insAligning) {
            gpsData.altitude = 100.0;  // Distinctive altitude for aligning state
        } else {
            gpsData.altitude = parseFloat(fields[14]);
        }
    }
    
    // Field 15: Undulation (skip)
    // Field 16,17,18: North, East, Up velocities (m/s)
    if (strlen(fields[16]) > 0 && strlen(fields[17]) > 0 && strlen(fields[18]) > 0)
    {
        gpsData.northVelocity = parseFloat(fields[16]);
        gpsData.eastVelocity = parseFloat(fields[17]);
        gpsData.upVelocity = parseFloat(fields[18]);
        
        // Calculate speed in knots from north/east velocities
        float speedMs = sqrt(gpsData.northVelocity * gpsData.northVelocity + 
                            gpsData.eastVelocity * gpsData.eastVelocity);
        gpsData.speedKnots = speedMs * 1.94384f; // m/s to knots
        gpsData.hasVelocity = true;
    }
    
    // Field 19,20,21: Roll, Pitch, Azimuth (degrees)
    if (strlen(fields[19]) > 0 && strlen(fields[20]) > 0 && strlen(fields[21]) > 0)
    {
        gpsData.insRoll = parseFloat(fields[19]);
        gpsData.insPitch = parseFloat(fields[20]);
        gpsData.insHeading = parseFloat(fields[21]);
        
        // For UM981, use INS heading/roll as dual antenna data
        gpsData.dualHeading = gpsData.insHeading;
        gpsData.dualRoll = gpsData.insRoll;
        gpsData.hasDualHeading = true;
    }
    
    // Field 22: Reserved (skip)
    // Field 23,24,25: Position StdDev (lat, lon, height) in meters  
    if (strlen(fields[23]) > 0 && strlen(fields[24]) > 0 && strlen(fields[25]) > 0)
    {
        gpsData.posStdDevLat = parseFloat(fields[23]);
        gpsData.posStdDevLon = parseFloat(fields[24]);
        gpsData.posStdDevAlt = parseFloat(fields[25]);
    }
    
    // Field 26,27,28: Velocity StdDev (north, east, up) in m/s
    if (strlen(fields[26]) > 0 && strlen(fields[27]) > 0 && strlen(fields[28]) > 0)
    {
        gpsData.velStdDevNorth = parseFloat(fields[26]);
        gpsData.velStdDevEast = parseFloat(fields[27]);
        gpsData.velStdDevUp = parseFloat(fields[28]);
    }
    
    // Set fix quality and other parameters based on INS status
    if (insAligning) {
        gpsData.fixQuality = 0;     // No fix while aligning
        gpsData.posType = 0;        // No position type
        gpsData.insStatus = 0;      // INS not ready
        gpsData.numSatellites = 0;  // No solution yet
        gpsData.hdop = 99.9f;       // Poor HDOP
        
        // Important: Mark that we have INS/dual antenna system
        // This allows PAOGI messages to be sent even without fix
        gpsData.hasINS = true;
        gpsData.hasDualHeading = true;
        
        // Clear position but keep message tracking
        gpsData.hasPosition = false;
    } else {
        gpsData.fixQuality = 1;     // Good INS solution (will be updated based on position type)
        gpsData.posType = 16;       // INS position
        gpsData.insStatus = 1;      // Mark as having INS
        gpsData.numSatellites = 12; // Typical for INS solution
        gpsData.hdop = 0.9f;        // Good HDOP for INS solution
        gpsData.hasINS = true;
        gpsData.hasDualHeading = true;
    }
    
    // Set GPS time from fields 5,6 (week, seconds) from header
    if (strlen(fields[5]) > 0 && strlen(fields[6]) > 0)
    {
        // Store GPS week and seconds for UTC conversion
        gpsData.gpsWeek = (uint16_t)atoi(fields[5]);
        gpsData.gpsSeconds = parseFloat(fields[6]);
        
        // Also store as fixTime for compatibility
        float seconds = gpsData.gpsSeconds;
        int hours = (int)(seconds / 3600) % 24;
        int minutes = (int)((seconds - hours * 3600) / 60);
        float secs = seconds - hours * 3600 - minutes * 60;
        int intSecs = (int)secs;
        gpsData.fixTime = hours * 10000 + minutes * 100 + intSecs;
        gpsData.fixTimeFractional = secs - intSecs;
    }
    
    gpsData.hasINS = true;
    gpsData.isValid = true;
    gpsData.messageTypeMask |= (1 << 7);  // Set INSPVA bit
    
    // Update the last update time
    gpsData.lastUpdateTime = millis();
    
    // Debug output
    if (enableDebug)
    {
        LOG_DEBUG(EventSource::GNSS, "INSPVAXA: Lat=%.8f±%.3fm Lon=%.8f±%.3fm Alt=%.1f±%.3fm",
                  gpsData.latitude, gpsData.posStdDevLat, 
                  gpsData.longitude, gpsData.posStdDevLon, 
                  gpsData.altitude, gpsData.posStdDevAlt);
        LOG_DEBUG(EventSource::GNSS, "INSPVAXA: Hdg=%.1f Roll=%.1f Pitch=%.1f VelN=%.2f±%.3f VelE=%.2f±%.3f",
                      gpsData.insHeading, gpsData.insRoll, gpsData.insPitch,
                      gpsData.northVelocity, gpsData.velStdDevNorth,
                      gpsData.eastVelocity, gpsData.velStdDevEast);
    }
    
    return true;
}

// PGN Support Implementation

// External reference to NetworkBase send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

// Get ConfigManager instance
extern ConfigManager configManager;

// Removed registerPGNCallbacks - broadcast PGNs are handled automatically

// Static callback for broadcast PGNs (Hello and Scan Request)
void GNSSProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len)
{
    // Check if this is a Hello PGN
    if (pgn == 200)
    {
        // When we receive a Hello from AgIO, we should respond
        
        // GPS Hello reply format from PGN.md: 
        // Source: 0x78 (120), PGN: 0x78 (120), Length: 5
        uint8_t helloFromGPS[] = {0x80, 0x81, GPS_SOURCE_ID, GPS_HELLO_REPLY, 5, 0, 0, 0, 0, 0, 0};
        
        // Calculate and set CRC
        calculateAndSetCRC(helloFromGPS, sizeof(helloFromGPS));
        
        // Send the reply
        sendUDPbytes(helloFromGPS, sizeof(helloFromGPS));
    }
    // Check if this is a Scan Request PGN
    else if (pgn == 202)
    {
        
        // Subnet GPS reply format from PGN.md:
        // Src: 0x78 (120), PGN: 0xCB (203), Len: 7
        // IP_One, IP_Two, IP_Three, IP_Four, Subnet_One, Subnet_Two, Subnet_Three
        uint8_t ip[4];
        configManager.getIPAddress(ip);
        
        uint8_t subnetReply[] = {
            0x80, 0x81,              // PGN header
            GPS_SOURCE_ID,           // Source: 0x78 (120)
            0xCB,                    // PGN: 203
            7,                       // Data length
            ip[0],  // IP_One
            ip[1],  // IP_Two
            ip[2],  // IP_Three
            ip[3],  // IP_Four
            ip[0],  // Subnet_One
            ip[1],  // Subnet_Two
            ip[2],  // Subnet_Three
            0                        // CRC placeholder
        };
        
        // Calculate and set CRC
        calculateAndSetCRC(subnetReply, sizeof(subnetReply));
        
        // Send the reply
        sendUDPbytes(subnetReply, sizeof(subnetReply));
    }
}

void GNSSProcessor::sendGPSData()
{
    // PGN 214 (0xD6) - GPS data format is complex (51 bytes)
    // Not yet implemented - AgIO doesn't currently support GPS data via PGN
    
    if (!gpsData.isValid)
        return;
        
    // Future implementation will send full GPS data packet
    // Format is defined in PGN.md - Main Antenna section
}

void GNSSProcessor::sendCompleteNMEA()
{
    // Send complete NMEA sentence including checksum with CRLF appended
    // This is called when UDP passthrough is enabled
    // The parseBuffer contains the complete sentence including $ or #, data, *, and checksum
    
    if (bufferIndex == 0) {
        return;
    }
    
    // Build complete sentence with CRLF
    uint8_t sentence[310];  // 300 for parseBuffer + 10 extra for CRLF
    
    // Copy the complete sentence (includes $/#, data, *, and checksum)
    memcpy(sentence, parseBuffer, bufferIndex);
    int len = bufferIndex;
    
    // Add CRLF
    sentence[len++] = '\r';
    sentence[len++] = '\n';
    
    // Send via UDP
    sendUDPbytes(sentence, len);
    
    // Debug logging
    static uint32_t lastPassthroughLog = 0;
    static uint32_t passthroughCount = 0;
    passthroughCount++;
    
    if (millis() - lastPassthroughLog > 5000) {
        lastPassthroughLog = millis();
        LOG_DEBUG(EventSource::GNSS, "UDP Passthrough: %lu sentences sent", passthroughCount);
        passthroughCount = 0;
    }
}

GNSSProcessor::MessageType GNSSProcessor::detectMessageType(const char* msgType) {
    // Fast message type detection using character comparison
    // Skip past the talker ID (GP, GN, etc.) if present
    const char* typeStart = msgType;
    
    // Find where the actual message type starts (after talker ID)
    if (strlen(msgType) >= 5) {
        // Standard NMEA has 2-char talker ID
        if (msgType[2] >= 'A' && msgType[2] <= 'Z') {
            typeStart = msgType + 2;
        }
    }
    
    // Now do fast character comparison
    switch(typeStart[0]) {
        case 'G':
            if (typeStart[1] == 'G' && typeStart[2] == 'A') return MSG_GGA;
            if (typeStart[1] == 'N' && typeStart[2] == 'S') return MSG_GNS;
            break;
            
        case 'V':
            if (typeStart[1] == 'T' && typeStart[2] == 'G') return MSG_VTG;
            break;
            
        case 'R':
            if (typeStart[1] == 'M' && typeStart[2] == 'C') return MSG_RMC;
            break;
            
        case 'H':
            if (typeStart[1] == 'P' && typeStart[2] == 'R') return MSG_HPR;
            break;
            
        case 'K':
            if (typeStart[1] == 'S' && typeStart[2] == 'X' && typeStart[3] == 'T') return MSG_KSXT;
            break;
            
        case 'I':
            if (strncmp(typeStart, "INSPVAA", 7) == 0) return MSG_INSPVAA;
            if (strncmp(typeStart, "INSPVAXA", 8) == 0) return MSG_INSPVAXA;
            break;
            
        case 'B':
            if (strncmp(typeStart, "BESTGNSSPOS", 11) == 0) return MSG_BESTGNSSPOS;
            break;
            
        case 'A':
            if (typeStart[1] == 'V' && typeStart[2] == 'R') return MSG_AVR;
            break;
    }
    
    return MSG_UNKNOWN;
}

void GNSSProcessor::cacheNMEACoordinates(double lat, double lon) {
    // Convert decimal degrees to NMEA format for caching
    
    // Latitude
    gpsData.latDir = (lat < 0) ? 'S' : 'N';
    double absLat = fabs(lat);
    int latDegrees = (int)absLat;
    double latMinutes = (absLat - latDegrees) * 60.0;
    gpsData.latitudeNMEA = latDegrees * 100.0 + latMinutes;
    
    // Longitude
    gpsData.lonDir = (lon < 0) ? 'W' : 'E';
    double absLon = fabs(lon);
    int lonDegrees = (int)absLon;
    double lonMinutes = (absLon - lonDegrees) * 60.0;
    gpsData.longitudeNMEA = lonDegrees * 100.0 + lonMinutes;
}

void GNSSProcessor::parseFieldsZeroCopy() {
    fieldCount = 0;
    
    // Skip the '$' or '#' and start parsing
    const char* fieldStart = parseBuffer + 1;
    
    for (int i = 1; i < bufferIndex && fieldCount < 35; i++) {
        char c = parseBuffer[i];
        
        if (c == ',' || c == ';' || c == '\0') {
            // Found end of field
            fieldRefs[fieldCount].start = fieldStart;
            fieldRefs[fieldCount].length = (parseBuffer + i) - fieldStart;
            fieldCount++;
            
            // Next field starts after the delimiter
            fieldStart = parseBuffer + i + 1;
        }
    }
    
    // Handle the last field if buffer doesn't end with delimiter
    if (fieldStart < parseBuffer + bufferIndex && fieldCount < 35) {
        fieldRefs[fieldCount].start = fieldStart;
        fieldRefs[fieldCount].length = (parseBuffer + bufferIndex) - fieldStart;
        fieldCount++;
    }
}

// Zero-copy utility functions
float GNSSProcessor::parseFloatZeroCopy(const FieldRef& field) {
    if (field.length == 0) return 0.0f;
    
    // Temporarily null-terminate the field
    char saved = field.start[field.length];
    const_cast<char*>(field.start)[field.length] = '\0';
    float result = atof(field.start);
    const_cast<char*>(field.start)[field.length] = saved;
    
    return result;
}

double GNSSProcessor::parseDoubleZeroCopy(const FieldRef& field) {
    if (field.length == 0) return 0.0;
    
    // Temporarily null-terminate the field
    char saved = field.start[field.length];
    const_cast<char*>(field.start)[field.length] = '\0';
    double result = atof(field.start);
    const_cast<char*>(field.start)[field.length] = saved;
    
    return result;
}

int GNSSProcessor::parseIntZeroCopy(const FieldRef& field) {
    if (field.length == 0) return 0;
    
    // Temporarily null-terminate the field
    char saved = field.start[field.length];
    const_cast<char*>(field.start)[field.length] = '\0';
    int result = atoi(field.start);
    const_cast<char*>(field.start)[field.length] = saved;
    
    return result;
}

bool GNSSProcessor::fieldEquals(const FieldRef& field, const char* str) {
    size_t len = strlen(str);
    if (field.length != len) return false;
    return strncmp(field.start, str, len) == 0;
}

bool GNSSProcessor::fieldStartsWith(const FieldRef& field, const char* prefix) {
    size_t len = strlen(prefix);
    if (field.length < len) return false;
    return strncmp(field.start, prefix, len) == 0;
}

double GNSSProcessor::parseLatitudeZeroCopy(const FieldRef& lat, const FieldRef& ns) {
    if (lat.length < 4 || ns.length < 1) return 0.0;
    
    // Cache the original NMEA value and direction
    gpsData.latitudeNMEA = parseDoubleZeroCopy(lat);
    gpsData.latDir = ns.start[0];
    
    double degrees = gpsData.latitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;
    
    double result = wholeDegrees + (minutes / 60.0);
    
    if (ns.start[0] == 'S')
        result = -result;
        
    return result;
}

double GNSSProcessor::parseLongitudeZeroCopy(const FieldRef& lon, const FieldRef& ew) {
    if (lon.length < 5 || ew.length < 1) return 0.0;
    
    // Cache the original NMEA value and direction
    gpsData.longitudeNMEA = parseDoubleZeroCopy(lon);
    gpsData.lonDir = ew.start[0];
    
    double degrees = gpsData.longitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;
    
    double result = wholeDegrees + (minutes / 60.0);
    
    if (ew.start[0] == 'W')
        result = -result;
        
    return result;
}

bool GNSSProcessor::parseGGAZeroCopy() {
    if (fieldCount < 9)
        return false;

    // Field 1: Time (HHMMSS.SS format)
    if (fieldRefs[1].length > 0) {
        float time = parseFloatZeroCopy(fieldRefs[1]);
        gpsData.fixTime = (uint32_t)time;
        gpsData.fixTimeFractional = time - (uint32_t)time;
    }

    // Fields 2-3: Latitude
    gpsData.latitude = parseLatitudeZeroCopy(fieldRefs[2], fieldRefs[3]);

    // Fields 4-5: Longitude
    gpsData.longitude = parseLongitudeZeroCopy(fieldRefs[4], fieldRefs[5]);

    // Field 6: Fix quality
    if (fieldRefs[6].length > 0) {
        gpsData.fixQuality = fieldRefs[6].start[0] - '0';  // Direct digit conversion
    }

    // Field 7: Number of satellites
    gpsData.numSatellites = parseIntZeroCopy(fieldRefs[7]);

    // Field 8: HDOP
    gpsData.hdop = parseFloatZeroCopy(fieldRefs[8]);

    // Field 9: Altitude
    if (fieldCount > 9) {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[9]);
    }

    // Field 13: Age of DGPS
    if (fieldCount > 13) {
        gpsData.ageDGPS = parseIntZeroCopy(fieldRefs[13]);
    }

    // Set status flags
    gpsData.hasPosition = (gpsData.latitude != 0.0 || gpsData.longitude != 0.0) && 
                         gpsData.fixQuality >= 1;
    gpsData.messageTypeMask |= (1 << 0);  // Set GGA bit

    if (enableDebug) {
        logDebug("GGA processed (zero-copy)");
    }

    return true;
}

bool GNSSProcessor::parseGNSZeroCopy() {
    if (fieldCount < 9)
        return false;

    // Field 1: Time (HHMMSS.SS format)
    if (fieldRefs[1].length > 0) {
        float time = parseFloatZeroCopy(fieldRefs[1]);
        gpsData.fixTime = (uint32_t)time;
        gpsData.fixTimeFractional = time - (uint32_t)time;
    }

    // Fields 2-3: Latitude
    gpsData.latitude = parseLatitudeZeroCopy(fieldRefs[2], fieldRefs[3]);

    // Fields 4-5: Longitude
    gpsData.longitude = parseLongitudeZeroCopy(fieldRefs[4], fieldRefs[5]);

    // Field 6: Mode indicator (convert to fix quality)
    if (fieldRefs[6].length > 0) {
        // GNS mode is a string like "AAN" where each char represents GPS/GLONASS/Galileo
        // Convert first character to fix quality
        char mode = fieldRefs[6].start[0];
        switch(mode) {
            case 'N': gpsData.fixQuality = 0; break;  // No fix
            case 'A': gpsData.fixQuality = 1; break;  // Autonomous
            case 'D': gpsData.fixQuality = 2; break;  // Differential
            case 'P': gpsData.fixQuality = 3; break;  // Precise
            case 'R': gpsData.fixQuality = 4; break;  // RTK Fixed
            case 'F': gpsData.fixQuality = 5; break;  // RTK Float
            default:  gpsData.fixQuality = 0; break;
        }
    }

    // Field 7: Number of satellites
    if (fieldCount > 7 && fieldRefs[7].length > 0) {
        gpsData.numSatellites = parseIntZeroCopy(fieldRefs[7]);
    }
    
    // Field 8: HDOP
    if (fieldCount > 8 && fieldRefs[8].length > 0) {
        gpsData.hdop = parseFloatZeroCopy(fieldRefs[8]);
    }

    // Field 9: Altitude
    if (fieldCount > 9 && fieldRefs[9].length > 0) {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[9]);
    }

    // Field 12: Age of DGPS (optional)
    if (fieldCount > 12 && fieldRefs[12].length > 0) {
        gpsData.ageDGPS = parseIntZeroCopy(fieldRefs[12]);
    }

    // Set status flags
    gpsData.hasPosition = (gpsData.latitude != 0.0 || gpsData.longitude != 0.0) && 
                         gpsData.fixQuality >= 1;
    gpsData.messageTypeMask |= (1 << 1);  // Set GNS bit

    if (enableDebug) {
        logDebug("GNS processed (zero-copy)");
    }

    return true;
}

bool GNSSProcessor::parseVTGZeroCopy() {
    if (fieldCount < 8)
        return false;

    // Field 1: Track made good (true)
    if (fieldRefs[1].length > 0) {
        gpsData.headingTrue = parseFloatZeroCopy(fieldRefs[1]);
    }

    // Field 5: Speed over ground in knots
    if (fieldRefs[5].length > 0) {
        gpsData.speedKnots = parseFloatZeroCopy(fieldRefs[5]);
        gpsData.hasVelocity = true;
    }

    // Field 7: Speed over ground in km/h (optional check)
    // We use knots, but can validate if both are present

    // Set status flags
    gpsData.messageTypeMask |= (1 << 2);  // Set VTG bit

    if (enableDebug) {
        logDebug("VTG processed (zero-copy)");
    }

    return true;
}

bool GNSSProcessor::parseHPRZeroCopy() {
    if (fieldCount < 10)
        return false;

    // Field 1: Time (seconds since midnight)
    if (fieldRefs[1].length > 0) {
        float time = parseFloatZeroCopy(fieldRefs[1]);
        gpsData.fixTime = (uint32_t)time;
        gpsData.fixTimeFractional = time - (uint32_t)time;
    }

    // Field 2: Longitude (decimal degrees)
    if (fieldRefs[2].length > 0) {
        gpsData.longitude = parseDoubleZeroCopy(fieldRefs[2]);
    }

    // Field 3: Latitude (decimal degrees)
    if (fieldRefs[3].length > 0) {
        gpsData.latitude = parseDoubleZeroCopy(fieldRefs[3]);
    }
    
    // Cache NMEA format coordinates
    cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);

    // Field 4: Altitude
    if (fieldRefs[4].length > 0) {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[4]);
    }

    // Field 5: Heading (dual antenna)
    if (fieldRefs[5].length > 0) {
        gpsData.dualHeading = parseFloatZeroCopy(fieldRefs[5]);
    }
    // Always set hasDualHeading for HPR messages - even if heading is 0
    gpsData.hasDualHeading = true;

    // Field 6: Pitch
    // Not used for AgOpenGPS

    // Field 7: Roll  
    if (fieldRefs[7].length > 0) {
        gpsData.dualRoll = parseFloatZeroCopy(fieldRefs[7]);
    }

    // Field 8: Quality (0=no fix, 1=single, 2=float, 4=fixed)
    if (fieldRefs[8].length > 0) {
        gpsData.headingQuality = parseIntZeroCopy(fieldRefs[8]);
        
        // Map heading quality to fix quality if we don't have position fix
        if (gpsData.fixQuality == 0 && gpsData.headingQuality >= 2) {
            gpsData.fixQuality = (gpsData.headingQuality == 4) ? 4 : 5; // 4=RTK Fixed, 5=RTK Float
        }
    }

    // Field 9: Number of satellites
    if (fieldRefs[9].length > 0) {
        gpsData.numSatellites = parseIntZeroCopy(fieldRefs[9]);
    }

    // Field 10: Age of DGPS
    if (fieldCount > 10 && fieldRefs[10].length > 0) {
        gpsData.ageDGPS = parseIntZeroCopy(fieldRefs[10]);
    }

    // Set status flags
    gpsData.hasPosition = (gpsData.latitude != 0.0 || gpsData.longitude != 0.0);
    gpsData.messageTypeMask |= (1 << 4);  // Set HPR bit

    if (enableDebug) {
        LOG_DEBUG(EventSource::GNSS, "HPR processed (zero-copy): heading=%.1f, hasDual=%d", 
                  gpsData.dualHeading, gpsData.hasDualHeading);
    }

    return true;
}

bool GNSSProcessor::parseINSPVAAZeroCopy() {
    // INSPVAA format: #INSPVAA,header;week,seconds,lat,lon,height,north_vel,east_vel,up_vel,roll,pitch,azimuth,status*checksum
    // Field 12 starts latitude (after header fields)
    if (fieldCount < 18)
        return false;
    
    // Debug field output
    if (enableDebug) {
        LOG_DEBUG(EventSource::GNSS, "INSPVAA Fields (zero-copy, total=%d)", fieldCount);
    }
    
    // Field 12: Latitude (degrees)
    if (fieldRefs[12].length > 0) {
        gpsData.latitude = parseFloatZeroCopy(fieldRefs[12]);
        gpsData.hasPosition = true;
    }
    
    // Field 13: Longitude (degrees)  
    if (fieldRefs[13].length > 0) {
        gpsData.longitude = parseFloatZeroCopy(fieldRefs[13]);
    }
    
    // Cache NMEA format coordinates
    if (gpsData.hasPosition) {
        cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);
    }
    
    // Field 14: Height (meters)
    if (fieldRefs[14].length > 0) {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[14]);
    }
    
    // Field 15,16,17: North, East, Up velocities (m/s)
    if (fieldRefs[15].length > 0 && fieldRefs[16].length > 0 && fieldRefs[17].length > 0) {
        gpsData.northVelocity = parseFloatZeroCopy(fieldRefs[15]);
        gpsData.eastVelocity = parseFloatZeroCopy(fieldRefs[16]);
        gpsData.upVelocity = parseFloatZeroCopy(fieldRefs[17]);
        
        // Calculate speed in knots from north/east velocities
        float speedMs = sqrt(gpsData.northVelocity * gpsData.northVelocity + 
                            gpsData.eastVelocity * gpsData.eastVelocity);
        gpsData.speedKnots = speedMs * 1.94384f;  // m/s to knots
        gpsData.hasVelocity = true;
    }
    
    // Field 18: Roll (degrees)
    if (fieldRefs[18].length > 0) {
        gpsData.insRoll = parseFloatZeroCopy(fieldRefs[18]);
    }
    
    // Field 19: Pitch (degrees)
    if (fieldRefs[19].length > 0) {
        gpsData.insPitch = parseFloatZeroCopy(fieldRefs[19]);
    }
    
    // Field 20: Azimuth/Heading (degrees)
    if (fieldRefs[20].length > 0) {
        gpsData.insHeading = parseFloatZeroCopy(fieldRefs[20]);
        gpsData.headingTrue = gpsData.insHeading;  // Use INS heading as true heading
    }
    
    // Field 21: INS Status
    if (fieldCount > 21 && fieldRefs[21].length > 0) {
        gpsData.insStatus = parseIntZeroCopy(fieldRefs[21]);
    }
    
    // Field 10: Position type (determines fix quality)
    if (fieldRefs[10].length > 0) {
        gpsData.posType = parseIntZeroCopy(fieldRefs[10]);
        
        // Map position type to fix quality
        switch(gpsData.posType) {
            case 0:  gpsData.fixQuality = 0; break;  // NONE
            case 1:  gpsData.fixQuality = 1; break;  // FIXEDPOS
            case 2:  gpsData.fixQuality = 1; break;  // FIXEDHEIGHT
            case 8:  gpsData.fixQuality = 2; break;  // SINGLE
            case 16: gpsData.fixQuality = 2; break;  // PSRDIFF
            case 17: gpsData.fixQuality = 2; break;  // WAAS
            case 32: gpsData.fixQuality = 5; break;  // L1_FLOAT
            case 34: gpsData.fixQuality = 5; break;  // NARROW_FLOAT
            case 48: gpsData.fixQuality = 4; break;  // L1_INT (Fixed)
            case 50: gpsData.fixQuality = 4; break;  // NARROW_INT (Fixed)
            default: 
                if (gpsData.posType >= 48) {
                    gpsData.fixQuality = 4;  // Various RTK fixed modes
                } else if (gpsData.posType >= 32) {
                    gpsData.fixQuality = 5;  // Various RTK float modes
                } else {
                    gpsData.fixQuality = 1;  // Other modes
                }
                break;
        }
    }
    
    // Extract INS alignment status from field 10 if it contains "INS_"
    if (fieldRefs[10].length > 4 && fieldStartsWith(fieldRefs[10], "INS_")) {
        if (fieldEquals(fieldRefs[10], "INS_INACTIVE")) {
            gpsData.insAlignmentStatus = 0;
        } else if (fieldEquals(fieldRefs[10], "INS_ALIGNING")) {
            gpsData.insAlignmentStatus = 7;
        } else if (fieldEquals(fieldRefs[10], "INS_SOLUTION_GOOD")) {
            gpsData.insAlignmentStatus = 3;
        }
    }
    
    // Set status flags
    gpsData.hasINS = true;
    gpsData.messageTypeMask |= (1 << 7);  // Set INSPVAA bit
    
    if (enableDebug) {
        logDebug("INSPVAA processed (zero-copy)");
    }
    
    return true;
}