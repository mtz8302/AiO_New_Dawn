#ifndef HARDWAREMANAGER_H_
#define HARDWAREMANAGER_H_

#include "Arduino.h"

// Pin definitions (hardware-specific only)
const uint8_t WAS_SENSOR_PIN = A15;
const uint8_t SPEEDPULSE_PIN = 33;
const uint8_t SPEEDPULSE10_PIN = 37;
const uint8_t BUZZER = 36;
#define SLEEP_PIN 4
#define PWM1_PIN 5
#define PWM2_PIN 6
#define STEER_PIN 2
#define WORK_PIN A17
#define KICKOUT_D_PIN 3
#define CURRENT_PIN A13
#define KICKOUT_A_PIN A12

class HardwareManager
{
private:
    static HardwareManager *instance;
    bool isInitialized;
    uint8_t pwmFrequencyMode;

public:
    HardwareManager();
    ~HardwareManager();

    static HardwareManager *getInstance();
    static void init();

    // Initialization methods
    bool initialize();
    bool initializeHardware(); // Main initialization method called from setup()
    bool initializePins();
    bool initializePWM();
    bool initializeADC();

    // PWM configuration
    bool setPWMFrequency(uint8_t mode);

    // Pin access methods (return values from pcb.h)
    uint8_t getWASSensorPin() const;
    uint8_t getSpeedPulsePin() const;
    uint8_t getSpeedPulse10Pin() const;
    uint8_t getBuzzerPin() const;
    uint8_t getSleepPin() const;
    uint8_t getPWM1Pin() const;
    uint8_t getPWM2Pin() const;
    uint8_t getSteerPin() const;
    uint8_t getWorkPin() const;
    uint8_t getKickoutDPin() const;
    uint8_t getCurrentPin() const;
    uint8_t getKickoutAPin() const;

    // Hardware control methods
    void enableBuzzer();
    void disableBuzzer();
    void enableSteerMotor();
    void disableSteerMotor();

    // Status and debug
    void printHardwareStatus();
    void printPinConfiguration();
    bool getInitializationStatus() const;
};

// Global instance (following the same pattern as configManager)
extern HardwareManager hardwareManager;

#endif // HARDWAREMANAGER_H_