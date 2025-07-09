#include "MachineProcessor.h"
#include <Arduino.h>
#include "PGNProcessor.h"
#include "PGNUtils.h"
#include <Wire.h>
#include "Adafruit_PWMServoDriver.h"
#include "EventLogger.h"
#include "QNetworkBase.h"

extern void sendUDPbytes(uint8_t *message, int msgLen);

// External network configuration - defined elsewhere
extern struct NetworkConfig netConfig;

// PCA9685 PWM driver for section outputs - moved to static instance
// Note: Front panel LEDs use 0x70 on Wire, sections use 0x44

// Hardware pin mappings from schematic
// Section signal pins on PCA9685 (control the actual sections)
const uint8_t SECTION_PINS[6] = {0, 1, 4, 5, 10, 9};  // SEC1_SIG through SEC6_SIG

// DRV8243 control pins on PCA9685
const uint8_t DRVOFF_PINS[3] = {2, 6, 8};    // DRVOFF pins (must be LOW to enable)

// DRV8243 sleep pins - these need a reset pulse to activate
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
    LOG_DEBUG(EventSource::MACHINE, "Constructor called");
}

MachineProcessor* MachineProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new MachineProcessor();
        machinePTR = instance;
    }
    return instance;
}

bool MachineProcessor::init() {
    LOG_INFO(EventSource::MACHINE, "Initializing (Phase 4 - Full functionality)");
    getInstance();
    return instance->initialize();
}

bool MachineProcessor::initialize() {
    LOG_INFO(EventSource::MACHINE, "Initializing...");
    
    // Clear initial state
    memset(&machineState, 0, sizeof(machineState));
    memset(&machineConfig, 0, sizeof(machineConfig));
    memset(&pinConfig, 0, sizeof(pinConfig));
    
    // Set default pin assignments (pins 1-6 = sections 1-6)
    for (int i = 1; i <= 6; i++) {
        pinConfig.pinFunction[i] = i;  // Default: pin N controls section N
    }
    
    machineState.lastPGN239Time = 0;
    
    // Initialize hardware
    if (!initializeSectionOutputs()) {
        LOG_ERROR(EventSource::MACHINE, "Failed to initialize section outputs!");
        return false;
    }
    
    // Register PGN handlers
    LOG_DEBUG(EventSource::MACHINE, "Registering PGN callbacks...");
    // Register for broadcast PGNs (200, 202)
    bool regBroadcast = PGNProcessor::instance->registerBroadcastCallback(handleBroadcastPGN, "Machine");
    bool reg236 = PGNProcessor::instance->registerCallback(MACHINE_PGN_PIN_CONFIG, handlePGN236, "Machine-PinConfig");
    bool reg238 = PGNProcessor::instance->registerCallback(MACHINE_PGN_CONFIG, handlePGN238, "Machine-Config");
    bool reg239 = PGNProcessor::instance->registerCallback(MACHINE_PGN_DATA, handlePGN239, "Machine-Data");
    LOG_INFO(EventSource::MACHINE, "PGN registrations - Broadcast:%d, 236:%d, 238:%d, 239:%d", 
             regBroadcast, reg236, reg238, reg239);
    
    LOG_INFO(EventSource::MACHINE, "Initialized successfully");
    return true;
}

bool MachineProcessor::initializeSectionOutputs() {
    LOG_DEBUG(EventSource::MACHINE, "Initializing section outputs...");
    
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
    
    // 5. Put all DRV8243s to sleep initially
    LOG_DEBUG(EventSource::MACHINE, "Putting all DRV8243 drivers to sleep");
    for (uint8_t pin : SLEEP_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW for sleep mode
    }
    delayMicroseconds(150); // Wait for sleep mode to settle
    
    // 6. Set all section outputs to OFF state before waking drivers
    LOG_DEBUG(EventSource::MACHINE, "Setting section outputs LOW (OFF state)");
    for (uint8_t pin : SECTION_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW = OFF
    }
    
    // 7. Wake up the section DRV8243s with reset pulse
    LOG_DEBUG(EventSource::MACHINE, "Waking section DRV8243 drivers");
    getSectionOutputs().setPin(13, 187, 1); // Section 1/2 - 30µs LOW pulse
    getSectionOutputs().setPin(3, 187, 1);  // Section 3/4 - 30µs LOW pulse
    getSectionOutputs().setPin(7, 187, 1);  // Section 5/6 - 30µs LOW pulse
    
    // 9. Enable DRV8243 outputs by setting DRVOFF LOW
    LOG_DEBUG(EventSource::MACHINE, "Enabling DRV8243 outputs (DRVOFF = LOW)");
    for (uint8_t pin : DRVOFF_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW to enable outputs
    }
    
    LOG_INFO(EventSource::MACHINE, "Section outputs initialized");
    return true;
}

bool MachineProcessor::checkPCA9685() {
    Wire.beginTransmission(0x44);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        LOG_DEBUG(EventSource::MACHINE, "Found PCA9685 at 0x44");
        return true;
    }
    LOG_ERROR(EventSource::MACHINE, "PCA9685 not found at 0x44!");
    return false;
}

void MachineProcessor::process() {
    // Check ethernet link state
    static bool previousLinkState = true;
    bool currentLinkState = QNetworkBase::isConnected();
    
    if (previousLinkState && !currentLinkState) {
        // Link just went down - turn off all functions immediately
        LOG_INFO(EventSource::MACHINE, "All outputs turned off - ethernet link down");
        memset(machineState.functions, 0, sizeof(machineState.functions));
        machineState.sectionStates = 0;
        updateMachineOutputs();
        
        // Clear the timer
        machineState.lastPGN239Time = 0;
    }
    previousLinkState = currentLinkState;
    
    // Watchdog timer - turn off all outputs if no PGN 239 for 2 seconds
    if (machineState.lastPGN239Time > 0 && 
        (millis() - machineState.lastPGN239Time) > 2000) {
        
        LOG_INFO(EventSource::MACHINE, "All outputs turned off - watchdog timeout");
        memset(machineState.functions, 0, sizeof(machineState.functions));
        machineState.sectionStates = 0;
        updateMachineOutputs();
        
        // Reset timer to prevent repeated messages
        machineState.lastPGN239Time = 0;
    }
}

void MachineProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) {
        LOG_ERROR(EventSource::MACHINE, "No instance for broadcast PGN!");
        return;
    }
    
    if (pgn == 200) {
        
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
        
    }
    else if (pgn == 202) {
        
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
        
    }
}


// Helper function to convert byte to binary string with spaces
static void byteToBinary(uint8_t byte, char* buffer) {
    int pos = 0;
    for (int i = 7; i >= 0; i--) {
        buffer[pos++] = (byte & (1 << i)) ? '1' : '0';
        if (i > 0) buffer[pos++] = ' ';  // Add space between bits
    }
    buffer[pos] = '\0';
}

// Helper function to convert 16-bit value to binary string with spaces
static void uint16ToBinary(uint16_t value, char* buffer) {
    int pos = 0;
    for (int i = 15; i >= 0; i--) {
        buffer[pos++] = (value & (1 << i)) ? '1' : '0';
        if (i > 0) buffer[pos++] = ' ';  // Add space between bits
    }
    buffer[pos] = '\0';
}

void MachineProcessor::handlePGN239(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // First check if this is a broadcast PGN
    if (pgn == 200 || pgn == 202) {
        handleBroadcastPGN(pgn, data, len);
        return;
    }
    
    // Need at least 12 bytes for all machine data
    // PGN 239 format: uturn(5), speed(6), hydLift(7), tram(8), geo(9), reserved(10), SC1-8(11), SC9-16(12)
    if (len < 8) return;
    
    // Update watchdog timer
    instance->machineState.lastPGN239Time = millis();
    
    // For Phase 1, just log the data we're receiving
    // Log machine control data for Phase 1
    if (len >= 5) {
        uint8_t uturn = data[0];      // Byte 5 - not used yet
        uint8_t speed = data[1];      // Byte 6 - speed * 10
        
        if (len >= 5) {  // Have hydraulic, tram, geo data
            uint8_t hydLift = data[2];   // Byte 7: 0=off, 1=down, 2=up
            uint8_t tram = data[3];      // Byte 8: bit0=right, bit1=left
            uint8_t geoStop = data[4];   // Byte 9: 0=inside, 1=outside
            
            LOG_DEBUG(EventSource::MACHINE, "PGN 239 Machine Data: speed=%.1f km/h, hyd=%d, tram=0x%02X, geo=%d", 
                      speed / 10.0f, hydLift, tram, geoStop);
            
            // Store machine control data
            instance->machineState.hydLift = hydLift;
            instance->machineState.tramline = tram;
            instance->machineState.geoStop = geoStop;
        }
    }
    
    if (len >= 8) {
        // Extract section states from bytes 11 & 12 (array indices 6 & 7)
        uint16_t sectionStates = data[6] | (data[7] << 8);
        
        // Track if any states changed
        bool statesChanged = false;
        
        // Check if section states changed
        if (sectionStates != instance->machineState.sectionStates) {
            instance->machineState.sectionStates = sectionStates;
            statesChanged = true;
        }
        
        // Update all function states
        instance->updateFunctionStates();
        
        // Check if any function changed
        if (instance->machineState.functionsChanged) {
            statesChanged = true;
            instance->machineState.functionsChanged = false;  // Reset flag
        }
        
        // Only log and update outputs if something changed
        if (statesChanged) {
            // Show active functions for our 6 outputs
            char activeMsg[256];
            snprintf(activeMsg, sizeof(activeMsg), "Active functions:");
            
            // Check what function each output is assigned to
            for (int pin = 1; pin <= 6; pin++) {
                uint8_t func = instance->pinConfig.pinFunction[pin];
                if (func > 0 && func <= MAX_FUNCTIONS) {
                    if (instance->machineState.functions[func]) {
                        char buf[50];
                        snprintf(buf, sizeof(buf), " Out%d=%s", pin, instance->getFunctionName(func));
                        strncat(activeMsg, buf, sizeof(activeMsg) - strlen(activeMsg) - 1);
                    }
                }
            }
            
            LOG_INFO(EventSource::MACHINE, "%s", activeMsg);
            
            // Log raw section states for debugging
            char bin1[16], bin2[16];
            byteToBinary(data[6], bin1);
            byteToBinary(data[7], bin2);
            LOG_DEBUG(EventSource::MACHINE, "Sections: SC1-8=0b%s, SC9-16=0b%s", bin1, bin2);
            
            // Update outputs
            instance->updateSectionOutputs();
        }
    }
    
}



void MachineProcessor::updateSectionOutputs() {
    // For Phase 1, maintain backward compatibility
    // This will be replaced by updateMachineOutputs in Phase 3
    
    // Update physical outputs for sections 1-6
    // Logic is inverted: HIGH turns LED ON, LOW turns LED OFF
    for (int i = 0; i < 6; i++) {
        if (machineState.functions[i + 1]) {  // Functions 1-6 are sections 1-6
            // Section ON: HIGH = LED ON
            getSectionOutputs().setPin(SECTION_PINS[i], 0, 1);
        } else {
            // Section OFF: LOW = LED OFF
            getSectionOutputs().setPin(SECTION_PINS[i], 0, 0);
        }
    }
}
void MachineProcessor::handlePGN236(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // PGN 236 - Machine Pin Config
    // Expected length: 30 bytes (5 header + 24 pin configs + 1 reserved)
    if (len < 24) {
        LOG_ERROR(EventSource::MACHINE, "PGN 236 too short: %d bytes", len);
        return;
    }
    
    LOG_INFO(EventSource::MACHINE, "PGN 236 - Machine Pin Config received");
    
    // Parse pin function assignments (bytes 0-23 map to pins 1-24)
    for (int i = 0; i < 24 && i < len; i++) {
        uint8_t function = data[i];
        
        // Validate function number (0=unassigned, 1-21=valid functions)
        if (function > MAX_FUNCTIONS) {
            LOG_WARNING(EventSource::MACHINE, "Pin %d: Invalid function %d (max %d)", 
                        i + 1, function, MAX_FUNCTIONS);
            function = 0;  // Set to unassigned
        }
        
        instance->pinConfig.pinFunction[i + 1] = function;
        
        // Log assignments for first 6 pins (our physical outputs)
        if (i < 6) {
            LOG_INFO(EventSource::MACHINE, "Output %d assigned to %s", 
                     i + 1, instance->getFunctionName(function));
        }
    }
    
    instance->pinConfig.configReceived = true;
    
    // TODO: Save to EEPROM in Phase 4
}

void MachineProcessor::handlePGN238(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // PGN 238 - Machine Config
    // Expected length: 14 bytes (5 header + 8 config + 1 reserved)
    if (len < 8) {
        LOG_ERROR(EventSource::MACHINE, "PGN 238 too short: %d bytes", len);
        return;
    }
    
    LOG_INFO(EventSource::MACHINE, "PGN 238 - Machine Config received");
    
    // Parse configuration
    instance->machineConfig.raiseTime = data[0];        // Byte 5
    instance->machineConfig.lowerTime = data[1];        // Byte 6
    instance->machineConfig.hydEnable = data[2];        // Byte 7
    instance->machineConfig.isPinActiveHigh = data[3];  // Byte 8
    instance->machineConfig.user1 = data[4];            // Byte 9
    instance->machineConfig.user2 = data[5];            // Byte 10
    instance->machineConfig.user3 = data[6];            // Byte 11
    instance->machineConfig.user4 = data[7];            // Byte 12
    
    instance->machineConfig.configReceived = true;
    
    LOG_INFO(EventSource::MACHINE, "Machine Config: RaiseTime=%ds, LowerTime=%ds, HydEnable=%d, ActiveHigh=%d",
             instance->machineConfig.raiseTime,
             instance->machineConfig.lowerTime,
             instance->machineConfig.hydEnable,
             instance->machineConfig.isPinActiveHigh);
    
    LOG_DEBUG(EventSource::MACHINE, "User values: U1=%d, U2=%d, U3=%d, U4=%d",
              instance->machineConfig.user1,
              instance->machineConfig.user2,
              instance->machineConfig.user3,
              instance->machineConfig.user4);
    
    // TODO: Save to EEPROM in Phase 4
}

const char* MachineProcessor::getFunctionName(uint8_t functionNum) {
    static const char* functionNames[] = {
        "Unassigned",     // 0
        "Section 1",      // 1
        "Section 2",      // 2
        "Section 3",      // 3
        "Section 4",      // 4
        "Section 5",      // 5
        "Section 6",      // 6
        "Section 7",      // 7
        "Section 8",      // 8
        "Section 9",      // 9
        "Section 10",     // 10
        "Section 11",     // 11
        "Section 12",     // 12
        "Section 13",     // 13
        "Section 14",     // 14
        "Section 15",     // 15
        "Section 16",     // 16
        "Hyd Up",         // 17
        "Hyd Down",       // 18
        "Tramline Right", // 19
        "Tramline Left",  // 20
        "Geo Stop"        // 21
    };
    
    if (functionNum <= MAX_FUNCTIONS) {
        return functionNames[functionNum];
    }
    return "Invalid";
}

void MachineProcessor::updateFunctionStates() {
    // Save previous states to detect changes
    bool previousStates[MAX_FUNCTIONS + 1];
    memcpy(previousStates, machineState.functions, sizeof(previousStates));
    
    // Clear all function states first
    memset(machineState.functions, 0, sizeof(machineState.functions));
    
    // Map section states to functions 1-16
    for (int i = 0; i < 16; i++) {
        machineState.functions[i + 1] = (machineState.sectionStates & (1 << i)) != 0;
    }
    
    // Map hydraulic states to functions 17-18
    // hydLift: 0=off, 1=down, 2=up
    if (machineState.hydLift == 2) {
        machineState.functions[17] = true;  // Hyd Up
        machineState.functions[18] = false; // Hyd Down
    } else if (machineState.hydLift == 1) {
        machineState.functions[17] = false; // Hyd Up
        machineState.functions[18] = true;  // Hyd Down
    } else {
        machineState.functions[17] = false; // Hyd Up
        machineState.functions[18] = false; // Hyd Down
    }
    
    // Map tramline bits to functions 19-20
    // tramline: bit0=right, bit1=left
    machineState.functions[19] = (machineState.tramline & 0x01) != 0; // Tram Right
    machineState.functions[20] = (machineState.tramline & 0x02) != 0; // Tram Left
    
    // Map geo stop to function 21
    // geoStop: 0=inside boundary, 1=outside boundary
    machineState.functions[21] = (machineState.geoStop != 0);
    
    // Check if any function state changed
    machineState.functionsChanged = false;
    for (int i = 1; i <= MAX_FUNCTIONS; i++) {
        if (machineState.functions[i] != previousStates[i]) {
            machineState.functionsChanged = true;
            
            // Log state changes for debugging
            LOG_DEBUG(EventSource::MACHINE, "Function %d (%s) changed to %s", 
                      i, getFunctionName(i), 
                      machineState.functions[i] ? "ON" : "OFF");
        }
    }
}

void MachineProcessor::updateMachineOutputs() {
    // Placeholder - will be implemented in Phase 3
    // For now, just call the existing section outputs
    updateSectionOutputs();
    
    // Phase 2 debug: Log non-section functions if active
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 1000) {  // Limit to once per second
        bool hasActive = false;
        for (int i = 17; i <= 21; i++) {
            if (machineState.functions[i]) {
                if (!hasActive) {
                    LOG_DEBUG(EventSource::MACHINE, "Non-section functions active:");
                    hasActive = true;
                }
                LOG_DEBUG(EventSource::MACHINE, "  Function %d: %s", i, getFunctionName(i));
            }
        }
        if (hasActive) {
            lastDebugTime = millis();
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

