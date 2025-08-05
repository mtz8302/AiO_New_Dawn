#include "LittleDawnInterface.h"
#include "SerialManager.h"
#include "AutosteerProcessor.h"
#include "ADProcessor.h"
#include "IMUProcessor.h"

// Global instance
LittleDawnInterface littleDawnInterface;

// Initialize the interface
void LittleDawnInterface::init() {
    // SerialESP32 is already initialized by SerialManager at 460800 baud
    LOG_INFO(EventSource::SYSTEM, "Little Dawn interface initialized on Serial2 (460800 baud)");
}

// Main processing loop - called from main.cpp
void LittleDawnInterface::process() {
    // Send data every 100ms
    if (millis() - lastTransmitTime >= TRANSMIT_INTERVAL_MS) {
        sendMachineStatus();
        lastTransmitTime = millis();
    }
}

// Calculate simple checksum
uint8_t LittleDawnInterface::calculateChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum;  // One's complement
}

// Send message to Little Dawn
void LittleDawnInterface::sendToLittleDawn(uint8_t id, const uint8_t* data, uint8_t length) {
    uint8_t buffer[68];  // id + length + data + checksum
    buffer[0] = id;
    buffer[1] = length;
    memcpy(&buffer[2], data, length);
    buffer[2 + length] = calculateChecksum(buffer, 2 + length);
    
    SerialESP32.write(buffer, 3 + length);
}

// Send machine status with current data
void LittleDawnInterface::sendMachineStatus() {
    MachineStatus status;
    
    // Get speed from AutosteerProcessor (km/h)
    float speedKmh = 0.0f;
    if (AutosteerProcessor::getInstance()) {
        speedKmh = AutosteerProcessor::getInstance()->getVehicleSpeed();
    }
    
    // Get WAS angle from ADProcessor
    float wasAngle = 0.0f;
    wasAngle = adProcessor.getWASAngle();
    
    // Get IMU data if available
    float heading = 0.0f;
    float roll = 0.0f;
    float pitch = 0.0f;
    
    if (imuProcessor.hasValidData()) {
        IMUData imuData = imuProcessor.getCurrentData();
        heading = imuData.heading;
        roll = imuData.roll;
        pitch = imuData.pitch;
    }
    
    // Convert to protocol format
    status.speed = (int16_t)(speedKmh * 100);        // Convert to 0.01 km/h units
    status.heading = (int16_t)(heading * 10);         // Convert to 0.1 degree units
    status.roll = (int16_t)(roll * 10);               // Convert to 0.1 degree units
    status.pitch = (int16_t)(pitch * 10);             // Convert to 0.1 degree units
    status.steerAngle = (int16_t)(wasAngle * 10);    // Convert to 0.1 degree units
    
    // Send the data
    sendToLittleDawn(MSG_MACHINE_STATUS, (uint8_t*)&status, sizeof(status));
}

// Check if interface is active
bool LittleDawnInterface::isActive() const {
    // Consider active if we've sent data recently
    return (millis() - lastTransmitTime) < (TRANSMIT_INTERVAL_MS * 2);
}

// Print status information
void LittleDawnInterface::printStatus() {
    Serial.println("\n=== Little Dawn Interface Status ===");
    Serial.printf("Active: %s\n", isActive() ? "Yes" : "No");
    Serial.printf("Last transmit: %lu ms ago\n", millis() - lastTransmitTime);
    
    // Print current data being sent
    float speedKmh = AutosteerProcessor::getInstance() ? 
                     AutosteerProcessor::getInstance()->getVehicleSpeed() : 0.0f;
    float wasAngle = adProcessor.getWASAngle();
    
    Serial.printf("Current data:\n");
    Serial.printf("  Speed: %.2f km/h\n", speedKmh);
    Serial.printf("  WAS angle: %.1f deg\n", wasAngle);
    
    if (imuProcessor.hasValidData()) {
        IMUData imuData = imuProcessor.getCurrentData();
        Serial.printf("  Heading: %.1f deg\n", imuData.heading);
        Serial.printf("  Roll: %.1f deg\n", imuData.roll);
        Serial.printf("  Pitch: %.1f deg\n", imuData.pitch);
    } else {
        Serial.println("  IMU: No valid data");
    }
}