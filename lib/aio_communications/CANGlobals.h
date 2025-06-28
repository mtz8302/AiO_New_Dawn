// CANGlobals.h - Global CAN bus instances to avoid ownership conflicts
#ifndef CAN_GLOBALS_H
#define CAN_GLOBALS_H

#include <FlexCAN_T4.h>

// Global CAN instances - defined here, instantiated in CANGlobals.cpp
extern FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> globalCAN1;
extern FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> globalCAN2;
extern FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> globalCAN3;

// Initialize all CAN buses
void initializeGlobalCANBuses();

#endif // CAN_GLOBALS_H