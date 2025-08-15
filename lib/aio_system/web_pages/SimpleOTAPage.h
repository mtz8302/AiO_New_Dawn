// SimpleOTAPage.h
// Simplified OTA update page with embedded CSS

#ifndef SIMPLE_OTA_PAGE_H
#define SIMPLE_OTA_PAGE_H

#include <Arduino.h>

const char SIMPLE_OTA_UPDATE_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>OTA Update - AiO New Dawn</title>
    <style>
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
        h1 { color: #333; }
        h2 { color: #555; }
        .status {
            margin: 10px 0;
            padding: 10px;
            background-color: #e8f5e9;
            border-radius: 5px;
        }
        .form-group { margin: 15px 0; }
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            margin: 5px;
            color: white;
        }
        .btn-home { background-color: #6c757d; }
        .btn-primary { background-color: #007bff; }
        .help-text { 
            color: #666; 
            font-size: 0.9em; 
            margin-top: 5px;
        }
        .warning {
            background-color: #ffeeee;
            border: 1px solid #ff0000;
            padding: 10px;
            margin: 20px 0;
            border-radius: 5px;
        }
        .nav-buttons {
            margin-top: 20px;
        }
        progress {
            width: 100%;
            height: 20px;
            margin: 10px 0;
        }
        #status {
            margin: 10px 0;
            padding: 10px;
            text-align: center;
            border-radius: 5px;
        }
    </style>
    <script>
        function uploadFile() {
            try {
                const fileInput = document.getElementById('file');
                const file = fileInput.files[0];
                
                if (!file) {
                    alert('Please select a firmware file');
                    return false;
                }
                
                if (!file.name.endsWith('.hex')) {
                    alert('Please select a .hex firmware file');
                    return false;
                }
            
                const statusDiv = document.getElementById('status');
                const progressBar = document.getElementById('progress');
                const uploadBtn = document.getElementById('uploadBtn');
            
                statusDiv.innerHTML = 'Uploading firmware...';
                uploadBtn.disabled = true;
                progressBar.style.display = 'block';
                
                // Read file content directly
                const reader = new FileReader();
                reader.onload = function(e) {
                    const content = e.target.result;
                    
                    const xhr = new XMLHttpRequest();
                    
                    xhr.addEventListener('load', function() {
                        if (xhr.status === 200) {
                            statusDiv.innerHTML = '<span style="color: green;">Upload successful! System will reboot...</span>';
                            setTimeout(() => {
                                window.location.href = '/';
                            }, 5000);
                        } else {
                            statusDiv.innerHTML = '<span style="color: red;">Upload failed: ' + xhr.responseText + '</span>';
                            uploadBtn.disabled = false;
                        }
                    });
                    
                    xhr.addEventListener('error', function() {
                        statusDiv.innerHTML = '<span style="color: red;">Upload error occurred</span>';
                        uploadBtn.disabled = false;
                    });
                    
                    // Monitor upload progress manually
                    let lastProgress = 0;
                    const updateProgress = setInterval(() => {
                        if (xhr.readyState === 4) {
                            clearInterval(updateProgress);
                        } else {
                            lastProgress = Math.min(lastProgress + 10, 90);
                            progressBar.value = lastProgress;
                            statusDiv.innerHTML = 'Uploading: ' + lastProgress + '%';
                        }
                    }, 100);
                    
                    xhr.open('POST', '/api/ota/upload');
                    xhr.setRequestHeader('Content-Type', 'text/plain');
                    xhr.send(content);
                };
                
                reader.readAsText(file);
                
            } catch (error) {
                console.error('Upload error:', error);
                alert('Error: ' + error.message);
                document.getElementById('uploadBtn').disabled = false;
                return false;
            }
            
            return false;
        }
    </script>
</head>
<body>
    <div class='container'>
        <h1>OTA Firmware Update</h1>
        
        <div class='status'>
            <p>Current Firmware: %FIRMWARE_VERSION%</p>
            <p>Board Type: %BOARD_TYPE%</p>
        </div>
        
        <div class='warning'>
            <strong>Warning:</strong> Incorrect firmware can brick your device. Only upload firmware built for Teensy 4.1.
        </div>
        
        <form onsubmit='return false;'>
            <h2>Select Firmware File</h2>
            
            <div class='form-group'>
                <input type='file' id='file' name='firmware' accept='.hex' required>
                <div class='help-text'>
                    Select a .hex firmware file built for Teensy 4.1
                </div>
            </div>
            
            <progress id='progress' value='0' max='100' style='display: none;'></progress>
            
            <div id='status'></div>
            
            <div class='nav-buttons'>
                <button type='button' class='btn btn-home' onclick='window.location="/"'>Home</button>
                <button type='button' id='uploadBtn' class='btn btn-primary' onclick='uploadFile()'>Upload Firmware</button>
            </div>
        </form>
    </div>
</body>
</html>
)rawliteral";

#endif // SIMPLE_OTA_PAGE_H