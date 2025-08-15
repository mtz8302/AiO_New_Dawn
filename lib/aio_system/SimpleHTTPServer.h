// SimpleHTTPServer.h
// Lightweight HTTP server implementation using QNEthernet
// Optimized for serving PROGMEM content efficiently

#ifndef SIMPLE_HTTP_SERVER_H
#define SIMPLE_HTTP_SERVER_H

#include <Arduino.h>
#include <QNEthernet.h>
#include <functional>
#include <vector>

using namespace qindesign::network;

// Route handler function type - simplified for efficiency
using HTTPHandler = std::function<void(EthernetClient&, const String&, const String&)>;

// Simple HTTP server optimized for PROGMEM content
class SimpleHTTPServer {
public:
    SimpleHTTPServer();
    ~SimpleHTTPServer();
    
    // Server control
    bool begin(uint16_t port = 80);
    void stop();
    void handleClient();
    
    // Route registration - only GET and POST supported
    void on(const String& path, HTTPHandler handler);
    
    // Server info
    bool isRunning() const { return running; }
    uint16_t getPort() const { return serverPort; }
    
    // Helper methods for responses
    static void send(EthernetClient& client, int code, const String& contentType, const String& content);
    static void sendP(EthernetClient& client, int code, const String& contentType, const char* content);
    static void sendJSON(EthernetClient& client, const String& json);
    static void redirect(EthernetClient& client, const String& location);
    
private:
    struct Route {
        String path;
        HTTPHandler handler;
    };
    
    EthernetServer server;
    std::vector<Route> routes;
    uint16_t serverPort;
    bool running;
    
    // Request parsing
    bool parseRequest(EthernetClient& client, String& method, String& path, String& query);
    
    // Route matching
    Route* findRoute(const String& path);
    
    // Default 404 handler
    void handleNotFound(EthernetClient& client);
};

#endif // SIMPLE_HTTP_SERVER_H