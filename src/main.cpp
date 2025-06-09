#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"
#include "pcb.h"

void setup() {
  delay(5000); //delay for time to start monitor
  Serial.begin(115200);

  // Minimal GPS1 serial setup for RTCM testing
  SerialGPS1.begin(460800); // baudGPS from pcb.h
  SerialGPS1.addMemoryForWrite(GPS1txbuffer, sizeof(GPS1txbuffer));

  storedCfgSetup();
  ethernet_init();
  mongoose_init();
  udpSetup();

}

void loop() {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000)
  { // Print every 5 seconds
    Serial.println("Main loop running...");
    lastPrint = millis();
  }
  mongoose_poll();
}
