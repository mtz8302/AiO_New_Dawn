#include "ADProcessor.h"

// Static instance
ADProcessor* ADProcessor::instance = nullptr;

ADProcessor::ADProcessor() : 
    wasRaw(0),
    wasOffset(0),
    wasCountsPerDegree(1.0f),
    kickoutAnalogRaw(0),
    motorCurrentRaw(0),
    debounceDelay(50),  // 50ms default debounce
    lastProcessTime(0)
{
    // Initialize switch states
    workSwitch = {false, false, 0, false};
    steerSwitch = {false, false, 0, false};
    
    instance = this;
}

ADProcessor* ADProcessor::getInstance()
{
    if (instance == nullptr) {
        instance = new ADProcessor();
    }
    return instance;
}

bool ADProcessor::init()
{
    Serial.print("\r\n=== A/D Processor Initialization ===");
    
    // Configure pins
    pinMode(AD_STEER_PIN, INPUT_PULLUP);      // Steer switch with internal pullup
    pinMode(AD_WORK_PIN, INPUT_PULLUP);       // Work switch with pullup
    pinMode(AD_WAS_PIN, INPUT_DISABLE);       // WAS analog input (no pullup)
    pinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE); // Pressure sensor analog input
    pinMode(AD_CURRENT_PIN, INPUT_DISABLE);   // Current sensor analog input
    
    // Test immediately after setting
    Serial.printf("\r\n- After pinMode: Pin %d digital=%d", AD_STEER_PIN, digitalRead(AD_STEER_PIN));
    
    // Configure ADC for 12-bit resolution with averaging
    analogReadResolution(12);              // 12-bit (0-4095)
    analogReadAveraging(16);               // Average 16 samples
    
    // Take initial readings
    updateWAS();
    updateSwitches();
    
    // Clear any initial change flags
    workSwitch.hasChanged = false;
    steerSwitch.hasChanged = false;
    
    Serial.print("\r\n- Pin configuration complete");
    Serial.printf("\r\n- Initial WAS reading: %d (%.2fV)", wasRaw, getWASVoltage());
    Serial.printf("\r\n- Work switch: %s (pin A17)", workSwitch.debouncedState ? "ON" : "OFF");
    Serial.printf("\r\n- Steer switch: %s (pin %d)", steerSwitch.debouncedState ? "ON" : "OFF", AD_STEER_PIN);
    
    Serial.print("\r\n- A/D Processor initialization SUCCESS\r\n");
    
    return true;
}

void ADProcessor::process()
{
    updateWAS();
    updateSwitches();
    
    // Read kickout sensors
    kickoutAnalogRaw = analogRead(AD_KICKOUT_A_PIN);
    motorCurrentRaw = analogRead(AD_CURRENT_PIN);
    
    // Debug pin 2 periodically
    static uint32_t lastDebugTime = 0;
    
    if (millis() - lastDebugTime > 1000) {
        int pin2Digital = digitalRead(AD_STEER_PIN);
        Serial.printf("\r\n[AD] Pin %d: digital=%d", AD_STEER_PIN, pin2Digital);
        lastDebugTime = millis();
    }
    
    lastProcessTime = millis();
}

void ADProcessor::updateWAS()
{
    // Read WAS with hardware averaging (16 samples)
    wasRaw = analogRead(AD_WAS_PIN);
}

void ADProcessor::updateSwitches()
{
    // Simple digital read - just like old firmware
    int steerPinRaw = digitalRead(AD_STEER_PIN);
    int workPinRaw = digitalRead(AD_WORK_PIN);
    
    // Convert to active states
    bool workRaw = !workPinRaw;     // Work is active LOW (pressed = 0)
    bool steerRaw = !steerPinRaw;   // Steer is active LOW (pressed pulls down)
    
    // Debug raw pin state changes
    static int lastSteerPinRaw = -1;
    if (steerPinRaw != lastSteerPinRaw) {
        Serial.printf("\r\n[AD] Steer pin %d: digital=%d, active=%d", 
                      AD_STEER_PIN, steerPinRaw, steerRaw);
        lastSteerPinRaw = steerPinRaw;
    }
    
    // Apply debouncing
    if (debounceSwitch(workSwitch, workRaw)) {
        workSwitch.hasChanged = true;
    }
    
    if (debounceSwitch(steerSwitch, steerRaw)) {
        steerSwitch.hasChanged = true;
        Serial.printf("\r\n[AD] Steer switch debounced: %s", 
                      steerSwitch.debouncedState ? "ON" : "OFF");
    }
}

bool ADProcessor::debounceSwitch(SwitchState& sw, bool rawState)
{
    bool stateChanged = false;
    
    if (rawState != sw.currentState) {
        // Raw state changed, update tracking
        sw.currentState = rawState;
        sw.lastChangeTime = millis();
    }
    else if (sw.currentState != sw.debouncedState) {
        // Check if state has been stable long enough
        if ((millis() - sw.lastChangeTime) >= debounceDelay) {
            sw.debouncedState = sw.currentState;
            stateChanged = true;
        }
    }
    
    return stateChanged;
}

float ADProcessor::getWASAngle() const
{
    // Calculate angle from raw reading
    // Angle = (raw - offset) / countsPerDegree
    if (wasCountsPerDegree != 0) {
        return (float)(wasRaw - wasOffset) / wasCountsPerDegree;
    }
    return 0.0f;
}

float ADProcessor::getWASVoltage() const
{
    // Convert 12-bit ADC reading to actual sensor voltage
    // The PCB has a 10k/10k voltage divider (R46/R48)
    // This divides by 2: 0-5V sensor -> 0-2.5V ADC
    // ADC voltage = (wasRaw * 3.3V) / 4095
    // Sensor voltage = ADC voltage * 2
    float adcVoltage = (wasRaw * 3.3f) / 4095.0f;
    return adcVoltage * 2.0f;  // Account for voltage divider
}

void ADProcessor::printStatus() const
{
    Serial.print("\r\n\r\n=== A/D Processor Status ===");
    
    // WAS information
    Serial.print("\r\nWAS (Wheel Angle Sensor):");
    Serial.printf("\r\n  Raw ADC: %d", wasRaw);
    Serial.printf("\r\n  Voltage: %.3fV", getWASVoltage());
    Serial.printf("\r\n  Angle: %.2f°", getWASAngle());
    Serial.printf("\r\n  Offset: %d", wasOffset);
    Serial.printf("\r\n  Counts/Degree: %.2f", wasCountsPerDegree);
    
    // Switch states
    Serial.print("\r\n\r\nSwitches:");
    Serial.printf("\r\n  Work: %s%s", 
                  workSwitch.debouncedState ? "ON" : "OFF",
                  workSwitch.hasChanged ? " (changed)" : "");
    Serial.printf("\r\n  Steer: %s%s", 
                  steerSwitch.debouncedState ? "ON" : "OFF",
                  steerSwitch.hasChanged ? " (changed)" : "");
    
    // Configuration
    Serial.print("\r\n\r\nConfiguration:");
    Serial.printf("\r\n  Debounce delay: %dms", debounceDelay);
    Serial.printf("\r\n  ADC resolution: 12-bit");
    Serial.printf("\r\n  ADC averaging: 16 samples");
    
    Serial.print("\r\n=============================\r\n");
}