#ifndef PGN_UTILS_H
#define PGN_UTILS_H

#include <stdint.h>

// Utility function for PGN CRC calculation
// Calculates and sets the CRC byte for PGN messages
// Based on AOG NG-V6 implementation
inline void calculateAndSetCRC(uint8_t myMessage[], uint8_t myLen) 
{
    if (myLen <= 2) return;

    uint8_t crc = 0;
    for (uint8_t i = 2; i < myLen - 1; i++) 
    {
        crc = (crc + myMessage[i]);
    }
    myMessage[myLen - 1] = crc;
}

#endif // PGN_UTILS_H