#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"
#include "ConfigManager.h"
#include "HardwareManager.h"
#include "SerialManager.h"
#include "SerialGlobals.h"
#include "GNSSProcessor.h"

// ConfigManager pointer definition (same pattern as machinePTR)
// This is the ONLY definition - all other files use extern declaration
ConfigManager *configPTR = nullptr;
extern HardwareManager *hardwarePTR;
extern SerialManager *serialPTR;
extern GNSSProcessor *gnssPTR;

void setup()
{
  delay(5000); // delay for time to start monitor
  Serial.begin(115200);

  Serial.print("\r\n\n=== Teensy 4.1 AiO-NG-v6 New Dawn ===");
  Serial.print("\r\nInitializing subsystems...");

  // Minimal GPS1 serial setup for RTCM testing
  // SerialGPS1.begin(460800); // baudGPS from pcb.h
  // SerialGPS1.addMemoryForWrite(GPS1txbuffer, sizeof(GPS1txbuffer));

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

  Serial.print("\r\n\n*** Class Testing Complete ***\r\n");

  // Print status of all managers
  hardwarePTR->printHardwareStatus();
  serialPTR->printSerialStatus();

  // NEW: Test GNSSProcessor
  Serial.print("\r\n\n*** Testing GNSSProcessor ***");
  gnssPTR = new GNSSProcessor();
  if (gnssPTR->setup(true, true)) // Enable debug and noise filter
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

  Serial.print("\r\n\n=== New Dawn Initialization Complete ===");
  Serial.print("\r\nEntering main loop...\r\n");

  Serial.print("\r\n=== System Ready ===\r\n");
}

void loop()
{
  mongoose_poll();

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000)
  { // Print every 5 seconds
    Serial.println("Main loop running...");
    lastPrint = millis();
  }

  // if (serialPTR && serialPTR->isSerialInitialized())
  // {
  //   // Update bridge mode (handles USB DTR detection)
  //   serialPTR->updateBridgeMode();

  //   // Process serial ports
  //   serialPTR->processGPS1();
  //   serialPTR->processGPS2();
  //   serialPTR->processRTK();
  //   serialPTR->processRS232();
  //   serialPTR->processESP32();
  // }
  // else
  // {
  //   // Fall back to existing serial processing if SerialManager failed
  //   // Keep your existing serialGPS(), serialESP32(), serialRTCM() calls here
  // }

  // if (SerialGPS1.available())
  // {
  //   char c = SerialGPS1.read();
  //   Serial.println(c);
  //   // gnssPTR->processNMEAChar(c);

  //   // // Debug: Show first 100 characters to see what we're getting
  //   // static uint32_t charCount = 0;
  //   // if (charCount < 100)
  //   // {
  //   //   Serial.print(c);
  //   //   charCount++;
  //   //   if (charCount == 100)
  //   //   {
  //   //     Serial.println("\r\n--- End of first 100 GPS characters ---");
  //   //   }
  //   // }
  // }

  // Just feed GPS data to processor - use if instead of while to prevent blocking
  if (SerialGPS1.available())
  {
    char c = SerialGPS1.read();
    gnssPTR->processNMEAChar(c);
  }

  // Print structure contents every 5 seconds to see if data is getting in
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000)
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

    // Show stats
    const auto &stats = gnssPTR->getStats();
    Serial.printf("Messages processed: %lu\r\n", stats.messagesProcessed);
    Serial.printf("Parse errors: %lu\r\n", stats.parseErrors);
    Serial.printf("Success rate: %.1f%%\r\n", gnssPTR->getSuccessRate());
    Serial.println("=====================================");
  }

}