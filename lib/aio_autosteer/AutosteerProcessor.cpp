#include "AutosteerProcessor.h"
#include "PGNProcessor.h"
#include "ADProcessor.h"
#include "MotorDriverInterface.h"
#include "KeyaCANDriver.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "EventLogger.h"
#include "QNetworkBase.h"
#include <cmath>  // For sin() function

// External network function
extern void sendUDPbytes(uint8_t* data, int len);

// External objects and pointers
extern ConfigManager configManager;
extern LEDManager ledManager;
extern ADProcessor adProcessor;
extern MotorDriverInterface* motorPTR;

// External network config
extern struct NetworkConfig netConfig;

// Global pointer definition
AutosteerProcessor* autosteerPTR = nullptr;

// Singleton instance
AutosteerProcessor* AutosteerProcessor::instance = nullptr;

AutosteerProcessor::AutosteerProcessor() {
    // Initialize config with defaults
    memset(&steerConfig, 0, sizeof(SteerConfig));
    memset(&steerSettings, 0, sizeof(SteerSettings));
    
    // Set some sensible defaults
    steerSettings.Kp = 1.0f;
    steerSettings.highPWM = 255;
    steerSettings.lowPWM = 10;
    steerSettings.minPWM = 5;
}

AutosteerProcessor* AutosteerProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new AutosteerProcessor();
    }
    return instance;
}

bool AutosteerProcessor::init() {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing AutosteerProcessor");
    
    // Make sure instance is set
    instance = this;
    autosteerPTR = this;  // Also set global pointer
    
    // Initialize button pin
    pinMode(2, INPUT_PULLUP);
    LOG_DEBUG(EventSource::AUTOSTEER, "Button pin 2 configured as INPUT_PULLUP");
    
    // Initialize PID controller with default values
    pid.setKp(5.0f);  // Default proportional gain
    pid.setOutputLimit(100.0f);  // Motor speed limit (±100%)
    LOG_DEBUG(EventSource::AUTOSTEER, "PID controller initialized");
    
    // Register PGN handlers with PGNProcessor
    if (PGNProcessor::instance) {
        LOG_DEBUG(EventSource::AUTOSTEER, "Registering PGN callbacks...");
        
        // Register for broadcast messages (PGN 200, 202)
        bool regBroadcast = PGNProcessor::instance->registerBroadcastCallback(handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 251 (Steer Config)
        bool reg251 = PGNProcessor::instance->registerCallback(251, handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 252 (Steer Settings)
        bool reg252 = PGNProcessor::instance->registerCallback(252, handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 254 (Steer Data with button)
        bool reg254 = PGNProcessor::instance->registerCallback(254, handlePGNStatic, "AutosteerHandler");
            
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN registrations: Broadcast=%d, 251=%d, 252=%d, 254=%d", regBroadcast, reg251, reg252, reg254);
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "PGNProcessor not initialized!");
        return false;
    }
    
    LOG_INFO(EventSource::AUTOSTEER, "AutosteerProcessor initialized successfully");
    return true;
}

void AutosteerProcessor::process() {
    // Run autosteer loop at 100Hz
    uint32_t currentTime = millis();
    if (currentTime - lastLoopTime < LOOP_TIME) {
        return;  // Not time yet
    }
    lastLoopTime = currentTime;
    
    // === 100Hz AUTOSTEER LOOP STARTS HERE ===
    
    // Read button state with debouncing
    static uint32_t lastButtonPrint = 0;
    static bool lastButtonReading = HIGH;
    bool buttonReading = digitalRead(2);  // HIGH when not pressed, LOW when pressed
    
    // === BUTTON LOGIC ===
    // 1. Check if guidance status changed from AgOpenGPS
    if (guidanceStatusChanged) {
        if (guidanceActive) {
            // Guidance turned ON in AgOpenGPS
            steerState = 0;  // Activate steering
            LOG_INFO(EventSource::AUTOSTEER, "Autosteer ARMED via AgOpenGPS (OSB)");
        }
        guidanceStatusChanged = false;  // Clear flag
    }
    
    // 2. If AgOpenGPS has stopped steering, turn off after delay
    static int switchCounter = 0;
    if (steerState == 0 && !guidanceActive) {
        if (switchCounter++ > 30) {  // 30 * 10ms = 300ms delay
            steerState = 1;
            switchCounter = 0;
            LOG_INFO(EventSource::AUTOSTEER, "Autosteer DISARMED - guidance inactive");
        }
    } else {
        switchCounter = 0;
    }
    
    // 3. Physical button press toggles state
    if (buttonReading == LOW && lastButtonReading == HIGH) {
        // Button was just pressed
        steerState = !steerState;
        LOG_INFO(EventSource::AUTOSTEER, "Autosteer %s via physical button", 
                 steerState == 0 ? "ARMED" : "DISARMED");
    }
    lastButtonReading = buttonReading;
    
    
    // Check Keya motor slip if steering is active
    if (motorPTR && motorPTR->getType() == MotorDriverType::KEYA_CAN && 
        steerState == 0 && guidanceActive) {
        KeyaCANDriver* keya = static_cast<KeyaCANDriver*>(motorPTR);
        if (keya->checkMotorSlip()) {
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: Keya motor slip detected");
            emergencyStop();
            return;  // Skip the rest of this cycle
        }
    }
    
    // Update motor control
    updateMotorControl();
    
    // Send PGN 253 status to AgOpenGPS
    sendPGN253();
    
    // Update LED status
    bool wasReady = true;  // ADProcessor is always available as an object
    bool enabled = (steerState == 0);    // Button/OSB active
    bool active = (shouldSteerBeActive() && motorSpeed != 0.0f);  // Actually steering
    ledManager.setSteerState(wasReady, enabled, active);
}

void AutosteerProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len) {
    // Check if this is a Hello PGN
    if (pgn == 200) {
        // PGN 200 - Hello from AgIO, send reply
        sendHelloReply();
    }
    // Check if this is a Scan Request PGN
    else if (pgn == 202) {
        // PGN 202 - Scan request from AgIO
        sendScanReply();
    }
}

void AutosteerProcessor::sendHelloReply() {
    // Hello from AutoSteer - PGN 126 (0x7E)
    // Format: {header, source, pgn, length, angleLo, angleHi, countsLo, countsHi, switches, checksum}
    
    uint8_t helloFromSteer[] = {
        128, 129,       // Header (0x80, 0x81)
        126,            // Source: Steer module (0x7E)
        126,            // PGN: Steer reply (0x7E)
        5,              // Length
        0, 0,           // Angle (0 for now)
        0, 0,           // Counts (0 for now)
        0,              // Switch byte (0 for now)
        71              // Checksum (hardcoded like NG-V6)
    };
    
    // Send via UDP
    sendUDPbytes(helloFromSteer, sizeof(helloFromSteer));
}

void AutosteerProcessor::sendScanReply() {
    // Scan reply from AutoSteer - PGN 203 (0xCB)
    // Format: {header, source, pgn, length, ip1, ip2, ip3, subnet1, subnet2, subnet3, checksum}
    
    uint8_t scanReply[] = {
        0x80, 0x81,                    // Header
        0x7E,                          // Source: Steer module
        0xCB,                          // PGN: 203 Scan reply
        0x07,                          // Length (data only)
        netConfig.currentIP[0],        // IP octet 1
        netConfig.currentIP[1],        // IP octet 2
        netConfig.currentIP[2],        // IP octet 3
        netConfig.currentIP[3],        // IP octet 4
        netConfig.currentIP[0],        // Subnet octet 1 (repeat IP)
        netConfig.currentIP[1],        // Subnet octet 2 (repeat IP)
        netConfig.currentIP[2],        // Subnet octet 3 (repeat IP)
        0                              // CRC placeholder
    };
    
    // Calculate CRC
    uint8_t crc = 0;
    for (int i = 2; i < sizeof(scanReply) - 1; i++) {
        crc += scanReply[i];
    }
    scanReply[sizeof(scanReply) - 1] = crc;
    
    // Send via UDP
    sendUDPbytes(scanReply, sizeof(scanReply));
}

void AutosteerProcessor::handleSteerConfig(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 251 - Steer Config
    // Expected length is 14 bytes total, but we get data after header
    
    LOG_DEBUG(EventSource::AUTOSTEER, "PGN 251 (Steer Config) received, %d bytes", len);
    
    // Debug: dump entire PGN 251 packet
    char debugMsg[256];
    snprintf(debugMsg, sizeof(debugMsg), "Raw PGN 251 data:");
    for (int i = 0; i < len && strlen(debugMsg) < 200; i++) {
        char buf[20];
        snprintf(buf, sizeof(buf), " [%d]=0x%02X(%d)", i, data[i], data[i]);
        strncat(debugMsg, buf, sizeof(debugMsg) - strlen(debugMsg) - 1);
    }
    LOG_DEBUG(EventSource::AUTOSTEER, "%s", debugMsg);
    
    if (len < 4) {  // Changed from 5 to 4 since we only need up to index 3
        LOG_ERROR(EventSource::AUTOSTEER, "PGN 251 too short!");
        return;
    }
    
    // Data array indices (after header/length removal):
    // Per PGN.md: set0, pulseCount, minSpeed, sett1, ***, ***, ***, ***
    // [0] = setting0 byte
    // [1] = pulseCountMax
    // [2] = minSpeed
    // [3] = setting1 byte
    // [4-7] = reserved/unused
    
    uint8_t sett0 = data[0];
    steerConfig.InvertWAS = bitRead(sett0, 0);
    steerConfig.IsRelayActiveHigh = bitRead(sett0, 1);
    steerConfig.MotorDriveDirection = bitRead(sett0, 2);
    steerConfig.SingleInputWAS = bitRead(sett0, 3);
    steerConfig.CytronDriver = bitRead(sett0, 4);
    steerConfig.SteerSwitch = bitRead(sett0, 5);
    steerConfig.SteerButton = bitRead(sett0, 6);
    steerConfig.ShaftEncoder = bitRead(sett0, 7);
    
    steerConfig.PulseCountMax = data[1];  // Fixed: was data[2]
    steerConfig.MinSpeed = data[2];       // Fixed: was data[3]
    
    uint8_t sett1 = data[3];              // Fixed: was data[4]
    steerConfig.IsDanfoss = bitRead(sett1, 0);
    steerConfig.PressureSensor = bitRead(sett1, 1);
    steerConfig.CurrentSensor = bitRead(sett1, 2);
    steerConfig.IsUseY_Axis = bitRead(sett1, 3);
    
    LOG_DEBUG(EventSource::AUTOSTEER, "InvertWAS: %d", steerConfig.InvertWAS);
    LOG_DEBUG(EventSource::AUTOSTEER, "MotorDriveDirection: %d", steerConfig.MotorDriveDirection);
    LOG_DEBUG(EventSource::AUTOSTEER, "CytronDriver: %d", steerConfig.CytronDriver);
    LOG_DEBUG(EventSource::AUTOSTEER, "SteerSwitch: %d", steerConfig.SteerSwitch);
    LOG_DEBUG(EventSource::AUTOSTEER, "SteerButton: %d", steerConfig.SteerButton);
    LOG_DEBUG(EventSource::AUTOSTEER, "PulseCountMax: %d", steerConfig.PulseCountMax);
    LOG_DEBUG(EventSource::AUTOSTEER, "MinSpeed: %d", steerConfig.MinSpeed);
    
    // Log all settings at INFO level in a single message so users see everything
    LOG_INFO(EventSource::AUTOSTEER, "Steer config: WAS=%s Motor=%s MinSpeed=%d Steer=%s Encoder=%s Cytron=%s", 
             steerConfig.InvertWAS ? "Inv" : "Norm",
             steerConfig.MotorDriveDirection ? "Rev" : "Norm",
             steerConfig.MinSpeed,
             steerConfig.SteerSwitch ? "On" : "Off",
             steerConfig.ShaftEncoder ? "Yes" : "No",
             steerConfig.CytronDriver ? "Yes" : "No");
}

void AutosteerProcessor::handleSteerSettings(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 252 - Steer Settings
    // Expected length is 14 bytes total, but we get data after header
    
    LOG_DEBUG(EventSource::AUTOSTEER, "PGN 252 (Steer Settings) received, %d bytes", len);
    
    if (len < 8) {
        LOG_ERROR(EventSource::AUTOSTEER, "PGN 251 too short!");
        return;
    }
    
    // Data array indices (after header/length removal):
    // [0] = Kp (proportional gain)
    // [1] = highPWM (max limit)
    // [2] = lowPWM (minimum to move)
    // [3] = minPWM
    // [4] = steerSensorCounts
    // [5-6] = wasOffset (int16)
    // [7] = ackermanFix
    
    steerSettings.Kp = data[0];  // Note: V6 doesn't divide by 10 here
    steerSettings.highPWM = data[1];
    steerSettings.lowPWM = data[2];
    steerSettings.minPWM = data[3];
    
    // V6-NG adjusts lowPWM
    float temp = (float)steerSettings.minPWM * 1.2;
    steerSettings.lowPWM = (uint8_t)temp;
    
    steerSettings.steerSensorCounts = data[4];
    
    // WAS offset is int16
    steerSettings.wasOffset = data[5] | (data[6] << 8);
    
    steerSettings.AckermanFix = (float)data[7] * 0.01f;
    
    LOG_DEBUG(EventSource::AUTOSTEER, "Kp: %d", steerSettings.Kp);
    LOG_DEBUG(EventSource::AUTOSTEER, "highPWM: %d", steerSettings.highPWM);
    LOG_DEBUG(EventSource::AUTOSTEER, "lowPWM: %d", steerSettings.lowPWM);
    LOG_DEBUG(EventSource::AUTOSTEER, "minPWM: %d", steerSettings.minPWM);
    LOG_DEBUG(EventSource::AUTOSTEER, "steerSensorCounts: %d", steerSettings.steerSensorCounts);
    LOG_DEBUG(EventSource::AUTOSTEER, "wasOffset: %d", steerSettings.wasOffset);
    LOG_DEBUG(EventSource::AUTOSTEER, "AckermanFix: %.2f", steerSettings.AckermanFix);
    
    // Update PID controller with new Kp
    float scaledKp = steerSettings.Kp / 10.0f;  // AgOpenGPS sends Kp * 10
    pid.setKp(scaledKp);
    LOG_DEBUG(EventSource::AUTOSTEER, "PID updated with Kp=%.1f", scaledKp);
    
    // Log all settings at INFO level in a single message so users see everything
    LOG_INFO(EventSource::AUTOSTEER, "Steer settings: Kp=%.1f PWM=%d-%d-%d WAS_offset=%d counts=%d Ackerman=%.2f", 
             scaledKp,
             steerSettings.minPWM, steerSettings.lowPWM, steerSettings.highPWM,
             steerSettings.wasOffset, steerSettings.steerSensorCounts,
             steerSettings.AckermanFix);
}

void AutosteerProcessor::handleSteerData(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 254 - Steer Data (comes at 10Hz from AgOpenGPS)
    // For now, we only care about the autosteer enable bit
    
    if (len < 3) {
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN 254 too short, ignoring");
        return;  // Too short, ignore
    }
    
    lastPGN254Time = millis();
    lastCommandTime = millis();  // Update watchdog timer
    
    // Debug: Log raw PGN 254 data
    static uint32_t lastRawLog = 0;
    if (millis() - lastRawLog > 500) { // Every 500ms
        lastRawLog = millis();
        char hexBuf[64];
        int pos = 0;
        for (int i = 0; i < len && i < 16 && pos < 60; i++) {
            pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, "%02X ", data[i]);
        }
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN254 raw (%d bytes): %s", len, hexBuf);
    }
    
    
    // Data format (from PGNProcessor we get data starting at speed):
    // [0-1] = Speed (uint16, cm/s)
    // [2] = Status byte
    //       Bit 0: Guidance active
    //       Bit 6: Autosteer enable (this is what we need for button)
    // [3-4] = Steer angle setpoint (int16, divide by 100 for degrees)
    // [5] = XTE (cross track error)
    // [6] = Machine sections 1-8
    // [7] = Machine sections 9-16
    
    // Extract speed
    uint16_t speedCmS = (uint16_t)(data[1] << 8 | data[0]);
    vehicleSpeed = speedCmS * 0.036f; // Convert cm/s to km/h
    
    // Extract status
    uint8_t status = data[2];
    bool newAutosteerState = (status & 0x40) != 0;  // Bit 6 is autosteer enable
    
    
    // Track guidance status changes
    prevGuidanceStatus = guidanceActive;
    guidanceActive = (status & 0x01) != 0;           // Bit 0 is guidance active
    guidanceStatusChanged = (guidanceActive != prevGuidanceStatus);
    // Extract steer angle
    int16_t angleRaw = (int16_t)(data[4] << 8 | data[3]);
    targetAngle = angleRaw / 100.0f;
    
    // Debug log for AgIO test mode
    if (targetAngle != 0.0f || autosteerEnabled) {
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN254: speed=%.1f km/h, target=%.1f°, enabled=%d, guidance=%d", 
                  vehicleSpeed, targetAngle, autosteerEnabled, guidanceActive);
    }
    
    // Extract XTE
    crossTrackError = (int8_t)data[5];
    
    // Extract sections
    uint8_t sections1_8 = data[6];
    uint8_t sections9_16 = data[7];
    machineSections = (uint16_t)(sections9_16 << 8 | sections1_8);
    
    // Only print on state change to avoid spam
    if (newAutosteerState != autosteerEnabled) {
        LOG_INFO(EventSource::AUTOSTEER, "AgOpenGPS autosteer request: %s", 
                      newAutosteerState ? "ENGAGE" : "DISENGAGE");
        autosteerEnabled = newAutosteerState;
    }
}

// Static callback wrapper
void AutosteerProcessor::handlePGNStatic(uint8_t pgn, const uint8_t* data, size_t len) {
    if (instance) {
        // Don't print for PGN 254, 200, or 202 since they come frequently
        if (pgn != 254 && pgn != 200 && pgn != 202) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Received PGN %d", pgn);
        }
        
        // Handle broadcast PGNs
        if (pgn == 200 || pgn == 202) {
            instance->handleBroadcastPGN(pgn, data, len);
        }
        else if (pgn == 251) {
            instance->handleSteerConfig(pgn, data, len);
        }
        else if (pgn == 252) {
            instance->handleSteerSettings(pgn, data, len);
        }
        else if (pgn == 254) {
            instance->handleSteerData(pgn, data, len);
        }
        // We'll add other PGNs one at a time as needed
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "No instance!");
    }
}

void AutosteerProcessor::sendPGN253() {
    // PGN 253 - Status data TO AgOpenGPS
    // Format: {header, source, pgn, length, 
    //          steerAngleLo, steerAngleHi,    // bytes 5-6: actual steer angle
    //          headingLo, headingHi,           // bytes 7-8: IMU heading (deprecated)
    //          rollLo, rollHi,                 // bytes 9-10: roll (deprecated)
    //          switchByte,                     // byte 11: switch states
    //          pwmDisplay,                     // byte 12: PWM value
    //          checksum}
    
    // Get actual values
    int16_t actualSteerAngle = (int16_t)(currentAngle * 100.0f);  // Current angle * 100
    int16_t heading = 0;            // Deprecated - sent by GNSS
    int16_t roll = 0;               // Deprecated - sent by GNSS  
    uint8_t pwmDisplay = (uint8_t)(abs(motorSpeed) * 2.55f);  // Convert % to 0-255
    
    // Build switch byte
    // Bit 0: work switch (inverted)
    // Bit 1: steer switch (inverted steerState)
    // Bit 2: remote/kickout input
    uint8_t switchByte = 0;
    switchByte |= (0 << 2);        // No remote/kickout for now
    switchByte |= (steerState << 1);  // Steer state in bit 1
    switchByte |= 1;               // No work switch, so set bit 0
    
    uint8_t pgn253[] = {
        0x80, 0x81,                    // Header
        0x7D,                          // Source: Steer module
        0xFD,                          // PGN: 253
        8,                             // Length
        (uint8_t)(actualSteerAngle & 0xFF),      // Steer angle low
        (uint8_t)(actualSteerAngle >> 8),        // Steer angle high
        (uint8_t)(heading & 0xFF),                // Heading low
        (uint8_t)(heading >> 8),                  // Heading high
        (uint8_t)(roll & 0xFF),                   // Roll low
        (uint8_t)(roll >> 8),                     // Roll high
        switchByte,                               // Switch byte
        pwmDisplay,                               // PWM display
        0                                         // CRC placeholder
    };
    
    // Calculate CRC
    uint8_t crc = 0;
    for (int i = 2; i < sizeof(pgn253) - 1; i++) {
        crc += pgn253[i];
    }
    pgn253[sizeof(pgn253) - 1] = crc;
    
    // Send via UDP
    sendUDPbytes(pgn253, sizeof(pgn253));
}

void AutosteerProcessor::updateMotorControl() {
    // Get current WAS angle
    currentAngle = adProcessor.getWASAngle();
    
    // Check if steering should be active
    bool shouldBeActive = shouldSteerBeActive();
    
    // Handle state transitions
    if (shouldBeActive && motorState == MotorState::DISABLED) {
        // Transition: Start soft-start sequence
        motorState = MotorState::SOFT_START;
        softStartBeginTime = millis();
        softStartRampValue = 0.0f;
        LOG_INFO(EventSource::AUTOSTEER, "Motor STARTING - soft-start sequence (%dms)", 
                 softStartDurationMs);
    } 
    else if (!shouldBeActive && motorState != MotorState::DISABLED) {
        // Transition: Disable motor
        motorState = MotorState::DISABLED;
        motorSpeed = 0.0f;
        if (motorPTR) {
            motorPTR->enable(false);
            motorPTR->setSpeed(0.0f);
        }
        // Give more specific disable reason
        if (vehicleSpeed <= 0.1f) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled - speed too low (%.1f km/h)", vehicleSpeed);
        } else if (!guidanceActive) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled - guidance inactive");
        } else if (steerState != 0) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled - steer switch off");
        } else {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled");
        }
        return;
    }
    else if (!shouldBeActive) {
        // Already disabled, nothing to do
        return;
    }
    
    // Calculate PID output
    float pidOutput = pid.compute(targetAngle, currentAngle);
    
    // Apply PWM scaling - use steerSettings directly, not configPTR
    if (steerSettings.highPWM > 0) {  // Check if we have valid settings
        float highPWM = steerSettings.highPWM;
        float lowPWM = steerSettings.lowPWM;
        float minPWM = steerSettings.minPWM;
        
        if (abs(pidOutput) < 0.1f) {
            // Dead zone
            motorSpeed = 0.0f;
        } else {
            // Scale PID output to PWM range
            float absPID = abs(pidOutput);
            float pwmRange = highPWM - lowPWM;
            float scaledPWM = lowPWM + (absPID / 100.0f) * pwmRange;
            
            // Ensure we don't exceed highPWM
            if (scaledPWM > highPWM) {
                scaledPWM = highPWM;
            }
            
            // Apply minimum PWM threshold
            if (scaledPWM < minPWM) {
                scaledPWM = 0.0f;
                LOG_DEBUG(EventSource::AUTOSTEER, "PWM ZEROED (<%d)", (int)minPWM);
            }
            
            // Convert to percentage for motor driver
            motorSpeed = (scaledPWM / 255.0f) * 100.0f;
            if (pidOutput < 0) motorSpeed = -motorSpeed;
            
            // Apply soft-start if active
            if (motorState == MotorState::SOFT_START) {
                uint32_t elapsed = millis() - softStartBeginTime;
                
                if (elapsed >= softStartDurationMs) {
                    // Soft-start complete, transition to normal
                    motorState = MotorState::NORMAL_CONTROL;
                    LOG_INFO(EventSource::AUTOSTEER, "Motor ACTIVE - normal steering control");
                } else {
                    // Calculate ramp progress (0.0 to 1.0)
                    float rampProgress = (float)elapsed / softStartDurationMs;
                    
                    // Use sine curve for smooth acceleration (slow-fast-slow)
                    float sineRamp = sin(rampProgress * PI / 2.0f);
                    
                    // Calculate soft-start limit based on lowPWM
                    float softStartLimit = (lowPWM / 255.0f) * 100.0f * softStartMaxPWM * sineRamp;
                    
                    // Apply limit in direction of motor speed
                    if (motorSpeed > 0) {
                        motorSpeed = min(motorSpeed, softStartLimit);
                    } else if (motorSpeed < 0) {
                        motorSpeed = max(motorSpeed, -softStartLimit);
                    }
                    
                    softStartRampValue = softStartLimit;
                    
                    // Debug logging every 50ms during soft-start
                    static uint32_t lastSoftStartDebug = 0;
                    if (millis() - lastSoftStartDebug > 50) {
                        lastSoftStartDebug = millis();
                        LOG_DEBUG(EventSource::AUTOSTEER, "Soft-start: elapsed=%dms, progress=%.2f, limit=%.1f%%, speed=%.1f%%", 
                                  elapsed, rampProgress, softStartLimit, motorSpeed);
                    }
                }
            }
        }
    } else {
        // No config, use raw PID output
        motorSpeed = pidOutput;
    }
    
    // Apply motor direction from config
    if (configManager.getMotorDriveDirection()) {
        motorSpeed = -motorSpeed;  // Invert if configured
    }
    
    // Send to motor
    if (motorPTR) {
        motorPTR->enable(true);
        motorPTR->setSpeed(motorSpeed);
    }
    
}

bool AutosteerProcessor::shouldSteerBeActive() const {
    // Check kickout cooldown
    if (kickoutTime > 0 && (millis() - kickoutTime < KICKOUT_COOLDOWN_MS)) {
        return false;  // Still in cooldown
    }
    
    // Check watchdog timeout
    if (millis() - lastCommandTime > WATCHDOG_TIMEOUT) {
        return false;  // No recent commands
    }
    
    // Check all enable conditions
    // Note: We use guidanceActive (bit 0) and steerState instead of autosteerEnabled (bit 6)
    // because AgOpenGPS may not set bit 6 until it receives confirmation from us
    bool active = guidanceActive &&           // Guidance line active (bit 0 from PGN 254)
                  (steerState == 0) &&        // Our button/OSB state (0=active)
                  (vehicleSpeed > 0.1f);      // Moving (TODO: use MinSpeed from config)
    
    // Debug logging for test mode
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 1000) {
        lastDebugTime = millis();
        LOG_DEBUG(EventSource::AUTOSTEER, "shouldSteerBeActive: guidance=%d, steerState=%d, speed=%.1f -> %s",
                  guidanceActive, steerState, vehicleSpeed, active ? "YES" : "NO");
    }
    
    return active;
}

void AutosteerProcessor::emergencyStop() {
    LOG_WARNING(EventSource::AUTOSTEER, "EMERGENCY STOP");
    
    // Reset motor state
    motorState = MotorState::DISABLED;
    
    // Disable motor immediately
    motorSpeed = 0.0f;
    if (motorPTR) {
        motorPTR->setSpeed(0.0f);
        motorPTR->enable(false);
    }
    
    // Set inactive state
    steerState = 1;
    
    // Start kickout cooldown
    kickoutTime = millis();
}