#include "CommandHandler.h"

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
    
    // Handle state-specific commands
    switch (currentState) {
        case CommandState::MAIN_MENU:
            handleMainMenu(cmd);
            break;
            
        case CommandState::LOGGING_MENU:
            handleLoggingMenu(cmd);
            break;
            
        case CommandState::CONFIG_MENU:
            handleConfigMenu(cmd);
            break;
    }
}

void CommandHandler::handleMainMenu(char cmd) {
    Serial.printf("\r\n[CMD] Main menu received: '%c' (0x%02X)", cmd, cmd);  // Debug
    switch (cmd) {
        case 'l':
        case 'L':
            currentState = CommandState::LOGGING_MENU;
            showLoggingMenu();
            break;
            
        case 'd':
        case 'D':
            Serial.print("\r\n\n*** Running Section Diagnostics ***");
            if (machinePtr) {
                machinePtr->runSectionDiagnostics();
            } else {
                Serial.print("\r\nERROR: MachineProcessor not initialized!");
            }
            break;
            
            
        case 'c':
        case 'C':
            if (configPtr) {
                currentState = CommandState::CONFIG_MENU;
                showConfigMenu();
            } else {
                Serial.print("\r\nConfiguration not available");
            }
            break;
            
        case '?':
        case 'h':
        case 'H':
            showMainMenu();
            break;
            
        default:
            // Unknown command - could show help
            break;
    }
}

void CommandHandler::handleLoggingMenu(char cmd) {
    EventConfig& config = loggerPtr->getConfig();
    
    switch (cmd) {
        case '1':  // Toggle serial output
            loggerPtr->enableSerial(!config.enableSerial);
            Serial.printf("\r\nSerial logging %s", config.enableSerial ? "ENABLED" : "DISABLED");
            break;
            
        case '2':  // Toggle UDP syslog
            loggerPtr->enableUDP(!config.enableUDP);
            Serial.printf("\r\nUDP syslog %s", config.enableUDP ? "ENABLED" : "DISABLED");
            break;
            
        case '3':  // Increase serial level
            if (config.serialLevel > 0) {
                config.serialLevel--;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '4':  // Decrease serial level
            if (config.serialLevel < 7) {
                config.serialLevel++;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '5':  // Increase UDP level
            if (config.udpLevel > 0) {
                config.udpLevel--;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case '6':  // Decrease UDP level
            if (config.udpLevel < 7) {
                config.udpLevel++;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case 't':  // Test log messages
        case 'T':
            Serial.print("\r\nGenerating test log messages...");
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
            Serial.printf("\r\nTotal events logged: %lu", loggerPtr->getEventCount());
            break;
            
        case 'r':  // Reset counter
        case 'R':
            loggerPtr->resetEventCount();
            Serial.print("\r\nEvent counter reset");
            break;
            
        // QNEthernet doesn't need explicit log level management
        // Removed Mongoose log level option
            
        case 'q':  // Quit to main menu
        case 'Q':
        case 27:   // ESC key
            currentState = CommandState::MAIN_MENU;
            Serial.print("\r\nReturned to main menu");
            break;
            
        case '?':
        case 'h':
        case 'H':
            showLoggingMenu();
            break;
            
        default:
            Serial.printf("\r\nUnknown command: '%c'", cmd);
            break;
    }
}


void CommandHandler::handleConfigMenu(char cmd) {
    // Placeholder for future config menu
    switch (cmd) {
        case 'q':
        case 'Q':
        case 27:  // ESC
            currentState = CommandState::MAIN_MENU;
            Serial.print("\r\nReturned to main menu");
            break;
            
        default:
            Serial.print("\r\nConfig menu not yet implemented");
            currentState = CommandState::MAIN_MENU;
            break;
    }
}

void CommandHandler::showMainMenu() {
    Serial.print("\r\n\n=== Main Menu ===");
    Serial.print("\r\nL - Logging configuration");
    Serial.print("\r\nD - Run section diagnostics");
    if (configPtr) {
        Serial.print("\r\nC - Configuration menu");
    }
    Serial.print("\r\n? - Show this menu");
    Serial.print("\r\n=================");
}

void CommandHandler::showLoggingMenu() {
    loggerPtr->printConfig();
    Serial.print("\r\n\n=== Logging Control Menu ===");
    Serial.print("\r\n1 - Toggle serial output");
    Serial.print("\r\n2 - Toggle UDP syslog");
    Serial.print("\r\n3/4 - Decrease/Increase serial level");
    Serial.print("\r\n5/6 - Decrease/Increase UDP level");
    // QNEthernet handles its own logging internally
    Serial.print("\r\nT - Generate test messages");
    Serial.print("\r\nS - Show statistics");
    Serial.print("\r\nR - Reset event counter");
    Serial.print("\r\nQ - Return to main menu");
    Serial.print("\r\n? - Show this menu");
    Serial.print("\r\n============================");
}


void CommandHandler::showConfigMenu() {
    Serial.print("\r\n\n=== Configuration Menu ===");
    Serial.print("\r\n(Not yet implemented)");
    Serial.print("\r\nQ - Return to main menu");
    Serial.print("\r\n==========================");
}