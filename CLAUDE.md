- 1 - 0x7000001 x 8 00 00 00 00 00 00 00 01 - Heartbeat from keya.
- 2 - 0x6000001 x 8 23 0C 20 01 00 00 00 00 - Diaable from New Dawn.
- 3 -  0x6000001 x 8 23 00 20 01 00 00 00 00 - Speed to 0 from New Dawn.
- 4 - 0x5800001 x 8 60 0C 20 00 00 00 00 00 - Acknowledge from Keya motor.
~/.platformio/penv/bin/pio run -e teensy41
No module should register for PGN 200 or 202 - they're broadcast to everyone automatically.
## Debugging Principles
- When some code we just wrote or modified isn't working, it is most likely that code. Hypothisizing about something external that has been working for days or weeks wastes time and effort. Start with the most likely cause.
- Look within to find the source of an error when working on code you just wrote.

## Command Line Tips
- Use apple mdfind could be useful for:
  - Quick file name searches across the project
  - Finding files by type/extension
  - Content searches (though it depends on Spotlight indexing)
  - Finding files modified recently

## Version Control
- When commiting fixes, increment the patch version number in ./sys/version.h