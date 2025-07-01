// KeyaCANDriver.h - Minimal Keya CAN motor driver (standalone)
#ifndef KEYA_CAN_DRIVER_H
#define KEYA_CAN_DRIVER_H

#include "MotorDriverInterface.h"
#include "CANGlobals.h"
#include "EventLogger.h"

class KeyaCANDriver : public MotorDriverInterface {
private:
    // Use pointer to global CAN3
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256>* can3;
    
    bool enabled = false;
    float targetSpeed = 0.0f;
    uint32_t lastSendTime = 0;
    
    // Heartbeat-based feedback (from 0x07000001)
    float actualRPM = 0.0f;
    float commandedRPM = 0.0f;
    uint16_t motorPosition = 0;
    uint16_t motorCurrent = 0;
    uint16_t motorErrorCode = 0;
    uint32_t lastHeartbeat = 0;
    bool heartbeatValid = false;
    
    // Command alternation state
    enum CommandState {
        SEND_ENABLE,
        SEND_SPEED
    };
    CommandState nextCommand = SEND_ENABLE;
    
    // Slip detection parameters (counter-based like the working example)
    static constexpr uint8_t SLIP_COUNT_THRESHOLD = 8;  // 8 consecutive errors before kickout
    static constexpr float SLIP_RPM_TOLERANCE = 10.0f;  // RPM error tolerance
    
public:
    KeyaCANDriver() : can3(&globalCAN3) {}
    
    bool init() override {
        // CAN3 already initialized by global init
        LOG_INFO(EventSource::AUTOSTEER, "KeyaCANDriver initialized");
        return true;
    }
    
    void enable(bool en) override {
        if (!enabled && en) {
            // Motor is being enabled
            LOG_INFO(EventSource::AUTOSTEER, "Keya motor enabled");
        }
        enabled = en;
    }
    
    void setSpeed(float percent) override {
        float newSpeed = constrain(percent, -100.0f, 100.0f);
        
        
        targetSpeed = newSpeed;
        // For a 100 RPM motor: -100% to +100% = -100 RPM to +100 RPM
        // The CAN command multiplies by 10, so -1000 to +1000 = -100 RPM to +100 RPM
        commandedRPM = targetSpeed;  // 100% = 100 RPM
    }
    
    void stop() override {
        targetSpeed = 0.0f;
    }
    
    void process() override {
        // Check for CAN messages multiple times per process() call
        // to ensure we don't miss responses
        for (int i = 0; i < 5; i++) {
            checkCANMessages();
        }
        
        // Send commands in rotation every 20ms
        if (millis() - lastSendTime >= 20) {
            CAN_message_t msg;
            msg.id = 0x06000001;
            msg.flags.extended = 1;
            msg.len = 8;
            
            if (enabled) {
                switch (nextCommand) {
                case SEND_ENABLE:
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
                    nextCommand = SEND_SPEED;
                    break;
                    
                case SEND_SPEED:
                    {
                        // Send speed command
                        int32_t speedValue = (int32_t)(targetSpeed * 10.0f);  // -1000 to +1000 as 32-bit
                        msg.buf[0] = 0x23;
                        msg.buf[1] = 0x00;  // Speed command
                        msg.buf[2] = 0x20;
                        msg.buf[3] = 0x01;
                        msg.buf[4] = (speedValue >> 8) & 0xFF;   // DATA_L(H) - bits 15-8
                        msg.buf[5] = speedValue & 0xFF;          // DATA_L(L) - bits 7-0
                        msg.buf[6] = (speedValue >> 24) & 0xFF;  // DATA_H(H) - bits 31-24 (sign extension)
                        msg.buf[7] = (speedValue >> 16) & 0xFF;  // DATA_H(L) - bits 23-16 (sign extension)
                        can3->write(msg);
                        nextCommand = SEND_ENABLE;
                    }
                    break;
                }
            } else {
                // Send alternating disable and zero speed commands
                static bool sendDisable = true;
                
                if (sendDisable) {
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
                } else {
                    // Send zero speed command
                    msg.buf[0] = 0x23;
                    msg.buf[1] = 0x00;  // Speed command
                    msg.buf[2] = 0x20;
                    msg.buf[3] = 0x01;
                    msg.buf[4] = 0x00;  // Speed 0
                    msg.buf[5] = 0x00;
                    msg.buf[6] = 0x00;
                    msg.buf[7] = 0x00;
                    can3->write(msg);
                }
                
                // Toggle for next time
                sendDisable = !sendDisable;
            }
            
            lastSendTime = millis();
        }
    }
    
    MotorStatus getStatus() const override {
        MotorStatus status;
        status.enabled = enabled;
        status.targetSpeed = targetSpeed;
        status.actualSpeed = heartbeatValid ? (actualRPM / 100.0f * 100.0f) : targetSpeed;
        status.currentDraw = 0.0f;
        status.hasError = (motorErrorCode != 0 && motorErrorCode != 0x4001);  // 0x4001 = normal
        return status;
    }
    
    // RPM feedback methods
    float getActualRPM() const { return actualRPM; }
    float getCommandedRPM() const { return commandedRPM; }
    bool hasRPMFeedback() const { return heartbeatValid; }
    uint16_t getMotorErrorCode() const { return motorErrorCode; }
    
    // Check for motor slip (counter-based like working example)
    bool checkMotorSlip() {
        // Static counter persists between calls
        static uint8_t slipCounter = 0;
        
        if (!heartbeatValid || !enabled) {
            slipCounter = 0;
            return false;
        }
        
        // Check if motor has error flags (0x0140 = normal/enabled based on example: 40 01)
        // Bit 0 of byte 7 indicates enabled/disabled state
        if (motorErrorCode != 0 && motorErrorCode != 0x4001) {
            // Check specific error bits from the working example
            uint8_t errorLow = motorErrorCode & 0xFF;  // byte 7
            uint8_t errorHigh = (motorErrorCode >> 8) & 0xFF;  // byte 6
            
            // Only trigger kickout on actual errors, not just disabled state
            if (errorLow > 1 || errorHigh > 0) {
                LOG_WARNING(EventSource::AUTOSTEER, "Keya motor error code: 0x%04X", motorErrorCode);
                return true;  // Immediate kickout on error
            }
        }
        
        // Calculate error as absolute difference between actual and commanded
        float error = abs(actualRPM - commandedRPM);
        
        
        // Check if error exceeds commanded speed + tolerance
        // This matches the working example: if (error > abs(keyaCurrentSetSpeed) + 10)
        if (error > abs(commandedRPM) + SLIP_RPM_TOLERANCE) {
            slipCounter++;
            if (slipCounter >= SLIP_COUNT_THRESHOLD) {
                LOG_WARNING(EventSource::AUTOSTEER, 
                           "Keya motor slip detected! Counter=%d Cmd=%.1f Act=%.1f Error=%.1f",
                           slipCounter, commandedRPM, actualRPM, error);
                return true;
            }
        } else {
            // Reset counter if speed is within tolerance
            if (slipCounter > 0) {
                LOG_DEBUG(EventSource::AUTOSTEER, "Keya slip counter reset (was %d)", slipCounter);
            }
            slipCounter = 0;
        }
        return false;
    }
    
private:
    void checkCANMessages() {
        CAN_message_t rxMsg;
        
        while (can3->read(rxMsg)) {
            // Check for heartbeat message from Keya (ID: 0x07000001)
            if (rxMsg.id == 0x07000001 && rxMsg.flags.extended) {
                // Heartbeat format (from manual - big-endian/MSB first):
                // Bytes 0-1: Position/Angle (uint16) - high byte first
                // Bytes 2-3: Speed/RPM (int16) - high byte first, with sign
                // Bytes 4-5: Current (int16) - high byte first, with sign
                // Bytes 6-7: Error code (uint16) - high byte first
                
                // Extract position (high byte first)
                motorPosition = (uint16_t)((rxMsg.buf[0] << 8) | rxMsg.buf[1]);
                
                // Extract speed (high byte first, signed)
                int16_t speedRaw = (int16_t)((rxMsg.buf[2] << 8) | rxMsg.buf[3]);
                actualRPM = (float)speedRaw;
                
                // Extract current (high byte first, signed)
                int16_t currentRaw = (int16_t)((rxMsg.buf[4] << 8) | rxMsg.buf[5]);
                motorCurrent = (uint16_t)abs(currentRaw);
                
                // Extract error code (high byte first)
                motorErrorCode = (uint16_t)((rxMsg.buf[6] << 8) | rxMsg.buf[7]);
                
                heartbeatValid = true;
                lastHeartbeat = millis();
                
            }
        }
        
        // Invalidate heartbeat if not received for 500ms
        if (heartbeatValid && millis() - lastHeartbeat > 500) {
            heartbeatValid = false;
        }
    }
    
    MotorDriverType getType() const override { return MotorDriverType::KEYA_CAN; }
    const char* getTypeName() const override { return "Keya CAN"; }
    bool hasCurrentSensing() const override { return false; }
    bool hasPositionFeedback() const override { return false; }
};

#endif // KEYA_CAN_DRIVER_H