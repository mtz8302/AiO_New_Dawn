// main.cpp - Updated section with motor driver testing
#include "Arduino.h"
#include <QNEthernet.h>
#include "QNetworkBase.h"
#include "AsyncUDPHandler.h"
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
#include "PGNProcessor.h"
#include "RTCMProcessor.h"

// Global objects (no more pointers)
ConfigManager configManager;
HardwareManager hardwareManager;
SerialManager serialManager;
I2CManager i2cManager;
CANManager canManager;
GNSSProcessor gnssProcessor;
IMUProcessor imuProcessor;
ADProcessor adProcessor;
PWMProcessor pwmProcessor;
LEDManager ledManager;
MotorDriverInterface *motorPTR = nullptr; // Motor driver still uses factory pattern

void setup()
{
  delay(5000); // delay for time to start monitor
  Serial.begin(115200);

  Serial.print("\r\n\n=== Teensy 4.1 AiO-NG-v6 New Dawn ===");
  Serial.print("\r\nInitializing subsystems...");

  // Network and communication setup FIRST
  QNetworkBase::init();
  
  // Give the network stack time to stabilize - match example timing
  delay(2000);  // 2 seconds for static IP as per AsyncUDP examples
  
  // Verify we have an IP address
  IPAddress localIP = Ethernet.localIP();
  if (localIP == INADDR_NONE) {
    Serial.print("\r\n- ERROR: No IP address assigned!");
  } else {
    Serial.printf("\r\n- IP ready: %d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
  }
  
  // Use AsyncUDP instead of traditional UDP
  AsyncUDPHandler::init();

  Serial.print("\r\n- Network stack initialized with AsyncUDP");
  
  // Initialize global CAN buses AFTER mongoose
  initializeGlobalCANBuses();

  // ConfigManager is already constructed
  Serial.print("\r\n- ConfigManager initialized");

  // Initialize EventLogger early so other modules can use it
  EventLogger::init();
  Serial.print("\r\n- EventLogger initialized (startup mode)");
  delay(10);  // Small delay to ensure EventLogger is fully initialized
  
  // Initialize PGNProcessor early (needed by many modules)
  PGNProcessor::init();
  LOG_INFO(EventSource::SYSTEM, "PGNProcessor initialized");
  
  // Initialize RTCMProcessor
  RTCMProcessor::init();
  LOG_INFO(EventSource::SYSTEM, "RTCMProcessor initialized");

  // Initialize HardwareManager
  if (hardwareManager.initializeHardware())
  {
    LOG_INFO(EventSource::SYSTEM, "HardwareManager initialized");
    
    // Quick buzzer beep to indicate hardware is ready
    hardwareManager.enableBuzzer();
    delay(100);
    hardwareManager.disableBuzzer();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "HardwareManager FAILED");
  }

  // Initialize I2CManager
  if (i2cManager.initializeI2C())
  {
    LOG_INFO(EventSource::SYSTEM, "I2CManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "I2CManager FAILED");
  }

  // Initialize LED Manager
  if (ledManager.init()) 
  {
    LOG_INFO(EventSource::SYSTEM, "LEDManager initialized");
    ledManager.setBrightness(configManager.getLEDBrightness());
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "LEDManager FAILED");
  }

  // Initialize CANManager
  if (canManager.init())
  {
    LOG_INFO(EventSource::SYSTEM, "CANManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "CANManager FAILED");
  }

  // Initialize SerialManager
  if (serialManager.initializeSerial())
  {
    LOG_INFO(EventSource::SYSTEM, "SerialManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "SerialManager FAILED");
  }

  // Initialize GNSSProcessor
  if (gnssProcessor.setup(false, true)) // Disable debug, enable noise filter
  {
    LOG_INFO(EventSource::SYSTEM, "GNSSProcessor initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "GNSSProcessor FAILED");
  }

  // Initialize IMUProcessor
  if (imuProcessor.initialize())
  {
    LOG_INFO(EventSource::SYSTEM, "IMUProcessor initialized");
    imuProcessor.registerPGNCallbacks();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "IMUProcessor FAILED");
  }

  // Initialize ADProcessor
  if (adProcessor.init())
  {
    LOG_INFO(EventSource::SYSTEM, "ADProcessor initialized");
    adProcessor.setWASOffset(1553);  // 2.5V center with 10k/10k voltage divider
    adProcessor.setWASCountsPerDegree(30.0f);
    adProcessor.process();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "ADProcessor FAILED");
  }
  
  // Initialize PWMProcessor
  if (pwmProcessor.init())
  {
    LOG_INFO(EventSource::SYSTEM, "PWMProcessor initialized");
    pwmProcessor.setSpeedPulseHz(10.0f);
    pwmProcessor.setSpeedPulseDuty(0.5f);
    pwmProcessor.enableSpeedPulse(true);
    pwmProcessor.setPulsesPerMeter(130.0f);  // ISO 11786 standard
    pwmProcessor.setSpeedKmh(10.0f);
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "PWMProcessor FAILED");
  }

  // Initialize NAVProcessor
  NAVProcessor::init();
  LOG_INFO(EventSource::SYSTEM, "NAVProcessor initialized");

  // Initialize Motor Driver
  MotorDriverType detectedType = MotorDriverFactory::detectMotorType(&canManager);
  motorPTR = MotorDriverFactory::createMotorDriver(detectedType, &hardwareManager, &canManager);
  
  if (motorPTR && motorPTR->init()) {
    LOG_INFO(EventSource::SYSTEM, "Motor driver initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "Motor driver FAILED");
  }

  // Initialize AutosteerProcessor
  AutosteerProcessor* autosteerPTR = AutosteerProcessor::getInstance();
  if (autosteerPTR->init()) {
    LOG_INFO(EventSource::SYSTEM, "AutosteerProcessor initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "AutosteerProcessor FAILED");
  }

  // Initialize MachineProcessor
  MachineProcessor* machinePTR = MachineProcessor::getInstance();
  if (machinePTR->initialize()) {
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
  cmdHandler->setConfigManager(&configManager);
  cmdHandler->setMachineProcessor(machinePTR);
  LOG_INFO(EventSource::SYSTEM, "CommandHandler initialized");


  // Exit startup mode - start enforcing configured log levels
  EventLogger::getInstance()->setStartupMode(false);
  
  LOG_INFO(EventSource::SYSTEM, "=== System Ready ===");
}

void loop()
{
  // Process Ethernet events - REQUIRED for QNEthernet!
  Ethernet.loop();
  
  QNetworkBase::poll();
  
  // AsyncUDP handles all UDP packet reception via callbacks
  // No need for polling - just periodic status update
  static uint32_t lastNetworkStatus = 0;
  if (millis() - lastNetworkStatus > 5000) {
    lastNetworkStatus = millis();
    IPAddress localIP = Ethernet.localIP();
    LOG_DEBUG(EventSource::NETWORK, "Network status: IP=%d.%d.%d.%d, Link=%s", 
              localIP[0], localIP[1], localIP[2], localIP[3],
              Ethernet.linkStatus() ? "UP" : "DOWN");
  }
  
  // Check network status and display system ready message when appropriate
  static uint32_t lastNetworkCheck = 0;
  if (millis() - lastNetworkCheck > 100) {  // Check every 100ms for responsiveness
    lastNetworkCheck = millis();
    EventLogger::getInstance()->checkNetworkReady();
  }

  // Process serial commands through CommandHandler
  CommandHandler::getInstance()->process();
  
  // Process IMU data
  imuProcessor.process();
  
  // Process A/D inputs
  adProcessor.process();

  // Process NAV messages
  NAVProcessor::getInstance()->process();

  // Poll CAN messages (like NG-V6 does)
  canManager.pollForDevices();
  
  // Process motor driver (must be called regularly for CAN motors)
  if (motorPTR)
  {
    motorPTR->process();
  }
  
  // Process autosteer
  AutosteerProcessor::getInstance()->process();
  
  // Process machine
  MachineProcessor::getInstance()->process();
  
  // Update LEDs
  static uint32_t lastLEDUpdate = 0;
  if (millis() - lastLEDUpdate > 100)  // Update every 100ms
  {
    lastLEDUpdate = millis();
    ledManager.updateAll();
  }
  




  
  
  // Update PWM speed pulse from GPS
  static uint32_t lastSpeedUpdate = 0;
  
  if (millis() - lastSpeedUpdate > 200)  // Update every 200ms like V6-NG
  {
    lastSpeedUpdate = millis();
    
    if (pwmProcessor.isSpeedPulseEnabled())
    {
      float speedKmh = 0.0f;
      
      // Use actual GPS speed
      const auto &gpsData = gnssProcessor.getData();
      if (gpsData.hasVelocity)
      {
        // Convert knots to km/h
        speedKmh = gpsData.speedKnots * 1.852f;
      }
      
      // Set the speed
      pwmProcessor.setSpeedKmh(speedKmh);
    }
  }


  // Process GPS1 data if available
  while (SerialGPS1.available())
  {
    char c = SerialGPS1.read();
    gnssProcessor.processNMEAChar(c);
  }
  
  // Process GPS2 data if available (for F9P dual RELPOSNED)
  if (SerialGPS2.available())
  {
    uint8_t b = SerialGPS2.read();
    gnssProcessor.processUBXByte(b);
  }

}