#include "PWMProcessor.h"

// Static instance
PWMProcessor* PWMProcessor::instance = nullptr;

PWMProcessor::PWMProcessor() : 
    pulseFrequency(0.0f),
    pulseDuty(0.5f),        // 50% duty cycle default
    pulseEnabled(false),
    currentSpeedKmh(0.0f),
    pulsesPerMeter(1.0f)    // Default 1 pulse per meter
{
    instance = this;
}

PWMProcessor* PWMProcessor::getInstance()
{
    if (instance == nullptr) {
        instance = new PWMProcessor();
    }
    return instance;
}

bool PWMProcessor::init()
{
    Serial.print("\r\n=== PWM Processor Initialization ===");
    
    // Configure speed pulse pin as output
    pinMode(SPEED_PULSE_PIN, OUTPUT);
    
    // Start with output LOW (transistor OFF = output HIGH due to pull-up)
    // Remember: output is inverted through transistor
    digitalWrite(SPEED_PULSE_PIN, LOW);
    
    // Configure PWM
    // Teensy 4.1 PWM is very flexible
    // analogWriteFrequency affects all pins on the same timer
    // Pin 36 is on FlexPWM2_3_A
    analogWriteFrequency(SPEED_PULSE_PIN, 100);  // Start at 100Hz
    analogWriteResolution(12);  // 12-bit resolution (0-4095)
    
    Serial.print("\r\n- Speed pulse pin (D33) configured");
    Serial.print("\r\n- PWM resolution: 12-bit");
    Serial.print("\r\n- Default frequency: 100Hz");
    Serial.print("\r\n- Output type: Open collector (inverted)");
    Serial.print("\r\n- PWM Processor initialization SUCCESS\r\n");
    
    return true;
}

void PWMProcessor::setSpeedPulseHz(float hz)
{
    if (hz < 0.0f) hz = 0.0f;
    if (hz > 10000.0f) hz = 10000.0f;  // Limit to 10kHz
    
    pulseFrequency = hz;
    
    if (hz > 0.0f) {
        analogWriteFrequency(SPEED_PULSE_PIN, (int)hz);
    }
    
    updatePWM();
}

void PWMProcessor::setSpeedPulseDuty(float duty)
{
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;
    
    pulseDuty = duty;
    updatePWM();
}

void PWMProcessor::enableSpeedPulse(bool enable)
{
    pulseEnabled = enable;
    updatePWM();
}

void PWMProcessor::setSpeedKmh(float speedKmh)
{
    if (speedKmh < 0.0f) speedKmh = 0.0f;
    
    currentSpeedKmh = speedKmh;
    
    // Convert speed to pulse frequency
    float hz = speedToFrequency(speedKmh);
    setSpeedPulseHz(hz);
}

void PWMProcessor::setPulsesPerMeter(float ppm)
{
    if (ppm <= 0.0f) ppm = 1.0f;  // Minimum 1 pulse per meter
    
    pulsesPerMeter = ppm;
    
    // Update frequency if we have a speed set
    if (currentSpeedKmh > 0.0f) {
        float hz = speedToFrequency(currentSpeedKmh);
        setSpeedPulseHz(hz);
    }
}

void PWMProcessor::updatePWM()
{
    if (pulseEnabled && pulseFrequency > 0.0f) {
        // Calculate PWM value (12-bit: 0-4095)
        // Note: Output is inverted through transistor
        // HIGH PWM = transistor ON = output LOW
        // LOW PWM = transistor OFF = output HIGH (pull-up)
        // So we invert the duty cycle
        int pwmValue = (int)((1.0f - pulseDuty) * 4095.0f);
        analogWrite(SPEED_PULSE_PIN, pwmValue);
    } else {
        // Disable PWM - set output LOW (transistor OFF = output HIGH)
        digitalWrite(SPEED_PULSE_PIN, LOW);
    }
}

float PWMProcessor::speedToFrequency(float speedKmh) const
{
    // Convert km/h to m/s
    float speedMs = speedKmh / 3.6f;
    
    // Calculate pulse frequency
    // frequency (Hz) = speed (m/s) * pulses per meter
    float hz = speedMs * pulsesPerMeter;
    
    return hz;
}

void PWMProcessor::printStatus() const
{
    Serial.print("\r\n\r\n=== PWM Processor Status ===");
    
    Serial.print("\r\nSpeed Pulse Output:");
    Serial.printf("\r\n  Enabled: %s", pulseEnabled ? "YES" : "NO");
    Serial.printf("\r\n  Frequency: %.1f Hz", pulseFrequency);
    Serial.printf("\r\n  Duty Cycle: %.1f%%", pulseDuty * 100.0f);
    Serial.printf("\r\n  Pin: D%d (open collector)", SPEED_PULSE_PIN);
    
    Serial.print("\r\n\r\nSpeed Settings:");
    Serial.printf("\r\n  Current Speed: %.1f km/h", currentSpeedKmh);
    Serial.printf("\r\n  Pulses/Meter: %.2f", pulsesPerMeter);
    Serial.printf("\r\n  Calculated Hz: %.1f", speedToFrequency(currentSpeedKmh));
    
    Serial.print("\r\n=============================\r\n");
}