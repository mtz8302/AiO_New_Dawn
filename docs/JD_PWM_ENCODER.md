# John Deere Autotrac PWM Encoder Support

## Overview

The AiO New Dawn firmware (v1.0.19-beta and later) includes support for John Deere Autotrac PWM encoders as a steering wheel motion detection (kickout) method. This feature allows tractors equipped with John Deere's PWM-based steering wheel encoders to detect manual steering input and disengage autosteer accordingly.

## How It Works

The JD PWM encoder outputs a PWM signal with a duty cycle that changes based on steering wheel rotation:
- PWM period range: 500-4500 microseconds
- Center position: ~2600 microseconds
- The firmware measures duty cycle changes to detect steering wheel motion
- Motion above the configured threshold triggers autosteer disengagement

## Hardware Connection

### Pin Assignment
- Connect the JD PWM encoder signal to the **Kickout-A pin** (normally used for pressure sensors)
- This is pin A12 on the Teensy 4.1 (physical pin location depends on your PCB version)

### Wiring
1. **Signal Wire**: Connect the PWM output from the JD encoder to Kickout-A
2. **Power**: Provide appropriate power to the encoder per John Deere specifications
3. **Ground**: Ensure common ground between encoder and AiO board

**IMPORTANT**: When JD PWM mode is enabled, the Kickout-A pin switches from analog to digital mode, so it cannot be used for pressure sensors simultaneously.

## Configuration

### Enable JD PWM Mode

1. Access the AiO web interface at `http://192.168.5.126`
2. Navigate to **Device Settings**
3. Under **Turn Sensor Configuration**:
   - Check the box for **John Deere PWM Encoder Mode**
   - Set the **JD PWM Motion Threshold** (default: 20)
     - Lower values = more sensitive (easier to trigger kickout)
     - Higher values = less sensitive (requires more wheel movement)
     - Range: 5-100
4. Click **Apply Changes**

### AgOpenGPS Configuration

**IMPORTANT**: AgOpenGPS must be configured to use "Pressure Sensor" kickout mode for the JD PWM encoder to work. This is because the JD PWM feature sends its motion detection data through the pressure sensor channel for compatibility.

In AgOpenGPS, configure the steer module settings:
1. Open the **Steer Configuration** window
2. In the **Turn Sensor** or **Kickout** section:
   - **Enable "Pressure Sensor"** (REQUIRED - even though you're using a PWM encoder)
   - Set the pressure threshold value
   - The threshold in AgOpenGPS works together with the web interface threshold
3. Disable other kickout methods (Encoder, Current) unless you have those sensors on different pins

**Why Pressure Mode?** The JD PWM encoder reuses the pressure sensor data channel to maintain compatibility with AgOpenGPS without requiring software modifications. AgOpenGPS sees the motion values as "pressure" readings.

## Calibration

### Finding the Right Threshold

1. Enable JD PWM mode with the default threshold (20)
2. Start autosteer and try turning the steering wheel manually
3. Adjust the threshold based on your needs:
   - If kickout is too sensitive (triggers too easily): Increase the threshold
   - If kickout requires too much wheel movement: Decrease the threshold

### Testing Procedure

1. With the tractor stationary and engine running:
   - Enable autosteer
   - Gently turn the steering wheel
   - Verify that autosteer disengages
   
2. Test different steering speeds:
   - Slow, deliberate movements
   - Quick corrections
   - Ensure kickout works reliably in both cases

3. Field testing:
   - Test in actual field conditions
   - Verify that normal field bumps don't trigger false kickouts
   - Ensure manual override works when needed

## Troubleshooting

### Kickout Not Working
- Verify JD PWM mode is enabled in Device Settings
- Check wiring connections
- Ensure the PWM signal is reaching the Kickout-A pin
- Try lowering the threshold value

### False Kickouts
- Increase the threshold value
- Check for loose connections or signal interference
- Verify encoder is properly mounted and not vibrating

### Conflict with Pressure Sensor
- JD PWM mode and analog pressure sensors cannot be used simultaneously on the same pin
- If you need pressure sensor kickout, disable JD PWM mode
- Only one kickout method can use the Kickout-A pin at a time

### AgOpenGPS Shows Wrong Sensor Type
- This is normal - AgOpenGPS will show "Pressure Sensor" even though you're using JD PWM
- The firmware sends JD PWM data through the pressure channel for compatibility
- This does not affect functionality

## Technical Details

### Signal Processing
- Rising edge interrupt captures start time
- Falling edge interrupt calculates duty cycle
- Motion is calculated as the change in duty cycle between samples
- Filtering prevents large jumps from causing false triggers

### Performance
- PWM measurement resolution: 1 microsecond
- Update rate: Synchronized with main control loop (100Hz)
- Minimal CPU overhead using hardware interrupts

## Compatibility

- Firmware version: 1.0.19-beta or later
- Requires Teensy 4.1
- Compatible with all motor driver types (PWM, Danfoss, Keya)
- Cannot be used simultaneously with analog pressure sensors

## Future Enhancements

Potential future improvements:
- Automatic threshold calibration
- Dual-mode support (switching between pressure and JD PWM)
- Enhanced filtering algorithms
- Data logging for threshold optimization