#ifndef TURN_SENSOR_TYPES_H
#define TURN_SENSOR_TYPES_H

#include <Arduino.h>

// Turn sensor types matching AgOpenGPS
enum class TurnSensorType : uint8_t {
    NONE = 0,
    ENCODER = 1,      // Rotary encoder (single or quadrature)
    PRESSURE = 2,     // Hydraulic pressure sensor
    CURRENT = 3       // Motor current sensor
};

// Encoder types
enum class EncoderType : uint8_t {
    SINGLE = 1,       // Single channel encoder
    QUADRATURE = 2    // Quadrature (dual channel) encoder
};

// Turn sensor configuration
struct TurnSensorConfig {
    TurnSensorType sensorType = TurnSensorType::NONE;
    EncoderType encoderType = EncoderType::SINGLE;
    uint8_t maxPulseCount = 5;        // Maximum pulse count before kickout
    uint8_t pressureThreshold = 100;  // Pressure sensor threshold (0-255)
    uint8_t currentThreshold = 100;   // Current sensor threshold (0-255)
    uint16_t currentZeroOffset = 90;  // Zero current offset for calibration
    
    // Constructor with defaults
    TurnSensorConfig() = default;
};

#endif // TURN_SENSOR_TYPES_H