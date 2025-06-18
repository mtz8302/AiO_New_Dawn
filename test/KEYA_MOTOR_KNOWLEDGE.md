# Keya Motor Control Knowledge

## Key Findings

### CAN IDs
- **0x06000001** - Command ID (send TO motor)
- **0x07000001** - Heartbeat ID (receive FROM motor)  
- **0x05800001** - Acknowledge ID (receive FROM motor)

### Command Format
All commands are 8 bytes, extended CAN ID:
- Byte 0: Always 0x23
- Byte 1: Command type
  - 0x00 = Speed command
  - 0x0C = Disable motor
  - 0x0D = Enable motor
- Byte 2: Always 0x20
- Byte 3: Always 0x01
- Bytes 4-7: Command specific

### Speed Command
- Bytes 4-5: Speed value in big-endian (high byte first)
- Speed range: -1000 to +1000 (maps to -100% to +100%)
- Example: 500 (50rpm) = 0x01F4, so byte[4]=0x01, byte[5]=0xF4
- Bytes 6-7: Direction indicator
  - Positive speed: 0x00, 0x00
  - Negative speed: 0xFF, 0xFF

### Enable/Disable Command
- Bytes 4-7: Always 0x00, 0x00, 0x00, 0x00

### Critical Requirements
1. **Must send BOTH speed and enable commands every 20ms**
2. Send speed command first, then enable command
3. Motor stops if commands aren't received within timeout

### Startup Sequence
1. Send speed command (even if 0)
2. Send enable command
3. Start 20ms resend timer

### Example Working Sequence
```
// Speed 500 (50rpm)
23 00 20 01 01 F4 00 00

// Enable
23 0D 20 01 00 00 00 00

// Repeat both every 20ms
```

## Issues Discovered
- Serial.flush() can cause crashes after motor enable (electrical interference?)
- delay() function crashes after motor enable
- Complex architectures with callbacks/message routing cause crashes
- Simple direct CAN write/read works best