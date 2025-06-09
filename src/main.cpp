#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"
#include "pcb.h"
#include "ConfigManager.h"

// ConfigManager pointer definition (same pattern as machinePTR)
// This is the ONLY definition - all other files use extern declaration
ConfigManager *configPTR = nullptr;

void setup()
{
  delay(5000); // delay for time to start monitor
  Serial.begin(115200);

  Serial.print("\r\n\n=== Teensy 4.1 AiO-NG-v6 New Dawn ===");
  Serial.print("\r\nInitializing subsystems...");

  // Minimal GPS1 serial setup for RTCM testing
  SerialGPS1.begin(460800); // baudGPS from pcb.h
  SerialGPS1.addMemoryForWrite(GPS1txbuffer, sizeof(GPS1txbuffer));

  // Network and communication setup FIRST
  storedCfgSetup();
  ethernet_init();
  mongoose_init();
  udpSetup();

  Serial.print("\r\n- Network stack initialized");

  // Initialize ConfigManager AFTER Mongoose (same pattern as machinePTR)
  configPTR = new ConfigManager();
  Serial.print("\r\n- ConfigManager initialized");

  Serial.print("\r\n=== System Ready ===\r\n");
}

void loop()
{
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000)
  { // Print every 5 seconds
    Serial.println("Main loop running...");
    lastPrint = millis();
  }
  mongoose_poll();
}