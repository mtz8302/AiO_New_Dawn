// KeyaCANDriver.h - Minimal Keya CAN motor driver (standalone)
#ifndef KEYA_CAN_DRIVER_H
#define KEYA_CAN_DRIVER_H

#include "MotorDriverInterface.h"
#include "CANGlobals.h"

class KeyaCANDriver : public MotorDriverInterface {
private:
    // Use pointer to global CAN3
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256>* can3;
    
    bool enabled = false;
    float targetSpeed = 0.0f;
    uint32_t lastSendTime = 0;
    
public:
    KeyaCANDriver() : can3(&globalCAN3) {}
    
    bool init() override {
        // CAN3 already initialized by global init
        Serial.print("\r\n[KeyaCANDriver] Initialized");
        return true;
    }
    
    void enable(bool en) override {
        enabled = en;
    }
    
    void setSpeed(float percent) override {
        targetSpeed = constrain(percent, -100.0f, 100.0f);
    }
    
    void stop() override {
        targetSpeed = 0.0f;
    }
    
    void process() override {
        // Simple alternating pattern - send one command every 20ms
        if (millis() - lastSendTime >= 20) {
            CAN_message_t msg;
            msg.id = 0x06000001;
            msg.flags.extended = 1;
            msg.len = 8;
            
            if (enabled) {
                // Alternate between enable and speed commands
                static bool sendEnable = true;
                
                if (sendEnable) {
                    // Send enable command
                    msg.buf[0] = 0x23;
                    msg.buf[1] = 0x0D;  // Enable
                    msg.buf[2] = 0x20;
                    msg.buf[3] = 0x01;
                    msg.buf[4] = 0x00;
                    msg.buf[5] = 0x00;
                    msg.buf[6] = 0x00;
                    msg.buf[7] = 0x00;
                    can3->write(msg);
                    // Serial.printf("\r\n[Keya] Sent ENABLE");
                } else {
                    // Send speed command
                    int32_t speedValue = (int32_t)(targetSpeed * 10.0f);  // -1000 to +1000 as 32-bit
                    // Manual shows: 23 00 20 01 DATA_L(H) DATA_L(L) DATA_H(H) DATA_H(L)
                    // Working log shows negative values use sign extension (FF FF for negative)
                    msg.buf[0] = 0x23;
                    msg.buf[1] = 0x00;  // Speed command
                    msg.buf[2] = 0x20;
                    msg.buf[3] = 0x01;
                    msg.buf[4] = (speedValue >> 8) & 0xFF;   // DATA_L(H) - bits 15-8
                    msg.buf[5] = speedValue & 0xFF;          // DATA_L(L) - bits 7-0
                    msg.buf[6] = (speedValue >> 24) & 0xFF;  // DATA_H(H) - bits 31-24 (sign extension)
                    msg.buf[7] = (speedValue >> 16) & 0xFF;  // DATA_H(L) - bits 23-16 (sign extension)
                    can3->write(msg);
                    // Serial.printf("\r\n[Keya] Sent SPEED %d (0x%08X)", speedValue, speedValue);
                }
                
                // Toggle for next time
                sendEnable = !sendEnable;
            } else {
                // Send disable command
                msg.buf[0] = 0x23;
                msg.buf[1] = 0x0C;  // Disable
                msg.buf[2] = 0x20;
                msg.buf[3] = 0x01;
                msg.buf[4] = 0x00;
                msg.buf[5] = 0x00;
                msg.buf[6] = 0x00;
                msg.buf[7] = 0x00;
                can3->write(msg);
            }
            
            lastSendTime = millis();
        }
    }
    
    MotorStatus getStatus() const override {
        MotorStatus status;
        status.enabled = enabled;
        status.targetSpeed = targetSpeed;
        status.actualSpeed = targetSpeed;  // No actual feedback available
        status.currentDraw = 0.0f;
        status.hasError = false;
        return status;
    }
    
    MotorDriverType getType() const override { return MotorDriverType::KEYA_CAN; }
    const char* getTypeName() const override { return "Keya CAN"; }
    bool hasCurrentSensing() const override { return false; }
    bool hasPositionFeedback() const override { return false; }
};

#endif // KEYA_CAN_DRIVER_H