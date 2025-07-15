// PWMMotorDriver.h - PWM-based motor driver implementation
#ifndef PWM_MOTOR_DRIVER_H
#define PWM_MOTOR_DRIVER_H

#include "MotorDriverInterface.h"

// PWM motor driver for Cytron, IBT-2, DRV8701, etc.
class PWMMotorDriver : public MotorDriverInterface {
private:
    MotorDriverType driverType;
    MotorStatus status;
    
    // Pin assignments
    uint8_t pwmPin;
    uint8_t dirPin;
    uint8_t enablePin;
    uint8_t currentPin;  // Optional current sense
    
    // PWM parameters  
    static constexpr uint32_t PWM_FREQUENCY = 18310;  // Hz - DRV8701 recommended
    static constexpr uint8_t PWM_MAX = 255;
    
    // Current sensing
    bool hasCurrentSense;
    float currentScale;  // ADC to Amps conversion factor
    float currentOffset; // Zero current ADC offset
    
public:
    PWMMotorDriver(MotorDriverType type, uint8_t pwm, uint8_t dir, 
                   uint8_t enable = 255, uint8_t current = 255);
    ~PWMMotorDriver() = default;
    
    // MotorDriverInterface implementation
    bool init() override;
    void enable(bool en) override;
    void setSpeed(float speedPercent) override;
    void stop() override;
    
    MotorStatus getStatus() const override { return status; }
    MotorDriverType getType() const override { return driverType; }
    const char* getTypeName() const override;
    bool hasCurrentSensing() const override { return hasCurrentSense; }
    bool hasPositionFeedback() const override { return false; }
    
    float getCurrent() const override;
    void resetErrors() override;
    
    // PWM-specific configuration
    void setCurrentScaling(float scale, float offset);
    void setPWMFrequency(uint32_t freq);
    
    // New interface methods
    bool isDetected() override { return true; }  // PWM drivers are always "detected"
    void handleKickout(KickoutType type, float value) override;
    float getCurrentDraw() override { return getCurrent(); }
};

#endif // PWM_MOTOR_DRIVER_H