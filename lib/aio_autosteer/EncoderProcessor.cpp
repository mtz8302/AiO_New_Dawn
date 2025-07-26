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
    
    // Debug logging every 2 seconds
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 2000) {
        lastDebugTime = millis();
        uint8_t pinA = hardwareManager.getKickoutAPin();
        uint8_t pinD = hardwareManager.getKickoutDPin();
        LOG_INFO(EventSource::AUTOSTEER, "Encoder debug - Type: %s, Raw: %ld, PinA=%d, PinD=%d",
                 encoderType == EncoderType::SINGLE ? "Single" : "Quad",
                 encoder->read(), digitalRead(pinA), digitalRead(pinD));
    }
    
    if (encoderType == EncoderType::SINGLE) {
        // Single channel encoder - count pulses
        pulseCount = encoder->read();
        
        // Log changes
        if (pulseCount != lastEncoderValue) {
            LOG_INFO(EventSource::AUTOSTEER, "Encoder pulse count: %d (delta=%d)", 
                     pulseCount, pulseCount - lastEncoderValue);
            lastEncoderValue = pulseCount;
        }
    } else {
        // Quadrature encoder - use absolute position
        int32_t rawCount = encoder->read();
        pulseCount = abs(rawCount);
        
        // Log changes
        if (pulseCount != lastEncoderValue) {
            LOG_INFO(EventSource::AUTOSTEER, "Encoder position: %d (raw=%d, delta=%d)", 
                     pulseCount, rawCount, pulseCount - lastEncoderValue);
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
    
    HardwareManager* hwMgr = HardwareManager::getInstance();
    
    // Request ownership of pins
    bool gotPinA = false;
    bool gotPinD = false;
    
    if (encoderType == EncoderType::QUADRATURE) {
        // For quadrature encoder, need both pins
        // First, request transfer from ADProcessor who normally owns KICKOUT_A
        if (hwMgr->getPinOwner(pinA) == HardwareManager::OWNER_ADPROCESSOR) {
            // Need to transfer ownership from ADProcessor
            gotPinA = hwMgr->transferPinOwnership(pinA, 
                HardwareManager::OWNER_ADPROCESSOR,
                HardwareManager::OWNER_ENCODERPROCESSOR,
                "EncoderProcessor");
        } else {
            gotPinA = hwMgr->requestPinOwnership(pinA, 
                HardwareManager::OWNER_ENCODERPROCESSOR, "EncoderProcessor");
        }
    }
    
    // Always need pin D
    gotPinD = hwMgr->requestPinOwnership(pinD, 
        HardwareManager::OWNER_ENCODERPROCESSOR, "EncoderProcessor");
    
    if ((encoderType == EncoderType::QUADRATURE && !gotPinA) || !gotPinD) {
        LOG_ERROR(EventSource::AUTOSTEER, "Failed to get ownership of encoder pins");
        return;
    }
    
    // Don't configure pins here - the Encoder library will set them to INPUT_PULLUP
    // Just update our tracking
    if (encoderType == EncoderType::QUADRATURE) {
        hwMgr->updatePinMode(pinA, INPUT_PULLUP);
        hwMgr->updatePinMode(pinD, INPUT_PULLUP);
    } else {
        hwMgr->updatePinMode(pinD, INPUT_PULLUP);
    }
    
    if (encoderType == EncoderType::SINGLE) {
        // Single channel encoder uses only digital pin
        encoder = new Encoder(pinD, pinD);  // Use same pin twice for single channel
        LOG_INFO(EventSource::AUTOSTEER, "Single channel encoder initialized on pin %d", pinD);
        // Allow time for pullups to stabilize
        delay(5);
        LOG_INFO(EventSource::AUTOSTEER, "Single encoder pin D=%d after init", digitalRead(pinD));
    } else {
        // Quadrature encoder uses both pins - match test sketch order
        encoder = new Encoder(pinA, pinD);  // A12 first, then pin 3
        LOG_INFO(EventSource::AUTOSTEER, "Quadrature encoder initialized on pins A=%d, D=%d", pinA, pinD);
        // Allow time for pullups to stabilize
        delay(5);
        LOG_INFO(EventSource::AUTOSTEER, "Quadrature encoder pins after init - A12=%d, D=%d", 
                 digitalRead(pinA), digitalRead(pinD));
    }
    
    // Reset count
    resetPulseCount();
}

void EncoderProcessor::deinitEncoder() {
    if (encoder) {
        // Get pin numbers before deleting encoder
        uint8_t pinA = hardwareManager.getKickoutAPin();
        uint8_t pinD = hardwareManager.getKickoutDPin();
        
        delete encoder;
        encoder = nullptr;
        pulseCount = 0;
        lastEncoderValue = 0;
        
        // Detach interrupts that the Encoder library may have attached
        detachInterrupt(digitalPinToInterrupt(pinA));
        detachInterrupt(digitalPinToInterrupt(pinD));
        
        // Reset pins to default state
        pinMode(pinA, INPUT_DISABLE);
        pinMode(pinD, INPUT_DISABLE);
        
        // Release pin ownership
        HardwareManager* hwMgr = HardwareManager::getInstance();
        if (encoderType == EncoderType::QUADRATURE) {
            hwMgr->releasePinOwnership(pinA, HardwareManager::OWNER_ENCODERPROCESSOR);
        }
        hwMgr->releasePinOwnership(pinD, HardwareManager::OWNER_ENCODERPROCESSOR);
        
        LOG_INFO(EventSource::AUTOSTEER, "Encoder deinitialized and pins released");
    }
}