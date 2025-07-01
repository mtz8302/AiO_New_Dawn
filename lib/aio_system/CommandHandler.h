#ifndef COMMANDHANDLER_H_
#define COMMANDHANDLER_H_

#include "Arduino.h"
#include "EventLogger.h"
#include "MachineProcessor.h"

// Command handler states
enum class CommandState {
    MAIN_MENU,
    LOGGING_MENU
};

class CommandHandler {
private:
    static CommandHandler* instance;
    CommandState currentState = CommandState::MAIN_MENU;
    
    // Pointers to system components
    MachineProcessor* machinePtr = nullptr;
    EventLogger* loggerPtr = nullptr;
    
    // Private constructor for singleton
    CommandHandler();
    
    // Menu handlers
    void handleMainMenu(char cmd);
    void handleLoggingMenu(char cmd);
    
    // Menu display functions
    void showMainMenu();
    void showLoggingMenu();

public:
    ~CommandHandler();
    
    // Singleton access
    static CommandHandler* getInstance();
    static void init();
    
    // Set component pointers
    void setMachineProcessor(MachineProcessor* ptr) { machinePtr = ptr; }
    
    // Main process function - call this from loop()
    void process();
    
    // Get current state
    CommandState getState() const { return currentState; }
};

#endif // COMMANDHANDLER_H_