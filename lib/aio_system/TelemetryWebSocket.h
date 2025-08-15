// TelemetryWebSocket.h
// WebSocket server for high-frequency telemetry streaming

#ifndef TELEMETRY_WEBSOCKET_H
#define TELEMETRY_WEBSOCKET_H

#include <Arduino.h>
#include <QNEthernet.h>
#include "SimpleWebSocket.h"
#include <map>

using namespace qindesign::network;

// Binary telemetry packet structure (packed for efficiency)
struct __attribute__((packed)) TelemetryPacket {
    uint32_t timestamp;      // millis()
    float was_angle;         // Wheel angle sensor
    float was_angle_target;  // Target angle
    int16_t encoder_count;   // Encoder position
    float current_draw;      // Motor current
    float speed_kph;         // Vehicle speed
    float heading;           // Compass heading
    uint16_t status_flags;   // Various status bits
    uint8_t steer_switch;    // Steering switch state
    uint8_t work_switch;     // Work switch state (digital)
    uint8_t work_analog_percent; // Analog work switch percentage (0-100)
    uint8_t reserved[1];     // Padding to 32 bytes
};

class TelemetryWebSocket {
public:
    TelemetryWebSocket();
    ~TelemetryWebSocket();
    
    // Initialize and start the WebSocket server
    bool begin(uint16_t port = 8081);
    
    // Stop the server
    void stop();
    
    // Process client connections (call from main loop)
    void handleClient();
    
    // Broadcast telemetry to all connected clients
    void broadcastTelemetry(const TelemetryPacket& packet);
    
    // Get server status
    bool isRunning() const { return running; }
    uint16_t getPort() const { return serverPort; }
    size_t getClientCount() const;
    
private:
    static const uint8_t MAX_CLIENTS = 4;
    
    SimpleWebSocketServer wsServer;
    EthernetServer httpServer;  // For serving test page
    uint16_t serverPort;
    bool running;
    
    // Track client update rates
    uint32_t lastBroadcast;
    uint16_t broadcastRateHz;
    
    // Handle WebSocket messages
    void handleWebSocketMessage(const uint8_t* data, size_t length, bool binary);
    
    // Handle HTTP requests for test page
    void handleHttpRequest();
    
    // Send WebSocket test page
    void sendTestPage(EthernetClient& client);
};

#endif // TELEMETRY_WEBSOCKET_H