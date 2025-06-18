// MotorDriverInterface.h - Abstract interface for all motor drivers
#ifndef MOTOR_DRIVER_INTERFACE_H
#define MOTOR_DRIVER_INTERFACE_H

#include <Arduino.h>

// Motor driver types supported
enum class MotorDriverType {
    NONE,
    CYTRON_MD30C,      // PWM-based Cytron MD30C
    IBT2,              // PWM-based IBT-2 
    DRV8701,           // PWM-based DRV8701
    KEYA_CAN,          // CAN-based Keya motor
    GENERIC_PWM        // Generic PWM driver
};

// Motor status information
struct MotorStatus {
    bool enabled;
    float targetSpeed;     // -100% to +100%
    float actualSpeed;     // -100% to +100% (if feedback available)
    float currentDraw;     // Amps (if available)
    uint32_t errorCount;
    uint32_t lastUpdateMs;
    bool hasError;
    char errorMessage[64];  // Fixed size to avoid String allocation issues
};

// Abstract base class for all motor drivers
class MotorDriverInterface {
public:
    virtual ~MotorDriverInterface() = default;
    
    // Core motor control
    virtual bool init() = 0;
    virtual void enable(bool en) = 0;
    virtual void setSpeed(float speedPercent) = 0;  // -100 to +100
    virtual void stop() = 0;
    
    // Status and diagnostics  
    virtual MotorStatus getStatus() const = 0;
    virtual MotorDriverType getType() const = 0;
    virtual const char* getTypeName() const = 0;
    virtual bool hasCurrentSensing() const = 0;
    virtual bool hasPositionFeedback() const = 0;
    
    // Optional features (override if supported)
    virtual float getCurrent() const { return 0.0f; }
    virtual float getPosition() const { return 0.0f; }
    virtual void resetErrors() { }
    
    // Process function for drivers that need regular updates
    virtual void process() { }
};

#endif // MOTOR_DRIVER_INTERFACE_H