#include "GNSSProcessor.h"
#include "UBXParser.h"
#include "calc_crc32.h"
#include "PGNUtils.h"
#include "EventLogger.h"
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
                                 ubxParser(nullptr)
{

    // Initialize data structures
    memset(&gpsData, 0, sizeof(gpsData));
    // Initialize data

    gpsData.hdop = 99.9f;
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
    
    // Register with PGNProcessor to receive broadcast messages
    if (PGNProcessor::instance)
    {
        // Register using GPS_SOURCE_ID (120) to receive broadcast PGNs like 200 and 202
        bool success = PGNProcessor::instance->registerCallback(GPS_SOURCE_ID, handleBroadcastPGN, "GPS Handler");
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
            parseBuffer[bufferIndex] = '\0';
            state = READ_CHECKSUM;
            receivedChecksum = 0;
            receivedChecksum32 = 0;
            checksumIndex = 0;
        }
        else if (c == '\r' || c == '\n')
        {
            // Message without checksum
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
            if (isUnicoreMessage)
            {
                // Unicore uses 32-bit CRC (8 hex digits)
                if (checksumIndex < 8)
                {
                    receivedChecksum32 = (receivedChecksum32 << 4) | hexToInt(c);
                    checksumIndex++;
                    if (checksumIndex == 8)
                    {
                        if (validateChecksum())
                        {
                            return processMessage();
                        }
                        else
                        {
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
                    if (validateChecksum())
                    {
                        return processMessage();
                    }
                    else
                    {
                        resetParser();
                    }
                }
            }
        }
        else if (c == '\r' || c == '\n')
        {
            resetParser();
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
    
    parseFields();

    if (fieldCount < 1)
    {
        resetParser();
        return false;
    }

    // Determine message type and process
    const char *msgType = fields[0];
    bool processed = false;
    
    // Debug enabled for testing
    if (enableDebug)
    {
        LOG_DEBUG(EventSource::GNSS, "Message type: %s, fields: %d", msgType, fieldCount);
    }

    if (strstr(msgType, "GGA"))
    {
        processed = parseGGA();
    }
    else if (strstr(msgType, "GNS"))
    {
        processed = parseGNS();
    }
    else if (strstr(msgType, "VTG"))
    {
        processed = parseVTG();
    }
    else if (strstr(msgType, "HPR"))
    {
        processed = parseHPR();
    }
    else if (strstr(msgType, "KSXT"))
    {
        processed = parseKSXT();
    }
    else if (strstr(msgType, "INSPVAA"))
    {
        processed = parseINSPVAA();
    }
    else if (strstr(msgType, "INSPVAXA"))
    {
        if (enableDebug)
        {
            LOG_DEBUG(EventSource::GNSS, "INSPVAXA detected, fieldCount=%d, bufferIndex=%d", fieldCount, bufferIndex);
        }
        processed = parseINSPVAXA();
        if (processed)
        {
            if (enableDebug)
            {
                LOG_DEBUG(EventSource::GNSS, "INSPVAXA parsed successfully");
            }
        }
        else if (enableDebug)
        {
            LOG_DEBUG(EventSource::GNSS, "INSPVAXA parse failed");
        }
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

    // Field 1: Time
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

    // Field 1: Time
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
    }

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

    double degrees = atof(lat) / 100.0;
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

    double degrees = atof(lon) / 100.0;
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
        
        // Debug output - always show INS status for now
        LOG_INFO(EventSource::GNSS, "INS Status: '%s' (alignment=%d, fixQuality=%d)", 
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
        int secs = (int)(seconds) % 60;
        gpsData.fixTime = hours * 10000 + minutes * 100 + secs;
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
        int secs = (int)(seconds) % 60;
        gpsData.fixTime = hours * 10000 + minutes * 100 + secs;
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

// External network config
extern struct NetConfigStruct {
    uint8_t currentIP[5];
    uint8_t gatewayIP[5];
    uint8_t broadcastIP[5];
} netConfig;

// Static instance pointer for callback access
static GNSSProcessor* gnssInstance = nullptr;

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
        uint8_t subnetReply[] = {
            0x80, 0x81,              // PGN header
            GPS_SOURCE_ID,           // Source: 0x78 (120)
            0xCB,                    // PGN: 203
            7,                       // Data length
            netConfig.currentIP[0],  // IP_One
            netConfig.currentIP[1],  // IP_Two
            netConfig.currentIP[2],  // IP_Three
            netConfig.currentIP[3],  // IP_Four
            netConfig.currentIP[0],  // Subnet_One
            netConfig.currentIP[1],  // Subnet_Two
            netConfig.currentIP[2],  // Subnet_Three
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
    // For now this is just a placeholder for future implementation
    // when AgIO supports GPS data via PGN
    
    if (!gpsData.isValid)
        return;
        
    // Future implementation will send full GPS data packet
    // Format is defined in PGN.md - Main Antenna section
}