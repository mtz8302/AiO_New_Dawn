#ifndef LITTLEDAWNINTERFACE_H
#define LITTLEDAWNINTERFACE_H

#include <Arduino.h>
#include "EventLogger.h"

// Message IDs
#define MSG_MACHINE_STATUS 0x01

// Machine status structure (must match Little Dawn)
struct MachineStatus {
    int16_t speed;        // Speed in 0.01 km/h
    int16_t heading;      // Heading in 0.1 degrees
    int16_t roll;         // Roll in 0.1 degrees
    int16_t pitch;        // Pitch in 0.1 degrees
    int16_t steerAngle;   // WAS - Steer angle in 0.1 degrees
} __attribute__((packed));

/**
 * LittleDawnInterface - Serial communication interface for Little Dawn ESP32
 * 
 * Sends machine status data to Little Dawn ISOBUS co-processor via Serial2
 * Data is sent every 100ms with a simple checksum protocol
 */
class LittleDawnInterface {
private:    
    // Timing
    uint32_t lastTransmitTime = 0;
    static constexpr uint32_t TRANSMIT_INTERVAL_MS = 100;  // 100ms = 10Hz
    
    // Serial configuration
    static constexpr uint32_t BAUD_RATE = 460800;  // Must match SerialManager BAUD_ESP32
    
    // Helper methods
    uint8_t calculateChecksum(const uint8_t* data, size_t len);
    void sendToLittleDawn(uint8_t id, const uint8_t* data, uint8_t length);
    void sendMachineStatus();
    
public:
    LittleDawnInterface() = default;
    ~LittleDawnInterface() = default;
    
    // Main interface
    void init();
    void process();  // Called from main loop
    
    // Status
    bool isActive() const;
    void printStatus();
};

// Global instance
extern LittleDawnInterface littleDawnInterface;

#endif // LITTLEDAWNINTERFACE_H