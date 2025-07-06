# Sensor Fusion Implementation Plan

## Overview
This plan breaks down the sensor fusion implementation into small, testable increments with frequent compile/test/commit points.

## Phase 1: Foundation (Week 1-2)

### Step 1.1: Basic Infrastructure
**Goal**: Create the foundation classes and integrate with existing system

1. Create `WheelAngleFusion` class skeleton
   - Basic class structure with empty methods
   - Configuration struct
   - Integration with ConfigManager
   
2. Add fusion enable flag to existing config
   - Use existing `insUseFusion` flag in ConfigManager
   - Add web UI toggle in Device Settings page
   
**Test**: Compile and verify no existing functionality broken
**Commit**: "feat: add WheelAngleFusion class skeleton"

### Step 1.2: Motor Position Tracking
**Goal**: Extract and track motor position from Keya CAN messages

1. Update `KeyaCANDriver` to expose motor position
   - Add `getMotorPosition()` method
   - Add `getPositionDelta()` for changes since last call
   - Handle position rollover properly
   
2. Add position tracking variables
   - Current position
   - Previous position
   - Delta accumulator
   
**Test**: Log motor position values, verify they change with steering
**Commit**: "feat: add motor position tracking to KeyaCANDriver"

### Step 1.3: Basic Kalman Filter
**Goal**: Implement core Kalman filter algorithm

1. Implement basic Kalman filter in `WheelAngleFusion`
   - State variables (X, P)
   - Fixed parameters (Q, R)
   - Prediction step using motor encoder
   - Update step (placeholder for GPS angle)
   
2. Add update() method
   - Take encoder delta as input
   - Run prediction step
   - Return current estimate
   
**Test**: Feed known encoder values, verify output changes
**Commit**: "feat: implement basic Kalman filter algorithm"

### Step 1.4: GPS Angle Calculation
**Goal**: Calculate steering angle from GPS/INS data

1. Add GPS angle calculation
   - Get heading rate from NAVProcessor
   - Get vehicle speed from NAVProcessor
   - Calculate wheel angle using Ackermann geometry
   - Add wheelbase to configuration
   
2. Add sanity checks
   - Minimum speed threshold
   - Maximum angle limits
   - Invalid data detection
   
**Test**: Log calculated GPS angles at various speeds
**Commit**: "feat: add GPS-derived wheel angle calculation"

### Step 1.5: Connect to AutosteerProcessor
**Goal**: Use fusion output for steering control

1. Modify `AutosteerProcessor::updateMotorControl()`
   - Check if fusion is enabled
   - Get angle from fusion instead of ADProcessor
   - Maintain WAS fallback option
   
2. Add fusion status to PGN 253
   - Include fusion health status
   - Send fusion angle vs WAS angle
   
**Test**: Enable fusion, verify steering still responds
**Commit**: "feat: connect sensor fusion to autosteer control"

## Phase 2: Calibration & Tuning (Week 3-4)

### Step 2.1: Orbital Valve Compensation
**Goal**: Handle non-linear relationship between motor and wheel angle

**CRITICAL**: Orbital valves are the primary steering system in most tractors and have significant non-linearity that MUST be addressed for acceptable accuracy.

1. Multi-point calibration system
   - Collect motor position vs actual angle at multiple points (-40° to +40° in 4° steps)
   - Build interpolation tables for different load conditions (low/medium/high)
   - Track steering rate effects on the ratio
   - Implement smooth curve fitting between points
   - Save calibration curves to EEPROM
   
2. Load detection system
   - Monitor motor current for load estimation
   - Track position change rate vs wheel angle change rate
   - Use vehicle speed as additional load indicator
   - Build load-dependent compensation curves
   
3. Real-time learning and adaptation
   - Continuously update calibration during operation with GPS available
   - Use exponential smoothing for gradual adaptation
   - Detect and compensate for temperature effects
   - Track long-term valve wear patterns
   - Only learn from high-confidence GPS data (straight sections, good fix)
   
4. Web interface for calibration
   - Add calibration wizard page
   - Real-time angle display with motor position
   - Visual curve editor with interpolation preview
   - Load condition indicators
   - Import/export calibration data
   - Compare current vs learned curves
   
**Test**: 
- Collect data under various loads (empty field, heavy implement, slopes)
- Compare linear vs compensated accuracy
- Verify learning improves accuracy over time
- Test with different hydraulic temperatures

**Expected Results**:
- Improve accuracy from ±2° (linear) to ±0.5° (compensated)
- Consistent performance across all load conditions
- Reduced need for manual recalibration

**Commit**: "feat: add orbital valve compensation with multi-point calibration"

### Step 2.2: Wheelbase Calibration
**Goal**: Determine vehicle wheelbase for GPS angle calculation

1. Add auto-calibration during straight driving
   - Detect straight sections (low heading rate)
   - Compare encoder angle to GPS angle
   - Adjust wheelbase to minimize error
   
2. Manual wheelbase entry
   - Add to vehicle settings
   - Persist to EEPROM
   
**Test**: Drive straight sections, verify wheelbase converges
**Commit**: "feat: add wheelbase calibration"

### Step 2.3: Fixed Parameter Tuning
**Goal**: Tune Q and R parameters for optimal performance

1. Add parameter adjustment
   - Web interface sliders for Q and R
   - Real-time parameter updates
   - Visual feedback of filter behavior
   
2. Add performance metrics
   - Innovation (measurement residual)
   - Uncertainty (P value)
   - Lag between encoder and output
   
**Test**: Adjust parameters while steering, find optimal values
**Commit**: "feat: add Kalman filter parameter tuning"

## Phase 3: Adaptive Algorithm (Week 5-6)

### Step 3.1: Variance Calculation
**Goal**: Calculate GPS angle variance for adaptive gain

1. Implement circular buffer for variance
   - Store recent GPS angles during straight driving
   - Calculate running mean and variance
   - Update only during good conditions
   
2. Add variance monitoring
   - Display current variance
   - Show buffer status
   - Log variance changes
   
**Test**: Drive various patterns, verify variance calculation
**Commit**: "feat: add adaptive variance calculation"

### Step 3.2: Adaptive Kalman Gain
**Goal**: Make filter adapt to GPS quality

1. Implement adaptive R calculation
   - Scale R based on variance
   - Add min/max limits
   - Smooth transitions
   
2. Add quality indicators
   - GPS fix quality
   - Speed adequacy
   - Heading rate validity
   
**Test**: Simulate GPS degradation, verify adaptation
**Commit**: "feat: implement adaptive Kalman gain"

### Step 3.3: Drift Compensation
**Goal**: Compensate for encoder drift

1. Add bias estimation
   - Detect and measure systematic drift
   - Apply correction to encoder readings
   - Reset bias on significant steering
   
2. Add drift monitoring
   - Track accumulated drift
   - Alert when recalibration needed
   - Auto-zero during straight driving
   
**Test**: Extended operation, verify drift compensation
**Commit**: "feat: add encoder drift compensation"

## Phase 4: Enhanced Features (Week 7-8)

### Step 4.1: Multi-Sensor Support
**Goal**: Support different sensor combinations

1. Add IMU heading rate option
   - Get rotation rate from IMU
   - Switch between GPS and IMU sources
   - Automatic source selection
   
2. Add sensor priority system
   - Define sensor hierarchy
   - Automatic failover
   - Health monitoring
   
**Test**: Disable various sensors, verify failover
**Commit**: "feat: add multi-sensor support"

### Step 4.2: Advanced Diagnostics
**Goal**: Comprehensive diagnostics and monitoring

1. Add diagnostic web page
   - Real-time sensor values
   - Fusion algorithm state
   - Performance metrics
   - Graphical displays
   
2. Add data logging
   - Record all sensor inputs
   - Log fusion outputs
   - Export for analysis
   
**Test**: Monitor during various conditions
**Commit**: "feat: add fusion diagnostics interface"

### Step 4.3: Vehicle Dynamics Model
**Goal**: Improve prediction with vehicle model

1. Add bicycle model
   - Include steering rate limits
   - Model steering system inertia
   - Account for tire slip
   
2. Add Ackermann compensation
   - Inside/outside wheel angles
   - Speed-dependent effects
   - Calibratable parameters
   
**Test**: High-speed steering, verify improved tracking
**Commit**: "feat: add vehicle dynamics model"

## Phase 5: Testing & Optimization (Week 9-10)

### Step 5.1: Test Suite
**Goal**: Comprehensive testing infrastructure

1. Unit tests
   - Test each component in isolation
   - Edge cases and error conditions
   - Performance benchmarks
   
2. Integration tests
   - Full system tests
   - Recorded data playback
   - Regression testing
   
**Test**: Run full test suite
**Commit**: "test: add sensor fusion test suite"

### Step 5.2: Performance Optimization
**Goal**: Optimize for Teensy 4.1

1. Profile code performance
   - Identify bottlenecks
   - Optimize math operations
   - Reduce memory allocations
   
2. Add performance monitoring
   - CPU usage tracking
   - Update rate monitoring
   - Latency measurement
   
**Test**: Verify <5% CPU usage
**Commit**: "perf: optimize sensor fusion performance"

### Step 5.3: Documentation
**Goal**: Complete user and developer documentation

1. User documentation
   - Setup guide
   - Calibration procedures
   - Troubleshooting guide
   
2. Developer documentation
   - API reference
   - Algorithm details
   - Extension guide
   
**Test**: Follow documentation as new user
**Commit**: "docs: add sensor fusion documentation"

## Testing Strategy

### After Each Step:
1. **Compile**: Ensure no build errors
2. **Basic Function**: Verify existing features still work
3. **New Feature**: Test the specific new functionality
4. **Logging**: Add temporary debug logs to verify behavior
5. **Commit**: Make atomic commit with clear message

### Integration Testing Points:
- End of Phase 1: Basic fusion working
- End of Phase 2: Calibrated and tuned
- End of Phase 3: Adaptive algorithm active
- End of Phase 4: All features integrated
- End of Phase 5: Production ready

### Field Testing Milestones:
1. **Stationary**: Verify angle tracking with manual steering
2. **Low Speed**: Test in yard at <5 mph
3. **Field Speed**: Normal operation at 5-10 mph
4. **Edge Cases**: Test GPS loss, tight turns, rough terrain

## Risk Mitigation

### Technical Risks:
1. **Performance**: Start with 10Hz update rate, optimize later
2. **Accuracy**: Always maintain WAS fallback option
3. **Stability**: Extensive testing before removing WAS support

### Schedule Risks:
1. **Delays**: Each phase is independently useful
2. **Complexity**: Can stop at Phase 2 for basic functionality
3. **Testing**: Allocate 20% extra time for unforeseen issues

## Success Metrics

### Phase 1 Complete:
- Fusion angle within 2° of WAS
- No impact on existing functionality
- Basic operation at >1 m/s

### Phase 2 Complete:
- Orbital valve compensation working with 3-level load detection
- Multi-point calibration implemented (21+ points per load level)
- Real-time learning adapting to changing conditions
- Angle accuracy ±0.5° RMS across all conditions (improved from ±2°)
- Stable operation under varying loads and temperatures
- Web-based calibration wizard functional

### Phase 3 Complete:
- Adapts to GPS quality changes
- Maintains accuracy during GPS degradation
- No manual tuning required

### Phase 4 Complete:
- Supports 3+ sensor configurations
- Comprehensive diagnostics available
- Field-tested in various conditions

### Phase 5 Complete:
- Meets all performance targets
- Complete documentation
- Ready for production use