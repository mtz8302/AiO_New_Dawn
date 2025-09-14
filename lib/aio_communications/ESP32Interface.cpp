#include "ESP32Interface.h"
#include "SerialManager.h"
#include "QNEthernetUDPHandler.h"

// Global instance
ESP32Interface esp32Interface;

// Initialize the interface
void ESP32Interface::init() {
    // SerialESP32 is already initialized by SerialManager at 460800 baud
    LOG_INFO(EventSource::SYSTEM, "ESP32 interface initialized on Serial2 (460800 baud)");
    Serial.println("ESP32Interface: Initialized on Serial2 (460800 baud)");
    
    // Clear receive buffer
    rxBufferIndex = 0;
    memset(rxBuffer, 0, RX_BUFFER_SIZE);
}

// Main processing loop - called from main.cpp
void ESP32Interface::process() {
    // Debug: Show we're running
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 10000) {  // Every 10 seconds
        Serial.printf("ESP32Interface: Running, detected=%s, Serial2 available=%d\n", 
                      esp32Detected ? "YES" : "NO", SerialESP32.available());
        lastDebugTime = millis();
    }
    
    // Process any incoming serial data
    processIncomingData();
    
    // Check for timeout - if we haven't heard hello in a while
    if (esp32Detected && (millis() - lastHelloTime > HELLO_TIMEOUT_MS)) {
        esp32Detected = false;
        LOG_WARNING(EventSource::SYSTEM, "ESP32 connection lost (hello timeout)");
        Serial.println("ESP32Interface: Connection lost (hello timeout)");
    }
}

// Send data to ESP32 (called by UDP handler)
void ESP32Interface::sendToESP32(const uint8_t* data, size_t length) {
    if (!esp32Detected) {
        return;  // Don't send if ESP32 not detected
    }
    
    // Log first few PGNs for debugging
    static int pgnCount = 0;
    if (pgnCount < 20 && length >= 5) {
        Serial.printf("ESP32 TX: PGN=%d (0x%02X), len=%d, raw: ", 
                      data[3], data[3], length);
        // Show all bytes
        for (size_t i = 0; i < length && i < 20; i++) {
            Serial.printf("%02X ", data[i]);
        }
        if (length > 20) Serial.print("...");
        
        // Calculate and show CRC
        if (length >= 6) {
            uint8_t calcCrc = 0;
            for (size_t i = 0; i < length - 1; i++) {
                calcCrc ^= data[i];
            }
            Serial.printf(" (CRC: calc=%02X, pkt=%02X)", calcCrc, data[length-1]);
        }
        
        Serial.println();
        pgnCount++;
    }
    
    // Send raw bytes to ESP32
    SerialESP32.write(data, length);
}

// Process incoming data from ESP32
void ESP32Interface::processIncomingData() {
    static bool firstByte = true;
    while (SerialESP32.available()) {
        uint8_t byte = SerialESP32.read();
        
        // Debug: Log first few bytes received
        if (firstByte) {
            Serial.println("\nESP32Interface: Receiving data from ESP32!");
            firstByte = false;
        }
        
        // Add to buffer
        if (rxBufferIndex < RX_BUFFER_SIZE) {
            rxBuffer[rxBufferIndex++] = byte;
        }
        
        // Check for hello message
        checkForHello();
        
        // Check for complete PGN message
        // PGN format: [0x80][0x81][Source][PGN][Length][Data...][CRC]
        if (rxBufferIndex >= 7) {  // Minimum PGN size (header + length + CRC)
            // Look for PGN header
            bool foundPGN = false;
            size_t pgnStart = 0;
            
            // Scan buffer for PGN header
            for (size_t i = 0; i <= rxBufferIndex - 7; i++) {
                if (rxBuffer[i] == 0x80 && rxBuffer[i + 1] == 0x81) {
                    // Found potential PGN header
                    pgnStart = i;
                    
                    // Extract message info
                    // uint8_t source = rxBuffer[i + 2];  // Source ID
                    // uint8_t pgnId = rxBuffer[i + 3];   // PGN ID
                    uint8_t dataLength = rxBuffer[i + 4];  // Data length
                    
                    // Calculate total message length: header(5) + dataLength + CRC(1)
                    size_t totalLength = 5 + dataLength + 1;
                    
                    // Check if we have the complete message
                    if ((pgnStart + totalLength) <= rxBufferIndex) {
                        foundPGN = true;
                        
                        // Send complete PGN to UDP9999 broadcast
                        uint8_t* pgnData = &rxBuffer[pgnStart];
                        
                        // Debug log received PGN
                        uint8_t source = rxBuffer[pgnStart + 2];
                        uint8_t pgn = rxBuffer[pgnStart + 3];
                        Serial.printf("ESP32 RX: PGN=%d, source=%d, len=%d -> UDP9999\n", 
                                      pgn, source, totalLength);
                        
                        QNEthernetUDPHandler::sendUDP9999Packet(pgnData, totalLength);
                        
                        // Remove processed data from buffer
                        size_t remaining = rxBufferIndex - (pgnStart + totalLength);
                        if (remaining > 0) {
                            memmove(rxBuffer, &rxBuffer[pgnStart + totalLength], remaining);
                        }
                        rxBufferIndex = remaining;
                        
                        break;  // Process one PGN at a time
                    }
                }
            }
            
            // If no PGN found and buffer is getting full, clear old data
            if (!foundPGN && rxBufferIndex > RX_BUFFER_SIZE - 100) {
                // Keep last 100 bytes
                memmove(rxBuffer, &rxBuffer[rxBufferIndex - 100], 100);
                rxBufferIndex = 100;
            }
        }
    }
}

// Check buffer for ESP32 hello message
void ESP32Interface::checkForHello() {
    const char* helloMsg = "ESP32-hello";
    size_t helloLen = strlen(helloMsg);
    
    // Debug: Show buffer contents periodically
    static uint32_t lastBufferDebug = 0;
    if (rxBufferIndex > 0 && millis() - lastBufferDebug > 2000) {
        Serial.printf("ESP32 RX Buffer (%d bytes): ", rxBufferIndex);
        for (size_t i = 0; i < rxBufferIndex && i < 20; i++) {
            if (rxBuffer[i] >= 32 && rxBuffer[i] <= 126) {
                Serial.printf("%c", rxBuffer[i]);
            } else {
                Serial.printf("[%02X]", rxBuffer[i]);
            }
        }
        if (rxBufferIndex > 20) Serial.print("...");
        Serial.println();
        lastBufferDebug = millis();
    }
    
    // Look for hello message in buffer
    if (rxBufferIndex >= helloLen) {
        for (size_t i = 0; i <= rxBufferIndex - helloLen; i++) {
            if (memcmp(&rxBuffer[i], helloMsg, helloLen) == 0) {
                // Found hello message
                if (!esp32Detected) {
                    esp32Detected = true;
                    LOG_INFO(EventSource::SYSTEM, "ESP32 detected and connected");
                    LOG_INFO(EventSource::SYSTEM, "ESP32 will now receive PGNs from UDP port 8888");
                    Serial.println("\n*** ESP32 DETECTED AND CONNECTED ***");
                    Serial.println("ESP32 will now receive PGNs from UDP port 8888");
                }
                lastHelloTime = millis();
                
                // Remove hello message from buffer
                size_t remaining = rxBufferIndex - (i + helloLen);
                if (remaining > 0) {
                    memmove(&rxBuffer[i], &rxBuffer[i + helloLen], remaining);
                }
                rxBufferIndex -= helloLen;
                
                break;
            }
        }
    }
}

// Print status information
void ESP32Interface::printStatus() {
    Serial.println("\n=== ESP32 Interface Status ===");
    Serial.printf("Detected: %s\n", esp32Detected ? "Yes" : "No");
    
    if (esp32Detected) {
        Serial.printf("Last hello: %lu ms ago\n", millis() - lastHelloTime);
    }
    
    Serial.printf("RX buffer: %d bytes\n", rxBufferIndex);
}