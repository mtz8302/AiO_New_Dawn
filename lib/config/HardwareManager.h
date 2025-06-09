#ifndef HARDWAREMANAGER_H_
#define HARDWAREMANAGER_H_

#include "Arduino.h"
#include "HardwareSerial.h"

class HardwareManager
{
private:
    static HardwareManager *instance;

    // Serial port configurations
    static const int32_t BAUD_GPS = 460800;
    static const int32_t BAUD_RTK = 115200;
    static const int32_t BAUD_RS232 = 38400;
    static const int32_t BAUD_ESP32 = 460800;
    static const int32_t BAUD_IMU = 115200;

    // Buffer sizes
    static const size_t GPS_BUFFER_SIZE = 384;
    static const size_t RTK_BUFFER_SIZE = 64;
    static const size_t RS232_BUFFER_SIZE = 256;
    static const size_t ESP32_BUFFER_SIZE = 256;

    // Hardware state
    bool isInitialized;
    uint8_t pwmFrequencyMode;

    // Buffer allocations
    uint8_t *gps1RxBuffer;
    uint8_t *gps1TxBuffer;
    uint8_t *gps2RxBuffer;
    uint8_t *gps2TxBuffer;
    uint8_t *rtkRxBuffer;
    uint8_t *rs232TxBuffer;
    uint8_t *esp32RxBuffer;
    uint8_t *esp32TxBuffer;

public:
    HardwareManager();
    ~HardwareManager();

    // Singleton access
    static HardwareManager *getInstance();
    static void init();

    // Hardware initialization
    bool initializeHardware();
    bool initializePins();
    bool initializeSerial();
    bool initializePWM();
    bool initializeADC();

    // Pin access methods - use the #define values from pcb.h
    uint8_t getWASSensorPin() const;
    uint8_t getSpeedPulsePin() const;
    uint8_t getSpeedPulse10Pin() const;
    uint8_t getBuzzerPin() const;
    uint8_t getSleepPin() const;
    uint8_t getPWM1Pin() const;
    uint8_t getPWM2Pin() const;
    uint8_t getSteerPin() const;
    uint8_t getWorkPin() const;
    uint8_t getKickoutDPin() const;
    uint8_t getCurrentPin() const;
    uint8_t getKickoutAPin() const;

    // Baud rate access methods
    int32_t getGPSBaudRate() const { return BAUD_GPS; }
    int32_t getRTKBaudRate() const { return BAUD_RTK; }
    int32_t getRS232BaudRate() const { return BAUD_RS232; }
    int32_t getESP32BaudRate() const { return BAUD_ESP32; }
    int32_t getIMUBaudRate() const { return BAUD_IMU; }

    // PWM management
    bool setPWMFrequency(uint8_t mode);
    uint8_t getPWMFrequencyMode() const { return pwmFrequencyMode; }

    // Hardware control
    void enableBuzzer();
    void disableBuzzer();
    void enableSteerMotor();
    void disableSteerMotor();

    // Status
    bool isHardwareInitialized() const { return isInitialized; }

    // Diagnostic methods
    void printHardwareStatus();
    void printPinConfiguration();
    void printSerialConfiguration();

private:
    bool allocateSerialBuffers();
    void deallocateSerialBuffers();
    void configureAnalogPins();
    void configureDigitalPins();
    void configurePWMPins();
};

#endif // HARDWAREMANAGER_H_