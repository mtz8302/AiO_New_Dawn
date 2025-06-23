#include "AutosteerProcessor.h"
#include "ADProcessor.h"
#include "MotorDriverInterface.h"
#include "ConfigManager.h"
#include "PGNProcessor.h"
#include "IMUProcessor.h"
#include "KickoutMonitor.h"
#include "KeyaCANDriver.h"

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
    steerEnabled(false),
    lastAgOpenGPSState(false) {
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
        pid.setOutputLimit(100.0f);  // PID works with -100 to +100
        
        // Debug: Print button config on startup
        Serial.printf("\r\n  SteerSwitch=%d, SteerButton=%d", 
                      configPTR->getSteerSwitch(), 
                      configPTR->getSteerButton());
    }
    
    // Initialize kickout monitor
    KickoutMonitor::getInstance()->init();
    
    // Register PGN handlers
    if (PGNProcessor::instance) {
        Serial.print("\r\n  Registering PGN callbacks...");
        bool reg254 = PGNProcessor::instance->registerCallback(254, handleSteerDataStatic, "AutosteerData");
        bool reg252 = PGNProcessor::instance->registerCallback(252, handleSteerSettingsStatic, "AutosteerSettings");
        bool reg251 = PGNProcessor::instance->registerCallback(251, handleSteerConfigStatic, "AutosteerConfig");
        Serial.printf("\r\n  PGN registrations: 254=%d, 252=%d, 251=%d", reg254, reg252, reg251);
    } else {
        Serial.print("\r\n  ERROR: PGNProcessor not initialized!");
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
    
    // Check kickout monitor FIRST
    KickoutMonitor* kickoutMon = KickoutMonitor::getInstance();
    kickoutMon->process();
    
    if (kickoutMon->hasKickout() && state == SteerState::ACTIVE) {
        Serial.printf("\r\n[AUTOSTEER] KICKOUT TRIGGERED: %s", 
                      kickoutMon->getReasonString());
        emergencyStop();
        steerEnabled = false;  // Just disable - no special state
        kickoutTime = millis();  // Start cooldown timer
        kickoutMon->clearKickout();  // Clear immediately
        return;
    }
    
    // Check Keya motor slip if using Keya driver
    if (motorPTR && motorPTR->getType() == MotorDriverType::KEYA_CAN && state == SteerState::ACTIVE) {
        KeyaCANDriver* keya = static_cast<KeyaCANDriver*>(motorPTR);
        if (keya->checkMotorSlip()) {
            Serial.print("\r\n[AUTOSTEER] KICKOUT: Keya motor slip detected");
            emergencyStop();
            steerEnabled = false;  // Just disable - no special state
            kickoutTime = millis();  // Start cooldown timer
            return;
        }
    }
    
    // Clear kickout if we're not in active state
    if (state != SteerState::ACTIVE && kickoutMon->hasKickout()) {
        kickoutMon->clearKickout();
    }
    
    
    // Check for timeout (2 seconds)
    if (millis() - lastCommand > 2000 && state == SteerState::ACTIVE) {
        Serial.print("\r\n[Autosteer] Command timeout - disabling");
        state = SteerState::READY;
        steerEnabled = false;
    }
    
    // Static variable for button edge detection
    static bool lastPhysicalState = false;
    
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
            // Button mode - simple toggle on press
            
            // Detect rising edge (button press)
            if (currentPhysicalState && !lastPhysicalState) {
                // Toggle enable state
                steerEnabled = !steerEnabled;
                enable(steerEnabled);
                Serial.printf("\r\n[Autosteer] Button pressed - autosteer %s", 
                              steerEnabled ? "ENABLED" : "DISABLED");
                
                // Record button press time
                buttonPressTime = millis();
                
                // Update lastAgOpenGPSState to prevent spurious toggles
                lastAgOpenGPSState = steerEnabled;
                
                // Send update to AgOpenGPS immediately
                sendPGN253();
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
                float pidOutput = pid.compute(targetAngle, currentAngle);
                
                // Apply PWM scaling based on AgOpenGPS settings
                if (configPTR) {
                    float highPWM = configPTR->getHighPWM();
                    float lowPWM = configPTR->getLowPWM();
                    float minPWM = configPTR->getMinPWM();
                    
                    // Scale PID output to PWM range
                    if (abs(pidOutput) < 0.1f) {
                        // Dead zone
                        motorSpeed = 0.0f;
                    } else {
                        // PWM calculation:
                        // - LowPWM = base PWM to start motor moving
                        // - HighPWM = maximum PWM limit (not to exceed)
                        // - PID output adds to base, but total is capped at HighPWM
                        float absPID = abs(pidOutput);
                        
                        // Start with lowPWM as base, add PID contribution
                        // Scale PID output (0-100%) to available headroom (lowPWM to highPWM)
                        float pwmRange = highPWM - lowPWM;
                        float scaledPWM = lowPWM + (absPID / 100.0f) * pwmRange;
                        
                        // Ensure we don't exceed highPWM limit
                        if (scaledPWM > highPWM) {
                            scaledPWM = highPWM;
                        }
                        
                        // Apply minimum PWM threshold
                        if (scaledPWM < minPWM) {
                            scaledPWM = 0.0f;
                        }
                        
                        // Convert back to percentage for motor driver (0-100%)
                        motorSpeed = (scaledPWM / 255.0f) * 100.0f;
                        if (pidOutput < 0) motorSpeed = -motorSpeed;
                    }
                } else {
                    // No config, use raw PID output
                    motorSpeed = pidOutput;
                }
                
                // Show RPM data if using Keya motor
                if (motorPTR && motorPTR->getType() == MotorDriverType::KEYA_CAN) {
                    KeyaCANDriver* keya = static_cast<KeyaCANDriver*>(motorPTR);
                    Serial.printf("\r\n[Autosteer] ACTIVE: Target=%.1f° Current=%.1f° PID=%.1f Motor=%.1f%% | CMD_RPM=%.0f ACT_RPM=%.0f", 
                                  targetAngle, currentAngle, pidOutput, motorSpeed,
                                  keya->getCommandedRPM(), keya->getActualRPM());
                } else {
                    Serial.printf("\r\n[Autosteer] ACTIVE: Target=%.1f Current=%.1f PID=%.1f Motor=%.1f%% (H=%d L=%.0f M=%d)", 
                                  targetAngle, currentAngle, pidOutput, motorSpeed,
                                  configPTR ? configPTR->getHighPWM() : 255,
                                  configPTR ? configPTR->getLowPWM() : 0,
                                  configPTR ? configPTR->getMinPWM() : 0);
                }
                
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
    // Check if we're in kickout cooldown period
    if (enabled && kickoutTime > 0) {
        if (millis() - kickoutTime < KICKOUT_COOLDOWN_MS) {
            // Still in cooldown - ignore enable request
            static uint32_t lastCooldownMsg = 0;
            if (millis() - lastCooldownMsg > 500) {  // Only print message every 500ms
                uint32_t remaining = KICKOUT_COOLDOWN_MS - (millis() - kickoutTime);
                Serial.printf("\r\n[Autosteer] Kickout cooldown active - %lu ms remaining", remaining);
                lastCooldownMsg = millis();
            }
            return;
        } else {
            // Cooldown expired
            kickoutTime = 0;
            Serial.print("\r\n[Autosteer] Kickout cooldown expired");
        }
    }
    
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
    
    // Serial.printf("\r\n[DEBUG] handleSteerData called: len=%d", len);
    // Serial.printf("\r\n[DEBUG] Data bytes: ");
    // for (int i = 0; i < len && i < 8; i++) {
    //     Serial.printf("[%d]=0x%02X ", i, data[i]);
    // }
    
    // PGN 254 format (data array positions after removing length byte):
    // 0: Status
    // 1-2: Steer angle setpoint (int16)
    // 3: Tram
    // 4: Relay
    // 5: Relay Hi
    // 6-7: Reserved
    
    lastCommand = millis();
    
    // Get steer angle setpoint (divide by 100)
    int16_t steerAngleRaw = (int16_t)(data[2] << 8 | data[1]);
    float steerAngle = steerAngleRaw / 100.0f;
    
    // Get status byte
    uint8_t status = data[0];
    bool guidanceActive = (status & 0x01);
    bool autosteerActive = (status & 0x40);  // Bit 6 seems to be the autosteer enable
    
    // Debug the status byte
    // Serial.printf("\r\n[DEBUG] Status byte: 0x%02X, guidance=%d, autosteer=%d", 
    //               status, guidanceActive, autosteerActive);
    
    // Debug output only on status change
    static uint8_t lastStatus = 0xFF;
    static bool lastAutosteerActive = false;
    if (status != lastStatus || autosteerActive != lastAutosteerActive) {
        Serial.printf("\r\n[Autosteer] PGN254: Status=0x%02X (Guidance=%d, Autosteer=%d) Angle=%.1f°", 
                      status, 
                      guidanceActive ? 1 : 0,
                      autosteerActive ? 1 : 0,
                      steerAngle);
        if (autosteerActive != lastAutosteerActive) {
            Serial.printf(" [AUTOSTEER STATE CHANGED: %d->%d]", lastAutosteerActive, autosteerActive);
        }
        lastStatus = status;
        lastAutosteerActive = autosteerActive;
    }
    
    // Update target angle
    setTargetAngle(steerAngle);
    
    // Only use virtual button if no physical switch/button configured
    if (configPTR) {
        if (!configPTR->getSteerSwitch() && !configPTR->getSteerButton()) {
            // No physical switch/button - use virtual button from AgOpenGPS
            enable(autosteerActive);
        } else if (configPTR->getSteerButton()) {
            // In button mode - detect when AgOpenGPS changes state
            // But ignore for 1 second after physical button press
            uint32_t timeSinceButton = millis() - instance->buttonPressTime;
            
            if (autosteerActive != instance->lastAgOpenGPSState && timeSinceButton > 1000) {
                // AgOpenGPS changed the state
                instance->lastAgOpenGPSState = autosteerActive;
                enable(autosteerActive);
                steerEnabled = autosteerActive;
                Serial.printf("\r\n[Autosteer] AgOpenGPS toggled - autosteer %s", 
                              autosteerActive ? "ENABLED" : "DISABLED");
            } else if (timeSinceButton > 1000) {
                // After 1 second, sync our tracking variable
                instance->lastAgOpenGPSState = autosteerActive;
            }
        }
        // Physical switch handling is done in process()
    } else {
        // No config - default to virtual button
        enable(autosteerActive);
    }
}

void AutosteerProcessor::handleSteerSettings(uint8_t* data, uint8_t len) {
    
    // Debug: Print raw bytes
    Serial.printf("\r\n[Autosteer] PGN252 raw data (%d bytes): ", len);
    for (int i = 0; i < len && i < 10; i++) {
        Serial.printf("[%d]=0x%02X(%d) ", i, data[i], data[i]);
    }
    
    // PGN 252 format per PGN.md:
    // 0: gainP (proportional gain)
    // 1: highPWM (max limit)
    // 2: lowPWM (minimum to move)
    // 3: minPWM
    // 4: countsPerDeg
    // 5-6: steerOffset
    // 7: ackermanFix
    
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
    
    Serial.printf("\r\n[Autosteer] Settings updated: Kp=%.1f, HighPWM=%d, LowPWM=%d, MinPWM=%d", 
                  kp, data[1], data[2], data[3]);
}

void AutosteerProcessor::handleSteerConfig(uint8_t* data, uint8_t len) {
    
    // Debug raw data
    Serial.printf("\r\n[Autosteer] PGN 251 received, len=%d, bytes:", len);
    for (int i = 0; i < len; i++) {
        Serial.printf(" %02X", data[i]);
    }
    
    // PGN 251 format (data array positions after removing length byte):
    // 0: sett0 - bit flags
    // 1: byte 1 (unused)
    // 2: pulseCountMax  
    // 3: minSpeed
    // 4: sett1 - bit flags
    
    uint8_t sett0 = data[0];
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
            if (instance->steerEnabled) switchByte |= 0x01;  // Steer enabled on bit 0
            if (adPTR && adPTR->isWorkSwitchOn()) switchByte |= 0x02;   // Work switch on bit 1
            
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