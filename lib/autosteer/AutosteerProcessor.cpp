#include "AutosteerProcessor.h"
#include "PGNProcessor.h"
#include "ADProcessor.h"
#include "MotorDriverInterface.h"
#include "KeyaCANDriver.h"
#include "ConfigManager.h"
#include "LEDManager.h"

// External network function
extern void sendUDPbytes(uint8_t* data, int len);

// External pointers
extern ConfigManager* configPTR;
extern LEDManager* ledPTR;

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
    Serial.print("\r\n- Initializing AutosteerProcessor");
    
    // Make sure instance is set
    instance = this;
    autosteerPTR = this;  // Also set global pointer
    
    // Initialize button pin
    pinMode(2, INPUT_PULLUP);
    Serial.print("\r\n  Button pin 2 configured as INPUT_PULLUP");
    
    // Initialize PID controller with default values
    pid.setKp(5.0f);  // Default proportional gain
    pid.setOutputLimit(100.0f);  // Motor speed limit (±100%)
    Serial.print("\r\n  PID controller initialized");
    
    // Register PGN handlers with PGNProcessor
    if (PGNProcessor::instance) {
        Serial.print("\r\n  Registering PGN callbacks...");
        
        // Register for a dummy PGN so we receive broadcast messages like PGN 200
        // Using PGN 255 as it's unused
        bool reg255 = PGNProcessor::instance->registerCallback(255, handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 251 (Steer Config)
        bool reg251 = PGNProcessor::instance->registerCallback(251, handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 252 (Steer Settings)
        bool reg252 = PGNProcessor::instance->registerCallback(252, handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 254 (Steer Data with button)
        bool reg254 = PGNProcessor::instance->registerCallback(254, handlePGNStatic, "AutosteerHandler");
            
        Serial.printf("\r\n  PGN registrations: 255=%d, 251=%d, 252=%d, 254=%d", reg255, reg251, reg252, reg254);
    } else {
        Serial.print("\r\n  ERROR: PGNProcessor not initialized!");
        return false;
    }
    
    Serial.print(" - SUCCESS");
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
            Serial.print("\r\n[Autosteer] Guidance activated from AgOpenGPS");
        }
        guidanceStatusChanged = false;  // Clear flag
    }
    
    // 2. If AgOpenGPS has stopped steering, turn off after delay
    static int switchCounter = 0;
    if (steerState == 0 && !guidanceActive) {
        if (switchCounter++ > 30) {  // 30 * 10ms = 300ms delay
            steerState = 1;
            switchCounter = 0;
            Serial.print("\r\n[Autosteer] Auto-deactivated (guidance off)");
        }
    } else {
        switchCounter = 0;
    }
    
    // 3. Physical button press toggles state
    if (buttonReading == LOW && lastButtonReading == HIGH) {
        // Button was just pressed
        steerState = !steerState;
        Serial.printf("\r\n[Autosteer] Physical button pressed - steerState now: %d", steerState);
    }
    lastButtonReading = buttonReading;
    
    // Print button state every 500ms (for debugging)
    if (currentTime - lastButtonPrint > 500) {
        Serial.printf("\r\n[Autosteer] Button: %s, steerState: %d, guidance: %d", 
                      buttonReading ? "HIGH" : "LOW", steerState, guidanceActive);
        lastButtonPrint = currentTime;
    }
    
    // Check Keya motor slip if steering is active
    if (motorPTR && motorPTR->getType() == MotorDriverType::KEYA_CAN && 
        steerState == 0 && guidanceActive) {
        KeyaCANDriver* keya = static_cast<KeyaCANDriver*>(motorPTR);
        if (keya->checkMotorSlip()) {
            Serial.print("\r\n[Autosteer] KICKOUT: Keya motor slip detected");
            emergencyStop();
            return;  // Skip the rest of this cycle
        }
    }
    
    // Update motor control
    updateMotorControl();
    
    // Send PGN 253 status to AgOpenGPS
    sendPGN253();
    
    // Update LED status
    if (ledPTR) {
        bool wasReady = (adPTR != nullptr);  // Have WAS sensor
        bool enabled = (steerState == 0);    // Button/OSB active
        bool active = (shouldSteerBeActive() && motorSpeed != 0.0f);  // Actually steering
        ledPTR->setSteerState(wasReady, enabled, active);
    }
}

void AutosteerProcessor::handleHelloPGN(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 200 - Hello from AgIO, send reply
    // Serial.print("\r\n[Autosteer] Hello from AgIO received - sending reply");
    sendHelloReply();
}

void AutosteerProcessor::sendHelloReply() {
    // Hello from AutoSteer - PGN 126 (0x7E)
    // Format: {header, source, pgn, length, angleLo, angleHi, countsLo, countsHi, switches, checksum}
    
    // Serial.print("\r\n[Autosteer] Sending hello reply...");
    
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
    // Serial.print(" - SENT");
}

void AutosteerProcessor::handleSteerConfig(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 251 - Steer Config
    // Expected length is 14 bytes total, but we get data after header
    
    Serial.printf("\r\n[Autosteer] PGN 251 (Steer Config) received, %d bytes", len);
    
    if (len < 5) {
        Serial.print(" - ERROR: Too short!");
        return;
    }
    
    // Data array indices (after header/length removal):
    // [0] = setting0 byte
    // [1] = unused
    // [2] = pulseCountMax
    // [3] = minSpeed 
    // [4] = setting1 byte
    
    uint8_t sett0 = data[0];
    steerConfig.InvertWAS = bitRead(sett0, 0);
    steerConfig.IsRelayActiveHigh = bitRead(sett0, 1);
    steerConfig.MotorDriveDirection = bitRead(sett0, 2);
    steerConfig.SingleInputWAS = bitRead(sett0, 3);
    steerConfig.CytronDriver = bitRead(sett0, 4);
    steerConfig.SteerSwitch = bitRead(sett0, 5);
    steerConfig.SteerButton = bitRead(sett0, 6);
    steerConfig.ShaftEncoder = bitRead(sett0, 7);
    
    steerConfig.PulseCountMax = data[2];
    steerConfig.MinSpeed = data[3];
    
    uint8_t sett1 = data[4];
    steerConfig.IsDanfoss = bitRead(sett1, 0);
    steerConfig.PressureSensor = bitRead(sett1, 1);
    steerConfig.CurrentSensor = bitRead(sett1, 2);
    steerConfig.IsUseY_Axis = bitRead(sett1, 3);
    
    Serial.print("\r\n  InvertWAS: "); Serial.print(steerConfig.InvertWAS);
    Serial.print("\r\n  MotorDriveDirection: "); Serial.print(steerConfig.MotorDriveDirection);
    Serial.print("\r\n  CytronDriver: "); Serial.print(steerConfig.CytronDriver);
    Serial.print("\r\n  SteerSwitch: "); Serial.print(steerConfig.SteerSwitch);
    Serial.print("\r\n  SteerButton: "); Serial.print(steerConfig.SteerButton);
    Serial.print("\r\n  PulseCountMax: "); Serial.print(steerConfig.PulseCountMax);
    Serial.print("\r\n  MinSpeed: "); Serial.print(steerConfig.MinSpeed);
}

void AutosteerProcessor::handleSteerSettings(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 252 - Steer Settings
    // Expected length is 14 bytes total, but we get data after header
    
    Serial.printf("\r\n[Autosteer] PGN 252 (Steer Settings) received, %d bytes", len);
    
    if (len < 8) {
        Serial.print(" - ERROR: Too short!");
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
    
    Serial.print("\r\n  Kp: "); Serial.print(steerSettings.Kp);
    Serial.print("\r\n  highPWM: "); Serial.print(steerSettings.highPWM);
    Serial.print("\r\n  lowPWM: "); Serial.print(steerSettings.lowPWM);
    Serial.print("\r\n  minPWM: "); Serial.print(steerSettings.minPWM);
    Serial.print("\r\n  steerSensorCounts: "); Serial.print(steerSettings.steerSensorCounts);
    Serial.print("\r\n  wasOffset: "); Serial.print(steerSettings.wasOffset);
    Serial.print("\r\n  AckermanFix: "); Serial.print(steerSettings.AckermanFix);
    
    // Update PID controller with new Kp
    pid.setKp(steerSettings.Kp);  // Use Kp from settings
    Serial.printf("\r\n  PID updated with Kp=%d", steerSettings.Kp);
}

void AutosteerProcessor::handleSteerData(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 254 - Steer Data (comes at 10Hz from AgOpenGPS)
    // For now, we only care about the autosteer enable bit
    
    // Comment out frequent logging
    // Serial.printf("\r\n[Autosteer] handleSteerData called! pgn=%d, len=%d", pgn, len);
    
    if (len < 3) {
        Serial.print(" - Too short, ignoring");
        return;  // Too short, ignore
    }
    
    lastPGN254Time = millis();
    lastCommandTime = millis();  // Update watchdog timer
    
    // Debug: print first few data bytes (commented out for now)
    // Serial.print(" Data:");
    // for (int i = 0; i < 8 && i < len; i++) {
    //     Serial.printf(" %02X", data[i]);
    // }
    
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
    
    // Debug speed and autosteer state
    static uint32_t lastSpeedDebug = 0;
    if (millis() - lastSpeedDebug > 1000) {
        Serial.printf("\r\n[Autosteer] PGN254: Speed=%dcm/s (%.1fkm/h) AutosteerBit=%d", 
                      speedCmS, vehicleSpeed, newAutosteerState);
        lastSpeedDebug = millis();
    }
    
    // Track guidance status changes
    prevGuidanceStatus = guidanceActive;
    guidanceActive = (status & 0x01) != 0;           // Bit 0 is guidance active
    guidanceStatusChanged = (guidanceActive != prevGuidanceStatus);
    
    // Extract steer angle
    int16_t angleRaw = (int16_t)(data[4] << 8 | data[3]);
    targetAngle = angleRaw / 100.0f;
    
    // Extract XTE
    crossTrackError = (int8_t)data[5];
    
    // Extract sections
    uint8_t sections1_8 = data[6];
    uint8_t sections9_16 = data[7];
    machineSections = (uint16_t)(sections9_16 << 8 | sections1_8);
    
    // Only print detailed info on state changes (commented out frequent logging)
    // Serial.printf(" Speed=%.1fkm/h Status=0x%02X(Auto=%d,Guide=%d) Angle=%.1f° XTE=%d Sections=%04X", 
    //               vehicleSpeed, status, newAutosteerState, guidanceActive, targetAngle, crossTrackError, machineSections);
    
    // Only print on state change to avoid spam
    if (newAutosteerState != autosteerEnabled) {
        Serial.printf("\r\n[Autosteer] Button state changed: %s -> %s", 
                      autosteerEnabled ? "ON" : "OFF",
                      newAutosteerState ? "ON" : "OFF");
        autosteerEnabled = newAutosteerState;
    }
}

// Static callback wrapper
void AutosteerProcessor::handlePGNStatic(uint8_t pgn, const uint8_t* data, size_t len) {
    if (instance) {
        // Don't print for PGN 254 or 200 since they come frequently
        if (pgn != 254 && pgn != 200) {
            Serial.printf("\r\n[Autosteer] Received PGN %d", pgn);
        }
        
        // Handle broadcast PGNs
        if (pgn == 200) {
            instance->handleHelloPGN(pgn, data, len);
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
        Serial.print("\r\n[Autosteer] ERROR: No instance!");
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
    if (adPTR) {
        currentAngle = adPTR->getWASAngle();
    }
    
    // Check if steering should be active
    if (!shouldSteerBeActive()) {
        // Debug why it's not active
        static uint32_t lastInactiveDebug = 0;
        if (millis() - lastInactiveDebug > 1000) {
            Serial.printf("\r\n[Autosteer] Motor disabled - guidance:%d enabled:%d state:%d speed:%.1fkm/h watchdog:%lums", 
                          guidanceActive, autosteerEnabled, steerState, vehicleSpeed,
                          millis() - lastCommandTime);
            lastInactiveDebug = millis();
        }
        
        // Stop motor
        motorSpeed = 0.0f;
        if (motorPTR) {
            motorPTR->enable(false);
            motorPTR->setSpeed(0.0f);
        }
        return;
    }
    
    // Calculate PID output
    float pidOutput = pid.compute(targetAngle, currentAngle);
    
    // Apply PWM scaling based on config
    if (configPTR) {
        float highPWM = configPTR->getHighPWM();
        float lowPWM = configPTR->getLowPWM();
        float minPWM = configPTR->getMinPWM();
        
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
            }
            
            // Convert to percentage for motor driver
            motorSpeed = (scaledPWM / 255.0f) * 100.0f;
            if (pidOutput < 0) motorSpeed = -motorSpeed;
        }
    } else {
        // No config, use raw PID output
        motorSpeed = pidOutput;
    }
    
    // Apply motor direction from config
    if (configPTR && configPTR->getMotorDriveDirection()) {
        motorSpeed = -motorSpeed;  // Invert if configured
    }
    
    // Send to motor
    if (motorPTR) {
        motorPTR->enable(true);
        motorPTR->setSpeed(motorSpeed);
    }
    
    // Debug output (reduced frequency)
    static uint32_t lastMotorDebug = 0;
    if (millis() - lastMotorDebug > 250) {  // Every 250ms
        Serial.printf("\r\n[Autosteer] Motor: Target=%.1f° Current=%.1f° PID=%.1f Speed=%.1f%%", 
                      targetAngle, currentAngle, pidOutput, motorSpeed);
        lastMotorDebug = millis();
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
    
    // Debug output
    static uint32_t lastActiveDebug = 0;
    if (!active && millis() - lastActiveDebug > 1000) {
        Serial.printf("\r\n[Autosteer] Not active: guidance=%d steerState=%d speed=%.1f", 
                      guidanceActive, steerState, vehicleSpeed);
        lastActiveDebug = millis();
    }
    
    return active;
}

void AutosteerProcessor::emergencyStop() {
    Serial.print("\r\n[Autosteer] EMERGENCY STOP");
    
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