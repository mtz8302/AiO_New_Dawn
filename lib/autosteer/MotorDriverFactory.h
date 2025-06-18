// MotorDriverFactory.h - Factory for creating motor drivers
#ifndef MOTOR_DRIVER_FACTORY_H
#define MOTOR_DRIVER_FACTORY_H

#include "MotorDriverInterface.h"
#include "PWMMotorDriver.h"
#include "KeyaCANDriver.h"
#include "HardwareManager.h"
#include "CANManager.h"

class MotorDriverFactory {
public:
    // Create motor driver based on configuration
    static MotorDriverInterface* createMotorDriver(MotorDriverType type, 
                                                   HardwareManager* hwMgr,
                                                   CANManager* canMgr) {
        
        switch (type) {
            case MotorDriverType::DRV8701:
                return new PWMMotorDriver(
                    MotorDriverType::DRV8701,
                    hwMgr->getPWM1Pin(),       // PWM pin
                    hwMgr->getPWM2Pin(),       // Direction pin  
                    hwMgr->getSleepPin(),      // Enable pin (SLEEP_PIN on DRV8701)
                    hwMgr->getCurrentPin()     // Current sense pin
                );
                
            case MotorDriverType::KEYA_CAN:
                return new KeyaCANDriver();
                
            default:
                Serial.print("\r\n[MotorFactory] WARNING: Unknown motor type");
                return nullptr;
        }
    }
    
    // Auto-detect motor type
    static MotorDriverType detectMotorType(CANManager* canMgr) {
        // Check for Keya motor on CAN with new CANManager
        if (canMgr && canMgr->isKeyaDetected()) {
            Serial.print("\r\n[MotorFactory] Keya motor detected on CAN3");
            return MotorDriverType::KEYA_CAN;
        }
        
        // Default to DRV8701 if no CAN motor detected
        Serial.print("\r\n[MotorFactory] No CAN motor detected, defaulting to DRV8701");
        return MotorDriverType::DRV8701;
    }
};

#endif // MOTOR_DRIVER_FACTORY_H