// MotorDriverDetector.h - Handles motor driver detection and configuration
#ifndef MOTOR_DRIVER_DETECTOR_H
#define MOTOR_DRIVER_DETECTOR_H

#include <Arduino.h>
#include "MotorDriverInterface.h"
#include "EventLogger.h"
#include "ConfigManager.h"

// Motor driver configuration values from PGN251 Byte 8
enum class MotorDriverConfig : uint8_t {
    DRV8701_WHEEL_ENCODER = 0x00,
    DANFOSS_WHEEL_ENCODER = 0x01,
    DRV8701_PRESSURE_SENSOR = 0x02,
    DANFOSS_PRESSURE_SENSOR = 0x03,
    DRV8701_CURRENT_SENSOR = 0x04
};

class MotorDriverDetector {
private:
    static MotorDriverDetector* instance;
    
    // Detection state
    MotorDriverType detectedType = MotorDriverType::NONE;
    KickoutType kickoutType = KickoutType::NONE;
    bool detectionComplete = false;
    uint32_t detectionStartTime = 0;
    
    // Configuration from EEPROM
    uint8_t motorConfigByte = 0x00;  // From PGN251 Byte 8
    
    // Private constructor for singleton
    MotorDriverDetector() {}
    
public:
    static MotorDriverDetector* getInstance() {
        if (instance == nullptr) {
            instance = new MotorDriverDetector();
        }
        return instance;
    }
    
    // Initialize detection process
    void init() {
        LOG_INFO(EventSource::AUTOSTEER, "Initializing motor driver detection");
        detectionStartTime = millis();
        
        // Read motor configuration from EEPROM
        readMotorConfig();
    }
    
    // Perform detection - returns true when complete
    bool detect(bool keyaHeartbeatDetected) {
        if (detectionComplete) {
            return true;
        }
        
        // Priority 1: Check for Keya CAN heartbeat
        if (keyaHeartbeatDetected) {
            detectedType = MotorDriverType::KEYA_CAN;
            kickoutType = KickoutType::NONE;  // Keya uses motor slip detection
            LOG_INFO(EventSource::AUTOSTEER, "Detected Keya CAN motor via heartbeat");
            detectionComplete = true;
            return true;
        }
        
        // Wait up to 2 seconds for Keya heartbeat
        if (millis() - detectionStartTime < 2000) {
            return false;  // Still waiting
        }
        
        // Priority 2: Check EEPROM configuration
        switch (static_cast<MotorDriverConfig>(motorConfigByte)) {
            case MotorDriverConfig::DANFOSS_WHEEL_ENCODER:
                detectedType = MotorDriverType::DANFOSS;
                kickoutType = KickoutType::WHEEL_ENCODER;
                LOG_INFO(EventSource::AUTOSTEER, "Detected Danfoss valve with wheel encoder (config 0x%02X)", motorConfigByte);
                break;
                
            case MotorDriverConfig::DANFOSS_PRESSURE_SENSOR:
                detectedType = MotorDriverType::DANFOSS;
                kickoutType = KickoutType::PRESSURE_SENSOR;
                LOG_INFO(EventSource::AUTOSTEER, "Detected Danfoss valve with pressure sensor (config 0x%02X)", motorConfigByte);
                break;
                
            case MotorDriverConfig::DRV8701_WHEEL_ENCODER:
                detectedType = MotorDriverType::DRV8701;
                kickoutType = KickoutType::WHEEL_ENCODER;
                LOG_INFO(EventSource::AUTOSTEER, "Detected DRV8701 with wheel encoder (config 0x%02X)", motorConfigByte);
                break;
                
            case MotorDriverConfig::DRV8701_PRESSURE_SENSOR:
                detectedType = MotorDriverType::DRV8701;
                kickoutType = KickoutType::PRESSURE_SENSOR;
                LOG_INFO(EventSource::AUTOSTEER, "Detected DRV8701 with pressure sensor (config 0x%02X)", motorConfigByte);
                break;
                
            case MotorDriverConfig::DRV8701_CURRENT_SENSOR:
                detectedType = MotorDriverType::DRV8701;
                kickoutType = KickoutType::CURRENT_SENSOR;
                LOG_INFO(EventSource::AUTOSTEER, "Detected DRV8701 with current sensor (config 0x%02X)", motorConfigByte);
                break;
                
            default:
                // Priority 3: Default to DRV8701 with wheel encoder
                detectedType = MotorDriverType::DRV8701;
                kickoutType = KickoutType::WHEEL_ENCODER;
                LOG_WARNING(EventSource::AUTOSTEER, "Unknown motor config 0x%02X, defaulting to DRV8701 with wheel encoder", motorConfigByte);
                break;
        }
        
        detectionComplete = true;
        return true;
    }
    
    // Get detection results
    MotorDriverType getDetectedType() const { return detectedType; }
    KickoutType getKickoutType() const { return kickoutType; }
    bool isDetectionComplete() const { return detectionComplete; }
    
    // Update motor configuration from PGN251
    void updateMotorConfig(uint8_t configByte) {
        if (motorConfigByte != configByte) {
            motorConfigByte = configByte;
            LOG_INFO(EventSource::AUTOSTEER, "Motor config updated to 0x%02X - restart required for changes to take effect", configByte);
            
            // Save to EEPROM
            extern ConfigManager configManager;
            configManager.setMotorDriverConfig(configByte);
            configManager.saveSteerConfig();
        }
    }
    
private:
    void readMotorConfig() {
        // Read from ConfigManager (EEPROM)
        extern ConfigManager configManager;
        motorConfigByte = configManager.getMotorDriverConfig();
        
        const char* configDesc = "Unknown";
        switch (motorConfigByte) {
            case 0x00: configDesc = "DRV8701 + Wheel Encoder"; break;
            case 0x01: configDesc = "Danfoss + Wheel Encoder"; break;
            case 0x02: configDesc = "DRV8701 + Pressure Sensor"; break;
            case 0x03: configDesc = "Danfoss + Pressure Sensor"; break;
            case 0x04: configDesc = "DRV8701 + Current Sensor"; break;
        }
        
        LOG_INFO(EventSource::AUTOSTEER, "Motor config from EEPROM: 0x%02X (%s)", motorConfigByte, configDesc);
    }
};

// Declare static instance (definition goes in .cpp file)

#endif // MOTOR_DRIVER_DETECTOR_H