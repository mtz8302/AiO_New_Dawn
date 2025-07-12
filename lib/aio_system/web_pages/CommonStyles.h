// CommonStyles.h
// Common CSS styles for all web pages

#ifndef COMMON_STYLES_H
#define COMMON_STYLES_H

#include <Arduino.h>

const char COMMON_CSS[] PROGMEM = R"rawliteral(
body {
    font-family: Arial, sans-serif;
    margin: 20px;
    background-color: #f0f0f0;
}
.container {
    max-width: 800px;
    margin: 0 auto;
    background-color: white;
    padding: 20px;
    border-radius: 10px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}
h1 {
    color: #333;
}
h2 {
    color: #555;
}
.status {
    margin: 10px 0;
    padding: 10px;
    background-color: #e8f5e9;
    border-radius: 5px;
}
.form-group {
    margin: 15px 0;
}
label {
    display: inline-block;
    width: 150px;
    font-weight: bold;
}
select, input[type="number"], input[type="text"] {
    padding: 5px;
    width: 200px;
}
button {
    background-color: #4CAF50;
    color: white;
    padding: 10px 20px;
    border: none;
    border-radius: 5px;
    cursor: pointer;
    margin: 5px;
}
button:hover {
    background-color: #45a049;
}
.btn {
    padding: 10px 20px;
    border: none;
    border-radius: 5px;
    cursor: pointer;
    margin: 5px;
    color: white;
    text-decoration: none;
    display: inline-block;
}
.btn-home {
    background-color: #2196F3;
}
.btn-home:hover {
    background-color: #1976D2;
}
.btn-primary {
    background-color: #4CAF50;
}
.btn-primary:hover {
    background-color: #45a049;
}
.btn-warning {
    background-color: #ff9800;
}
.btn-warning:hover {
    background-color: #e68900;
}
.btn-danger {
    background-color: #f44336;
}
.btn-danger:hover {
    background-color: #da190b;
}
.btn-secondary {
    background-color: #757575;
}
.btn-secondary:hover {
    background-color: #616161;
}
.nav-buttons {
    margin-top: 20px;
    display: flex;
    flex-wrap: wrap;
    gap: 5px;
}
.info {
    background-color: #e8f5e9;
    padding: 10px;
    border-radius: 5px;
    margin: 10px 0;
}
a {
    color: #2196F3;
    text-decoration: none;
}
a:hover {
    text-decoration: underline;
}
)rawliteral";

#endif // COMMON_STYLES_H