// en_EventLoggerPage.h
// English version of EventLogger configuration page

#ifndef EN_EVENTLOGGER_PAGE_H
#define EN_EVENTLOGGER_PAGE_H

#include <Arduino.h>

const char EN_EVENTLOGGER_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Event Logger Configuration</title>
    <style>%CSS_STYLES%</style>
</head>
<body>
    <div class='container'>
        <h1>Event Logger Configuration</h1>
        <div class='info'>Configure syslog output for remote logging. Changes are saved to EEPROM.</div>
        
        <form id='configForm'>
            <h3>Serial Console</h3>
            <div class='form-group'>
                <label>Serial Enabled:</label>
                <input type='checkbox' id='serialEnabled' %SERIAL_ENABLED%>
            </div>
            
            <div class='form-group'>
                <label>Serial Level:</label>
                <select id='serialLevel'>
                    %SERIAL_LEVEL_OPTIONS%
                </select>
            </div>
            
            <h3>Remote Syslog (UDP)</h3>
            <div class='form-group'>
                <label>UDP Enabled:</label>
                <input type='checkbox' id='udpEnabled' %UDP_ENABLED%>
            </div>
            
            <div class='form-group'>
                <label>UDP Level:</label>
                <select id='udpLevel'>
                    %UDP_LEVEL_OPTIONS%
                </select>
            </div>
            
            <div class='form-group'>
                <label>Syslog Port:</label>
                <input type='number' id='syslogPort' value='%SYSLOG_PORT%' min='1' max='65535'>
            </div>
            
            <div class='info'>Syslog server IP is configured by your network admin. Default port is 514.</div>
            
            <button type='button' onclick='saveConfig()'>Save Configuration</button>
            <button type='button' onclick='window.location="/"'>Back to Home</button>
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
                    alert('Configuration saved successfully!');
                } else {
                    alert('Error saving configuration');
                }
            });
        }
        </script>
    </div>
</body>
</html>
)rawliteral";

// Text strings for translation
const char EN_EVENTLOGGER_TITLE[] PROGMEM = "Event Logger Configuration";
const char EN_EVENTLOGGER_DESC[] PROGMEM = "Configure syslog output for remote logging. Changes are saved to EEPROM.";
const char EN_SERIAL_CONSOLE[] PROGMEM = "Serial Console";
const char EN_SERIAL_ENABLED[] PROGMEM = "Serial Enabled:";
const char EN_SERIAL_LEVEL[] PROGMEM = "Serial Level:";
const char EN_REMOTE_SYSLOG[] PROGMEM = "Remote Syslog (UDP)";
const char EN_UDP_ENABLED[] PROGMEM = "UDP Enabled:";
const char EN_UDP_LEVEL[] PROGMEM = "UDP Level:";
const char EN_SYSLOG_PORT[] PROGMEM = "Syslog Port:";
const char EN_SYSLOG_INFO[] PROGMEM = "Syslog server IP is configured by your network admin. Default port is 514.";
const char EN_SAVE_CONFIG[] PROGMEM = "Save Configuration";
const char EN_BACK_HOME[] PROGMEM = "Back to Home";
const char EN_SAVE_SUCCESS[] PROGMEM = "Configuration saved successfully!";
const char EN_SAVE_ERROR[] PROGMEM = "Error saving configuration";

#endif // EN_EVENTLOGGER_PAGE_H