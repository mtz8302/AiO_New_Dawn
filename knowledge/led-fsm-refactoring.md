# LED Manager FSM Refactoring

## Overview

This document describes the refactoring of the LED Manager from traditional if-else chains to a Finite State Machine (FSM) approach.

## Benefits of FSM Approach

### 1. **Clearer State Management**
- Each LED has well-defined states with meaningful names
- State transitions are explicit and trackable
- Easy to add logging for state changes

### 2. **Table-Driven Configuration**
- LED behavior defined in static lookup tables
- Easy to modify LED colors/modes without changing logic
- No nested if-else chains

### 3. **Separation of Concerns**
- State determination logic separated from LED control
- FSM state management separated from hardware control
- Easier to test and maintain

### 4. **Extensibility**
- Adding new states is straightforward
- Easy to add state entry/exit actions if needed
- Can add state history tracking

## Implementation Details

### State Definitions

Each LED subsystem has its own state enum:

```cpp
// Power/Ethernet LED states
enum PowerState {
    PWR_NO_ETHERNET,      // No ethernet connection
    PWR_NO_AGIO,         // Ethernet OK, no AgIO
    PWR_CONNECTED        // Fully connected
};

// GPS LED states
enum GPSState {
    GPS_NO_DATA,         // No GPS data
    GPS_NO_FIX,          // GPS data but no fix
    GPS_BASIC_FIX,       // GPS or DGPS fix
    GPS_RTK_FLOAT,       // RTK float
    GPS_RTK_FIXED        // RTK fixed
};
```

### State-to-LED Mapping

Each state maps to a specific LED color and mode through lookup tables:

```cpp
const PowerStateMap powerStateMap[] = {
    {PWR_NO_ETHERNET,  RED,    SOLID},
    {PWR_NO_AGIO,      YELLOW, BLINKING},
    {PWR_CONNECTED,    GREEN,  SOLID}
};
```

### Before (If-Else Chains)

```cpp
void LEDManager::setPowerState(bool hasEthernet, bool hasAgIO) {
    if (!hasEthernet) {
        setLED(PWR_ETH, RED, SOLID);
    } else if (!hasAgIO) {
        setLED(PWR_ETH, YELLOW, BLINKING);
    } else {
        setLED(PWR_ETH, GREEN, SOLID);
    }
}
```

### After (FSM)

```cpp
// State determination
PowerState newPowerState;
if (!ethernetUp) {
    newPowerState = PWR_NO_ETHERNET;
} else if (!hasAgIO) {
    newPowerState = PWR_NO_AGIO;
} else {
    newPowerState = PWR_CONNECTED;
}
transitionPowerState(newPowerState);

// State transition with logging
void transitionPowerState(PowerState newState) {
    if (powerState != newState) {
        LOG_DEBUG("Power LED state transition: %d -> %d", powerState, newState);
        powerState = newState;
        updatePowerLED();
    }
}

// Table-driven LED update
void updatePowerLED() {
    for (int i = 0; i < sizeof(powerStateMap)/sizeof(powerStateMap[0]); i++) {
        if (powerStateMap[i].state == powerState) {
            setLED(PWR_ETH, powerStateMap[i].color, powerStateMap[i].mode);
            break;
        }
    }
}
```

## Performance Considerations

- FSM approach has minimal overhead
- State transitions only occur when state changes
- Table lookups are O(n) where n is small (3-5 states per LED)
- No performance degradation compared to if-else chains

## Testing

The FSM approach makes testing easier:
- Can test state transitions independently
- Can verify LED outputs for each state
- Can add state history for debugging

## Migration Plan

1. Create new `LEDManagerFSM` class alongside existing `LEDManager`
2. Test FSM implementation thoroughly
3. Switch main.cpp to use FSM version
4. Remove old implementation after verification

## Future Enhancements

- Add state entry/exit actions
- Add state transition conditions/guards
- Add state history for debugging
- Add configuration file support for LED mappings