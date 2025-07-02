# Continuation Prompt - AsyncWebServer Implementation

## Current Status
We have successfully:
1. Migrated from Mongoose to QNEthernet for all network functionality
2. Implemented AsyncWebServer_Teensy41 with a modular PROGMEM-based web page structure
3. Created multi-language support (English/German) with easy switching
4. Added EventLogger configuration page for syslog settings

## System Architecture
- **Network Stack**: QNEthernet with static IP 192.168.5.126
- **UDP**: AsyncUDP_Teensy41 for AgOpenGPS communication (port 8888) and RTCM (port 2233)
- **Web Server**: AsyncWebServer_Teensy41 on port 80
- **Web Pages**: PROGMEM-based templates in `/lib/aio_system/web_pages/`

## File Structure
```
lib/aio_system/
├── WebManager.cpp/h         # Main web server implementation
└── web_pages/
    ├── WebPages.h           # Language selector and includes
    ├── CommonStyles.h       # Shared CSS in PROGMEM
    ├── en_HomePage.h        # English home page
    ├── en_EventLoggerPage.h # English EventLogger config
    ├── de_HomePage.h        # German home page
    └── de_EventLoggerPage.h # German EventLogger config
```

## How the Web System Works
1. Each page is stored as a PROGMEM string with placeholders like `%IP_ADDRESS%`
2. WebManager loads the template and replaces placeholders with actual values
3. Language selection stored in `currentLanguage` variable
4. Pages are selected via `WebPageSelector::getPageName(language)`

## Next Configuration Pages to Consider
Based on the discussion, this web UI is for **installation-time configuration** only - things that currently require editing INO files and recompiling:

### Network Configuration (`/network`)
- Static IP address
- Subnet mask
- Gateway
- DNS server (if needed)

### Hardware Selection (`/hardware`)
- IMU type (BNO085, CMPS14, etc.)
- GPS configuration (single/dual, baud rate)
- Motor driver type (Cytron, IBT2, Keya, Danfoss)
- CAN termination resistor enable/disable

### Pin Assignments (`/pins`)
- Work switch pin
- Steer switch pin
- Remote/Auto switch pins
- Any other configurable I/O

### Machine Parameters (`/machine`)
- Wheelbase
- Antenna height
- Tool measurements
- Any machine-specific calibration values

## Design Philosophy
- **No stats/monitoring** - farmers don't care about packet counts
- **Set and forget** - configure during installation, rarely touch again
- **Simple and functional** - no fancy dashboards
- **Eliminate code editing** - no more "edit line 47 and recompile"

## Technical Notes
- AsyncWebServer handles all requests asynchronously
- PROGMEM templates save RAM (HTML stays in flash)
- Simple string replacement for dynamic content
- Each page in separate file for easy maintenance/translation

## Current Branch
Working on: `feature/qnethernet-migration`

## To Continue Tomorrow
1. Decide which configuration page to implement next
2. Create the PROGMEM template in appropriate language files
3. Add route handler in WebManager
4. Implement the configuration save/load from EEPROM
5. Test the configuration changes take effect

## Example: Adding a New Page
1. Create `en_NetworkPage.h` with PROGMEM template
2. Add to `WebPages.h` includes and selector
3. Add route in `WebManager::setupRoutes()`
4. Add handler method like `handleNetworkPage()`
5. Create API endpoints for saving configuration