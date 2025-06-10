#include "SerialManager.h"

// Global instance pointer
SerialManager *serialPTR = nullptr;

// Static instance pointer
SerialManager *SerialManager::instance = nullptr;

SerialManager::SerialManager()
    : isInitialized(false), serialIMU(&Serial4), prevUSB1DTR(false), prevUSB2DTR(false)
{
    instance = this;
    serialPTR = this;
}

SerialManager::~SerialManager()
{
    instance = nullptr;
    serialPTR = nullptr;
}

SerialManager *SerialManager::getInstance()
{
    return instance;
}

void SerialManager::init()
{
    if (instance == nullptr)
    {
        new SerialManager();
    }
}

bool SerialManager::initializeSerial()
{
    Serial.print("\r\n=== Serial Manager Initialization ===");

    if (!initializeSerialPorts())
    {
        Serial.print("\r\n** Serial port initialization FAILED **");
        return false;
    }

    isInitialized = true;
    Serial.print("\r\n- Serial initialization SUCCESS");
    return true;
}

bool SerialManager::initializeSerialPorts()
{
    Serial.print("\r\n- Initializing serial ports");

    // GPS1 Serial - use class member buffers
    SerialGPS1.begin(BAUD_GPS);
    SerialGPS1.addMemoryForRead(gps1RxBuffer, sizeof(gps1RxBuffer));
    SerialGPS1.addMemoryForWrite(gps1TxBuffer, sizeof(gps1TxBuffer));

    // GPS2 Serial - use class member buffers
    SerialGPS2.begin(BAUD_GPS);
    SerialGPS2.addMemoryForRead(gps2RxBuffer, sizeof(gps2RxBuffer));
    SerialGPS2.addMemoryForWrite(gps2TxBuffer, sizeof(gps2TxBuffer));

    // RTK Radio Serial - use class member buffer
    SerialRTK.begin(BAUD_RTK);
    SerialRTK.addMemoryForRead(rtkRxBuffer, sizeof(rtkRxBuffer));

    // RS232 Serial - use class member buffer
    SerialRS232.begin(BAUD_RS232);
    SerialRS232.addMemoryForWrite(rs232TxBuffer, sizeof(rs232TxBuffer));

    // ESP32 Serial - use class member buffers
    SerialESP32.begin(BAUD_ESP32);
    SerialESP32.addMemoryForRead(esp32RxBuffer, sizeof(esp32RxBuffer));
    SerialESP32.addMemoryForWrite(esp32TxBuffer, sizeof(esp32TxBuffer));

    // IMU Serial
    serialIMU->begin(BAUD_IMU);

    Serial.printf("\r\n  - SerialGPS1/GPS2: %i baud", BAUD_GPS);
    Serial.printf("\r\n  - SerialRTK: %i baud", BAUD_RTK);
    Serial.printf("\r\n  - SerialRS232: %i baud", BAUD_RS232);
    Serial.printf("\r\n  - SerialESP32: %i baud", BAUD_ESP32);
    Serial.printf("\r\n  - SerialIMU: %i baud", BAUD_IMU);

    return true;
}

void SerialManager::processGPS1()
{
    // Basic GPS1 processing without external dependencies
    if (!isGPS1Bridged())
    {
        uint16_t gps1Available = SerialGPS1.available();
        if (gps1Available)
        {
            if (gps1Available > sizeof(gps1RxBuffer) - 10)
            {
                SerialGPS1.clear();
                Serial.printf("\r\n%i *SerialGPS1 buffer cleared!*", millis());
                return;
            }

            uint8_t gps1Read = SerialGPS1.read();

            // Basic NMEA forwarding to RS232
            SerialRS232.write(gps1Read);
        }
    }
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
    else
    {
        handleGPS1BridgeMode();
    }
#endif
}

void SerialManager::processGPS2()
{
    // Basic GPS2 processing without external dependencies
    if (!isGPS2Bridged())
    {
        uint16_t gps2Available = SerialGPS2.available();
        if (gps2Available)
        {
            if (gps2Available > sizeof(gps2RxBuffer) - 10)
            {
                SerialGPS2.clear();
                Serial.printf("\r\n%i *SerialGPS2 buffer cleared!*", millis());
                return;
            }

            uint8_t gps2Read = SerialGPS2.read();
            // Basic processing - can be enhanced later
            (void)gps2Read; // Suppress unused variable warning
        }
    }
#if defined(USB_TRIPLE_SERIAL)
    else
    {
        handleGPS2BridgeMode();
    }
#endif
}

void SerialManager::processRTK()
{
    // Forward RTK data to GPS1
    if (SerialRTK.available())
    {
        uint8_t rtcmByte = SerialRTK.read();
        if (!isGPS1Bridged())
        {
            SerialGPS1.write(rtcmByte);
        }
    }
}

void SerialManager::processRS232()
{
    // RS232 processing - placeholder for future implementation
}

void SerialManager::processESP32()
{
    // ESP32 PGN processing
    if (SerialESP32.available())
    {
        static uint8_t incomingBytes[50];
        static uint8_t incomingIndex = 0;

        incomingBytes[incomingIndex] = SerialESP32.read();
        incomingIndex++;

        // Check for CR/LF termination
        if (incomingIndex >= 2 &&
            incomingBytes[incomingIndex - 2] == 13 &&
            incomingBytes[incomingIndex - 1] == 10)
        {
            if (validatePGNHeader(incomingBytes, incomingIndex - 2))
            {
                processESP32PGN(incomingBytes, incomingIndex - 2);
            }
            else
            {
                Serial.print("\r\n*** ESP32 invalid PGN header ***");
            }
            incomingIndex = 0;
        }

        // Prevent buffer overflow
        if (incomingIndex >= sizeof(incomingBytes))
        {
            incomingIndex = 0;
        }
    }
}

void SerialManager::processIMU()
{
    // IMU processing - placeholder for future implementation
    if (serialIMU->available())
    {
        uint8_t imuByte = serialIMU->read();
        // Process IMU data here
        (void)imuByte; // Suppress unused variable warning
    }
}

void SerialManager::updateBridgeMode()
{
    // Bridge mode detection - simplified for now
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
    // Update bridge mode status based on USB DTR lines
    // This will be enhanced when USB bridge functionality is needed
#endif
}

bool SerialManager::isGPS1Bridged() const
{
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
    // Return USB1DTR status when available
    return false; // Placeholder
#else
    return false;
#endif
}

bool SerialManager::isGPS2Bridged() const
{
#if defined(USB_TRIPLE_SERIAL)
    // Return USB2DTR status when available
    return false; // Placeholder
#else
    return false;
#endif
}

void SerialManager::handleGPS1BridgeMode()
{
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
    // Bridge GPS1 to USB1 for u-center access
    if (SerialGPS1.available())
    {
        while (SerialGPS1.available())
        {
            SerialUSB1.write(SerialGPS1.read());
        }
    }

    if (SerialUSB1.available())
    {
        while (SerialUSB1.available())
        {
            SerialGPS1.write(SerialUSB1.read());
        }
    }
#endif
}

void SerialManager::handleGPS2BridgeMode()
{
#if defined(USB_TRIPLE_SERIAL)
    // Bridge GPS2 to USB2 for u-center access
    if (SerialGPS2.available())
    {
        while (SerialGPS2.available())
        {
            SerialUSB2.write(SerialGPS2.read());
        }
    }

    if (SerialUSB2.available())
    {
        while (SerialUSB2.available())
        {
            SerialGPS2.write(SerialUSB2.read());
        }
    }
#endif
}

void SerialManager::clearSerialBuffers()
{
    SerialGPS1.clear();
    SerialGPS2.clear();
    SerialESP32.clear();
}

void SerialManager::sendToRS232(uint8_t *data, uint16_t length)
{
    SerialRS232.write(data, length);
}

void SerialManager::sendToESP32(uint8_t *data, uint16_t length)
{
    SerialESP32.write(data, length);
}

void SerialManager::processESP32PGN(uint8_t *data, uint8_t length)
{
    // ESP32 PGN processing - placeholder for network forwarding
    // This will send PGN data to AgIO via UDP when network is available
    Serial.print("\r\nESP32 PGN received, length: ");
    Serial.print(length);
}

bool SerialManager::validatePGNHeader(uint8_t *data, uint8_t length)
{
    if (length < 2)
        return false;

    return (data[0] == 128 && data[1] == 129);
}

int32_t SerialManager::getGPSBaudRate() const
{
    return BAUD_GPS;
}

int32_t SerialManager::getRTKBaudRate() const
{
    return BAUD_RTK;
}

int32_t SerialManager::getESP32BaudRate() const
{
    return BAUD_ESP32;
}

int32_t SerialManager::getRS232BaudRate() const
{
    return BAUD_RS232;
}

int32_t SerialManager::getIMUBaudRate() const
{
    return BAUD_IMU;
}

void SerialManager::printSerialStatus()
{
    Serial.print("\r\n\n=== Serial Manager Status ===");
    Serial.printf("\r\nInitialized: %s", isInitialized ? "YES" : "NO");
    Serial.printf("\r\nGPS1 Bridged: %s", isGPS1Bridged() ? "YES" : "NO");
    Serial.printf("\r\nGPS2 Bridged: %s", isGPS2Bridged() ? "YES" : "NO");

    printSerialConfiguration();
    Serial.print("\r\n=============================\r\n");
}

void SerialManager::printSerialConfiguration()
{
    Serial.print("\r\n\n--- Serial Configuration ---");
    Serial.printf("\r\nSerialGPS1 (Serial5): %i baud", BAUD_GPS);
    Serial.printf("\r\nSerialGPS2 (Serial8): %i baud", BAUD_GPS);
    Serial.printf("\r\nSerialRTK (Serial3): %i baud", BAUD_RTK);
    Serial.printf("\r\nSerialRS232 (Serial7): %i baud", BAUD_RS232);
    Serial.printf("\r\nSerialESP32 (Serial2): %i baud", BAUD_ESP32);
    Serial.printf("\r\nSerialIMU (Serial4): %i baud", BAUD_IMU);
}

bool SerialManager::getInitializationStatus() const
{
    return isInitialized;
}

bool SerialManager::isSerialInitialized() const
{
    return isInitialized;
}