// SimplePlaceholderPages.h
// Temporary placeholder pages for testing

#ifndef SIMPLE_PLACEHOLDER_PAGES_H
#define SIMPLE_PLACEHOLDER_PAGES_H

#include <Arduino.h>

const char SIMPLE_NETWORK_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Network Settings</title>
    <style>%CSS_STYLES%</style>
</head>
<body>
    <div class='container'>
        <h1>Network Settings</h1>
        <p>Network configuration page - Coming soon</p>
        <p><a href='/'>Back to Home</a></p>
    </div>
</body>
</html>
)rawliteral";

const char SIMPLE_OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>OTA Update</title>
    <style>%CSS_STYLES%</style>
</head>
<body>
    <div class='container'>
        <h1>OTA Update</h1>
        <p>OTA update functionality - Coming soon</p>
        <p><a href='/'>Back to Home</a></p>
    </div>
</body>
</html>
)rawliteral";

const char SIMPLE_ANALOG_WORK_SWITCH_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Analog Work Switch</title>
    <style>%CSS_STYLES%</style>
</head>
<body>
    <div class='container'>
        <h1>Analog Work Switch</h1>
        <p>Analog work switch configuration - Coming soon</p>
        <p><a href='/'>Back to Home</a></p>
    </div>
</body>
</html>
)rawliteral";

#endif // SIMPLE_PLACEHOLDER_PAGES_H