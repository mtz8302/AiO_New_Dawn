// en_OTAPage.h
// English version of OTA update page

#ifndef EN_OTA_PAGE_H
#define EN_OTA_PAGE_H

#include <Arduino.h>

const char EN_OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>OTA Update - AiO New Dawn</title>
    <style>%CSS_STYLES%</style>
    <script>
        function uploadFile() {
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
            
            const formData = new FormData();
            formData.append('firmware', file);
            
            const xhr = new XMLHttpRequest();
            
            xhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    progressBar.value = percentComplete;
                    statusDiv.innerHTML = 'Uploading: ' + Math.round(percentComplete) + '%';
                }
            });
            
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
            
            xhr.open('POST', '/api/ota/upload');
            xhr.send(formData);
            
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
        
        <div class='warning' style='background-color: #ffeeee; border: 1px solid #ff0000; padding: 10px; margin: 20px 0;'>
            Incorrect firmware can brick your device. Only upload firmware built for Teensy 4.1.
        </div>
        
        <form onsubmit='return uploadFile()'>
            <h2>Select Firmware File</h2>
            
            <div class='form-group'>
                <input type='file' id='file' name='firmware' accept='.hex' required>
                <div class='help-text'>
                    Select a .hex firmware file built for Teensy 4.1
                </div>
            </div>
            
            <progress id='progress' value='0' max='100' style='width: 100%; display: none; margin: 10px 0;'></progress>
            
            <div id='status' style='margin: 10px 0;'></div>
            
            <button type='submit' id='uploadBtn' class='btn btn-warning'>Upload Firmware</button>
            <button type='button' class='btn' onclick='window.location.href="/"'>Cancel</button>
        </form>
        
        <div style='margin-top: 30px; color: #666;'>
            <h3>Instructions:</h3>
            <ol>
                <li>Build your firmware in PlatformIO</li>
                <li>Locate the .hex file in .pio/build/teensy41/</li>
                <li>Select the file and click Upload Firmware</li>
                <li>Wait for upload to complete</li>
                <li>System will automatically reboot with new firmware</li>
            </ol>
        </div>
    </div>
</body>
</html>
)rawliteral";

#endif // EN_OTA_PAGE_H