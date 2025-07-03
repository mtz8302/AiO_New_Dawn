// en_HomePage.h
// English version of home page

#ifndef EN_HOME_PAGE_H
#define EN_HOME_PAGE_H

#include <Arduino.h>

const char EN_HOME_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>AiO New Dawn</title>
    <style>%CSS_STYLES%</style>
</head>
<body>
    <div class='container'>
        <h1>AgOpenGPS AiO New Dawn</h1>
        <div class='status'>System is running!</div>
        
        <h2>Configuration</h2>
        <ul>
            <li><a href='/eventlogger'>Event Logger Settings</a></li>
            <li><a href='/network'>Network Settings</a></li>
            <li><a href='/device'>Device Settings</a></li>
        </ul>
        
        <h2>Tools</h2>
        <ul>
            <li><a href='/api/status'>System Status (JSON)</a></li>
            <li><a href='/was-demo'>WAS Live Data Demo</a></li>
            <li><a href='/restart'>Restart System</a></li>
            <li><a href='/ota'>OTA Update</a></li>
        </ul>
        
        <p>IP: %IP_ADDRESS% | Link: %LINK_SPEED% Mbps</p>
        <p>Language: <a href='/lang/en'>English</a> | <a href='/lang/de'>Deutsch</a></p>
        <p>Firmware Version: %FIRMWARE_VERSION%</p>
    </div>
</body>
</html>
)rawliteral";

// Text strings for home page
const char EN_HOME_TITLE[] PROGMEM = "AgOpenGPS AiO New Dawn";
const char EN_SYSTEM_RUNNING[] PROGMEM = "System is running!";
const char EN_NETWORK_INFO[] PROGMEM = "Network Information";
const char EN_IP_ADDRESS[] PROGMEM = "IP Address:";
const char EN_LINK_SPEED[] PROGMEM = "Link Speed:";
const char EN_CONFIGURATION[] PROGMEM = "Configuration";
const char EN_EVENT_LOGGER_LINK[] PROGMEM = "Event Logger Settings";
const char EN_NETWORK_LINK[] PROGMEM = "Network Settings";
const char EN_MACHINE_LINK[] PROGMEM = "Machine Configuration";
const char EN_SENSORS_LINK[] PROGMEM = "Sensor Configuration";
const char EN_TOOLS[] PROGMEM = "Tools";
const char EN_SYSTEM_STATUS_LINK[] PROGMEM = "System Status (JSON)";
const char EN_RESTART_LINK[] PROGMEM = "Restart System";

#endif // EN_HOME_PAGE_H