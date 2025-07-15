// MotorDriverFactory.h - Factory for creating motor drivers
#ifndef MOTOR_DRIVER_FACTORY_H
#define MOTOR_DRIVER_FACTORY_H

#include "MotorDriverInterface.h"
#include "PWMMotorDriver.h"
#include "KeyaCANDriver.h"
#include "MotorDriverDetector.h"
#include "HardwareManager.h"
#include "CANManager.h"
#include "EventLogger.h"

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
                    hwMgr->getSleepPin(),      // Enable pin (SLEEP_PIN on DRV8701, also LOCK output)
                    hwMgr->getCurrentPin()     // Current sense pin
                );
                
            case MotorDriverType::KEYA_CAN:
                return new KeyaCANDriver();
                
            case MotorDriverType::DANFOSS:
                LOG_WARNING(EventSource::AUTOSTEER, "Danfoss driver not yet implemented - using DRV8701");
                // TODO: Return DanfossMotorDriver when implemented
                return new PWMMotorDriver(
                    MotorDriverType::DRV8701,
                    hwMgr->getPWM1Pin(),
                    hwMgr->getPWM2Pin(),
                    hwMgr->getSleepPin(),
                    hwMgr->getCurrentPin()
                );
                
            default:
                LOG_WARNING(EventSource::AUTOSTEER, "Unknown motor type");
                return nullptr;
        }
    }
    
    // Auto-detect motor type
    static MotorDriverType detectMotorType(CANManager* canMgr) {
        // Initialize the detector
        MotorDriverDetector* detector = MotorDriverDetector::getInstance();
        detector->init();
        
        // Wait for detection to complete (up to 2 seconds for Keya)
        uint32_t startTime = millis();
        while (!detector->isDetectionComplete() && (millis() - startTime) < 2100) {
            // Check for Keya heartbeat
            bool keyaDetected = canMgr && canMgr->isKeyaDetected();
            detector->detect(keyaDetected);
            delay(10);
        }
        
        // Force detection completion if timeout
        if (!detector->isDetectionComplete()) {
            detector->detect(false);
        }
        
        return detector->getDetectedType();
    }
};

#endif // MOTOR_DRIVER_FACTORY_H