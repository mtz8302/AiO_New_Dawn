// CANManager.h - Manages CAN bus initialization and communication
#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <Arduino.h>
#include <FlexCAN_T4.h>

// Common CAN speeds
#define CAN_SPEED_250KBPS   250000
#define CAN_SPEED_500KBPS   500000
#define CAN_SPEED_1MBPS     1000000

// ISOBUS/J1939 speeds
#define CAN_SPEED_ISOBUS    250000  // Standard ISOBUS speed

// CAN message types
enum class CANMessageType {
    UNKNOWN,
    J1939,
    ISOBUS,
    STANDARD,
    EXTENDED
};

class CANManager {
public:
    // Bus status tracking
    struct CANBusInfo {
        bool initialized;
        uint32_t speed;
        uint32_t messagesReceived;
        uint32_t messagesSent;
        uint32_t errors;
        bool busOff;
        // Keya motor tracking for CAN3
        uint32_t keyaMotorMessages = 0;
        uint32_t lastKeyaMessageTime = 0;
    };

private:
    // CAN bus instances for Teensy 4.1
    FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1;
    FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> can2;
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> can3;  // Match NG-V6: TX_SIZE_256
    
    CANBusInfo can1Info;
    CANBusInfo can2Info;
    CANBusInfo can3Info;
    
    // Message handlers
    static void can1MessageHandler(const CAN_message_t &msg);
    static void can2MessageHandler(const CAN_message_t &msg);
    static void can3MessageHandler(const CAN_message_t &msg);
    
    // Process received message
    void processCANMessage(uint8_t busNum, const CAN_message_t &msg);
    
    // Identify message type
    CANMessageType identifyMessageType(const CAN_message_t &msg);
    const char* getMessageTypeName(CANMessageType type);
    
    // Instance pointer for callbacks
    static CANManager* instance;
    
public:
    CANManager();
    ~CANManager() = default;
    
    // Initialize CAN buses
    bool initializeCAN();
    bool initializeBus(uint8_t busNum, uint32_t speed = CAN_SPEED_250KBPS);
    
    // Enable/disable specific buses
    bool enableBus(uint8_t busNum);
    bool disableBus(uint8_t busNum);
    
    // Send messages
    bool sendMessage(uint8_t busNum, const CAN_message_t &msg);
    bool sendJ1939Message(uint8_t busNum, uint32_t pgn, uint8_t priority, 
                          uint8_t sourceAddr, const uint8_t* data, uint8_t len);
    
    // Bus management
    bool setBusSpeed(uint8_t busNum, uint32_t speed);
    bool setBusFilters(uint8_t busNum, uint32_t filterID, uint32_t filterMask);
    void clearBusFilters(uint8_t busNum);
    
    // Status and debugging
    void printCANStatus();
    void printBusStatus(uint8_t busNum);
    CANBusInfo* getBusInfo(uint8_t busNum);
    
    // Error handling
    bool isBusOff(uint8_t busNum);
    bool resetBus(uint8_t busNum);
    uint32_t getErrorCount(uint8_t busNum);
    
    // Getters for bus status
    bool isCAN1Initialized() const { return can1Info.initialized; }
    bool isCAN2Initialized() const { return can2Info.initialized; }
    bool isCAN3Initialized() const { return can3Info.initialized; }
    
    // Polling method for CAN messages (like NG-V6)
    void pollCANMessages();
    
    // Get FlexCAN objects for advanced use
    FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& getCAN1() { return can1; }
    FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>& getCAN2() { return can2; }
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256>& getCAN3() { return can3; }
};

// Global pointer following established pattern
extern CANManager* canPTR;

#endif // CAN_MANAGER_H