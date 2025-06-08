#include "Arduino.h"
#include "mongoose_glue.h"
#include "NetworkBase.h"

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while (!Serial) delay(50);

  ethernet_init();
  mongoose_init();
}

void loop() {
  mongoose_poll();
}
