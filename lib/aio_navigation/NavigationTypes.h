#ifndef NAVIGATION_TYPES_H
#define NAVIGATION_TYPES_H

// GPS detection modes
enum class GPSMode {
    NO_GPS,           // No GPS detected
    SINGLE,           // GGA/VTG single antenna
    DUAL_MODE1,       // GPS2 with RELPOSNED (Teensy calculates heading/roll)
    DUAL_MODE2,       // KSXT or HPR (use heading and sub pitch for roll)
    IMU_INTEGRATED    // INSPVA (IMU integrated single, use heading and roll from GPS)
};

// IMU detection modes
enum class IMUType {
    NONE = 0,         // No IMU detected
    BNO085,           // BNO08x IMU present, use with single GPS
    TM171,            // TM171 IMU present, use with single GPS
    UM981_INTEGRATED, // IMU integrated with GPS (UM981)
    CMPS14,           // CMPS14 compass
    GENERIC           // Generic IMU
};

// For backward compatibility
enum class IMUMode {
    NO_IMU,           // No IMU detected
    BNO08X,           // BNO08x IMU present, use with single GPS
    TM171,            // TM171 IMU present, use with single GPS
    INTEGRATED        // IMU integrated with GPS (UM981)
};

#endif // NAVIGATION_TYPES_H