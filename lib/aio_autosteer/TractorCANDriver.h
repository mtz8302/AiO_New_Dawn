// TractorCANDriver.h - Unified CAN driver for all tractor brands
#ifndef TRACTOR_CAN_DRIVER_H
#define TRACTOR_CAN_DRIVER_H

#include "MotorDriverInterface.h"
#include "CANGlobals.h"
#include "EventLogger.h"
#include "ConfigManager.h"
#include "ConfigGlobals.h"

// Tractor brands enumeration
enum class TractorBrand : uint8_t {
    DISABLED = 0,
    FENDT = 1,
    VALTRA_MASSEY = 2,
    CASEIH_NH = 3,
    FENDT_ONE = 4,
    CLAAS = 5,
    JCB = 6,
    LINDNER = 7,
    CAT_MT = 8,
    GENERIC = 9
};

class TractorCANDriver : public MotorDriverInterface {
private:
    // Configuration
    CANSteerConfig config;

    // CAN bus pointers (assigned based on config)
    // Using void* to handle different template instantiations
    void* steerCAN = nullptr;
    void* buttonCAN = nullptr;
    void* hitchCAN = nullptr;

    // Track which bus numbers are assigned
    uint8_t steerBusNum = 0;
    uint8_t buttonBusNum = 0;
    uint8_t hitchBusNum = 0;

    // Common state
    bool enabled = false;
    int16_t targetPWM = 0;
    bool steerReady = false;
    uint32_t lastSteerReadyTime = 0;
    uint32_t lastCommandTime = 0;

    // Keya-specific state (used when brand == KEYA)
    float actualRPM = 0.0f;
    float commandedRPM = 0.0f;
    uint16_t motorPosition = 0;
    uint16_t motorCurrent = 0;
    uint16_t motorErrorCode = 0;
    bool heartbeatValid = false;
    uint32_t lastHeartbeat = 0;

    // Command alternation for Keya
    enum CommandState {
        SEND_ENABLE,
        SEND_SPEED
    };
    CommandState nextCommand = SEND_ENABLE;

    // Massey K_Bus tracking
    uint8_t mfRollingCounter[8] = {0};  // Track last K_Bus message for F1/F2
    bool engageButtonPressed = false;   // Track K_Bus engage button

    // Fendt K_Bus tracking
    bool fendtButtonPressed = false;    // Track Fendt armrest button

    // Helper methods
    void assignCANBuses();
    void* getBusPointer(uint8_t busNum);
    bool readCANMessage(uint8_t busNum, CAN_message_t& msg);
    void writeCANMessage(uint8_t busNum, const CAN_message_t& msg);
    void processIncomingMessages();
    void sendSteerCommands();
    bool hasKeyaFunction() const;

    // Brand-specific message handlers
    void processKeyaMessage(const CAN_message_t& msg);
    void processFendtMessage(const CAN_message_t& msg);
    void processFendtKBusMessage(const CAN_message_t& msg);
    void processValtraMessage(const CAN_message_t& msg);
    void processMasseyKBusMessage(const CAN_message_t& msg);

    // Brand-specific command senders
    void sendKeyaCommands();
    void sendFendtCommands();
    void sendValtraCommands();
    void sendMasseyF1();
    void sendMasseyF2();

public:
    TractorCANDriver() {}

    bool init() override;
    void enable(bool en) override;
    void setPWM(int16_t pwm) override;
    void stop() override;
    void process() override;
    MotorStatus getStatus() const override;

    // Configuration
    void setConfig(const CANSteerConfig& newConfig);
    CANSteerConfig getConfig() const { return config; }

    // Motor type identification
    MotorDriverType getType() const override { return MotorDriverType::TRACTOR_CAN; }
    const char* getTypeName() const override;
    bool hasCurrentSensing() const override { return false; }
    bool hasPositionFeedback() const override {
        return hasKeyaFunction();
    }

    // Detection and safety
    bool isDetected() override { return steerReady; }
    void handleKickout(KickoutType type, float value) override;
    float getCurrentDraw() override { return 0.0f; }

    // Keya-specific methods (for compatibility)
    float getActualRPM() const { return actualRPM; }
    float getCommandedRPM() const { return commandedRPM; }
    uint16_t getMotorPosition() const { return motorPosition; }
    bool hasRPMFeedback() const {
        return hasKeyaFunction() && heartbeatValid;
    }

    // Massey-specific methods
    bool isEngageButtonPressed() const { return engageButtonPressed; }
    void pressMasseyF1() { sendMasseyF1(); }
    void pressMasseyF2() { sendMasseyF2(); }

    // Fendt-specific methods
    bool isFendtButtonPressed() const { return fendtButtonPressed; }
};

#endif // TRACTOR_CAN_DRIVER_H