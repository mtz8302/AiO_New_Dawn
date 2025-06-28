#ifndef COMMANDHANDLER_H_
#define COMMANDHANDLER_H_

#include "Arduino.h"
#include "EventLogger.h"
#include "MachineProcessor.h"
#include "ConfigManager.h"

// Command handler states
enum class CommandState {
    MAIN_MENU,
    LOGGING_MENU,
    CONFIG_MENU
};

class CommandHandler {
private:
    static CommandHandler* instance;
    CommandState currentState = CommandState::MAIN_MENU;
    
    // Pointers to system components
    MachineProcessor* machinePtr = nullptr;
    ConfigManager* configPtr = nullptr;
    EventLogger* loggerPtr = nullptr;
    
    // Private constructor for singleton
    CommandHandler();
    
    // Menu handlers
    void handleMainMenu(char cmd);
    void handleLoggingMenu(char cmd);
    void handleConfigMenu(char cmd);
    
    // Menu display functions
    void showMainMenu();
    void showLoggingMenu();
    void showConfigMenu();

public:
    ~CommandHandler();
    
    // Singleton access
    static CommandHandler* getInstance();
    static void init();
    
    // Set component pointers
    void setMachineProcessor(MachineProcessor* ptr) { machinePtr = ptr; }
    void setConfigManager(ConfigManager* ptr) { configPtr = ptr; }
    
    // Main process function - call this from loop()
    void process();
    
    // Get current state
    CommandState getState() const { return currentState; }
};

#endif // COMMANDHANDLER_H_