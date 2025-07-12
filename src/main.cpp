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
#include "WebManager.h"
#include "Version.h"
#include "OTAHandler.h"

// Flash ID for OTA verification - must match FLASH_ID in FlashTxx.h
const char* flash_id = "fw_teensy41";

// Global objects (no more pointers)
ConfigManager configManager;
HardwareManager hardwareManager;
SerialManager serialManager;
I2CManager i2cManager;
CANManager canManager;
GNSSProcessor gnssProcessor;
GNSSProcessor* gnssProcessorPtr = &gnssProcessor;  // Global pointer for external access
IMUProcessor imuProcessor;
ADProcessor adProcessor;
PWMProcessor pwmProcessor;
LEDManager ledManager;
WebManager webManager;
MotorDriverInterface *motorPTR = nullptr; // Motor driver still uses factory pattern

void setup()
{
  delay(5000); // delay for time to start monitor
  Serial.begin(115200);

  Serial.print("\r\n\n=== Teensy 4.1 AiO-NG-v6 New Dawn v");
  Serial.print(FIRMWARE_VERSION);
  Serial.print(" ===\r\n");
  Serial.print("Initializing subsystems...");

  // Network and communication setup FIRST
  QNetworkBase::init();
  
  // Wait for network speed to stabilize (some switches negotiate in steps)
  Serial.print("\r\n- Waiting for network speed negotiation...");
  uint32_t startWait = millis();
  int lastSpeed = 0;
  
  // Wait up to 10 seconds for link speed to stabilize
  while (millis() - startWait < 10000) {
    int currentSpeed = Ethernet.linkSpeed();
    if (currentSpeed != lastSpeed) {
      Serial.printf("\r\n  Link speed changed: %d Mbps\r\n", currentSpeed);
      lastSpeed = currentSpeed;
      startWait = millis(); // Reset timer on speed change
    }
    
    // If we've been stable at 100Mbps for 2 seconds, we're good
    if (currentSpeed == 100 && millis() - startWait > 2000) {
      Serial.print("\r\n  Link stable at 100 Mbps\r\n");
      break;
    }
    
    delay(100);
  }
  
  // Additional delay for network stack to stabilize
  Serial.print("\r\n- Waiting for network stack to stabilize...");
  delay(2000);
  
  // Verify we have an IP address
  IPAddress localIP = Ethernet.localIP();
  if (localIP == IPAddress(0, 0, 0, 0)) {
    Serial.print("\r\n- ERROR: No IP address assigned!\r\n");
  } else {
    Serial.printf("\r\n- IP ready: %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
    Serial.printf("\r\n- Final link speed: %d Mbps\r\n", Ethernet.linkSpeed());
  }
  
  // Network stack is ready but don't initialize AsyncUDP yet
  Serial.print("\r\n- Network stack initialized\r\n");
  
  // Initialize global CAN buses
  initializeGlobalCANBuses();

  // ConfigManager is already constructed
  Serial.print("\r\n- ConfigManager initialized\r\n");

  // Initialize EventLogger early so other modules can use it
  EventLogger::init();
  Serial.print("\r\n- EventLogger initialized (startup mode)\r\n");
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

  // Initialize CANManager first (more critical than I2C)
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

  // Add a delay before I2C to let network/interrupts stabilize
  delay(100);
  
  // Initialize I2CManager later in sequence after critical systems
  if (i2cManager.initializeI2C())
  {
    LOG_INFO(EventSource::SYSTEM, "I2CManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "I2CManager FAILED");
  }

  // Initialize LED Manager (needs I2C)
  if (ledManager.init()) 
  {
    LOG_INFO(EventSource::SYSTEM, "LEDManager initialized");
    ledManager.setBrightness(configManager.getLEDBrightness());
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "LEDManager FAILED");
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

  // NOW initialize AsyncUDP after ALL hardware is up
  LOG_INFO(EventSource::SYSTEM, "All hardware initialized, starting AsyncUDP");
  AsyncUDPHandler::init();
  LOG_INFO(EventSource::SYSTEM, "AsyncUDP handlers ready");

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
  cmdHandler->setMachineProcessor(machinePTR);
  LOG_INFO(EventSource::SYSTEM, "CommandHandler initialized");

  // Initialize WebManager
  if (webManager.begin()) {
    LOG_INFO(EventSource::SYSTEM, "WebManager initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "WebManager FAILED");
  }

  // Exit startup mode - start enforcing configured log levels
  EventLogger::getInstance()->setStartupMode(false);
  
  // Mark web manager as ready for SSE updates
  webManager.setSystemReady();
  
  // Display access information
  localIP = Ethernet.localIP();  // Reuse existing variable
  Serial.println("\r\n");
  Serial.println("========================================");
  Serial.println("=== AiO New Dawn - System Ready ===");
  Serial.println("========================================");
  Serial.printf("IP Address: %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
  Serial.println("Web Interface: http://192.168.5.126");
  Serial.println("DHCP Server: Enabled");
  Serial.println("========================================");
  Serial.println();
  
  LOG_INFO(EventSource::SYSTEM, "=== System Ready ===");
}

void loop()
{
  // Timing variables
  static uint32_t loopStartTime = 0;
  static uint32_t functionTime = 0;
  static uint32_t lastTimingPrint = 0;
  static uint32_t loopCount = 0;
  
  // Timing accumulators for averaging
  static uint32_t otaTime = 0;
  static uint32_t ethernetTime = 0;
  static uint32_t networkTime = 0;
  static uint32_t udpTime = 0;
  
  loopStartTime = micros();
  loopCount++;
  
  // Check for OTA update apply
  functionTime = micros();
  OTAHandler::applyUpdate();
  otaTime += micros() - functionTime;
  
  // Process Ethernet events - REQUIRED for QNEthernet!
  functionTime = micros();
  Ethernet.loop();
  ethernetTime += micros() - functionTime;
  
  functionTime = micros();
  QNetworkBase::poll();
  networkTime += micros() - functionTime;
  
  // Poll AsyncUDP for network diagnostics
  functionTime = micros();
  AsyncUDPHandler::poll();
  udpTime += micros() - functionTime;
  
  // AsyncUDP handles all UDP packet reception via callbacks
  // The poll() call above is just for diagnostics and status monitoring
  
  // More timing accumulators
  static uint32_t commandTime = 0;
  static uint32_t imuTime = 0;
  static uint32_t adcTime = 0;
  static uint32_t navTime = 0;
  static uint32_t canTime = 0;
  static uint32_t motorTime = 0;
  static uint32_t autosteerTime = 0;
  static uint32_t machineTime = 0;
  static uint32_t ledTime = 0;
  static uint32_t sseTime = 0;
  static uint32_t gpsTime = 0;
  
  // Check network status and display system ready message when appropriate
  static uint32_t lastNetworkCheck = 0;
  if (millis() - lastNetworkCheck > 100) {  // Check every 100ms for responsiveness
    lastNetworkCheck = millis();
    EventLogger::getInstance()->checkNetworkReady();
  }

  // Process serial commands through CommandHandler
  functionTime = micros();
  CommandHandler::getInstance()->process();
  commandTime += micros() - functionTime;
  
  // Process IMU data
  functionTime = micros();
  imuProcessor.process();
  imuTime += micros() - functionTime;
  
  // Process A/D inputs
  functionTime = micros();
  adProcessor.process();
  adcTime += micros() - functionTime;

  // Process NAV messages
  functionTime = micros();
  NAVProcessor::getInstance()->process();
  navTime += micros() - functionTime;

  // Poll CAN messages (like NG-V6 does)
  functionTime = micros();
  canManager.pollForDevices();
  canTime += micros() - functionTime;
  
  // Process motor driver (must be called regularly for CAN motors)
  functionTime = micros();
  if (motorPTR)
  {
    motorPTR->process();
  }
  motorTime += micros() - functionTime;
  
  // Debug: Print motor driver type once
  static bool motorTypeLogged = false;
  if (motorPTR && !motorTypeLogged) {
    motorTypeLogged = true;
    Serial.println("Motor driver is active");
  }
  
  // Process autosteer
  functionTime = micros();
  AutosteerProcessor::getInstance()->process();
  autosteerTime += micros() - functionTime;
  
  // Process machine
  functionTime = micros();
  MachineProcessor::getInstance()->process();
  machineTime += micros() - functionTime;
  
  // Update LEDs
  static uint32_t lastLEDUpdate = 0;
  functionTime = micros();
  if (millis() - lastLEDUpdate > 100)  // Update every 100ms
  {
    lastLEDUpdate = millis();
    ledManager.updateAll();
  }
  ledTime += micros() - functionTime;
  
  // Update SSE clients with WAS data if enabled
  functionTime = micros();
  webManager.updateWASClients();
  sseTime += micros() - functionTime;
  




  
  
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
  functionTime = micros();
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
  gpsTime += micros() - functionTime;

  // Print timing report every second
  if (millis() - lastTimingPrint >= 1000) {
    Serial.println("\n=== Loop Timing Report (microseconds) ===");
    Serial.printf("Loops: %lu, Avg loop time: %lu us\n", loopCount, 1000000UL / loopCount);
    Serial.printf("OTA:       %lu us\n", otaTime / loopCount);
    Serial.printf("Ethernet:  %lu us\n", ethernetTime / loopCount);
    Serial.printf("Network:   %lu us\n", networkTime / loopCount);
    Serial.printf("UDP:       %lu us\n", udpTime / loopCount);
    Serial.printf("Command:   %lu us\n", commandTime / loopCount);
    Serial.printf("IMU:       %lu us\n", imuTime / loopCount);
    Serial.printf("ADC:       %lu us\n", adcTime / loopCount);
    Serial.printf("NAV:       %lu us\n", navTime / loopCount);
    Serial.printf("CAN:       %lu us\n", canTime / loopCount);
    Serial.printf("Motor:     %lu us\n", motorTime / loopCount);
    Serial.printf("Autosteer: %lu us\n", autosteerTime / loopCount);
    Serial.printf("Machine:   %lu us\n", machineTime / loopCount);
    Serial.printf("LED:       %lu us\n", ledTime / loopCount);
    Serial.printf("SSE:       %lu us\n", sseTime / loopCount);
    Serial.printf("GPS:       %lu us\n", gpsTime / loopCount);
    
    uint32_t totalTime = (otaTime + ethernetTime + networkTime + udpTime + commandTime + 
                         imuTime + adcTime + navTime + canTime + motorTime + autosteerTime + 
                         machineTime + ledTime + sseTime + gpsTime) / loopCount;
    Serial.printf("Total:     %lu us\n", totalTime);
    Serial.println("========================================");
    
    // Reset counters
    lastTimingPrint = millis();
    loopCount = 0;
    otaTime = ethernetTime = networkTime = udpTime = commandTime = 0;
    imuTime = adcTime = navTime = canTime = motorTime = 0;
    autosteerTime = machineTime = ledTime = sseTime = gpsTime = 0;
  }
}