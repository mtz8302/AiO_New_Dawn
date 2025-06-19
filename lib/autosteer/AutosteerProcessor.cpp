#include "AutosteerProcessor.h"
#include "ADProcessor.h"
#include "MotorDriverInterface.h"
#include "ConfigManager.h"
#include "PGNProcessor.h"
#include "IMUProcessor.h"

// External network function
extern void sendUDPbytes(uint8_t* data, int len);

// External global pointers
extern ConfigManager* configPTR;
extern MotorDriverInterface* motorPTR;
extern ADProcessor* adPTR;
extern IMUProcessor* imuPTR;

// Global pointer definition
AutosteerProcessor* autosteerPTR = nullptr;

// Singleton instance
AutosteerProcessor* AutosteerProcessor::instance = nullptr;

AutosteerProcessor::AutosteerProcessor() : 
    state(SteerState::OFF),
    targetAngle(0.0f),
    currentAngle(0.0f),
    motorSpeed(0.0f),
    lastUpdate(0),
    lastCommand(0),
    steerEnabled(false) {
}

AutosteerProcessor* AutosteerProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new AutosteerProcessor();
    }
    return instance;
}

bool AutosteerProcessor::init() {
    Serial.print("\r\n- Initializing AutosteerProcessor");
    
    // Get config values
    if (configPTR) {
        pid.setKp(configPTR->getKp());
        pid.setOutputLimit(100.0f);  // Motor accepts -100 to +100
    }
    
    // Register PGN handlers
    if (PGNProcessor::instance) {
        PGNProcessor::instance->registerCallback(254, handleSteerDataStatic, "AutosteerData");
        PGNProcessor::instance->registerCallback(252, handleSteerSettingsStatic, "AutosteerSettings");
        PGNProcessor::instance->registerCallback(251, handleSteerConfigStatic, "AutosteerConfig");
    }
    
    state = SteerState::READY;
    Serial.print(" - SUCCESS");
    return true;
}

void AutosteerProcessor::process() {
    // Debug to confirm we're being called
    static uint32_t lastProcessDebug = 0;
    static uint32_t processCount = 0;
    processCount++;
    if (millis() - lastProcessDebug > 5000) {
        Serial.printf("\r\n[Autosteer] process() called %lu times in last 5s", processCount);
        processCount = 0;
        lastProcessDebug = millis();
    }
    
    // Get current WAS angle
    if (adPTR) {
        currentAngle = adPTR->getWASAngle();
    }
    
    // Check for timeout (2 seconds)
    if (millis() - lastCommand > 2000 && state == SteerState::ACTIVE) {
        Serial.print("\r\n[Autosteer] Command timeout - disabling");
        state = SteerState::READY;
        steerEnabled = false;
    }
    
    // Static variables for button handling - MUST be at function scope
    static bool lastPhysicalState = false;
    static uint32_t lastButtonPress = 0;
    static bool buttonLatchedState = false;  // Latched state for button mode
    static bool firstRun = true;
    
    if (firstRun) {
        Serial.print("\r\n[Autosteer] Button state variables initialized");
        firstRun = false;
    }
    
    // Handle physical switch/button if configured
    if (configPTR && adPTR) {
        bool currentPhysicalState = adPTR->isSteerSwitchOn();
        
        // Debug config state periodically
        static uint32_t lastConfigDebug = 0;
        if (millis() - lastConfigDebug > 2000) {
            Serial.printf("\r\n[Autosteer] Config: SteerSwitch=%d, SteerButton=%d, PhysicalState=%d", 
                          configPTR->getSteerSwitch(), 
                          configPTR->getSteerButton(),
                          currentPhysicalState);
            lastConfigDebug = millis();
        }
        
        if (configPTR->getSteerSwitch() && !configPTR->getSteerButton()) {
            // Physical switch mode (not button mode)
            // Switch directly controls enable state
            if (currentPhysicalState != lastPhysicalState) {
                Serial.printf("\r\n[Autosteer] Physical switch changed to %s", 
                              currentPhysicalState ? "ON" : "OFF");
                enable(currentPhysicalState);
                lastPhysicalState = currentPhysicalState;
            }
        } else if (configPTR->getSteerButton() && !configPTR->getSteerSwitch()) {
            // Button mode (toggle on press, maintain state on release)
            
            // Debug output every 500ms
            static uint32_t lastDebugPrint = 0;
            static bool firstEntry = true;
            if (firstEntry) {
                Serial.print("\r\n[Autosteer] BUTTON MODE ACTIVE");
                firstEntry = false;
            }
            
            // Debug physical state changes
            if (currentPhysicalState != lastPhysicalState) {
                Serial.printf("\r\n[Autosteer] Button physical state CHANGED: %d -> %d (latched=%d)", 
                              lastPhysicalState, currentPhysicalState, buttonLatchedState);
            }
            
            if (millis() - lastDebugPrint > 500) {
                Serial.printf("\r\n[Autosteer] Button: physical=%d, latched=%d, enabled=%d", 
                              currentPhysicalState, buttonLatchedState, steerEnabled);
                lastDebugPrint = millis();
            }
            
            // Detect rising edge (button press) - button goes from 0 to 1 when pressed
            if (currentPhysicalState && !lastPhysicalState) {
                uint32_t timeSinceLastPress = millis() - lastButtonPress;
                Serial.printf("\r\n[Autosteer] Button press detected, time since last: %lu ms", timeSinceLastPress);
                
                if (timeSinceLastPress > 1000) { // 1 second minimum between presses
                    // Toggle the latched state
                    buttonLatchedState = !buttonLatchedState;
                    Serial.printf("\r\n[Autosteer] Button PRESSED - latched state now %s", 
                                  buttonLatchedState ? "ON" : "OFF");
                    lastButtonPress = millis();
                } else {
                    Serial.printf("\r\n[Autosteer] Button press IGNORED (too soon - %lu ms)", timeSinceLastPress);
                }
            }
            
            // Always apply the latched state (not the physical button state)
            if (steerEnabled != buttonLatchedState) {
                Serial.printf("\r\n[Autosteer] Applying latched state: %d -> %d", steerEnabled, buttonLatchedState);
                enable(buttonLatchedState);
                
                // Double-check it stuck
                if (steerEnabled != buttonLatchedState) {
                    Serial.printf("\r\n[Autosteer] ERROR: State didn't stick! enabled=%d, latched=%d", 
                                  steerEnabled, buttonLatchedState);
                }
            }
            
            lastPhysicalState = currentPhysicalState;
        }
        // If neither switch nor button mode, use virtual button from AgOpenGPS
    }
    
    // State machine
    switch (state) {
        case SteerState::OFF:
            // Do nothing
            motorSpeed = 0.0f;
            break;
            
        case SteerState::READY:
            // Motor disabled, waiting for command
            motorSpeed = 0.0f;
            if (motorPTR) {
                motorPTR->enable(false);
            }
            break;
            
        case SteerState::ACTIVE:
            // Calculate motor speed using PID
            if (steerEnabled) {
                motorSpeed = pid.compute(targetAngle, currentAngle);
                
                // Serial.printf("\r\n[Autosteer] ACTIVE: Target=%.1f Current=%.1f Error=%.1f Motor=%.1f%%", 
                //               targetAngle, currentAngle, targetAngle - currentAngle, motorSpeed);
                
                if (motorPTR) {
                    motorPTR->enable(true);
                    motorPTR->setSpeed(motorSpeed);
                }
            } else {
                // Autosteer disabled
                Serial.printf("\r\n[Autosteer] Switching to READY - SteerEnabled=%d", steerEnabled);
                state = SteerState::READY;
                if (motorPTR) {
                    motorPTR->enable(false);
                }
            }
            break;
    }
    
    // Send PGN 253 (autosteer data) back to AgOpenGPS every 100ms
    static uint32_t lastPGN253Send = 0;
    if (millis() - lastPGN253Send > 100) {
        lastPGN253Send = millis();
        sendPGN253();
    }
    
    lastUpdate = millis();
}

void AutosteerProcessor::setTargetAngle(float angle) {
    targetAngle = angle;
}

void AutosteerProcessor::enable(bool enabled) {
    steerEnabled = enabled;
    
    if (enabled && state == SteerState::READY) {
        state = SteerState::ACTIVE;
        Serial.print("\r\n[Autosteer] Enabled");
    } else if (!enabled && state == SteerState::ACTIVE) {
        state = SteerState::READY;
        Serial.print("\r\n[Autosteer] Disabled");
    }
}

void AutosteerProcessor::emergencyStop() {
    state = SteerState::READY;
    steerEnabled = false;
    motorSpeed = 0.0f;
    
    if (motorPTR) {
        motorPTR->stop();
        motorPTR->enable(false);
    }
    
    Serial.print("\r\n[Autosteer] EMERGENCY STOP");
}

void AutosteerProcessor::handleSteerData(uint8_t* data, uint8_t len) {
    // Serial.printf("\r\n[DEBUG] handleSteerData: len=%d", len);
    if (len < 9) {
        // Serial.printf(" - TOO SHORT!");
        return;
    }
    
    // PGN 254 format:
    // 0: Speed
    // 1: Status
    // 2-3: Steer angle setpoint (int16)
    // 4: Tram
    // 5: Relay
    // 6: Relay Hi
    // 7-8: Reserved
    // 9: Checksum
    
    lastCommand = millis();
    
    // Get steer angle setpoint (divide by 100)
    int16_t steerAngleRaw = (int16_t)(data[3] << 8 | data[2]);
    float steerAngle = steerAngleRaw / 100.0f;
    
    // Get status byte
    uint8_t status = data[1];
    bool guidanceActive = (status & 0x01);
    bool autosteerActive = (status & 0x40);  // Bit 6 seems to be the autosteer enable
    
    // Debug output only on status change
    static uint8_t lastStatus = 0xFF;
    if (status != lastStatus) {
        Serial.printf("\r\n[Autosteer] PGN254: Status=0x%02X (Bit0=%d, Bit6=%d) Angle=%.1f°", 
                      status, 
                      guidanceActive,
                      autosteerActive ? 1 : 0,
                      steerAngle);
        lastStatus = status;
    }
    
    // Update target angle
    setTargetAngle(steerAngle);
    
    // Only use virtual button if no physical switch/button configured
    if (configPTR) {
        if (!configPTR->getSteerSwitch() && !configPTR->getSteerButton()) {
            // No physical switch/button - use virtual button from AgOpenGPS
            enable(autosteerActive);
        } else if (configPTR->getSteerButton()) {
            // In button mode - completely ignore virtual button
            // Physical button has full control
            if (autosteerActive != steerEnabled) {
                Serial.printf("\r\n[Autosteer] WARNING: AgIO trying to set state to %d, but button mode active (keeping %d)", 
                              autosteerActive, steerEnabled);
            }
        }
        // Physical switch handling is done in process()
    } else {
        // No config - default to virtual button
        enable(autosteerActive);
    }
}

void AutosteerProcessor::handleSteerSettings(uint8_t* data, uint8_t len) {
    if (len < 8) return;
    
    // PGN 252 format:
    // 0: Kp
    // 1: highPWM  
    // 2: lowPWM
    // 3: minPWM
    // 4-5: steer sensor counts
    // 6-7: was offset
    
    float kp = data[0] / 10.0f;  // Kp is sent as byte * 10
    pid.setKp(kp);
    
    // Update config
    if (configPTR) {
        configPTR->setKp(kp);
        configPTR->setHighPWM(data[1]);
        configPTR->setLowPWM(data[2]);
        configPTR->setMinPWM(data[3]);
        
        // Save to EEPROM
        configPTR->saveSteerSettings();
    }
    
    Serial.printf("\r\n[Autosteer] Settings updated: Kp=%.1f", kp);
}

void AutosteerProcessor::handleSteerConfig(uint8_t* data, uint8_t len) {
    if (len < 4) return;
    
    // Debug raw data
    Serial.printf("\r\n[Autosteer] PGN 251 received, len=%d, bytes:", len);
    for (int i = 0; i < len; i++) {
        Serial.printf(" %02X", data[i]);
    }
    
    // PGN 251 format:
    // 0: byte 0 (unused)
    // 1: sett0 - bit flags
    // 2: pulseCountMax  
    // 3: minSpeed
    // 4: sett1 - bit flags
    
    uint8_t sett0 = data[1];
    uint8_t pulseCountMax = data[2];
    uint8_t minSpeed = data[3];
    uint8_t sett1 = data[4];
    
    // Update config settings
    if (configPTR) {
        // Extract boolean flags from setting0 byte
        configPTR->setInvertWAS(bitRead(sett0, 0));
        configPTR->setIsRelayActiveHigh(bitRead(sett0, 1));
        configPTR->setMotorDriveDirection(bitRead(sett0, 2));
        configPTR->setSingleInputWAS(bitRead(sett0, 3));
        configPTR->setCytronDriver(bitRead(sett0, 4));
        configPTR->setSteerSwitch(bitRead(sett0, 5));
        configPTR->setSteerButton(bitRead(sett0, 6));
        configPTR->setShaftEncoder(bitRead(sett0, 7));
        
        // Set numeric values
        configPTR->setPulseCountMax(pulseCountMax);
        configPTR->setMinSpeed(minSpeed);
        
        // Extract boolean flags from setting1 byte
        configPTR->setIsDanfoss(bitRead(sett1, 0));
        configPTR->setPressureSensor(bitRead(sett1, 1));
        configPTR->setCurrentSensor(bitRead(sett1, 2));
        configPTR->setIsUseYAxis(bitRead(sett1, 3));
        
        // Save to EEPROM
        configPTR->saveSteerConfig();
    }
    
    Serial.printf("\r\n[Autosteer] Config updated: sett0=0x%02X MotorDir=%d, Cytron=%d, SteerSwitch=%d, SteerButton=%d", 
                  sett0, bitRead(sett0, 2), bitRead(sett0, 4), bitRead(sett0, 5), bitRead(sett0, 6));
    
    // Verify what was saved
    Serial.printf("\r\n[Autosteer] Config stored: SteerSwitch=%d, SteerButton=%d", 
                  configPTR->getSteerSwitch(), configPTR->getSteerButton());
}

// Static callback wrappers
void AutosteerProcessor::handleSteerDataStatic(uint8_t pgn, const uint8_t* data, size_t len) {
    // Serial.printf("\r\n[DEBUG] handleSteerDataStatic called: pgn=%d, len=%d", pgn, len);
    if (instance) {
        // Handle broadcast PGNs first
        if (pgn == 200) {
            // Hello from AgIO - send autosteer reply with actual data
            // Format: {0x80, 0x81, source, pgn, length, angleLo, angleHi, countsLo, countsHi, switches, checksum}
            
            // Get current WAS angle in degrees * 100
            int16_t angleInt = (int16_t)(instance->currentAngle * 100.0f);
            
            // Get WAS raw counts
            uint16_t counts = adPTR ? adPTR->getWASRaw() : 0;
            
            // Build switch byte
            uint8_t switchByte = 0;
            if (adPTR) {
                if (adPTR->isSteerSwitchOn()) switchByte |= 0x01;  // Steer switch on bit 0
                if (adPTR->isWorkSwitchOn()) switchByte |= 0x02;   // Work switch on bit 1
            }
            
            uint8_t helloFromSteer[] = {
                0x80, 0x81, 
                0x7E,  // Source: Steer module
                0x7E,  // PGN: Steer reply
                5,     // Length
                (uint8_t)(angleInt & 0xFF),         // Angle low byte
                (uint8_t)((angleInt >> 8) & 0xFF),  // Angle high byte
                (uint8_t)(counts & 0xFF),            // Counts low byte
                (uint8_t)((counts >> 8) & 0xFF),     // Counts high byte
                switchByte,                          // Switch states
                0      // CRC placeholder
            };
            
            // Calculate checksum
            uint8_t ck = 0;
            for (int i = 2; i < 10; i++) {
                ck += helloFromSteer[i];
            }
            helloFromSteer[10] = ck;
            
            // Send via UDP
            sendUDPbytes(helloFromSteer, sizeof(helloFromSteer));
        }
        else if (pgn == 254) {
            // Serial.printf("\r\n[DEBUG] Calling handleSteerData");
            instance->handleSteerData((uint8_t*)data, len);
        }
    }
}

void AutosteerProcessor::handleSteerSettingsStatic(uint8_t pgn, const uint8_t* data, size_t len) {
    if (instance) {
        // Handle broadcast PGNs first
        if (pgn == 200) {
            // Hello already handled in handleSteerDataStatic
            // Both callbacks receive broadcasts, so we only need to respond once
            return;
        }
        else if (pgn == 252) {
            instance->handleSteerSettings((uint8_t*)data, len);
        }
    }
}

void AutosteerProcessor::handleSteerConfigStatic(uint8_t pgn, const uint8_t* data, size_t len) {
    if (instance) {
        // Handle broadcast PGNs first
        if (pgn == 200) {
            // Hello already handled in handleSteerDataStatic
            return;
        }
        else if (pgn == 251) {
            instance->handleSteerConfig((uint8_t*)data, len);
        }
    }
}

void AutosteerProcessor::sendPGN253() {
    // PGN 253 - From AutoSteer
    // Format: {header, source, pgn, length, 
    //          actualSteerAngle*100 (2 bytes), 
    //          imuHeading (2 bytes), 
    //          imuRoll (2 bytes), 
    //          switchByte, 
    //          pwmDisplay, 
    //          checksum}
    
    uint8_t pgn253[] = {
        0x80, 0x81,  // Header
        0x7E,        // Source: 126 (steer module)
        0xFD,        // PGN: 253
        8,           // Length
        0, 0,        // ActualSteerAngle * 100 (bytes 5-6)
        0, 0,        // IMU Heading Hi/Lo (bytes 7-8)
        0, 0,        // IMU Roll Hi/Lo (bytes 9-10)
        0,           // Switch byte (byte 11)
        0,           // PWM Display (byte 12)
        0            // Checksum
    };
    
    // Get current WAS angle in degrees * 100
    int16_t steerAngle = (int16_t)(currentAngle * 100.0f);
    pgn253[5] = (uint8_t)(steerAngle & 0xFF);      // Low byte first
    pgn253[6] = (uint8_t)((steerAngle >> 8) & 0xFF); // High byte second
    
    // Get IMU data if available
    if (imuPTR && imuPTR->hasValidData()) {
        IMUData imuData = imuPTR->getCurrentData();
        
        // Heading in degrees * 10
        int16_t heading = (int16_t)(imuData.heading * 10.0f);
        pgn253[7] = (uint8_t)((heading >> 8) & 0xFF);  // High byte
        pgn253[8] = (uint8_t)(heading & 0xFF);         // Low byte
        
        // Roll in degrees * 10
        int16_t roll = (int16_t)(imuData.roll * 10.0f);
        pgn253[9] = (uint8_t)((roll >> 8) & 0xFF);     // High byte
        pgn253[10] = (uint8_t)(roll & 0xFF);           // Low byte
    }
    
    // Build switch byte
    uint8_t switchByte = 0;
    if (adPTR) {
        if (adPTR->isWorkSwitchOn()) switchByte |= 0x01;   // Work switch on bit 0
    }
    // For steer switch, send the actual autosteer enabled state, not physical button
    if (steerEnabled) {
        switchByte |= 0x02;  // Steer enabled on bit 1
    }
    pgn253[11] = switchByte;
    
    // PWM Display value (motor PWM percentage)
    pgn253[12] = (uint8_t)(abs(motorSpeed) * 2.55f); // Convert -100 to 100 into 0-255
    
    // Calculate checksum
    uint8_t ck = 0;
    for (int i = 2; i < 13; i++) {
        ck += pgn253[i];
    }
    pgn253[13] = ck;
    
    // Send via UDP
    sendUDPbytes(pgn253, sizeof(pgn253));
}