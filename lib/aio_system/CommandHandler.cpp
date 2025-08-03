#include "CommandHandler.h"
#include "ADProcessor.h"

// Static instance pointer
CommandHandler* CommandHandler::instance = nullptr;

CommandHandler::CommandHandler() {
    loggerPtr = EventLogger::getInstance();
}

CommandHandler::~CommandHandler() {
    instance = nullptr;
}

CommandHandler* CommandHandler::getInstance() {
    if (instance == nullptr) {
        instance = new CommandHandler();
    }
    return instance;
}

void CommandHandler::init() {
    getInstance();  // Create instance if needed
}

void CommandHandler::process() {
    if (!Serial.available()) {
        return;
    }
    
    char cmd = Serial.read();
    
    // Ignore line ending characters (CR and LF)
    if (cmd == '\r' || cmd == '\n') {
        return;
    }
    
    // Handle command
    handleCommand(cmd);
}

void CommandHandler::handleCommand(char cmd) {
    EventConfig& config = loggerPtr->getConfig();
    
    switch (cmd) {
        case '1':  // Toggle serial output
            loggerPtr->enableSerial(!config.enableSerial);
            Serial.printf("\r\nSerial logging %s\r\n", config.enableSerial ? "ENABLED" : "DISABLED");
            break;
            
        case '2':  // Toggle UDP syslog
            loggerPtr->enableUDP(!config.enableUDP);
            Serial.printf("\r\nUDP syslog %s\r\n", config.enableUDP ? "ENABLED" : "DISABLED");
            break;
            
        case '3':  // Increase serial level
            if (config.serialLevel > 0) {
                config.serialLevel--;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '4':  // Decrease serial level
            if (config.serialLevel < 7) {
                config.serialLevel++;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '5':  // Increase UDP level
            if (config.udpLevel > 0) {
                config.udpLevel--;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case '6':  // Decrease UDP level
            if (config.udpLevel < 7) {
                config.udpLevel++;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case '7':  // Toggle rate limiting
            loggerPtr->setRateLimitEnabled(!loggerPtr->isRateLimitEnabled());
            break;
            
        case 't':  // Test log messages
        case 'T':
            LOG_INFO(EventSource::USER, "Generating test log messages...");
            LOG_DEBUG(EventSource::USER, "Test DEBUG message");
            LOG_INFO(EventSource::USER, "Test INFO message");
            LOG_NOTICE(EventSource::USER, "Test NOTICE message");
            LOG_WARNING(EventSource::USER, "Test WARNING message");
            LOG_ERROR(EventSource::USER, "Test ERROR message");
            LOG_CRITICAL(EventSource::USER, "Test CRITICAL message");
            LOG_ALERT(EventSource::USER, "Test ALERT message");
            LOG_EMERGENCY(EventSource::USER, "Test EMERGENCY message");
            break;
            
        case 's':  // Show statistics
        case 'S':
            Serial.printf("\r\n\nEvent Statistics:");
            Serial.printf("\r\nTotal events logged: %lu\r\n", loggerPtr->getEventCount());
            break;
            
        case 'r':  // Reset counter
        case 'R':
            loggerPtr->resetEventCount();
            Serial.print("Event counter reset\r\n");
            break;
            
        // QNEthernet doesn't need explicit log level management
        // Removed Mongoose log level option
            
        case 'l':  // Loop timing diagnostics (moved from T to avoid conflict)
        case 'L':
            toggleLoopTiming();
            break;
            
        case 'a':  // ADC test
        case 'A':
            testCurrentSensor();
            break;
            
        case 'c':  // Continuous current monitoring
        case 'C':
            continuousCurrentMonitor();
            break;
            
        case 'w':  // Work switch analog test
        case 'W':
            testAnalogWorkSwitch();
            break;
            
        case '?':
        case 'h':
        case 'H':
            showMenu();
            break;
            
        default:
            Serial.printf("\r\nUnknown command: '%c'\r\n", cmd);
            break;
    }
}


void CommandHandler::showMenu() {
    loggerPtr->printConfig();
    Serial.print("\r\n=== Firmware Controls ===");
    Serial.print("\r\n1 - Toggle serial output");
    Serial.print("\r\n2 - Toggle UDP syslog");
    Serial.print("\r\n3/4 - Decrease/Increase serial level");
    Serial.print("\r\n5/6 - Decrease/Increase UDP level");
    Serial.print("\r\n7 - Toggle rate limiting");
    Serial.print("\r\nT - Generate test messages");
    Serial.print("\r\nS - Show statistics");
    Serial.print("\r\nR - Reset event counter");
    Serial.print("\r\nL - Toggle loop timing diagnostics");
    Serial.print("\r\nA - Test current sensor ADC");
    Serial.print("\r\nC - Continuous current monitoring");
    Serial.print("\r\nW - Test analog work switch");
    Serial.print("\r\n? - Show this menu");
    Serial.print("\r\n=========================\r\n");
}

void CommandHandler::testCurrentSensor() {
    Serial.println("\r\n=== ADC Current Sensor Test ===");
    
    // Force enable the motor driver for this test
    Serial.println("Enabling motor driver (pin 4 HIGH)...");
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);  // Wake up DRV8701
    
    // Set up PWM pins
    Serial.println("Setting up PWM (pins 5 & 6)...");
    pinMode(5, OUTPUT);
    pinMode(6, OUTPUT);
    analogWriteFrequency(5, 75);
    analogWriteFrequency(6, 75);
    
    // Apply small PWM to generate current
    Serial.println("Applying PWM: 50/256 on pin 6 (RIGHT)");
    analogWrite(5, 0);     // PWM1 = 0
    analogWrite(6, 50);    // PWM2 = 50 (small current)
    delay(100);  // Let current stabilize
    
    Serial.println("Reading pin A13 with motor active...");
    
    for (int i = 0; i < 10; i++) {
        int rawADC = analogRead(A13);
        float voltage = (rawADC * 3.3f) / 4095.0f;
        Serial.printf("  Reading %d: ADC=%d, Voltage=%.3fV\r\n", 
                     i+1, rawADC, voltage);
        delay(100);
    }
    
    // Rapid sampling test
    Serial.println("\r\nRapid sampling test (100 samples in 100ms):");
    int minVal = 4095, maxVal = 0;
    long sum = 0;
    for (int i = 0; i < 100; i++) {
        int val = analogRead(A13);
        sum += val;
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
        delayMicroseconds(1000);  // 1ms between samples
    }
    float avg = sum / 100.0;
    Serial.printf("  Min=%d, Max=%d, Avg=%.1f, Range=%d\r\n", 
                  minVal, maxVal, avg, maxVal - minVal);
    
    // Calculate expected values
    Serial.println("\r\nExpected ADC values:");
    Serial.println("  DRV8701 SO: 455mV/A typical (0.455V/A)");
    Serial.println("  Baseline: ~90 counts (0.072V)");
    Serial.println("  0.5A load: +282 counts = 372 total");
    Serial.println("  1.0A load: +565 counts = 655 total");
    Serial.println("  2.0A load: +1130 counts = 1220 total");
    
    // Calculate what current the readings indicate
    float baselineVoltage = 0.072f;  // ~90 counts
    float avgVoltage = (avg * 3.3f) / 4095.0f;
    float deltaVoltage = avgVoltage - baselineVoltage;
    float impliedCurrent = deltaVoltage / 0.455f;  // 455mV/A
    
    Serial.printf("\r\nMeasurement analysis:");
    Serial.printf("\r\n  Average voltage: %.3fV", avgVoltage);
    Serial.printf("\r\n  Delta from baseline: %.3fV", deltaVoltage);
    Serial.printf("\r\n  Implied current: %.3fA\r\n", impliedCurrent);
    
    if (maxVal - minVal > 100) {
        Serial.println("\r\nWARNING: High variation detected!");
        Serial.println("Possible issues:");
        Serial.println("  - PWM switching noise");
        Serial.println("  - Need to filter baseline readings");
        Serial.println("  - Check motor driver is enabled");
    }
    
    // Show detection threshold
    Serial.printf("\r\nCurrent detection threshold: ADC > 100");
    Serial.printf("\r\n  This ignores readings below %.3fV\r\n", (100 * 3.3f) / 4095.0f);
    
    // Now test with PWM off
    Serial.println("\r\n=== Testing with PWM OFF ===");
    analogWrite(5, 0);
    analogWrite(6, 0);
    delay(100);
    
    Serial.println("Reading 5 times with motor driver enabled but no PWM:");
    for (int i = 0; i < 5; i++) {
        int rawADC = analogRead(A13);
        float voltage = (rawADC * 3.3f) / 4095.0f;
        Serial.printf("  ADC=%d, Voltage=%.3fV\r\n", rawADC, voltage);
        delay(100);
    }
    
    // Disable motor driver
    Serial.println("\r\nDisabling motor driver...");
    digitalWrite(4, LOW);
}

void CommandHandler::continuousCurrentMonitor() {
    Serial.println("\r\n=== WARNING ===");
    Serial.println("This test BLOCKS the main loop!");
    Serial.println("Motor control will NOT work during this test.");
    Serial.println("Press 'y' to continue anyway, or any other key to cancel...");
    
    // Wait for user response
    while (!Serial.available()) {
        delay(10);
    }
    
    char response = Serial.read();
    // Clear any remaining input
    while (Serial.available()) {
        Serial.read();
    }
    
    // Debug what we received
    Serial.printf("Received character: %d (0x%02X)\r\n", response, response);
    
    // Accept various forms of "yes" - enter, y, Y
    if (response != '\n' && response != '\r' && response != 'y' && response != 'Y') {
        Serial.println("Test cancelled.");
        return;
    }
    
    Serial.println("\r\n=== PWM Interference Analysis ===");
    Serial.println("PWM frequency: 75Hz (13.3ms period)");
    Serial.println("Taking 200 rapid samples...");
    
    // Arrays to store rapid samples
    const int NUM_SAMPLES = 200;
    uint32_t timestamps[NUM_SAMPLES];
    int adcValues[NUM_SAMPLES];
    
    // Collect rapid samples
    uint32_t startTime = millis();
    for (int i = 0; i < NUM_SAMPLES; i++) {
        timestamps[i] = millis() - startTime;
        adcValues[i] = analogRead(A13);
        delayMicroseconds(500);  // 0.5ms between samples for ~2kHz sampling
    }
    
    // Analyze the pattern
    Serial.println("\r\nPattern Analysis:");
    int minADC = 4095, maxADC = 0;
    int lowCount = 0, highCount = 0;
    const int threshold = 200;  // Threshold between baseline and load readings
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (adcValues[i] < minADC) minADC = adcValues[i];
        if (adcValues[i] > maxADC) maxADC = adcValues[i];
        
        if (adcValues[i] < threshold) lowCount++;
        else highCount++;
    }
    
    Serial.printf("Min ADC: %d (%.3fV)\r\n", minADC, (minADC * 3.3f) / 4095.0f);
    Serial.printf("Max ADC: %d (%.3fV)\r\n", maxADC, (maxADC * 3.3f) / 4095.0f);
    Serial.printf("Range: %d counts (%.3fV)\r\n", maxADC - minADC, ((maxADC - minADC) * 3.3f) / 4095.0f);
    Serial.printf("Low readings (<200): %d (%.1f%%)\r\n", lowCount, (lowCount * 100.0f) / NUM_SAMPLES);
    Serial.printf("High readings (>=200): %d (%.1f%%)\r\n", highCount, (highCount * 100.0f) / NUM_SAMPLES);
    
    // Look for pattern transitions
    Serial.println("\r\nTransition pattern (first 50 samples):");
    Serial.println("Time(ms), ADC, State, Transition");
    char lastState = adcValues[0] < threshold ? 'L' : 'H';
    for (int i = 0; i < min(50, NUM_SAMPLES); i++) {
        char currentState = adcValues[i] < threshold ? 'L' : 'H';
        bool transition = (currentState != lastState);
        Serial.printf("%lu, %d, %c, %s\r\n", 
                      timestamps[i], adcValues[i], currentState,
                      transition ? "<<<" : "");
        lastState = currentState;
    }
    
    // Try to find optimal sampling phase
    Serial.println("\r\n=== Finding Optimal PWM Phase ===");
    Serial.println("Testing different phase offsets...");
    
    // Test different phase offsets within PWM period
    const int PWM_PERIOD_MS = 13;  // 75Hz = 13.3ms period
    int bestPhase = 0;
    int bestConsistency = 0;
    
    for (int phase = 0; phase < PWM_PERIOD_MS; phase++) {
        Serial.printf("\r\nPhase offset %dms: ", phase);
        
        // Take 10 samples at this phase
        int phaseSamples[10];
        int phaseSum = 0;
        
        // Sync to phase
        uint32_t syncTime = millis();
        while ((millis() - syncTime) % PWM_PERIOD_MS != phase) {
            // Wait for correct phase
        }
        
        // Take samples at this phase
        for (int i = 0; i < 10; i++) {
            phaseSamples[i] = analogRead(A13);
            phaseSum += phaseSamples[i];
            
            // Wait for next PWM cycle
            uint32_t nextTime = millis() + PWM_PERIOD_MS;
            while (millis() < nextTime) {
                // Wait
            }
        }
        
        // Calculate consistency (lower variance = better)
        int avgPhase = phaseSum / 10;
        int variance = 0;
        for (int i = 0; i < 10; i++) {
            int diff = phaseSamples[i] - avgPhase;
            variance += diff * diff;
        }
        variance /= 10;
        
        Serial.printf("Avg=%d, Var=%d, Samples: ", avgPhase, variance);
        for (int i = 0; i < 10; i++) {
            Serial.printf("%d ", phaseSamples[i]);
        }
        
        // Track best phase (for load readings, we want high average with low variance)
        if (avgPhase > 400 && variance < 1000) {
            if (avgPhase > bestConsistency) {
                bestPhase = phase;
                bestConsistency = avgPhase;
            }
        }
    }
    
    Serial.printf("\r\n\r\nBest phase offset: %dms (avg ADC: %d)\r\n", bestPhase, bestConsistency);
    Serial.println("\r\nRecommendation: ADC reads should be synchronized with PWM period");
    Serial.println("to avoid sampling during switching transitions.");
    
    // Clear serial buffer
    while (Serial.available()) {
        Serial.read();
    }
    Serial.println("\r\nAnalysis complete. Motor control resumed.");
}

void CommandHandler::testAnalogWorkSwitch() {
    Serial.println("\r\n=== Analog Work Switch Test ===");
    
    ADProcessor* adProcessor = ADProcessor::getInstance();
    
    // Toggle analog mode
    bool currentMode = adProcessor->isAnalogWorkSwitchEnabled();
    adProcessor->setAnalogWorkSwitchEnabled(!currentMode);
    
    Serial.printf("Analog work switch mode: %s -> %s\r\n", 
                  currentMode ? "ENABLED" : "DISABLED",
                  !currentMode ? "ENABLED" : "DISABLED");
    
    // Only reconfigure the work pin, don't reinitialize entire ADProcessor
    Serial.println("Reconfiguring work pin...");
    adProcessor->configureWorkPin();
    
    if (!currentMode) {
        // Now enabled - show continuous readings
        Serial.println("\r\nAnalog work switch enabled. Press any key to stop monitoring...\r\n");
        Serial.println("Raw ADC | Percent | State");
        Serial.println("--------|---------|------");
        
        while (!Serial.available()) {
            // Update ADProcessor
            adProcessor->updateSwitches();
            
            uint16_t raw = adProcessor->getWorkSwitchAnalogRaw();
            float percent = adProcessor->getWorkSwitchAnalogPercent();
            bool state = adProcessor->isWorkSwitchOn();
            
            Serial.printf("%4d    | %5.1f%% | %s\r\n", 
                         raw, percent, state ? "ON " : "OFF");
            
            delay(250);  // Update 4 times per second
        }
        
        // Clear serial buffer
        while (Serial.available()) {
            Serial.read();
        }
        Serial.println("\r\nMonitoring stopped.");
    } else {
        Serial.println("Analog mode disabled. Work switch is now in digital mode.");
    }
}

