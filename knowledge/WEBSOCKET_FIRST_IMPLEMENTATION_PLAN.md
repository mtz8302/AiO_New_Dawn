# WebSocket-First Implementation Plan

## Overview
Implement WebSocket telemetry first using ArduinoWebsockets, running alongside AsyncWebServer. Once WebSockets are proven to work at 100Hz, then migrate HTTP endpoints from AsyncWebServer to a lightweight QNEthernet-based server.

## Architecture (Phased Approach)

```
Phase 1-2: WebSocket Testing
Port 80: AsyncWebServer (existing)
Port 8081: ArduinoWebsockets Server (new)
         └── /ws/telemetry - Binary telemetry at 100Hz

Phase 3-5: Full Migration  
Port 80: QNEthernet EthernetServer (HTTP) + ArduinoWebsockets (WS)
```

## Phase 1: ArduinoWebsockets Setup (Day 1)

### Goal
Implement WebSocket server using ArduinoWebsockets library on port 8081, keeping AsyncWebServer unchanged.

### Tasks
1. **Copy ArduinoWebsockets library**
   ```bash
   cp -r test/ArduinoWebsockets-0.5.4 lib/ArduinoWebsockets
   ```

2. **Create TelemetryWebSocket class**
   ```cpp
   class TelemetryWebSocket {
   private:
       WebsocketsServer wsServer;
       std::vector<WebsocketsClient> clients;
       uint16_t port;
       
   public:
       bool begin(uint16_t port = 8081);
       void handleClient();
       void broadcastTelemetry(const TelemetryPacket& packet);
   };
   ```

3. **Implement binary protocol handler**
   - Accept WebSocket connections
   - Handle text commands (rate changes)
   - Broadcast binary telemetry packets

### Test
- Create test HTML page connecting to ws://192.168.5.126:8081/
- Verify connection establishment
- Test binary data reception

## Phase 2: WebSocket Performance Testing (Day 1-2)

### Goal
Verify WebSocket can handle 100Hz telemetry updates reliably.

### Implementation
```cpp
void TelemetryWebSocket::broadcastTelemetry(const TelemetryPacket& packet) {
    for (auto& client : clients) {
        if (client.available()) {
            client.sendBinary((char*)&packet, sizeof(TelemetryPacket));
        } else {
            // Mark for removal
        }
    }
    // Clean up disconnected clients
}
```

### Tests
1. **Single client at various rates**
   - 10Hz - Verify smooth updates
   - 50Hz - Check for delays
   - 100Hz - Confirm no data loss

2. **Multiple clients**
   - 3 clients at 10Hz
   - 3 clients at 50Hz
   - Stress test limits

3. **Long duration test**
   - Run for 1 hour at 100Hz
   - Monitor for memory leaks
   - Check connection stability

### Success Criteria
- Stable 100Hz updates to at least 2 clients
- No memory growth over time
- Automatic slow client disconnection

## Phase 3: Simple HTTP Server (Day 2)

### Goal
Create lightweight HTTP server using QNEthernet's EthernetServer on port 8080.

### Implementation
```cpp
class SimpleWebServer {
private:
    EthernetServer server;
    TelemetryWebSocket* wsServer;  // Reference to WebSocket server
    
public:
    bool begin(uint16_t port = 8080);
    void handleClient();
    void attachWebSocket(TelemetryWebSocket* ws) { wsServer = ws; }
};
```

### Features
- Basic HTTP request parsing
- Serve static pages from PROGMEM
- Handle API endpoints
- Redirect WebSocket upgrade requests to WebSocket server

### Test
- Access http://192.168.5.126:8080/test
- Verify static page serving
- Test simple API endpoint

## Phase 4: Migrate HTTP Endpoints (Day 2-3)

### Goal
Port all AsyncWebServer endpoints to SimpleWebServer, still running on port 8080.

### Endpoints Priority Order
1. **Static pages** (/, /index.html)
2. **Read-only API** (/api/status, /api/network/status)
3. **Configuration API** (/api/device/settings GET/POST)
4. **Event logger API** (/api/eventlogger/*)

### Parallel Testing
- AsyncWebServer on :80 (original)
- SimpleWebServer on :8080 (new)
- Compare responses for correctness

## Phase 5: Switchover and Cleanup (Day 3-4)

### Goal
Switch to new implementation and remove AsyncWebServer.

### Steps
1. **Update client code**
   - Change WebSocket URL to use same port as HTTP
   - Update any hardcoded ports

2. **Switch ports**
   - Stop AsyncWebServer
   - Move SimpleWebServer + WebSocket to port 80

3. **Remove AsyncWebServer**
   - Delete library files
   - Remove initialization code
   - Update documentation

4. **Optimize**
   - Tune buffer sizes
   - Minimize memory usage
   - Add connection limits

## Key Implementation Files

### TelemetryWebSocket.h
```cpp
#pragma once
#include <ArduinoWebSockets.h>
#include <vector>

struct TelemetryPacket {
    uint32_t timestamp;
    float was_angle;
    // ... other fields
} __attribute__((packed));

class TelemetryWebSocket {
    // ... as shown above
};
```

### SimpleWebServer.h
```cpp
#pragma once
#include <QNEthernet.h>

class SimpleWebServer {
    // ... as shown above
};
```

## Testing Plan for Each Phase

### Phase 1-2: WebSocket Only
- Run WebSocket test page on port 8081
- Keep using AsyncWebServer UI on port 80
- Monitor both simultaneously

### Phase 3-4: Parallel HTTP Servers  
- AsyncWebServer on :80 (production)
- SimpleWebServer on :8080 (testing)
- A/B test all endpoints

### Phase 5: Final Testing
- Full system test on port 80
- Verify all features work
- Performance benchmarks

## Advantages of This Approach

1. **Zero downtime** - WebSocket tested separately first
2. **Incremental migration** - Prove each piece works
3. **Easy rollback** - AsyncWebServer unchanged until final phase
4. **Performance validation** - Confirm 100Hz before committing
5. **Parallel testing** - Compare old vs new side-by-side

## Success Metrics

- ✅ WebSocket handles 100Hz telemetry
- ✅ HTTP server uses <10KB flash
- ✅ Total RAM usage reduced by >30%
- ✅ All existing endpoints work
- ✅ No increase in latency