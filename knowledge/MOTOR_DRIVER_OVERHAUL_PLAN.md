# Motor Driver Overhaul Implementation Plan

## Overview
This plan outlines the phased implementation to support three motor driver types with automatic detection and configuration based on the V6 implementation.

## Current State Analysis

### V6 Implementation
- **Keya CAN**: Detected by heartbeat (0x7000001), controlled via CAN commands
- **Danfoss**: Configured via PGN251 Byte 8, uses Output 5 (enable) and Output 6 (analog PWM)
- **DRV8701P**: Default driver, uses PWM1/PWM2 pins with complementary control

### New Dawn Current State
- Already has `MotorDriverInterface` abstraction
- Has `KeyaCANDriver` implementation (partially complete)
- Has `PWMMotorDriver` for standard PWM control
- Has `MotorDriverFactory` for driver instantiation
- Missing: Danfoss driver, proper detection logic, kickout handling

## Detection Priority Order
1. **Keya CAN** - Heartbeat detection on CAN3
2. **Danfoss** - PGN251 configuration
3. **DRV8701P** - Default fallback

## Configuration Mapping (PGN251 Byte 8)
- 0x00: DRV8701P + Wheel Encoder
- 0x01: Danfoss + Wheel Encoder
- 0x02: DRV8701P + Pressure Sensor
- 0x03: Danfoss + Pressure Sensor
- 0x04: DRV8701P + Current Sensor

## Implementation Phases

### Phase 1: Foundation & Detection Logic (2-3 days)
**Objective**: Establish detection framework and update interfaces

**Tasks**:
1. Update `MotorDriverInterface` to include:
   ```cpp
   virtual bool isDetected() = 0;
   virtual MotorDriverType getType() = 0;
   virtual void handleKickout(KickoutType type, float value) = 0;
   virtual float getCurrentDraw() = 0;
   ```

2. Create `MotorDriverDetector` class:
   - Monitor CAN3 for Keya heartbeat
   - Parse PGN251 Byte 8 for driver configuration
   - Implement detection state machine
   - Add detection logging

3. Update `SteerConfig` structure to include:
   - `IsDanfoss` flag
   - `KickoutType` enum (Encoder, Pressure, Current)
   - Motor driver configuration from PGN251

**Test Points**:
- Simulate Keya heartbeat, verify detection
- Send PGN251 with various Byte 8 values
- Verify detection priority order

**Deliverables**:
- Updated interfaces
- Detection logic implementation
- Unit tests for detection

---

### Phase 2: Keya CAN Driver Enhancement (1-2 days)
**Objective**: Complete Keya implementation based on V6

**Tasks**:
1. Update `KeyaCANDriver`:
   - Port heartbeat monitoring from V6
   - Implement proper speed mapping (-255 to 255 → -995 to 995)
   - Add current sensor reading support
   - Implement enable/disable commands
   - Add version query support

2. CAN message handling:
   - Heartbeat response (0x7000001)
   - Speed/direction commands (0x6000001)
   - Current query/response
   - Encoder/speed query

**Test Points**:
- Test with actual Keya motor
- Verify speed control accuracy
- Test current sensor feedback
- Verify kickout functionality

**Deliverables**:
- Complete Keya driver
- Integration tests

---

### Phase 3: Danfoss Valve Driver (2-3 days)
**Objective**: Implement Danfoss valve control

**Tasks**:
1. Create `DanfossMotorDriver` class:
   ```cpp
   class DanfossMotorDriver : public MotorDriverInterface {
       // Output 5: Enable pin (HIGH = enabled)
       // Output 6: Analog control (25% = left, 50% = center, 75% = right)
   };
   ```

2. Implement control logic:
   - Map -100% to +100% speed → 25% to 75% PWM
   - Control Output 5 for enable/disable
   - Implement 12V analog output on Output 6
   - Add center position calibration

3. Configure outputs:
   - Use machine output pins 5 & 6
   - Ensure proper PWM frequency for analog output
   - Add safety limits

**Test Points**:
- Measure Output 6 voltage (should be 3-9V range)
- Test enable/disable on Output 5
- Verify smooth control response
- Test with actual Danfoss valve

**Deliverables**:
- Danfoss driver implementation
- Voltage measurement logs
- Integration tests

---

### Phase 4: DRV8701P Driver Update (1-2 days)
**Objective**: Update PWM driver to match V6 behavior

**Tasks**:
1. Update `PWMMotorDriver` to `DRV8701PMotorDriver`:
   - Ensure complementary PWM operation
   - Inactive pin must be LOW (not HIGH)
   - Add current sensor support via CURRENT_PIN
   - Implement sleep mode control

2. PWM control logic:
   ```cpp
   if (speed > 0) {
       analogWrite(PWM2_PIN, 255);      // Turn off first
       analogWrite(PWM1_PIN, 255 - pwm); // Then activate
   } else {
       analogWrite(PWM1_PIN, 255);      // Turn off first
       analogWrite(PWM2_PIN, 255 - pwm); // Then activate
   }
   ```

3. Add current monitoring:
   - Read from CURRENT_PIN (A13)
   - Implement filtering
   - Support kickout threshold

**Test Points**:
- Verify PWM signals on scope
- Test current sensor readings
- Verify motor direction control
- Test kickout functionality

**Deliverables**:
- Updated DRV8701P driver
- Current sensor calibration data

---

### Phase 5: Kickout System Integration (2 days)
**Objective**: Unified kickout handling across all drivers

**Tasks**:
1. Create `KickoutMonitor` class:
   - Support multiple input types
   - Configurable thresholds
   - Debouncing logic
   - Event generation

2. Input handling:
   - Wheel encoder (KICKOUT_D_PIN)
   - Pressure sensor (KICKOUT_A_PIN)
   - Current sensor (driver-specific)
   - Implement quadrature encoder support

3. Integration:
   - Each driver registers with KickoutMonitor
   - Monitor calls driver's handleKickout()
   - Add kickout event logging

**Test Points**:
- Test each kickout type
- Verify threshold settings
- Test debouncing
- Verify all drivers respond correctly

**Deliverables**:
- Kickout monitoring system
- Integration with all drivers

---

### Phase 6: Factory & Auto-Selection (1 day)
**Objective**: Automatic driver selection and instantiation

**Tasks**:
1. Update `MotorDriverFactory`:
   - Integrate with MotorDriverDetector
   - Implement detection → instantiation flow
   - Add fallback logic
   - Support runtime driver switching

2. AutosteerProcessor integration:
   - Remove hardcoded driver selection
   - Use factory for driver creation
   - Handle driver switching events
   - Update status reporting

**Test Points**:
- Test automatic detection
- Test driver switching
- Verify fallback to DRV8701P
- Test PGN251 configuration changes

**Deliverables**:
- Updated factory implementation
- Auto-selection logic

---

### Phase 7: Testing & Optimization (2-3 days)
**Objective**: Full system validation

**Tasks**:
1. Integration testing:
   - Test all driver combinations
   - Verify kickout configurations
   - Test driver switching scenarios
   - Performance profiling

2. Edge cases:
   - CAN bus disconnection/reconnection
   - Invalid PGN251 values
   - Kickout during active steering
   - Power cycling behavior

3. Documentation:
   - Update user manual
   - Create troubleshooting guide
   - Document configuration options

**Test Points**:
- Field testing with each driver type
- Stress testing
- Configuration validation
- Performance benchmarks

**Deliverables**:
- Test results
- Bug fixes
- Documentation

---

## Risk Mitigation

1. **Hardware Availability**: Ensure access to all three motor types for testing
2. **Backward Compatibility**: Maintain existing functionality during transition
3. **Safety**: Implement failsafe modes for each driver type
4. **Testing Coverage**: Create simulators for motors not immediately available

## Success Criteria

1. Automatic detection works reliably for all three motor types
2. Each driver provides equivalent steering performance
3. Kickout functionality works correctly for all configurations
4. No regression in existing functionality
5. Clean, maintainable code structure

## Timeline Estimate

Total: 12-16 days of development

- Phase 1: 2-3 days
- Phase 2: 1-2 days  
- Phase 3: 2-3 days
- Phase 4: 1-2 days
- Phase 5: 2 days
- Phase 6: 1 day
- Phase 7: 2-3 days

## Next Steps

1. Review and approve this plan
2. Set up test hardware for all three motor types
3. Begin Phase 1 implementation
4. Schedule regular testing checkpoints