// KeyaCANMotorDriver.cpp - Just make motor spin
#include "KeyaCANMotorDriver.h"

KeyaCANMotorDriver::KeyaCANMotorDriver(CANManager* canMgr, uint8_t busNum) 
    : canManager(canMgr), canBusNum(busNum), lastMs(0) {
    status = {false, 0.0f, 0.0f, 0.0f, 0, 0, false, {0}};
}

bool KeyaCANMotorDriver::init() {
    // DO NOTHING - just return true
    return true;
}

void KeyaCANMotorDriver::process() {
    if (millis() - lastMs >= 20) {
        // Speed
        CAN_message_t msg;
        msg.id = 0x06000001;
        msg.flags.extended = 1;
        msg.len = 8;
        msg.buf[0] = 0x23;
        msg.buf[1] = 0x00;
        msg.buf[2] = 0x20;
        msg.buf[3] = 0x01;
        msg.buf[4] = 0x01;
        msg.buf[5] = 0xF4;
        msg.buf[6] = 0x00;
        msg.buf[7] = 0x00;
        canManager->sendMessage(canBusNum, msg);
        
        // Enable
        msg.buf[1] = 0x0D;
        msg.buf[4] = 0x00;
        msg.buf[5] = 0x00;
        canManager->sendMessage(canBusNum, msg);
        
        lastMs = millis();
    }
}

// Stubs
void KeyaCANMotorDriver::enable(bool en) {}
void KeyaCANMotorDriver::setSpeed(float speedPercent) {}
void KeyaCANMotorDriver::stop() {}
void KeyaCANMotorDriver::resetErrors() {}
MotorStatus KeyaCANMotorDriver::getStatus() const { return status; }
MotorDriverType KeyaCANMotorDriver::getType() const { return MotorDriverType::KEYA_CAN; }
const char* KeyaCANMotorDriver::getTypeName() const { return "Keya"; }
bool KeyaCANMotorDriver::hasCurrentSensing() const { return false; }
bool KeyaCANMotorDriver::hasPositionFeedback() const { return false; }
float KeyaCANMotorDriver::getCurrent() const { return 0.0f; }