// PWMMotorDriver.cpp - DRV8701 motor driver implementation with complementary PWM
#include "PWMMotorDriver.h"
#include <Arduino.h>
#include "EventLogger.h"
#include "HardwareManager.h"

PWMMotorDriver::PWMMotorDriver(MotorDriverType type, uint8_t pwm, uint8_t dir, 
                               uint8_t enable, uint8_t current) 
    : driverType(type), pwmPin(pwm), dirPin(dir), enablePin(enable), 
      currentPin(current), hasCurrentSense(false), currentScale(0.5f), currentOffset(0.0f) {
    
    // Initialize status
    status = {
        false,    // enabled
        0.0f,     // targetSpeed
        0.0f,     // actualSpeed  
        0.0f,     // currentDraw
        0,        // errorCount
        0,        // lastUpdateMs
        false,    // hasError
        {0}       // errorMessage (empty string)
    };
    
    // Check if current sensing is available
    if (currentPin != 255) {
        hasCurrentSense = true;
    }
}

bool PWMMotorDriver::init() {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing DRV8701 motor driver...");
    
    // Configure pins
    pinMode(pwmPin, OUTPUT);    // PWM1 (pin 5)
    pinMode(dirPin, OUTPUT);    // PWM2 (pin 6) - for complementary PWM
    
    if (enablePin != 255) {
        pinMode(enablePin, OUTPUT);
        // Send wake-up pulse for DRV8701 nSLEEP
        digitalWrite(enablePin, LOW);   // Pull low
        delay(1);                       // Hold for 1ms
        digitalWrite(enablePin, HIGH);  // Rising edge wakes the driver
        delayMicroseconds(100);         // Let it stabilize
        digitalWrite(enablePin, LOW);   // Return to sleep/LOW (like NG-V6)
        LOG_DEBUG(EventSource::AUTOSTEER, "Sent wake-up pulse on nSLEEP pin %d, now LOW", enablePin);
    }
    
    if (hasCurrentSense) {
        pinMode(currentPin, INPUT);
        LOG_DEBUG(EventSource::AUTOSTEER, "Current sensing enabled on pin A%d", currentPin - A0);
    }
    
    // Set initial state - both PWM outputs to LOW
    analogWrite(pwmPin, 0);
    analogWrite(dirPin, 0);
    
    // Configure PWM frequency for DRV8701 on both pins
    analogWriteFrequency(pwmPin, PWM_FREQUENCY);
    analogWriteFrequency(dirPin, PWM_FREQUENCY);
    
    LOG_INFO(EventSource::AUTOSTEER, "DRV8701 initialized with complementary PWM on pins %d (LEFT) and %d (RIGHT)", pwmPin, dirPin);
    
    return true;
}

void PWMMotorDriver::enable(bool en) {
    status.enabled = en;
    
    if (enablePin != 255) {
        // Pin 4 serves dual purpose: motor nSLEEP and LOCK output
        // For DRV8701: HIGH = awake/enabled, LOW = sleep/disabled
        // This also controls the LOCK output through the same MOSFET
        digitalWrite(enablePin, en ? HIGH : LOW);
        
        static bool lastState = false;
        if (en != lastState) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor driver %s (nSLEEP/LOCK pin %d = %s)", 
                     en ? "ENABLED" : "DISABLED", enablePin, en ? "HIGH" : "LOW");
            lastState = en;
        }
    }
    
    if (!en) {
        // Stop motor when disabling - set both outputs to LOW
        analogWrite(pwmPin, 0);
        analogWrite(dirPin, 0);
        
        status.targetSpeed = 0.0f;
        status.actualSpeed = 0.0f;
    }
    
}

void PWMMotorDriver::setSpeed(float speedPercent) {
    if (!status.enabled) {
        return;  // Ignore speed commands when disabled
    }
    
    // Constrain to valid range
    speedPercent = constrain(speedPercent, -100.0f, 100.0f);
    status.targetSpeed = speedPercent;
    
    // DRV8701 complementary PWM mode: PWM1 = LEFT, PWM2 = RIGHT
    // Inactive pin is set to 0, active pin gets PWM value (0-256)
    // Note: On Teensy, analogWrite(pin, 256) puts the pin in Hi-Z mode
    
    uint16_t pwmValue = (uint16_t)(abs(speedPercent) * 256.0f / 100.0f);
    pwmValue = constrain(pwmValue, 0, 256);
    
    if (speedPercent < 0) {
        // LEFT: PWM1 active, PWM2 low
        analogWrite(pwmPin, pwmValue);     // PWM1 active for LEFT (0-256)
        analogWrite(dirPin, 0);            // PWM2 LOW
    } else if (speedPercent > 0) {
        // RIGHT: PWM2 active, PWM1 low
        analogWrite(pwmPin, 0);            // PWM1 LOW
        analogWrite(dirPin, pwmValue);     // PWM2 active for RIGHT (0-256)
    } else {
        // Stop: both outputs LOW
        analogWrite(pwmPin, 0);
        analogWrite(dirPin, 0);
    }
    
    // Debug output
    static uint32_t lastDebug = 0;
    if (millis() - lastDebug > 1000) {
        lastDebug = millis();
        if (hasCurrentSense) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Speed: %.1f%% -> PWM1=%d, PWM2=%d, Current: %.2fA", 
                     speedPercent, 
                     speedPercent < 0 ? pwmValue : 0,
                     speedPercent > 0 ? pwmValue : 0,
                     getCurrent());
        } else {
            LOG_DEBUG(EventSource::AUTOSTEER, "Speed: %.1f%% -> PWM1=%d, PWM2=%d", 
                     speedPercent, 
                     speedPercent < 0 ? pwmValue : 0,
                     speedPercent > 0 ? pwmValue : 0);
        }
    }
    
    // For PWM motors, actual speed follows target immediately
    status.actualSpeed = speedPercent;
    status.lastUpdateMs = millis();
}

void PWMMotorDriver::stop() {
    // Set both outputs to LOW
    analogWrite(pwmPin, 0);
    analogWrite(dirPin, 0);
    
    status.targetSpeed = 0.0f;
    status.actualSpeed = 0.0f;
    status.lastUpdateMs = millis();
}

float PWMMotorDriver::getCurrent() const {
    if (!hasCurrentSense) return 0.0f;
    
    // Read ADC value (Teensy 4.1 has 12-bit ADC)
    int adcValue = analogRead(currentPin);
    
    // Convert to voltage (3.3V reference)
    float voltage = (adcValue * 3.3f) / 4095.0f;
    
    // Convert to current using scale and offset
    // For DRV8701: Typical current sense is 0.5V/A
    // Default scale = 0.5V/A, offset = 0V (no current = 0V output)
    float current = (voltage - currentOffset) / currentScale;
    
    // Ensure non-negative (current sensor should only report positive values)
    return max(0.0f, current);
}

void PWMMotorDriver::resetErrors() {
    status.errorCount = 0;
    status.hasError = false;
    status.errorMessage[0] = '\0';  // Clear error message
}

const char* PWMMotorDriver::getTypeName() const {
    switch (driverType) {
        case MotorDriverType::DRV8701:
            return "DRV8701 PWM Driver";
        case MotorDriverType::GENERIC_PWM:
            return "Generic PWM Driver";
        default:
            return "Unknown PWM Driver";
    }
}

void PWMMotorDriver::setCurrentScaling(float scale, float offset) {
    currentScale = scale;
    currentOffset = offset;
    LOG_INFO(EventSource::AUTOSTEER, "Current scaling set: scale=%.3f, offset=%.3f", scale, offset);
}

void PWMMotorDriver::setPWMFrequency(uint32_t freq) {
    analogWriteFrequency(pwmPin, freq);
    LOG_INFO(EventSource::AUTOSTEER, "PWM frequency set to %lu Hz", freq);
}

void PWMMotorDriver::handleKickout(KickoutType type, float value) {
    // Handle kickout based on type
    switch (type) {
        case KickoutType::WHEEL_ENCODER:
            LOG_WARNING(EventSource::AUTOSTEER, "PWM motor kickout: Wheel encoder count %d", (int)value);
            break;
        case KickoutType::PRESSURE_SENSOR:
            LOG_WARNING(EventSource::AUTOSTEER, "PWM motor kickout: Pressure %.1f", value);
            break;
        case KickoutType::CURRENT_SENSOR:
            LOG_WARNING(EventSource::AUTOSTEER, "PWM motor kickout: Current %.2fA", value);
            break;
        default:
            break;
    }
    
    // Disable the motor
    enable(false);
    stop();
}