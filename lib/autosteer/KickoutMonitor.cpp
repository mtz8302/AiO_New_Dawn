#include "KickoutMonitor.h"
#include "ConfigManager.h"
#include "ADProcessor.h"
#include "pcb.h"

// External global pointers
extern ConfigManager* configPTR;
extern ADProcessor* adPTR;

// Singleton instance
KickoutMonitor* KickoutMonitor::instance = nullptr;

// No ISR needed - we'll poll the encoder pin

KickoutMonitor::KickoutMonitor() : 
    configMgr(nullptr),
    adProcessor(nullptr),
    encoderPulseCount(0),
    lastPulseCheck(0),
    lastPulseCount(0),
    lastEncoderState(false),
    lastPressureReading(0),
    lastCurrentReading(0),
    kickoutActive(false),
    kickoutReason(NONE),
    kickoutTime(0) {
}

KickoutMonitor::~KickoutMonitor() {
    instance = nullptr;
}

KickoutMonitor* KickoutMonitor::getInstance() {
    if (instance == nullptr) {
        instance = new KickoutMonitor();
    }
    return instance;
}

bool KickoutMonitor::init() {
    Serial.print("\r\n- Initializing KickoutMonitor");
    
    // Get external dependencies
    configMgr = configPTR;
    adProcessor = adPTR;
    
    if (!configMgr || !adProcessor) {
        Serial.print(" - ERROR: Missing dependencies");
        return false;
    }
    
    // Initialize encoder pin if enabled
    if (configMgr->getShaftEncoder()) {
        pinMode(KICKOUT_D_PIN, INPUT_PULLUP);
        lastEncoderState = digitalRead(KICKOUT_D_PIN);
        Serial.print("\r\n  - Encoder input configured on pin ");
        Serial.print(KICKOUT_D_PIN);
        Serial.print(" (polling mode)");
    }
    
    Serial.print(" - SUCCESS");
    return true;
}

void KickoutMonitor::process() {
    // Poll encoder pin for pulses if enabled
    if (configMgr->getShaftEncoder()) {
        bool currentState = digitalRead(KICKOUT_D_PIN);
        // Detect rising edge
        if (currentState && !lastEncoderState) {
            encoderPulseCount++;
        }
        lastEncoderState = currentState;
    }
    
    // Only check for kickouts if not already active
    if (!kickoutActive) {
        // Check each kickout condition based on configuration
        if (configMgr->getShaftEncoder() && checkEncoderKickout()) {
            kickoutActive = true;
            kickoutReason = ENCODER_OVERSPEED;
            kickoutTime = millis();
        }
        else if (configMgr->getPressureSensor() && checkPressureKickout()) {
            kickoutActive = true;
            kickoutReason = PRESSURE_HIGH;
            kickoutTime = millis();
        }
        else if (configMgr->getCurrentSensor() && checkCurrentKickout()) {
            kickoutActive = true;
            kickoutReason = CURRENT_HIGH;
            kickoutTime = millis();
        }
    }
}

bool KickoutMonitor::checkEncoderKickout() {
    // Check encoder pulses every 100ms
    uint32_t now = millis();
    if (now - lastPulseCheck < 100) {
        return false;
    }
    
    // Calculate pulses in the last period
    uint32_t currentCount = encoderPulseCount;
    uint32_t pulsesSinceLast = currentCount - lastPulseCount;
    
    lastPulseCheck = now;
    lastPulseCount = currentCount;
    
    // Get max pulse count from config
    uint16_t maxPulses = configMgr->getPulseCountMax();
    
    // Check if we exceeded the threshold
    if (pulsesSinceLast > maxPulses) {
        Serial.printf("\r\n[KICKOUT] Encoder overspeed: %lu pulses (max %u)", 
                      pulsesSinceLast, maxPulses);
        return true;
    }
    
    return false;
}

bool KickoutMonitor::checkPressureKickout() {
    // Read pressure sensor on KICKOUT_A pin
    lastPressureReading = adProcessor->getKickoutAnalog();
    
    // TODO: Get pressure threshold from config (not yet implemented)
    // For now, use a reasonable default
    const uint16_t PRESSURE_THRESHOLD = 800;  // ADC value
    
    if (lastPressureReading > PRESSURE_THRESHOLD) {
        Serial.printf("\r\n[KICKOUT] Pressure high: %u (threshold %u)", 
                      lastPressureReading, PRESSURE_THRESHOLD);
        return true;
    }
    
    return false;
}

bool KickoutMonitor::checkCurrentKickout() {
    // Read current sensor
    lastCurrentReading = adProcessor->getMotorCurrent();
    
    // TODO: Get current threshold from config (not yet implemented)
    // For now, use a reasonable default
    const uint16_t CURRENT_THRESHOLD = 900;  // ADC value
    
    if (lastCurrentReading > CURRENT_THRESHOLD) {
        Serial.printf("\r\n[KICKOUT] Current high: %u (threshold %u)", 
                      lastCurrentReading, CURRENT_THRESHOLD);
        return true;
    }
    
    return false;
}

void KickoutMonitor::clearKickout() {
    if (kickoutActive) {
        Serial.printf("\r\n[KICKOUT] Cleared after %lu ms", 
                      millis() - kickoutTime);
    }
    kickoutActive = false;
    kickoutReason = NONE;
    kickoutTime = 0;
    
    // Reset encoder count
    encoderPulseCount = 0;
    lastPulseCount = 0;
}


const char* KickoutMonitor::getReasonString() const {
    switch (kickoutReason) {
        case NONE: return "None";
        case ENCODER_OVERSPEED: return "Encoder Overspeed";
        case PRESSURE_HIGH: return "Pressure High";
        case CURRENT_HIGH: return "Current High";
        case MOTOR_SLIP: return "Motor Slip";
        default: return "Unknown";
    }
}