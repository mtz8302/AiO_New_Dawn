# Motor Driver Detection Test Plan

## Phase 1 Testing

### 1. PGN 251 Byte 8 Reception Test
- Send PGN 251 with byte 8 = 0x00 (DRV8701 + Wheel Encoder)
- Send PGN 251 with byte 8 = 0x01 (Danfoss + Wheel Encoder)
- Send PGN 251 with byte 8 = 0x02 (DRV8701 + Pressure Sensor)
- Send PGN 251 with byte 8 = 0x03 (Danfoss + Pressure Sensor)
- Send PGN 251 with byte 8 = 0x04 (DRV8701 + Current Sensor)

Expected: Log should show "Motor driver config byte: 0xXX" for each test

### 2. EEPROM Storage Test
- Send PGN 251 with byte 8 = 0x01
- Restart device
- Check logs for "Read motor config from EEPROM: 0x01"

### 3. Keya Detection Test
- Connect Keya motor to CAN3
- Power on Keya motor
- Within 2 seconds, should see "Detected Keya CAN motor via heartbeat"

### 4. Default Fallback Test
- Disconnect all CAN devices
- Set PGN 251 byte 8 = 0xFF (invalid)
- Restart device
- Should see "Unknown motor config 0xFF, defaulting to DRV8701 with wheel encoder"

## Test Commands

To send test PGN 251 with motor driver config = 0x01:
```
80 81 7F FB 08 00 00 00 00 00 00 00 01 [CRC]
```

Where:
- 80 81 = Header
- 7F = Source (AgOpenGPS)
- FB = PGN 251
- 08 = Length (8 bytes of data)
- Byte 0-6 = Existing config bytes
- Byte 7 = 0x01 (Motor driver config)
- [CRC] = Calculated checksum

Example with actual values from your system:
```
80 81 7F FB 08 48 90 10 00 00 00 00 01 6B
```
This would set motor driver to Danfoss + Wheel Encoder (0x01)