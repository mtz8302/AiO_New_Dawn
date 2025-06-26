#include "MachineProcessor.h"
#include <Arduino.h>
#include "PGNProcessor.h"
#include "PGNUtils.h"
#include <Wire.h>
#include "Adafruit_PWMServoDriver.h"

extern void sendUDPbytes(uint8_t *message, int msgLen);

// External network configuration - defined elsewhere
struct NetConfigStruct {
    uint8_t currentIP[4];
    uint8_t subnetMask[4];
    uint8_t gatewayIP[4];
    uint8_t destIP[4];
    uint16_t listenPort;
    uint16_t destPort;
};
extern NetConfigStruct netConfig;

// PCA9685 PWM driver for section outputs - moved to static instance
// Note: Front panel LEDs use 0x70 on Wire, sections use 0x44

// Hardware pin mappings from schematic
// Section signal pins on PCA9685 (control the actual sections)
const uint8_t SECTION_PINS[6] = {0, 1, 4, 5, 10, 9};  // SEC1_SIG through SEC6_SIG

// DRV8234 control pins on PCA9685
const uint8_t DRVOFF_PINS[3] = {2, 6, 8};    // DRVOFF pins (must be LOW to enable)

// DRV8234 sleep pins - these need a reset pulse to activate
const uint8_t SLEEP_PINS[3] = {
    13, // Section 1/2 nSLEEP
    3,  // Section 3/4 nSLEEP
    7   // Section 5/6 nSLEEP
};
// Note: Pins 14 (LOCK) and 15 (AUX) removed - may be motor-related

// Hardware configuration:
// - nSLEEP: Also has 4.7K pullup, but needs reset pulse
// - MODE: Pulled LOW through 8.2K resistor (independent mode)
// - Solder jumpers OPEN = Independent mode

// In independent mode:
// - Each INx directly controls OUTx
// - HIGH on INx = OUTx sinks current (LED ON)
// - LOW on INx = OUTx high impedance (LED OFF)

// Static instance pointers
MachineProcessor* MachineProcessor::instance = nullptr;
MachineProcessor* machinePTR = nullptr;

// Static member to avoid global constructor
static Adafruit_PWMServoDriver& getSectionOutputs() {
    static Adafruit_PWMServoDriver sectionOutputs(0x44, Wire);  // Changed from Wire1 to Wire
    return sectionOutputs;
}

MachineProcessor::MachineProcessor() {
    Serial.println("MachineProcessor: Constructor called");
}

MachineProcessor* MachineProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new MachineProcessor();
        machinePTR = instance;
    }
    return instance;
}

bool MachineProcessor::init() {
    Serial.println("MachineProcessor: Initializing (Phase 4 - Full functionality)");
    getInstance();
    return instance->initialize();
}

bool MachineProcessor::initialize() {
    Serial.println("MachineProcessor: Initializing...");
    
    // Clear initial state
    memset(&sectionState, 0, sizeof(sectionState));
    sectionState.autoStates = 0xFFFF;  // Start with all sections in auto mode
    state.lastPGN238Time = 0;
    state.lastPGN232Time = 0;
    state.isLowered = true;
    
    // Phase 4: Hardware initialization enabled
    if (!initializeSectionOutputs()) {
        Serial.println("MachineProcessor: ERROR - Failed to initialize section outputs!");
        return false;
    }
    
    // Register PGN handlers
    Serial.println("MachineProcessor: Registering PGN callbacks...");
    // We'll receive broadcast PGNs (200, 202) through our normal registrations
    bool reg238 = PGNProcessor::instance->registerCallback(MACHINE_PGN_CONFIG, handlePGN238, "Machine");
    bool reg239 = PGNProcessor::instance->registerCallback(MACHINE_PGN_DATA, handlePGN239, "Machine");
    Serial.printf("MachineProcessor: PGN registrations - 238:%d, 239:%d\n", reg238, reg239);
    
    Serial.println("MachineProcessor: Initialized successfully with hardware");
    return true;
}

bool MachineProcessor::initializeSectionOutputs() {
    Serial.println("MachineProcessor: Initializing section outputs...");
    
    // 1. Check for PCA9685 at expected address
    if (!checkPCA9685()) {
        return false;
    }
    
    // 2. Initialize PCA9685
    getSectionOutputs().begin();
    Wire.setClock(1000000);  // Set to 1MHz for PCA9685 (same as working version)
    
    // 3. Wake PCA9685 from sleep mode
    getSectionOutputs().reset();  // This clears MODE1 sleep bit
    delay(1);  // Oscillator stabilization
    
    // 4. Configure PCA9685
    getSectionOutputs().setPWMFreq(1526);  // Max frequency
    getSectionOutputs().setOutputMode(true);  // Push-pull outputs
    
    // 5. Put all DRV8234s to sleep initially
    Serial.println("MachineProcessor: Putting all DRV8234 drivers to sleep");
    for (uint8_t pin : SLEEP_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW for sleep mode
    }
    delayMicroseconds(150); // Wait for sleep mode to settle
    
    // 6. Set all section outputs to OFF state before waking drivers
    Serial.println("MachineProcessor: Setting section outputs LOW (OFF state)");
    for (uint8_t pin : SECTION_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW = OFF
    }
    
    // 7. Wake up the section DRV8234s with reset pulse
    Serial.println("MachineProcessor: Waking section DRV8234 drivers");
    getSectionOutputs().setPin(13, 187, 1); // Section 1/2 - 30µs LOW pulse
    getSectionOutputs().setPin(3, 187, 1);  // Section 3/4 - 30µs LOW pulse
    getSectionOutputs().setPin(7, 187, 1);  // Section 5/6 - 30µs LOW pulse
    
    // 9. Enable DRV8234 outputs by setting DRVOFF LOW
    Serial.println("MachineProcessor: Enabling DRV8234 outputs (DRVOFF = LOW)");
    for (uint8_t pin : DRVOFF_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW to enable outputs
    }
    
    Serial.println("MachineProcessor: Section outputs initialized");
    return true;
}

bool MachineProcessor::checkPCA9685() {
    Wire.beginTransmission(0x44);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("MachineProcessor: Found PCA9685 at 0x44");
        return true;
    }
    Serial.println("MachineProcessor: ERROR - PCA9685 not found at 0x44!");
    return false;
}

void MachineProcessor::process() {
    // Phase 3: Process enabled with status logging
    static uint32_t lastStatusTime = 0;
    
    if (millis() - lastStatusTime > 5000) {
        lastStatusTime = millis();
        if (isActive()) {
            printStatus();
        }
    }
}

void MachineProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) {
        Serial.println("MachineProcessor: ERROR - No instance for broadcast PGN!");
        return;
    }
    
    if (pgn == 200) {
        // Serial.println("\r\n[MachineProcessor] Received Hello broadcast (PGN 200)");
        
        uint8_t helloReply[] = {
            0x80, 0x81,               // Header
            MACHINE_HELLO_REPLY,      // Source: Machine module (123)
            MACHINE_HELLO_REPLY,      // PGN: Machine reply (123)
            5,                        // Length
            0, 0, 0, 0, 0,           // Data
            0                         // CRC placeholder
        };
        
        calculateAndSetCRC(helloReply, sizeof(helloReply));
        sendUDPbytes(helloReply, sizeof(helloReply));
        
        // Serial.println("[MachineProcessor] Sent Hello reply (PGN 123)");
    }
    else if (pgn == 202) {
        Serial.println("\r\n[MachineProcessor] Received Scan Request (PGN 202)");
        
        uint8_t scanReply[] = {
            0x80, 0x81,                    // Header
            MACHINE_HELLO_REPLY,           // Source: Machine module (123)
            0xCB,                          // PGN: 203 Scan reply
            7,                             // Length
            netConfig.currentIP[0],
            netConfig.currentIP[1],
            netConfig.currentIP[2],
            netConfig.currentIP[3],
            netConfig.currentIP[0],        // Subnet (repeat IP)
            netConfig.currentIP[1],
            netConfig.currentIP[2],
            0                              // CRC placeholder
        };
        
        calculateAndSetCRC(scanReply, sizeof(scanReply));
        sendUDPbytes(scanReply, sizeof(scanReply));
        
        Serial.printf("[MachineProcessor] Sent Scan reply IP: %d.%d.%d.%d",
                      netConfig.currentIP[0], netConfig.currentIP[1], 
                      netConfig.currentIP[2], netConfig.currentIP[3]);
    }
}

void MachineProcessor::handlePGN238(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // First check if this is a broadcast PGN
    if (pgn == 200 || pgn == 202) {
        Serial.printf("\r\n[Machine] Received broadcast PGN %d via handler 238", pgn);
        handleBroadcastPGN(pgn, data, len);
        return;
    }
    
    // Silently ignore 3-byte PGN 238 messages (unknown purpose)
    if (len < 8) {
        return;
    }
    
    Serial.println("MachineProcessor: Received PGN 238 (Machine Config)");
    
    instance->state.lastPGN238Time = millis();
    
    instance->config.raiseTime = data[0];
    instance->config.lowerTime = data[1];
    instance->config.enableHydraulicLift = (data[2] & 0x01);
    
    Serial.printf("MachineProcessor: Config - Raise:%d Lower:%d Hydraulic:%s\n", 
        instance->config.raiseTime, 
        instance->config.lowerTime,
        instance->config.enableHydraulicLift ? "Yes" : "No");
    
    // No reply needed - AgIO does nothing with PGN 237
}

void MachineProcessor::handlePGN239(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // First check if this is a broadcast PGN
    if (pgn == 200 || pgn == 202) {
        Serial.printf("\r\n[Machine] Received broadcast PGN %d via handler 239", pgn);
        handleBroadcastPGN(pgn, data, len);
        return;
    }
    
    if (len < 8) return;
    
    instance->state.lastPGN232Time = millis();
    
    // Debug: Print all PGN 239 data
    static uint32_t lastPGN239Debug = 0;
    static uint8_t lastData[16] = {0};
    
    // Check if any data changed
    bool dataChanged = false;
    for (int i = 0; i < len && i < 16; i++) {
        if (data[i] != lastData[i]) {
            dataChanged = true;
            lastData[i] = data[i];
        }
    }
    
    // Print when data changes or every 2 seconds
    if (dataChanged || (millis() - lastPGN239Debug > 2000)) {
        Serial.printf("\r\n[Machine] PGN 239 len=%d:", len);
        // Print all bytes with labels
        if (len >= 8) {
            Serial.printf(" [0]uturn=0x%02X", data[0]);
            Serial.printf(" [1]speed=0x%02X", data[1]);
            Serial.printf(" [2]hydLift=0x%02X", data[2]);
            Serial.printf(" [3]tram=0x%02X", data[3]);
            Serial.printf(" [4]geoStop=0x%02X", data[4]);  // Per PGN.md
            Serial.printf(" [5]reserved=0x%02X", data[5]);  // Per PGN.md
            Serial.printf(" [6]SC1-8=0x%02X", data[6]);     // Section states 1-8
            Serial.printf(" [7]SC9-16=0x%02X", data[7]);    // Section states 9-16
        }
        
        // Show any extra bytes
        for (int i = 8; i < len && i < 16; i++) {
            Serial.printf(" [%d]=0x%02X", i, data[i]);
        }
        
        lastPGN239Debug = millis();
    }
    
    // Extract section states from bytes 6-7 (indices 6-7)
    uint16_t sectionStates = data[6] | (data[7] << 8);
    
    // Extract auto/manual states from bytes 4-5
    // NOTE: PGN.md says byte 4 is geoStop and byte 5 is reserved,
    // but the working code uses these for auto/manual states
    uint16_t autoStates = data[4] | (data[5] << 8);
    
    // Check if there might be auto states in later bytes (bytes 8-9 if they exist)
    if (len >= 10) {
        uint16_t possibleAutoStates = data[8] | (data[9] << 8);
        if (possibleAutoStates != 0) {
            Serial.printf("\r\n[Machine] Found non-zero data in bytes 8-9: 0x%04X", possibleAutoStates);
        }
    }
    
    // Only update if changed
    if (sectionStates != instance->sectionState.rawPGNData || autoStates != instance->sectionState.autoStates) {
        instance->sectionState.rawPGNData = sectionStates;
        instance->sectionState.autoStates = autoStates;
        instance->sectionState.lastUpdateTime = millis();
        
        // Decode section states
        // In AgOpenGPS:
        // - Section ON (Green/Yellow) = section bit is 1
        // - Section OFF (Red) = section bit is 0
        // - Auto mode = auto bit is 1
        // - Manual mode = auto bit is 0
        // NOTE: The bits are inverted from what the old comments said!
        for (int i = 0; i < 16; i++) {
            bool sectionBit = (sectionStates & (1 << i)) != 0;  // bit=1 means ON
            bool autoMode = (autoStates & (1 << i)) != 0;       // bit=1 means AUTO
            
            // Section is ON if the section bit is set, regardless of auto/manual mode
            // AgOpenGPS sends section state in the section bits, not auto bits
            instance->sectionState.isOn[i] = sectionBit;
        }
        
        Serial.printf("MachineProcessor: Sections=0x%04X Auto=0x%04X\n", sectionStates, autoStates);
        
        // Debug: Show which sections should be ON
        Serial.print("MachineProcessor: Section states:");
        for (int i = 0; i < 6; i++) {
            Serial.printf(" S%d=%s", i+1, instance->sectionState.isOn[i] ? "ON" : "OFF");
        }
        Serial.println();
        
        // Phase 4: Section output updates enabled
        instance->updateSectionOutputs();
    }
    
    // Extract hydraulic lift state
    uint8_t hydLift = data[2];
    bool newLowered = (hydLift != 0);
    if (newLowered != instance->state.isLowered) {
        instance->state.isLowered = newLowered;
        Serial.printf("MachineProcessor: Hydraulic %s\n", newLowered ? "lowered" : "raised");
    }
}

void MachineProcessor::printStatus() {
    Serial.print("Machine: Active=");
    Serial.print(isActive() ? "Yes" : "No");
    Serial.print(" Sections=0x");
    Serial.print(sectionState.rawPGNData, HEX);
    Serial.print(" Lowered=");
    Serial.println(state.isLowered ? "Yes" : "No");
}

void MachineProcessor::runSectionDiagnostics() {
    Serial.println("\n=== Section Diagnostics ===");
    
    // 1. Check PCA9685 communication
    if (!checkPCA9685()) {
        Serial.println("ERROR: PCA9685 not responding!");
        return;
    }
    
    // 2. Test ONLY section control pins (avoid motor driver pins)
    Serial.println("\nTesting ONLY section control pins...");
    Serial.println("Section pins: 0, 1, 4, 5, 10, 9");
    
    // First ensure all section pins are LOW (OFF)
    Serial.println("\nSetting all sections LOW (OFF)...");
    for (int i = 0; i < 6; i++) {
        getSectionOutputs().setPin(SECTION_PINS[i], 0, 0); // LOW = OFF
    }
    delay(1000);
    
    // Test sections one by one
    Serial.println("\nTesting each section individually (1 second each)...");
    for (int i = 0; i < 6; i++) {
        Serial.printf("\nSection %d (pin %d):\n", i+1, SECTION_PINS[i]);
        Serial.println("  Setting HIGH (LED should turn ON)...");
        getSectionOutputs().setPin(SECTION_PINS[i], 0, 1); // HIGH = ON
        delay(1000);
        
        Serial.println("  Setting LOW (LED should turn OFF)...");
        getSectionOutputs().setPin(SECTION_PINS[i], 0, 0); // LOW = OFF
        delay(500);
    }
    
    // Test all sections together
    Serial.println("\nTesting all sections together...");
    Serial.println("All sections HIGH (all LEDs ON):");
    for (int i = 0; i < 6; i++) {
        getSectionOutputs().setPin(SECTION_PINS[i], 0, 1); // HIGH = ON
    }
    delay(2000);
    
    Serial.println("All sections LOW (all LEDs OFF):");
    for (int i = 0; i < 6; i++) {
        getSectionOutputs().setPin(SECTION_PINS[i], 0, 0); // LOW = OFF
    }
    delay(1000);
    
    // 3. Current state summary
    Serial.println("\nCurrent section states:");
    for (int i = 0; i < 6; i++) {
        Serial.printf("  Section %d: %s\n", i+1, sectionState.isOn[i] ? "ON" : "OFF");
    }
    
    // 4. Pin summary
    Serial.println("\nPin configuration summary:");
    Serial.println("- Section signal pins: 0, 1, 4, 5, 10, 9");
    Serial.println("- DRVOFF pins: 2, 6, 8 (LOW = enabled)");
    Serial.println("- nSLEEP pins: 13, 3, 7 (sections only)");
    Serial.println("- Mode: Independent (solder jumpers open)");
    Serial.println("\nNOTE: Avoiding pins 11, 12, 14, 15 which may control motor drivers");
    
    // 5. Test control pins
    Serial.println("\nChecking DRVOFF states...");
    // Make sure all DRVOFF pins are LOW (enabled)
    for (uint8_t pin : DRVOFF_PINS) {
        Serial.printf("Setting DRVOFF pin %d LOW (enabled)\n", pin);
        getSectionOutputs().setPin(pin, 0, 0); // Ensure LOW
    }
    delay(100);
    
    Serial.println("\nTesting if DRVOFF disables sections...");
    // Turn all sections ON
    for (int i = 0; i < 6; i++) {
        getSectionOutputs().setPin(SECTION_PINS[i], 0, 0); // LOW = ON
    }
    delay(1000);
    
    // Toggle DRVOFF to see effect
    Serial.println("Setting DRVOFF HIGH (should disable all)...");
    for (uint8_t pin : DRVOFF_PINS) {
        getSectionOutputs().setPin(pin, 0, 1); // HIGH = disabled
    }
    delay(1000);
    
    Serial.println("Setting DRVOFF LOW (should re-enable all)...");
    for (uint8_t pin : DRVOFF_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // LOW = enabled
    }
    delay(1000);
    
    Serial.println("\n=== Diagnostics Complete ===");
}

void MachineProcessor::updateSectionOutputs() {
    // Debug: Show we're updating outputs
    static uint32_t lastUpdateDebug = 0;
    if (millis() - lastUpdateDebug > 1000) {
        Serial.println("\r\n[Machine] updateSectionOutputs called");
        lastUpdateDebug = millis();
    }
    
    // Update physical outputs for sections 1-6
    // Logic is inverted: HIGH turns LED ON, LOW turns LED OFF
    for (int i = 0; i < 6; i++) {
        if (sectionState.isOn[i]) {
            // Section ON: HIGH = LED ON
            getSectionOutputs().setPin(SECTION_PINS[i], 0, 1);
            // Serial.printf("Section %d: ON (LED lit)\n", i+1);
        } else {
            // Section OFF: LOW = LED OFF
            getSectionOutputs().setPin(SECTION_PINS[i], 0, 0);
            // Serial.printf("Section %d: OFF\n", i+1);
        }
    }
}

// Helper methods for clarity
void MachineProcessor::setPinHigh(uint8_t pin) {
    // For PCA9685: HIGH = no PWM, full ON
    getSectionOutputs().setPWM(pin, 4096, 0);
}

void MachineProcessor::setPinLow(uint8_t pin) {
    // For PCA9685: LOW = no PWM, full OFF
    getSectionOutputs().setPWM(pin, 0, 4096);
}

void MachineProcessor::setPinPWM(uint8_t pin, uint16_t dutyCycle) {
    // For future PWM control if needed
    getSectionOutputs().setPWM(pin, 0, dutyCycle);
}