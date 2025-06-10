#include "GNSSProcessor.h"
#include <string.h>
#include <math.h>

// Global pointer
GNSSProcessor *gnssPTR = nullptr;

GNSSProcessor::GNSSProcessor() : bufferIndex(0),
                                 state(WAIT_START),
                                 calculatedChecksum(0),
                                 receivedChecksum(0),
                                 checksumIndex(0),
                                 fieldCount(0),
                                 enableNoiseFilter(true),
                                 enableDebug(false)
{

    // Initialize data structures
    memset(&gpsData, 0, sizeof(gpsData));
    memset(&stats, 0, sizeof(stats));

    gpsData.hdop = 99.9f;
    resetParser();
}

GNSSProcessor::~GNSSProcessor()
{
    // Cleanup if needed
}

bool GNSSProcessor::init()
{
    resetStats();
    resetParser();
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
            Serial.println("GNSS Processor init failed");
        }
        return false;
    }

    if (enableDebug)
    {
        Serial.println("GNSS Processor initialized successfully");
    }

    return true;
}

bool GNSSProcessor::processNMEAChar(char c)
{
    switch (state)
    {
    case WAIT_START:
        if (c == '$')
        {
            resetParser();
            state = READ_DATA;
            calculatedChecksum = 0;
            parseBuffer[bufferIndex++] = c;
        }
        break;

    case READ_DATA:
        if (c == '*')
        {
            parseBuffer[bufferIndex] = '\0';
            state = READ_CHECKSUM;
            receivedChecksum = 0;
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
                calculatedChecksum ^= c;
            }
        }
        break;

    case READ_CHECKSUM:
        if (isHex(c))
        {
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
                    stats.checksumErrors++;
                    resetParser();
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
    memset(parseBuffer, 0, sizeof(parseBuffer));
    memset(fields, 0, sizeof(fields));
}

bool GNSSProcessor::validateChecksum()
{
    return calculatedChecksum == receivedChecksum;
}

bool GNSSProcessor::processMessage()
{
    parseFields();

    if (fieldCount < 1)
    {
        stats.parseErrors++;
        resetParser();
        return false;
    }

    // Determine message type and process
    const char *msgType = fields[0];
    bool processed = false;

    if (strstr(msgType, "GGA"))
    {
        processed = parseGGA();
        if (processed)
            stats.ggaCount++;
    }
    else if (strstr(msgType, "GNS"))
    {
        processed = parseGNS();
        if (processed)
            stats.gnsCount++;
    }
    else if (strstr(msgType, "VTG"))
    {
        processed = parseVTG();
        if (processed)
            stats.vtgCount++;
    }
    else if (strstr(msgType, "HPR"))
    {
        processed = parseHPR();
        if (processed)
            stats.hprCount++;
    }
    else if (strstr(msgType, "KSXT"))
    {
        processed = parseKSXT();
        if (processed)
            stats.ksxtCount++;
    }

    if (processed)
    {
        stats.messagesProcessed++;
        gpsData.lastUpdateTime = millis();
    }
    else
    {
        stats.parseErrors++;
    }

    resetParser();
    return processed;
}

void GNSSProcessor::parseFields()
{
    fieldCount = 0;
    uint8_t fieldIndex = 0;

    // Skip the '$' and parse comma-separated fields
    for (int i = 1; i < bufferIndex && fieldCount < 20; i++)
    {
        char c = parseBuffer[i];

        if (c == ',' || c == '\0')
        {
            fields[fieldCount][fieldIndex] = '\0';
            fieldCount++;
            fieldIndex = 0;
        }
        else if (fieldIndex < 15)
        {
            fields[fieldCount][fieldIndex++] = c;
        }
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

    gpsData.hasPosition = (gpsData.fixQuality > 0);
    gpsData.isValid = gpsData.hasPosition;

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
        Serial.printf("%lu GNSS: %s\r\n", millis(), msg);
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

float GNSSProcessor::getSuccessRate() const
{
    uint32_t total = stats.messagesProcessed + stats.parseErrors + stats.checksumErrors;
    return total > 0 ? (float)stats.messagesProcessed / total * 100.0f : 0.0f;
}

void GNSSProcessor::resetStats()
{
    memset(&stats, 0, sizeof(stats));
}

void GNSSProcessor::printData() const
{
    Serial.println("=== GNSS Data ===");
    Serial.printf("Position: %.6f, %.6f (Alt: %.1fm)\r\n",
                  gpsData.latitude, gpsData.longitude, gpsData.altitude);
    Serial.printf("Fix: Quality=%d Sats=%d HDOP=%.1f\r\n",
                  gpsData.fixQuality, gpsData.numSatellites, gpsData.hdop);
    Serial.printf("Speed: %.3f knots, Heading: %.1f°\r\n",
                  gpsData.speedKnots, gpsData.headingTrue);

    if (gpsData.hasDualHeading)
    {
        Serial.printf("Dual: Heading=%.2f° Roll=%.2f° Quality=%d\r\n",
                      gpsData.dualHeading, gpsData.dualRoll, gpsData.headingQuality);
    }

    Serial.printf("Status: Valid=%s Fresh=%s Age=%lums\r\n",
                  gpsData.isValid ? "Yes" : "No",
                  isDataFresh() ? "Yes" : "No",
                  getDataAge());
}

void GNSSProcessor::printStats() const
{
    Serial.println("=== GNSS Statistics ===");
    Serial.printf("Messages: Total=%lu Success=%.1f%%\r\n",
                  stats.messagesProcessed, getSuccessRate());
    Serial.printf("Errors: Parse=%lu Checksum=%lu\r\n",
                  stats.parseErrors, stats.checksumErrors);
    Serial.printf("Types: GGA=%lu GNS=%lu VTG=%lu HPR=%lu KSXT=%lu\r\n",
                  stats.ggaCount, stats.gnsCount, stats.vtgCount,
                  stats.hprCount, stats.ksxtCount);
}