#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"

void setup() {
  delay(5000); //delay for time to start monitor
  Serial.begin(115200);

  storedCfgSetup();
  ethernet_init();
  mongoose_init();
  udpSetup();

}

void loop() {
  mongoose_poll();
}
