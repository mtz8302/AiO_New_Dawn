// TractorCANDriver.cpp - Unified CAN driver implementation
#include "TractorCANDriver.h"

bool TractorCANDriver::init() {
    // Load configuration from EEPROM
    config = configManager.getCANSteerConfig();

    // Assign CAN bus pointers based on configuration
    assignCANBuses();

    LOG_INFO(EventSource::AUTOSTEER, "TractorCANDriver initialized - Brand: %d", config.brand);

    return true;
}

void TractorCANDriver::assignCANBuses() {
    // For now, simple mapping: find which bus has Keya function
    // TODO: Implement full flexible mapping for all functions

    // Check CAN1
    if (config.can1Function == static_cast<uint8_t>(CANFunction::KEYA)) {
        steerBusNum = 1;
        steerCAN = getBusPointer(1);
    }
    // Check CAN2
    else if (config.can2Function == static_cast<uint8_t>(CANFunction::KEYA)) {
        steerBusNum = 2;
        steerCAN = getBusPointer(2);
    }
    // Check CAN3
    else if (config.can3Function == static_cast<uint8_t>(CANFunction::KEYA)) {
        steerBusNum = 3;
        steerCAN = getBusPointer(3);
    }
    else {
        steerBusNum = 0;
        steerCAN = nullptr;
    }

    // For now, no button or hitch bus assignment
    buttonBusNum = 0;
    buttonCAN = nullptr;
    hitchBusNum = 0;
    hitchCAN = nullptr;
}

void* TractorCANDriver::getBusPointer(uint8_t busNum) {
    switch (busNum) {
        case 1: return (void*)&globalCAN1;  // K_Bus
        case 2: return (void*)&globalCAN2;  // ISO_Bus
        case 3: return (void*)&globalCAN3;  // V_Bus
        default: return nullptr;         // None
    }
}

bool TractorCANDriver::readCANMessage(uint8_t busNum, CAN_message_t& msg) {
    switch (busNum) {
        case 1: return globalCAN1.read(msg);
        case 2: return globalCAN2.read(msg);
        case 3: return globalCAN3.read(msg);
        default: return false;
    }
}

void TractorCANDriver::writeCANMessage(uint8_t busNum, const CAN_message_t& msg) {
    switch (busNum) {
        case 1: globalCAN1.write(msg); break;
        case 2: globalCAN2.write(msg); break;
        case 3: globalCAN3.write(msg); break;
    }
}

void TractorCANDriver::enable(bool en) {
    if (!enabled && en) {
        LOG_INFO(EventSource::AUTOSTEER, "TractorCAN enabled - %s", getTypeName());
    } else if (enabled && !en) {
        LOG_INFO(EventSource::AUTOSTEER, "TractorCAN disabled");
        targetPWM = 0;
        commandedRPM = 0.0f;
    }
    enabled = en;
}

void TractorCANDriver::setPWM(int16_t pwm) {
    pwm = constrain(pwm, -255, 255);
    targetPWM = pwm;

    // For Keya, convert PWM to RPM
    if (hasKeyaFunction()) {
        commandedRPM = (float)pwm * 100.0f / 255.0f;  // 255 PWM = 100 RPM
    }
}

void TractorCANDriver::stop() {
    targetPWM = 0;
    commandedRPM = 0.0f;
}

void TractorCANDriver::process() {
    // Process incoming CAN messages
    processIncomingMessages();

    // Send commands if we have a steering bus configured
    // For Keya, we need to send commands even when disabled to keep CAN alive
    if (steerCAN && steerBusNum > 0) {
        sendSteerCommands();
    }

    // Check for timeouts
    if (config.brand != static_cast<uint8_t>(TractorBrand::DISABLED)) {
        if (steerReady && (millis() - lastSteerReadyTime > 200)) {
            steerReady = false;
            if (hasKeyaFunction()) {
                heartbeatValid = false;
                LOG_ERROR(EventSource::AUTOSTEER, "TractorCAN connection lost - no heartbeat");
            }
        }
    }
}

void TractorCANDriver::processIncomingMessages() {
    CAN_message_t msg;

    // Process messages from each configured bus
    if (steerCAN && steerBusNum > 0) {
        while (readCANMessage(steerBusNum, msg)) {
            // If this bus has Keya function, process Keya messages
            if (hasKeyaFunction()) {
                processKeyaMessage(msg);
            } else {
                // Otherwise process based on brand
                switch (static_cast<TractorBrand>(config.brand)) {
                    case TractorBrand::FENDT:
                    case TractorBrand::FENDT_ONE:
                        processFendtMessage(msg);
                        break;
                    case TractorBrand::VALTRA_MASSEY:
                        processValtraMessage(msg);
                        break;
                    // Add other brands as needed
                }
            }
        }
    }

    // Process button bus messages if configured
    if (buttonCAN && buttonBusNum > 0 && buttonBusNum != steerBusNum) {
        while (readCANMessage(buttonBusNum, msg)) {
            // TODO: Process work switch messages
        }
    }

    // Process hitch bus messages if configured
    if (hitchCAN && hitchBusNum > 0 && hitchBusNum != steerBusNum && hitchBusNum != buttonBusNum) {
        while (readCANMessage(hitchBusNum, msg)) {
            // TODO: Process hitch control messages
        }
    }
}

void TractorCANDriver::sendSteerCommands() {
    // If any bus has Keya function, send Keya commands
    if (hasKeyaFunction()) {
        sendKeyaCommands();
    } else {
        // Otherwise send based on brand
        switch (static_cast<TractorBrand>(config.brand)) {
            case TractorBrand::FENDT:
            case TractorBrand::FENDT_ONE:
                sendFendtCommands();
                break;
            case TractorBrand::VALTRA_MASSEY:
                sendValtraCommands();
                break;
            // Add other brands as needed
        }
    }
}

// ===== Keya Implementation =====
void TractorCANDriver::processKeyaMessage(const CAN_message_t& msg) {
    // Check for heartbeat message (ID: 0x07000001)
    if (msg.id == 0x07000001 && msg.flags.extended) {
        // Heartbeat format (big-endian/MSB first):
        // Bytes 0-1: Position/Angle (uint16)
        // Bytes 2-3: Speed/RPM (int16)
        // Bytes 4-5: Current (int16)
        // Bytes 6-7: Error code (uint16)

        motorPosition = (uint16_t)((msg.buf[0] << 8) | msg.buf[1]);

        int16_t speedRaw = (int16_t)((msg.buf[2] << 8) | msg.buf[3]);
        actualRPM = (float)speedRaw;

        int16_t currentRaw = (int16_t)((msg.buf[4] << 8) | msg.buf[5]);
        motorCurrent = (uint16_t)abs(currentRaw);

        motorErrorCode = (uint16_t)((msg.buf[6] << 8) | msg.buf[7]);

        // Update status
        if (!steerReady) {
            LOG_INFO(EventSource::AUTOSTEER, "Keya motor detected and ready");
        }
        steerReady = true;
        heartbeatValid = true;
        lastSteerReadyTime = millis();
        lastHeartbeat = millis();
    }
}

void TractorCANDriver::sendKeyaCommands() {
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
                writeCANMessage(steerBusNum, msg);
                nextCommand = SEND_SPEED;
                break;

            case SEND_SPEED:
                {
                    // Send speed command
                    int32_t speedValue = (int32_t)(commandedRPM * 10.0f);  // -1000 to +1000

                    msg.buf[0] = 0x23;
                    msg.buf[1] = 0x00;  // Speed command
                    msg.buf[2] = 0x20;
                    msg.buf[3] = 0x01;
                    msg.buf[4] = (speedValue >> 8) & 0xFF;   // DATA_L(H)
                    msg.buf[5] = speedValue & 0xFF;          // DATA_L(L)
                    msg.buf[6] = (speedValue >> 24) & 0xFF;  // DATA_H(H)
                    msg.buf[7] = (speedValue >> 16) & 0xFF;  // DATA_H(L)
                    writeCANMessage(steerBusNum, msg);
                    nextCommand = SEND_ENABLE;
                }
                break;
        }
    } else {
        // Send alternating disable and zero speed commands to keep CAN alive
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
            writeCANMessage(steerBusNum, msg);
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
            writeCANMessage(steerBusNum, msg);
        }

        // Toggle for next time
        sendDisable = !sendDisable;
    }
}

// ===== Fendt Implementation (placeholder) =====
void TractorCANDriver::processFendtMessage(const CAN_message_t& msg) {
    // TODO: Implement Fendt message processing
    // Check for Fendt steer ready signals
    if (msg.id == 0x0CF02300) {  // Example Fendt ready message
        steerReady = true;
        lastSteerReadyTime = millis();
    }
}

void TractorCANDriver::sendFendtCommands() {
    // TODO: Implement Fendt steering commands
    // This will send appropriate messages based on targetPWM
}

// ===== Valtra Implementation (placeholder) =====
void TractorCANDriver::processValtraMessage(const CAN_message_t& msg) {
    // TODO: Implement Valtra message processing
    if (msg.id == 0x18EF52A0) {  // Example Valtra ready message
        steerReady = true;
        lastSteerReadyTime = millis();
    }
}

void TractorCANDriver::sendValtraCommands() {
    // TODO: Implement Valtra steering commands
}

// ===== Common Methods =====
MotorStatus TractorCANDriver::getStatus() const {
    MotorStatus status;
    status.enabled = enabled;
    status.targetPWM = targetPWM;

    // For Keya, we have actual feedback
    if (hasKeyaFunction() && heartbeatValid) {
        status.actualPWM = (int16_t)(actualRPM * 255.0f / 100.0f);
    } else {
        status.actualPWM = targetPWM;
    }

    status.currentDraw = 0.0f;  // No current sensing via CAN
    status.hasError = !steerReady;

    if (!steerReady) {
        snprintf(status.errorMessage, sizeof(status.errorMessage),
                 "No CAN connection");
    } else {
        status.errorMessage[0] = '\0';
    }

    return status;
}

const char* TractorCANDriver::getTypeName() const {
    // Check if Keya function is active
    if (hasKeyaFunction()) {
        return "Keya CAN";
    }

    switch (static_cast<TractorBrand>(config.brand)) {
        case TractorBrand::FENDT:         return "Fendt SCR/S4/Gen6";
        case TractorBrand::FENDT_ONE:     return "Fendt One";
        case TractorBrand::VALTRA_MASSEY: return "Valtra/Massey";
        case TractorBrand::CASEIH_NH:     return "Case IH/NH";
        case TractorBrand::CLAAS:         return "Claas";
        case TractorBrand::JCB:           return "JCB";
        case TractorBrand::LINDNER:       return "Lindner";
        case TractorBrand::CAT_MT:        return "CAT MT";
        case TractorBrand::GENERIC:       return "Generic CAN";
        default:                          return "Tractor CAN";
    }
}

void TractorCANDriver::handleKickout(KickoutType type, float value) {
    // For CAN-based systems, kickout is usually handled by the tractor
    // This is here for compatibility with the interface
}

void TractorCANDriver::setConfig(const CANSteerConfig& newConfig) {
    config = newConfig;
    // Reassign buses when config changes
    assignCANBuses();

    // Reset state
    steerReady = false;
    heartbeatValid = false;

    LOG_INFO(EventSource::AUTOSTEER, "TractorCAN config updated - Brand: %d, SteerBus: %d",
             config.brand, steerBusNum);
}

bool TractorCANDriver::hasKeyaFunction() const {
    return (config.can1Function == static_cast<uint8_t>(CANFunction::KEYA) ||
            config.can2Function == static_cast<uint8_t>(CANFunction::KEYA) ||
            config.can3Function == static_cast<uint8_t>(CANFunction::KEYA));
}