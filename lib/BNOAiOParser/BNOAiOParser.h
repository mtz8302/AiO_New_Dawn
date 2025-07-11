#ifndef BNOAIOPARSER_H_
#define BNOAIOPARSER_H_

#include <Arduino.h>

// BNO085 RVC parser using state machine approach for AgOpenGPS
// Processes BNO085 serial RVC packets with minimal overhead
class BNOAiOParser
{
private:
    // State machine for packet parsing
    enum State
    {
        WAIT_HEADER1,
        WAIT_HEADER2,
        COLLECT_DATA,
        WAIT_CHECKSUM
    };

    // Constants
    static const uint8_t HEADER1 = 0xAA;
    static const uint8_t HEADER2 = 0xAA;
    static const uint8_t PACKET_SIZE = 19;     // Total packet size: 2 headers + 16 data + 1 checksum
    static const uint8_t DATA_SIZE = 18;       // Headers (2) + payload (16) before checksum
    static const uint8_t PAYLOAD_START = 2;   // After two header bytes
    
    // Parser state
    State state;
    uint8_t buffer[PACKET_SIZE];
    uint8_t bufferIndex;
    
    // Parsed data
    int16_t yawX10;       // Yaw in degrees x 10
    int16_t pitchX10;     // Pitch in degrees x 10
    int16_t rollX10;      // Roll in degrees x 10
    int16_t yawX100;      // Raw yaw value x 100 (for angular velocity)
    
    // Angular velocity tracking
    int16_t prevYaw;
    int16_t angVel;       // Running total of angular velocity
    uint8_t angCounter;
    
    // Timing
    uint32_t lastValidTime;
    bool dataValid;
    
    // Configuration
    bool isSwapXY;
    
    // Private methods
    void resetParser();
    bool validateChecksum();
    void parsePacket();
    
public:
    BNOAiOParser();
    
    // Main interface
    void processByte(uint8_t byte);
    
    // Data access
    float getYaw() const { return yawX10 / 10.0f; }
    float getPitch() const { return pitchX10 / 10.0f; }
    float getRoll() const { return rollX10 / 10.0f; }
    float getYawRate() const { return angVel / 10.0f; }
    int16_t getYawX10() const { return yawX10; }
    int16_t getPitchX10() const { return pitchX10; }
    int16_t getRollX10() const { return rollX10; }
    int16_t getAngVel() const { return angVel; }
    
    bool isDataValid() const { return dataValid && (millis() - lastValidTime < 100); }
    uint32_t getTimeSinceLastValid() const { return millis() - lastValidTime; }
    bool isActive() const { return isDataValid(); }
    
    // Configuration
    void setSwapXY(bool swap) { isSwapXY = swap; }
    
    // Debug
    void printDebug();
};

#endif // BNOAIOPARSER_H_