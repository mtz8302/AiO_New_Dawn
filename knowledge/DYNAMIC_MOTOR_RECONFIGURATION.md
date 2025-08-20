# Dynamic Motor Driver Reconfiguration

## Current State
- Motor type changes trigger a system reboot
- Sensor type changes (encoder/pressure/current) are handled dynamically
- Pin ownership is now properly managed through HardwareManager

## Why Reboots Were Needed
1. **Pin conflicts**: Different motor drivers use different pins
2. **PWM settings**: Different frequencies and resolutions
3. **CAN initialization**: Keya driver needs CAN setup
4. **No cleanup**: Old driver wasn't properly released

## What's Changed with New Architecture
1. **Pin ownership tracking**: Can transfer pins between owners
2. **Resource coordination**: PWM settings tracked and managed
3. **Proper cleanup**: Modules can release resources

## Implementation Plan for Dynamic Motor Changes

### 1. Add Motor Driver Cleanup
```cpp
class MotorDriverInterface {
    virtual void deinit() = 0;  // Release all resources
};
```

### 2. Update Each Motor Driver
- **PWMMotorDriver**: Release PWM pins, restore pin modes
- **CytronMotorDriver**: Release DIR/PWM pins
- **KeyaCANDriver**: Unregister CAN callbacks
- **DanfossCANDriver**: Unregister CAN callbacks

### 3. Update MotorDriverManager
```cpp
void MotorDriverManager::switchMotorDriver(MotorDriverType newType) {
    // 1. Disable current driver
    if (currentDriver) {
        currentDriver->enable(false);
        currentDriver->deinit();
        delete currentDriver;
    }
    
    // 2. Create new driver
    currentDriver = createMotorDriver(newType, hwMgr, canMgr);
    
    // 3. Initialize new driver
    if (currentDriver) {
        currentDriver->init();
    }
    
    // 4. Update dependent modules
    kickoutMonitor->setMotorDriver(currentDriver);
    autosteerProcessor->setMotorDriver(currentDriver);
}
```

### 4. Handle Pin Transfers
Using HardwareManager's transfer mechanism:
```cpp
// Example: Switching from PWM to Cytron
hwMgr->transferPinOwnership(PWM1_PIN, 
    HardwareManager::OWNER_PWMMOTORDRIVER,
    HardwareManager::OWNER_CYTRONMOTORDRIVER,
    "CytronMotorDriver",
    [](uint8_t pin) { 
        digitalWrite(pin, LOW);  // Cleanup callback
    });
```

## Benefits
1. **No reboot required**: Faster configuration changes
2. **Safer**: Proper cleanup prevents conflicts
3. **Better user experience**: Seamless switching
4. **Consistent**: Matches sensor behavior

## Considerations
1. **Safety**: Motor must be disabled during switch
2. **State preservation**: Save motor position if applicable
3. **Timing**: Switch only when autosteer is inactive
4. **Testing**: Each transition path needs validation

## Current Workarounds
Since dynamic switching isn't implemented yet:
1. Motor changes still require reboot
2. Sensor changes work dynamically
3. This is acceptable for now since motor type rarely changes

## Future Enhancement
With the new HardwareManager architecture in place, implementing dynamic motor switching is now feasible and would complete the dynamic reconfiguration story.