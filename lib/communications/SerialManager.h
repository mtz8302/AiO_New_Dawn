#ifndef SERIALMANAGER_H_
#define SERIALMANAGER_H_

#include "Arduino.h"

// Serial port definitions (self-contained, no pcb.h dependency)
#define SerialRTK Serial3
#define SerialGPS1 Serial5
#define SerialGPS2 Serial8
#define SerialRS232 Serial7
#define SerialESP32 Serial2
1
    // Baud rates (self-contained constants)
    const int32_t baudGPS = 460800;
const int32_t baudRTK = 115200;
const int32_t baudRS232 = 38400;
const int32_t baudESP32 = 460800;
const int32_t baudIMU = 115200;

// GPS type enumeration
enum class GPSType
{
    UNKNOWN = 0,
    F9P_SINGLE,   // u-blox F9P single antenna
    F9P_DUAL,     // u-blox F9P dual antenna (moving base)
    UM981,        // Unicore UM981 single antenna with IMU
    UM982_SINGLE, // Unicore UM982 single antenna
    UM982_DUAL,   // Unicore UM982 dual antenna
    GENERIC_NMEA  // Generic NMEA GPS
};

// IMU type enumeration
enum class IMUType
{
    NONE = 0,
    BNO085,           // BNO085 in RVC mode
    TM171,            // TM171 IMU
    CMPS14,           // CMPS14 compass
    UM981_INTEGRATED, // IMU integrated in UM981 GPS
    GENERIC           // Generic IMU
};

class SerialManager
{
private:
    static SerialManager *instance;
    bool isInitialized;

    // Private serial buffers (encapsulated, not global)
    uint8_t gps1RxBuffer[128];
    uint8_t gps1TxBuffer[256];
    uint8_t gps2RxBuffer[128];
    uint8_t gps2TxBuffer[256];
    uint8_t rtkRxBuffer[64];
    uint8_t rs232TxBuffer[256];
    uint8_t esp32RxBuffer[256];
    uint8_t esp32TxBuffer[256];

    // SerialIMU - owned by SerialManager
    HardwareSerial *serialIMU;

    // Bridge mode tracking
    bool prevUSB1DTR;
    bool prevUSB2DTR;

    // Detected device types
    GPSType detectedGPS1Type;
    GPSType detectedGPS2Type;
    IMUType detectedIMUType;

    // Detection timeouts (ms)
    static const uint32_t GPS_DETECT_TIMEOUT = 2000;
    static const uint32_t IMU_DETECT_TIMEOUT = 1000;

    // Detection helper methods
    GPSType detectGPSType(HardwareSerial &port, const char *portName);
    IMUType detectIMUType();
    int32_t detectGPSBaudRate(HardwareSerial &port, const char *portName);
    GPSType detectUnicoreGPS(int portNum); // New method for Unicore detection with buffer expansion
    bool sendAndWaitForResponse(HardwareSerial &port, const uint8_t *cmd, uint16_t cmdLen,
                                uint8_t *response, uint16_t &responseLen, uint32_t timeout);
    bool checkForNMEASentence(HardwareSerial &port, const char *sentenceType, uint32_t timeout);

    // Supported GPS baud rates for detection
    static const int32_t GPS_BAUD_RATES[];
    static const uint8_t NUM_GPS_BAUD_RATES;

public:
    // Buffer sizes (matching pcb.h values - using existing global buffers)
    static const uint16_t GPS_BUFFER_SIZE = 128;    // GPS1rxbuffer size from pcb.h
    static const uint16_t GPS_TX_BUFFER_SIZE = 256; // GPS1txbuffer size from pcb.h
    static const uint16_t RTK_BUFFER_SIZE = 64;
    static const uint16_t RS232_BUFFER_SIZE = 256;
    static const uint16_t ESP32_BUFFER_SIZE = 256;

    // Baud rates (matching pcb.h values)
    static const int32_t BAUD_GPS = 460800;
    static const int32_t BAUD_RTK = 115200;
    static const int32_t BAUD_RS232 = 38400;
    static const int32_t BAUD_ESP32 = 460800;
    static const int32_t BAUD_IMU = 115200;

    SerialManager();
    ~SerialManager();

    static SerialManager *getInstance();
    static void init();

    // Initialization
    bool initializeSerial();
    bool initializeSerialPorts();

    // Device detection
    void detectConnectedDevices();
    GPSType getGPS1Type() const { return detectedGPS1Type; }
    GPSType getGPS2Type() const { return detectedGPS2Type; }
    IMUType getIMUType() const { return detectedIMUType; }
    const char *getGPSTypeName(GPSType type) const;
    const char *getIMUTypeName(IMUType type) const;

    // Serial processing methods
    void processGPS1();
    void processGPS2();
    void processRTK();
    void processRS232();
    void processESP32();
    void processIMU();

    // Bridge mode management
    void updateBridgeMode();
    bool isGPS1Bridged() const;
    bool isGPS2Bridged() const;
    void handleGPS1BridgeMode();
    void handleGPS2BridgeMode();

    // Utility methods
    void clearSerialBuffers();
    void sendToRS232(uint8_t *data, uint16_t length);
    void sendToESP32(uint8_t *data, uint16_t length);

    // Baud rate getters
    int32_t getGPSBaudRate() const;
    int32_t getRTKBaudRate() const;
    int32_t getESP32BaudRate() const;
    int32_t getRS232BaudRate() const;
    int32_t getIMUBaudRate() const;

    // ESP32 PGN handling
    void processESP32PGN(uint8_t *data, uint8_t length);
    bool validatePGNHeader(uint8_t *data, uint8_t length);

    // Status and debug
    void printSerialStatus();
    void printSerialConfiguration();
    bool getInitializationStatus() const;
    bool isSerialInitialized() const;
};

// Global pointer (following the same pattern as configPTR)
extern SerialManager *serialPTR;

#endif // SERIALMANAGER_H_