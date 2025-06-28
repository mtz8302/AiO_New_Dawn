#ifndef EVENTLOGGER_H_
#define EVENTLOGGER_H_

#include "Arduino.h"
#include <vector>
#include <cstdarg>

// Event severity levels (following syslog standards)
enum class EventSeverity : uint8_t {
    EMERGENCY = 0,  // System is unusable
    ALERT = 1,      // Action must be taken immediately
    CRITICAL = 2,   // Critical conditions
    ERROR = 3,      // Error conditions
    WARNING = 4,    // Warning conditions
    NOTICE = 5,     // Normal but significant condition
    INFO = 6,       // Informational messages
    DEBUG = 7       // Debug-level messages
};

// Event sources (facilities in syslog terms)
enum class EventSource : uint8_t {
    SYSTEM = 0,
    NETWORK = 1,
    GNSS = 2,
    IMU = 3,
    AUTOSTEER = 4,
    MACHINE = 5,
    CAN = 6,
    CONFIG = 7,
    USER = 8
};

// Event configuration stored in EEPROM
struct EventConfig {
    uint8_t serialLevel = static_cast<uint8_t>(EventSeverity::INFO);     // Default to INFO and above
    uint8_t udpLevel = static_cast<uint8_t>(EventSeverity::WARNING);     // Default to WARNING and above
    bool enableSerial = true;
    bool enableUDP = false;
    uint8_t syslogPort[2] = {2, 2};  // Port 514 (0x0202)
    uint8_t reserved[10];  // Future expansion
};

class EventLogger {
private:
    static EventLogger* instance;
    EventConfig config;
    char messageBuffer[256];
    uint32_t eventCounter = 0;
    
    // For rate limiting
    uint32_t lastLogTime[8] = {0};  // Per severity level
    uint32_t rateLimit[8] = {0, 0, 100, 100, 200, 500, 1000, 2000};  // ms between logs per level
    
    // Severity names for display
    const char* severityNames[8] = {
        "EMERG", "ALERT", "CRIT", "ERROR", "WARN", "NOTICE", "INFO", "DEBUG"
    };
    
    // Source names for display
    const char* sourceNames[9] = {
        "SYS", "NET", "GNSS", "IMU", "STEER", "MACH", "CAN", "CFG", "USER"
    };
    
    EventLogger();
    
    // Format message according to RFC3164 syslog format
    void formatSyslog(EventSeverity severity, EventSource source, const char* message);
    
    // Send to outputs
    void outputSerial(EventSeverity severity, EventSource source, const char* message);
    void outputUDP(EventSeverity severity, EventSource source, const char* message);
    
    // Check if we should log this severity level
    bool shouldLog(EventSeverity severity, bool forUDP = false);
    
    // Rate limiting check
    bool checkRateLimit(EventSeverity severity);
    
    // Mongoose logging
    int mongooseLogLevel = 3;  // Default to debug during startup
    bool mongooseLogReduced = false;
    
    // Startup mode tracking
    bool startupMode = true;  // Don't enforce levels during startup

public:
    ~EventLogger();
    
    // Singleton access
    static EventLogger* getInstance();
    static void init();
    
    // Main logging function
    void log(EventSeverity severity, EventSource source, const char* format, ...);
    
    // Configuration
    void loadConfig();
    void saveConfig();
    void setSerialLevel(EventSeverity level);
    void setUDPLevel(EventSeverity level);
    void enableSerial(bool enable);
    void enableUDP(bool enable);
    
    // Get current config
    EventConfig& getConfig() { return config; }
    
    // Utility to convert string to severity
    EventSeverity stringToSeverity(const char* str);
    const char* severityToString(EventSeverity severity);
    const char* sourceToString(EventSource source);
    
    // Statistics
    uint32_t getEventCount() { return eventCounter; }
    void resetEventCount() { eventCounter = 0; }
    
    // Display current configuration
    void printConfig();
    
    // Mongoose log control
    void setMongooseLogLevel(int level);
    int getMongooseLogLevel() { return mongooseLogLevel; }
    void checkNetworkReady();  // Auto-adjust Mongoose logging when network is ready
    
    // System startup logging control
    void setStartupMode(bool startup);
    bool isStartupMode() { return startupMode; }
    
    // Get the effective log level for UDP syslog
    EventSeverity getEffectiveLogLevel() {
        return static_cast<EventSeverity>(config.udpLevel);
    }
    
    // Get human-readable name for a log level
    const char* getLevelName(EventSeverity level) {
        switch(level) {
            case EventSeverity::EMERGENCY: return "EMERGENCY";
            case EventSeverity::ALERT: return "ALERT";
            case EventSeverity::CRITICAL: return "CRITICAL";
            case EventSeverity::ERROR: return "ERROR";
            case EventSeverity::WARNING: return "WARNING";
            case EventSeverity::NOTICE: return "NOTICE";
            case EventSeverity::INFO: return "INFO";
            case EventSeverity::DEBUG: return "DEBUG";
            default: return "UNKNOWN";
        }
    }
};

// Convenience macros for common logging patterns
#define LOG_EMERGENCY(source, ...) EventLogger::getInstance()->log(EventSeverity::EMERGENCY, source, __VA_ARGS__)
#define LOG_ALERT(source, ...) EventLogger::getInstance()->log(EventSeverity::ALERT, source, __VA_ARGS__)
#define LOG_CRITICAL(source, ...) EventLogger::getInstance()->log(EventSeverity::CRITICAL, source, __VA_ARGS__)
#define LOG_ERROR(source, ...) EventLogger::getInstance()->log(EventSeverity::ERROR, source, __VA_ARGS__)
#define LOG_WARNING(source, ...) EventLogger::getInstance()->log(EventSeverity::WARNING, source, __VA_ARGS__)
#define LOG_NOTICE(source, ...) EventLogger::getInstance()->log(EventSeverity::NOTICE, source, __VA_ARGS__)
#define LOG_INFO(source, ...) EventLogger::getInstance()->log(EventSeverity::INFO, source, __VA_ARGS__)
#define LOG_DEBUG(source, ...) EventLogger::getInstance()->log(EventSeverity::DEBUG, source, __VA_ARGS__)

#endif // EVENTLOGGER_H_