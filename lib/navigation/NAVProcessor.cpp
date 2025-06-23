#include "NAVProcessor.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// External pointers from main.cpp
extern GNSSProcessor* gnssPTR;
extern IMUProcessor* imuPTR;

// External UDP send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

// Static instance
NAVProcessor* NAVProcessor::instance = nullptr;

NAVProcessor::NAVProcessor() {
    instance = this;
    
    // Initialize statistics
    stats = {0, 0, 0, 0};
    
    // Clear message buffer
    memset(messageBuffer, 0, BUFFER_SIZE);
    
    // Initialize startup check
    startupCheckComplete = false;
    
    Serial.print("\r\n- NAVProcessor initialized");
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
    if (!gnssPTR) {
        return NavMessageType::NONE;
    }
    
    const auto& gnssData = gnssPTR->getData();
    
    // For dual/INS systems, send PAOGI even without fix (for INS_ALIGNING state)
    if (gnssData.hasDualHeading || gnssData.hasINS) {
        return NavMessageType::PAOGI;  // Dual GPS/INS detected
    }
    
    // For single GPS, require fix
    if (!gnssPTR->hasFix()) {
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
    if (!gnssPTR || !gnssPTR->isValid()) {
        return false;
    }
    
    const auto& gnssData = gnssPTR->getData();
    
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
    
    if (imuPTR && imuPTR->hasValidData()) {
        const auto& imuData = imuPTR->getCurrentData();
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
    if (!gnssPTR || !gnssPTR->getData().hasDualHeading) {
        return false;
    }
    // Allow PAOGI even without valid position for INS_ALIGNING state
    
    const auto& gnssData = gnssPTR->getData();
    
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
    else if (imuPTR && imuPTR->hasValidData()) {
        const auto& imuData = imuPTR->getCurrentData();
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
    
    // Also send to Serial for debugging (without extra CR+LF)
    // Serial.printf("\r\n>> %s", message);  // Commented out to reduce spam
}

void NAVProcessor::process() {
    // Simple startup detection and status reporting
    static bool startupCheckComplete = false;
    static uint32_t lastStatusPrint = 0;
    
    if (!startupCheckComplete && millis() > 3000) {  // Wait 3 seconds for systems to initialize
        // Check GPS status
        if (!gnssPTR || !gnssPTR->hasGPS()) {
            Serial.print("\r\n[NAV] No GPS");
        } else if (!gnssPTR->hasFix()) {
            Serial.print("\r\n[NAV] GPS detected, waiting for fix...");
        } else {
            const auto& gnssData = gnssPTR->getData();
            if (gnssData.hasDualHeading) {
                Serial.print("\r\n[NAV] GPS Mode: Dual antenna with fix");
            } else {
                Serial.print("\r\n[NAV] GPS Mode: Single antenna with fix");
            }
        }
        
        // Check IMU status
        if (!imuPTR || !imuPTR->hasValidData()) {
            Serial.print("\r\n[NAV] No IMU");
        } else {
            Serial.printf("\r\n[NAV] IMU: %s detected", imuPTR->getIMUTypeName());
        }
        
        startupCheckComplete = true;
    }
    
    // Don't process if no GPS connection
    if (!gnssPTR || !gnssPTR->hasGPS()) {
        // Print status occasionally
        if (millis() - lastStatusPrint > 5000) {
            Serial.print("\r\n[NAV] No GPS detected");
            lastStatusPrint = millis();
        }
        return;
    }
    
    // Special handling for dual antenna/INS systems that need to send PAOGI even without fix
    const auto& gnssData = gnssPTR->getData();
    bool isDualSystem = gnssData.hasDualHeading || gnssData.hasINS;
    
    // Don't send messages if no fix (except for dual/INS systems)
    if (!gnssPTR->hasFix() && !isDualSystem) {
        // Print status occasionally
        if (millis() - lastStatusPrint > 5000) {
            Serial.print("\r\n[NAV] GPS detected, waiting for fix...");
            lastStatusPrint = millis();
        }
        return;
    }
    
    // For dual/INS systems without fix, print special status
    if (!gnssPTR->hasFix() && isDualSystem) {
        if (millis() - lastStatusPrint > 5000) {
            Serial.print("\r\n[NAV] UM981 INS aligning - needs movement");
            lastStatusPrint = millis();
        }
        // Continue to send PAOGI messages with timestamp updates
    }
    
    // Check if it's time to send a message
    if (timeSinceLastMessage < MESSAGE_INTERVAL_MS) {
        return;
    }
    
    timeSinceLastMessage = 0;
    
    // Debug output
    static uint32_t debugCounter = 0;
    debugCounter++;
    // if (debugCounter % 10 == 0) {  // Print every 10th message (1Hz)
    //     Serial.printf("\r\n[NAV] Processing message #%lu", debugCounter);
    // }
    
    // Select and format appropriate message type
    NavMessageType msgType = selectMessageType();
    bool success = false;
    
    // if (debugCounter % 10 == 1) {  // Print message type
    //     Serial.printf(" - Type: %s", 
    //         msgType == NavMessageType::PANDA ? "PANDA" :
    //         msgType == NavMessageType::PAOGI ? "PAOGI" : "NONE");
    // }
    
    switch (msgType) {
        case NavMessageType::PANDA:
            success = formatPANDAMessage();
            if (success) {
                sendMessage(messageBuffer);
                stats.pandaMessagesSent++;
            }
            break;
            
        case NavMessageType::PAOGI:
            success = formatPAOGIMessage();
            if (success) {
                sendMessage(messageBuffer);
                stats.paogiMessagesSent++;
            }
            break;
            
        case NavMessageType::NONE:
        default:
            // No valid data to send
            break;
    }
    
    if (!success && msgType != NavMessageType::NONE) {
        stats.messageErrors++;
    }
    
    stats.lastMessageTime = millis();
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
    Serial.print("\r\n\n=== NAVProcessor Status ===");
    
    NavMessageType currentType = getCurrentMessageType();
    Serial.printf("\r\nCurrent mode: %s", 
        currentType == NavMessageType::PANDA ? "PANDA (Single GPS)" :
        currentType == NavMessageType::PAOGI ? "PAOGI (Dual GPS)" : "NONE");
    
    Serial.printf("\r\nMessage rate: %d Hz", 1000 / MESSAGE_INTERVAL_MS);
    
    Serial.print("\r\n\nStatistics:");
    Serial.printf("\r\n  PANDA messages sent: %lu", stats.pandaMessagesSent);
    Serial.printf("\r\n  PAOGI messages sent: %lu", stats.paogiMessagesSent);
    Serial.printf("\r\n  Message errors: %lu", stats.messageErrors);
    
    if (stats.lastMessageTime > 0) {
        Serial.printf("\r\n  Time since last message: %lu ms", 
            millis() - stats.lastMessageTime);
    }
    
    // Show data sources
    Serial.print("\r\n\nData sources:");
    if (gnssPTR && gnssPTR->isValid()) {
        const auto& gnssData = gnssPTR->getData();
        Serial.printf("\r\n  GPS: Valid (Fix=%d, Sats=%d)", 
            gnssData.fixQuality, gnssData.numSatellites);
        if (gnssData.hasDualHeading) {
            Serial.printf("\r\n  Dual GPS: Active (Quality=%d)", 
                gnssData.headingQuality);
        }
    } else {
        Serial.print("\r\n  GPS: No valid fix");
    }
    
    if (imuPTR && imuPTR->hasValidData()) {
        Serial.printf("\r\n  IMU: %s connected", imuPTR->getIMUTypeName());
    } else {
        Serial.print("\r\n  IMU: Not detected");
    }
    
    Serial.print("\r\n========================\r\n");
}