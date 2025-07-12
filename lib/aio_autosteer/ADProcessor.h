#ifndef ADPROCESSOR_H
#define ADPROCESSOR_H

#include <Arduino.h>
#include <ADC.h>

/**
 * ADProcessor - Analog/Digital Input Processor for Autosteer
 * 
 * Handles:
 * - WAS (Wheel Angle Sensor) reading from Teensy ADC
 *   Note: WAS outputs 0-5V but Teensy ADC max is 3.3V
 *   PCB has 10k/10k voltage divider (R46/R48) = 2:1 ratio
 *   0-5V sensor -> 0-2.5V ADC, 2.5V center -> 1.25V ADC
 *   ADC value at center = 1553 (1.25V/3.3V * 4095)
 * - Work switch input with debouncing
 * - Steer switch input with debouncing
 */
class ADProcessor {
public:
    ADProcessor();
    ~ADProcessor() = default;
    
    // Initialization
    bool init();
    
    // Main processing function - call regularly from loop
    void process();
    
    // Digital switch states (debounced)
    bool isWorkSwitchOn() const { return workSwitch.debouncedState; }
    bool isSteerSwitchOn() const { return steerSwitch.debouncedState; }
    bool hasWorkSwitchChanged() const { return workSwitch.hasChanged; }
    bool hasSteerSwitchChanged() const { return steerSwitch.hasChanged; }
    
    // Clear change flags after reading
    void clearWorkSwitchChange() { workSwitch.hasChanged = false; }
    void clearSteerSwitchChange() { steerSwitch.hasChanged = false; }
    
    // WAS readings (Teensy ADC only)
    int16_t getWASRaw() const { return wasRaw; }
    float getWASAngle() const;
    float getWASVoltage() const;
    
    // Kickout sensor readings
    uint16_t getKickoutAnalog() const { return kickoutAnalogRaw; }
    float getPressureReading() const { return pressureReading; }
    uint16_t getMotorCurrent() const { return motorCurrentRaw; }
    
    // Configuration
    void setWASOffset(int16_t offset) { wasOffset = offset; }
    void setWASCountsPerDegree(float counts) { wasCountsPerDegree = counts; }
    void setDebounceTime(uint16_t ms) { debounceDelay = ms; }
    
    // Get configuration
    int16_t getWASOffset() const { return wasOffset; }
    float getWASCountsPerDegree() const { return wasCountsPerDegree; }
    
    // Diagnostics
    void printStatus() const;
    
    // Static instance for singleton pattern
    static ADProcessor* instance;
    static ADProcessor* getInstance();

private:
    // Pin assignments from pcb.h
    static constexpr uint8_t AD_STEER_PIN = 2;         // Steer switch input (STEER_PIN from pcb.h)
    static constexpr uint8_t AD_WORK_PIN = A17;        // Work switch input (WORK_PIN from pcb.h)  
    static constexpr uint8_t AD_WAS_PIN = A15;         // WAS sensor input
    static constexpr uint8_t AD_KICKOUT_A_PIN = A12;   // Pressure sensor input (KICKOUT_A from pcb.h)
    static constexpr uint8_t AD_CURRENT_PIN = A13;     // Motor current sensor (CURRENT_PIN from pcb.h)
    
    // Switch debouncing structure
    struct SwitchState {
        bool currentState;
        bool debouncedState;
        uint32_t lastChangeTime;
        bool hasChanged;
    };
    
    SwitchState workSwitch;
    SwitchState steerSwitch;
    
    // WAS data
    int16_t wasRaw;
    int16_t wasOffset;
    float wasCountsPerDegree;
    
    // Kickout sensor data
    uint16_t kickoutAnalogRaw;
    float pressureReading;  // Filtered pressure sensor reading
    uint16_t motorCurrentRaw;
    
    // Configuration
    uint16_t debounceDelay;
    
    // Timing
    uint32_t lastProcessTime;
    
    // Teensy ADC object
    ADC* teensyADC;
    
    // Helper methods
    void updateSwitches();
    void updateWAS();
    bool debounceSwitch(SwitchState& sw, bool rawState);
};

#endif // ADPROCESSOR_H