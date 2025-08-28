// CANGlobals.cpp - Global CAN bus instances
#include "CANGlobals.h"
#include <Arduino.h>
#include "EventLogger.h"

// Instantiate the global CAN objects
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> globalCAN1;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> globalCAN2;
FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> globalCAN3;

void initializeGlobalCANBuses() {
    LOG_INFO(EventSource::CAN, "Initializing Global CAN Buses");
    
    // Initialize CAN1
    globalCAN1.begin();
    globalCAN1.setBaudRate(250000);
    LOG_INFO(EventSource::CAN, "CAN1: 250kbps");
    
    // Initialize CAN2
    globalCAN2.begin();
    globalCAN2.setBaudRate(250000);
    LOG_INFO(EventSource::CAN, "CAN2: 250kbps");
    
    // Initialize CAN3
    globalCAN3.begin();
    globalCAN3.setBaudRate(250000);
    LOG_INFO(EventSource::CAN, "CAN3: 250kbps");
    
    LOG_INFO(EventSource::CAN, "Global CAN Buses Ready");
}