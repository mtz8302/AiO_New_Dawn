# Blocking Operations Analysis

## Summary of Potential Blocking Operations

### 1. Main Loop GPS Processing (`src/main.cpp`)
**Lines 463-467**: While loop reading GPS1 data
```cpp
while (SerialGPS1.available())
{
    char c = SerialGPS1.read();
    gnssProcessor.processNMEAChar(c);
}
```
**Risk**: If GPS1 is sending data continuously at high baud rate, this could consume significant time.
**Mitigation**: Limit number of bytes processed per loop iteration.

### 2. Serial Bridge Mode (`lib/aio_communications/SerialManager.cpp`)
**Multiple locations**: Unbounded while loops for bridging serial data
- Lines 216-219: GPS1 to USB1 bridge
- Lines 224-227: USB1 to GPS1 bridge  
- Lines 238-241: GPS2 to USB2 bridge
- Lines 246-249: USB2 to GPS2 bridge

**Risk**: These are unbounded loops that will process ALL available data. If data is coming in faster than it's being sent out, this could block indefinitely.
**Note**: These appear to only be active in bridge mode, not normal operation.

### 3. IMU Processor (`lib/aio_navigation/IMUProcessor.cpp`)
**During initialization only** (not in main loop):
- Lines 99-102: Clearing serial buffer
- Lines 108-124: Waiting for BNO data with delay(5)
- Lines 151-154: Clearing serial buffer  
- Lines 161-174: Waiting for TM171 data with delay(10)

**In process() functions** (called from main loop):
- Lines 210-214: processBNO() - while loop processing all available bytes
- Lines 244-248: processTM171() - while loop processing all available bytes

**Risk**: The process() functions have unbounded while loops that could consume time if IMU is sending lots of data.

### 4. Delays in Setup/Initialization
Various delays during setup (not in main loop):
- `main.cpp` line 53: delay(5000) - startup delay
- `main.cpp` line 84: delay(100) - network negotiation
- `main.cpp` line 89: delay(2000) - network stabilization
- `main.cpp` line 112: delay(10) - EventLogger init
- `main.cpp` line 129: delay(100) - buzzer beep
- `main.cpp` line 168: delay(100) - before I2C init

### 5. Web System (`lib/aio_system/WebSystem.cpp`)
- Line 188: delay(100) - after restart command
- Line 829: delay(100) - before firmware update
- Line 836: while(1) {} - infinite loop waiting for reset
- Lines 718-724: while loop processing OTA data

**Risk**: The while(1) is intentional for system reset. The OTA while loop is bounded by data length.

### 6. Other Minor Delays
- `I2CManager.cpp` line 67: delay(10) - I2C bus stabilization
- `MachineProcessor.cpp` line 148: delay(1) - PCA9685 oscillator stabilization

## Recommendations

### High Priority Fixes
1. **GPS Processing in main loop**: Add byte limit per iteration
2. **IMU Processing in main loop**: Add byte limit per iteration
3. **Serial Bridge Mode**: Add byte limits or make non-blocking

### Medium Priority 
1. Consider making serial operations non-blocking throughout
2. Add timeouts to any operations that wait for responses

### Low Priority
1. Setup delays are acceptable as they're one-time
2. Hardware stabilization delays are necessary

## Code Patterns to Avoid
1. `while (Serial.available())` without byte limits
2. `delay()` calls in frequently called functions
3. Waiting for specific responses without timeouts
4. `flush()` operations on serial ports (none found)
5. Blocking `waitFor` operations (none found)