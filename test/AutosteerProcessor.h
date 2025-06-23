#ifndef AUTOSTEER_PROCESSOR_H
#define AUTOSTEER_PROCESSOR_H

#include <Arduino.h>
#include "PIDController.h"

enum class SteerState {
    OFF,
    READY,
    ACTIVE
};

class AutosteerProcessor {
private:
    static AutosteerProcessor* instance;
    
    SteerState state;
    PIDController pid;
    
    float targetAngle;
    float currentAngle;
    float motorSpeed;
    
    uint32_t lastUpdate;
    uint32_t lastCommand;
    
    bool steerEnabled;
    bool lastAgOpenGPSState = false;  // Track previous state from AgOpenGPS
    uint32_t buttonPressTime = 0;     // Time when physical button was pressed
    
    // Kickout cooldown
    uint32_t kickoutTime = 0;
    static constexpr uint32_t KICKOUT_COOLDOWN_MS = 2000;  // 2 second cooldown after kickout
    
    AutosteerProcessor();
    
public:
    static AutosteerProcessor* getInstance();
    
    bool init();
    void process();
    
    // Control interface
    void setTargetAngle(float angle);
    void enable(bool enabled);
    void emergencyStop();
    
    // PGN handlers
    void handleSteerData(uint8_t* data, uint8_t len);
    void handleSteerSettings(uint8_t* data, uint8_t len);
    void handleSteerConfig(uint8_t* data, uint8_t len);
    
    // Status
    SteerState getState() const { return state; }
    float getCurrentAngle() const { return currentAngle; }
    float getTargetAngle() const { return targetAngle; }
    float getMotorSpeed() const { return motorSpeed; }
    bool isEnabled() const { return steerEnabled; }
    
    // Send PGN 253 data to AgOpenGPS
    void sendPGN253();
    
    // Static PGN handlers for callback registration
    static void handleSteerDataStatic(uint8_t pgn, const uint8_t* data, size_t len);
    static void handleSteerSettingsStatic(uint8_t pgn, const uint8_t* data, size_t len);
    static void handleSteerConfigStatic(uint8_t pgn, const uint8_t* data, size_t len);
};

// Global pointer
extern AutosteerProcessor* autosteerPTR;

#endif