# Hardware Manager Unification

## Overview
Unified PinOwnershipManager and SharedResourceManager into HardwareManager to create a single, cohesive hardware management system.

## Rationale
The three managers had overlapping responsibilities:
- **HardwareManager**: Pin definitions and hardware initialization
- **PinOwnershipManager**: Dynamic pin ownership tracking
- **SharedResourceManager**: Shared resource coordination (PWM, ADC, I2C)

This created unnecessary complexity and coupling between closely related systems.

## Changes Made

### 1. Unified API in HardwareManager
The HardwareManager now provides:
- Pin definitions (existing)
- Pin ownership tracking (from PinOwnershipManager)
- Shared resource coordination (from SharedResourceManager)

### 2. Key Features

#### Pin Ownership
```cpp
// Request ownership
bool requestPinOwnership(uint8_t pin, PinOwner owner, const char* ownerName);

// Transfer ownership with cleanup
bool transferPinOwnership(uint8_t pin, PinOwner fromOwner, PinOwner toOwner, 
                         const char* toOwnerName, void (*cleanupCallback)(uint8_t) = nullptr);

// Query ownership
PinOwner getPinOwner(uint8_t pin) const;
const char* getPinOwnerName(uint8_t pin) const;
```

#### PWM Management
```cpp
// Request PWM frequency for a timer group
bool requestPWMFrequency(uint8_t pin, uint32_t frequency, const char* owner);

// Request global PWM resolution
bool requestPWMResolution(uint8_t resolution, const char* owner);
```

#### ADC Management
```cpp
// Request ADC configuration
bool requestADCConfig(ADCModule module, uint8_t resolution, uint8_t averaging, const char* owner);
```

#### I2C Management
```cpp
// Request I2C bus speed
bool requestI2CSpeed(I2CBus bus, uint32_t speed, const char* owner);
```

### 3. Migration Summary

All modules now use HardwareManager instead of the separate managers:

| Module | Old Usage | New Usage |
|--------|-----------|-----------|
| ADProcessor | PinOwnershipManager + SharedResourceManager | HardwareManager |
| PWMMotorDriver | SharedResourceManager | HardwareManager |
| PWMProcessor | SharedResourceManager | HardwareManager |
| EncoderProcessor | PinOwnershipManager | HardwareManager |
| I2CManager | SharedResourceManager | HardwareManager |
| MachineProcessor | SharedResourceManager | HardwareManager |

### 4. Benefits

1. **Single Point of Truth**: All hardware management in one place
2. **Reduced Complexity**: No need to coordinate between three managers
3. **Better Cohesion**: Related functionality grouped together
4. **Easier Maintenance**: One class to update instead of three
5. **Clear Ownership**: Hardware management is clearly centralized

### 5. Status Methods

```cpp
// Print complete hardware status
void printHardwareStatus();

// Print pin ownership details
void printPinOwnership();

// Print resource allocation
void printResourceStatus();
```

## Testing
After unification, verify:
- [ ] Pin ownership tracking works correctly
- [ ] PWM frequency conflicts are detected
- [ ] PWM resolution conflicts are handled
- [ ] ADC configurations are tracked
- [ ] I2C speed management works
- [ ] All modules compile and function correctly

## Future Enhancements
- Add priority levels for resource requests
- Implement resource release callbacks
- Add runtime resource negotiation
- Support dynamic resource sharing