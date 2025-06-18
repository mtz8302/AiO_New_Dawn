// KeyaCANMotorDriver.h - Minimal Keya CAN motor driver
#ifndef KEYA_CAN_MOTOR_DRIVER_H
#define KEYA_CAN_MOTOR_DRIVER_H

#include "MotorDriverInterface.h"
#include "CANManager.h"

class KeyaCANMotorDriver : public MotorDriverInterface {
private:
    // Keya motor CAN IDs
    static constexpr uint32_t KEYA_COMMAND_ID = 0x06000001;
    static constexpr uint32_t KEYA_HEARTBEAT_ID = 0x07000001;
    
    CANManager* canManager;
    uint8_t canBusNum;
    MotorStatus status;
    
    uint32_t lastMs;
    
public:
    KeyaCANMotorDriver(CANManager* canMgr, uint8_t busNum = 3);
    ~KeyaCANMotorDriver() = default;
    
    // MotorDriverInterface implementation
    bool init() override;
    void enable(bool en) override;
    void setSpeed(float speedPercent) override;
    void stop() override;
    void process() override;
    void resetErrors() override;
    
    MotorStatus getStatus() const override;
    MotorDriverType getType() const override;
    const char* getTypeName() const override;
    bool hasCurrentSensing() const override;
    bool hasPositionFeedback() const override;
    float getCurrent() const override;
};

#endif // KEYA_CAN_MOTOR_DRIVER_H