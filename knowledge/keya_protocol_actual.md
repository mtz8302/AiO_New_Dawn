# Keya Motor Protocol - Actual Implementation

Based on CAN bus captures of the V6-NG system running with a Keya motor.

## CAN Message IDs

- **0x06000001** - Command messages TO motor (enable/disable/speed)
- **0x07000001** - Heartbeat/status FROM motor  
- **0x05800001** - Acknowledge FROM motor

## Command Messages (TO Motor - ID 0x06000001)

All commands use the same CAN ID with different data bytes.

### Enable Command
```
23 0D 20 01 00 00 00 00
```
- Byte 0: 0x23 (command prefix)
- Byte 1: 0x0D (enable command)
- Byte 2: 0x20
- Byte 3: 0x01
- Bytes 4-7: 0x00

### Disable Command
```
23 0C 20 01 00 00 00 00
```
- Byte 0: 0x23 (command prefix)
- Byte 1: 0x0C (disable command)
- Byte 2: 0x20
- Byte 3: 0x01
- Bytes 4-7: 0x00

### Speed Command
```
23 00 20 01 [SPEED_L] [SPEED_H] FF FF
```
- Byte 0: 0x23 (command prefix)
- Byte 1: 0x00 (speed command)
- Byte 2: 0x20
- Byte 3: 0x01
- Bytes 4-5: Speed value (signed 16-bit, little-endian)
- Bytes 6-7: 0xFF

Example: Speed -18435 (0xB7FD)
```
23 00 20 01 FD B7 FF FF
```

## Heartbeat Messages (FROM Motor - ID 0x07000001)

### When Stopped
```
00 00 00 00 00 00 00 01
```
- All zeros except last byte

### When Running
```
[ANGLE_H] [ANGLE_L] [SPEED_H] [SPEED_L] [CURRENT_H] [CURRENT_L] [ERROR_H] [ERROR_L]
```
- Bytes 0-1: Cumulative angle (big-endian)
- Bytes 2-3: Motor speed (big-endian, signed)
- Bytes 4-5: Motor current (big-endian, signed)
- Bytes 6-7: Error code (big-endian)

Example: Running motor
```
A7 CE FF C4 FF FF 00 00
```
- Angle: 0xCEA7
- Speed: 0xC4FF (-15105)
- Current: 0xFFFF (-1)
- Error: 0x0000 (no error)

## Acknowledge Messages (FROM Motor - ID 0x05800001)

Response to commands:
```
60 [CMD] 20 00 00 00 00 00
```
- Byte 0: 0x60 (ack prefix)
- Byte 1: Echo of command type (0x00, 0x0C, 0x0D)
- Byte 2: 0x20
- Remaining bytes: 0x00

## Key Differences from V6-NG Source Code

The actual implementation differs completely from the source code:

| Aspect | V6-NG Source | Actual CAN Capture |
|--------|--------------|-------------------|
| Speed ID | 0x0CFE1A27 | 0x06000001 |
| Enable ID | 0x0CF00400 | 0x06000001 (cmd 0x0D) |
| Disable ID | 0x0CF00301 | 0x06000001 (cmd 0x0C) |
| Protocol | Different IDs per command | Same ID, different data |
| Speed Format | Complex template | Simple: 23 00 20 01 [speed] FF FF |

## Implementation Notes

1. **20ms Command Repeat**: Keya requires speed commands to be sent every 20ms
2. **Little vs Big Endian**: Commands use little-endian for speed, heartbeats use big-endian
3. **Speed Range**: Appears to use full int16 range (-32767 to +32767)
4. **Acknowledgments**: Motor acknowledges each command with ID 0x05800001

This is why it was called a "kludge" - the actual implementation doesn't match the documented protocol at all!