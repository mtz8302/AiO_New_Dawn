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
#include "NAVProcessor.h"

// ConfigManager pointer definition (same pattern as machinePTR)
// This is the ONLY definition - all other files use extern declaration
ConfigManager *configPTR = nullptr;

// Define the global pointers (only declared as extern in headers)
HardwareManager *hardwarePTR = nullptr;
SerialManager *serialPTR = nullptr;
GNSSProcessor *gnssPTR = nullptr;
IMUProcessor *imuPTR = nullptr;
NAVProcessor *navPTR = nullptr;

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
  if (gnssPTR->setup(false, true)) // Disable debug, enable noise filter
  {
    Serial.print("\r\n✓ GNSSProcessor SUCCESS");
    Serial.print("\r\n  - Debug enabled: NO");
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
  static uint32_t lastNAVStatus = 0;

  // Process IMU data
  if (imuPTR)
  {
    imuPTR->process();
  }

  // Process NAV messages
  if (navPTR)
  {
    navPTR->process();
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

  // NAV processor status every 10 seconds
  if (millis() - lastNAVStatus > 10000)
  {
    lastNAVStatus = millis();

    if (navPTR)
    {
      navPTR->printStatus();
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
  
  while (SerialGPS1.available())
  {
    char c = SerialGPS1.read();
    gps1ByteCount++;
    gnssPTR->processNMEAChar(c);
  }
  
  // Report GPS1 byte count every 5 seconds
  if (millis() - lastGPS1Report > 5000 && gps1ByteCount > 0)
  {
    lastGPS1Report = millis();
    Serial.printf("\r\n[GPS1] Received %lu bytes in last 5s", gps1ByteCount);
    gps1ByteCount = 0;
  }
  
  // Process GPS2 data if available (for F9P dual RELPOSNED)
  if (SerialGPS2.available())
  {
    uint8_t b = SerialGPS2.read();
    gnssPTR->processUBXByte(b);
  }

  // Print GNSS structure contents every 5 seconds to see if data is getting in
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000)
  {
    lastCheck = millis();

    const auto &data = gnssPTR->getData();
    
    Serial.print("\r\n\n=== GNSSProcessor Data Structure ===");
    Serial.printf("\r\nisValid: %s", data.isValid ? "YES" : "NO");
    Serial.printf("\r\nhasPosition: %s", data.hasPosition ? "YES" : "NO");
    Serial.printf("\r\nhasVelocity: %s", data.hasVelocity ? "YES" : "NO");
    Serial.printf("\r\nhasDualHeading: %s", data.hasDualHeading ? "YES" : "NO");
    Serial.printf("\r\nhasINS: %s", data.hasINS ? "YES" : "NO");
    Serial.printf("\r\nlatitude: %.8f", data.latitude);
    Serial.printf("\r\nlongitude: %.8f", data.longitude);
    Serial.printf("\r\naltitude: %.2f", data.altitude);
    Serial.printf("\r\nfixQuality: %d", data.fixQuality);
    Serial.printf("\r\nnumSatellites: %d", data.numSatellites);
    Serial.printf("\r\nhdop: %.1f", data.hdop);
    Serial.printf("\r\nspeedKnots: %.1f", data.speedKnots);
    Serial.printf("\r\nheadingTrue: %.1f", data.headingTrue);
    Serial.printf("\r\ndataAge: %lu ms", gnssPTR->getDataAge());
    Serial.printf("\r\ndual heading: %.2f", data.dualHeading);
    Serial.printf("\r\ndual roll: %.2f", data.dualRoll);
    Serial.printf("\r\nINS pitch: %.2f", data.insPitch);
    Serial.printf("\r\nheading quality: %d", data.headingQuality);
    
    // Display INSPVAXA standard deviation data if available
    if (data.hasINS && (data.posStdDevLat > 0 || data.posStdDevLon > 0))
    {
      Serial.print("\r\n--- INSPVAXA Std Dev Data ---");
      Serial.printf("\r\nPos StdDev: Lat=%.3fm Lon=%.3fm Alt=%.3fm", 
                    data.posStdDevLat, data.posStdDevLon, data.posStdDevAlt);
      Serial.printf("\r\nVel StdDev: N=%.3fm/s E=%.3fm/s U=%.3fm/s", 
                    data.velStdDevNorth, data.velStdDevEast, data.velStdDevUp);
    }
    
    Serial.print("\r\n=====================================");
    
    gnssPTR->printStats();

  }
}