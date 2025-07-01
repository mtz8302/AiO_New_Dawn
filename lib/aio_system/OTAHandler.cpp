// OTAHandler.cpp
// OTA firmware update handler implementation for Teensy 4.1

#include "OTAHandler.h"
#include "FXUtil.h"
#include <AsyncWebServer_Teensy41.h>

// Static member definitions
bool OTAHandler::otaInProgress = false;
bool OTAHandler::otaComplete = false;
bool OTAHandler::otaApply = false;
uint32_t OTAHandler::bufferAddr = 0;
uint32_t OTAHandler::bufferSize = 0;
char OTAHandler::line[96] = {0};
int OTAHandler::lineIndex = 0;
char OTAHandler::data[32] __attribute__ ((aligned (8))) = {0};
OTAHandler::HexInfo OTAHandler::hexInfo = {
    OTAHandler::data, 0, 0, 0,        // data, addr, num, code
    0, 0xFFFFFFFF, 0,                 // base, min, max
    0, 0                              // eof, lines
};

// External flash ID from main.cpp
extern const char* flash_id;

bool OTAHandler::init() {
    // Nothing to initialize - all state is reset per upload
    LOG_INFO(EventSource::SYSTEM, "OTA handler initialized");
    return true;
}

void OTAHandler::handleOTAUpload(AsyncWebServerRequest *request, String filename, 
                                 size_t index, uint8_t *data, size_t len, bool final) {
    
    // Start OTA process on first chunk
    if (!otaInProgress) {
        LOG_INFO(EventSource::NETWORK, "Starting OTA firmware upload: %s", filename.c_str());
        
        // Initialize firmware buffer
        if (firmware_buffer_init(&bufferAddr, &bufferSize) == 0) {
            LOG_ERROR(EventSource::NETWORK, "Failed to create firmware buffer");
            request->send(500, "text/plain", "Failed to create firmware buffer");
            return;
        }
        
        LOG_INFO(EventSource::NETWORK, "Created firmware buffer: %luK %s (0x%08lX - 0x%08lX)",
                 bufferSize/1024, IN_FLASH(bufferAddr) ? "FLASH" : "RAM",
                 bufferAddr, bufferAddr + bufferSize);
        
        // Reset hex parsing state
        lineIndex = 0;
        hexInfo.lines = 0;
        hexInfo.eof = 0;
        hexInfo.base = 0;
        hexInfo.min = 0xFFFFFFFF;
        hexInfo.max = 0;
        
        otaInProgress = true;
        otaComplete = false;
    }
    
    // Process data chunk
    if (otaInProgress && len > 0) {
        size_t i = 0;
        while (i < len) {
            // Process line by line
            if (data[i] == '\n' || lineIndex == sizeof(line) - 1) {
                line[lineIndex] = 0;  // null-terminate
                
                // Parse hex line
                if (parse_hex_line(line, hexInfo.data, &hexInfo.addr, 
                                  &hexInfo.num, &hexInfo.code) == 0) {
                    LOG_ERROR(EventSource::NETWORK, "Invalid hex line: %s", line);
                    request->send(400, "text/plain", "Invalid hex line");
                    return;
                }
                
                // Process hex record
                if (process_hex_record(&hexInfo) != 0) {
                    LOG_ERROR(EventSource::NETWORK, "Invalid hex code: %d", hexInfo.code);
                    request->send(400, "text/plain", "Invalid hex code");
                    return;
                }
                
                // Handle data records
                if (hexInfo.code == 0) {
                    uint32_t addr = bufferAddr + hexInfo.base + hexInfo.addr - FLASH_BASE_ADDR;
                    
                    // Check address bounds
                    if (hexInfo.max > (FLASH_BASE_ADDR + bufferSize)) {
                        LOG_ERROR(EventSource::NETWORK, "Address 0x%08lX exceeds buffer", hexInfo.max);
                        request->send(400, "text/plain", "Address exceeds buffer");
                        return;
                    }
                    
                    // Write to buffer
                    if (!IN_FLASH(bufferAddr)) {
                        // RAM buffer - direct copy
                        memcpy((void*)addr, (void*)hexInfo.data, hexInfo.num);
                    } else {
                        // Flash buffer - use flash write
                        int error = flash_write_block(addr, hexInfo.data, hexInfo.num);
                        if (error) {
                            LOG_ERROR(EventSource::NETWORK, "Flash write error: 0x%02X", error);
                            request->send(400, "text/plain", "Flash write error");
                            return;
                        }
                    }
                }
                
                hexInfo.lines++;
                lineIndex = 0;
            } else if (data[i] != '\r') {
                // Add character to line (skip CR)
                line[lineIndex++] = data[i];
            }
            i++;
        }
    }
    
    // Handle final chunk
    if (final) {
        LOG_INFO(EventSource::NETWORK, "OTA upload complete: %d lines, %lu bytes (0x%08lX - 0x%08lX)",
                 hexInfo.lines, hexInfo.max - hexInfo.min, hexInfo.min, hexInfo.max);
        otaComplete = true;
    }
}

void OTAHandler::handleOTAComplete(AsyncWebServerRequest *request) {
    if (!otaComplete) {
        request->send(400, "text/plain", "Upload incomplete");
        return;
    }
    
    bool valid = true;
    
    // Verify FSEC value for Kinetis (not needed for Teensy 4.x)
    #if defined(KINETISK) || defined(KINETISL)
    uint32_t fsec = *(uint32_t *)(0x40C + bufferAddr);
    if (fsec != 0xfffff9de) {
        LOG_ERROR(EventSource::NETWORK, "Invalid FSEC value: 0x%08lX (expected 0xFFFFF9DE)", fsec);
        valid = false;
    }
    #endif
    
    // Verify flash ID
    if (valid && !check_flash_id(bufferAddr, hexInfo.max - hexInfo.min)) {
        LOG_ERROR(EventSource::NETWORK, "Firmware missing target ID: %s", flash_id);
        valid = false;
    } else if (valid) {
        LOG_INFO(EventSource::NETWORK, "Firmware contains correct target ID: %s", flash_id);
    }
    
    // Send response
    if (valid) {
        request->send(200, "text/plain", "OTA Success! System will reboot in 2 seconds...");
        otaApply = true;
    } else {
        request->send(500, "text/plain", "OTA validation failed");
        // Clean up buffer
        firmware_buffer_free(bufferAddr, bufferSize);
        otaInProgress = false;
        otaComplete = false;
    }
}

void OTAHandler::applyUpdate() {
    if (!otaApply || !otaComplete) {
        return;
    }
    
    LOG_INFO(EventSource::NETWORK, "Applying firmware update...");
    delay(100);  // Let log message send
    
    // Move firmware from buffer to flash base
    flash_move(FLASH_BASE_ADDR, bufferAddr, hexInfo.max - hexInfo.min);
    
    // Reboot
    SCB_AIRCR = 0x05FA0004;  // System reset
    while(1) {}  // Wait for reset
}

// Intel hex parsing helper
int OTAHandler::process_hex_record(HexInfo *hex) {
    if (hex->code == 0) {  // data record
        uint32_t addr = hex->base + hex->addr;
        if (addr < hex->min) hex->min = addr;
        addr += hex->num;
        if (addr > hex->max) hex->max = addr;
    } else if (hex->code == 1) {  // EOF record
        hex->eof = 1;
    } else if (hex->code == 2) {  // extended segment address
        hex->base = ((hex->data[0] << 8) | hex->data[1]) << 4;
    } else if (hex->code == 4) {  // extended linear address
        hex->base = ((hex->data[0] << 8) | hex->data[1]) << 16;
    } else if (hex->code == 5) {  // start address
        // Ignore - we don't use the start address
    } else {
        return -1;  // unknown hex code
    }
    return 0;
}