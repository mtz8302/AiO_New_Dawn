// LEDManager.cpp - Implementation of front panel LED control
#include "LEDManager.h"
#include <Wire.h>
#include "EventLogger.h"

// Global pointer
LEDManager* ledPTR = nullptr;

// LED pin assignments on PCA9685 (from NG-V6)
const uint8_t LEDManager::LED_PINS[4][3] = {
    {13, 14, 15},  // PWR_ETH: R=13, G=14, B=15
    {5, 7, 12},    // GPS: R=5, G=7, B=12
    {1, 0, 3},     // STEER: R=1, G=0, B=3
    {6, 4, 2}      // INS: R=6, G=4, B=2
};

// Color definitions at 100% brightness (12-bit PWM: 0-4095)
const uint16_t LEDManager::COLOR_VALUES[4][3] = {
    {0, 0, 0},           // OFF
    {4095, 0, 0},        // RED
    {4095, 2048, 0},     // YELLOW (Red + half Green)
    {0, 4095, 0}         // GREEN
};

LEDManager::LEDManager() : pwm(nullptr), brightness(DEFAULT_BRIGHTNESS) {
    // Initialize LED states
    for (int i = 0; i < 4; i++) {
        leds[i].color = OFF;
        leds[i].mode = SOLID;
        leds[i].blinkState = false;
        leds[i].lastBlinkTime = 0;
    }
}

bool LEDManager::init() {
    LOG_INFO(EventSource::SYSTEM, "Initializing LED Manager");
    
    // Create PCA9685 driver - explicitly specify Wire like NG-V6
    pwm = new Adafruit_PWMServoDriver(LED_CONTROLLER_ADDRESS, Wire);
    
    // Check if PCA9685 is present
    Wire.beginTransmission(LED_CONTROLLER_ADDRESS);
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        LOG_ERROR(EventSource::SYSTEM, "PCA9685 not found at 0x70 (error=%d)", error);
        delete pwm;
        pwm = nullptr;
        return false;
    }
    LOG_DEBUG(EventSource::SYSTEM, "PCA9685 detected at 0x70");
    
    // Initialize PCA9685
    pwm->begin();
    
    // Set I2C speed to 1MHz like NG-V6
    Wire.setClock(1000000);
    
    pwm->setPWMFreq(120);  // 120Hz like NG-V6 to avoid flicker
    pwm->setOutputMode(false);  // false: open drain mode for common anode LEDs
    
    // Turn off all channels using setPin like NG-V6
    for (int i = 0; i < 16; i++) {
        pwm->setPin(i, 0, true);  // 0 with invert=true means fully off
    }
    
    LOG_INFO(EventSource::SYSTEM, "LED Manager initialized (brightness=%d%%)", brightness);
    
    // Direct test - try to turn off channel 0 completely
    LOG_DEBUG(EventSource::SYSTEM, "LED Direct test: turning off channel 0");
    pwm->setPWM(0, 0, 4095);  // Full off
    delay(500);
    LOG_DEBUG(EventSource::SYSTEM, "LED Direct test: turning on channel 0");
    pwm->setPWM(0, 0, 0);     // Full on
    delay(500);
    LOG_DEBUG(EventSource::SYSTEM, "LED Direct test: done");
    
    // Quick test flash - all LEDs green for 100ms
    for (int i = 0; i < 4; i++) {
        setLED((LED_ID)i, GREEN, SOLID);
    }
    delay(100);
    
    // Turn off all LEDs
    for (int i = 0; i < 4; i++) {
        setLED((LED_ID)i, OFF, SOLID);
    }
    
    return true;
}

void LEDManager::update() {
    if (!pwm) return;
    
    uint32_t now = millis();
    
    // Update each LED
    for (int i = 0; i < 4; i++) {
        if (leds[i].mode == BLINKING) {
            // Check if it's time to toggle
            if (now - leds[i].lastBlinkTime >= BLINK_INTERVAL_MS) {
                leds[i].blinkState = !leds[i].blinkState;
                leds[i].lastBlinkTime = now;
                updateSingleLED((LED_ID)i);
            }
        }
    }
}

void LEDManager::setBrightness(uint8_t percent) {
    brightness = constrain(percent, 5, 100);  // Minimum 5% to ensure visibility
    
    // Update all LEDs with new brightness
    for (int i = 0; i < 4; i++) {
        updateSingleLED((LED_ID)i);
    }
}

uint16_t LEDManager::scalePWM(uint16_t value) {
    return (value * brightness) / 100;
}

void LEDManager::setPowerState(bool hasEthernet, bool hasAgIO) {
    if (!hasEthernet) {
        setLED(PWR_ETH, RED, SOLID);
    } else if (!hasAgIO) {
        setLED(PWR_ETH, YELLOW, BLINKING);
    } else {
        setLED(PWR_ETH, GREEN, SOLID);
    }
}

void LEDManager::setGPSState(uint8_t fixQuality, bool hasData) {
    if (!hasData) {
        setLED(GPS, OFF, SOLID);
    } else {
        switch (fixQuality) {
            case 0:  // No fix
                setLED(GPS, RED, SOLID);
                break;
            case 1:  // GPS
            case 2:  // DGPS
                setLED(GPS, YELLOW, SOLID);
                break;
            case 5:  // RTK Float
                setLED(GPS, YELLOW, BLINKING);
                break;
            case 4:  // RTK Fixed
                setLED(GPS, GREEN, SOLID);
                break;
            default:
                setLED(GPS, YELLOW, SOLID);
                break;
        }
    }
}

void LEDManager::setSteerState(bool wasReady, bool enabled, bool active) {
    if (!wasReady) {
        setLED(STEER, OFF, SOLID);
    } else if (!enabled) {
        setLED(STEER, YELLOW, SOLID);
    } else if (!active) {
        setLED(STEER, GREEN, BLINKING);
    } else {
        setLED(STEER, GREEN, SOLID);
    }
}

void LEDManager::setIMUState(bool detected, bool initialized, bool valid) {
    if (!detected) {
        setLED(INS, OFF, SOLID);
    } else if (!initialized) {
        setLED(INS, RED, BLINKING);
    } else if (!valid) {
        setLED(INS, YELLOW, SOLID);
    } else {
        setLED(INS, GREEN, SOLID);
    }
}

void LEDManager::setLED(LED_ID id, LED_COLOR color, LED_MODE mode) {
    if (!pwm || id > INS) return;
    
    leds[id].color = color;
    leds[id].mode = mode;
    
    // Reset blink state when changing modes
    if (mode == SOLID) {
        leds[id].blinkState = false;
    }
    
    updateSingleLED(id);
}

void LEDManager::updateSingleLED(LED_ID id) {
    if (!pwm || id > INS) return;
    
    static bool firstUpdate[4] = {false, false, false, false};
    
    // Determine if LED should be on
    bool ledOn = (leds[id].mode == SOLID) || 
                 (leds[id].mode == BLINKING && leds[id].blinkState);
    
    if (!ledOn || leds[id].color == OFF) {
        // Turn off LED
        if (!firstUpdate[id]) {
            LOG_DEBUG(EventSource::SYSTEM, "LED %d: OFF", id);
            firstUpdate[id] = true;
        }
        setLEDPins(id, 0, 0, 0);
    } else {
        // Get color values and apply brightness
        uint16_t r = scalePWM(COLOR_VALUES[leds[id].color][0]);
        uint16_t g = scalePWM(COLOR_VALUES[leds[id].color][1]);
        uint16_t b = scalePWM(COLOR_VALUES[leds[id].color][2]);
        
        // Additional scaling for colors that tend to be too bright
        if (leds[id].color == RED) {
            r = (r * 80) / 100;  // Red at 80% of scaled value
        } else if (leds[id].color == YELLOW) {
            r = (r * 60) / 100;  // Yellow red at 60%
            g = (g * 60) / 100;  // Yellow green at 60%
        }
        
        if (!firstUpdate[id]) {
            const char* colors[] = {"OFF", "RED", "YELLOW", "GREEN"};
            LOG_DEBUG(EventSource::SYSTEM, "LED %d: %s (R=%d G=%d B=%d)", 
                         id, colors[leds[id].color], r, g, b);
            firstUpdate[id] = true;
        }
        
        setLEDPins(id, r, g, b);
    }
}

void LEDManager::setLEDPins(LED_ID id, uint16_t r, uint16_t g, uint16_t b) {
    if (!pwm || id > INS) return;
    
    // Use setPin with invert flag for common anode LEDs
    // The library will handle the inversion when invert=true
    pwm->setPin(LED_PINS[id][0], r, true);
    delayMicroseconds(50);  // Small delay to prevent crosstalk
    pwm->setPin(LED_PINS[id][1], g, true);
    delayMicroseconds(50);
    pwm->setPin(LED_PINS[id][2], b, true);
}

void LEDManager::testLEDs() {
    if (!pwm) return;
    
    LOG_INFO(EventSource::SYSTEM, "Running LED test sequence");
    
    // Test each LED with each color
    for (int led = 0; led < 4; led++) {
        const char* ledNames[] = {"PWR_ETH", "GPS", "STEER", "INS"};
        LOG_DEBUG(EventSource::SYSTEM, "Testing %s LED:", ledNames[led]);
        
        for (int color = 1; color <= 3; color++) {  // Skip OFF
            const char* colorNames[] = {"", "RED", "YELLOW", "GREEN"};
            LOG_DEBUG(EventSource::SYSTEM, "  %s", colorNames[color]);
            
            setLED((LED_ID)led, (LED_COLOR)color, SOLID);
            delay(500);
            setLED((LED_ID)led, OFF, SOLID);
            delay(100);
        }
    }
    
    // Test blinking
    LOG_DEBUG(EventSource::SYSTEM, "Testing all LEDs blinking green");
    for (int led = 0; led < 4; led++) {
        setLED((LED_ID)led, GREEN, BLINKING);
    }
    
    // Let them blink for 3 seconds
    for (int i = 0; i < 30; i++) {
        update();
        delay(100);
    }
    
    // Turn off all LEDs
    for (int led = 0; led < 4; led++) {
        setLED((LED_ID)led, OFF, SOLID);
    }
    
    LOG_INFO(EventSource::SYSTEM, "LED test sequence complete");
}