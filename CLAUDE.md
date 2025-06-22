- 1 - 0x7000001 x 8 00 00 00 00 00 00 00 01 - Heartbeat from keya.
- 2 - 0x6000001 x 8 23 0C 20 01 00 00 00 00 - Diaable from New Dawn.
- 3 -  0x6000001 x 8 23 00 20 01 00 00 00 00 - Speed to 0 from New Dawn.
- 4 - 0x5800001 x 8 60 0C 20 00 00 00 00 00 - Acknowledge from Keya motor.
~/.platformio/penv/bin/pio run -e teensy41
No module should register for PGN 200 or 202 - they're broadcast to everyone automatically.