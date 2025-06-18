# Motor Driver Test Instructions

## Enable Motor Test Mode

1. Edit `src/main.cpp` and change line 20:
   ```cpp
   static bool MOTOR_TEST_MODE = true;  // Change from false to true
   ```

2. Compile and upload to Teensy 4.1

## Test Sequence

When the system starts with motor test mode enabled:

1. **Auto-Detection**: The system will auto-detect motor type
   - If Keya motor heartbeats detected on CAN3 → Keya driver
   - Otherwise → DRV8701 PWM driver

2. **Automatic Test**: Runs through test sequence
   - Enable motor
   - Forward 25% (2 seconds)
   - Forward 50% (2 seconds)
   - Stop (1 second)
   - Reverse -25% (2 seconds)
   - Reverse -50% (2 seconds)
   - Stop and disable

3. **Interactive Mode**: After automatic test, use serial commands:
   - `+` : Increase speed by 10%
   - `-` : Decrease speed by 10%
   - `e` : Enable motor
   - `d` : Disable motor
   - `s` : Stop (speed to 0)
   - `f` : Forward (keep current speed magnitude)
   - `r` : Reverse (keep current speed magnitude)
   - `?` : Show detailed status

## What to Verify

### For DRV8701 (PWM Driver):
- PWM signal on pin 5
- Direction signal on pin 6
- SLEEP/Enable on pin 4
- Current sensing on A13 (if connected)
- Motor responds to speed changes
- Direction changes work correctly

### For Keya (CAN Driver):
- CAN3 messages being sent every 20ms
- Command ID: 0x06000001
- Enable command: `23 0D 20 01 00 00 00 00`
- Disable command: `23 0C 20 01 00 00 00 00`
- Speed command: `23 00 20 01 [SPEED_L] [SPEED_H] FF FF`
- Motor acknowledges with ID 0x05800001
- Heartbeat messages from motor (ID 0x07000001)

## Monitoring

The system will print status every second showing:
- Motor type
- Current speed
- Enabled/disabled state
- Current draw (if available)

## Safety Notes

1. **Start with motor disconnected** from wheels/steering
2. Ensure emergency stop is available
3. Start with low speeds (25%)
4. Monitor current draw if available
5. Check for proper direction (forward/reverse)

## Troubleshooting

### DRV8701 Issues:
- Check SLEEP pin is going high when enabled
- Verify PWM frequency (18.31kHz)
- Check current limit settings
- Ensure motor power supply is connected

### Keya Issues:
- Verify CAN3 termination resistor (120Ω)
- Check CAN baud rate (250kbps)
- Monitor for heartbeat messages
- Ensure motor has power
- Check motor DIP switch settings

## Disable Test Mode

Remember to set `MOTOR_TEST_MODE = false` when done testing!