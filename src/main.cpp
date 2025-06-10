#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"
//#include "pcb.h"
#include "ConfigManager.h"
#include "HardwareManager.h"
#include "SerialManager.h"
#include "SerialGlobals.h"

// ConfigManager pointer definition (same pattern as machinePTR)
// This is the ONLY definition - all other files use extern declaration
ConfigManager *configPTR = nullptr;
extern HardwareManager *hardwarePTR;
extern SerialManager *serialPTR;

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

  if (serialPTR && serialPTR->isSerialInitialized())
  {
    // Update bridge mode (handles USB DTR detection)
    serialPTR->updateBridgeMode();

    // Process serial ports
    serialPTR->processGPS1();
    serialPTR->processGPS2();
    serialPTR->processRTK();
    serialPTR->processRS232();
    serialPTR->processESP32();
  }
  else
  {
    // Fall back to existing serial processing if SerialManager failed
    // Keep your existing serialGPS(), serialESP32(), serialRTCM() calls here
  }
}