// AsyncUDPHandler.h
// Async UDP handler using AsyncUDP_Teensy41 library
// Replaces the polling-based UDP reception with async callbacks

#ifndef ASYNCUDPHANDLER_H
#define ASYNCUDPHANDLER_H

#include <stdint.h>

class AsyncUDPHandler {
public:
    static void init();
    static void sendUDPPacket(uint8_t* data, int length);
    static void poll();  // Periodic network status check
    
    // DHCP Server control
    static void enableDHCPServer(bool enable);
    static bool isDHCPServerEnabled();
    
private:
    static bool dhcpServerEnabled;
};

#endif // ASYNCUDPHANDLER_H