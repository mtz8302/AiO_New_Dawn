// MotorDriverFactory.h - Factory for creating motor drivers
#ifndef MOTOR_DRIVER_FACTORY_H
#define MOTOR_DRIVER_FACTORY_H

#include "MotorDriverInterface.h"
#include "PWMMotorDriver.h"
#include "KeyaCANDriver.h"
#include "DanfossMotorDriver.h"
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
                LOG_INFO(EventSource::AUTOSTEER, "Creating Danfoss valve driver");
                return new DanfossMotorDriver(hwMgr);
                
            default:
                LOG_WARNING(EventSource::AUTOSTEER, "Unknown motor type");
                return nullptr;
        }
    }
    
    // Auto-detect motor type
    static MotorDriverType detectMotorType(CANManager* canMgr) {
        LOG_INFO(EventSource::AUTOSTEER, "Starting motor driver detection...");
        
        // Initialize the detector
        MotorDriverDetector* detector = MotorDriverDetector::getInstance();
        detector->init();
        
        // Wait for detection to complete (up to 2 seconds for Keya)
        uint32_t startTime = millis();
        bool keyaChecked = false;
        
        while (!detector->isDetectionComplete() && (millis() - startTime) < 2100) {
            // Check for Keya heartbeat
            bool keyaDetected = canMgr && canMgr->isKeyaDetected();
            if (keyaDetected && !keyaChecked) {
                LOG_INFO(EventSource::AUTOSTEER, "Keya CAN heartbeat detected");
                keyaChecked = true;
            }
            detector->detect(keyaDetected);
            delay(10);
        }
        
        // Force detection completion if timeout
        if (!detector->isDetectionComplete()) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Detection timeout - using configured/default driver");
            detector->detect(false);
        }
        
        MotorDriverType detectedType = detector->getDetectedType();
        const char* driverName = "Unknown";
        
        switch (detectedType) {
            case MotorDriverType::KEYA_CAN:
                driverName = "Keya CAN Motor";
                break;
            case MotorDriverType::DANFOSS:
                driverName = "Danfoss Valve";
                break;
            case MotorDriverType::DRV8701:
                driverName = "DRV8701 PWM";
                break;
            default:
                break;
        }
        
        LOG_INFO(EventSource::AUTOSTEER, "Motor driver detected: %s", driverName);
        return detectedType;
    }
};

#endif // MOTOR_DRIVER_FACTORY_H