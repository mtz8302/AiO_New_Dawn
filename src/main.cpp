// main.cpp - Updated section with motor driver testing
#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"
#include "ConfigManager.h"
#include "HardwareManager.h"
#include "SerialManager.h"
#include "SerialGlobals.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h" // Add this include
#include "NAVProcessor.h"
#include "I2CManager.h"
#include "CANManager.h"
#include "ADProcessor.h"
#include "PWMProcessor.h"
#include "MotorDriverInterface.h"
#include "MotorDriverFactory.h"
#include "CANGlobals.h"
#include "AutosteerProcessor.h"
#include "KeyaCANDriver.h"
#include "LEDManager.h"
#include "MachineProcessor.h"
#include "SubnetManager.h"
#include "EventLogger.h"
#include "CommandHandler.h"

// ConfigManager pointer definition (same pattern as machinePTR)
// This is the ONLY definition - all other files use extern declaration
ConfigManager *configPTR = nullptr;

// Define the global pointers (only declared as extern in headers)
HardwareManager *hardwarePTR = nullptr;
SerialManager *serialPTR = nullptr;
I2CManager *i2cPTR = nullptr;
CANManager *canPTR = nullptr;
GNSSProcessor *gnssPTR = nullptr;
IMUProcessor *imuPTR = nullptr;
NAVProcessor *navPTR = nullptr;
ADProcessor *adPTR = nullptr;
PWMProcessor *pwmPTR = nullptr;
MotorDriverInterface *motorPTR = nullptr;

void setup()
{
  delay(5000); // delay for time to start monitor
  Serial.begin(115200);

  Serial.print("\r\n\n=== Teensy 4.1 AiO-NG-v6 New Dawn ===");
  Serial.print("\r\nInitializing subsystems...");

  // Network and communication setup FIRST
  storedCfgSetup();
  ethernet_init();
  mongoose_init();
  udpSetup();

  Serial.print("\r\n- Network stack initialized");
  
  // Initialize global CAN buses AFTER mongoose
  initializeGlobalCANBuses();

  // Initialize ConfigManager (already implemented)
  configPTR = new ConfigManager();
  Serial.print("\r\n- ConfigManager initialized");

  // Initialize EventLogger early so other modules can use it
  EventLogger::init();
  Serial.print("\r\n- EventLogger initialized (startup mode)");
  delay(10);  // Small delay to ensure EventLogger is fully initialized

  // Initialize HardwareManager
  hardwarePTR = new HardwareManager();
  if (hardwarePTR->initializeHardware())
  {
    LOG_INFO(EventSource::SYSTEM, "HardwareManager initialized");
    
    // Quick buzzer beep to indicate hardware is ready
    hardwarePTR->enableBuzzer();
    delay(100);
    hardwarePTR->disableBuzzer();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "HardwareManager FAILED");
  }

  // Initialize I2CManager
  i2cPTR = new I2CManager();
  if (i2cPTR->initializeI2C())
  {
    LOG_INFO(EventSource::SYSTEM, "I2CManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "I2CManager FAILED");
  }

  // Initialize LED Manager
  ledPTR = new LEDManager();
  if (ledPTR->init()) 
  {
    LOG_INFO(EventSource::SYSTEM, "LEDManager initialized");
    ledPTR->setBrightness(configPTR->getLEDBrightness());
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "LEDManager FAILED");
  }

  // Initialize CANManager
  canPTR = new CANManager();
  if (canPTR->init())
  {
    LOG_INFO(EventSource::SYSTEM, "CANManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "CANManager FAILED");
  }

  // Initialize SerialManager
  serialPTR = new SerialManager();
  if (serialPTR->initializeSerial())
  {
    LOG_INFO(EventSource::SYSTEM, "SerialManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "SerialManager FAILED");
  }

  // Initialize GNSSProcessor
  gnssPTR = new GNSSProcessor();
  if (gnssPTR->setup(false, true)) // Disable debug, enable noise filter
  {
    LOG_INFO(EventSource::SYSTEM, "GNSSProcessor initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "GNSSProcessor FAILED");
  }

  // Initialize IMUProcessor
  imuPTR = new IMUProcessor();
  if (imuPTR->initialize())
  {
    LOG_INFO(EventSource::SYSTEM, "IMUProcessor initialized");
    imuPTR->registerPGNCallbacks();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "IMUProcessor FAILED");
  }

  // Initialize ADProcessor
  adPTR = ADProcessor::getInstance();
  if (adPTR->init())
  {
    LOG_INFO(EventSource::SYSTEM, "ADProcessor initialized");
    adPTR->setWASOffset(1553);  // 2.5V center with 10k/10k voltage divider
    adPTR->setWASCountsPerDegree(30.0f);
    adPTR->process();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "ADProcessor FAILED");
  }
  
  // Initialize PWMProcessor
  pwmPTR = PWMProcessor::getInstance();
  if (pwmPTR->init())
  {
    LOG_INFO(EventSource::SYSTEM, "PWMProcessor initialized");
    pwmPTR->setSpeedPulseHz(10.0f);
    pwmPTR->setSpeedPulseDuty(0.5f);
    pwmPTR->enableSpeedPulse(true);
    pwmPTR->setPulsesPerMeter(130.0f);  // ISO 11786 standard
    pwmPTR->setSpeedKmh(10.0f);
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "PWMProcessor FAILED");
  }

  // Initialize NAVProcessor
  NAVProcessor::init();
  navPTR = navPTR->getInstance();
  LOG_INFO(EventSource::SYSTEM, "NAVProcessor initialized");

  // Initialize Motor Driver
  MotorDriverType detectedType = MotorDriverFactory::detectMotorType(canPTR);
  motorPTR = MotorDriverFactory::createMotorDriver(detectedType, hardwarePTR, canPTR);
  
  if (motorPTR && motorPTR->init()) {
    LOG_INFO(EventSource::SYSTEM, "Motor driver initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "Motor driver FAILED");
  }

  // Initialize AutosteerProcessor
  autosteerPTR = AutosteerProcessor::getInstance();
  if (autosteerPTR->init()) {
    LOG_INFO(EventSource::SYSTEM, "AutosteerProcessor initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "AutosteerProcessor FAILED");
  }

  // Initialize MachineProcessor
  if (MachineProcessor::init()) {
    LOG_INFO(EventSource::SYSTEM, "MachineProcessor initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "MachineProcessor FAILED");
  }

  // Initialize SubnetManager for PGN 201 handling
  if (SubnetManager::init()) {
    LOG_INFO(EventSource::SYSTEM, "SubnetManager initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "SubnetManager FAILED");
  }

  // Initialize CommandHandler
  CommandHandler::init();
  CommandHandler* cmdHandler = CommandHandler::getInstance();
  cmdHandler->setConfigManager(configPTR);
  cmdHandler->setMachineProcessor(MachineProcessor::getInstance());
  LOG_INFO(EventSource::SYSTEM, "CommandHandler initialized");


  // Exit startup mode - start enforcing configured log levels
  EventLogger::getInstance()->setStartupMode(false);
  
  LOG_INFO(EventSource::SYSTEM, "=== System Ready ===");
}

void loop()
{
  mongoose_poll();
  
  // Check network status and display system ready message when appropriate
  static uint32_t lastNetworkCheck = 0;
  if (millis() - lastNetworkCheck > 100) {  // Check every 100ms for responsiveness
    lastNetworkCheck = millis();
    EventLogger::getInstance()->checkNetworkReady();
  }

  // Process serial commands through CommandHandler
  CommandHandler::getInstance()->process();
  
  // Process IMU data
  if (imuPTR)
  {
    imuPTR->process();
  }
  
  // Process A/D inputs
  if (adPTR)
  {
    adPTR->process();
  }

  // Process NAV messages
  if (navPTR)
  {
    navPTR->process();
  }

  // Poll CAN messages (like NG-V6 does)
  if (canPTR)
  {
    canPTR->pollForDevices();
  }
  
  // Process motor driver (must be called regularly for CAN motors)
  if (motorPTR)
  {
    motorPTR->process();
  }
  
  // Process autosteer
  if (autosteerPTR)
  {
    autosteerPTR->process();
  }
  
  // Process machine
  if (machinePTR)
  {
    machinePTR->process();
  }
  
  // Update LEDs
  static uint32_t lastLEDUpdate = 0;
  if (ledPTR && millis() - lastLEDUpdate > 100)  // Update every 100ms
  {
    lastLEDUpdate = millis();
    ledPTR->updateAll();
  }
  




  
  
  // Update PWM speed pulse from GPS
  static uint32_t lastSpeedUpdate = 0;
  
  if (millis() - lastSpeedUpdate > 200)  // Update every 200ms like V6-NG
  {
    lastSpeedUpdate = millis();
    
    if (pwmPTR && pwmPTR->isSpeedPulseEnabled() && gnssPTR)
    {
      float speedKmh = 0.0f;
      
      // Use actual GPS speed
      const auto &gpsData = gnssPTR->getData();
      if (gpsData.hasVelocity)
      {
        // Convert knots to km/h
        speedKmh = gpsData.speedKnots * 1.852f;
      }
      
      // Set the speed
      pwmPTR->setSpeedKmh(speedKmh);
    }
  }


  // Process GPS1 data if available
  while (SerialGPS1.available())
  {
    char c = SerialGPS1.read();
    gnssPTR->processNMEAChar(c);
  }
  
  // Process GPS2 data if available (for F9P dual RELPOSNED)
  if (SerialGPS2.available())
  {
    uint8_t b = SerialGPS2.read();
    gnssPTR->processUBXByte(b);
  }

}