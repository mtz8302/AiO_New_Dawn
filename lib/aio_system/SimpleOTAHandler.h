// SimpleOTAHandler.h
// Simple OTA firmware update handler for Teensy 4.1 with SimpleHTTPServer

#ifndef SIMPLE_OTA_HANDLER_H
#define SIMPLE_OTA_HANDLER_H

#include <Arduino.h>
#include <QNEthernet.h>

// Forward declarations
using namespace qindesign::network;

// FlasherX includes
extern "C" {
    #include "FlashTxx.h"
}
#include "FXUtil.h"

class SimpleOTAHandler {
public:
    // Initialize OTA handler
    static bool init();
    
    // Reset state for new upload
    static void reset();
    
    // Process chunk of hex data
    static bool processChunk(const uint8_t* data, size_t len);
    
    // Finalize upload and check if valid
    static bool finalize();
    
    // Apply the firmware update (triggers reboot)
    static bool applyUpdate();
    
    // Get upload progress (0-100)
    static uint8_t getProgress() { return progress; }
    
    // Get error message
    static const char* getError() { return errorMsg; }
    
    // Check if upload is in progress
    static bool isInProgress() { return otaInProgress; }
    
    // Check if upload is complete
    static bool isComplete() { return otaComplete; }
    
private:
    // OTA state
    static bool otaInProgress;
    static bool otaComplete;
    static uint32_t totalBytes;
    static uint32_t processedBytes;
    static uint8_t progress;
    static const char* errorMsg;
    
    // Buffer for accumulating hex lines
    static String hexBuffer;
    
    // Firmware buffer info
    static uint32_t bufferAddr;
    static uint32_t bufferSize;
    
    // Intel hex parsing
    static bool processHexLine(const String& line);
    static bool parseIntelHex(const String& line, uint32_t& addr, uint8_t* data, uint8_t& len, uint8_t& type);
    static uint8_t hexToByte(char c);
    static uint32_t baseAddress;
    static uint32_t minAddress;
    static uint32_t maxAddress;
};

#endif // SIMPLE_OTA_HANDLER_H