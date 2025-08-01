// KeyaSerialDriver.h - Keya Serial motor driver
#ifndef KEYA_SERIAL_DRIVER_H
#define KEYA_SERIAL_DRIVER_H

#include "MotorDriverInterface.h"
#include "SerialManager.h"
#include "EventLogger.h"

class KeyaSerialDriver : public MotorDriverInterface {
private:
    // Motor state
    bool enabled = false;
    int16_t targetPWM = 0;
    
    // Command/response buffers
    uint8_t commandBuffer[4];
    uint8_t responseBuffer[16];
    uint8_t responseIndex = 0;
    
    // Timing
    uint32_t lastCommandTime = 0;
    uint32_t lastResponseTime = 0;
    
    // Response data
    bool hasValidResponse = false;
    int8_t actualRPM = 0;
    uint32_t motorPosition = 0;
    int8_t motorCurrent = 0;      // in 0.1A units
    uint8_t motorVoltage = 0;     // in V
    uint16_t motorErrorCode = 0;
    uint8_t motorTemperature = 0; // in °C
    
    // Slip detection
    bool motorSlipDetected = false;
    uint32_t slipStartTime = 0;
    
public:
    KeyaSerialDriver() = default;
    ~KeyaSerialDriver() = default;
    
    // MotorDriverInterface implementation
    bool init() override;
    void enable(bool en) override;
    void setPWM(int16_t pwm) override;
    void stop() override;
    void process() override;
    MotorStatus getStatus() const override;
    
    // Type information
    MotorDriverType getType() const override { return MotorDriverType::KEYA_SERIAL; }
    const char* getTypeName() const override { return "Keya Serial"; }
    bool hasCurrentSensing() const override { return true; }
    bool hasPositionFeedback() const override { return true; }
    
    // Detection
    bool isDetected() override { return hasValidResponse; }
    
    // Kickout
    void handleKickout(KickoutType type, float value) override;
    float getCurrentDraw() override { return motorCurrent * 0.1f; } // Convert to Amps
    
    // Motor slip detection
    bool checkMotorSlip();
    
private:
    // Internal methods
    void sendCommand();
    void checkResponse();
    void buildCommand();
    uint8_t calculateChecksum(uint8_t* data, uint8_t length);
};

#endif // KEYA_SERIAL_DRIVER_H