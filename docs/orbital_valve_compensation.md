# Orbital Valve Compensation for Virtual WAS

## Problem Statement

Orbital (orbitrol) steering valves do not provide a constant relationship between steering motor rotation and wheel angle. The ratio varies based on:

1. **Hydraulic load** - Higher loads require more motor turns
2. **Flow rate** - Faster steering changes the ratio
3. **Pressure differential** - System pressure affects response
4. **Temperature** - Oil viscosity changes with temperature
5. **Valve wear** - Older valves have more internal leakage

This non-linear behavior means our simple `countsPerDegree` approach will have significant errors.

## Current Limitations

With fixed calibration:
- Accurate at calibration conditions only
- Errors increase with load changes
- Poor performance during heavy steering
- Inconsistent between different conditions

## Proposed Solution for Phase 2

### 1. Multi-Point Calibration

Instead of a single counts/degree value, build a calibration table:

```
Motor Position | Actual Wheel Angle | Conditions
---------------|-------------------|------------
0              | 0°                | Stationary
1000           | 8.5°              | Low speed
2000           | 16.2°             | Low speed
1000           | 9.8°              | High load
2000           | 18.5°             | High load
```

### 2. Real-Time Learning

During operation with GPS available:
- Continuously map motor position to GPS-derived angle
- Build a correction curve over time
- Store in EEPROM for persistence

### 3. Load Detection

Detect loading conditions using:
- Rate of motor position change vs wheel angle change
- Motor current (if available)
- Hydraulic pressure (if sensor installed)
- Vehicle speed and turning rate

### 4. Adaptive Prediction Model

Replace linear prediction with:
```cpp
// Current (Phase 1)
predictedAngle = encoderAngle;

// Proposed (Phase 2)
predictedAngle = lookupTable.interpolate(motorPosition, currentLoad);
```

### 5. Hysteresis Compensation

Account for valve deadband:
- Track direction of movement
- Apply different curves for left/right
- Compensate for backlash

## Implementation Approach

### Phase 2.1: Data Collection
- Log motor position vs GPS angle during normal operation
- Identify patterns and build initial curves
- Determine load indicators

### Phase 2.2: Adaptive Algorithm
- Implement lookup table with interpolation
- Add load estimation
- Real-time curve updates

### Phase 2.3: Advanced Features
- Temperature compensation
- Wear detection and alerting
- Auto-calibration routines

## Expected Improvements

- Accuracy: From ±2° to ±0.5° RMS
- Consistency across conditions
- Better low-speed performance
- Reduced calibration requirements

## Testing Approach

1. Collect data with various loads
2. Compare fixed vs adaptive approaches
3. Validate across different tractors
4. Long-term stability testing