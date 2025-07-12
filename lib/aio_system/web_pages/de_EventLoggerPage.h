// de_EventLoggerPage.h
// German version of EventLogger configuration page

#ifndef DE_EVENTLOGGER_PAGE_H
#define DE_EVENTLOGGER_PAGE_H

#include <Arduino.h>

const char DE_EVENTLOGGER_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Ereignisprotokoll-Konfiguration</title>
    <style>%CSS_STYLES%</style>
</head>
<body>
    <div class='container'>
        <h1>Ereignisprotokoll-Konfiguration</h1>
        <div class='info'>Konfigurieren Sie die Syslog-Ausgabe für die Remote-Protokollierung. Änderungen werden im EEPROM gespeichert.</div>
        
        <form id='configForm'>
            <h3>Serielle Konsole</h3>
            <div class='form-group'>
                <label>Seriell aktiviert:</label>
                <input type='checkbox' id='serialEnabled' %SERIAL_ENABLED%>
            </div>
            
            <div class='form-group'>
                <label>Serielle Stufe:</label>
                <select id='serialLevel'>
                    %SERIAL_LEVEL_OPTIONS%
                </select>
            </div>
            
            <h3>Remote-Syslog (UDP)</h3>
            <div class='form-group'>
                <label>UDP aktiviert:</label>
                <input type='checkbox' id='udpEnabled' %UDP_ENABLED%>
            </div>
            
            <div class='form-group'>
                <label>UDP-Stufe:</label>
                <select id='udpLevel'>
                    %UDP_LEVEL_OPTIONS%
                </select>
            </div>
            
            <div class='form-group'>
                <label>Syslog-Port:</label>
                <input type='number' id='syslogPort' value='%SYSLOG_PORT%' min='1' max='65535'>
            </div>
            
            <div class='info'>Die Syslog-Server-IP wird von Ihrem Netzwerkadministrator konfiguriert. Standardport ist 514.</div>
            
            <button type='button' onclick='saveConfig()'>Konfiguration speichern</button>
            <button type='button' onclick='window.location="/"'>Zurück zur Startseite</button>
        </form>
        
        <script>
        function saveConfig() {
            const data = {
                serialEnabled: document.getElementById('serialEnabled').checked,
                serialLevel: parseInt(document.getElementById('serialLevel').value),
                udpEnabled: document.getElementById('udpEnabled').checked,
                udpLevel: parseInt(document.getElementById('udpLevel').value),
                syslogPort: parseInt(document.getElementById('syslogPort').value)
            };
            fetch('/api/eventlogger/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(data)
            }).then(response => {
                if (response.ok) {
                    alert('Konfiguration erfolgreich gespeichert!');
                } else {
                    alert('Fehler beim Speichern der Konfiguration');
                }
            });
        }
        </script>
    </div>
</body>
</html>
)rawliteral";

// Text strings for translation
const char DE_EVENTLOGGER_TITLE[] PROGMEM = "Ereignisprotokoll-Konfiguration";
const char DE_EVENTLOGGER_DESC[] PROGMEM = "Konfigurieren Sie die Syslog-Ausgabe für die Remote-Protokollierung. Änderungen werden im EEPROM gespeichert.";
const char DE_SERIAL_CONSOLE[] PROGMEM = "Serielle Konsole";
const char DE_SERIAL_ENABLED[] PROGMEM = "Seriell aktiviert:";
const char DE_SERIAL_LEVEL[] PROGMEM = "Serielle Stufe:";
const char DE_REMOTE_SYSLOG[] PROGMEM = "Remote-Syslog (UDP)";
const char DE_UDP_ENABLED[] PROGMEM = "UDP aktiviert:";
const char DE_UDP_LEVEL[] PROGMEM = "UDP-Stufe:";
const char DE_SYSLOG_PORT[] PROGMEM = "Syslog-Port:";
const char DE_SYSLOG_INFO[] PROGMEM = "Die Syslog-Server-IP wird von Ihrem Netzwerkadministrator konfiguriert. Standardport ist 514.";
const char DE_SAVE_CONFIG[] PROGMEM = "Konfiguration speichern";
const char DE_BACK_HOME[] PROGMEM = "Zurück zur Startseite";
const char DE_SAVE_SUCCESS[] PROGMEM = "Konfiguration erfolgreich gespeichert!";
const char DE_SAVE_ERROR[] PROGMEM = "Fehler beim Speichern der Konfiguration";

#endif // DE_EVENTLOGGER_PAGE_H