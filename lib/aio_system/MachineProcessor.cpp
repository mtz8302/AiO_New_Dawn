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
    memset(&sectionState, 0, sizeof(sectionState));
    sectionState.lastPGN239Time = 0;
    
    // Initialize hardware
    if (!initializeSectionOutputs()) {
        LOG_ERROR(EventSource::MACHINE, "Failed to initialize section outputs!");
        return false;
    }
    
    // Register PGN handlers
    LOG_DEBUG(EventSource::MACHINE, "Registering PGN callbacks...");
    // Register for broadcast PGNs (200, 202)
    bool regBroadcast = PGNProcessor::instance->registerBroadcastCallback(handleBroadcastPGN, "Machine");
    bool reg239 = PGNProcessor::instance->registerCallback(MACHINE_PGN_DATA, handlePGN239, "Machine");
    LOG_DEBUG(EventSource::MACHINE, "PGN registrations - Broadcast:%d, 239:%d", regBroadcast, reg239);
    
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
        // Link just went down - turn off all sections immediately
        if (sectionState.currentStates != 0) {
            LOG_INFO(EventSource::MACHINE, "Sections turned off - ethernet link down");
            sectionState.currentStates = 0;
            memset(sectionState.isOn, 0, sizeof(sectionState.isOn));
            updateSectionOutputs();
            
            // Clear the timer so sections stay off
            sectionState.lastPGN239Time = 0;
        }
    }
    previousLinkState = currentLinkState;
    
    // Watchdog timer - turn off all sections if no PGN 239 for 2 seconds
    if (sectionState.lastPGN239Time > 0 && 
        (millis() - sectionState.lastPGN239Time) > 2000) {
        
        // Turn off all sections
        if (sectionState.currentStates != 0) {
            LOG_INFO(EventSource::MACHINE, "Sections turned off - watchdog timeout");
            sectionState.currentStates = 0;
            memset(sectionState.isOn, 0, sizeof(sectionState.isOn));
            updateSectionOutputs();
        }
        
        // Reset timer to prevent repeated messages
        sectionState.lastPGN239Time = 0;
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
    
    // Need at least 8 bytes to have section data in bytes 6 & 7
    if (len < 8) return;
    
    // Update watchdog timer
    instance->sectionState.lastPGN239Time = millis();
    
    // Extract section states from bytes 6 & 7
    uint16_t sectionStates = data[6] | (data[7] << 8);
    
    // Only update if changed
    if (sectionStates != instance->sectionState.currentStates) {
        instance->sectionState.currentStates = sectionStates;
        
        // Decode section states - simple on/off from bytes 6 & 7
        // Bit 1 = ON, Bit 0 = OFF
        for (int i = 0; i < 16; i++) {
            instance->sectionState.isOn[i] = (sectionStates & (1 << i)) != 0;
        }
        
        // Log section changes with binary format
        char bin1[16], bin2[16];  // 8 bits + 7 spaces + null terminator
        byteToBinary(data[6], bin1);
        byteToBinary(data[7], bin2);
        LOG_INFO(EventSource::MACHINE, "Sections changed: [6]SC1-8=0x%02X (0b%s) [7]SC9-16=0x%02X (0b%s)", 
                 data[6], bin1, data[7], bin2);
        
        // Show which sections are ON (only first 6 sections we control)
        char sectionMsg[100];
        snprintf(sectionMsg, sizeof(sectionMsg), "Section states:");
        for (int i = 0; i < 6; i++) {
            char buf[20];
            snprintf(buf, sizeof(buf), " S%d=%s", i+1, instance->sectionState.isOn[i] ? "ON" : "OFF");
            strncat(sectionMsg, buf, sizeof(sectionMsg) - strlen(sectionMsg) - 1);
        }
        LOG_INFO(EventSource::MACHINE, "%s", sectionMsg);
        
        // Phase 4: Section output updates enabled
        instance->updateSectionOutputs();
    }
    
}



void MachineProcessor::updateSectionOutputs() {
    // Removed periodic debug logging - this is called frequently from PGN 239
    
    // Update physical outputs for sections 1-6
    // Logic is inverted: HIGH turns LED ON, LOW turns LED OFF
    for (int i = 0; i < 6; i++) {
        if (sectionState.isOn[i]) {
            // Section ON: HIGH = LED ON
            getSectionOutputs().setPin(SECTION_PINS[i], 0, 1);
        } else {
            // Section OFF: LOW = LED OFF
            getSectionOutputs().setPin(SECTION_PINS[i], 0, 0);
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

