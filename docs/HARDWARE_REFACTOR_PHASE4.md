# Hardware Refactor Phase 4 - Shared Resource Coordination

## Overview
Phase 4 implements a coordination system for shared hardware resources that multiple modules need to configure, preventing conflicts and ensuring proper settings.

## Changes Made

### 1. Created SharedResourceManager Class
A comprehensive resource coordination system that manages:
- **PWM Frequencies** - Per timer group (pins sharing timers)
- **PWM Resolution** - Global setting affecting all PWM
- **ADC Configuration** - Resolution, averaging per ADC module
- **I2C Bus Speeds** - Clock speed per bus with override support

### 2. Updated PWMMotorDriver
- Requests 8-bit PWM resolution through SharedResourceManager
- Requests 75Hz PWM frequency for motor pins
- Handles frequency changes through the manager
- Logs warnings if requests fail due to conflicts

### 3. Updated PWMProcessor  
- Requests 12-bit PWM resolution (conflicts with motor's 8-bit!)
- Requests variable PWM frequencies for speed pulse
- Dynamic frequency changes go through manager

### 4. Updated ADProcessor
- Registers ADC0 configuration (12-bit, 4x averaging)
- Registers ADC1 configuration (12-bit, no averaging)
- Documents ADC ownership

### 5. Updated I2CManager
- Registers I2C bus speeds during initialization
- All buses default to 400kHz (FAST mode)

### 6. Updated MachineProcessor
- Requests I2C speed override to 1MHz
- Handles denial gracefully with warning

### 7. Deprecated HardwareManager PWM
- setPWMFrequency now logs warning
- Directs users to use motor drivers instead

## Resource Conflicts Detected

### PWM Resolution Conflict
- **PWMMotorDriver** wants 8-bit (0-255 range)
- **PWMProcessor** wants 12-bit (0-4095 range)
- **Impact**: First module wins, second gets warning
- **Solution**: May need to standardize or find compromise

### PWM Timer Groups
The manager understands Teensy 4.1 timer groups:
- Pins on same timer must share frequency
- Manager prevents conflicting frequencies
- Example: Pins 5 and 6 on different timers (OK for motor)

### I2C Speed Increases
- Manager allows speed increases (400kHz → 1MHz)
- Logs warning but permits (faster is usually OK)
- Prevents speed decreases that could break devices

## Benefits Achieved

### 1. Conflict Detection
```
[WARNING] PWM resolution conflict: PWMProcessor wants 12-bit, PWMMotorDriver has 8-bit
[WARNING] PWM frequency conflict on timer group 3: Module2 wants 1000Hz, Module1 has 100Hz
```

### 2. Resource Tracking
```cpp
SharedResourceManager::getInstance()->printStatus();
// Outputs:
// PWM Resolution: 8-bit (owner: PWMMotorDriver)
// PWM Timer Group 4: 75Hz (owner: PWMMotorDriver)
// ADC0: 12-bit, 4 avg (owner: ADProcessor)
// I2C Bus 0: 1000000Hz (owner: MachineProcessor)
```

### 3. Graceful Degradation
- Modules handle denial and continue with defaults
- System remains functional even with conflicts
- Warnings guide resolution

### 4. Future Extensibility
- Easy to add new resource types
- Can add priority system if needed
- Could add resource release/sharing

## Remaining Issues

### 1. PWM Resolution Conflict
Need to decide:
- Standardize on 8-bit for all PWM?
- Use 12-bit and scale motor values?
- Add resolution negotiation?

### 2. Initialization Order
Current order matters:
1. First module to request wins
2. Later modules get warnings
3. May need priority system

### 3. Resource Release
Currently no way to:
- Release a resource
- Change ownership
- Downgrade settings

## Usage Example

```cpp
// In module initialization
SharedResourceManager* resMgr = SharedResourceManager::getInstance();

// Request PWM settings
if (!resMgr->requestPWMResolution(8, "MyModule")) {
    LOG_WARNING("Could not get 8-bit PWM");
    // Handle gracefully
}

if (!resMgr->requestPWMFrequency(pin, 1000, "MyModule")) {
    LOG_WARNING("Could not set PWM frequency");
    // Use whatever is set
}

// Check what's configured
uint32_t currentFreq = resMgr->getPWMFrequency(timerGroup);
uint8_t currentRes = resMgr->getPWMResolution();
```

## Testing Checklist
- [ ] Motor driver gets 8-bit PWM
- [ ] Speed pulse works despite resolution conflict
- [ ] ADC configurations are tracked
- [ ] I2C speed override works for PCA9685
- [ ] Conflict warnings appear in logs
- [ ] System remains functional with conflicts
- [ ] Resource status can be printed