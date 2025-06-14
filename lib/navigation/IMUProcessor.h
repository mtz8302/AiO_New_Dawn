#ifndef IMUPROCESSOR_H_
#define IMUPROCESSOR_H_

#include "Arduino.h"
#include "SerialManager.h"
#include "BNO_RVC.h"
#include "TM171MinimalParser.h"
#include "elapsedMillis.h"

// IMU data structure
struct IMUData
{
    float heading;      // degrees (0-360)
    float roll;         // degrees
    float pitch;        // degrees
    float yawRate;      // degrees/second
    uint8_t quality;    // 0-10 quality indicator
    uint32_t timestamp; // millis() when data was received
    bool isValid;       // data validity flag
};

// IMU Processor class
class IMUProcessor
{
private:
    static IMUProcessor *instance;
    SerialManager *serialMgr;
    IMUType detectedType;
    bool isInitialized;

    // BNO085 RVC support
    BNO_RVC *bno;
    HardwareSerial *imuSerial;

    // TM171 support
    TM171MinimalParser *tm171Parser;

    // Latest IMU data
    IMUData currentData;

    // Statistics
    uint32_t packetsReceived;
    uint32_t packetsErrors;
    elapsedMillis timeSinceLastPacket;

    // Private methods
    bool initBNO085();
    bool initTM171();
    void processBNO085Data();
    void processTM171Data();

public:
    IMUProcessor();
    ~IMUProcessor();

    // Singleton access
    static IMUProcessor *getInstance();
    static void init();

    // Main interface
    bool initialize();
    void process();
    bool isActive() const { return isInitialized && timeSinceLastPacket < 100; }

    // Data access
    IMUData getCurrentData() const { return currentData; }
    bool hasValidData() const { return currentData.isValid; }

    // Info and stats
    IMUType getIMUType() const { return detectedType; }
    const char *getIMUTypeName() const;
    uint32_t getPacketCount() const { return packetsReceived; }
    uint32_t getErrorCount() const { return packetsErrors; }

    // Debug
    void printStatus();
    void printCurrentData();
};

// Global pointer
extern IMUProcessor *imuPTR;

#endif // IMUPROCESSOR_H_