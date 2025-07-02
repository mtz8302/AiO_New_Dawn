# WAS-less Sensor Fusion Proposal for AiO_New_Dawn

## Executive Summary

This document proposes integrating a sensor fusion algorithm for wheel angle estimation without a traditional Wheel Angle Sensor (WAS). The approach is based on the proven Kalman filter implementation from the AOG_Teensy_UM98X project, which combines motor encoder feedback with GPS/INS heading rate to estimate steering angle.

## Background

### Current State
- AiO_New_Dawn currently requires a physical WAS (potentiometer) for steering feedback
- The system has all necessary sensors available but doesn't utilize them for angle estimation:
  - Keya motor with position feedback via CAN
  - IMU providing heading and rotation rates
  - GPS/GNSS providing heading and speed
  - Motor RPM and current feedback

### Reference Implementation
The AOG_Teensy_UM98X project demonstrates successful field-tested sensor fusion using:
- Keya motor encoder for high-frequency angle changes
- GPS/INS heading rate for drift-free reference
- Adaptive Kalman filter with variance-based gain adjustment

## Technical Overview

### Core Algorithm

The fusion algorithm uses a Kalman filter that combines:

1. **Prediction Step**: Uses encoder changes for rapid response
   ```
   angle_predicted = previous_angle + encoder_change
   ```

2. **Correction Step**: Uses GPS-derived angle for drift correction
   ```
   angle_gps = atan(heading_rate * wheelbase / speed) * RAD_TO_DEG
   angle_fused = angle_predicted + K * (angle_gps - angle_predicted)
   ```

3. **Adaptive Gain**: Kalman gain K adapts based on GPS quality
   ```
   K = P / (P + R * variance)
   ```

### Key Innovation

The algorithm continuously calculates the variance of GPS-derived angles during good conditions (straight driving, good GPS fix). This variance is used to scale the measurement noise (R), making the filter automatically adapt to GPS quality.

## Integration Architecture

### Proposed Class Structure

```cpp
class WheelAngleFusion {
private:
    // Kalman state
    float fusedAngle;          // Current estimate
    float predictionError;     // Uncertainty
    
    // Sensor inputs
    float motorPosition;       // From KeyaCANDriver
    float vehicleSpeed;        // From GNSS/AgOpenGPS
    float headingRate;         // From IMU or GPS
    
    // Adaptive parameters
    float measurementVariance;
    CircularBuffer<float> varianceBuffer;
    
    // Configuration
    struct Config {
        float wheelbase;       // Vehicle wheelbase (m)
        float steeringRatio;   // Motor counts to wheel degrees
        float minSpeed;        // Minimum speed for fusion (m/s)
        float processNoise;    // Q parameter
        float measurementNoise;// R base parameter
    } config;
    
public:
    void update(float dt);
    float getFusedAngle();
    void calibrate();
    bool isHealthy();
};
```

### Integration Points

1. **AutosteerProcessor**
   - Add fusion as alternative to WAS reading
   - Switch based on `insUseFusion` config flag
   - Maintain compatibility with existing PID control

2. **KeyaCANDriver**
   - Extract and accumulate motor position
   - Calculate position changes between updates
   - Handle position rollover

3. **IMUProcessor/GNSSProcessor**
   - Provide heading rate (either from IMU or GPS)
   - Supply vehicle speed
   - Quality indicators for sensor health

## Implementation Plan

### Phase 1: Basic Fusion (2-3 weeks)
- [ ] Create `SensorFusion` class in `lib/aio_autosteer/`
- [ ] Implement basic Kalman filter with fixed parameters
- [ ] Add motor position tracking to `KeyaCANDriver`
- [ ] Create calibration routine for motor-to-wheel ratio
- [ ] Enable/disable via existing `insUseFusion` config flag
- [ ] Basic testing with logged data

### Phase 2: Enhanced Algorithm (2-3 weeks)
- [ ] Implement adaptive variance calculation
- [ ] Add vehicle dynamics model (bicycle model)
- [ ] Include Ackermann compensation
- [ ] Add sensor health monitoring and fallback logic
- [ ] Implement auto-calibration during straight driving
- [ ] Web interface for monitoring and tuning

### Phase 3: Multi-Sensor Support (3-4 weeks)
- [ ] Generalize for different sensor combinations
- [ ] Add support for steering rate from motor RPM
- [ ] Implement sensor fault detection and switching
- [ ] Create configuration wizard
- [ ] Comprehensive testing suite

## Sensor Configurations

### 1. Premium: Dual GPS + IMU + Encoder
- Best accuracy, full redundancy
- Heading from dual GPS
- Rotation rate from IMU
- Position from encoder

### 2. Standard: Single GPS/INS + Encoder
- Good accuracy, proven in field
- Heading rate from GPS
- Position from encoder
- This is the configuration used in AOG_Teensy_UM98X

### 3. Basic: IMU + Encoder
- Works at all speeds including standstill
- Rotation rate from IMU
- Position from encoder
- Subject to IMU drift over time

### 4. Fallback: Encoder Only
- Emergency mode when other sensors fail
- Dead reckoning with accumulated drift
- Requires periodic recalibration

## Pros and Cons

### Advantages
- ✅ **Cost Savings**: Eliminates $100+ WAS hardware
- ✅ **Reliability**: No potentiometer wear or mechanical linkage issues
- ✅ **Maintenance**: Immune to dirt/moisture on steering components
- ✅ **Installation**: No sensor mounting or alignment required
- ✅ **Proven**: Algorithm field-tested with good results
- ✅ **Adaptive**: Automatically adjusts to GPS quality

### Disadvantages
- ❌ **Minimum Speed**: Requires ~0.5 m/s for GPS-based correction
- ❌ **Calibration**: Initial setup of steering ratio and wheelbase
- ❌ **Complexity**: More difficult to troubleshoot
- ❌ **GPS Dependent**: Degraded performance in poor GPS conditions
- ❌ **Motor Required**: Only works with motors having position feedback

## Technical Improvements Over Reference

### Algorithm Enhancements
1. **Multi-rate Kalman Filter**
   - Properly handle different sensor update rates
   - Time-synchronized prediction steps

2. **Outlier Rejection**
   - Statistical detection of GPS heading jumps
   - Smooth handling of sensor dropouts

3. **Extended State Vector**
   - Include steering rate and sensor biases
   - Better modeling of system dynamics

4. **Adaptive Process Noise**
   - Increase Q during active steering
   - Decrease Q during straight driving

5. **Latency Compensation**
   - Account for GPS/INS processing delays
   - Timestamp-based synchronization

### Implementation Quality
1. **Modern C++ Design**
   - Template-based for different numeric types
   - RAII for resource management
   - Clear separation of concerns

2. **Testing Infrastructure**
   - Unit tests for algorithm components
   - Integration tests with recorded data
   - Hardware-in-loop simulation

3. **Diagnostics**
   - Real-time web interface for tuning
   - Data logging for offline analysis
   - Performance metrics tracking

## Limitations and Mitigations

| Limitation | Impact | Mitigation Strategy |
|------------|--------|-------------------|
| Minimum speed requirement | No angle estimate when stopped | Use last known angle, detect stationary state |
| GPS outages (tunnels, trees) | Temporary degraded accuracy | Fall back to IMU+encoder, limit integration time |
| Calibration drift | Growing angle offset | Auto-calibration during straight sections |
| Initial setup complexity | User frustration | Guided calibration wizard with visual feedback |
| Encoder resolution | Angle quantization noise | Interpolation, recommend high-resolution encoders |
| Steering play/backlash | Hysteresis in measurements | Deadband compensation, direction detection |

## Success Criteria

### Performance Metrics
- **Accuracy**: ±0.5° RMS (compared to ±0.25° for good WAS)
- **Response Time**: <50ms to steering input
- **Minimum Speed**: 0.3 m/s for full accuracy
- **GPS Outage Tolerance**: 30 seconds before significant drift
- **CPU Usage**: <5% on Teensy 4.1

### Functional Requirements
- Seamless fallback to physical WAS if available
- No changes required to AgOpenGPS
- Compatible with existing autosteer tuning
- Works with all supported motor controllers

## Development Timeline

| Week | Tasks |
|------|-------|
| 1-2 | Basic Kalman implementation, motor position tracking |
| 3-4 | Calibration routines, configuration UI |
| 5-6 | Adaptive variance, vehicle dynamics model |
| 7-8 | Multi-sensor support framework |
| 9-10 | Testing suite, performance optimization |
| 11-12 | Field testing, documentation |

Total estimated time: 12 weeks for full implementation

## Recommendations

1. **Start with Phase 1** implementing basic Keya motor + GPS fusion
2. **Maintain WAS compatibility** throughout development
3. **Focus on single GPS + encoder** configuration initially (most common)
4. **Develop comprehensive diagnostics** early for easier debugging
5. **Create simulator** using recorded field data for testing

## Conclusion

The proposed sensor fusion system offers a cost-effective and reliable alternative to traditional wheel angle sensors. By leveraging existing sensors already present in the AiO system, we can provide accurate steering feedback without additional hardware. The adaptive Kalman filter approach from the AOG_Teensy_UM98X project provides a solid foundation, with clear paths for enhancement and generalization.

The phased implementation approach allows for incremental development and testing while maintaining compatibility with existing systems. This ensures that users can benefit from the technology as it develops, without requiring a complete system overhaul.

## References

- AOG_Teensy_UM98X source code: [GitHub Repository]
- Kalman Filter theory: "Optimal State Estimation" by Dan Simon
- Vehicle dynamics: "Vehicle Dynamics and Control" by Rajamani
- AgOpenGPS documentation: [Official Wiki]

## Appendix: Key Code Snippets from Reference

### Variance Calculation (zKalmanKeya.ino)
```cpp
// Calculate running variance for adaptive Kalman gain
if (speed > (settings.minSpeedKalman * 0.7) && 
    abs(insWheelAngle) < 30 && 
    strstr(insStatus, "INS_SOLUTION") != NULL) {
    
    varianceBuffer[indexVarBuffer++] = insWheelAngle;
    indexVarBuffer = indexVarBuffer % lenVarianceBuffer;
    
    // Compute mean
    varianceMean = 0;
    for (uint16_t i = 0; i < lenVarianceBuffer; i++)
        varianceMean += varianceBuffer[i];
    varianceMean /= lenVarianceBuffer;
    
    // Compute variance
    angleVariance = 0;
    for (int i = 0; i < lenVarianceBuffer; i++) {
        angleVariance += (varianceBuffer[i] - varianceMean) * 
                        (varianceBuffer[i] - varianceMean);
    }
    angleVariance /= (lenVarianceBuffer - 1);
}
```

### Kalman Update (zKalmanKeya.ino)
```cpp
void KalmanUpdate() {
    // Encoder change since last update
    float angleDiff = (keyaEncoder / steerSettings.keyaSteerSensorCounts) - 
                      steerAngleActualOld;
    
    X = KalmanWheelAngle;
    
    // Prediction step
    Pp = P + settings.kalmanQ;  // Prediction uncertainty
    Xp = X + angleDiff;         // Predicted angle
    
    // Correction step with adaptive gain
    K = Pp / (Pp + (settings.kalmanR * angleVariance));
    P = (1 - K) * Pp;           // Updated uncertainty
    X = Xp + K * (insWheelAngle - Xp);  // Fused estimate
    
    KalmanWheelAngle = X;
    steerAngleActualOld = keyaEncoder / steerSettings.keyaSteerSensorCounts;
}
```

### GPS Wheel Angle Derivation
```cpp
// Calculate wheel angle from vehicle dynamics
heading_rate = (heading - headingOld) / settings.intervalINS;

// Handle wrap-around
if (heading_rate > 300) heading_rate -= 360;
if (heading_rate < -300) heading_rate += 360;

// Ackermann steering geometry
insWheelAngle = atan(heading_rate / RAD_TO_DEG * 
                     calibrationData.wheelBase / speed) * 
                RAD_TO_DEG * workingDir;

// Sanity check
if (!(insWheelAngle < 50 && insWheelAngle > -50))
    insWheelAngle = 0;
```