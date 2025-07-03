// de_HomePage.h
// German version of home page

#ifndef DE_HOME_PAGE_H
#define DE_HOME_PAGE_H

#include <Arduino.h>

const char DE_HOME_PAGE[] PROGMEM = R"rawliteral(
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
        <div class='status'>System läuft!</div>
        
        <h2>Konfiguration</h2>
        <ul>
            <li><a href='/eventlogger'>Ereignisprotokoll-Einstellungen</a></li>
            <li><a href='/network'>Netzwerkeinstellungen</a></li>
            <li><a href='/device'>Geräteeinstellungen</a></li>
        </ul>
        
        <h2>Werkzeuge</h2>
        <ul>
            <li><a href='/api/status'>Systemstatus (JSON)</a></li>
            <li><a href='/restart'>System neu starten</a></li>
            <li><a href='/ota'>OTA Update</a></li>
        </ul>
        
        <p>IP: %IP_ADDRESS% | Link: %LINK_SPEED% Mbps</p>
        <p>Sprache: <a href='/lang/en'>English</a> | <a href='/lang/de'>Deutsch</a></p>
        <p>Firmware Version: %FIRMWARE_VERSION%</p>
    </div>
</body>
</html>
)rawliteral";

#endif // DE_HOME_PAGE_H