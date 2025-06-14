// main.cpp - Updated section with IMU testing
#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"
#include "ConfigManager.h"
#include "HardwareManager.h"
#include "SerialManager.h"
#include "SerialGlobals.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h" // Add this include

// ConfigManager pointer definition (same pattern as machinePTR)
// This is the ONLY definition - all other files use extern declaration
ConfigManager *configPTR = nullptr;
extern HardwareManager *hardwarePTR;
extern SerialManager *serialPTR;
extern GNSSProcessor *gnssPTR;
extern IMUProcessor *imuPTR; // Add this extern

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
  if (gnssPTR->setup(false, true)) // Enable debug and noise filter
  {
    Serial.print("\r\n✓ GNSSProcessor SUCCESS");
    Serial.print("\r\n  - Debug enabled: YES");
    Serial.print("\r\n  - Noise filter: YES");
    Serial.print("\r\n  - Ready for NMEA data");

    // Print initial stats
    gnssPTR->printStats();
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
  }
  else
  {
    Serial.print("\r\n✗ IMUProcessor - No IMU detected");
    Serial.print("\r\n  - Check wiring and power");
    Serial.print("\r\n  - For TM171: TX on Teensy -> RX on TM171 (reversed labels!)");
  }

  Serial.print("\r\n\n*** Class Testing Complete ***\r\n");

  // Print status of all managers
  hardwarePTR->printHardwareStatus();
  serialPTR->printSerialStatus();
  if (imuPTR)
  {
    imuPTR->printStatus();
  }

  Serial.print("\r\n\n*** Class Testing Complete ***\r\n");

  // Print status of all managers
  hardwarePTR->printHardwareStatus();
  serialPTR->printSerialStatus();
  if (imuPTR)
  {
    imuPTR->printStatus();
  }

  // Print status of all managers
  hardwarePTR->printHardwareStatus();
  serialPTR->printSerialStatus();

  Serial.print("\r\n\n=== New Dawn Initialization Complete ===");
  Serial.print("\r\nEntering main loop...\r\n");

  Serial.print("\r\n=== System Ready ===\r\n");
}

void loop()
{
  mongoose_poll();

  static uint32_t lastPrint = 0;
  static uint32_t lastIMUDebug = 0;
  static uint32_t lastDetailedStatus = 0;

  // Process IMU data
  if (imuPTR)
  {
    imuPTR->process();
  }

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

  // Detailed IMU debug every 5 seconds
  if (millis() - lastIMUDebug > 5000)
  {
    lastIMUDebug = millis();

    if (imuPTR)
    {
      imuPTR->printStatus();
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

  // Process GPS data if available
  if (SerialGPS1.available())
  {
    char c = SerialGPS1.read();
    gnssPTR->processNMEAChar(c);
  }

  // Print GNSS structure contents every 10 seconds to see if data is getting in
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 10000)
  {
    lastCheck = millis();

    const auto &data = gnssPTR->getData();

    Serial.println("\r\n=== GNSSProcessor Data Structure ===");
    Serial.printf("isValid: %s\r\n", data.isValid ? "YES" : "NO");
    Serial.printf("hasPosition: %s\r\n", data.hasPosition ? "YES" : "NO");
    Serial.printf("hasVelocity: %s\r\n", data.hasVelocity ? "YES" : "NO");
    Serial.printf("hasDualHeading: %s\r\n", data.hasDualHeading ? "YES" : "NO");
    Serial.printf("latitude: %.6f\r\n", data.latitude);
    Serial.printf("longitude: %.6f\r\n", data.longitude);
    Serial.printf("fixQuality: %d\r\n", data.fixQuality);
    Serial.printf("numSatellites: %d\r\n", data.numSatellites);
    Serial.printf("hdop: %.1f\r\n", data.hdop);
    Serial.printf("speedKnots: %.1f\r\n", data.speedKnots);
    Serial.printf("headingTrue: %.1f\r\n", data.headingTrue);
    Serial.printf("dataAge: %lu ms\r\n", gnssPTR->getDataAge());
    Serial.printf("dual heading: %.2f\r\n", data.dualHeading);
    Serial.printf("dual roll: %.2f\r\n", data.dualRoll);
    Serial.printf("heading quality: %d\r\n", data.headingQuality);

    // Show stats
    const auto &stats = gnssPTR->getStats();
    Serial.printf("Messages processed: %lu\r\n", stats.messagesProcessed);
    Serial.printf("Parse errors: %lu\r\n", stats.parseErrors);
    Serial.printf("Success rate: %.1f%%\r\n", gnssPTR->getSuccessRate());
    Serial.println("=====================================");
  }
}