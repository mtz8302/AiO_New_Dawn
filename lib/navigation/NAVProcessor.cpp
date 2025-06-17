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
    if (!gnssPTR || !gnssPTR->isValid()) {
        return NavMessageType::NONE;
    }
    
    const auto& gnssData = gnssPTR->getData();
    
    if (gnssData.hasDualHeading) {
        return NavMessageType::PAOGI;  // Dual GPS detected
    } else {
        return NavMessageType::PANDA;  // Single GPS
    }
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
    
    // Get IMU data if available
    int16_t roll = 65535;  // Default "no IMU" value
    int16_t pitch = -1;
    float yawRate = 0.0;
    
    if (imuPTR && imuPTR->hasValidData()) {
        const auto& imuData = imuPTR->getCurrentData();
        roll = (int16_t)round(imuData.roll);
        pitch = (int16_t)round(imuData.pitch);
        yawRate = imuData.yawRate;
    }
    
    // Format heading from VTG (multiply by 10 for message format)
    int headingX10 = (int)(gnssData.headingTrue * 10.0);
    
    // Format time (HHMMSS.S)
    // fixTime is stored as HHMMSS integer, convert to float with decimal
    float timeFloat = gnssData.fixTime + (millis() % 1000) / 1000.0;
    
    // Build PANDA message without checksum
    int len = snprintf(messageBuffer, BUFFER_SIZE - 4,
        "$PANDA,%.1f,%.6f,%c,%.6f,%c,%d,%d,%.1f,%.3f,%s,%.3f,%d,%d,%d,%.2f",
        timeFloat,                          // Time
        latNMEA, latDir,                   // Latitude
        lonNMEA, lonDir,                   // Longitude
        gnssData.fixQuality,               // Fix quality
        gnssData.numSatellites,            // Satellites
        gnssData.hdop,                     // HDOP
        gnssData.altitude,                 // Altitude
        gnssData.ageDGPS > 0 ? String(gnssData.ageDGPS).c_str() : "", // Age DGPS
        gnssData.speedKnots,               // Speed
        headingX10,                        // Heading * 10
        roll,                              // Roll
        pitch,                             // Pitch
        yawRate                            // Yaw rate
    );
    
    // Calculate and append checksum
    uint8_t checksum = calculateNMEAChecksum(messageBuffer);
    snprintf(messageBuffer + len, 5, "*%02X", checksum);
    
    return true;
}

bool NAVProcessor::formatPAOGIMessage() {
    if (!gnssPTR || !gnssPTR->isValid() || !gnssPTR->getData().hasDualHeading) {
        return false;
    }
    
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
    
    // Format time
    float timeFloat = gnssData.fixTime + (millis() % 1000) / 1000.0;
    
    // Build PAOGI message without checksum
    int len = snprintf(messageBuffer, BUFFER_SIZE - 4,
        "$PAOGI,%.1f,%.6f,%c,%.6f,%c,%d,%d,%.1f,%.3f,%.1f,%.3f,%.1f,%d,%d,%.2f",
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