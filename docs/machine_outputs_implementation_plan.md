# Machine Outputs Implementation Plan

## Overview
Implement multi-function machine outputs that can control sections, hydraulics, tramlines, and geo-stop functions based on configuration from AgOpenGPS.

## Key Concepts
- The 6 section outputs (pins 0, 1, 4, 5, 10, 9 on PCA9685) are multi-function
- Functions are configured via PGN 236 (Pin Config) and PGN 238 (Machine Config)
- Control data comes from PGN 239 (Machine Data)
- Each output can be assigned to any of 21 functions:
  - Functions 1-16: Section control (spray sections)
  - Function 17: Hydraulic Up
  - Function 18: Hydraulic Down
  - Function 19: Tramline Right
  - Function 20: Tramline Left
  - Function 21: Geo Stop

## Implementation Phases

### Phase 1: Data Structures and PGN Handlers
**Goal**: Set up the foundation for storing configuration and state data

1. **Update MachineProcessor.h**
   - Add machine state structure for functions 1-21
   - Add configuration storage for pin assignments
   - Add PGN 236 and 238 handler declarations

2. **Update MachineProcessor.cpp**
   - Register PGN 236 (0xEC) and PGN 238 (0xEE) handlers
   - Implement basic PGN parsing to store configuration
   - Add debug logging for received configurations

3. **Update ConfigManager**
   - Add methods to save/load machine pin configuration to EEPROM
   - Add methods to save/load machine settings (raise/lower times, etc.)

**Testing**: Compile, flash, verify PGN reception and logging
**Commit**: "feat: add machine config PGN handlers and data structures"

---

### Phase 2: State Management
**Goal**: Parse control data and maintain function states

1. **Update PGN 239 handler**
   - Parse byte 7: hydLift (0=off, 1=down, 2=up)
   - Parse byte 8: tramline (bit0=right, bit1=left)
   - Parse byte 9: geoStop (0=inside, 1=outside)
   - Parse byte 11: sections 1-8 (existing)
   - Parse byte 12: sections 9-16 (existing)

2. **Add state mapping logic**
   - Create function array [1-21] to track states
   - Map section data to functions 1-16
   - Map hydraulic states to functions 17-18
   - Map tramline bits to functions 19-20
   - Map geo stop to function 21

3. **Add state change detection**
   - Track previous states to detect changes
   - Log state changes for debugging

**Testing**: Compile, flash, verify state parsing with debug logs
**Commit**: "feat: implement machine state management from PGN 239"

---

### Phase 3: Output Control
**Goal**: Control physical outputs based on configuration and state

1. **Create updateMachineOutputs() method**
   - Loop through 6 physical outputs
   - Look up assigned function for each output
   - Get current state of that function
   - Apply active high/low configuration
   - Set PCA9685 output accordingly

2. **Integrate with existing section control**
   - Modify updateSectionOutputs() to call updateMachineOutputs()
   - Ensure backward compatibility with section-only operation

3. **Add hydraulic timing logic**
   - Implement raise/lower time limits
   - Add automatic shutoff after configured time

**Testing**: Compile, flash, test with multimeter/LEDs on outputs
**Commit**: "feat: implement machine output control logic"

---

### Phase 4: Configuration Persistence
**Goal**: Save and restore configuration across reboots

1. **Implement EEPROM storage**
   - Save pin function assignments (24 bytes)
   - Save machine config (raise/lower times, enables)
   - Add version/magic number for validation

2. **Add load/save triggers**
   - Save when receiving PGN 236 or 238
   - Load during initialization
   - Add defaults for first-time setup

3. **Add configuration validation**
   - Verify function assignments are valid (0-21)
   - Ensure critical functions have assignments
   - Log configuration on startup

**Testing**: Compile, flash, verify config persists across reboot
**Commit**: "feat: add machine configuration persistence"

---

### Phase 5: Safety and Diagnostics
**Goal**: Add safety features and diagnostic capabilities

1. **Add safety features**
   - Ethernet link loss turns off all outputs
   - PGN timeout (2s) turns off all outputs
   - Boundary/geo-stop override for sections

2. **Add diagnostic output**
   - Web interface status display
   - Serial console status command
   - LED indication for active functions

3. **Add error handling**
   - Invalid configuration detection
   - PCA9685 communication errors
   - State conflict resolution

**Testing**: Full system test with all safety scenarios
**Commit**: "feat: add machine output safety and diagnostics"

---

## Technical Details

### PGN Formats

**PGN 236 (0xEC) - Machine Pin Config**
- Length: 30 bytes
- Bytes 5-28: Pin function assignments (24 pins)
- Each byte: 0=unassigned, 1-21=function number

**PGN 238 (0xEE) - Machine Config**
- Length: 14 bytes
- Byte 5: raiseTime
- Byte 6: lowerTime
- Byte 7: hydEnable
- Byte 8: set0 (isPinActiveHigh)
- Bytes 9-12: User1-User4

**PGN 239 (0xEF) - Machine Data**
- Length: varies
- Byte 7: hydLift (0=off, 1=down, 2=up)
- Byte 8: tramline (bit0=right, bit1=left)
- Byte 9: geoStop (0=inside, 1=outside)
- Byte 11: sections 1-8
- Byte 12: sections 9-16

### Hardware Mapping

**PCA9685 Output Pins**
- Output 1: Pin 0
- Output 2: Pin 1
- Output 3: Pin 4
- Output 4: Pin 5
- Output 5: Pin 10
- Output 6: Pin 9

**DRV8243 Control**
- Already implemented for section control
- Same drivers handle all machine functions
- Active high/low configurable per AOG settings

## Testing Strategy

1. **Unit Testing**
   - Test each PGN parser independently
   - Verify state mapping logic
   - Test EEPROM read/write

2. **Integration Testing**
   - Test with AOG simulator
   - Verify all 21 functions
   - Test configuration changes
   - Test safety features

3. **Hardware Testing**
   - LED board for visual verification
   - Multimeter for voltage checks
   - Oscilloscope for timing verification
   - Real hydraulic valve testing (with caution)

## Notes
- Maintain backward compatibility with existing section control
- Use existing PCA9685 and DRV8243 initialization
- Follow NG-V6 architecture patterns
- Extensive debug logging during development
- Remove verbose logging before final release