// PWMMotorDriver.cpp - PWM motor driver implementation for DRV8701
#include "PWMMotorDriver.h"
#include <Arduino.h>

PWMMotorDriver::PWMMotorDriver(MotorDriverType type, uint8_t pwm, uint8_t dir, 
                               uint8_t enable, uint8_t current) 
    : driverType(type), pwmPin(pwm), dirPin(dir), enablePin(enable), 
      currentPin(current), hasCurrentSense(false), currentScale(1.0f), currentOffset(0.0f) {
    
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
    Serial.print("\r\n[PWMMotor] Initializing DRV8701 motor driver...");
    
    // Configure pins
    pinMode(pwmPin, OUTPUT);
    pinMode(dirPin, OUTPUT);
    
    if (enablePin != 255) {
        pinMode(enablePin, OUTPUT);
        digitalWrite(enablePin, LOW);  // Start disabled
    }
    
    if (hasCurrentSense) {
        pinMode(currentPin, INPUT);
        Serial.print("\r\n  - Current sensing enabled on pin A");
        Serial.print(currentPin - A0);
    }
    
    // Set initial state
    analogWrite(pwmPin, 0);
    digitalWrite(dirPin, LOW);
    
    // Configure PWM frequency for DRV8701
    // Teensy 4.1 default is fine for DRV8701
    analogWriteFrequency(pwmPin, PWM_FREQUENCY);
    
    Serial.print("\r\n  - PWM on pin ");
    Serial.print(pwmPin);
    Serial.print(", DIR on pin ");
    Serial.print(dirPin);
    if (enablePin != 255) {
        Serial.print(", EN on pin ");
        Serial.print(enablePin);
    }
    
    Serial.print(" SUCCESS");
    return true;
}

void PWMMotorDriver::enable(bool en) {
    status.enabled = en;
    
    if (enablePin != 255) {
        digitalWrite(enablePin, en ? HIGH : LOW);
    }
    
    if (!en) {
        // Stop motor when disabling
        analogWrite(pwmPin, 0);
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
    
    // Set direction based on sign
    if (speedPercent >= 0) {
        digitalWrite(dirPin, HIGH);  // Forward
    } else {
        digitalWrite(dirPin, LOW);   // Reverse
    }
    
    // Convert percentage to PWM value (0-255)
    uint8_t pwmValue = (uint8_t)(abs(speedPercent) * PWM_MAX / 100.0f);
    analogWrite(pwmPin, pwmValue);
    
    // For PWM motors, actual speed follows target immediately
    status.actualSpeed = speedPercent;
    status.lastUpdateMs = millis();
    
    // Debug output
    static uint32_t lastDebug = 0;
    if (millis() - lastDebug > 1000) {
        lastDebug = millis();
        Serial.printf("\r\n[PWMMotor] Speed: %.1f%% -> PWM: %d, DIR: %s", 
                     speedPercent, pwmValue, 
                     digitalRead(dirPin) ? "FWD" : "REV");
        
        if (hasCurrentSense) {
            Serial.printf(", Current: %.2fA", getCurrent());
        }
    }
}

void PWMMotorDriver::stop() {
    analogWrite(pwmPin, 0);
    status.targetSpeed = 0.0f;
    status.actualSpeed = 0.0f;
    status.lastUpdateMs = millis();
}

float PWMMotorDriver::getCurrent() const {
    if (!hasCurrentSense) return 0.0f;
    
    // Read ADC value
    int adcValue = analogRead(currentPin);
    
    // Convert to current using scale and offset
    // For DRV8701: Vout = offset + (current * scale)
    // Default values would need calibration for specific board
    float voltage = (adcValue * 3.3f) / 4095.0f;
    float current = (voltage - currentOffset) / currentScale;
    
    return current;
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
    Serial.printf("\r\n[PWMMotor] Current scaling set: scale=%.3f, offset=%.3f", scale, offset);
}

void PWMMotorDriver::setPWMFrequency(uint32_t freq) {
    analogWriteFrequency(pwmPin, freq);
    Serial.printf("\r\n[PWMMotor] PWM frequency set to %lu Hz", freq);
}