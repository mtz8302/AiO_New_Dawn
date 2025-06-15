// Firmware_Teensy_AiO-NG-v6 is copyright 2025 by the AOG Group
// GNSSProcessor - Pure NMEA parsing to structured data

#ifndef GNSS_PROCESSOR_H
#define GNSS_PROCESSOR_H

#include "Arduino.h"
#include <stdint.h>

// Forward declaration
class UBX_Parser;

class GNSSProcessor
{
public:
    // Clean GPS data structure
    struct GNSSData
    {
        // Position data
        double latitude;  // decimal degrees
        double longitude; // decimal degrees
        float altitude;   // meters
        uint32_t fixTime; // HHMMSS as integer

        // Quality indicators
        uint8_t fixQuality; // 0=invalid, 1=GPS, 2=DGPS, 4=RTK, 5=Float
        uint8_t numSatellites;
        float hdop;
        uint16_t ageDGPS; // seconds since last DGPS update

        // Velocity data
        float speedKnots;
        float headingTrue; // degrees

        // Dual GPS data (from HPR messages)
        float dualHeading;      // degrees
        float dualRoll;         // degrees
        uint8_t headingQuality; // dual GPS solution quality

        // Status flags
        uint32_t lastUpdateTime;
        bool isValid;
        bool hasPosition;
        bool hasVelocity;
        bool hasDualHeading;
    };

    // Statistics
    struct Statistics
    {
        uint32_t messagesProcessed;
        uint32_t parseErrors;
        uint32_t checksumErrors;
        uint32_t ggaCount;
        uint32_t gnsCount;
        uint32_t vtgCount;
        uint32_t hprCount;
        uint32_t ksxtCount;
    };

private:
    // NMEA parsing state machine
    enum ParseState
    {
        WAIT_START,
        READ_DATA,
        READ_CHECKSUM
    };

    // Parse buffer and state
    char parseBuffer[200];
    uint8_t bufferIndex;
    ParseState state;
    uint8_t calculatedChecksum;
    uint8_t receivedChecksum;
    uint8_t checksumIndex;

    // Field parsing
    char fields[20][16]; // Max 20 fields, 16 chars each
    uint8_t fieldCount;

    // Data storage
    GNSSData gpsData;
    Statistics stats;

    // Configuration
    bool enableNoiseFilter;
    bool enableDebug;
    
    // UBX parser for GPS2 RELPOSNED
    UBX_Parser* ubxParser;

    // Internal parsing methods
    void resetParser();
    bool validateChecksum();
    void parseFields();
    bool processMessage();

    // Message handlers
    bool parseGGA();
    bool parseGNS();
    bool parseVTG();
    bool parseHPR();
    bool parseKSXT();

    // Field parsing utilities
    double parseLatitude(const char *lat, const char *ns);
    double parseLongitude(const char *lon, const char *ew);
    float parseFloat(const char *str);
    uint32_t parseTime(const char *str);
    uint8_t parseFixQuality(const char *str, bool isGNS = false);
    uint8_t hexToInt(char c);
    bool isHex(char c);
    void logDebug(const char *msg);

public:
    GNSSProcessor();
    ~GNSSProcessor();

    // Initialization and setup
    bool init();
    bool setup(bool enableDebug = false, bool enableNoiseFilter = true);

    // Configuration
    void setNoiseFilter(bool enable) { enableNoiseFilter = enable; }
    void setDebug(bool enable) { enableDebug = enable; }

    // Main processing - single character input
    bool processNMEAChar(char c);
    bool processUBXByte(uint8_t b);

    // Batch processing
    uint16_t processNMEAStream(const char *data, uint16_t length);

    // Data access
    const GNSSData &getData() const { return gpsData; }
    bool isValid() const { return gpsData.isValid; }
    bool hasPosition() const { return gpsData.hasPosition; }
    bool hasVelocity() const { return gpsData.hasVelocity; }
    bool hasDualHeading() const { return gpsData.hasDualHeading; }

    // Status checking
    uint32_t getDataAge() const;
    bool isDataFresh(uint32_t maxAgeMs = 2000) const;

    // Statistics
    const Statistics &getStats() const { return stats; }
    float getSuccessRate() const;
    void resetStats();

    // Debug output
    void printData() const;
    void printStats() const;
};

// Global pointer following established pattern
extern GNSSProcessor *gnssPTR;

#endif // GNSS_PROCESSOR_H