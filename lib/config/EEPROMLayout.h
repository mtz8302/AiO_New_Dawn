#ifndef EEPROM_LAYOUT_H
#define EEPROM_LAYOUT_H

// EEPROM Version - increment this when EEPROM layout changes
#define EEPROM_VERSION 106

// EEPROM Address Map
#define EE_VERSION_ADDR      1      // Version number (2 bytes)
#define NETWORK_CONFIG_ADDR  100    // Network configuration (100-199)
#define STEER_CONFIG_ADDR    200    // Steer configuration (200-299)
#define STEER_SETTINGS_ADDR  300    // Steer settings (300-399)
#define GPS_CONFIG_ADDR      400    // GPS configuration (400-499)
#define MACHINE_CONFIG_ADDR  500    // Machine configuration (500-599)
#define KWAS_CONFIG_ADDR     600    // KWAS configuration (600-699)
#define INS_CONFIG_ADDR      700    // INS configuration (700-799)

#endif // EEPROM_LAYOUT_H