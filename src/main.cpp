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

  // Test HardwareManager
  Serial.print("\r\n\n*** Testing HardwareManager ***");
  hardwarePTR = new HardwareManager();
  if (hardwarePTR->initializeHardware())
  {
    Serial.print("\r\n✓ HardwareManager SUCCESS");

    // Test some pin access methods
    Serial.printf("\r\n  - WAS pin: A%i", hardwarePTR->getWASSensorPin() - A0);
    Serial.printf("\r\n  - PWM1 pin: %i", hardwarePTR->getPWM1Pin());
    Serial.printf("\r\n  - Buzzer pin: %i", hardwarePTR->getBuzzerPin());

    // Test buzzer control
    Serial.print("\r\n  - Testing buzzer: ON");
    hardwarePTR->enableBuzzer();
    delay(100);
    Serial.print(" -> OFF");
    hardwarePTR->disableBuzzer();

    // Print full status
    hardwarePTR->printHardwareStatus();
  }
  else
  {
    Serial.print("\r\n✗ HardwareManager FAILED");
  }

  // Test I2CManager
  Serial.print("\r\n\n*** Testing I2CManager ***");
  i2cPTR = new I2CManager();
  if (i2cPTR->initializeI2C())
  {
    Serial.print("\r\n✓ I2CManager SUCCESS");
    
    // Print full status
    i2cPTR->printI2CStatus();
  }
  else
  {
    Serial.print("\r\n✗ I2CManager FAILED");
  }

  // Initialize LED Manager
  Serial.print("\r\n\n*** Testing LEDManager ***");
  ledPTR = new LEDManager();
  if (ledPTR->init()) 
  {
    Serial.print("\r\n✓ LEDManager SUCCESS");
    
    // Load saved brightness from config
    ledPTR->setBrightness(configPTR->getLEDBrightness());
    Serial.printf("\r\n  - Brightness: %d%%", ledPTR->getBrightness());
    
    // Optional: Run LED test
    // ledPTR->testLEDs();
  }
  else
  {
    Serial.print("\r\n✗ LEDManager FAILED");
  }

  // Test CANManager with global instances
  Serial.print("\r\n\n*** Testing CANManager ***");
  canPTR = new CANManager();
  if (canPTR->init())
  {
    Serial.print("\r\n✓ CANManager SUCCESS");
    
    // Test some basic functionality
    Serial.printf("\r\n  - CAN1 active: %s", canPTR->isCAN1Active() ? "YES" : "NO");
    Serial.printf("\r\n  - CAN2 active: %s", canPTR->isCAN2Active() ? "YES" : "NO");
    Serial.printf("\r\n  - CAN3 active: %s", canPTR->isCAN3Active() ? "YES" : "NO");
    
    // Simple status for new CANManager
    Serial.printf("\r\n  - Keya detected: %s", canPTR->isKeyaDetected() ? "YES" : "NO");
  }
  else
  {
    Serial.print("\r\n✗ CANManager FAILED");
  }

  // Test SerialManager
  Serial.print("\r\n\n*** Testing SerialManager ***");
  serialPTR = new SerialManager();
  if (serialPTR->initializeSerial())
  {
    Serial.print("\r\n✓ SerialManager SUCCESS");

    // Test baud rate access
    Serial.printf("\r\n  - GPS baud: %i", serialPTR->getGPSBaudRate());
    Serial.printf("\r\n  - RTK baud: %i", serialPTR->getRTKBaudRate());
    Serial.printf("\r\n  - ESP32 baud: %i", serialPTR->getESP32BaudRate());

    // Print full status
    serialPTR->printSerialStatus();
  }
  else
  {
    Serial.print("\r\n✗ SerialManager FAILED");
  }

  // Test GNSSProcessor
  Serial.print("\r\n\n*** Testing GNSSProcessor ***");
  gnssPTR = new GNSSProcessor();
  if (gnssPTR->setup(false, true)) // Disable debug, enable noise filter
  {
    Serial.print("\r\n✓ GNSSProcessor SUCCESS");
    Serial.print("\r\n  - Debug enabled: NO");
    Serial.print("\r\n  - Noise filter: YES");
    Serial.print("\r\n  - Ready for NMEA data");

    // Print initial stats
    gnssPTR->printStats();
    
    // PGN callbacks handled automatically - no registration needed
  }
  else
  {
    Serial.print("\r\n✗ GNSSProcessor FAILED");
  }

  // Test IMUProcessor
  Serial.print("\r\n\n*** Testing IMUProcessor ***");
  imuPTR = new IMUProcessor();

  // Initialize the IMU
  if (imuPTR->initialize())
  {
    Serial.print("\r\n✓ IMUProcessor SUCCESS");
    Serial.printf("\r\n  - IMU Type: %s", imuPTR->getIMUTypeName());

    if (imuPTR->getIMUType() == IMUType::TM171)
    {
      Serial.print("\r\n  - TM171 detected - waiting for angle data...");
      Serial.print("\r\n  - Note: TM171 TX/RX silkscreen labels are reversed!");
    }
    
    // Register PGN callbacks
    imuPTR->registerPGNCallbacks();
  }
  else
  {
    Serial.print("\r\n✗ IMUProcessor - No IMU detected");
    Serial.print("\r\n  - Check wiring and power");
    Serial.print("\r\n  - For TM171: TX on Teensy -> RX on TM171 (reversed labels!)");
    // Don't register PGN callbacks when no IMU detected
  }

  // Test ADProcessor
  Serial.print("\r\n\n*** Testing ADProcessor ***");
  adPTR = ADProcessor::getInstance();
  if (adPTR->init())
  {
    Serial.print("\r\n✓ ADProcessor SUCCESS");
    
    // Print initial status
    adPTR->printStatus();
    
    // Test configuration
    // With 10k/10k divider: 2.5V sensor -> 1.25V ADC
    // 1.25V / 3.3V * 4095 = 1553
    Serial.print("\r\n  - Setting WAS offset to 1553 (2.5V center)");
    adPTR->setWASOffset(1553);  // 2.5V center with 10k/10k voltage divider
    Serial.print("\r\n  - Setting counts per degree to 30");
    adPTR->setWASCountsPerDegree(30.0f);
    
    // Take a reading
    adPTR->process();
    Serial.printf("\r\n  - Current angle: %.2f°", adPTR->getWASAngle());
  }
  else
  {
    Serial.print("\r\n✗ ADProcessor FAILED");
  }
  
  // Test PWMProcessor
  Serial.print("\r\n\n*** Testing PWMProcessor ***");
  pwmPTR = PWMProcessor::getInstance();
  if (pwmPTR->init())
  {
    Serial.print("\r\n✓ PWMProcessor SUCCESS");
    
    // Test direct frequency control
    Serial.print("\r\n\r\n- Testing direct frequency control:");
    Serial.print("\r\n  Setting 10Hz at 50% duty");
    pwmPTR->setSpeedPulseHz(10.0f);
    pwmPTR->setSpeedPulseDuty(0.5f);
    pwmPTR->enableSpeedPulse(true);
    
    // Test speed-based control
    Serial.print("\r\n\r\n- Testing speed-based control:");
    Serial.print("\r\n  Setting 130 pulses per meter (ISO 11786)");
    pwmPTR->setPulsesPerMeter(130.0f);  // ISO 11786 standard
    Serial.print("\r\n  Setting speed to 10 km/h");
    pwmPTR->setSpeedKmh(10.0f);
    Serial.printf("\r\n  Calculated frequency: %.1f Hz", pwmPTR->getSpeedPulseHz());
    
    // Print status
    pwmPTR->printStatus();
  }
  else
  {
    Serial.print("\r\n✗ PWMProcessor FAILED");
  }

  Serial.print("\r\n\n*** Class Testing Complete ***\r\n");

  // Initialize NAVProcessor
  Serial.print("\r\n\n*** Initializing NAVProcessor ***");
  NAVProcessor::init();
  navPTR = navPTR->getInstance();
  navPTR->printStatus();

  // Print status of all managers
  hardwarePTR->printHardwareStatus();
  serialPTR->printSerialStatus();
  if (imuPTR)
  {
    imuPTR->printStatus();
  }

  // Initialize Motor Driver
  Serial.print("\r\n\n*** Initializing Motor Driver ***");
  MotorDriverType detectedType = MotorDriverFactory::detectMotorType(canPTR);
  motorPTR = MotorDriverFactory::createMotorDriver(detectedType, hardwarePTR, canPTR);
  
  if (motorPTR && motorPTR->init()) {
    Serial.printf("\r\n✓ %s motor driver initialized", motorPTR->getTypeName());
  } else {
    Serial.print("\r\n✗ Motor driver init failed");
  }

  // Initialize AutosteerProcessor
  Serial.print("\r\n\n*** Initializing AutosteerProcessor ***");
  autosteerPTR = AutosteerProcessor::getInstance();
  if (autosteerPTR->init()) {
    Serial.print("\r\n✓ AutosteerProcessor initialized");
  } else {
    Serial.print("\r\n✗ AutosteerProcessor init failed");
  }

  // Motor Driver Testing
  if (MOTOR_TEST_MODE) {
    Serial.print("\r\n\n*** Motor Driver Test Mode ***");
    
    // Auto-detect motor type
    MotorDriverType detectedType = MotorDriverFactory::detectMotorType(canPTR);
    
    // Create motor driver
    Serial.print("\r\n- Creating motor driver...");
    motorPTR = MotorDriverFactory::createMotorDriver(detectedType, hardwarePTR, canPTR);
    
    if (motorPTR) {
      if (motorPTR->init()) {
        Serial.printf("\r\n✓ %s motor driver initialized", motorPTR->getTypeName());
      
        // Skip automatic test for Keya
        if (motorPTR->getType() != MotorDriverType::KEYA_CAN) {
          // Run automatic test for PWM motors only
          Serial.print("\r\n1. Enable motor");
          motorPTR->enable(true);
          delay(1000);
          
          Serial.print("\r\n2. Test forward 25%");
          motorPTR->setSpeed(25.0f);
          delay(2000);
          
          Serial.print("\r\n3. Test forward 50%");
          motorPTR->setSpeed(50.0f);
          delay(2000);
          
          Serial.print("\r\n4. Test stop");
          motorPTR->stop();
          delay(1000);
          
          Serial.print("\r\n5. Test reverse -25%");
          motorPTR->setSpeed(-25.0f);
          delay(2000);
          
          Serial.print("\r\n6. Test reverse -50%");
          motorPTR->setSpeed(-50.0f);
          delay(2000);
          
          Serial.print("\r\n7. Stop and disable");
          motorPTR->stop();
          motorPTR->enable(false);
        }
        
        Serial.print("\r\n\r\nCommands: e/d (enable/disable), +/- (speed), s (stop), ? (status)");
      } else {
        if (motorPTR && motorPTR->getType() == MotorDriverType::KEYA_CAN) {
          Serial.print("\r\n✗ Keya motor not detected on CAN3");
        } else {
          Serial.print("\r\n✗ Motor driver init failed");
        }
      }
    }
  }

  Serial.print("\r\n\n=== New Dawn Initialization Complete ===");
  Serial.print("\r\nEntering main loop...\r\n");

  Serial.print("\r\n=== System Ready ===\r\n");
  
}

void loop()
{
  mongoose_poll();

  static uint32_t lastPrint = 0;
  static uint32_t lastDetailedStatus = 0;
  static uint32_t lastNAVStatus = 0;
  static uint32_t lastCANStatus = 0;
  static uint32_t lastADStatus = 0;
  static uint32_t lastPWMTest = 0;
  static uint32_t lastAutosteerStatus = 0;
  
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

  // Normal operation (non-test mode)
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
  
  // Update LEDs
  static uint32_t lastLEDUpdate = 0;
  static bool ledDebugPrinted = false;
  if (ledPTR && millis() - lastLEDUpdate > 100)  // Update every 100ms
  {
    lastLEDUpdate = millis();
    
    // Debug print once
    if (!ledDebugPrinted) {
      Serial.print("\r\n[LED] Update loop started");
      ledDebugPrinted = true;
    }
    
    // Power/Ethernet LED
    // TODO: Add proper ethernet link detection when NetworkBase is updated
    bool ethernetUp = true;  // For now assume ethernet is up
    ledPTR->setPowerState(ethernetUp, navPTR && navPTR->hasAgIOConnection());
    
    // GPS LED
    if (gnssPTR) {
      ledPTR->setGPSState(gnssPTR->getData().fixQuality, gnssPTR->hasGPS());
    }
    
    // Steer LED - commented out for minimal AutosteerProcessor
    /*
    if (adPTR && autosteerPTR) {
      bool wasReady = adPTR->getWASRaw() > 0;  // Simple WAS presence check
      ledPTR->setSteerState(wasReady, 
                           autosteerPTR->isEnabled(), 
                           autosteerPTR->getState() == SteerState::ACTIVE);
    }
    */
    
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
      
      // Debug output once
      static bool insDebugPrinted = false;
      if (!insDebugPrinted) {
        Serial.printf("\r\n[LED] INS: alignStatus=%d, detected=%d, init=%d, valid=%d",
                     gpsData.insAlignmentStatus, insDetected, insInitialized, insValid);
        insDebugPrinted = true;
      }
      
      ledPTR->setIMUState(insDetected, insInitialized, insValid);
    } else {
      // No IMU/INS detected
      ledPTR->setIMUState(false, false, false);
    }
    
    // Update LED hardware (handles blinking)
    ledPTR->update();
  }
  
  // Autosteer status - commented out for minimal AutosteerProcessor
  /*
  if (autosteerPTR && !MOTOR_TEST_MODE && millis() - lastAutosteerStatus > 2000)
  {
    lastAutosteerStatus = millis();
    
    const char* stateStr = "OFF";
    switch(autosteerPTR->getState()) {
      case SteerState::OFF: stateStr = "OFF"; break;
      case SteerState::READY: stateStr = "READY"; break;
      case SteerState::ACTIVE: stateStr = "ACTIVE"; break;
    }
    
    // Show RPM info if using Keya motor
    if (motorPTR && motorPTR->getType() == MotorDriverType::KEYA_CAN) {
        KeyaCANDriver* keya = static_cast<KeyaCANDriver*>(motorPTR);
        Serial.printf("\r\n[Autosteer] State: %s | Target: %.1f° | Current: %.1f° | Motor: %.1f%% | RPM: Cmd=%.0f Act=%.0f",
                      stateStr,
                      autosteerPTR->getTargetAngle(),
                      autosteerPTR->getCurrentAngle(),
                      autosteerPTR->getMotorSpeed(),
                      keya->getCommandedRPM(),
                      keya->getActualRPM());
    } else {
        Serial.printf("\r\n[Autosteer] State: %s | Target: %.1f° | Current: %.1f° | Motor: %.1f%%",
                      stateStr,
                      autosteerPTR->getTargetAngle(),
                      autosteerPTR->getCurrentAngle(),
                      autosteerPTR->getMotorSpeed());
    }
  }
  */

  // Quick status print every second
  if (millis() - lastPrint > 1000)
  {
    lastPrint = millis();

    // Check for IMU data
    if (imuPTR && imuPTR->hasValidData())
    {
      IMUData data = imuPTR->getCurrentData();
      Serial.printf("\r\n[%.1fs] IMU: R=%.1f° P=%.1f° H=%.1f°",
                    millis() / 1000.0, data.roll, data.pitch, data.heading);
    }
  }


  // NAV processor status every 30 seconds
  if (millis() - lastNAVStatus > 30000)
  {
    lastNAVStatus = millis();

    if (navPTR)
    {
      navPTR->printStatus();
    }
  }

  // CAN status every 30 seconds to monitor Keya motor
  if (millis() - lastCANStatus > 30000)
  {
    lastCANStatus = millis();

    if (canPTR && canPTR->isCAN3Active())
    {
      Serial.printf("\r\n[CAN Status] CAN3 msgs: %lu, Keya detected: %s",
                    canPTR->getCAN3MessageCount(),
                    canPTR->isKeyaDetected() ? "YES" : "NO");
    }
  }
  
  // A/D status every 30 seconds with switch change detection
  if (millis() - lastADStatus > 30000)
  {
    lastADStatus = millis();
    
    if (adPTR)
    {
      Serial.printf("\r\n[A/D] WAS: %.1f° (%.2fV) Raw:%d | Work: %s | Steer: %s",
                    adPTR->getWASAngle(), 
                    adPTR->getWASVoltage(),
                    adPTR->getWASRaw(),
                    adPTR->isWorkSwitchOn() ? "ON" : "OFF",
                    adPTR->isSteerSwitchOn() ? "ON" : "OFF");
      
      // Check for switch changes
      if (adPTR->hasWorkSwitchChanged())
      {
        Serial.printf("\r\n[A/D] Work switch changed to: %s", 
                      adPTR->isWorkSwitchOn() ? "ON" : "OFF");
        adPTR->clearWorkSwitchChange();
      }
      
      if (adPTR->hasSteerSwitchChanged())
      {
        Serial.printf("\r\n[A/D] Steer switch changed to: %s", 
                      adPTR->isSteerSwitchOn() ? "ON" : "OFF");
        adPTR->clearSteerSwitchChange();
      }
    }
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
          
          // Debug output every 30 seconds
          if (millis() - lastPWMTest > 30000)
          {
            lastPWMTest = millis();
            Serial.printf("\r\n[PWM] GPS Speed: %.1f km/h = %.1f Hz", 
                          speedKmh, pwmPTR->getSpeedPulseHz());
          }
        }
      }
      
      // Set the speed
      pwmPTR->setSpeedKmh(speedKmh);
    }
  }

  // Very detailed status every 30 seconds
  if (millis() - lastDetailedStatus > 30000)
  {
    lastDetailedStatus = millis();

    Serial.print("\r\n\n=== Detailed System Status ===");
    hardwarePTR->printHardwareStatus();
    serialPTR->printSerialStatus();
    gnssPTR->printStats();

    if (imuPTR)
    {
      imuPTR->printStatus();
    }

    Serial.print("\r\n=== End Status ===\r\n");
  }

  // Process GPS1 data if available
  static uint32_t gps1ByteCount = 0;
  static uint32_t lastGPS1Report = 0;
  static uint32_t gps1TotalBytes = 0;
  
  while (SerialGPS1.available())
  {
    char c = SerialGPS1.read();
    gps1ByteCount++;
    gps1TotalBytes++;
    gnssPTR->processNMEAChar(c);
  }
  
  // Report GPS1 byte count every 5 seconds for debugging
  if (millis() - lastGPS1Report > 5000)
  {
    lastGPS1Report = millis();
    if (gps1ByteCount > 0) {
      Serial.printf("\r\n[GPS1] Received %lu bytes in last 5s (total: %lu)", gps1ByteCount, gps1TotalBytes);
    } else {
      Serial.printf("\r\n[GPS1] No data received in last 5s");
    }
    gps1ByteCount = 0;
  }
  
  // Process GPS2 data if available (for F9P dual RELPOSNED)
  if (SerialGPS2.available())
  {
    uint8_t b = SerialGPS2.read();
    gnssPTR->processUBXByte(b);
  }

}