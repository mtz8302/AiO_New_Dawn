/****************************************************************************************************************************
  multiFileProject.ino
  For ESP32

  AsyncTCP_SSL is a library for the ESP32

  Built by Khoi Hoang https://github.com/khoih-prog/AsyncTCP_SSL
  Licensed under MIT license
*****************************************************************************************************************************/

// To demo how to include files in multi-file Projects

#if 1
#if ( defined(CORE_TEENSY) && defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41) )
  // For Teensy 4.1
  #define BOARD_TYPE      "TEENSY 4.1"
  // Use true for NativeEthernet Library, false if using other Ethernet libraries
  #define USE_NATIVE_ETHERNET     false
  #define USE_QN_ETHERNET         true
#else
  #error Only Teensy 4.1 supported
#endif
#endif

#define TEENSY41_ASYNC_TCP_VERSION_MIN_TARGET      "Teensy41_AsyncTCP v1.0.0"
#define TEENSY41_ASYNC_TCP_VERSION_MIN             1000000

#include "multiFileProject.h"

// Can be included as many times as necessary, without `Multiple Definitions` Linker Error
#include "Teensy41_AsyncTCP.h"

void setup() 
{
  Serial.begin(115200);
  while (!Serial);

  delay(500);
  
  Serial.println("\nStart multiFileProject");
  Serial.println(TEENSY41_ASYNC_TCP_VERSION);

#if defined(TEENSY41_ASYNC_TCP_VERSION_MIN)
  if (TEENSY41_ASYNC_TCP_VERSION_INT < TEENSY41_ASYNC_TCP_VERSION_MIN)
  {
    Serial.print("Warning. Must use this example on Version equal or later than : ");
    Serial.println(TEENSY41_ASYNC_TCP_VERSION_MIN_TARGET);
  }
#endif

  Serial.print("You're OK now");
}

void loop() 
{
  // put your main code here, to run repeatedly:
}
