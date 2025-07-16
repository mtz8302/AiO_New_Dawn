#define STEER_PIN 2      // Digital Switch/Btn input for autosteer engage/disengage
#define KICKOUT_D_PIN 3  // Digital REMOTE, encoder or other digital disengage input

#define WAS_PIN A15      // Analog WAS input
#define CURRENT_PIN A13  // Analog CURRENT sense from on board DRV8701
#define WORK_PIN A17     // Analog WORK input, can also be used for Digital switches, see UI for settings
//#define KICKOUT_A_PIN   A12 // Analog PRESSURE (can also be used for 2nd Quadrature encodeder input)

// Cytron/DRV8701P
#define PWM1_PIN 6   // DRV PWM1 pin, one direction
#define PWM2_PIN 5   // DRV PWM2 pin, the other direction
#define SLEEP_PIN 4  // DRV Sleep pin, LOCK output




void setup() {
  pinMode(STEER_PIN, INPUT_PULLUP);
  pinMode(KICKOUT_D_PIN, INPUT_PULLUP);

  pinMode(WAS_PIN, INPUT_DISABLE);      // 200-3900 (12bit)
  pinMode(CURRENT_PIN, INPUT_DISABLE);  // 276 at sleep, 45 at idle (12bit)
  pinMode(WORK_PIN, INPUT_DISABLE);     // 4086-5 (12bit)

  pinMode(PWM1_PIN, OUTPUT);
  pinMode(PWM2_PIN, OUTPUT);
  pinMode(SLEEP_PIN, OUTPUT);

  analogWriteFrequency(PWM1_PIN, 75);
  analogWriteFrequency(PWM2_PIN, 75);

  // put both bridge control pins LOW for Hi-Z outputs
  analogWrite(PWM1_PIN, 256);
  analogWrite(PWM2_PIN, 256);
  digitalWrite(SLEEP_PIN, HIGH);  // LOW keeps DRV8701-Cytron asleep

  //analogReadResolution(12);
  //analogReadAveraging(16);

  Serial.begin(115200);
  Serial.print("\r\n\n\n*********************\r\nStarting...\r\n");
}

void loop() {
  static float currentReadings[100];
  static u_int8_t index = 0;
  static elapsedMillis asLoop, csLoop;
  if (asLoop > 100) {
    asLoop = 0;

    uint16_t readWAS = analogRead(WAS_PIN);
    Serial.printf("WAS: %4i", readWAS);

    // 51 - 978 is Matt's WAS range, adjust according to your active range
    int16_t pwmOut = constrain(map(readWAS, 51, 978, 0, 256), 0, 256);  // map WAS input 51-978 -> 0-256 PWM output
    Serial.printf(" PWM: %3i(%03i%)", pwmOut, int(float(pwmOut) / 2.55));

    if (pwmOut < 0) {
      analogWrite(PWM1_PIN, 256 + pwmOut);
      analogWrite(PWM2_PIN, 256);
    } else if (pwmOut > 0) {
      analogWrite(PWM1_PIN, 256);
      analogWrite(PWM2_PIN, 256 - pwmOut);
    } else {
      analogWrite(PWM1_PIN, 256);
      analogWrite(PWM2_PIN, 256);
    }

    float currentAve = 0;
    for (u_int8_t i = 0; i < 100; i++) {
      currentAve += currentReadings[i];
    }
    currentAve /= 100;
    uint16_t readCurrent = abs(analogRead(CURRENT_PIN) - ((digitalRead(SLEEP_PIN) == LOW) ? 99 : 12));
    Serial.printf(" Current feedback: %3i -> %2.3fV -> %2.3fA", readCurrent, currentAve, map((float)currentAve / 3.3 * 1024.0, 0.0, 88, 0.0, 1.4));  // 0 - 88, 0 - 1.4 A

    Serial.println();
  }

  if (csLoop > 1) {
    csLoop = 0;
    uint16_t readCurrent = abs(analogRead(CURRENT_PIN) - ((digitalRead(SLEEP_PIN) == LOW) ? 99 : 12));
    currentReadings[index++] = float(readCurrent) / 1024.0 * 3.3;
    if (index >= 100) {
      index = 0;
    }
  }
}