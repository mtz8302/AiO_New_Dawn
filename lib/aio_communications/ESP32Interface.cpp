#include "ESP32Interface.h"
#include "SerialManager.h"
#include "QNEthernetUDPHandler.h"

// Global instance
ESP32Interface esp32Interface;

// Initialize the interface
void ESP32Interface::init() {
    // SerialESP32 is already initialized by SerialManager at 460800 baud
    LOG_INFO(EventSource::SYSTEM, "ESP32 interface initialized on Serial2 (460800 baud)");
    
    // Clear receive buffer
    rxBufferIndex = 0;
    memset(rxBuffer, 0, RX_BUFFER_SIZE);
}

// Main processing loop - called from main.cpp
void ESP32Interface::process() {
    // Process any incoming serial data
    processIncomingData();
    
    // Check for timeout - if we haven't heard hello in a while
    if (esp32Detected && (millis() - lastHelloTime > HELLO_TIMEOUT_MS)) {
        esp32Detected = false;
        LOG_WARNING(EventSource::SYSTEM, "ESP32 connection lost (hello timeout)");
    }
}

// Send data to ESP32 (called by UDP handler)
void ESP32Interface::sendToESP32(const uint8_t* data, size_t length) {
    if (!esp32Detected) {
        return;  // Don't send if ESP32 not detected
    }
    
    // Send raw bytes to ESP32
    SerialESP32.write(data, length);
}

// Process incoming data from ESP32
void ESP32Interface::processIncomingData() {
    while (SerialESP32.available()) {
        uint8_t byte = SerialESP32.read();
        
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
    
    // Look for hello message in buffer
    if (rxBufferIndex >= helloLen) {
        for (size_t i = 0; i <= rxBufferIndex - helloLen; i++) {
            if (memcmp(&rxBuffer[i], helloMsg, helloLen) == 0) {
                // Found hello message
                if (!esp32Detected) {
                    esp32Detected = true;
                    LOG_INFO(EventSource::SYSTEM, "ESP32 detected and connected");
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