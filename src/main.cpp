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

// Test mode flag - set to true to run motor tests
static bool MOTOR_TEST_MODE = false;  // Disable for autosteer mode

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

  // Initialize HardwareManager
  hardwarePTR = new HardwareManager();
  if (hardwarePTR->initializeHardware())
  {
    Serial.print("\r\n- HardwareManager initialized");
    
    // Quick buzzer beep to indicate hardware is ready
    hardwarePTR->enableBuzzer();
    delay(100);
    hardwarePTR->disableBuzzer();
  }
  else
  {
    Serial.print("\r\n✗ HardwareManager FAILED");
  }

  // Initialize I2CManager
  i2cPTR = new I2CManager();
  if (i2cPTR->initializeI2C())
  {
    Serial.print("\r\n- I2CManager initialized");
  }
  else
  {
    Serial.print("\r\n✗ I2CManager FAILED");
  }

  // Initialize LED Manager
  ledPTR = new LEDManager();
  if (ledPTR->init()) 
  {
    Serial.print("\r\n- LEDManager initialized");
    ledPTR->setBrightness(configPTR->getLEDBrightness());
  }
  else
  {
    Serial.print("\r\n✗ LEDManager FAILED");
  }

  // Initialize CANManager
  canPTR = new CANManager();
  if (canPTR->init())
  {
    Serial.print("\r\n- CANManager initialized");
  }
  else
  {
    Serial.print("\r\n✗ CANManager FAILED");
  }

  // Initialize SerialManager
  serialPTR = new SerialManager();
  if (serialPTR->initializeSerial())
  {
    Serial.print("\r\n- SerialManager initialized");
  }
  else
  {
    Serial.print("\r\n✗ SerialManager FAILED");
  }

  // Initialize GNSSProcessor
  gnssPTR = new GNSSProcessor();
  if (gnssPTR->setup(false, true)) // Disable debug, enable noise filter
  {
    Serial.print("\r\n- GNSSProcessor initialized");
  }
  else
  {
    Serial.print("\r\n✗ GNSSProcessor FAILED");
  }

  // Initialize IMUProcessor
  imuPTR = new IMUProcessor();
  if (imuPTR->initialize())
  {
    Serial.print("\r\n- IMUProcessor initialized");
    imuPTR->registerPGNCallbacks();
  }
  else
  {
    Serial.print("\r\n✗ IMUProcessor FAILED");
  }

  // Initialize ADProcessor
  adPTR = ADProcessor::getInstance();
  if (adPTR->init())
  {
    Serial.print("\r\n- ADProcessor initialized");
    adPTR->setWASOffset(1553);  // 2.5V center with 10k/10k voltage divider
    adPTR->setWASCountsPerDegree(30.0f);
    adPTR->process();
  }
  else
  {
    Serial.print("\r\n✗ ADProcessor FAILED");
  }
  
  // Initialize PWMProcessor
  pwmPTR = PWMProcessor::getInstance();
  if (pwmPTR->init())
  {
    Serial.print("\r\n- PWMProcessor initialized");
    pwmPTR->setSpeedPulseHz(10.0f);
    pwmPTR->setSpeedPulseDuty(0.5f);
    pwmPTR->enableSpeedPulse(true);
    pwmPTR->setPulsesPerMeter(130.0f);  // ISO 11786 standard
    pwmPTR->setSpeedKmh(10.0f);
  }
  else
  {
    Serial.print("\r\n✗ PWMProcessor FAILED");
  }

  // Initialize NAVProcessor
  NAVProcessor::init();
  navPTR = navPTR->getInstance();
  Serial.print("\r\n- NAVProcessor initialized");

  // Initialize Motor Driver
  MotorDriverType detectedType = MotorDriverFactory::detectMotorType(canPTR);
  motorPTR = MotorDriverFactory::createMotorDriver(detectedType, hardwarePTR, canPTR);
  
  if (motorPTR && motorPTR->init()) {
    Serial.print("\r\n- Motor driver initialized");
  } else {
    Serial.print("\r\n✗ Motor driver FAILED");
  }

  // Initialize AutosteerProcessor
  autosteerPTR = AutosteerProcessor::getInstance();
  if (autosteerPTR->init()) {
    Serial.print("\r\n- AutosteerProcessor initialized");
  } else {
    Serial.print("\r\n✗ AutosteerProcessor FAILED");
  }

  // Initialize MachineProcessor
  if (MachineProcessor::init()) {
    Serial.print("\r\n- MachineProcessor initialized");
  } else {
    Serial.print("\r\n✗ MachineProcessor FAILED");
  }

  // Initialize SubnetManager for PGN 201 handling
  if (SubnetManager::init()) {
    Serial.print("\r\n- SubnetManager initialized");
  } else {
    Serial.print("\r\n✗ SubnetManager FAILED");
  }

  // Motor Driver Testing
  if (MOTOR_TEST_MODE) {
    Serial.print("\r\n\n*** Motor Test Mode Active ***");
    
    // Auto-detect motor type
    MotorDriverType detectedType = MotorDriverFactory::detectMotorType(canPTR);
    
    // Create motor driver
    motorPTR = MotorDriverFactory::createMotorDriver(detectedType, hardwarePTR, canPTR);
    
    if (motorPTR) {
      if (motorPTR->init()) {
        Serial.print("\r\n- Motor driver initialized (Test Mode)");
      
        // Skip automatic test for Keya
        if (motorPTR->getType() != MotorDriverType::KEYA_CAN) {
          // Run automatic test for PWM motors only
          motorPTR->enable(true);
          delay(1000);
          
          motorPTR->setSpeed(25.0f);
          delay(2000);
          
          motorPTR->setSpeed(50.0f);
          delay(2000);
          
          motorPTR->stop();
          delay(1000);
          
          motorPTR->setSpeed(-25.0f);
          delay(2000);
          
          motorPTR->setSpeed(-50.0f);
          delay(2000);
          
          motorPTR->stop();
          motorPTR->enable(false);
        }
        
        Serial.print("\r\nCommands: e/d (enable/disable), +/- (speed), s (stop), ? (status)");
      } else {
        Serial.print("\r\n✗ Motor driver FAILED (Test Mode)");
      }
    }
  }

  Serial.print("\r\n\n=== System Ready ===\r\n");
  
}

void loop()
{
  mongoose_poll();

  static uint32_t lastPWMTest = 0;
  
  // Motor test mode variables
  static float motorTestSpeed = 0.0f;

  // Motor test mode interactive commands
  if (MOTOR_TEST_MODE && motorPTR) {
    // Check for serial commands
    if (Serial.available()) {
      char cmd = Serial.read();
      
      switch (cmd) {
        case '+':
          motorTestSpeed = constrain(motorTestSpeed + 10.0f, -100.0f, 100.0f);
          motorPTR->setSpeed(motorTestSpeed);
          Serial.printf("\r\n[Motor] Speed: %.1f%%", motorTestSpeed);
          break;
          
        case '*':  // Add a way to set higher speed for testing
          motorTestSpeed = 50.0f;  // 50% = 500 in Keya units
          motorPTR->setSpeed(motorTestSpeed);
          Serial.printf("\r\n[Motor] Speed set to: %.1f%%", motorTestSpeed);
          break;
          
        case '-':
          motorTestSpeed = constrain(motorTestSpeed - 10.0f, -100.0f, 100.0f);
          motorPTR->setSpeed(motorTestSpeed);
          Serial.printf("\r\n[Motor] Speed: %.1f%%", motorTestSpeed);
          break;
          
        case 'e':
        case 'E':
          motorPTR->enable(true);
          Serial.print("\r\n[Motor] ENABLED");
          break;
          
        case 'd':
        case 'D':
          motorPTR->enable(false);
          Serial.print("\r\n[Motor] DISABLED");
          break;
          
        case 's':
        case 'S':
          motorTestSpeed = 0.0f;
          motorPTR->stop();
          Serial.print("\r\n[Motor] STOPPED");
          break;
          
        case 'f':
        case 'F':
          motorTestSpeed = abs(motorTestSpeed);
          motorPTR->setSpeed(motorTestSpeed);
          Serial.printf("\r\n[Motor] Forward: %.1f%%", motorTestSpeed);
          break;
          
        case 'r':
        case 'R':
          motorTestSpeed = -abs(motorTestSpeed);
          motorPTR->setSpeed(motorTestSpeed);
          Serial.printf("\r\n[Motor] Reverse: %.1f%%", motorTestSpeed);
          break;
          
        case '?':
          MotorStatus status = motorPTR->getStatus();
          Serial.printf("\r\n[Motor Status]");
          Serial.printf("\r\n  Type: %s", motorPTR->getTypeName());
          Serial.printf("\r\n  Enabled: %s", status.enabled ? "YES" : "NO");
          Serial.printf("\r\n  Target: %.1f%%", status.targetSpeed);
          Serial.printf("\r\n  Actual: %.1f%%", status.actualSpeed);
          if (motorPTR->hasCurrentSensing()) {
            Serial.printf("\r\n  Current: %.2fA", status.currentDraw);
          }
          if (status.hasError) {
            Serial.printf("\r\n  ERROR: %s", status.errorMessage);
          }
          break;
      }
    }
    // Don't return - let motor process() run below
  }

  // Section diagnostics command handler (available in all modes)
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == 'd' || cmd == 'D') {
      Serial.print("\r\n\n*** Running Section Diagnostics ***");
      if (machinePTR) {
        machinePTR->runSectionDiagnostics();
      } else {
        Serial.print("\r\nERROR: MachineProcessor not initialized!");
      }
    }
  }
  
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
  if (autosteerPTR && !MOTOR_TEST_MODE)
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
    
    // Power/Ethernet LED
    // TODO: Add proper ethernet link detection when NetworkBase is updated
    bool ethernetUp = true;  // For now assume ethernet is up
    ledPTR->setPowerState(ethernetUp, navPTR && navPTR->hasAgIOConnection());
    
    // GPS LED
    if (gnssPTR) {
      ledPTR->setGPSState(gnssPTR->getData().fixQuality, gnssPTR->hasGPS());
    }
    
    
    // IMU/INS LED
    if (imuPTR && imuPTR->getIMUType() != IMUType::NONE) {
      // Separate IMU detected (BNO08x or TM171)
      ledPTR->setIMUState(true,
                         imuPTR->isIMUInitialized(),
                         imuPTR->hasValidData());
    } else if (gnssPTR && gnssPTR->getData().hasINS) {
      // UM981 INS system
      const auto& gpsData = gnssPTR->getData();
      bool insDetected = true;
      bool insInitialized = gpsData.insAlignmentStatus != 0;
      bool insValid = gpsData.insAlignmentStatus == 3; // Solution good
      
      // Special handling for aligning state - show as initialized but not valid
      if (gpsData.insAlignmentStatus == 7) { // INS_ALIGNING
        insInitialized = true;
        insValid = false;
      }
      
      ledPTR->setIMUState(insDetected, insInitialized, insValid);
    } else {
      // No IMU/INS detected
      ledPTR->setIMUState(false, false, false);
    }
    
    // Update LED hardware (handles blinking)
    ledPTR->update();
  }
  




  
  
  // Update PWM speed pulse - TEST MODE with artificial speed
  static uint32_t lastSpeedUpdate = 0;
  static bool useTestSpeed = false;  // Set to true for testing without GPS
  
  if (millis() - lastSpeedUpdate > 200)  // Update every 200ms like V6-NG
  {
    lastSpeedUpdate = millis();
    
    if (pwmPTR && pwmPTR->isSpeedPulseEnabled())
    {
      float speedKmh = 0.0f;
      
      if (useTestSpeed)
      {
        // TEST MODE: Cycle through speeds automatically
        static float testSpeed = 0.0f;
        static uint32_t lastTestChange = 0;
        
        // Change speed every 5 seconds
        if (millis() - lastTestChange > 5000)
        {
          lastTestChange = millis();
          testSpeed += 5.0f;
          if (testSpeed > 30.0f) testSpeed = 0.0f;
        }
        
        speedKmh = testSpeed;
        
        // Debug output every 3 seconds
        if (millis() - lastPWMTest > 3000)
        {
          lastPWMTest = millis();
          Serial.printf("\r\n[PWM] TEST Speed: %.1f km/h = %.1f Hz (130 ppm)", 
                        speedKmh, pwmPTR->getSpeedPulseHz());
        }
      }
      else if (gnssPTR)
      {
        // GPS MODE: Use actual GPS speed
        const auto &gpsData = gnssPTR->getData();
        if (gpsData.hasVelocity)
        {
          // Convert knots to km/h
          speedKmh = gpsData.speedKnots * 1.852f;
          
        }
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