# Hardware Refactor Phase 3 - Implementation

## Overview
Phase 3 implements proper pin ownership management with a clean protocol for dynamic pin transfers, particularly for pins that switch between analog and digital modes.

## Changes Made

### 1. Created PinOwnershipManager Class
A new class that provides:
- **Pin ownership tracking** - Who currently owns each dynamic pin
- **Ownership requests** - Modules can request ownership of pins
- **Ownership transfers** - Clean handoff between modules
- **Conflict detection** - Prevents multiple modules from claiming the same pin
- **Mode tracking** - Tracks current pin mode (INPUT, OUTPUT, etc.)

### 2. Updated ADProcessor
- **Requests ownership** of KICKOUT_A pin during initialization
- **Tracks ownership** to prevent conflicts
- **Logs warnings** if ownership cannot be obtained

### 3. Updated EncoderProcessor
- **Requests ownership** of pins when encoder is enabled
- **Transfers ownership** from ADProcessor for KICKOUT_A when needed
- **Releases ownership** when encoder is disabled
- **Properly cleans up** interrupts and pin modes

### 4. Fixed Pin Initialization Issues
- **PWMMotorDriver**: Fixed current pin to use INPUT_DISABLE
- **KickoutMonitor**: Removed duplicate pin initialization
- **EncoderProcessor**: Added proper ownership management

## Pin Ownership Protocol

### For Static Pins (Single Owner)
```cpp
// Module initialization
PinOwnershipManager* pinMgr = PinOwnershipManager::getInstance();
if (pinMgr->requestPinOwnership(pin, OWNER_ID, "ModuleName")) {
    pinMode(pin, mode);
    pinMgr->updatePinMode(pin, mode);
}
```

### For Dynamic Pins (Multiple Owners)
```cpp
// Transfer ownership
if (pinMgr->transferPinOwnership(pin, 
    FROM_OWNER, TO_OWNER, "NewOwnerName", 
    cleanupCallback)) {
    // Configure pin for new mode
    pinMode(pin, newMode);
    pinMgr->updatePinMode(pin, newMode);
}
```

## Example: KICKOUT_A Pin Transfer

### Pressure Sensor → Encoder
1. ADProcessor owns KICKOUT_A in analog mode (INPUT_DISABLE)
2. User enables encoder in AgOpenGPS
3. EncoderProcessor requests transfer from ADProcessor
4. ADProcessor's analog readings stop
5. EncoderProcessor configures pin for digital input
6. Encoder starts counting

### Encoder → Pressure Sensor
1. EncoderProcessor owns KICKOUT_A in digital mode
2. User disables encoder in AgOpenGPS
3. EncoderProcessor releases ownership
4. Pin is reset to INPUT_DISABLE
5. ADProcessor can reclaim ownership
6. Pressure readings resume

## Benefits Achieved

### 1. Clear Ownership
- Every pin has exactly one owner at any time
- Ownership is logged and trackable
- Conflicts are detected and reported

### 2. Clean Transfers
- Proper cleanup when switching modes
- Interrupts are detached
- Pin modes are reset
- No lingering configuration

### 3. Debugging Support
- Ownership changes are logged
- Conflicts generate warnings
- Current owner can be queried

### 4. Future Extensibility
- Easy to add new dynamic pins
- Protocol can be extended for new requirements
- Callback support for custom cleanup

## Remaining Considerations

### 1. Initialization Order
Current order works well:
1. HardwareManager (defines pins)
2. ADProcessor (claims analog pins)
3. Motor drivers (claim their pins)
4. EncoderProcessor (may transfer pins)

### 2. Error Handling
- Modules should handle ownership denial gracefully
- Consider fallback behavior if ownership fails
- Log errors for debugging

### 3. Startup State
- ADProcessor gets first claim on KICKOUT_A
- This matches the most common use case (pressure sensor)
- Encoder can still take ownership when needed

## Testing Checklist
- [ ] Pressure sensor works on startup
- [ ] Encoder can be enabled and takes ownership
- [ ] Encoder can be disabled and releases ownership
- [ ] Pressure sensor resumes after encoder disabled
- [ ] No pin conflicts in logs
- [ ] All ownership changes are logged
- [ ] Current sensing still works
- [ ] All motor types initialize correctly