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
        
        <h2>Netzwerkinformationen</h2>
        <p>IP-Adresse: %IP_ADDRESS%</p>
        <p>Verbindungsgeschwindigkeit: %LINK_SPEED% Mbps</p>
        
        <h2>Konfiguration</h2>
        <ul>
            <li><a href='/eventlogger'>Ereignisprotokoll-Einstellungen</a></li>
            <li><a href='/network'>Netzwerkeinstellungen</a></li>
            <li><a href='/machine'>Maschinenkonfiguration</a></li>
            <li><a href='/sensors'>Sensorkonfiguration</a></li>
        </ul>
        
        <h2>Werkzeuge</h2>
        <ul>
            <li><a href='/api/status'>Systemstatus (JSON)</a></li>
            <li><a href='/restart'>System neu starten</a></li>
        </ul>
        
        <h2>Language / Sprache</h2>
        <p>
            <a href='/lang/en'>English</a> | 
            <a href='/lang/de'>Deutsch</a>
        </p>
    </div>
</body>
</html>
)rawliteral";

#endif // DE_HOME_PAGE_H