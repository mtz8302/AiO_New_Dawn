#ifndef KICKOUT_MONITOR_H
#define KICKOUT_MONITOR_H

#include <Arduino.h>

class ConfigManager;
class ADProcessor;

class KickoutMonitor {
public:
    enum KickoutReason {
        NONE = 0,
        ENCODER_OVERSPEED = 1,
        PRESSURE_HIGH = 2,
        CURRENT_HIGH = 3,
        MOTOR_SLIP = 4
    };

    KickoutMonitor();
    ~KickoutMonitor();
    
    // Get singleton instance
    static KickoutMonitor* getInstance();
    
    // Initialize kickout monitoring
    bool init();
    
    // Process kickout checks (call frequently)
    void process();
    
    // Kickout status
    bool hasKickout() const { return kickoutActive; }
    KickoutReason getReason() const { return kickoutReason; }
    const char* getReasonString() const;
    
    // Clear kickout (after steering disabled)
    void clearKickout();
    
    // Get current counts for debugging
    uint32_t getEncoderPulseCount() const { return encoderPulseCount; }
    uint16_t getPressureReading() const { return lastPressureReading; }
    uint16_t getCurrentReading() const { return lastCurrentReading; }
    
private:
    // Singleton instance
    static KickoutMonitor* instance;
    
    // External dependencies
    ConfigManager* configMgr;
    ADProcessor* adProcessor;
    
    // Encoder monitoring
    uint32_t encoderPulseCount;
    uint32_t lastPulseCheck;
    uint32_t lastPulseCount;
    bool lastEncoderState;
    
    // Sensor readings
    uint16_t lastPressureReading;
    uint16_t lastCurrentReading;
    
    // Kickout state
    bool kickoutActive;
    KickoutReason kickoutReason;
    uint32_t kickoutTime;
    
    // Check individual kickout conditions
    bool checkEncoderKickout();
    bool checkPressureKickout();
    bool checkCurrentKickout();
};

#endif // KICKOUT_MONITOR_H