#pragma once

// Re-enabling MachineProcessor - memory corruption was AgOpenGPS v6.7.1 bug
// #define MACHINE_PROCESSOR_DISABLED 1

#include <Arduino.h>
#include <stdint.h>

class MachineProcessor {
private:
    static MachineProcessor* instance;
    
    MachineProcessor();
    
    struct MachineConfig {
        uint8_t raiseTime = 2;
        uint8_t lowerTime = 4;
        bool enableHydraulicLift = false;
        uint8_t sectionCount = 8;
        uint8_t sectionWidths[16] = {0};
    } config;
    
    struct MachineState {
        uint16_t manualSwitches = 0;
        bool isLowered = true;
        uint32_t lastPGN238Time = 0;
        uint32_t lastPGN232Time = 0;
    } state;
    
    // Clear state representation for sections
    struct SectionState {
        bool isOn[16];           // True when section should be active
        uint16_t rawPGNData;     // Raw section state bits from PGN 239
        uint16_t autoStates;     // Auto/Manual state bits from PGN 239
        uint32_t lastUpdateTime;
    } sectionState;
    
public:
    static MachineProcessor* getInstance();
    static bool init();
    
    bool initialize();
    void process();
    
    static void handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len);
    static void handlePGN238(uint8_t pgn, const uint8_t* data, size_t len);
    static void handlePGN239(uint8_t pgn, const uint8_t* data, size_t len);
    
    void updateSectionOutputs();
    void printStatus();
    bool isActive() { return (millis() - state.lastPGN238Time) < 2000; }
    
    // Section control helpers
    bool initializeSectionOutputs();
    void setPinHigh(uint8_t pin);
    void setPinLow(uint8_t pin);
    void setPinPWM(uint8_t pin, uint16_t dutyCycle);
    bool checkPCA9685();
    void runSectionDiagnostics();
};

extern MachineProcessor machineProcessor;

constexpr uint8_t MACHINE_SOURCE_ID = 0x7C;
constexpr uint8_t MACHINE_PGN_CONFIG = 0xEE;    // 238 - Machine Config from AOG
constexpr uint8_t MACHINE_PGN_DATA = 0xEF;      // 239 - Machine Data from AOG (includes sections)
constexpr uint8_t MACHINE_PGN_REPLY = 0xED;     // 237 - From Machine to AOG
constexpr uint8_t MACHINE_HELLO_REPLY = 0x7B;