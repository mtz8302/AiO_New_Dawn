# AiO New Dawn Architecture Overview

This document provides a high-level overview of the AiO New Dawn system architecture and its key components.

## System Overview

AiO New Dawn is a modular agricultural control system built on the Teensy 4.1 platform. It integrates GPS/GNSS navigation, automated steering, section control, and network communication to work with AgOpenGPS.

## Core Architecture Principles

- **Modular Design**: Each subsystem is self-contained with clear interfaces
- **Event-Driven Communication**: Uses PGN (Parameter Group Number) messages for inter-module communication
- **Real-Time Processing**: Critical control loops run at consistent intervals
- **Network-First**: Built around QNEthernet for reliable UDP communication

## Main Components

### System Management

**main.cpp**
- Entry point that initializes all subsystems in the correct order
- Manages the main loop that polls each processor

**ConfigManager** (`lib/aio_config/`)
- Stores and manages all system configuration in EEPROM
- Provides centralized access to hardware pins, network settings, and operational parameters

**EventLogger** (`lib/aio_system/`)
- Centralized logging system with configurable output to Serial and UDP syslog
- Supports multiple severity levels and event sources with rate limiting

**CommandHandler** (`lib/aio_system/`)
- Provides interactive serial menu for configuration and debugging
- Handles user input for logging control and system status

### Network & Communication

**QNetworkBase** (`lib/aio_system/`)
- Manages Ethernet initialization and connection status
- Handles DHCP/static IP configuration and monitors link state

**AsyncUDPHandler** (`lib/aio_system/`)
- Manages all UDP communication with AgOpenGPS
- Routes incoming PGN messages to appropriate processors

**SubnetManager** (`lib/aio_system/`)
- Handles AgOpenGPS subnet scanning and module identification (PGN 201)
- Manages "Hello" broadcasts and scan responses

**WebManager** (`lib/aio_system/`)
- Provides web interface for configuration and status monitoring
- Supports OTA firmware updates and multi-language pages

**SerialManager** (`lib/aio_communications/`)
- Manages multiple hardware serial ports for GPS, RTK, IMU, and RS232
- Provides bridging functionality between serial devices and UDP

### Navigation & Positioning

**GNSSProcessor** (`lib/aio_navigation/`)
- Parses NMEA and UBX messages from GPS/GNSS receivers
- Manages dual GPS inputs with automatic failover

**RTCMProcessor** (`lib/aio_system/`)
- Handles RTK correction data (RTCM messages)
- Routes corrections between network/serial sources and GPS receivers

**IMUProcessor** (`lib/aio_navigation/`)
- Interfaces with BNO08x IMU for heading and roll data
- Processes IMU data and forwards to AgOpenGPS via PGN

**NAVProcessor** (`lib/aio_navigation/`)
- Combines GPS and IMU data for complete navigation solution
- Manages coordinate transformations and heading calculations

### Vehicle Control

**AutosteerProcessor** (`lib/aio_autosteer/`)
- Implements automated steering control with multiple motor driver support
- Handles steering angle sensing, motor control, and safety switches
- Sends PGN253 status messages with current wheel angle even when autosteer is off
- Supports wheel angle sensor fusion for improved accuracy

**MachineProcessor** (`lib/aio_system/`)
- Manages section control for implements (sprayers, seeders, etc.)
- Controls up to 16 sections via PCA9685 PWM controller at address 0x44
- Supports machine functions: hydraulic lift, tramlines, and geo-stop
- Automatically protects outputs 5 & 6 when Danfoss valve is configured
- Initializes Danfoss valve to 50% PWM (centered) at boot

**MotorDriverInterface** (`lib/aio_autosteer/`)
- Abstract interface for different motor driver types
- Implementations include:
  - PWM drivers: Cytron MD30C, IBT-2, DRV8701
  - CAN-based: Keya motor
  - Hydraulic: Danfoss proportional valve
- Supports coast/brake mode configuration for PWM drivers

**PWMProcessor** (`lib/aio_system/`)
- Generates speed pulse output for rate controllers
- Configurable frequency and duty cycle based on ground speed

### Hardware Interfaces

**HardwareManager** (`lib/aio_config/`)
- Manages hardware initialization and pin assignments
- Provides buzzer control and hardware status reporting

**I2CManager** (`lib/aio_communications/`)
- Manages I2C bus for multiple devices:
  - PCA9685 at 0x44: Section outputs and machine control
  - PCA9685 at 0x70: RGB LED control
- Handles bus initialization at 1MHz and error recovery

**CANManager** (`lib/aio_communications/`)
- Manages FlexCAN interfaces for motor control and implement communication
- Auto-detects connected CAN devices like Keya steering motors

**LEDManagerFSM** (`lib/aio_system/`)
- Controls RGB LEDs via PCA9685 at address 0x70 using FSM pattern
- Four status LEDs: PWR/ETH, GPS, STEER, INS
- Blue pulse overlays for RTCM data (GPS LED) and button press (STEER LED)
- Updates every 100ms with 25% default brightness

**ADProcessor** (`lib/aio_autosteer/`)
- Reads analog inputs for steering angle sensor (WAS)
- Provides filtering and calibration for analog signals
- Monitors work switch and remote switch inputs

### Data Processing

**PGNProcessor** (`lib/aio_system/`)
- Central router for all PGN messages in the system
- Manages callback registration and message distribution

**UBXParser** (`lib/aio_navigation/`)
- Parses u-blox UBX protocol messages
- Extracts high-precision position and timing data

**BNO_RVC** (`lib/aio_navigation/`)
- Interfaces with BNO08x IMU in RVC (Robot Vacuum Cleaner) mode
- Simplified binary protocol for heading and motion data

## Data Flow

1. **GPS/IMU Data** → GNSSProcessor/IMUProcessor → NAVProcessor → PGN messages → UDP to AgOpenGPS
2. **Steering Commands** ← UDP from AgOpenGPS → PGN messages → AutosteerProcessor → Motor Driver
3. **Section Control** ← UDP from AgOpenGPS → PGN messages → MachineProcessor → PCA9685 → Sections
4. **Configuration** ← Web Interface/Serial Menu → ConfigManager → EEPROM

## Key Design Patterns

- **Singleton Pattern**: Used for managers and processors that need global access
- **Factory Pattern**: Motor drivers created based on detected hardware or EEPROM configuration
- **Observer Pattern**: PGN callback system for message routing
- **Finite State Machine**: LED control and network connection handling
- **Strategy Pattern**: Multiple motor driver implementations with common interface

## Building and Development

The project uses PlatformIO with the Teensy 4.1 platform. Key directories:
- `lib/` - All modular components
- `src/` - Main application entry point
- `docs/` - Documentation
- `testing/` - Test utilities and examples

## Network Protocol

Uses AgOpenGPS PGN protocol over UDP port 8888:
- Receives control commands from AgOpenGPS
- Sends position, heading, and status data
- Supports module discovery via subnet scanning

## Recent Improvements

### Motor Driver Enhancements
- **Danfoss Valve Support**: Proper initialization at boot with 50% PWM centering
- **PWM Mode Fix**: Standard PWM mode for PCA9685 eliminates pulse glitches
- **Coast/Brake Mode**: Configurable motor behavior for PWM drivers
- **EEPROM-based Detection**: Motor type configuration stored persistently

### System Reliability
- **PGN253 Updates**: Wheel angle reported even when autosteer is disabled
- **Output Protection**: Machine outputs 5 & 6 reserved for Danfoss when configured
- **LED Responsiveness**: Removed hysteresis for instant RTCM pulse indication
- **Sensor Fusion**: Support for combined wheel angle sensor data

### Hardware Compatibility
- **Multiple Motor Drivers**: Cytron, IBT-2, DRV8701, Keya CAN, Danfoss
- **Dual GPS Support**: Automatic failover between primary and secondary
- **IMU Options**: BNO08x, TM171, UM981 integrated INS