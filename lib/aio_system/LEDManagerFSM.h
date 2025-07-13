// LEDManagerFSM.h - FSM-based LED control for front panel status LEDs
#ifndef LED_MANAGER_FSM_H
#define LED_MANAGER_FSM_H

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

class LEDManagerFSM {
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
    
    // Power/Ethernet LED states
    enum PowerState {
        PWR_NO_ETHERNET,      // No ethernet connection
        PWR_NO_AGIO,         // Ethernet OK, no AgIO
        PWR_CONNECTED        // Fully connected
    };
    
    // GPS LED states
    enum GPSState {
        GPS_NO_DATA,         // No GPS data
        GPS_NO_FIX,          // GPS data but no fix
        GPS_BASIC_FIX,       // GPS or DGPS fix
        GPS_RTK_FLOAT,       // RTK float
        GPS_RTK_FIXED        // RTK fixed
    };
    
    // Steer LED states
    enum SteerState {
        STEER_NOT_READY,     // WAS not ready
        STEER_DISABLED,      // Ready but not enabled
        STEER_STANDBY,       // Enabled but not active
        STEER_ACTIVE         // Actively steering
    };
    
    // IMU/INS LED states
    enum IMUState {
        IMU_NOT_DETECTED,    // No IMU detected
        IMU_INITIALIZING,    // Detected but not initialized
        IMU_NO_DATA,         // Initialized but no valid data
        IMU_VALID            // Valid data
    };
    
    LEDManagerFSM();
    ~LEDManagerFSM() = default;
    
    // Initialize the LED controller
    bool init();
    
    // Update LEDs (call regularly for blinking)
    void update();
    
    // Brightness control (0-100%)
    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const { return brightness; }
    
    // FSM state transitions
    void transitionPowerState(PowerState newState);
    void transitionGPSState(GPSState newState);
    void transitionSteerState(SteerState newState);
    void transitionIMUState(IMUState newState);
    
    // Get current states
    PowerState getPowerState() const { return powerState; }
    GPSState getGPSState() const { return gpsState; }
    SteerState getSteerState() const { return steerState; }
    IMUState getIMUState() const { return imuState; }
    
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
    
    // Current FSM states
    PowerState powerState;
    GPSState gpsState;
    SteerState steerState;
    IMUState imuState;
    
    // LED physical states
    struct LEDState {
        LED_COLOR color;
        LED_MODE mode;
        bool blinkState;
        uint32_t lastBlinkTime;
    };
    LEDState leds[4];
    
    // Blink timing
    static const uint32_t BLINK_INTERVAL_MS = 500;
    
    // State-to-LED mapping tables
    static const struct PowerStateMap {
        PowerState state;
        LED_COLOR color;
        LED_MODE mode;
    } powerStateMap[];
    
    static const struct GPSStateMap {
        GPSState state;
        LED_COLOR color;
        LED_MODE mode;
    } gpsStateMap[];
    
    static const struct SteerStateMap {
        SteerState state;
        LED_COLOR color;
        LED_MODE mode;
    } steerStateMap[];
    
    static const struct IMUStateMap {
        IMUState state;
        LED_COLOR color;
        LED_MODE mode;
    } imuStateMap[];
    
    // Helper functions
    uint16_t scalePWM(uint16_t value);
    void updateSingleLED(LED_ID id);
    void setLEDPins(LED_ID id, uint16_t r, uint16_t g, uint16_t b);
    void setLED(LED_ID id, LED_COLOR color, LED_MODE mode);
    
    // FSM update functions
    void updatePowerLED();
    void updateGPSLED();
    void updateSteerLED();
    void updateIMULED();
};

// Global instance following established pattern
extern LEDManagerFSM ledManagerFSM;

#endif // LED_MANAGER_FSM_H