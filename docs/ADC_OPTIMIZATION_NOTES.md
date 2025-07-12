# ADC Optimization Notes

## Performance Issue
- ADC.process was taking 0.26ms out of 0.39ms main loop time (67% of total)
- Main loop running at 2.5kHz was bottlenecked by ADC reads

## Root Cause
- 4 analog/digital reads every loop iteration
- analogReadAveraging(16) - hardware averaging of 16 samples per read
- All sensors read at full loop rate (2.5kHz)

## Optimizations Applied

### 1. Reduced Hardware Averaging
**File:** `/lib/aio_autosteer/ADProcessor.cpp`
**Line:** ~48
```cpp
// OLD:
analogReadAveraging(16);  // Average 16 samples

// NEW:
analogReadAveraging(4);   // Average 4 samples (was 16) for faster reads
```

### 2. Differential Sensor Reading Rates
**File:** `/lib/aio_autosteer/ADProcessor.cpp`
**Function:** `ADProcessor::process()`

Changed from reading all sensors every loop to:
- WAS (Wheel Angle Sensor): Read every loop (2.5kHz) - critical for steering
- Other sensors: Read every 10ms (100Hz) - adequate for monitoring

**Code change:**
```cpp
void ADProcessor::process()
{
    // Always read WAS - critical for steering
    updateWAS();
    
    // Read other sensors at reduced rate (every 10ms = 100Hz)
    static uint32_t lastSlowRead = 0;
    uint32_t now = millis();
    
    if (now - lastSlowRead >= 10) {
        lastSlowRead = now;
        
        // Update switches
        updateSwitches();
        
        // Read kickout sensors
        kickoutAnalogRaw = analogRead(AD_KICKOUT_A_PIN);
        motorCurrentRaw = analogRead(AD_CURRENT_PIN);
        
        // Update pressure sensor reading with filtering
        // Scale 12-bit ADC (0-4095) to match NG-V6 behavior
        float sensorSample = (float)kickoutAnalogRaw;
        sensorSample *= 0.15f;  // Scale down to try matching old AIO
        sensorSample = min(sensorSample, 255.0f);  // Limit to 1 byte (0-255)
        pressureReading = pressureReading * 0.8f + sensorSample * 0.2f;  // 80/20 filter
    }
    
    lastProcessTime = millis();
}
```

## Expected Performance Improvement
- 4x faster ADC reads (16 samples → 4 samples)
- 25x fewer reads for non-critical sensors
- ADC.process time should drop from 0.26ms to ~0.07ms
- Main loop frequency could increase from 2.5kHz to ~5kHz

## Files Modified
1. `/lib/aio_autosteer/ADProcessor.cpp` - Two changes as noted above