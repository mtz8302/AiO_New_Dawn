# AiO New Dawn Front Panel LED Status Guide

This guide explains the meaning of the four status LEDs on the front panel of the AiO New Dawn board.

## LED Overview

The front panel has four status LEDs arranged horizontally:

1. **PWR/ETH** - Power and Ethernet Status
2. **GPS** - GPS Status
3. **IMU** - IMU/INS Status  
4. **AUTO** - Autosteer Status

Each LED can display different colors and patterns to indicate various states.

---

## PWR/ETH LED (Power/Ethernet)

This LED indicates power, Ethernet connection, and AgIO communication status.

| Color | Pattern | Meaning |
|-------|---------|---------|
| 🔴 **RED** | Solid | No Ethernet connection |
| 🟡 **YELLOW** | Blinking | Ethernet connected but no AgIO communication |
| 🟢 **GREEN** | Solid | Ethernet connected and AgIO communication active |

**Notes:**
- AgIO is considered "connected" when the board is receiving any PGN messages from AgIO
- The connection timeout is 5 seconds - if no PGNs are received for 5 seconds, status changes to yellow

---

## GPS LED

This LED indicates GPS receiver status and fix quality.

| Color | Pattern | Meaning |
|-------|---------|---------|
| ⚫ **OFF** | - | No GPS data being received |
| 🔴 **RED** | Solid | GPS data received but no position fix |
| 🟡 **YELLOW** | Solid | Standard GPS or DGPS fix |
| 🟡 **YELLOW** | Blinking | RTK Float (partial RTK solution) |
| 🟢 **GREEN** | Solid | RTK Fixed (full RTK solution, best accuracy) |

**Fix Quality Details:**
- **No Fix**: GPS receiver is powered but cannot determine position
- **GPS/DGPS**: Standard positioning, accuracy typically 1-3 meters
- **RTK Float**: Partial RTK solution, accuracy typically 20-50 cm
- **RTK Fixed**: Full RTK solution, accuracy typically 2-5 cm

---

## IMU LED (Inertial Measurement Unit)

This LED indicates the status of the IMU or INS (Inertial Navigation System).

| Color | Pattern | Meaning |
|-------|---------|---------|
| ⚫ **OFF** | - | No IMU detected |
| 🔴 **RED** | Solid | IMU detected but not initialized |
| 🟡 **YELLOW** | Solid | IMU initialized but data not valid/calibrating |
| 🟢 **GREEN** | Solid | IMU fully operational with valid data |

**Supported IMU Types:**
- **BNO08x**: External IMU module
- **TM171**: Alternative external IMU
- **UM981 INS**: Integrated INS in UM981 GPS module

**INS Alignment States (UM981):**
- During alignment, the LED will show yellow
- Once fully aligned (status 3), the LED turns green

---

## AUTO LED (Autosteer)

This LED indicates autosteer engagement and steering status.

| Color | Pattern | Meaning |
|-------|---------|---------|
| ⚫ **OFF** | - | Autosteer disabled/not engaged |
| 🔴 **RED** | Solid | Autosteer engaged but AgIO communication lost |
| 🟡 **YELLOW** | Solid | Autosteer engaged, communicating, but not actively steering |
| 🟢 **GREEN** | Solid | Autosteer engaged and actively steering |

**Additional States:**
- The LED will be OFF if autosteer is manually disabled via button or switch
- Red indicates a communication problem that needs attention
- Yellow typically means the system is ready but waiting for steering commands
- Green confirms active steering control

---

## Startup Sequence

During startup, all LEDs will briefly test their colors:
1. All LEDs turn RED (0.5 seconds)
2. All LEDs turn YELLOW (0.5 seconds)  
3. All LEDs turn GREEN (0.5 seconds)
4. All LEDs turn OFF
5. Normal operation begins

This test sequence verifies all LED colors are working properly.

---

## Troubleshooting

### PWR/ETH LED Issues
- **Stays Red**: Check Ethernet cable and connection
- **Stays Yellow**: Verify AgIO is running and configured for the correct network
- **Flashing between states**: Check for intermittent network issues

### GPS LED Issues
- **Stays Off**: Check GPS module connection and power
- **Stays Red**: Verify antenna connection and sky visibility
- **Never reaches Green (RTK)**: Check NTRIP/base station configuration

### IMU LED Issues
- **Stays Off**: Verify IMU module is connected properly
- **Stays Red**: IMU may need firmware update or be defective
- **Stays Yellow**: Allow more time for calibration, ensure vehicle is stationary

### AUTO LED Issues
- **Won't turn on**: Check autosteer enable switch/button
- **Stays Red**: Verify network connection and AgIO communication
- **Never reaches Green**: Check if AgOpenGPS is sending steering commands

---

## LED Brightness

The LED brightness can be adjusted in the web interface under Device Settings. The brightness setting affects all LEDs equally and ranges from 0% (very dim) to 100% (full brightness).