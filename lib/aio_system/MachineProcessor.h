#pragma once

// Re-enabling MachineProcessor - memory corruption was AgOpenGPS v6.7.1 bug
// #define MACHINE_PROCESSOR_DISABLED 1

#include <Arduino.h>
#include <stdint.h>

class MachineProcessor {
private:
    static MachineProcessor* instance;
    
    MachineProcessor();
    
    // Simplified state - only track section states from PGN 239
    struct SectionState {
        uint16_t currentStates;      // Current section states (bytes 6 & 7 from PGN 239)
        uint32_t lastPGN239Time;     // For watchdog timeout
        bool isOn[16];               // Individual section states for easy access
    } sectionState;
    
public:
    static MachineProcessor* getInstance();
    static bool init();
    
    bool initialize();
    void process();
    
    static void handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len);
    static void handlePGN239(uint8_t pgn, const uint8_t* data, size_t len);
    
    void updateSectionOutputs();
    
    // Section control helpers
    bool initializeSectionOutputs();
    void setPinHigh(uint8_t pin);
    void setPinLow(uint8_t pin);
    bool checkPCA9685();
};

extern MachineProcessor machineProcessor;

constexpr uint8_t MACHINE_SOURCE_ID = 0x7C;
constexpr uint8_t MACHINE_PGN_CONFIG = 0xEE;    // 238 - Machine Config from AOG
constexpr uint8_t MACHINE_PGN_DATA = 0xEF;      // 239 - Machine Data from AOG (includes sections)
constexpr uint8_t MACHINE_PGN_REPLY = 0xED;     // 237 - From Machine to AOG
constexpr uint8_t MACHINE_HELLO_REPLY = 0x7B;