// WebManager.cpp
// Web server implementation using AsyncWebServer_Teensy41

#include "WebManager.h"
#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <AsyncWebServer_Teensy41.h>
#include <ArduinoJson.h>
#include "EventLogger.h"

using namespace qindesign::network;

// Macro to read strings from PROGMEM
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

WebManager::WebManager() : server(nullptr), isRunning(false), currentLanguage(WebLanguage::ENGLISH) {
    // TODO: Load language preference from config
}

WebManager::~WebManager() {
    stop();
}

bool WebManager::begin(uint16_t port) {
    if (isRunning) {
        return true;  // Already running
    }
    
    // Create server instance
    server = new AsyncWebServer(port);
    if (!server) {
        LOG_ERROR(EventSource::NETWORK, "Failed to create AsyncWebServer");
        return false;
    }
    
    // Setup all routes
    setupRoutes();
    
    // Start the server
    server->begin();
    isRunning = true;
    
    IPAddress ip = Ethernet.localIP();
    LOG_INFO(EventSource::NETWORK, "Web server started on http://%d.%d.%d.%d:%d", 
             ip[0], ip[1], ip[2], ip[3], port);
    
    return true;
}

void WebManager::stop() {
    if (server && isRunning) {
        server->end();
        delete server;
        server = nullptr;
        isRunning = false;
        LOG_INFO(EventSource::NETWORK, "Web server stopped");
    }
}

void WebManager::setupRoutes() {
    // Root endpoint
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });
    
    // API status endpoint
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleApiStatus(request);
    });
    
    // EventLogger configuration page
    server->on("/eventlogger", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleEventLoggerPage(request);
    });
    
    // Language selection
    server->on("/lang/en", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentLanguage = WebLanguage::ENGLISH;
        // TODO: Save language preference to EEPROM
        request->redirect("/");
    });
    
    server->on("/lang/de", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentLanguage = WebLanguage::GERMAN;
        // TODO: Save language preference to EEPROM
        request->redirect("/");
    });
    
    // Setup EventLogger API routes
    setupEventLoggerAPI();
    
    // 404 handler
    server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
}

void WebManager::handleRoot(AsyncWebServerRequest* request) {
    // Get IP address and link speed
    IPAddress ip = Ethernet.localIP();
    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
    String linkSpeed = String(Ethernet.linkSpeed());
    
    // Load template from PROGMEM and process replacements
    String html = FPSTR(WebPageSelector::getHomePage(currentLanguage));
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    html.replace("%IP_ADDRESS%", ipStr);
    html.replace("%LINK_SPEED%", linkSpeed);
    
    request->send(200, "text/html", html);
}

void WebManager::handleApiStatus(AsyncWebServerRequest* request) {
    // Simple JSON response for now
    JsonDocument doc;
    doc["status"] = "ok";
    doc["uptime"] = millis();
    doc["message"] = "Hello from AsyncWebServer on Teensy 4.1!";
    
    String response;
    serializeJsonPretty(doc, response);
    request->send(200, "application/json", response);
}

void WebManager::setupEventLoggerAPI() {
    // GET current EventLogger configuration
    server->on("/api/eventlogger/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        EventLogger* logger = EventLogger::getInstance();
        EventConfig& config = logger->getConfig();
        
        JsonDocument doc;
        doc["serialEnabled"] = config.enableSerial;
        doc["serialLevel"] = config.serialLevel;
        doc["udpEnabled"] = config.enableUDP;
        doc["udpLevel"] = config.udpLevel;
        doc["syslogPort"] = (config.syslogPort[0] << 8) | config.syslogPort[1];
        
        // Add level names for display
        doc["serialLevelName"] = logger->getLevelName(static_cast<EventSeverity>(config.serialLevel));
        doc["udpLevelName"] = logger->getLevelName(static_cast<EventSeverity>(config.udpLevel));
        
        String response;
        serializeJsonPretty(doc, response);
        request->send(200, "application/json", response);
    });
    
    // POST to update EventLogger configuration
    server->on("/api/eventlogger/config", HTTP_POST, 
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        },
        nullptr,  // No upload handler
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            // Body handler
            if (index == 0) {
                // First chunk, parse the JSON
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, data, len);
                if (error) {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                    return;
                }
                
                EventLogger* logger = EventLogger::getInstance();
                
                // Update settings if provided
                if (doc["serialEnabled"].is<bool>()) {
                    logger->enableSerial(doc["serialEnabled"]);
                }
                if (doc["serialLevel"].is<int>()) {
                    logger->setSerialLevel(static_cast<EventSeverity>(doc["serialLevel"].as<int>()));
                }
                if (doc["udpEnabled"].is<bool>()) {
                    logger->enableUDP(doc["udpEnabled"]);
                }
                if (doc["udpLevel"].is<int>()) {
                    logger->setUDPLevel(static_cast<EventSeverity>(doc["udpLevel"].as<int>()));
                }
                
                // Save configuration to EEPROM
                logger->saveConfig();
            }
        }
    );
    
    // GET EventLogger stats
    server->on("/api/eventlogger/stats", HTTP_GET, [](AsyncWebServerRequest* request) {
        EventLogger* logger = EventLogger::getInstance();
        
        JsonDocument doc;
        doc["eventCount"] = logger->getEventCount();
        doc["uptime"] = millis();
        doc["startupMode"] = logger->isStartupMode();
        
        String response;
        serializeJsonPretty(doc, response);
        request->send(200, "application/json", response);
    });
    
    // POST to reset EventLogger stats
    server->on("/api/eventlogger/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        EventLogger* logger = EventLogger::getInstance();
        logger->resetEventCount();
        request->send(200, "application/json", "{\"status\":\"reset complete\"}");
    });
}

void WebManager::handleEventLoggerPage(AsyncWebServerRequest* request) {
    EventLogger* logger = EventLogger::getInstance();
    EventConfig& config = logger->getConfig();
    
    // Prepare template variables
    String serialChecked = config.enableSerial ? "checked" : "";
    String udpChecked = config.enableUDP ? "checked" : "";
    String serialOptions = buildLevelOptions(config.serialLevel);
    String udpOptions = buildLevelOptions(config.udpLevel);
    String syslogPort = String((config.syslogPort[0] << 8) | config.syslogPort[1]);
    
    // Load template from PROGMEM and process replacements
    String html = FPSTR(WebPageSelector::getEventLoggerPage(currentLanguage));
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    html.replace("%SERIAL_ENABLED%", serialChecked);
    html.replace("%UDP_ENABLED%", udpChecked);
    html.replace("%SERIAL_LEVEL_OPTIONS%", serialOptions);
    html.replace("%UDP_LEVEL_OPTIONS%", udpOptions);
    html.replace("%SYSLOG_PORT%", syslogPort);
    
    request->send(200, "text/html", html);
}

String WebManager::buildLevelOptions(uint8_t selectedLevel) {
    EventLogger* logger = EventLogger::getInstance();
    String options;
    for (int i = 0; i <= 7; i++) {
        options += F("<option value='");
        options += String(i);
        options += F("'");
        if (selectedLevel == i) options += F(" selected");
        options += F(">");
        options += logger->getLevelName(static_cast<EventSeverity>(i));
        options += F("</option>");
    }
    return options;
}

void WebManager::handleNotFound(AsyncWebServerRequest* request) {
    String message = "404 Not Found\n\n";
    message += "URI: ";
    message += request->url();
    message += "\nMethod: ";
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    
    request->send(404, "text/plain", message);
}