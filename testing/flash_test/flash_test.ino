// Simple test program for OTA flash verification
// Prints "Flashing World" to serial console

// Flash ID for OTA verification - must match FLASH_ID in FlashTxx.h
const char* flash_id = "fw_teensy41";

void setup() {
    // Initialize serial
    Serial.begin(115200);
    
    // Wait up to 5 seconds for serial monitor
    unsigned long start = millis();
    while (!Serial && millis() - start < 5000) {
        ; // wait for serial port to connect
    }
    
    // Print our message
    Serial.println("\n\n=== OTA Flash Test ===");
    Serial.println("Flashing World");
    Serial.println("Flash successful!");
    Serial.print("Flash ID: ");
    Serial.println(flash_id);
    Serial.println("===================\n");
}

void loop() {
    // Print message every 5 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        Serial.print("Uptime: ");
        Serial.print(millis() / 1000);
        Serial.println(" seconds - Flashing World");
    }
}