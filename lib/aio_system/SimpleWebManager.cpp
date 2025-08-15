// SimpleWebManager.cpp
// Web server implementation using SimpleHTTPServer to replace AsyncWebServer

#include "SimpleWebManager.h"
#include "EventLogger.h"
#include "Version.h"
#include "HardwareManager.h"
#include "ConfigManager.h"
#include "QNetworkBase.h"
#include "ADProcessor.h"
#include "EncoderProcessor.h"
#include "SimpleOTAHandler.h"
#include "TelemetryWebSocket.h"
#include "web_pages/CommonStyles.h"  // Common CSS
#include "web_pages/SimpleDeviceSettingsNoReplace.h"  // Device settings without replacements
#include "web_pages/SimpleHomePage.h"  // New simplified home page
#include "web_pages/SimplePlaceholderPages.h"  // Placeholder pages
#include "web_pages/SimpleEventLoggerPage.h"  // Event logger page
#include "web_pages/SimpleNetworkPage.h"  // Simple network settings page
#include "web_pages/SimpleAnalogWorkSwitchPage.h"  // Analog work switch page
#include "web_pages/SimpleOTAPageFixed.h"  // Fixed OTA update page
#include <ArduinoJson.h>
#include <QNEthernet.h>

using namespace qindesign::network;

// FPSTR macro for PROGMEM strings
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

// External references
extern EncoderProcessor* encoderProcessor;
// extern AnalogWorkSwitchHandler* analogWorkSwitchHandler;  // TODO: Find correct handler

SimpleWebManager::SimpleWebManager() : 
    isRunning(false),
    currentLanguage(WebLanguage::ENGLISH),
    systemReady(false),
    lastTelemetryUpdate(0) {
}

SimpleWebManager::~SimpleWebManager() {
    stop();
}

bool SimpleWebManager::begin(uint16_t port) {
    // Load language preference from EEPROM
    uint8_t savedLang = EEPROM.read(WEB_CONFIG_ADDR);
    if (savedLang <= 1) {  // 0 = English, 1 = German
        currentLanguage = static_cast<WebLanguage>(savedLang);
    }
    
    // Setup routes first
    setupRoutes();
    
    // Start HTTP server
    if (!httpServer.begin(port)) {
        LOG_ERROR(EventSource::NETWORK, "Failed to start HTTP server");
        return false;
    }
    
    // Start WebSocket telemetry server on port 8082
    if (!telemetryWS.begin(8082)) {
        LOG_WARNING(EventSource::NETWORK, "Failed to start WebSocket telemetry server");
    }
    
    isRunning = true;
    
    IPAddress ip = Ethernet.localIP();
    LOG_INFO(EventSource::NETWORK, "Simple web server started on http://%d.%d.%d.%d:%d", 
             ip[0], ip[1], ip[2], ip[3], port);
    
    return true;
}

void SimpleWebManager::stop() {
    if (isRunning) {
        telemetryWS.stop();
        httpServer.stop();
        isRunning = false;
        LOG_INFO(EventSource::NETWORK, "Simple web server stopped");
    }
}

void SimpleWebManager::handleClient() {
    httpServer.handleClient();
    telemetryWS.handleClients();
}

void SimpleWebManager::setupRoutes() {
    // Home page
    httpServer.on("/", [this](EthernetClient& client, const String& method, const String& query) {
        sendHomePage(client);
    });
    
    // API status endpoint
    httpServer.on("/api/status", [this](EthernetClient& client, const String& method, const String& query) {
        handleApiStatus(client);
    });
    
    // EventLogger page
    httpServer.on("/eventlogger", [this](EthernetClient& client, const String& method, const String& query) {
        sendEventLoggerPage(client);
    });
    
    // Network settings page
    httpServer.on("/network", [this](EthernetClient& client, const String& method, const String& query) {
        sendNetworkPage(client);
    });
    
    // OTA Update page
    httpServer.on("/ota", [this](EthernetClient& client, const String& method, const String& query) {
        sendOTAPage(client);
    });
    
    // Device Settings page
    httpServer.on("/device", [this](EthernetClient& client, const String& method, const String& query) {
        sendDeviceSettingsPage(client);
    });
    
    // Analog Work Switch page
    httpServer.on("/analogworkswitch", [this](EthernetClient& client, const String& method, const String& query) {
        sendAnalogWorkSwitchPage(client);
    });
    
    // WAS Demo page removed - using WebSocket telemetry instead
    
    // Language selection
    httpServer.on("/lang/en", [this](EthernetClient& client, const String& method, const String& query) {
        currentLanguage = WebLanguage::ENGLISH;
        EEPROM.write(WEB_CONFIG_ADDR, static_cast<uint8_t>(currentLanguage));
        SimpleHTTPServer::redirect(client, "/");
    });
    
    httpServer.on("/lang/de", [this](EthernetClient& client, const String& method, const String& query) {
        currentLanguage = WebLanguage::GERMAN;
        EEPROM.write(WEB_CONFIG_ADDR, static_cast<uint8_t>(currentLanguage));
        SimpleHTTPServer::redirect(client, "/");
    });
    
    // API endpoints
    httpServer.on("/api/restart", [](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"restarting\"}");
            delay(100);
            SCB_AIRCR = 0x05FA0004;  // System reset
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    // EventLogger API
    httpServer.on("/api/eventlogger/config", [this](EthernetClient& client, const String& method, const String& query) {
        handleEventLoggerConfig(client, method);
    });
    
    // Network API
    httpServer.on("/api/network/config", [this](EthernetClient& client, const String& method, const String& query) {
        handleNetworkConfig(client, method);
    });
    
    // Device settings API
    httpServer.on("/api/device/settings", [this](EthernetClient& client, const String& method, const String& query) {
        handleDeviceSettings(client, method);
    });
    
    // Analog work switch API
    httpServer.on("/api/analogworkswitch/status", [this](EthernetClient& client, const String& method, const String& query) {
        handleAnalogWorkSwitchStatus(client);
    });
    
    httpServer.on("/api/analogworkswitch/config", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleAnalogWorkSwitchConfig(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    httpServer.on("/api/analogworkswitch/setpoint", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleAnalogWorkSwitchSetpoint(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    // OTA upload endpoint
    httpServer.on("/api/ota/upload", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleOTAUpload(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    // Note: Removed polling endpoints like /api/was/angle and /api/encoder/count
    // These are now provided via WebSocket telemetry
    
    LOG_INFO(EventSource::NETWORK, "Simple web routes configured");
}

// Page handlers

void SimpleWebManager::sendHomePage(EthernetClient& client) {
    extern const char SIMPLE_HOME_PAGE[];
    String html = FPSTR(SIMPLE_HOME_PAGE);
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    html.replace("%FIRMWARE_VERSION%", FIRMWARE_VERSION);
    
    SimpleHTTPServer::send(client, 200, "text/html", html);
}

void SimpleWebManager::sendEventLoggerPage(EthernetClient& client) {
    String html = FPSTR(SIMPLE_EVENTLOGGER_PAGE);
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    
    // TODO: Get actual EventLogger config when available
    // For now, use defaults
    
    // Replace checkbox states
    html.replace("%SERIAL_ENABLED%", "checked");  // Default to enabled
    html.replace("%UDP_ENABLED%", "");  // Default to disabled
    html.replace("%RATE_LIMIT_DISABLED%", "");  // Default to rate limiting on
    
    // Build level options - default to INFO (level 3)
    html.replace("%SERIAL_LEVEL_OPTIONS%", buildLevelOptions(3));
    html.replace("%UDP_LEVEL_OPTIONS%", buildLevelOptions(3));
    
    SimpleHTTPServer::send(client, 200, "text/html", html);
}

void SimpleWebManager::sendNetworkPage(EthernetClient& client) {
    extern const char SIMPLE_NETWORK_SETTINGS_PAGE[];
    String html = FPSTR(SIMPLE_NETWORK_SETTINGS_PAGE);
    
    // Get current network info
    IPAddress ip = Ethernet.localIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    html.replace("%IP_ADDRESS%", ipStr);
    
    // Get link speed
    int linkSpeed = Ethernet.linkSpeed();
    html.replace("%LINK_SPEED%", String(linkSpeed));
    
    SimpleHTTPServer::send(client, 200, "text/html", html);
}

void SimpleWebManager::sendOTAPage(EthernetClient& client) {
    extern const char SIMPLE_OTA_UPDATE_PAGE_FIXED[];
    
    // Send directly from PROGMEM without string manipulation
    SimpleHTTPServer::sendP(client, 200, "text/html", SIMPLE_OTA_UPDATE_PAGE_FIXED);
}

void SimpleWebManager::sendDeviceSettingsPage(EthernetClient& client) {
    extern const char SIMPLE_DEVICE_SETTINGS_NO_REPLACE[];
    
    // Send directly from PROGMEM without any string manipulation
    SimpleHTTPServer::sendP(client, 200, "text/html", SIMPLE_DEVICE_SETTINGS_NO_REPLACE);
}

void SimpleWebManager::sendAnalogWorkSwitchPage(EthernetClient& client) {
    // Send the page directly without replacements
    SimpleHTTPServer::sendP(client, 200, "text/html", SIMPLE_ANALOG_WORK_SWITCH_PAGE);
}

// WAS Demo page removed - using WebSocket telemetry instead

// API handlers

void SimpleWebManager::handleApiStatus(EthernetClient& client) {
    StaticJsonDocument<512> doc;
    
    // Basic system info
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis();
    doc["freeMemory"] = 0; // TODO: Calculate free memory
    
    // Network info
    JsonObject network = doc.createNestedObject("network");
    IPAddress localIP = Ethernet.localIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    network["ip"] = ipStr;
    network["connected"] = Ethernet.linkState();
    
    // Module info
    doc["deviceType"] = "Steer";  // TODO: Get from config when available
    doc["moduleId"] = 126;  // TODO: Get from config when available
    
    // System status
    doc["systemHealthy"] = true;
    
    String json;
    serializeJson(doc, json);
    SimpleHTTPServer::sendJSON(client, json);
}

void SimpleWebManager::handleEventLoggerConfig(EthernetClient& client, const String& method) {
    if (method == "GET") {
        // Return current configuration
        StaticJsonDocument<256> doc;
        doc["serialEnabled"] = true;  // TODO: Get from actual config
        doc["serialLevel"] = 3;  // INFO
        doc["udpEnabled"] = false;
        doc["udpLevel"] = 3;  // INFO
        doc["rateLimitDisabled"] = false;
        
        String json;
        serializeJson(doc, json);
        SimpleHTTPServer::sendJSON(client, json);
        
    } else if (method == "POST") {
        // Read POST body
        String body = readPostBody(client);
        
        // Parse JSON
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        
        // TODO: Actually save these settings when EventLogger config is available
        bool serialEnabled = doc["serialEnabled"] | true;
        bool udpEnabled = doc["udpEnabled"] | false;
        int serialLevel = doc["serialLevel"] | 3;
        int udpLevel = doc["udpLevel"] | 3;
        bool rateLimitDisabled = doc["rateLimitDisabled"] | false;
        
        LOG_INFO(EventSource::NETWORK, "EventLogger config: Serial=%d/%d, UDP=%d/%d, RateLimit=%d", 
                 serialEnabled, serialLevel, udpEnabled, udpLevel, rateLimitDisabled);
        
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"saved\"}");
        
    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void SimpleWebManager::handleNetworkConfig(EthernetClient& client, const String& method) {
    if (method == "GET") {
        // Return current IP as array
        IPAddress ip = Ethernet.localIP();
        
        StaticJsonDocument<128> doc;
        JsonArray ipArray = doc.createNestedArray("ip");
        ipArray.add(ip[0]);
        ipArray.add(ip[1]);
        ipArray.add(ip[2]);
        ipArray.add(ip[3]);
        
        String json;
        serializeJson(doc, json);
        SimpleHTTPServer::sendJSON(client, json);
        
    } else if (method == "POST") {
        // Read POST body
        String body = readPostBody(client);
        
        // Parse JSON
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        
        // Get IP array
        JsonArray ipArray = doc["ip"];
        if (ipArray.size() >= 3) {
            uint8_t octet1 = ipArray[0];
            uint8_t octet2 = ipArray[1];
            uint8_t octet3 = ipArray[2];
            
            // Update network configuration
            extern NetworkConfig netConfig;
            extern void save_current_net();
            
            // Update IP addresses (keeping 4th octet as 126)
            netConfig.ipAddress[0] = octet1;
            netConfig.ipAddress[1] = octet2;
            netConfig.ipAddress[2] = octet3;
            netConfig.ipAddress[3] = 126;
            
            // Update current IP to match
            netConfig.currentIP[0] = octet1;
            netConfig.currentIP[1] = octet2;
            netConfig.currentIP[2] = octet3;
            netConfig.currentIP[3] = 126;
            netConfig.currentIP[4] = 0;
            
            // Update broadcast IP for the new subnet
            netConfig.broadcastIP[0] = octet1;
            netConfig.broadcastIP[1] = octet2;
            netConfig.broadcastIP[2] = octet3;
            netConfig.broadcastIP[3] = 255;
            netConfig.broadcastIP[4] = 0;
            
            // Update destination IP to broadcast on new subnet
            netConfig.destIP[0] = octet1;
            netConfig.destIP[1] = octet2;
            netConfig.destIP[2] = octet3;
            netConfig.destIP[3] = 255;
            
            // Update gateway to .1 on new subnet
            netConfig.gateway[0] = octet1;
            netConfig.gateway[1] = octet2;
            netConfig.gateway[2] = octet3;
            netConfig.gateway[3] = 1;
            
            // Save to EEPROM
            save_current_net();
            
            LOG_INFO(EventSource::NETWORK, "Network IP saved: %d.%d.%d.126 (reboot required)", 
                     octet1, octet2, octet3);
            
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"ok\"}");
        } else {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"error\":\"Invalid IP format\"}");
        }
        
    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void SimpleWebManager::handleDeviceSettings(EthernetClient& client, const String& method) {
    if (method == "GET") {
        // Return current settings from ConfigManager
        ConfigManager* config = ConfigManager::getInstance();
        
        StaticJsonDocument<256> doc;
        doc["deviceType"] = "Steer";  // TODO: Get from config when available
        doc["moduleId"] = 126;  // Steer module ID
        doc["udpPassthrough"] = config->getGPSPassThrough();
        doc["sensorFusion"] = false;  // TODO: Get from actual config when implemented
        doc["pwmBrakeMode"] = config->getPWMBrakeMode();
        doc["encoderType"] = config->getEncoderType();
        
        String json;
        serializeJson(doc, json);
        SimpleHTTPServer::sendJSON(client, json);
        
    } else if (method == "POST") {
        // Read POST body
        String body = readPostBody(client);
        
        // Parse JSON
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        
        // Extract settings from JSON
        bool udpPassthrough = doc["udpPassthrough"] | false;
        bool sensorFusion = doc["sensorFusion"] | false;
        bool pwmBrakeMode = doc["pwmBrakeMode"] | false;
        int encoderType = doc["encoderType"] | 1;
        
        // Save to ConfigManager
        ConfigManager* config = ConfigManager::getInstance();
        config->setGPSPassThrough(udpPassthrough);
        config->setPWMBrakeMode(pwmBrakeMode);
        config->setEncoderType(encoderType);
        // TODO: Add setSensorFusion() when implemented in ConfigManager
        
        // Save to EEPROM
        config->saveTurnSensorConfig();  // This saves encoder type
        config->saveSteerConfig();       // This saves PWM brake mode
        config->saveGPSConfig();         // This saves GPS passthrough
        
        LOG_INFO(EventSource::NETWORK, "Device settings saved: UDP=%d, Brake=%d, Encoder=%d", 
                 udpPassthrough, pwmBrakeMode, encoderType);
        
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"saved\"}");
        
    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void SimpleWebManager::handleAnalogWorkSwitchStatus(EthernetClient& client) {
    LOG_DEBUG(EventSource::NETWORK, "Analog work switch status requested");
    ADProcessor* adProc = ADProcessor::getInstance();
    if (!adProc) {
        LOG_ERROR(EventSource::NETWORK, "ADProcessor not available");
        SimpleHTTPServer::send(client, 503, "application/json", "{\"error\":\"ADProcessor not available\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    doc["enabled"] = adProc->isAnalogWorkSwitchEnabled();
    doc["setpoint"] = (int)round(adProc->getWorkSwitchSetpoint());
    doc["hysteresis"] = (int)round(adProc->getWorkSwitchHysteresis());
    doc["invert"] = adProc->getInvertWorkSwitch();
    doc["percent"] = adProc->getWorkSwitchAnalogPercent();
    doc["state"] = adProc->isWorkSwitchOn();
    doc["raw"] = adProc->getWorkSwitchAnalogRaw();
    
    String json;
    serializeJson(doc, json);
    SimpleHTTPServer::sendJSON(client, json);
}

void SimpleWebManager::handleAnalogWorkSwitchConfig(EthernetClient& client) {
    // Read POST body
    String body = readPostBody(client);
    
    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    ADProcessor* adProc = ADProcessor::getInstance();
    if (!adProc) {
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"ADProcessor not available\"}");
        return;
    }
    
    // Update settings if provided
    if (doc.containsKey("enabled")) {
        adProc->setAnalogWorkSwitchEnabled(doc["enabled"]);
    }
    if (doc.containsKey("hysteresis")) {
        adProc->setWorkSwitchHysteresis(doc["hysteresis"]);
    }
    if (doc.containsKey("invert")) {
        adProc->setInvertWorkSwitch(doc["invert"]);
    }
    
    LOG_INFO(EventSource::NETWORK, "Analog work switch config updated");
    
    SimpleHTTPServer::sendJSON(client, "{\"status\":\"saved\"}");
}

void SimpleWebManager::handleAnalogWorkSwitchSetpoint(EthernetClient& client) {
    ADProcessor* adProc = ADProcessor::getInstance();
    if (!adProc) {
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"ADProcessor not available\"}");
        return;
    }
    
    // Set current reading as new setpoint
    float currentPercent = adProc->getWorkSwitchAnalogPercent();
    adProc->setWorkSwitchSetpoint(currentPercent);
    
    StaticJsonDocument<128> doc;
    doc["status"] = "saved";
    doc["newSetpoint"] = (int)round(currentPercent);
    
    String json;
    serializeJson(doc, json);
    SimpleHTTPServer::sendJSON(client, json);
    
    LOG_INFO(EventSource::NETWORK, "Analog work switch setpoint set to %.1f%%", currentPercent);
}

void SimpleWebManager::handleOTAUpload(EthernetClient& client) {
    // OTA upload request received
    
    // Initialize OTA handler if needed
    static bool otaInitialized = false;
    if (!otaInitialized) {
        // Initialize OTA handler
        if (!SimpleOTAHandler::init()) {
            LOG_ERROR(EventSource::NETWORK, "OTA init failed");
            SimpleHTTPServer::send(client, 500, "text/plain", "OTA init failed");
            return;
        }
        otaInitialized = true;
    }
    
    // Reset OTA handler for new upload
    SimpleOTAHandler::reset();
    
    // Read hex data from client
    
    // Read data in chunks directly from client
    uint8_t buffer[1024];
    size_t totalBytes = 0;
    bool foundStart = false;
    
    // Set a longer timeout for large files
    unsigned long timeout = 30000; // 30 seconds
    unsigned long start = millis();
    unsigned long lastDataTime = millis();
    
    while (client.connected() && (millis() - start < timeout) && !SimpleOTAHandler::isComplete()) {
        if (client.available()) {
            size_t bytesRead = client.readBytes(buffer, sizeof(buffer));
            
            if (bytesRead > 0) {
                lastDataTime = millis(); // Track when we last received data
                
                // Check for hex file start on first chunk
                if (!foundStart) {
                    // Look for ':' which starts a hex file
                    for (size_t i = 0; i < bytesRead; i++) {
                        if (buffer[i] == ':') {
                            foundStart = true;
                            // Process from this point
                            if (!SimpleOTAHandler::processChunk(&buffer[i], bytesRead - i)) {
                                const char* error = SimpleOTAHandler::getError();
                                LOG_ERROR(EventSource::NETWORK, "OTA processing failed: %s", error ? error : "Unknown error");
                                SimpleHTTPServer::send(client, 400, "text/plain", error ? error : "Processing failed");
                                return;
                            }
                            totalBytes += (bytesRead - i);
                            break;
                        }
                    }
                    if (!foundStart) {
                        LOG_ERROR(EventSource::NETWORK, "No hex data found in first chunk");
                        SimpleHTTPServer::send(client, 400, "text/plain", "Invalid hex file format");
                        return;
                    }
                } else {
                    // Process subsequent chunks
                    if (!SimpleOTAHandler::processChunk(buffer, bytesRead)) {
                        const char* error = SimpleOTAHandler::getError();
                        LOG_ERROR(EventSource::NETWORK, "OTA processing failed: %s", error ? error : "Unknown error");
                        SimpleHTTPServer::send(client, 400, "text/plain", error ? error : "Processing failed");
                        return;
                    }
                    totalBytes += bytesRead;
                }
                
                // Reset timeout on data received
                start = millis();
                
                // Log progress every 10KB
                if (totalBytes % 10240 < 1024) {
                    // Progress: totalBytes
                }
                
                // Check if OTA is complete
                if (SimpleOTAHandler::isComplete()) {
                    // Upload complete
                    break;
                }
            }
        } else {
            // No data available
            if (foundStart && millis() - lastDataTime > 1000) {
                // If we've started receiving data but haven't gotten any new data for 1 second,
                // assume the upload is complete
                // No data for 1 second, assuming complete
                break;
            }
            
            delay(1);
            
            // Also check for completion during idle time
            if (SimpleOTAHandler::isComplete()) {
                // Upload complete during idle
                break;
            }
        }
    }
    
    LOG_INFO(EventSource::NETWORK, "Received %lu total bytes", totalBytes);
    
    if (totalBytes == 0) {
        LOG_ERROR(EventSource::NETWORK, "No data received");
        SimpleHTTPServer::send(client, 400, "text/plain", "No data received");
        return;
    }
    
    // Check if we need to finalize even without explicit EOF
    if (!SimpleOTAHandler::isComplete() && foundStart) {
        // No EOF record found, check if valid
        // Process any remaining data in buffer
        if (SimpleOTAHandler::processChunk((const uint8_t*)"\n", 1)) {
            // Force a newline to process any pending hex line
        }
    }
    
    // Finalize OTA upload
    
    // Finalize the upload
    if (SimpleOTAHandler::finalize()) {
        LOG_INFO(EventSource::NETWORK, "OTA upload successful, sending response");
        
        // Send a quick success response
        SimpleHTTPServer::send(client, 200, "text/plain", "OK");
        
        // Make sure the response is fully sent
        client.flush();
        client.stop();  // Close the connection cleanly
        
        // Small delay to ensure TCP FIN is sent
        delay(100);
        
        // Apply the update
        LOG_INFO(EventSource::NETWORK, "Applying firmware update now");
        SimpleOTAHandler::applyUpdate();
    } else {
        const char* error = SimpleOTAHandler::getError();
        LOG_ERROR(EventSource::NETWORK, "OTA finalization failed: %s", error ? error : "Unknown error");
        SimpleHTTPServer::send(client, 400, "text/plain", error ? error : "Finalization failed");
    }
}

// Helper methods

String SimpleWebManager::readPostBody(EthernetClient& client) {
    String body;
    
    // Note: SimpleHTTPServer should have already consumed headers
    // Just read any remaining data
    int timeout = 100; // 100ms timeout
    unsigned long start = millis();
    
    while (millis() - start < timeout) {
        while (client.available()) {
            char c = client.read();
            body += c;
            start = millis(); // Reset timeout on data
        }
        if (body.length() > 0 && !client.available()) {
            // We have data and no more is immediately available
            delay(10); // Small delay to see if more data arrives
            if (!client.available()) {
                break; // No more data
            }
        }
    }
    
    return body;
}

String SimpleWebManager::buildLevelOptions(uint8_t selectedLevel) {
    String options;
    const char* levels[] = {"OFF", "ERROR", "WARNING", "INFO", "DEBUG", "VERBOSE"};
    
    for (uint8_t i = 0; i < 6; i++) {
        options += "<option value='" + String(i) + "'";
        if (i == selectedLevel) {
            options += " selected";
        }
        options += ">" + String(levels[i]) + "</option>";
    }
    
    return options;
}

// Telemetry broadcast

void SimpleWebManager::broadcastTelemetry() {
    // Rate limit to configured rate
    uint32_t now = millis();
    
    // Start with faster rate to prime the connection, then slow down
    static uint32_t connectionStart = 0;
    static bool connectionPrimed = false;
    
    if (telemetryWS.getClientCount() > 0 && connectionStart == 0) {
        connectionStart = now;
        connectionPrimed = false;
        LOG_DEBUG(EventSource::NETWORK, "WebSocket client connected, priming connection");
    } else if (telemetryWS.getClientCount() == 0) {
        connectionStart = 0;
        connectionPrimed = false;
    }
    
    // Determine target interval based on connection state
    uint32_t targetInterval;
    if (!connectionPrimed && connectionStart > 0 && now - connectionStart < 5000) {
        targetInterval = 5;  // 5ms = 200Hz for first 5 seconds
    } else {
        connectionPrimed = true;
        targetInterval = 10;  // 10ms = 100Hz normal rate
    }
    
    if (now - lastTelemetryUpdate < targetInterval) {
        return;
    }
    
    // Build telemetry packet
    TelemetryPacket packet;
    packet.timestamp = now;
    
    // Get WAS data
    ADProcessor* adProc = ADProcessor::getInstance();
    if (adProc) {
        packet.was_angle = adProc->getWASAngle();
        packet.was_angle_target = 0;  // TODO: Get target angle when available
        packet.current_draw = adProc->getMotorCurrent() / 1000.0f;  // Convert mA to A
    } else {
        packet.was_angle = 0;
        packet.was_angle_target = 0;
        packet.current_draw = 0;
    }
    
    // Get encoder data
    if (encoderProcessor) {
        packet.encoder_count = encoderProcessor->getPulseCount();
    } else {
        packet.encoder_count = 0;
    }
    
    // Get switch states from ADProcessor
    if (adProc) {
        packet.steer_switch = adProc->isSteerSwitchOn() ? 1 : 0;
        packet.work_switch = adProc->isWorkSwitchOn() ? 1 : 0;
        packet.work_analog_percent = (uint8_t)round(adProc->getWorkSwitchAnalogPercent());
    } else {
        packet.steer_switch = 0;
        packet.work_switch = 0;
        packet.work_analog_percent = 0;
    }
    
    // TODO: Fill in remaining fields
    packet.speed_kph = 0;
    packet.heading = 0;
    packet.status_flags = 0;
    packet.reserved[0] = 0;
    
    // Broadcast to all connected clients
    telemetryWS.broadcastBinary((const uint8_t*)&packet, sizeof(packet));
    
    lastTelemetryUpdate = now;
}