// LEDManager.h - Simple LED control for front panel status LEDs
#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

class LEDManager {
public:
    // LED identifiers
    enum LED_ID { 
        PWR_ETH = 0,  // Power/Ethernet status
        GPS = 1,      // GPS status
        STEER = 2,    // Autosteer status
        INS = 3       // INS/IMU status
    };
    
    // LED colors
    enum LED_COLOR { 
        OFF = 0,
        RED = 1,
        YELLOW = 2,
        GREEN = 3
    };
    
    // LED modes
    enum LED_MODE { 
        SOLID = 0,
        BLINKING = 1
    };
    
    LEDManager();
    ~LEDManager() = default;
    
    // Initialize the LED controller
    bool init();
    
    // Update LEDs (call regularly for blinking)
    void update();
    
    // Brightness control (0-100%)
    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const { return brightness; }
    
    // Status setters
    void setPowerState(bool hasEthernet, bool hasAgIO);
    void setGPSState(uint8_t fixQuality, bool hasData);
    void setSteerState(bool wasReady, bool enabled, bool active);
    void setIMUState(bool detected, bool initialized, bool valid);
    
    // Direct LED control
    void setLED(LED_ID id, LED_COLOR color, LED_MODE mode = SOLID);
    
    // Test mode - cycle through all colors on all LEDs
    void testLEDs();
    
    // Update all LED states based on system status
    // Call this periodically (e.g., every 100ms) from main loop
    void updateAll();
    
private:
    // PCA9685 controller
    Adafruit_PWMServoDriver* pwm;
    static const uint8_t LED_CONTROLLER_ADDRESS = 0x70;
    
    // LED channel assignments on PCA9685
    static const uint8_t LED_PINS[4][3];  // [LED_ID][R,G,B]
    
    // Color definitions (PWM values at 100% brightness)
    static const uint16_t COLOR_VALUES[4][3];  // [COLOR][R,G,B]
    
    // Brightness control
    uint8_t brightness;
    static const uint8_t DEFAULT_BRIGHTNESS = 25;  // 25% default
    
    // LED states
    struct LEDState {
        LED_COLOR color;
        LED_MODE mode;
        bool blinkState;
        uint32_t lastBlinkTime;
    };
    LEDState leds[4];
    
    // Blink timing
    static const uint32_t BLINK_INTERVAL_MS = 500;
    
    // Helper functions
    uint16_t scalePWM(uint16_t value);
    void updateSingleLED(LED_ID id);
    void setLEDPins(LED_ID id, uint16_t r, uint16_t g, uint16_t b);
};

// Global pointer following established pattern
extern LEDManager* ledPTR;

#endif // LED_MANAGER_H