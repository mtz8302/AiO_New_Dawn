#ifndef NAV_PROCESSOR_H
#define NAV_PROCESSOR_H

#include "Arduino.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h"
#include "elapsedMillis.h"

enum class NavMessageType {
    NONE,
    PANDA,  // Single GPS with/without IMU
    PAOGI   // Dual GPS with/without IMU
};

class NAVProcessor {
private:
    static NAVProcessor* instance;
    
    
    // Message buffer
    static constexpr size_t BUFFER_SIZE = 256;
    char messageBuffer[BUFFER_SIZE];
    
    // Timing control
    elapsedMillis timeSinceLastMessage;
    static constexpr uint32_t MESSAGE_INTERVAL_MS = 100; // 10Hz
    
    // Statistics
    struct Stats {
        uint32_t pandaMessagesSent;
        uint32_t paogiMessagesSent;
        uint32_t messageErrors;
        uint32_t lastMessageTime;
    } stats;
    
    // Private constructor for singleton
    NAVProcessor();
    
    // Message formatting methods
    NavMessageType selectMessageType();
    bool formatPANDAMessage();
    bool formatPAOGIMessage();
    
    // Simple startup checks
    bool startupCheckComplete;
    
    // Utility methods
    void convertToNMEACoordinates(double decimalDegrees, bool isLongitude, 
                                  double& nmeaValue, char& direction);
    uint8_t calculateNMEAChecksum(const char* sentence);
    void sendMessage(const char* message);
    
public:
    ~NAVProcessor();
    
    // Singleton access
    static NAVProcessor* getInstance();
    static void init();
    
    // Main processing method
    void process();
    
    // Configuration
    void setMessageRate(uint32_t intervalMs);
    
    // Status and debugging
    void printStatus();
    const Stats& getStats() const { return stats; }
    NavMessageType getCurrentMessageType();
};

// Global pointer declaration
extern NAVProcessor* navPTR;

#endif // NAV_PROCESSOR_H