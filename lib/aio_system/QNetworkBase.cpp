// QNetworkBase.cpp
// Network implementation using QNEthernet

#include "QNetworkBase.h"
#include <QNEthernet.h>

using namespace qindesign::network;

// Global network configuration
NetworkConfig netConfig;

// Save network configuration
void save_current_net() {
    // EEPROM saving of network config not yet implemented
}

// Initialize UDP services - handled by AsyncUDPHandler
void QNetworkBase::udpSetup() {
    // UDP setup is now handled by AsyncUDPHandler::init()
}