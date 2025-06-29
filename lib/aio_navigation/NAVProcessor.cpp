#include "NAVProcessor.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "EventLogger.h"

// External processor instances from main.cpp
extern GNSSProcessor gnssProcessor;
extern IMUProcessor imuProcessor;

// External UDP send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

// Static instance
NAVProcessor* NAVProcessor::instance = nullptr;

NAVProcessor::NAVProcessor() {
    instance = this;
    
    // Initialize timing
    lastMessageTime = 0;
    
    // Clear message buffer
    memset(messageBuffer, 0, BUFFER_SIZE);
    
    LOG_INFO(EventSource::GNSS, "NAVProcessor initialized");
    
    // Report what we see from GNSSProcessor during startup
    const auto& gnssData = gnssProcessor.getData();
    
    if (gnssData.messageTypeMask == 0) {
        LOG_DEBUG(EventSource::GNSS, "  No NMEA data available yet");
    } else {
        LOG_DEBUG(EventSource::GNSS, "  NMEA messages detected (mask=0x%02X)", gnssData.messageTypeMask);
        
        if (!gnssData.hasPosition) {
            LOG_DEBUG(EventSource::GNSS, "  No position fix (quality=%d, sats=%d)", 
                     gnssData.fixQuality, gnssData.numSatellites);
        } else {
            if (gnssData.hasDualHeading) {
                LOG_DEBUG(EventSource::GNSS, "  Dual antenna mode (heading=%.1f°)", gnssData.dualHeading);
            } else if (gnssData.hasINS) {
                LOG_DEBUG(EventSource::GNSS, "  INS mode (align status=%d)", gnssData.insAlignmentStatus);
            } else {
                LOG_DEBUG(EventSource::GNSS, "  Single antenna mode");
            }
        }
    }
}

NAVProcessor::~NAVProcessor() {
    instance = nullptr;
}

NAVProcessor* NAVProcessor::getInstance() {
    return instance;
}

void NAVProcessor::init() {
    if (instance == nullptr) {
        instance = new NAVProcessor();
        // Note: navPTR is set in main.cpp after calling init()
    }
}

NavMessageType NAVProcessor::selectMessageType() {
    const auto& gnssData = gnssProcessor.getData();
    
    // For dual/INS systems, send PAOGI even without fix (for INS_ALIGNING state)
    if (gnssData.hasDualHeading || gnssData.hasINS) {
        return NavMessageType::PAOGI;  // Dual GPS/INS detected
    }
    
    // For single GPS, require fix
    if (!gnssProcessor.hasFix()) {
        return NavMessageType::NONE;
    }
    
    return NavMessageType::PANDA;  // Single GPS with fix
}

void NAVProcessor::convertToNMEACoordinates(double decimalDegrees, bool isLongitude, 
                                           double& nmeaValue, char& direction) {
    // Determine direction
    if (isLongitude) {
        direction = (decimalDegrees < 0) ? 'W' : 'E';
    } else {
        direction = (decimalDegrees < 0) ? 'S' : 'N';
    }
    
    // Work with absolute value
    double absDegrees = fabs(decimalDegrees);
    
    // Extract degrees and minutes
    int degrees = (int)absDegrees;
    double minutes = (absDegrees - degrees) * 60.0;
    
    // Format as DDDMM.MMMMM or DDMM.MMMMM
    nmeaValue = degrees * 100.0 + minutes;
}

uint8_t NAVProcessor::calculateNMEAChecksum(const char* sentence) {
    uint8_t checksum = 0;
    
    // Skip the $ and calculate XOR up to * or end
    const char* p = sentence + 1;
    while (*p && *p != '*') {
        checksum ^= *p++;
    }
    
    return checksum;
}

float NAVProcessor::convertGPStoUTC(uint16_t gpsWeek, float gpsSeconds) {
    // GPS epoch (January 6, 1980) to Unix epoch (January 1, 1970) offset
    const uint32_t GPS_EPOCH_OFFSET = 315964800UL;
    
    // Current leap seconds as of 2024
    const uint8_t LEAP_SECONDS = 18;
    
    // Calculate total seconds since GPS epoch
    uint32_t totalGPSSeconds = (uint32_t)gpsWeek * 7 * 24 * 60 * 60 + (uint32_t)gpsSeconds;
    
    // Convert to Unix time and adjust for leap seconds
    uint32_t unixTime = GPS_EPOCH_OFFSET + totalGPSSeconds - LEAP_SECONDS;
    
    // Extract UTC time components
    uint32_t secondsToday = unixTime % (24 * 60 * 60);
    uint8_t hours = secondsToday / 3600;
    uint8_t minutes = (secondsToday % 3600) / 60;
    uint8_t seconds = secondsToday % 60;
    
    // Get milliseconds from the fractional part of gpsSeconds
    float fractionalSeconds = gpsSeconds - (uint32_t)gpsSeconds;
    
    // Return as HHMMSS.S format
    return hours * 10000.0f + minutes * 100.0f + seconds + fractionalSeconds;
}

bool NAVProcessor::formatPANDAMessage() {
    if (!gnssProcessor.isValid()) {
        return false;
    }
    
    const auto& gnssData = gnssProcessor.getData();
    
    // Convert coordinates to NMEA format
    double latNMEA, lonNMEA;
    char latDir, lonDir;
    convertToNMEACoordinates(gnssData.latitude, false, latNMEA, latDir);
    convertToNMEACoordinates(gnssData.longitude, true, lonNMEA, lonDir);
    
    // Get IMU data if available - using strings like old code
    char imuHeading[10] = "65535";  // Default "no IMU" value
    char imuRoll[10] = "0";         // Default "no IMU" value
    char imuPitch[10] = "0";        // Default "no IMU" value
    char imuYawRate[10] = "0";      // Default "no IMU" value
    
    if (imuProcessor.hasValidData()) {
        const auto& imuData = imuProcessor.getCurrentData();
        snprintf(imuHeading, sizeof(imuHeading), "%d", (int)(imuData.heading * 10.0));
        snprintf(imuRoll, sizeof(imuRoll), "%d", (int)round(imuData.roll));
        snprintf(imuPitch, sizeof(imuPitch), "%d", (int)round(imuData.pitch));
        snprintf(imuYawRate, sizeof(imuYawRate), "%.2f", imuData.yawRate);
    }
    
    // Format time (HHMMSS.S)
    // fixTime is stored as HHMMSS integer, convert to float with decimal
    float timeFloat = gnssData.fixTime + (millis() % 1000) / 1000.0;
    
    // Build PANDA message without checksum
    int len = snprintf(messageBuffer, BUFFER_SIZE - 4,
        "$PANDA,%.1f,%.6f,%c,%.6f,%c,%d,%d,%.1f,%.3f,%.1f,%.3f,%s,%s,%s,%s",
        timeFloat,                          // Time
        latNMEA, latDir,                   // Latitude
        lonNMEA, lonDir,                   // Longitude
        gnssData.fixQuality,               // Fix quality
        gnssData.numSatellites,            // Satellites
        gnssData.hdop,                     // HDOP
        gnssData.altitude,                 // Altitude
        (float)gnssData.ageDGPS,           // Age DGPS (always send value with decimal)
        gnssData.speedKnots,               // Speed
        imuHeading,                        // IMU Heading (65535 = no IMU)
        imuRoll,                           // IMU Roll
        imuPitch,                          // IMU Pitch
        imuYawRate                         // IMU Yaw Rate
    );
    
    // Calculate and append checksum
    uint8_t checksum = calculateNMEAChecksum(messageBuffer);
    snprintf(messageBuffer + len, 5, "*%02X", checksum);
    
    return true;
}

bool NAVProcessor::formatPAOGIMessage() {
    if (!gnssProcessor.getData().hasDualHeading) {
        return false;
    }
    // Allow PAOGI even without valid position for INS_ALIGNING state
    
    const auto& gnssData = gnssProcessor.getData();
    
    // Convert coordinates to NMEA format
    double latNMEA, lonNMEA;
    char latDir, lonDir;
    convertToNMEACoordinates(gnssData.latitude, false, latNMEA, latDir);
    convertToNMEACoordinates(gnssData.longitude, true, lonNMEA, lonDir);
    
    // Get IMU data if available (for pitch and yaw rate)
    int16_t pitch = -1;
    float yawRate = 0.0;
    
    // Prefer INS pitch if available (from UM981)
    if (gnssData.hasINS) {
        pitch = (int16_t)round(gnssData.insPitch);
    }
    else if (imuProcessor.hasValidData()) {
        const auto& imuData = imuProcessor.getCurrentData();
        pitch = (int16_t)round(imuData.pitch);
        yawRate = imuData.yawRate;
    }
    
    // Use dual GPS heading and roll
    float dualHeading = gnssData.dualHeading;
    int16_t roll = (int16_t)round(gnssData.dualRoll);
    
    // Format time - use UTC from GPS week/seconds if available
    float timeFloat;
    if (gnssData.gpsWeek > 0 && gnssData.gpsSeconds > 0) {
        // Convert GPS time to UTC
        timeFloat = convertGPStoUTC(gnssData.gpsWeek, gnssData.gpsSeconds);
    } else {
        // Fallback to fixTime with milliseconds
        timeFloat = gnssData.fixTime + (millis() % 1000) / 1000.0;
    }
    
    // Build PAOGI message without checksum
    // NMEA format: Lat DDMM.MMMMM (4 digits before decimal), Lon DDDMM.MMMMM (5 digits before decimal)
    int len = snprintf(messageBuffer, BUFFER_SIZE - 4,
        "$PAOGI,%.1f,%010.6f,%c,%011.6f,%c,%d,%d,%.1f,%.3f,%.1f,%.3f,%.1f,%d,%d,%.2f",
        timeFloat,                          // Time
        latNMEA, latDir,                   // Latitude
        lonNMEA, lonDir,                   // Longitude
        gnssData.fixQuality,               // Fix quality
        gnssData.numSatellites,            // Satellites
        gnssData.hdop,                     // HDOP
        gnssData.altitude,                 // Altitude
        (float)gnssData.ageDGPS,           // Age DGPS
        gnssData.speedKnots,               // Speed
        // Empty field for heading (double comma)
        dualHeading,                       // Dual heading in degrees
        roll,                              // Dual roll
        pitch,                             // Pitch (from IMU)
        yawRate                            // Yaw rate (from IMU)
    );
    
    // Calculate and append checksum
    uint8_t checksum = calculateNMEAChecksum(messageBuffer);
    snprintf(messageBuffer + len, 5, "*%02X", checksum);
    
    return true;
}

void NAVProcessor::sendMessage(const char* message) {
    // NMEA messages must end with CR+LF
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s\r\n", message);
    
    // Send via UDP to AgIO
    sendUDPbytes((uint8_t*)buffer, strlen(buffer));
    
}

void NAVProcessor::process() {
    // For single antenna systems, we need at least position data
    // For dual/INS systems, we can send messages even without full fix (for alignment)
    const auto& gnssData = gnssProcessor.getData();
    bool isDualSystem = gnssData.hasDualHeading || gnssData.hasINS;
    
    if (!isDualSystem && !gnssData.hasPosition) {
        return;  // Don't send messages yet
    }
    
    // Check if it's time to send a message
    if (timeSinceLastMessage < MESSAGE_INTERVAL_MS) {
        return;
    }
    
    timeSinceLastMessage = 0;
    
    // Select and format appropriate message type
    NavMessageType msgType = selectMessageType();
    bool success = false;
    
    // Track message type changes
    static NavMessageType lastMsgType = NavMessageType::NONE;
    static uint32_t messageTypeStartTime = 0;
    
    if (msgType != lastMsgType) {
        if (msgType != NavMessageType::NONE) {
            LOG_INFO(EventSource::GNSS, "Switching to %s messages", 
                     msgType == NavMessageType::PANDA ? "PANDA" : "PAOGI");
        }
        lastMsgType = msgType;
        messageTypeStartTime = millis();
    }
    
    switch (msgType) {
        case NavMessageType::PANDA:
            success = formatPANDAMessage();
            if (success) {
                sendMessage(messageBuffer);
                // Message sent successfully
                
            }
            break;
            
        case NavMessageType::PAOGI:
            success = formatPAOGIMessage();
            if (success) {
                sendMessage(messageBuffer);
                // Message sent successfully
                
            }
            break;
            
        case NavMessageType::NONE:
        default:
            // No valid data to send
            break;
    }
    
    if (!success && msgType != NavMessageType::NONE) {
        // Message format error occurred
        LOG_ERROR(EventSource::GNSS, "Failed to format %s message", 
                  msgType == NavMessageType::PANDA ? "PANDA" : "PAOGI");
    }
    
    lastMessageTime = millis();
}

void NAVProcessor::setMessageRate(uint32_t intervalMs) {
    // Clamp to reasonable values (1Hz to 100Hz)
    if (intervalMs >= 10 && intervalMs <= 1000) {
        const_cast<uint32_t&>(MESSAGE_INTERVAL_MS) = intervalMs;
    }
}

NavMessageType NAVProcessor::getCurrentMessageType() {
    return selectMessageType();
}

// Removed complex detection methods - simplified checking in process()

void NAVProcessor::printStatus() {
    LOG_INFO(EventSource::GNSS, "=== NAVProcessor Status ===");
    
    NavMessageType currentType = getCurrentMessageType();
    LOG_INFO(EventSource::GNSS, "Current mode: %s", 
        currentType == NavMessageType::PANDA ? "PANDA (Single GPS)" :
        currentType == NavMessageType::PAOGI ? "PAOGI (Dual GPS)" : "NONE");
    
    LOG_INFO(EventSource::GNSS, "Message rate: %d Hz", 1000 / MESSAGE_INTERVAL_MS);
    
    if (lastMessageTime > 0) {
        LOG_INFO(EventSource::GNSS, "Time since last message: %lu ms", 
            millis() - lastMessageTime);
    }
    
    // Show data sources
    LOG_INFO(EventSource::GNSS, "Data sources:");
    if (gnssProcessor.isValid()) {
        const auto& gnssData = gnssProcessor.getData();
        LOG_INFO(EventSource::GNSS, "  GPS: Valid (Fix=%d, Sats=%d)", 
            gnssData.fixQuality, gnssData.numSatellites);
        if (gnssData.hasDualHeading) {
            LOG_INFO(EventSource::GNSS, "  Dual GPS: Active (Quality=%d)", 
                gnssData.headingQuality);
        }
    } else {
        LOG_INFO(EventSource::GNSS, "  GPS: No valid fix");
    }
    
    if (imuProcessor.hasValidData()) {
        LOG_INFO(EventSource::GNSS, "  IMU: %s connected", imuProcessor.getIMUTypeName());
    } else {
        LOG_INFO(EventSource::GNSS, "  IMU: Not detected");
    }
}