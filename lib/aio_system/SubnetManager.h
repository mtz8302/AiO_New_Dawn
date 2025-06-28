#pragma once

#include <Arduino.h>

class SubnetManager {
private:
    static SubnetManager* instance;
    
    // Private constructor for singleton
    SubnetManager() {}
    
    // PGN handler
    static void handlePGN201(uint8_t pgn, const uint8_t* data, size_t len);
    
public:
    // Get singleton instance
    static SubnetManager* getInstance();
    
    // Initialize and register PGN handler
    static bool init();
};