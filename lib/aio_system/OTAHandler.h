// OTAHandler.h
// OTA firmware update handler for Teensy 4.1

#ifndef OTAHANDLER_H
#define OTAHANDLER_H

#include <Arduino.h>

// Forward declarations
class AsyncWebServerRequest;

// FlasherX includes
extern "C" {
    #include "FlashTxx.h"
}
#include "FXUtil.h"
#include "EventLogger.h"

class OTAHandler {
public:
    // Initialize OTA handler
    static bool init();
    
    // Handle OTA upload request
    static void handleOTAUpload(AsyncWebServerRequest *request, String filename, 
                               size_t index, uint8_t *data, size_t len, bool final);
    
    // Handle upload completion
    static void handleOTAComplete(AsyncWebServerRequest *request);
    
    // Apply the firmware update (triggers reboot)
    static void applyUpdate();
    
private:
    // OTA state
    static bool otaInProgress;
    static bool otaComplete;
    static bool otaApply;
    static uint32_t bufferAddr;
    static uint32_t bufferSize;
    
    // Hex parsing
    static char line[96];
    static int lineIndex;
    static char data[32] __attribute__ ((aligned (8)));
    
    // Intel hex info struct
    struct HexInfo {
        char *data;
        unsigned int addr;
        unsigned int code;
        unsigned int num;
        uint32_t base;
        uint32_t min;
        uint32_t max;
        int eof;
        int lines;
    };
    
    static HexInfo hexInfo;
    
    // Intel hex parsing functions
    static int process_hex_record(HexInfo *hex);
};

#endif // OTAHANDLER_H