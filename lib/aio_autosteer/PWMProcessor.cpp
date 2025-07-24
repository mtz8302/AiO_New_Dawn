#include "PWMProcessor.h"
#include "EventLogger.h"
#include "HardwareManager.h"

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
    LOG_INFO(EventSource::AUTOSTEER, "=== PWM Processor Initialization ===");
    
    // Configure speed pulse pin as output
    pinMode(SPEED_PULSE_PIN, OUTPUT);
    
    // Start with output LOW (transistor OFF = output HIGH due to pull-up)
    // Remember: output is inverted through transistor
    digitalWrite(SPEED_PULSE_PIN, LOW);
    
    // Configure PWM through HardwareManager
    HardwareManager* hwMgr = HardwareManager::getInstance();
    
    // Request 12-bit resolution for speed pulse
    if (!hwMgr->requestPWMResolution(12, "PWMProcessor")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set PWM resolution to 12-bit");
        // Fall back to whatever resolution is set
    }
    
    // Request initial frequency
    if (!hwMgr->requestPWMFrequency(SPEED_PULSE_PIN, 100, "PWMProcessor")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set initial PWM frequency");
    }
    
    LOG_DEBUG(EventSource::AUTOSTEER, "Speed pulse pin (D33) configured");
    LOG_DEBUG(EventSource::AUTOSTEER, "PWM resolution: 12-bit");
    LOG_DEBUG(EventSource::AUTOSTEER, "Default frequency: 100Hz");
    LOG_DEBUG(EventSource::AUTOSTEER, "Output type: Open collector (inverted)");
    LOG_INFO(EventSource::AUTOSTEER, "PWM Processor initialization SUCCESS");
    
    return true;
}

void PWMProcessor::setSpeedPulseHz(float hz)
{
    if (hz < 0.0f) hz = 0.0f;
    if (hz > 10000.0f) hz = 10000.0f;  // Limit to 10kHz
    
    pulseFrequency = hz;
    
    if (hz > 0.0f) {
        HardwareManager* hwMgr = HardwareManager::getInstance();
        if (!hwMgr->requestPWMFrequency(SPEED_PULSE_PIN, (int)hz, "PWMProcessor")) {
            LOG_WARNING(EventSource::AUTOSTEER, "Failed to change PWM frequency to %dHz", (int)hz);
        }
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
    LOG_INFO(EventSource::AUTOSTEER, "=== PWM Processor Status ===");
    
    LOG_INFO(EventSource::AUTOSTEER, "Speed Pulse Output:");
    LOG_INFO(EventSource::AUTOSTEER, "  Enabled: %s", pulseEnabled ? "YES" : "NO");
    LOG_INFO(EventSource::AUTOSTEER, "  Frequency: %.1f Hz", pulseFrequency);
    LOG_INFO(EventSource::AUTOSTEER, "  Duty Cycle: %.1f%%", pulseDuty * 100.0f);
    LOG_INFO(EventSource::AUTOSTEER, "  Pin: D%d (open collector)", SPEED_PULSE_PIN);
    
    LOG_INFO(EventSource::AUTOSTEER, "Speed Settings:");
    LOG_INFO(EventSource::AUTOSTEER, "  Current Speed: %.1f km/h", currentSpeedKmh);
    LOG_INFO(EventSource::AUTOSTEER, "  Pulses/Meter: %.2f", pulsesPerMeter);
    LOG_INFO(EventSource::AUTOSTEER, "  Calculated Hz: %.1f", speedToFrequency(currentSpeedKmh));
    
    LOG_INFO(EventSource::AUTOSTEER, "=============================");
}