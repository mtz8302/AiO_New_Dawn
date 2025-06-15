// CANManager.cpp - Implementation of CAN bus management
#include "CANManager.h"

// Static instance pointer for callbacks
CANManager* CANManager::instance = nullptr;

CANManager::CANManager() {
    // Set instance pointer for static callbacks
    instance = this;
    
    // Initialize bus info structures
    can1Info = {false, 0, 0, 0, 0, false, 0, 0};
    can2Info = {false, 0, 0, 0, 0, false, 0, 0};
    can3Info = {false, 0, 0, 0, 0, false, 0, 0};
}

bool CANManager::initializeCAN() {
    Serial.print("\r\n\n=== CAN Manager Initialization ===");
    
    bool success = true;
    
    // Initialize CAN1 - typically used for implement/ISOBUS
    Serial.print("\r\n- Initializing CAN1...");
    if (initializeBus(1, CAN_SPEED_250KBPS)) {
        Serial.print(" SUCCESS at 250kbps (ISOBUS)");
    } else {
        Serial.print(" FAILED");
        success = false;
    }
    
    // Initialize CAN2 - can be used for proprietary communications
    Serial.print("\r\n- Initializing CAN2...");
    if (initializeBus(2, CAN_SPEED_250KBPS)) {
        Serial.print(" SUCCESS at 250kbps");
    } else {
        Serial.print(" FAILED");
        success = false;
    }
    
    // CAN3 is optional on Teensy 4.1 (requires additional transceiver)
    Serial.print("\r\n- Initializing CAN3...");
    if (initializeBus(3, CAN_SPEED_250KBPS)) {
        Serial.print(" SUCCESS at 250kbps");
        Serial.print("\r\n  CAN3 ready for Keya motor (ID: 0x07000001)");
    } else {
        Serial.print(" FAILED");
        // Don't mark as failure - CAN3 is optional
    }
    
    return success;
}

bool CANManager::initializeBus(uint8_t busNum, uint32_t speed) {
    switch (busNum) {
        case 1:
            can1.begin();
            can1.setBaudRate(speed);
            can1.setMaxMB(16);  // Use 16 mailboxes
            can1.enableFIFO();
            can1.enableFIFOInterrupt();
            can1.onReceive(can1MessageHandler);
            can1.mailboxStatus();
            
            can1Info.initialized = true;
            can1Info.speed = speed;
            can1Info.busOff = false;
            return true;
            
        case 2:
            can2.begin();
            can2.setBaudRate(speed);
            can2.setMaxMB(16);
            can2.enableFIFO();
            can2.enableFIFOInterrupt();
            can2.onReceive(can2MessageHandler);
            can2.mailboxStatus();
            
            can2Info.initialized = true;
            can2Info.speed = speed;
            can2Info.busOff = false;
            return true;
            
        case 3: {
            // CAN3 - used for Keya motor (keep it simple like NG-V6)
            can3.begin();
            can3.setBaudRate(speed);
            delay(100);
            
            Serial.printf("\r\n  CAN3 actual baud: %lu", can3.getBaudRate());
            
            can3Info.initialized = true;
            can3Info.speed = speed;
            can3Info.busOff = false;
            return true;
        }
            
        default:
            return false;
    }
}

void CANManager::can1MessageHandler(const CAN_message_t &msg) {
    if (instance) {
        instance->processCANMessage(1, msg);
    }
}

void CANManager::can2MessageHandler(const CAN_message_t &msg) {
    if (instance) {
        instance->processCANMessage(2, msg);
    }
}

void CANManager::can3MessageHandler(const CAN_message_t &msg) {
    if (instance) {
        instance->processCANMessage(3, msg);
    }
}

void CANManager::processCANMessage(uint8_t busNum, const CAN_message_t &msg) {
    // Update statistics
    switch (busNum) {
        case 1:
            can1Info.messagesReceived++;
            break;
        case 2:
            can2Info.messagesReceived++;
            break;
        case 3:
            can3Info.messagesReceived++;
            
            // Debug first few messages on CAN3
            static uint32_t can3DebugCount = 0;
            if (can3DebugCount < 5) {
                can3DebugCount++;
                Serial.printf("\r\n[CAN3 MSG] ID: 0x%08X, Extended: %s, Len: %d, Data:", 
                    msg.id, msg.flags.extended ? "YES" : "NO", msg.len);
                for (int i = 0; i < msg.len; i++) {
                    Serial.printf(" %02X", msg.buf[i]);
                }
            }
            
            // Check for Keya motor heartbeat messages
            if (msg.id == 0x07000001) {
                can3Info.keyaMotorMessages++;
                can3Info.lastKeyaMessageTime = millis();
                
                // Print first Keya detection
                static bool keyaFirstDetected = false;
                if (!keyaFirstDetected) {
                    keyaFirstDetected = true;
                    Serial.print("\r\n*** Keya motor detected on CAN3! ***");
                }
            }
            break;
    }
    
    // Here you would process the message - for now just count it
    // In a full implementation, this would dispatch to appropriate handlers
}

bool CANManager::sendMessage(uint8_t busNum, const CAN_message_t &msg) {
    bool success = false;
    
    switch (busNum) {
        case 1:
            if (can1Info.initialized && !can1Info.busOff) {
                success = (can1.write(msg) == 1);
                if (success) can1Info.messagesSent++;
                else can1Info.errors++;
            }
            break;
            
        case 2:
            if (can2Info.initialized && !can2Info.busOff) {
                success = (can2.write(msg) == 1);
                if (success) can2Info.messagesSent++;
                else can2Info.errors++;
            }
            break;
            
        case 3:
            if (can3Info.initialized && !can3Info.busOff) {
                success = (can3.write(msg) == 1);
                if (success) can3Info.messagesSent++;
                else can3Info.errors++;
            }
            break;
    }
    
    return success;
}

bool CANManager::sendJ1939Message(uint8_t busNum, uint32_t pgn, uint8_t priority, 
                                  uint8_t sourceAddr, const uint8_t* data, uint8_t len) {
    CAN_message_t msg;
    
    // Build J1939 29-bit identifier
    // Priority (3 bits) | Reserved (1 bit) | Data Page (1 bit) | PDU Format (8 bits) | PDU Specific (8 bits) | Source Address (8 bits)
    msg.id = ((uint32_t)(priority & 0x07) << 26) |  // Priority
             ((pgn & 0x3FFFF) << 8) |                // PGN (includes DP, PF, PS)
             (sourceAddr & 0xFF);                    // Source address
    
    msg.flags.extended = 1;  // J1939 uses extended IDs
    msg.len = min(len, 8);   // CAN messages max 8 bytes
    
    // Copy data
    for (int i = 0; i < msg.len; i++) {
        msg.buf[i] = data[i];
    }
    
    return sendMessage(busNum, msg);
}

bool CANManager::setBusSpeed(uint8_t busNum, uint32_t speed) {
    switch (busNum) {
        case 1:
            if (can1Info.initialized) {
                can1.setBaudRate(speed);
                can1Info.speed = speed;
                return true;
            }
            break;
            
        case 2:
            if (can2Info.initialized) {
                can2.setBaudRate(speed);
                can2Info.speed = speed;
                return true;
            }
            break;
            
        case 3:
            if (can3Info.initialized) {
                can3.setBaudRate(speed);
                can3Info.speed = speed;
                return true;
            }
            break;
    }
    
    return false;
}

bool CANManager::enableBus(uint8_t busNum) {
    // Buses are enabled when initialized
    return true;
}

bool CANManager::disableBus(uint8_t busNum) {
    switch (busNum) {
        case 1:
            // FlexCAN_T4 doesn't have end(), just mark as disabled
            can1Info.initialized = false;
            return true;
            
        case 2:
            // FlexCAN_T4 doesn't have end(), just mark as disabled
            can2Info.initialized = false;
            return true;
            
        case 3:
            // FlexCAN_T4 doesn't have end(), just mark as disabled
            can3Info.initialized = false;
            return true;
    }
    
    return false;
}

CANMessageType CANManager::identifyMessageType(const CAN_message_t &msg) {
    if (msg.flags.extended) {
        // Extended ID - likely J1939/ISOBUS
        uint32_t pgn = (msg.id >> 8) & 0x3FFFF;
        
        // Check for common ISOBUS PGNs
        if (pgn >= 0xE000 && pgn <= 0xEFFF) {
            return CANMessageType::ISOBUS;
        }
        
        return CANMessageType::J1939;
    } else {
        // Standard 11-bit ID
        return CANMessageType::STANDARD;
    }
}

const char* CANManager::getMessageTypeName(CANMessageType type) {
    switch (type) {
        case CANMessageType::J1939:
            return "J1939";
        case CANMessageType::ISOBUS:
            return "ISOBUS";
        case CANMessageType::STANDARD:
            return "Standard CAN";
        case CANMessageType::EXTENDED:
            return "Extended CAN";
        case CANMessageType::UNKNOWN:
        default:
            return "Unknown";
    }
}

void CANManager::printCANStatus() {
    Serial.print("\r\n\n=== CAN Manager Status ===");
    
    Serial.print("\r\nInitialized buses:");
    int busCount = 0;
    if (can1Info.initialized) {
        Serial.print(" CAN1");
        busCount++;
    }
    if (can2Info.initialized) {
        Serial.print(" CAN2");
        busCount++;
    }
    if (can3Info.initialized) {
        Serial.print(" CAN3");
        busCount++;
    }
    if (busCount == 0) {
        Serial.print(" NONE");
    }
    
    // Print status for each initialized bus
    if (can1Info.initialized) {
        printBusStatus(1);
    }
    if (can2Info.initialized) {
        printBusStatus(2);
    }
    if (can3Info.initialized) {
        printBusStatus(3);
    }
    
    Serial.print("\r\n=============================\r\n");
}

void CANManager::printBusStatus(uint8_t busNum) {
    CANBusInfo* info = getBusInfo(busNum);
    if (!info || !info->initialized) return;
    
    Serial.printf("\r\n\n--- CAN%d Status ---", busNum);
    Serial.printf("\r\nSpeed: %d bps", info->speed);
    Serial.printf("\r\nMessages Received: %lu", info->messagesReceived);
    Serial.printf("\r\nMessages Sent: %lu", info->messagesSent);
    Serial.printf("\r\nErrors: %lu", info->errors);
    Serial.printf("\r\nBus Status: %s", info->busOff ? "BUS OFF" : "OK");
    
    // Show Keya motor status for CAN3
    if (busNum == 3 && info->keyaMotorMessages > 0) {
        Serial.printf("\r\nKeya Motor Messages: %lu", info->keyaMotorMessages);
        uint32_t timeSinceLast = millis() - info->lastKeyaMessageTime;
        Serial.printf("\r\nLast Keya Message: %lu ms ago", timeSinceLast);
        if (timeSinceLast < 1000) {
            Serial.print(" (ACTIVE)");
        } else {
            Serial.print(" (INACTIVE)");
        }
    }
}

CANManager::CANBusInfo* CANManager::getBusInfo(uint8_t busNum) {
    switch (busNum) {
        case 1: return &can1Info;
        case 2: return &can2Info;
        case 3: return &can3Info;
        default: return nullptr;
    }
}

bool CANManager::isBusOff(uint8_t busNum) {
    CANBusInfo* info = getBusInfo(busNum);
    return info ? info->busOff : true;
}

bool CANManager::resetBus(uint8_t busNum) {
    // Disable then re-enable the bus
    disableBus(busNum);
    delay(100);
    
    uint32_t speed = CAN_SPEED_250KBPS;  // Default
    CANBusInfo* info = getBusInfo(busNum);
    if (info) {
        speed = info->speed;
    }
    
    return initializeBus(busNum, speed);
}

uint32_t CANManager::getErrorCount(uint8_t busNum) {
    CANBusInfo* info = getBusInfo(busNum);
    return info ? info->errors : 0;
}

bool CANManager::setBusFilters(uint8_t busNum, uint32_t filterID, uint32_t filterMask) {
    // Implementation would set up hardware filters
    // For now, accept all messages
    return true;
}

void CANManager::clearBusFilters(uint8_t busNum) {
    // Implementation would clear hardware filters
    // For now, accept all messages
}

void CANManager::pollCANMessages() {
    // Poll only once per millisecond like NG-V6
    static uint32_t lastPollTime = 0;
    uint32_t currentTime = millis();
    
    if (currentTime == lastPollTime) {
        return;  // Already polled this millisecond
    }
    lastPollTime = currentTime;
    
    CAN_message_t msg;
    
    // Poll CAN3 for Keya messages (like NG-V6 does)
    if (can3Info.initialized) {
        // Debug polling rate
        static uint32_t pollCount = 0;
        static uint32_t lastDebug = 0;
        pollCount++;
        
        if (millis() - lastDebug > 5000) {
            lastDebug = millis();
            Serial.printf("\r\n[CAN Debug] Polling %lu times/5sec, initialized: %s", 
                         pollCount, can3Info.initialized ? "YES" : "NO");
            pollCount = 0;
        }
        
        while (can3.read(msg)) {  // Read all available messages
            processCANMessage(3, msg);
        }
    }
    
    // Could also poll CAN1 and CAN2 if needed
    if (can1Info.initialized) {
        while (can1.read(msg)) {
            processCANMessage(1, msg);
        }
    }
    
    if (can2Info.initialized) {
        while (can2.read(msg)) {
            processCANMessage(2, msg);
        }
    }
}