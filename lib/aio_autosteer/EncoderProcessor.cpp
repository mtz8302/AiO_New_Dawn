#include "EncoderProcessor.h"
#include "HardwareManager.h"
#include "ConfigManager.h"
#include "EventLogger.h"

// External objects
extern HardwareManager hardwareManager;
extern ConfigManager configManager;

// Static instance
EncoderProcessor* EncoderProcessor::instance = nullptr;

// Global pointer
EncoderProcessor* encoderProcessor = nullptr;

EncoderProcessor* EncoderProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new EncoderProcessor();
        encoderProcessor = instance;
    }
    return instance;
}

bool EncoderProcessor::init() {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing Encoder Processor");
    
    // Load configuration from EEPROM
    encoderType = (EncoderType)configManager.getEncoderType();
    encoderEnabled = configManager.getShaftEncoder();
    
    // Initialize encoder if enabled
    if (encoderEnabled) {
        initEncoder();
        LOG_INFO(EventSource::AUTOSTEER, "Encoder enabled - Type: %s", 
                 encoderType == EncoderType::SINGLE ? "Single" : "Quadrature");
    } else {
        LOG_INFO(EventSource::AUTOSTEER, "Encoder disabled");
    }
    
    return true;
}

void EncoderProcessor::updateConfig(EncoderType type, bool enabled) {
    // Check if configuration changed
    bool typeChanged = (encoderType != type);
    bool enableChanged = (encoderEnabled != enabled);
    
    // Update configuration
    encoderType = type;
    encoderEnabled = enabled;
    
    // Reinitialize if needed
    if (typeChanged || enableChanged) {
        deinitEncoder();
        if (encoderEnabled) {
            initEncoder();
        }
        LOG_INFO(EventSource::AUTOSTEER, "Encoder reconfigured - Enabled: %s, Type: %s", 
                 enabled ? "Yes" : "No",
                 type == EncoderType::SINGLE ? "Single" : "Quadrature");
    }
    
    // Save to EEPROM
    configManager.setShaftEncoder(enabled);
    configManager.setEncoderType((uint8_t)type);
    configManager.saveSteerConfig();
}

void EncoderProcessor::process() {
    if (!encoderEnabled || !encoder) {
        return;
    }
    
    if (encoderType == EncoderType::SINGLE) {
        // Single channel encoder - count pulses
        pulseCount = encoder->readCount();
        
        // Log changes
        if (pulseCount != lastEncoderValue) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Encoder pulse count: %d", pulseCount);
            lastEncoderValue = pulseCount;
        }
    } else {
        // Quadrature encoder - use absolute position
        pulseCount = abs(encoder->readPosition());
        
        // Log changes
        if (pulseCount != lastEncoderValue) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Encoder position: %d", pulseCount);
            lastEncoderValue = pulseCount;
        }
    }
}


void EncoderProcessor::resetPulseCount() {
    if (encoder) {
        encoder->write(0);
        pulseCount = 0;
        lastEncoderValue = 0;
        LOG_DEBUG(EventSource::AUTOSTEER, "Encoder pulse count reset");
    }
}

void EncoderProcessor::initEncoder() {
    uint8_t pinA = hardwareManager.getKickoutAPin();
    uint8_t pinD = hardwareManager.getKickoutDPin();
    
    if (encoderType == EncoderType::SINGLE) {
        // Single channel encoder uses only digital pin
        encoder = new Encoder(pinD, pinD);  // Use same pin twice for single channel
        LOG_INFO(EventSource::AUTOSTEER, "Single channel encoder initialized on pin %d", pinD);
    } else {
        // Quadrature encoder uses both pins
        encoder = new Encoder(pinD, pinA);
        LOG_INFO(EventSource::AUTOSTEER, "Quadrature encoder initialized on pins %d, %d", pinD, pinA);
    }
    
    // Reset count
    resetPulseCount();
}

void EncoderProcessor::deinitEncoder() {
    if (encoder) {
        delete encoder;
        encoder = nullptr;
        pulseCount = 0;
        lastEncoderValue = 0;
        LOG_INFO(EventSource::AUTOSTEER, "Encoder deinitialized");
    }
}