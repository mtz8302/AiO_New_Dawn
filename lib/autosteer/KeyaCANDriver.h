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
    bool keyaDetected = false;
    uint32_t lastHeartbeat = 0;
    
public:
    KeyaCANDriver() : can3(&globalCAN3) {}
    
    bool init() override {
        // CAN3 already initialized by global init
        // Check for Keya heartbeat
        uint32_t startTime = millis();
        CAN_message_t msg;
        
        while (millis() - startTime < 1000) {
            if (can3->read(msg)) {
                if (msg.flags.extended && msg.id == 0x07000001) {
                    keyaDetected = true;
                    lastHeartbeat = millis();
                    Serial.print("\r\n[KeyaCANDriver] Keya motor detected");
                    break;
                }
            }
            delay(10);
        }
        
        if (!keyaDetected) {
            Serial.print("\r\n[KeyaCANDriver] No Keya motor detected on CAN3");
        }
        
        return keyaDetected;
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
        if (!keyaDetected) return;
        
        // Check for heartbeat timeout (2 seconds)
        CAN_message_t rxMsg;
        while (can3->read(rxMsg)) {
            if (rxMsg.flags.extended && rxMsg.id == 0x07000001) {
                lastHeartbeat = millis();
            }
        }
        
        if (millis() - lastHeartbeat > 2000) {
            keyaDetected = false;
            Serial.print("\r\n[KeyaCANDriver] Lost connection to Keya motor");
            return;
        }
        
        // Send commands every 20ms
        if (millis() - lastSendTime >= 20) {
            // Calculate speed value (targetSpeed is -100 to +100%)
            // Keya uses 0-1000 where 1000 = 100rpm
            int16_t speedValue = (int16_t)(targetSpeed * 10.0f);  // -1000 to +1000
            
            // Send speed command
            CAN_message_t msg;
            msg.id = 0x06000001;
            msg.flags.extended = 1;
            msg.len = 8;
            msg.buf[0] = 0x23;
            msg.buf[1] = 0x00;  // Speed command
            msg.buf[2] = 0x20;
            msg.buf[3] = 0x01;
            msg.buf[4] = (speedValue >> 8) & 0xFF;  // High byte
            msg.buf[5] = speedValue & 0xFF;         // Low byte
            msg.buf[6] = 0x00;
            msg.buf[7] = 0x00;
            can3->write(msg);
            
            // Send enable/disable command
            msg.buf[1] = enabled ? 0x0D : 0x0C;  // Enable or Disable
            msg.buf[4] = 0x00;
            msg.buf[5] = 0x00;
            can3->write(msg);
            
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