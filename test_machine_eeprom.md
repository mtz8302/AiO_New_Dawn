# Machine Output EEPROM Test Procedure

## Test Setup
1. Connect to serial monitor (115200 baud)
2. Enable DEBUG logging in web interface or serial console
3. Have AgOpenGPS ready with machine configuration

## Test Steps

### 1. Initial Boot Test
- Power on the Teensy
- Look for these messages during startup:
  ```
  [MACHINE] Pin configuration loaded from EEPROM
  [MACHINE]   Output 1 -> Section 1
  [MACHINE]   Output 2 -> Section 2
  ...
  ```
  OR
  ```
  [MACHINE] No saved pin config, using defaults
  ```

### 2. PGN 236 (Pin Config) Test
- In AgOpenGPS, open Machine Settings
- Configure pin assignments (e.g., assign Output 1 to Hydraulic Up)
- Send configuration to module
- Look for these messages:
  ```
  [MACHINE] PGN 236 - Machine Pin Config received
  [MACHINE] Output 1 assigned to Hyd Up
  [MACHINE] Saving pin config to EEPROM at address 550
  [MACHINE]   Saved pin 1 = function 17 (Hyd Up)
  [MACHINE] Pin configuration saved to EEPROM (24 pins, final addr=576)
  ```

### 3. PGN 238 (Machine Config) Test
- In AgOpenGPS, configure raise/lower times
- Send configuration
- Look for these messages:
  ```
  [MACHINE] PGN 238 - Machine Config received
  [MACHINE] Machine Config: RaiseTime=8s, LowerTime=8s, HydEnable=1, ActiveHigh=0
  [MACHINE] Saving machine configuration to EEPROM...
  [MACHINE] Saving extended machine config to EEPROM at address 580
  [MACHINE] Extended machine config saved to EEPROM (U1=0, U2=0, U3=0, U4=0)
  ```

### 4. Power Cycle Test
- Power off the Teensy
- Power on again
- Verify saved configuration is loaded:
  ```
  [MACHINE] Pin configuration loaded from EEPROM
  [MACHINE]   Output 1 -> Hyd Up
  [MACHINE] Extended machine config loaded from EEPROM
  ```

## Expected EEPROM Addresses
- Pin config: Address 550 (MACHINE_CONFIG_ADDR + 50)
  - Magic: 0xAA55 at 550-551
  - Pin data: 552-575 (24 bytes)
- Extended config: Address 580 (MACHINE_CONFIG_ADDR + 80)
  - Magic: 0xBB66 at 580-581
  - User values: 582-585 (4 bytes)

## Troubleshooting
- If no save messages appear, check:
  1. Is DEBUG logging enabled?
  2. Are PGNs 236/238 being received? (check with PGN monitor)
  3. Is the MachineProcessor initialized successfully?