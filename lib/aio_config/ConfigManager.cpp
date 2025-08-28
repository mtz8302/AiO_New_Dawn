#include "ConfigManager.h"
#include "EventLogger.h"

// Use shared EEPROM version from EEPROMLayout.h
#define CURRENT_EE_VERSION EEPROM_VERSION

// Static instance pointer
ConfigManager *ConfigManager::instance = nullptr;

ConfigManager::ConfigManager()
{
    instance = this;
    resetToDefaults();
    if (checkVersion())
    {
        LOG_INFO(EventSource::CONFIG, "Version match - loading saved configs");
        loadAllConfigs();
        LOG_DEBUG(EventSource::CONFIG, "Loaded steerButton = %d, steerSwitch = %d", 
                      steerButton, steerSwitch);
    }
    else
    {
        LOG_WARNING(EventSource::CONFIG, "Version mismatch - using defaults");
        saveAllConfigs();
        updateVersion();
    }
}

ConfigManager::~ConfigManager()
{
    instance = nullptr;
}

ConfigManager *ConfigManager::getInstance()
{
    return instance;
}

void ConfigManager::init()
{
    if (instance == nullptr)
    {
        new ConfigManager();
    }
}

// EEPROM operations
void ConfigManager::saveSteerConfig()
{
    // Pack boolean values into bytes for efficient storage
    uint8_t configByte1 = 0;
    uint8_t configByte2 = 0;

    if (invertWAS)
        configByte1 |= 0x01;
    if (isRelayActiveHigh)
        configByte1 |= 0x02;
    if (motorDriveDirection)
        configByte1 |= 0x04;
    if (singleInputWAS)
        configByte1 |= 0x08;
    if (cytronDriver)
        configByte1 |= 0x10;
    if (steerSwitch)
        configByte1 |= 0x20;
    if (steerButton)
        configByte1 |= 0x40;
    if (shaftEncoder)
        configByte1 |= 0x80;

    if (isDanfoss)
        configByte2 |= 0x01;
    if (pressureSensor)
        configByte2 |= 0x02;
    if (currentSensor)
        configByte2 |= 0x04;
    if (isUseYAxis)
        configByte2 |= 0x08;
    if (pwmBrakeMode)
        configByte2 |= 0x10;

    LOG_DEBUG(EventSource::CONFIG, "Saving steer config: button=%d, switch=%d, byte1=0x%02X", 
                  steerButton, steerSwitch, configByte1);

    int addr = STEER_CONFIG_ADDR;
    EEPROM.put(addr, configByte1);
    addr += sizeof(configByte1);
    EEPROM.put(addr, configByte2);
    addr += sizeof(configByte2);
    EEPROM.put(addr, pulseCountMax);
    addr += sizeof(pulseCountMax);
    EEPROM.put(addr, minSpeed);
    addr += sizeof(minSpeed);
    EEPROM.put(addr, motorDriverConfig);
    
    // Verify the write
    uint8_t verifyByte1;
    EEPROM.get(STEER_CONFIG_ADDR, verifyByte1);
    LOG_DEBUG(EventSource::CONFIG, "Steer config verification: wrote=0x%02X, read=0x%02X", 
                  configByte1, verifyByte1);
}

void ConfigManager::loadSteerConfig()
{
    uint8_t configByte1, configByte2;

    int addr = STEER_CONFIG_ADDR;
    EEPROM.get(addr, configByte1);
    addr += sizeof(configByte1);
    EEPROM.get(addr, configByte2);
    addr += sizeof(configByte2);
    EEPROM.get(addr, pulseCountMax);
    addr += sizeof(pulseCountMax);
    EEPROM.get(addr, minSpeed);
    addr += sizeof(minSpeed);
    EEPROM.get(addr, motorDriverConfig);

    // Unpack boolean values
    invertWAS = (configByte1 & 0x01) != 0;
    isRelayActiveHigh = (configByte1 & 0x02) != 0;
    motorDriveDirection = (configByte1 & 0x04) != 0;
    singleInputWAS = (configByte1 & 0x08) != 0;
    cytronDriver = (configByte1 & 0x10) != 0;
    steerSwitch = (configByte1 & 0x20) != 0;
    steerButton = (configByte1 & 0x40) != 0;
    shaftEncoder = (configByte1 & 0x80) != 0;

    isDanfoss = (configByte2 & 0x01) != 0;
    pressureSensor = (configByte2 & 0x02) != 0;
    currentSensor = (configByte2 & 0x04) != 0;
    isUseYAxis = (configByte2 & 0x08) != 0;
    pwmBrakeMode = (configByte2 & 0x10) != 0;
}

void ConfigManager::saveSteerSettings()
{
    LOG_DEBUG(EventSource::CONFIG, "Saving steer settings: Kp=%.1f, High=%d, Low=%.1f, Min=%d",
                  kp, highPWM, lowPWM, minPWM);
    
    int addr = STEER_SETTINGS_ADDR;
    EEPROM.put(addr, kp);
    addr += sizeof(kp);
    EEPROM.put(addr, highPWM);
    addr += sizeof(highPWM);
    EEPROM.put(addr, lowPWM);
    addr += sizeof(lowPWM);
    EEPROM.put(addr, minPWM);
    addr += sizeof(minPWM);
    EEPROM.put(addr, steerSensorCounts);
    addr += sizeof(steerSensorCounts);
    EEPROM.put(addr, wasOffset);
    addr += sizeof(wasOffset);
    EEPROM.put(addr, ackermanFix);
    
    // Verify the save
    uint8_t verifyHighPWM;
    EEPROM.get(STEER_SETTINGS_ADDR + sizeof(kp), verifyHighPWM);
    LOG_DEBUG(EventSource::CONFIG, "Steer settings verification: saved highPWM=%d, read back=%d",
                  highPWM, verifyHighPWM);
}

void ConfigManager::loadSteerSettings()
{
    int addr = STEER_SETTINGS_ADDR;
    EEPROM.get(addr, kp);
    addr += sizeof(kp);
    EEPROM.get(addr, highPWM);
    addr += sizeof(highPWM);
    EEPROM.get(addr, lowPWM);
    addr += sizeof(lowPWM);
    EEPROM.get(addr, minPWM);
    addr += sizeof(minPWM);
    EEPROM.get(addr, steerSensorCounts);
    addr += sizeof(steerSensorCounts);
    EEPROM.get(addr, wasOffset);
    addr += sizeof(wasOffset);
    EEPROM.get(addr, ackermanFix);
    
    LOG_DEBUG(EventSource::CONFIG, "Loaded steer settings: Kp=%.1f, High=%d, Low=%.1f, Min=%d",
                  kp, highPWM, lowPWM, minPWM);
}

void ConfigManager::saveGPSConfig()
{
    int addr = GPS_CONFIG_ADDR;
    EEPROM.put(addr, gpsBaudRate);
    addr += sizeof(gpsBaudRate);

    uint8_t gpsConfigByte = 0;
    if (gpsSyncMode)
        gpsConfigByte |= 0x01;
    if (gpsPassThrough)
        gpsConfigByte |= 0x02;

    EEPROM.put(addr, gpsConfigByte);
    addr += sizeof(gpsConfigByte);
    EEPROM.put(addr, gpsProtocol);
}

void ConfigManager::loadGPSConfig()
{
    int addr = GPS_CONFIG_ADDR;
    EEPROM.get(addr, gpsBaudRate);
    addr += sizeof(gpsBaudRate);

    uint8_t gpsConfigByte;
    EEPROM.get(addr, gpsConfigByte);
    addr += sizeof(gpsConfigByte);
    EEPROM.get(addr, gpsProtocol);

    gpsSyncMode = (gpsConfigByte & 0x01) != 0;
    gpsPassThrough = (gpsConfigByte & 0x02) != 0;
}

void ConfigManager::saveMachineConfig()
{
    int addr = MACHINE_CONFIG_ADDR;
    EEPROM.put(addr, sectionCount);
    addr += sizeof(sectionCount);

    uint8_t machineConfigByte = 0;
    if (hydraulicLift)
        machineConfigByte |= 0x01;
    if (tramlineControl)
        machineConfigByte |= 0x02;
    if (isPinActiveHigh)
        machineConfigByte |= 0x04;

    EEPROM.put(addr, machineConfigByte);
    addr += sizeof(machineConfigByte);
    EEPROM.put(addr, workWidth);
    addr += sizeof(workWidth);
    EEPROM.put(addr, raiseTime);
    addr += sizeof(raiseTime);
    EEPROM.put(addr, lowerTime);
    addr += sizeof(lowerTime);
    EEPROM.put(addr, user1);
    addr += sizeof(user1);
    EEPROM.put(addr, user2);
    addr += sizeof(user2);
    EEPROM.put(addr, user3);
    addr += sizeof(user3);
    EEPROM.put(addr, user4);
    addr += sizeof(user4);
}

void ConfigManager::loadMachineConfig()
{
    int addr = MACHINE_CONFIG_ADDR;
    EEPROM.get(addr, sectionCount);
    addr += sizeof(sectionCount);

    uint8_t machineConfigByte;
    EEPROM.get(addr, machineConfigByte);
    addr += sizeof(machineConfigByte);
    EEPROM.get(addr, workWidth);
    addr += sizeof(workWidth);
    EEPROM.get(addr, raiseTime);
    addr += sizeof(raiseTime);
    EEPROM.get(addr, lowerTime);
    addr += sizeof(lowerTime);
    EEPROM.get(addr, user1);
    addr += sizeof(user1);
    EEPROM.get(addr, user2);
    addr += sizeof(user2);
    EEPROM.get(addr, user3);
    addr += sizeof(user3);
    EEPROM.get(addr, user4);

    hydraulicLift = (machineConfigByte & 0x01) != 0;
    tramlineControl = (machineConfigByte & 0x02) != 0;
    isPinActiveHigh = (machineConfigByte & 0x04) != 0;
}

void ConfigManager::saveKWASConfig()
{
    int addr = KWAS_CONFIG_ADDR;

    uint8_t kwasConfigByte = 0;
    if (kwasEnabled)
        kwasConfigByte |= 0x01;

    EEPROM.put(addr, kwasConfigByte);
    addr += sizeof(kwasConfigByte);
    EEPROM.put(addr, kwasMode);
    addr += sizeof(kwasMode);
    EEPROM.put(addr, kwasGain);
    addr += sizeof(kwasGain);
    EEPROM.put(addr, kwasDeadband);
    addr += sizeof(kwasDeadband);
    EEPROM.put(addr, kwasFilterLevel);
}

void ConfigManager::loadKWASConfig()
{
    int addr = KWAS_CONFIG_ADDR;

    uint8_t kwasConfigByte;
    EEPROM.get(addr, kwasConfigByte);
    addr += sizeof(kwasConfigByte);
    EEPROM.get(addr, kwasMode);
    addr += sizeof(kwasMode);
    EEPROM.get(addr, kwasGain);
    addr += sizeof(kwasGain);
    EEPROM.get(addr, kwasDeadband);
    addr += sizeof(kwasDeadband);
    EEPROM.get(addr, kwasFilterLevel);

    kwasEnabled = (kwasConfigByte & 0x01) != 0;
}

void ConfigManager::saveINSConfig()
{
    int addr = INS_CONFIG_ADDR;

    uint8_t insConfigByte = 0;
    if (insEnabled)
        insConfigByte |= 0x01;
    if (insUseFusion)
        insConfigByte |= 0x02;

    EEPROM.put(addr, insConfigByte);
    addr += sizeof(insConfigByte);
    EEPROM.put(addr, insMode);
    addr += sizeof(insMode);
    EEPROM.put(addr, insHeadingOffset);
    addr += sizeof(insHeadingOffset);
    EEPROM.put(addr, insRollOffset);
    addr += sizeof(insRollOffset);
    EEPROM.put(addr, insPitchOffset);
    addr += sizeof(insPitchOffset);
    EEPROM.put(addr, insFilterLevel);
    addr += sizeof(insFilterLevel);
    EEPROM.put(addr, insVarianceHeading);
    addr += sizeof(insVarianceHeading);
    EEPROM.put(addr, insVarianceRoll);
    addr += sizeof(insVarianceRoll);
    EEPROM.put(addr, insVariancePitch);
}

void ConfigManager::loadINSConfig()
{
    int addr = INS_CONFIG_ADDR;

    uint8_t insConfigByte;
    EEPROM.get(addr, insConfigByte);
    addr += sizeof(insConfigByte);
    EEPROM.get(addr, insMode);
    addr += sizeof(insMode);
    EEPROM.get(addr, insHeadingOffset);
    addr += sizeof(insHeadingOffset);
    EEPROM.get(addr, insRollOffset);
    addr += sizeof(insRollOffset);
    EEPROM.get(addr, insPitchOffset);
    addr += sizeof(insPitchOffset);
    EEPROM.get(addr, insFilterLevel);
    addr += sizeof(insFilterLevel);
    EEPROM.get(addr, insVarianceHeading);
    addr += sizeof(insVarianceHeading);
    EEPROM.get(addr, insVarianceRoll);
    addr += sizeof(insVarianceRoll);
    EEPROM.get(addr, insVariancePitch);

    insEnabled = (insConfigByte & 0x01) != 0;
    insUseFusion = (insConfigByte & 0x02) != 0;
}

void ConfigManager::loadAllConfigs()
{
    loadSteerConfig();
    loadSteerSettings();
    loadGPSConfig();
    loadMachineConfig();
    loadKWASConfig();
    loadINSConfig();
    loadTurnSensorConfig();
    loadAnalogWorkSwitchConfig();
}

void ConfigManager::saveAllConfigs()
{
    saveSteerConfig();
    saveSteerSettings();
    saveGPSConfig();
    saveMachineConfig();
    saveKWASConfig();
    saveINSConfig();
    saveTurnSensorConfig();
    saveAnalogWorkSwitchConfig();
}

void ConfigManager::resetToDefaults()
{
    // Steer config defaults
    invertWAS = false;
    isRelayActiveHigh = false;
    motorDriveDirection = false;
    singleInputWAS = false;
    cytronDriver = false;
    steerSwitch = false;
    steerButton = false;
    shaftEncoder = false;
    isDanfoss = false;
    pressureSensor = false;
    currentSensor = false;
    isUseYAxis = false;
    pwmBrakeMode = false;  // Default to coast mode
    pulseCountMax = 5;
    minSpeed = 3;
    motorDriverConfig = 0x00;  // Default to DRV8701 with wheel encoder

    // Steer settings defaults
    kp = 40.0;
    highPWM = 255;
    lowPWM = 30.0;
    minPWM = 10;
    steerSensorCounts = 30;
    wasOffset = 0;
    ackermanFix = 1.0;

    // GPS config defaults
    gpsBaudRate = 460800; // From pcb.h
    gpsSyncMode = false;
    gpsPassThrough = false;
    gpsProtocol = 0;

    // Machine config defaults
    sectionCount = 8;
    hydraulicLift = false;
    tramlineControl = false;
    workWidth = 1200; // 12 meters in cm
    raiseTime = 2;
    lowerTime = 4;
    isPinActiveHigh = false;
    user1 = 0;
    user2 = 0;
    user3 = 0;
    user4 = 0;

    // KWAS config defaults
    kwasEnabled = false;
    kwasMode = 0;
    kwasGain = 1.0;
    kwasDeadband = 50;
    kwasFilterLevel = 3;

    // INS config defaults
    insEnabled = false;
    insMode = 0;
    insHeadingOffset = 0.0;
    insRollOffset = 0.0;
    insPitchOffset = 0.0;
    insFilterLevel = 3;
    insUseFusion = false;
    insVarianceHeading = 1.0;
    insVarianceRoll = 1.0;
    insVariancePitch = 1.0;
    
    // LED defaults
    ledBrightness = 25;  // 25% default brightness
    
    // Turn sensor defaults
    turnSensorType = 0;      // None
    encoderType = 1;         // Single channel
    turnMaxPulseCount = 5;   // Same as pulseCountMax default
    pressureThreshold = 100; // Middle of range
    currentThreshold = 100;  // Middle of range
    currentZeroOffset = 90;  // From NG-V6 code
    
    // Analog work switch defaults
    analogWorkSwitchEnabled = false;
    workSwitchSetpoint = 50;     // 50%
    workSwitchHysteresis = 20;   // 20%
    invertWorkSwitch = false;

    eeVersion = CURRENT_EE_VERSION;
}

bool ConfigManager::checkVersion()
{
    uint16_t storedVersion;
    EEPROM.get(EE_VERSION_ADDR, storedVersion);
    LOG_DEBUG(EventSource::CONFIG, "EEPROM version check: stored=%d, current=%d", 
                  storedVersion, CURRENT_EE_VERSION);
    
    // If EEPROM is uninitialized (0 or 0xFFFF), initialize it
    if (storedVersion == 0 || storedVersion == 0xFFFF) {
        LOG_INFO(EventSource::CONFIG, "EEPROM appears uninitialized, performing first-time setup");
        return false;  // This will trigger saveAllConfigs() and updateVersion()
    }
    
    return (storedVersion == CURRENT_EE_VERSION);
}

void ConfigManager::updateVersion()
{
    LOG_DEBUG(EventSource::CONFIG, "Writing version %d to EEPROM address %d", 
                  CURRENT_EE_VERSION, EE_VERSION_ADDR);
    EEPROM.put(EE_VERSION_ADDR, (uint16_t)CURRENT_EE_VERSION);
    
    // Verify the write
    uint16_t verifyVersion;
    EEPROM.get(EE_VERSION_ADDR, verifyVersion);
    LOG_DEBUG(EventSource::CONFIG, "Version write verification: wrote=%d, read back=%d", 
                  CURRENT_EE_VERSION, verifyVersion);
}

void ConfigManager::saveTurnSensorConfig()
{
    LOG_DEBUG(EventSource::CONFIG, "Saving turn sensor config: Type=%d, EncoderType=%d", 
              turnSensorType, encoderType);
    
    int addr = TURN_SENSOR_CONFIG_ADDR;
    EEPROM.put(addr, turnSensorType);
    addr += sizeof(turnSensorType);
    EEPROM.put(addr, encoderType);
    addr += sizeof(encoderType);
    EEPROM.put(addr, turnMaxPulseCount);
    addr += sizeof(turnMaxPulseCount);
    EEPROM.put(addr, pressureThreshold);
    addr += sizeof(pressureThreshold);
    EEPROM.put(addr, currentThreshold);
    addr += sizeof(currentThreshold);
    EEPROM.put(addr, currentZeroOffset);
}

void ConfigManager::loadTurnSensorConfig()
{
    int addr = TURN_SENSOR_CONFIG_ADDR;
    EEPROM.get(addr, turnSensorType);
    addr += sizeof(turnSensorType);
    EEPROM.get(addr, encoderType);
    addr += sizeof(encoderType);
    EEPROM.get(addr, turnMaxPulseCount);
    addr += sizeof(turnMaxPulseCount);
    EEPROM.get(addr, pressureThreshold);
    addr += sizeof(pressureThreshold);
    EEPROM.get(addr, currentThreshold);
    addr += sizeof(currentThreshold);
    EEPROM.get(addr, currentZeroOffset);
    
    LOG_DEBUG(EventSource::CONFIG, "Loaded turn sensor config: Type=%d, EncoderType=%d", 
              turnSensorType, encoderType);
}

void ConfigManager::saveAnalogWorkSwitchConfig()
{
    LOG_INFO(EventSource::CONFIG, "Saving analog work switch config to EEPROM: Enabled=%d, SP=%d%%, H=%d%%, Inv=%d", 
             analogWorkSwitchEnabled, workSwitchSetpoint, workSwitchHysteresis, invertWorkSwitch);
    
    int addr = ANALOG_WORK_SWITCH_ADDR;
    EEPROM.put(addr, analogWorkSwitchEnabled);
    addr += sizeof(analogWorkSwitchEnabled);
    EEPROM.put(addr, workSwitchSetpoint);
    addr += sizeof(workSwitchSetpoint);
    EEPROM.put(addr, workSwitchHysteresis);
    addr += sizeof(workSwitchHysteresis);
    EEPROM.put(addr, invertWorkSwitch);
}

void ConfigManager::loadAnalogWorkSwitchConfig()
{
    int addr = ANALOG_WORK_SWITCH_ADDR;
    EEPROM.get(addr, analogWorkSwitchEnabled);
    addr += sizeof(analogWorkSwitchEnabled);
    EEPROM.get(addr, workSwitchSetpoint);
    addr += sizeof(workSwitchSetpoint);
    EEPROM.get(addr, workSwitchHysteresis);
    addr += sizeof(workSwitchHysteresis);
    EEPROM.get(addr, invertWorkSwitch);
    
    // Validate loaded values
    if (workSwitchSetpoint > 100) {
        workSwitchSetpoint = 50;  // Default
    }
    if (workSwitchHysteresis < 5 || workSwitchHysteresis > 25) {
        workSwitchHysteresis = 20;  // Default
    }
    
    LOG_INFO(EventSource::CONFIG, "Loaded analog work switch config from EEPROM: Enabled=%d, SP=%d%%, H=%d%%, Inv=%d", 
             analogWorkSwitchEnabled, workSwitchSetpoint, workSwitchHysteresis, invertWorkSwitch);
}