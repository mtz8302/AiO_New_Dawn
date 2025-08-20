# Lightweight Web Implementation Plan

## Overview
Replace AsyncWebServer with a lightweight combination:
- **QNEthernet EthernetServer** for HTTP static pages and API endpoints
- **ArduinoWebsockets** for high-frequency telemetry streaming

This approach uses libraries already available and proven to work on Teensy 4.1.

## Architecture

```
Port 80:
├── QNEthernet EthernetServer (HTTP)
│   ├── Static pages (HTML/CSS/JS from PROGMEM)
│   ├── REST API endpoints (JSON)
│   └── Simple routing logic
│
└── ArduinoWebsockets Server (WebSocket)
    └── /ws/telemetry - Binary telemetry at 100Hz
```

## Phase 1: Simple HTTP Server (Day 1)

### Goal
Create lightweight HTTP server using QNEthernet's EthernetServer.

### Implementation
```cpp
class SimpleWebServer {
private:
    EthernetServer server{80};
    
public:
    void begin();
    void handleClient();
    
private:
    void handleRequest(EthernetClient& client);
    void sendResponse(EthernetClient& client, int code, 
                     const char* contentType, const char* content);
    void serveStatic(EthernetClient& client, const char* path);
    void handleAPI(EthernetClient& client, const char* path, const char* method);
};
```

### Features
- Parse HTTP requests (GET/POST)
- Route to handlers based on path
- Serve static content from PROGMEM
- Handle JSON API endpoints
- CORS headers for cross-origin requests

### Test
- Serve main HTML page
- Handle /api/status endpoint
- Verify JSON responses

## Phase 2: WebSocket Server (Day 1-2)

### Goal
Add WebSocket support using ArduinoWebsockets library.

### Implementation
```cpp
#include <ArduinoWebSockets.h>
using namespace websockets;

class TelemetryWebSocket {
private:
    WebsocketsServer wsServer;
    std::vector<WebsocketsClient> clients;
    
public:
    void begin(uint16_t port = 80);
    void handleClient();
    void broadcastTelemetry(const TelemetryPacket& packet);
    
private:
    void onMessage(WebsocketsMessage message);
    void onEvent(WebsocketsEvent event, String data);
};
```

### Features
- Accept WebSocket connections on same port as HTTP
- Binary protocol for telemetry packets
- Handle rate change commands
- Auto-disconnect slow clients

### Test
- Connect WebSocket client
- Send/receive binary telemetry
- Test 10Hz, 50Hz, 100Hz rates

## Phase 3: Migrate Endpoints (Day 2)

### Goal
Port all AsyncWebServer endpoints to SimpleWebServer.

### Endpoints to Migrate
```
GET  /                    -> index.html from PROGMEM
GET  /api/status          -> System status JSON
GET  /api/network/status  -> Network info JSON
GET  /api/device/settings -> Device config JSON
POST /api/device/settings -> Update device config
GET  /api/eventlogger/... -> Event logger endpoints
```

### Implementation Pattern
```cpp
void SimpleWebServer::handleAPI(EthernetClient& client, 
                               const char* path, 
                               const char* method) {
    if (strcmp(path, "/api/status") == 0 && strcmp(method, "GET") == 0) {
        JsonDocument doc;
        doc["uptime"] = millis();
        doc["version"] = VERSION;
        
        String json;
        serializeJson(doc, json);
        sendResponse(client, 200, "application/json", json.c_str());
    }
    // ... more endpoints
}
```

## Phase 4: Client Integration (Day 3)

### Goal
Update web UI to use new endpoints and WebSocket.

### Changes
1. Update WebSocket URL if needed
2. Remove SSE code (broken anyway)
3. Add WebSocket reconnection logic
4. Update API endpoint calls if needed

## Phase 5: Remove AsyncWebServer (Day 3-4)

### Goal
Complete migration and remove AsyncWebServer.

### Steps
1. Switch SimpleWebServer to port 80
2. Remove AsyncWebServer initialization
3. Delete AsyncWebServer library files
4. Update documentation

## Code Size Comparison

### Current (AsyncWebServer)
- AsyncWebServer_Teensy41: ~50KB
- Teensy41_AsyncTCP: ~20KB
- Total: ~70KB + RAM usage

### New (Lightweight)
- QNEthernet (already included): 0KB extra
- ArduinoWebsockets: ~15KB
- SimpleWebServer: ~5KB
- Total: ~20KB + minimal RAM

## Benefits

1. **Smaller footprint** - Save ~50KB flash
2. **Less RAM usage** - No async buffers
3. **Simpler code** - Direct request/response model
4. **Better WebSocket** - Library designed for Arduino/Teensy
5. **Easier debugging** - Synchronous operation

## Example: SimpleWebServer Skeleton

```cpp
void SimpleWebServer::handleRequest(EthernetClient& client) {
    char line[256];
    char method[16] = {0};
    char path[128] = {0};
    char version[16] = {0};
    
    // Read request line
    int len = client.readBytesUntil('\n', line, sizeof(line)-1);
    line[len] = '\0';
    
    // Parse request line
    sscanf(line, "%15s %127s %15s", method, path, version);
    
    // Skip headers for now (read until empty line)
    while (client.available()) {
        len = client.readBytesUntil('\n', line, sizeof(line)-1);
        if (len <= 1) break; // Empty line (just \r\n)
    }
    
    // Route request
    if (strncmp(path, "/api/", 5) == 0) {
        handleAPI(client, path, method);
    } else if (strcmp(path, "/") == 0) {
        serveStatic(client, "/index.html");
    } else {
        sendResponse(client, 404, "text/plain", "Not Found");
    }
}

void SimpleWebServer::sendResponse(EthernetClient& client, 
                                 int code, 
                                 const char* contentType, 
                                 const char* content) {
    client.printf("HTTP/1.1 %d OK\r\n", code);
    client.printf("Content-Type: %s\r\n", contentType);
    client.printf("Content-Length: %d\r\n", strlen(content));
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");
    client.print(content);
    client.flush();
}
```

This lightweight approach provides all needed functionality with minimal overhead.