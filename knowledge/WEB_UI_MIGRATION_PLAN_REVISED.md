# REVISED Web UI Migration Plan: Mongoose to QNEthernet + AsyncWebServer

## Critical Issue: Network Stack Replacement
Mongoose currently provides:
- Complete TCP/IP stack (baremetal)
- DHCP client
- DNS resolver
- HTTP/WebSocket server
- Network interface management

This must ALL be replaced with QNEthernet.

## Revised Approach: Complete Network Stack Migration

### Phase 0: Network Stack Analysis (NEW - Checkpoint 0)
**Goal**: Understand current Mongoose network usage

1. **Inventory all Mongoose network functions**
   ```cpp
   // Current mongoose usage:
   - mg_mgr_init()           // Network manager
   - mg_http_listen()        // HTTP server
   - mg_timer_poll()         // Network polling
   - mg_json_*               // JSON parsing
   - DHCP client functions
   - DNS functions
   ```

2. **Map to QNEthernet equivalents**
   ```cpp
   // QNEthernet replacements:
   - Ethernet.begin()        // Network init
   - Ethernet.localIP()      // IP management
   - Ethernet.DHCP()         // DHCP client
   - DNSClient               // DNS resolver
   - AsyncWebServer          // HTTP/WS server
   ```

3. **Identify integration points**
   - Where does main.cpp poll mongoose?
   - How do other modules check network status?
   - What depends on mongoose timers?

### Phase 1: Parallel Network Stack (REVISED - Checkpoint 1)
**Goal**: Run QNEthernet alongside Mongoose temporarily

1. **Conditional compilation**
   ```cpp
   #ifdef USE_QNETHERNET
     #include <QNEthernet.h>
     using namespace qindesign::network;
   #else
     #include "mongoose.h"
   #endif
   ```

2. **Create NetworkManager abstraction**
   ```cpp
   class NetworkManager {
   public:
     virtual bool begin() = 0;
     virtual void update() = 0;
     virtual bool isConnected() = 0;
     virtual IPAddress getIP() = 0;
   };
   
   class MongooseNetworkManager : public NetworkManager { };
   class QNEthernetNetworkManager : public NetworkManager { };
   ```

3. **Dual network initialization**
   ```cpp
   void setup() {
     #ifdef USE_QNETHERNET
       networkMgr = new QNEthernetNetworkManager();
       // QNEthernet on different subnet for testing
       // e.g., Mongoose: 192.168.1.x, QNEthernet: 192.168.2.x
     #else
       networkMgr = new MongooseNetworkManager();
     #endif
   }
   ```

4. **Compile & Test Checkpoint 1**
   - Verify both stacks can coexist
   - Test with USE_QNETHERNET flag
   - Monitor for conflicts
   - Check memory usage

### Phase 2: EventLogger Network Abstraction (Checkpoint 2)
**Goal**: Make EventLogger network-agnostic

1. **Abstract syslog UDP**
   ```cpp
   class SyslogSender {
   public:
     virtual bool send(IPAddress ip, uint16_t port, 
                      const char* message) = 0;
   };
   
   class MongooseSyslog : public SyslogSender { };
   class QNEthernetSyslog : public SyslogSender { };
   ```

2. **Update EventLogger**
   - Use abstraction instead of direct mongoose calls
   - Test with both implementations

3. **Compile & Test Checkpoint 2**
   - Verify syslog works with both stacks
   - Test switching between implementations
   - Verify no data loss

### Phase 3: Replace Network Services (Checkpoint 3)
**Goal**: Switch core network services to QNEthernet

1. **DHCP Migration**
   ```cpp
   // Old (Mongoose)
   mg_tcpip_init(mgr, &tcpip_cfg);
   
   // New (QNEthernet)
   Ethernet.begin();  // Uses DHCP by default
   ```

2. **DNS Migration**
   ```cpp
   // Old (Mongoose)
   mg_resolve(mgr, name, callback);
   
   // New (QNEthernet)
   DNSClient dns;
   dns.begin(Ethernet.dnsServerIP());
   dns.getHostByName(name, ip);
   ```

3. **Network Status Monitoring**
   - Replace mongoose link detection
   - Update speed negotiation handling

4. **Compile & Test Checkpoint 3**
   - Test DHCP acquisition
   - Verify DNS resolution
   - Check network stability

### Phase 4: Web Server Migration (Checkpoint 4)
**Goal**: Replace Mongoose HTTP with AsyncWebServer

[Previous phases 2-6 from original plan go here, renumbered as 4-8]

### Phase 9: Remove Mongoose Completely (Checkpoint 9)
**Goal**: Final cleanup and removal

1. **Remove abstraction layers**
   - Delete Mongoose implementations
   - Simplify to QNEthernet only

2. **Clean up includes and defines**

3. **Final system test**

## Network Architecture Comparison

### Current (Mongoose)
```
Application Layer
    ↓
Mongoose HTTP/WS
    ↓
Mongoose TCP/IP Stack
    ↓
Ethernet PHY
```

### Target (QNEthernet)
```
Application Layer
    ↓
AsyncWebServer
    ↓
QNEthernet TCP/IP Stack
    ↓
Ethernet PHY
```

## Risk Mitigation

1. **Parallel Testing**
   - Run both stacks on different IPs/ports
   - A/B testing before cutover

2. **Gradual Migration**
   - One service at a time
   - Maintain working system throughout

3. **Rollback Points**
   - Each checkpoint is reversible
   - Keep Mongoose code until proven

## Required Libraries (UPDATED)

1. **QNEthernet** - Full network stack
   - Already in project
   - Includes DHCP, DNS, etc.

2. **AsyncWebServer_Teensy41** - Web server only
   - Requires QNEthernet for network
   - Not a standalone network stack

3. **ArduinoJson** - JSON handling
   - Replaces mg_json_*

## Critical Considerations

1. **Memory Usage**
   - QNEthernet uses more RAM than Mongoose
   - Need to verify sufficient memory

2. **Performance**
   - QNEthernet is Arduino-style (may be slower)
   - Mongoose is optimized for embedded

3. **Compatibility**
   - Ensure all network features have equivalents
   - Some Mongoose features may need custom implementation

## Go/No-Go Decision Points

After Phase 3 (Network Services):
- Is QNEthernet stable?
- Is performance acceptable?
- Is memory usage acceptable?

If NO to any → Keep Mongoose and abandon migration
If YES to all → Continue with web migration

## Timeline (REVISED)
- Phase 0: 4-5 hours (analysis)
- Phase 1: 6-8 hours (parallel stacks)
- Phase 2: 4-5 hours (abstractions)
- Phase 3: 8-10 hours (network services)
- Phases 4-9: 20-27 hours (as before)

**Total: 42-55 hours**

## Recommendation

This is a MAJOR architectural change. Consider:
1. Is the current Mongoose solution causing actual problems?
2. Would the effort be better spent elsewhere?
3. Could we keep Mongoose for networking and just customize the web UI?

The safest approach might be to use Mongoose's custom HTTP handler feature to serve your own HTML/JS while keeping the proven network stack.