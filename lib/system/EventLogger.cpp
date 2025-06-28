#include "EventLogger.h"
#include "EEPROMLayout.h"
#include "EEPROM.h"
#include "mongoose.h"
#include <cstdio>
#include <cstring>

// Forward declaration of NetConfigStruct from NetworkBase
struct NetConfigStruct {
    uint8_t currentIP[5];
    uint8_t gatewayIP[5];
    uint8_t broadcastIP[5];
};

// External declarations from NetworkBase
extern struct mg_mgr g_mgr;
extern struct mg_connection *sendAgio;
extern NetConfigStruct netConfig;

// Mongoose logging - mg_log_set is a macro, we need the actual variable
extern int mg_log_level;

// Network states from mongoose
#define MG_TCPIP_STATE_DOWN 0   // Interface is down
#define MG_TCPIP_STATE_UP 1     // Interface is up
#define MG_TCPIP_STATE_REQ 2    // Interface is up, DHCP REQUESTING state
#define MG_TCPIP_STATE_IP 3     // Interface is up and has an IP assigned
#define MG_TCPIP_STATE_READY 4  // Interface has fully come up, ready to work

// Static instance pointer
EventLogger* EventLogger::instance = nullptr;

// Use EEPROM layout from header

EventLogger::EventLogger() {
    loadConfig();
    
    // Set initial Mongoose log level for startup debugging
    mg_log_level = 3;  // Debug level during startup
}

EventLogger::~EventLogger() {
    instance = nullptr;
}

EventLogger* EventLogger::getInstance() {
    if (instance == nullptr) {
        instance = new EventLogger();
    }
    return instance;
}

void EventLogger::init() {
    getInstance();  // Create instance if needed
}

void EventLogger::log(EventSeverity severity, EventSource source, const char* format, ...) {
    // Check rate limiting first
    if (!checkRateLimit(severity)) {
        return;
    }
    
    // Format the message
    va_list args;
    va_start(args, format);
    vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
    va_end(args);
    
    eventCounter++;
    
    // Output to enabled channels
    if (config.enableSerial && shouldLog(severity, false)) {
        outputSerial(severity, source, messageBuffer);
    }
    
    if (config.enableUDP && shouldLog(severity, true)) {
        outputUDP(severity, source, messageBuffer);
    }
}

void EventLogger::outputSerial(EventSeverity severity, EventSource source, const char* message) {
    // Format: [HH:MM:SS.mmm] SEVERITY/SOURCE: message
    uint32_t now = millis();
    uint32_t hours = (now / 3600000) % 24;
    uint32_t minutes = (now / 60000) % 60;
    uint32_t seconds = (now / 1000) % 60;
    uint32_t milliseconds = now % 1000;
    
    Serial.printf("[%02lu:%02lu:%02lu.%03lu] %s/%s: %s\r\n",
                  hours, minutes, seconds, milliseconds,
                  severityNames[static_cast<uint8_t>(severity)],
                  sourceNames[static_cast<uint8_t>(source)],
                  message);
}

void EventLogger::outputUDP(EventSeverity severity, EventSource source, const char* message) {
    // RFC3164 syslog format: <priority>timestamp hostname tag: message
    // Priority = facility * 8 + severity
    // We'll use facility 16 (local0) as base + our source as offset
    uint8_t facility = 16 + static_cast<uint8_t>(source);
    uint8_t priority = facility * 8 + static_cast<uint8_t>(severity);
    
    // Get timestamp in syslog format (simplified - no RTC)
    uint32_t now = millis();
    uint32_t days = now / 86400000;
    uint32_t hours = (now / 3600000) % 24;
    uint32_t minutes = (now / 60000) % 60;
    uint32_t seconds = (now / 1000) % 60;
    
    // Month approximation (30 days per month)
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    uint8_t month = (days / 30) % 12;
    uint8_t day = (days % 30) + 1;
    
    // Format syslog message
    char syslogMsg[512];
    snprintf(syslogMsg, sizeof(syslogMsg), 
             "<%d>%s %2d %02lu:%02lu:%02lu AiO-%s[%lu]: %s",
             priority,
             months[month],
             day,
             hours, minutes, seconds,
             sourceNames[static_cast<uint8_t>(source)],
             eventCounter,
             message);
    
    // Send via UDP broadcast to syslog port
    extern struct mg_connection *sendAgio;
    extern NetConfigStruct netConfig;
    
    if (sendAgio && g_mgr.ifp->state == MG_TCPIP_STATE_READY) {
        // Create a new connection for syslog (port 514)
        char syslogURL[32];
        uint16_t port = (config.syslogPort[0] << 8) | config.syslogPort[1];
        snprintf(syslogURL, sizeof(syslogURL), "udp://%d.%d.%d.255:%d",
                 netConfig.currentIP[0], netConfig.currentIP[1], 
                 netConfig.currentIP[2], port);
        
        struct mg_connection *syslogConn = mg_connect(&g_mgr, syslogURL, NULL, NULL);
        if (syslogConn) {
            mg_send(syslogConn, syslogMsg, strlen(syslogMsg));
            mg_iobuf_del(&syslogConn->send, 0, syslogConn->send.len);
            syslogConn->is_closing = 1;  // Close after send
        }
    }
}

bool EventLogger::shouldLog(EventSeverity severity, bool forUDP) {
    // During startup, log everything to serial (UDP still respects configured level)
    if (startupMode && !forUDP) {
        return true;  // Log all messages during startup
    }
    
    uint8_t level = forUDP ? config.udpLevel : config.serialLevel;
    return static_cast<uint8_t>(severity) <= level;
}

bool EventLogger::checkRateLimit(EventSeverity severity) {
    // No rate limiting during startup mode
    if (startupMode) {
        return true;
    }
    
    uint8_t sevIndex = static_cast<uint8_t>(severity);
    uint32_t now = millis();
    
    if (rateLimit[sevIndex] == 0) {
        return true;  // No rate limit for this level
    }
    
    if (now - lastLogTime[sevIndex] >= rateLimit[sevIndex]) {
        lastLogTime[sevIndex] = now;
        return true;
    }
    
    return false;
}

void EventLogger::loadConfig() {
    // Check if we have valid config in EEPROM
    uint8_t marker;
    EEPROM.get(EVENT_CONFIG_ADDR - 1, marker);
    
    if (marker == 0xEE) {  // Valid config marker
        EEPROM.get(EVENT_CONFIG_ADDR, config);
    } else {
        // Use defaults and save
        saveConfig();
    }
}

void EventLogger::saveConfig() {
    uint8_t marker = 0xEE;
    EEPROM.put(EVENT_CONFIG_ADDR - 1, marker);
    EEPROM.put(EVENT_CONFIG_ADDR, config);
}

void EventLogger::setSerialLevel(EventSeverity level) {
    config.serialLevel = static_cast<uint8_t>(level);
    saveConfig();
}

void EventLogger::setUDPLevel(EventSeverity level) {
    config.udpLevel = static_cast<uint8_t>(level);
    saveConfig();
}

void EventLogger::enableSerial(bool enable) {
    config.enableSerial = enable;
    saveConfig();
}

void EventLogger::enableUDP(bool enable) {
    config.enableUDP = enable;
    saveConfig();
}

EventSeverity EventLogger::stringToSeverity(const char* str) {
    for (int i = 0; i < 8; i++) {
        if (strcasecmp(str, severityNames[i]) == 0) {
            return static_cast<EventSeverity>(i);
        }
    }
    return EventSeverity::INFO;  // Default
}

const char* EventLogger::severityToString(EventSeverity severity) {
    uint8_t index = static_cast<uint8_t>(severity);
    if (index < 8) {
        return severityNames[index];
    }
    return "UNKNOWN";
}

const char* EventLogger::sourceToString(EventSource source) {
    uint8_t index = static_cast<uint8_t>(source);
    if (index < 9) {
        return sourceNames[index];
    }
    return "UNKNOWN";
}

void EventLogger::printConfig() {
    Serial.println("\r\n===== Event Logger Configuration =====");
    Serial.printf("Serial Output: %s (Level: %s%s)\r\n", 
                  config.enableSerial ? "ENABLED" : "DISABLED",
                  severityNames[config.serialLevel],
                  startupMode ? " - STARTUP MODE" : "");
    Serial.printf("UDP Syslog: %s (Level: %s, Port: %d)\r\n",
                  config.enableUDP ? "ENABLED" : "DISABLED",
                  severityNames[config.udpLevel],
                  (config.syslogPort[0] << 8) | config.syslogPort[1]);
    Serial.printf("Mongoose Logging: Level %d\r\n", mongooseLogLevel);
    Serial.printf("Total Events Logged: %lu\r\n", eventCounter);
    Serial.println("=====================================");
}

void EventLogger::setMongooseLogLevel(int level) {
    if (level < 0 || level > 4) {
        LOG_WARNING(EventSource::SYSTEM, "Invalid Mongoose log level %d, must be 0-4", level);
        return;
    }
    
    mongooseLogLevel = level;
    mg_log_level = level;
    
    LOG_INFO(EventSource::SYSTEM, "Set Mongoose log level to %d", level);
}

void EventLogger::checkNetworkReady() {
    // Check if network interface exists
    if (!g_mgr.ifp) return;
    
    // Check current network state
    bool networkReady = (g_mgr.ifp->state == MG_TCPIP_STATE_READY);
    
    // Track network state changes for stability
    if (!networkReady && networkWasReady) {
        // Network went down - reset our tracking
        networkWasReady = false;
        lastNetworkDownTime = millis();
    } else if (networkReady && !networkWasReady) {
        // Network came up - but wait to ensure it's stable
        if (millis() - lastNetworkDownTime > 1000) {  // Only if network was down for > 1 second
            networkWasReady = true;
            networkReadyTime = millis();
            
            // First time network is ready - reduce Mongoose logging
            if (!mongooseLogReduced) {
                mongooseLogReduced = true;
                setMongooseLogLevel(2);  // Reduce to info level once network is ready
                LOG_INFO(EventSource::NETWORK, "Network ready, reducing Mongoose log level to 2");
                
                // Log the assigned IP address
                uint8_t* ip = netConfig.currentIP;
                LOG_INFO(EventSource::NETWORK, "IP Address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            }
        }
    }
    
    // Show system ready message 3 seconds after network is stable
    if (!systemReadyShown && networkWasReady && networkReady && (millis() - networkReadyTime > 3000)) {
        systemReadyShown = true;
        
        // Temporarily increase Mongoose log level to reduce interference
        int savedMongooseLevel = mongooseLogLevel;
        setMongooseLogLevel(1);  // Reduce Mongoose logging temporarily
        
        // Display the complete boxed message as separate lines to avoid rate limiting
        // Use Serial.print directly for the visual box to ensure it displays properly
        Serial.println("\r\n**************************************************");
        Serial.printf("*** System ready - UDP syslog active at %s level ***\r\n", 
                      getLevelName(getEffectiveLogLevel()));
        Serial.println("*** Press '?' for menu, 'L' for logging control ***");
        Serial.println("**************************************************\r\n");
        
        // Send a syslog-friendly message with menu instructions
        LOG_WARNING(EventSource::SYSTEM, "* System ready - Press '?' for menu, 'L' for logging control *");
        
        // Restore Mongoose log level after a brief delay
        delay(50);
        setMongooseLogLevel(savedMongooseLevel);
    }
}

void EventLogger::setStartupMode(bool startup) {
    if (!startup && startupMode) {
        // Exiting startup mode - now enforce configured levels
        startupMode = false;
        LOG_INFO(EventSource::SYSTEM, "System initialization complete - enforcing log level: %s", 
                 severityNames[config.serialLevel]);
    }
    // Note: We don't support re-entering startup mode after exiting
}