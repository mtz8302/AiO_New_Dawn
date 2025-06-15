#include "SerialManager.h"

// Supported GPS baud rates for detection
const int32_t SerialManager::GPS_BAUD_RATES[] = {460800, 115200, 57600, 38400};
const uint8_t SerialManager::NUM_GPS_BAUD_RATES = 4;


// Static instance pointer
SerialManager *SerialManager::instance = nullptr;

SerialManager::SerialManager()
    : isInitialized(false), serialIMU(&Serial4), prevUSB1DTR(false), prevUSB2DTR(false),
      detectedGPS1Type(GPSType::UNKNOWN), detectedGPS2Type(GPSType::UNKNOWN),
      detectedIMUType(IMUType::NONE)
{
    instance = this;
}

SerialManager::~SerialManager()
{
    instance = nullptr;
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

    // Detect connected devices
    detectConnectedDevices();

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

void SerialManager::detectConnectedDevices()
{
    Serial.print("\r\n\n--- Device Detection ---");

    // Clear buffers before detection
    clearSerialBuffers();
    delay(100); // Allow buffers to clear

    // Detect GPS1 baud rate and type
    Serial.print("\r\n- Detecting GPS1...");
    int32_t gps1Baud = detectGPSBaudRate(SerialGPS1, "GPS1");
    if (gps1Baud > 0)
    {
        Serial.printf(" found at %d baud,", gps1Baud);
        // First try standard detection (for F9P)
        detectedGPS1Type = detectGPSType(SerialGPS1, "GPS1");

        // If generic NMEA, try Unicore detection with larger buffer
        if (detectedGPS1Type == GPSType::GENERIC_NMEA)
        {
            GPSType unicoreType = detectUnicoreGPS(1); // 1 for GPS1
            if (unicoreType != GPSType::UNKNOWN)
            {
                detectedGPS1Type = unicoreType;
            }
        }
        Serial.printf(" type: %s", getGPSTypeName(detectedGPS1Type));
    }
    else
    {
        Serial.print(" not found");
        detectedGPS1Type = GPSType::UNKNOWN;
    }

    // Detect GPS2 baud rate and type
    Serial.print("\r\n- Detecting GPS2...");
    int32_t gps2Baud = detectGPSBaudRate(SerialGPS2, "GPS2");
    if (gps2Baud > 0)
    {
        Serial.printf(" found at %d baud,", gps2Baud);
        // First try standard detection (for F9P)
        detectedGPS2Type = detectGPSType(SerialGPS2, "GPS2");

        // If generic NMEA, try Unicore detection with larger buffer
        if (detectedGPS2Type == GPSType::GENERIC_NMEA)
        {
            GPSType unicoreType = detectUnicoreGPS(2); // 2 for GPS2
            if (unicoreType != GPSType::UNKNOWN)
            {
                detectedGPS2Type = unicoreType;
            }
        }
        Serial.printf(" type: %s", getGPSTypeName(detectedGPS2Type));
    }
    else
    {
        Serial.print(" not found");
        detectedGPS2Type = GPSType::UNKNOWN;
    }
    
    // Special case: If F9P detected on GPS1, check GPS2 for RELPOSNED even if GPS2 not detected normally
    if (detectedGPS1Type == GPSType::F9P_SINGLE && detectedGPS2Type == GPSType::UNKNOWN)
    {
        Serial.print("\r\n  Checking GPS2 for F9P dual RELPOSNED...");
        
        // Try GPS2 at GPS1's baud rate
        SerialGPS2.end();
        delay(10);
        SerialGPS2.begin(BAUD_GPS);
        delay(100);
        
        // Look for RELPOSNED on GPS2
        uint32_t startTime = millis();
        bool foundRELPOSNED = false;
        int bytesRead = 0;
        
        while (millis() - startTime < 2000 && !foundRELPOSNED)
        {
            if (SerialGPS2.available())
            {
                uint8_t b = SerialGPS2.read();
                bytesRead++;
                
                // Simple check for UBX RELPOSNED header pattern
                static uint8_t ubxPattern[4] = {0, 0, 0, 0};
                ubxPattern[0] = ubxPattern[1];
                ubxPattern[1] = ubxPattern[2];
                ubxPattern[2] = ubxPattern[3];
                ubxPattern[3] = b;
                
                if (ubxPattern[0] == 0xB5 && ubxPattern[1] == 0x62 && 
                    ubxPattern[2] == 0x01 && ubxPattern[3] == 0x3C)
                {
                    Serial.print(" RELPOSNED found on GPS2!");
                    foundRELPOSNED = true;
                    detectedGPS1Type = GPSType::F9P_DUAL;
                    detectedGPS2Type = GPSType::F9P_DUAL;
                }
            }
        }
        
        if (!foundRELPOSNED && bytesRead > 0) {
            Serial.printf(" No RELPOSNED (read %d bytes)", bytesRead);
        }
    }

    // Detect IMU (unless already detected as UM981 integrated)
    if (detectedGPS1Type == GPSType::UM981 || detectedGPS2Type == GPSType::UM981)
    {
        detectedIMUType = IMUType::UM981_INTEGRATED;
        Serial.printf("\r\n- IMU detected: %s (integrated with GPS)", getIMUTypeName(detectedIMUType));
    }
    else
    {
        Serial.print("\r\n- Detecting IMU...");
        detectedIMUType = detectIMUType();
        Serial.printf(" %s", getIMUTypeName(detectedIMUType));
    }
}

int32_t SerialManager::detectGPSBaudRate(HardwareSerial &port, const char *portName)
{
    // Try each baud rate
    for (uint8_t i = 0; i < NUM_GPS_BAUD_RATES; i++)
    {
        int32_t baudRate = GPS_BAUD_RATES[i];

        // Set baud rate
        port.end();
        delay(10);
        port.begin(baudRate);
        delay(100); // Give GPS time to start sending

        // Clear any garbage
        while (port.available())
        {
            port.read();
        }

        // Look for NMEA sentences at this baud rate
        if (checkForNMEASentence(port, "$G", 500))
        { // Look for any NMEA sentence starting with $G
            // Found valid NMEA data at this baud rate
            return baudRate;
        }
    }

    // No valid data found at any baud rate
    // Set back to default
    port.end();
    delay(10);
    port.begin(BAUD_GPS);

    return -1; // Not found
}

GPSType SerialManager::detectGPSType(HardwareSerial &port, const char *portName)
{
    // Clear any existing data
    while (port.available())
    {
        port.read();
    }

    // Try u-blox UBX MON-VER command first
    const uint8_t ubxMonVer[] = {0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34};
    uint8_t response[256];
    uint16_t responseLen;

    if (sendAndWaitForResponse(port, ubxMonVer, sizeof(ubxMonVer), response, responseLen, 500))
    {
        // Parse UBX response for F9P identification
        if (responseLen > 40)
        {
            // Look for "ZED-F9P" in the response
            for (int i = 0; i < responseLen - 7; i++)
            {
                if (memcmp(&response[i], "ZED-F9P", 7) == 0)
                {
                    // F9P detected, now check if it's configured for dual by looking for RELPOSNED
                    Serial.print("\r\n  F9P detected, checking for RELPOSNED...");
                    
                    // Clear buffer and wait for fresh data
                    while (port.available()) {
                        port.read();
                    }
                    delay(100); // Give GPS time to send new data
                    
                    // Look for dual antenna indicators in both UBX and NMEA streams
                    uint32_t startTime = millis();
                    uint8_t buffer[512];
                    uint16_t bufIndex = 0;
                    bool foundDualIndicator = false;
                    int bytesRead = 0;
                    int ubxCount = 0;
                    int nmeaCount = 0;
                    
                    // NMEA buffer for sentence detection
                    char nmeaBuffer[256];
                    uint8_t nmeaIndex = 0;
                    
                    Serial.print("\r\n  Looking for dual antenna indicators...");
                    
                    while (millis() - startTime < 3000 && !foundDualIndicator) // 3 second timeout
                    {
                        if (port.available())
                        {
                            uint8_t b = port.read();
                            bytesRead++;
                            
                            // Store in main buffer
                            if (bufIndex >= sizeof(buffer))
                            {
                                memmove(buffer, buffer + 1, sizeof(buffer) - 1);
                                bufIndex = sizeof(buffer) - 1;
                            }
                            buffer[bufIndex++] = b;
                            
                            // Check for NMEA sentences
                            if (b == '$')
                            {
                                nmeaIndex = 0;
                                nmeaBuffer[nmeaIndex++] = b;
                                nmeaCount++;
                            }
                            else if (nmeaIndex > 0 && nmeaIndex < sizeof(nmeaBuffer) - 1)
                            {
                                nmeaBuffer[nmeaIndex++] = b;
                                
                                if (b == '\n')
                                {
                                    nmeaBuffer[nmeaIndex] = '\0';
                                    
                                    // Check for dual antenna NMEA messages
                                    if (strstr(nmeaBuffer, "$GNHDT") != NULL || // Heading
                                        strstr(nmeaBuffer, "$GPHDT") != NULL || // Heading
                                        strstr(nmeaBuffer, "$PTNL,BPQ") != NULL) // Baseline/quality
                                    {
                                        Serial.printf("\r\n  Found dual antenna NMEA: %.20s...", nmeaBuffer);
                                        foundDualIndicator = true;
                                    }
                                    nmeaIndex = 0;
                                }
                            }
                            
                            // Also check for UBX RELPOSNED
                            if (bufIndex >= 4)
                            {
                                // Check for UBX header
                                if (buffer[bufIndex-4] == 0xB5 && buffer[bufIndex-3] == 0x62)
                                {
                                    ubxCount++;
                                    
                                    // Check if this is RELPOSNED
                                    if (buffer[bufIndex-2] == 0x01 && buffer[bufIndex-1] == 0x3C)
                                    {
                                        Serial.print("\r\n  RELPOSNED UBX message found!");
                                        foundDualIndicator = true;
                                    }
                                }
                            }
                        }
                    }
                    
                    Serial.printf("\r\n  Bytes: %d, NMEA msgs: %d, UBX msgs: %d", 
                                  bytesRead, nmeaCount, ubxCount);
                    
                    return foundDualIndicator ? GPSType::F9P_DUAL : GPSType::F9P_SINGLE;
                }
            }
        }
    }

    // If not F9P, return generic NMEA
    // Unicore detection will be done separately with larger buffer
    return GPSType::GENERIC_NMEA;
}

GPSType SerialManager::detectUnicoreGPS(int portNum)
{
    // Temporary larger buffers for Unicore detection
    const int tempBufferSize = 2048;
    uint8_t *tempRxBuffer = new uint8_t[tempBufferSize];
    uint8_t *tempTxBuffer = new uint8_t[256];

    GPSType detectedType = GPSType::UNKNOWN;

    if (portNum == 1)
    {
        // Clear buffer
        while (SerialGPS1.available())
        {
            SerialGPS1.read();
        }

        // Temporarily increase buffer size for GPS1
        SerialGPS1.addMemoryForRead(tempRxBuffer, tempBufferSize);
        SerialGPS1.addMemoryForWrite(tempTxBuffer, 256);

        // Send VERSION command
        SerialGPS1.write("VERSION\r\n");
        delay(100);

        // Read response
        uint32_t startTime = millis();
        while (millis() - startTime < 500)
        {
            if (SerialGPS1.available())
            {
                char incoming[256];
                memset(incoming, 0, sizeof(incoming));

                int bytesRead = SerialGPS1.readBytesUntil('\n', incoming, sizeof(incoming) - 1);

                if (bytesRead > 0)
                {
                    if (strstr(incoming, "UM981") != NULL)
                    {
                        Serial.printf("\r\n  UM981 VERSION: %s", incoming);
                        detectedType = GPSType::UM981;
                        break;
                    }
                    if (strstr(incoming, "UM982") != NULL)
                    {
                        Serial.printf("\r\n  UM982 VERSION: %s", incoming);
                        detectedType = GPSType::UM982_SINGLE;
                        
                        // Check for dual configuration by looking for GPHPR messages
                        Serial.print("\r\n  Checking for UM982 dual configuration...");
                        
                        // Clear buffer and wait for fresh data
                        while (SerialGPS1.available()) {
                            SerialGPS1.read();
                        }
                        
                        // Look for GPHPR messages
                        uint32_t checkStart = millis();
                        bool foundDualIndicator = false;
                        char buffer[256];
                        uint8_t bufIdx = 0;
                        
                        while (millis() - checkStart < 2000 && !foundDualIndicator)
                        {
                            if (SerialGPS1.available())
                            {
                                char c = SerialGPS1.read();
                                
                                if (c == '$')
                                {
                                    bufIdx = 0;
                                    buffer[bufIdx++] = c;
                                }
                                else if (bufIdx > 0 && bufIdx < sizeof(buffer) - 1)
                                {
                                    buffer[bufIdx++] = c;
                                    
                                    if (c == '\n')
                                    {
                                        buffer[bufIdx] = '\0';
                                        
                                        // Check for GNHPR message (dual antenna heading/pitch/roll)
                                        if (strstr(buffer, "$GNHPR") != NULL)
                                        {
                                            Serial.print(" GNHPR found - Dual configuration detected!");
                                            foundDualIndicator = true;
                                            detectedType = GPSType::UM982_DUAL;
                                        }
                                        bufIdx = 0;
                                    }
                                }
                            }
                        }
                        
                        if (!foundDualIndicator) {
                            Serial.print(" Single configuration");
                        }
                        
                        break;
                    }
                }
            }
        }

        // Restore original buffers
        SerialGPS1.addMemoryForRead(gps1RxBuffer, sizeof(gps1RxBuffer));
        SerialGPS1.addMemoryForWrite(gps1TxBuffer, sizeof(gps1TxBuffer));

        // Clear any remaining data
        while (SerialGPS1.available())
        {
            SerialGPS1.read();
        }
    }
    else if (portNum == 2)
    {
        // Clear buffer
        while (SerialGPS2.available())
        {
            SerialGPS2.read();
        }

        // Temporarily increase buffer size for GPS2
        SerialGPS2.addMemoryForRead(tempRxBuffer, tempBufferSize);
        SerialGPS2.addMemoryForWrite(tempTxBuffer, 256);

        // Send VERSION command
        SerialGPS2.write("VERSION\r\n");
        delay(100);

        // Read response
        uint32_t startTime = millis();
        while (millis() - startTime < 500)
        {
            if (SerialGPS2.available())
            {
                char incoming[256];
                memset(incoming, 0, sizeof(incoming));

                int bytesRead = SerialGPS2.readBytesUntil('\n', incoming, sizeof(incoming) - 1);

                if (bytesRead > 0)
                {
                    if (strstr(incoming, "UM981") != NULL)
                    {
                        Serial.printf("\r\n  UM981 VERSION: %s", incoming);
                        detectedType = GPSType::UM981;
                        break;
                    }
                    if (strstr(incoming, "UM982") != NULL)
                    {
                        Serial.printf("\r\n  UM982 VERSION: %s", incoming);
                        detectedType = GPSType::UM982_SINGLE;
                        break;
                    }
                }
            }
        }

        // Restore original buffers
        SerialGPS2.addMemoryForRead(gps2RxBuffer, sizeof(gps2RxBuffer));
        SerialGPS2.addMemoryForWrite(gps2TxBuffer, sizeof(gps2TxBuffer));

        // Clear any remaining data
        while (SerialGPS2.available())
        {
            SerialGPS2.read();
        }
    }

    // Clean up temporary buffers
    delete[] tempRxBuffer;
    delete[] tempTxBuffer;

    return detectedType;
}

IMUType SerialManager::detectIMUType()
{
    if (!serialIMU)
        return IMUType::NONE;

    // Clear IMU buffer
    while (serialIMU->available())
    {
        serialIMU->read();
    }

    // Wait for fresh data
    delay(100);

    // Try to detect IMU type by looking at data pattern
    uint32_t startTime = millis();
    uint8_t buffer[256]; // Larger buffer for full packets
    uint16_t bufIdx = 0;
    int byteCount = 0;

    Serial.print("\r\n  IMU detection: reading bytes...");

    while (millis() - startTime < 1500) // Increased timeout
    {
        if (serialIMU->available())
        {
            uint8_t currentByte = serialIMU->read();
            byteCount++;

            // Store in buffer
            if (bufIdx < sizeof(buffer))
            {
                buffer[bufIdx++] = currentByte;
            }
            else
            {
                // Shift buffer left by 1
                memmove(buffer, buffer + 1, sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = currentByte;
                bufIdx = sizeof(buffer);
            }

            // Debug output - show first 40 bytes
            if (byteCount <= 40)
            {
                Serial.printf("%02X ", currentByte);
                if (byteCount % 10 == 0)
                    Serial.print("\r\n  ");
            }

            // Check for BNO085 pattern (0xAA, 0xAA)
            if (bufIdx >= 2)
            {
                for (int i = 0; i <= bufIdx - 2; i++)
                {
                    if (buffer[i] == 0xAA && buffer[i + 1] == 0xAA)
                    {
                        Serial.print("\r\n  Found BNO085 header");
                        return IMUType::BNO085;
                    }
                }
            }

            // Check for TM171 EasyProfile pattern (0xAA, 0x55, size)
            if (bufIdx >= 4) // Need at least header + size + 1 data byte
            {
                for (int i = 0; i <= bufIdx - 4; i++)
                {
                    if (buffer[i] == 0xAA && buffer[i + 1] == 0x55)
                    {
                        uint8_t payloadSize = buffer[i + 2];

                        // TM171 typical payload sizes based on EasyProfile
                        // Common sizes: 13, 29, 45, 61 (from your original code)
                        // But also check for any reasonable size up to 120
                        if (payloadSize > 0 && payloadSize <= 120)
                        {
                            Serial.printf("\r\n  Found TM171 EasyProfile header with payload size %d", payloadSize);

                            // Wait to see if we get a complete packet
                            uint32_t packetWaitStart = millis();
                            int totalPacketSize = 2 + 1 + payloadSize + 2; // header + size + payload + checksum

                            while (millis() - packetWaitStart < 50)
                            {
                                if (serialIMU->available())
                                {
                                    buffer[bufIdx++] = serialIMU->read();
                                    if (bufIdx >= i + totalPacketSize)
                                    {
                                        // We have enough bytes for a complete packet
                                        // Could validate checksum here if needed
                                        Serial.print(" - Complete packet received");
                                        return IMUType::TM171;
                                    }
                                }
                            }

                            // Even if we didn't get a complete packet,
                            // finding the header with valid size is good enough
                            return IMUType::TM171;
                        }
                    }
                }
            }
        }
    }

    Serial.printf("\r\n  No IMU pattern detected (read %d bytes)", byteCount);

    // If we got data but couldn't identify the pattern, show what we got
    if (byteCount > 0 && byteCount <= 100)
    {
        Serial.print("\r\n  Full buffer dump: ");
        for (int i = 0; i < min(bufIdx, 50); i++)
        {
            Serial.printf("%02X ", buffer[i]);
            if ((i + 1) % 20 == 0)
                Serial.print("\r\n  ");
        }
    }

    return IMUType::NONE;
}

bool SerialManager::sendAndWaitForResponse(HardwareSerial &port, const uint8_t *cmd, uint16_t cmdLen,
                                           uint8_t *response, uint16_t &responseLen, uint32_t timeout)
{
    // Clear input buffer
    while (port.available())
    {
        port.read();
    }

    // Send command
    port.write(cmd, cmdLen);

    // Wait for response
    uint32_t startTime = millis();
    responseLen = 0;

    while (millis() - startTime < timeout && responseLen < 256)
    {
        if (port.available())
        {
            response[responseLen++] = port.read();
            startTime = millis(); // Reset timeout on data received
        }
    }

    return responseLen > 0;
}

bool SerialManager::checkForNMEASentence(HardwareSerial &port, const char *sentenceType, uint32_t timeout)
{
    uint32_t startTime = millis();
    char buffer[128];
    uint8_t bufIdx = 0;

    while (millis() - startTime < timeout)
    {
        if (port.available())
        {
            char c = port.read();

            if (c == '$')
            {
                bufIdx = 0;
                buffer[bufIdx++] = c;
            }
            else if (bufIdx > 0 && bufIdx < sizeof(buffer) - 1)
            {
                buffer[bufIdx++] = c;

                if (c == '\n')
                {
                    buffer[bufIdx] = '\0';
                    if (strstr(buffer, sentenceType) != nullptr)
                    {
                        return true;
                    }
                    bufIdx = 0;
                }
            }
        }
    }

    return false;
}

const char *SerialManager::getGPSTypeName(GPSType type) const
{
    switch (type)
    {
    case GPSType::F9P_SINGLE:
        return "F9P Single";
    case GPSType::F9P_DUAL:
        return "F9P Dual";
    case GPSType::UM981:
        return "UM981";
    case GPSType::UM982_SINGLE:
        return "UM982 Single";
    case GPSType::UM982_DUAL:
        return "UM982 Dual";
    case GPSType::GENERIC_NMEA:
        return "Generic NMEA";
    default:
        return "Unknown";
    }
}

const char *SerialManager::getIMUTypeName(IMUType type) const
{
    switch (type)
    {
    case IMUType::BNO085:
        return "BNO085";
    case IMUType::TM171:
        return "TM171";
    case IMUType::CMPS14:
        return "CMPS14";
    case IMUType::UM981_INTEGRATED:
        return "UM981 Integrated";
    case IMUType::GENERIC:
        return "Generic";
    default:
        return "None";
    }
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
                Serial.printf("\r\n%i *SerialGPS1 buffer cleared!-Normal at startup*", millis());
                return;
            }

            // Basic processing - just read the data for now
            uint8_t gps1Read = SerialGPS1.read();
            // Data will be processed by GNSSProcessor
            (void)gps1Read; // Suppress unused variable warning
        }
    }
    else
    {
        handleGPS1BridgeMode();
    }
}

void SerialManager::processGPS2()
{
    // Basic GPS2 processing
    if (!isGPS2Bridged())
    {
        uint16_t gps2Available = SerialGPS2.available();
        if (gps2Available)
        {
            if (gps2Available > sizeof(gps2RxBuffer) - 10)
            {
                SerialGPS2.clear();
                Serial.printf("\r\n%i *SerialGPS2 buffer cleared!-Normal at startup*", millis());
                return;
            }

            // Basic processing
            uint8_t gps2Read = SerialGPS2.read();
            (void)gps2Read; // Suppress unused variable warning
        }
    }
    else
    {
        handleGPS2BridgeMode();
    }
}

void SerialManager::processRTK()
{
    // RTK/RTCM processing
    if (SerialRTK.available())
    {
        uint8_t rtcmByte = SerialRTK.read();

        // Forward RTCM to GPS1 (unless bridged)
        if (!isGPS1Bridged())
        {
            SerialGPS1.write(rtcmByte);
        }

        // Optionally forward to GPS2 for special setups
        // if (!isGPS2Bridged()) {
        //     SerialGPS2.write(rtcmByte);
        // }
    }
}

void SerialManager::processRS232()
{
    // RS232 is typically output only for NMEA sentences
    // Add any RS232 input processing here if needed
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

        // Check for CRLF termination
        if (incomingIndex >= 2 &&
            incomingBytes[incomingIndex - 2] == 13 &&
            incomingBytes[incomingIndex - 1] == 10)
        {
            if (validatePGNHeader(incomingBytes, incomingIndex))
            {
                processESP32PGN(incomingBytes, incomingIndex - 2);
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

    Serial.print("\r\n\n--- Detected Devices ---");
    Serial.printf("\r\nGPS1: %s", getGPSTypeName(detectedGPS1Type));
    Serial.printf("\r\nGPS2: %s", getGPSTypeName(detectedGPS2Type));
    Serial.printf("\r\nIMU: %s", getIMUTypeName(detectedIMUType));

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