// WebManager.h
// Web server implementation using AsyncWebServer_Teensy41

#ifndef WEBMANAGER_H
#define WEBMANAGER_H

#include <Arduino.h>
#include "web_pages/WebPages.h"

// Forward declarations to avoid including AsyncWebServer in header
class AsyncWebServer;
class AsyncWebServerRequest;

class WebManager {
public:
    WebManager();
    ~WebManager();
    
    bool begin(uint16_t port = 80);
    void stop();
    
    // No need for handleClient() - AsyncWebServer handles everything async
    
private:
    AsyncWebServer* server;
    bool isRunning;
    WebLanguage currentLanguage;
    
    void setupRoutes();
    void setupEventLoggerAPI();
    void setupNetworkAPI();
    void setupOTARoutes();
    void handleRoot(AsyncWebServerRequest* request);
    void handleApiStatus(AsyncWebServerRequest* request);
    void handleEventLoggerPage(AsyncWebServerRequest* request);
    void handleNetworkPage(AsyncWebServerRequest* request);
    void handleOTAPage(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    
    // Helper to build select options for log levels
    String buildLevelOptions(uint8_t selectedLevel);
};

#endif // WEBMANAGER_H