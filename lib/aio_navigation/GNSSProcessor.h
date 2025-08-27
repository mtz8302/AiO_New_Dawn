// Firmware_Teensy_AiO-NG-v6 is copyright 2025 by the AOG Group
// GNSSProcessor - Pure NMEA parsing to structured data

#ifndef GNSS_PROCESSOR_H
#define GNSS_PROCESSOR_H

#include "Arduino.h"
#include <stdint.h>
#include "PGNProcessor.h"
#include "GPSTimingDiagnostics.h"

// PGN Constants for GPS module
constexpr uint8_t GPS_SOURCE_ID = 0x78;     // 120 decimal - GPS source address (from PGN.md GPS Reply)
constexpr uint8_t GPS_PGN_DATA = 0xD6;      // 214 decimal - GPS data PGN
constexpr uint8_t GPS_HELLO_REPLY = 0x78;   // 120 decimal - GPS hello reply

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
        
        // GPS time data (for UTC conversion)
        uint16_t gpsWeek;    // GPS week number
        float gpsSeconds;    // Seconds of week

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
        
        // INS data (from INSPVAA/INSPVAXA messages)
        float insPitch;         // degrees
        float insRoll;          // degrees  
        float insHeading;       // degrees (azimuth)
        float northVelocity;    // m/s
        float eastVelocity;     // m/s
        float upVelocity;       // m/s
        uint32_t insStatus;     // INS status word
        uint8_t posType;        // Position type (for INS quality)
        uint8_t insAlignmentStatus; // INS alignment status (0=inactive, 3=good, 7=aligning)
        
        // Extended INS data (from INSPVAXA)
        float posStdDevLat;     // Position std dev latitude (m)
        float posStdDevLon;     // Position std dev longitude (m)
        float posStdDevAlt;     // Position std dev altitude (m)
        float velStdDevNorth;   // Velocity std dev north (m/s)
        float velStdDevEast;    // Velocity std dev east (m/s)
        float velStdDevUp;      // Velocity std dev up (m/s)

        // Status flags
        uint32_t lastUpdateTime;
        bool isValid;           // Deprecated - use hasFix instead
        bool hasPosition;       // Has lat/lon data with good fix
        bool hasVelocity;
        bool hasDualHeading;
        bool hasINS;            // Has INS data from INSPVAA/INSPVAXA
        
        // Message tracking (bit mask)
        // Bit 0: GGA, Bit 1: VTG, Bit 2: GNS
        // Bit 3: RELPOSNED, Bit 4: PVT
        // Bit 5: HPR, Bit 6: KSXT
        // Bit 7: INSPVA/INSPVAXA
        uint8_t messageTypeMask;
    };

    // UDP passthrough control
    void setUDPPassthrough(bool enabled) { udpPassthroughEnabled = enabled; }
    bool isUDPPassthroughEnabled() const { return udpPassthroughEnabled; }

private:
    // NMEA parsing state machine
    enum ParseState
    {
        WAIT_START,
        READ_DATA,
        READ_CHECKSUM
    };

    // Parse buffer and state
    char parseBuffer[300];  // Increased for INSPVAXA messages
    uint16_t bufferIndex;  // Changed from uint8_t to support messages > 255 bytes
    ParseState state;
    uint8_t calculatedChecksum;
    uint8_t receivedChecksum;
    uint32_t receivedChecksum32;  // For Unicore 32-bit CRC
    uint8_t checksumIndex;
    bool isUnicoreMessage;        // Track if current message starts with #

    // Field parsing
    char fields[35][24]; // Max 35 fields, 24 chars each (increased for INSPVAXA)
    
#ifdef GPS_TIMING_DEBUG
    uint32_t sentenceStartMicros = 0;  // Timestamp when sentence starts
#endif
    uint8_t fieldCount;

    // Data storage
    GNSSData gpsData;

    // Configuration
    bool enableNoiseFilter;
    bool enableDebug;
    
    // UBX parser for GPS2 RELPOSNED
    UBX_Parser* ubxParser;
    
    // UDP passthrough
    bool udpPassthroughEnabled;

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
    bool parseINSPVAA();
    bool parseINSPVAXA();
    
    // UDP passthrough
    void sendNMEAViaUDP();

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
    bool isValid() const { return gpsData.isValid; }  // Deprecated - use hasFix()
    bool hasGPS() const { return gpsData.messageTypeMask > 0 && (millis() - gpsData.lastUpdateTime < 5000); }
    bool hasFix() const { return gpsData.hasPosition && gpsData.fixQuality > 0; }
    bool hasPosition() const { return gpsData.hasPosition; }
    bool hasVelocity() const { return gpsData.hasVelocity; }
    bool hasDualHeading() const { return gpsData.hasDualHeading; }
    bool hasINS() const { return gpsData.hasINS; }

    // Status checking
    uint32_t getDataAge() const;
    bool isDataFresh(uint32_t maxAgeMs = 2000) const;


    // Debug output
    void printData() const;
    
    // PGN support
    // registerPGNCallbacks removed - broadcast PGNs handled automatically
    void sendGPSData();  // Send PGN 214 (0xD6) - for future use
    
    // Static callback for PGN Hello (200)
    static void handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len);
};

// Global instance following established pattern
extern GNSSProcessor gnssProcessor;

#endif // GNSS_PROCESSOR_H