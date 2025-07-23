#ifndef CONFIGMANAGER_H_
#define CONFIGMANAGER_H_

#include "Arduino.h"
#include <EEPROM.h>
#include "EEPROMLayout.h"

class ConfigManager
{
private:
    static ConfigManager *instance;

    // Steer configuration (EEPROM 200-299)
    bool invertWAS;
    bool isRelayActiveHigh;
    bool motorDriveDirection;
    bool singleInputWAS;
    bool cytronDriver;
    bool steerSwitch;
    bool steerButton;
    bool shaftEncoder;
    bool isDanfoss;
    bool pressureSensor;
    bool currentSensor;
    bool isUseYAxis;
    bool pwmBrakeMode;  // false = coast mode (default), true = brake mode
    uint8_t pulseCountMax;
    uint8_t minSpeed;
    uint8_t motorDriverConfig;  // From PGN251 Byte 8

    // Steer settings (EEPROM 300-399)
    float kp;
    uint8_t highPWM;
    float lowPWM;
    uint8_t minPWM;
    uint8_t steerSensorCounts;
    int16_t wasOffset;
    float ackermanFix;

    // GPS configuration (EEPROM 400-499)
    uint32_t gpsBaudRate;
    bool gpsSyncMode;
    bool gpsPassThrough;
    uint8_t gpsProtocol;

    // Machine settings (EEPROM 500-599)
    uint8_t sectionCount;
    bool hydraulicLift;
    bool tramlineControl;
    uint16_t workWidth;
    uint8_t raiseTime;
    uint8_t lowerTime;
    bool isPinActiveHigh;

    // KWAS configuration (EEPROM 600-699)
    bool kwasEnabled;
    uint8_t kwasMode;
    float kwasGain;
    uint16_t kwasDeadband;
    uint8_t kwasFilterLevel;

    // INS configuration (EEPROM 700-1199)
    bool insEnabled;
    uint8_t insMode;
    float insHeadingOffset;
    float insRollOffset;
    float insPitchOffset;
    uint8_t insFilterLevel;
    bool insUseFusion;
    float insVarianceHeading;
    float insVarianceRoll;
    float insVariancePitch;

    // LED settings
    uint8_t ledBrightness;
    
    // Turn sensor configuration
    uint8_t turnSensorType;      // 0=None, 1=Encoder, 2=Pressure, 3=Current
    uint8_t encoderType;         // 1=Single, 2=Quadrature
    uint8_t turnMaxPulseCount;   // Max encoder pulses before kickout
    uint8_t pressureThreshold;   // Pressure sensor threshold
    uint8_t currentThreshold;    // Current sensor threshold
    uint16_t currentZeroOffset;  // Current sensor zero offset
    
    // Version control
    uint16_t eeVersion;

public:
    ConfigManager();
    ~ConfigManager();

    // Singleton access
    static ConfigManager *getInstance();
    static void init();

    // Steer configuration methods
    bool getInvertWAS() const { return invertWAS; }
    void setInvertWAS(bool value) { invertWAS = value; }
    bool getIsRelayActiveHigh() const { return isRelayActiveHigh; }
    void setIsRelayActiveHigh(bool value) { isRelayActiveHigh = value; }
    bool getMotorDriveDirection() const { return motorDriveDirection; }
    void setMotorDriveDirection(bool value) { motorDriveDirection = value; }
    bool getSingleInputWAS() const { return singleInputWAS; }
    void setSingleInputWAS(bool value) { singleInputWAS = value; }
    bool getCytronDriver() const { return cytronDriver; }
    void setCytronDriver(bool value) { cytronDriver = value; }
    bool getSteerSwitch() const { return steerSwitch; }
    void setSteerSwitch(bool value) { steerSwitch = value; }
    bool getSteerButton() const { return steerButton; }
    void setSteerButton(bool value) { steerButton = value; }
    bool getShaftEncoder() const { return shaftEncoder; }
    void setShaftEncoder(bool value) { shaftEncoder = value; }
    bool getIsDanfoss() const { return isDanfoss; }
    void setIsDanfoss(bool value) { isDanfoss = value; }
    bool getPressureSensor() const { return pressureSensor; }
    void setPressureSensor(bool value) { pressureSensor = value; }
    bool getCurrentSensor() const { return currentSensor; }
    void setCurrentSensor(bool value) { currentSensor = value; }
    bool getIsUseYAxis() const { return isUseYAxis; }
    void setIsUseYAxis(bool value) { isUseYAxis = value; }
    bool getPWMBrakeMode() const { return pwmBrakeMode; }
    void setPWMBrakeMode(bool value) { pwmBrakeMode = value; }
    uint8_t getPulseCountMax() const { return pulseCountMax; }
    void setPulseCountMax(uint8_t value) { pulseCountMax = value; }
    uint8_t getMinSpeed() const { return minSpeed; }
    void setMinSpeed(uint8_t value) { minSpeed = value; }
    uint8_t getMotorDriverConfig() const { return motorDriverConfig; }
    void setMotorDriverConfig(uint8_t value) { motorDriverConfig = value; }

    // Steer settings methods
    float getKp() const { return kp; }
    void setKp(float value) { kp = value; }
    uint8_t getHighPWM() const { return highPWM; }
    void setHighPWM(uint8_t value) { highPWM = value; }
    float getLowPWM() const { return lowPWM; }
    void setLowPWM(float value) { lowPWM = value; }
    uint8_t getMinPWM() const { return minPWM; }
    void setMinPWM(uint8_t value) { minPWM = value; }
    uint8_t getSteerSensorCounts() const { return steerSensorCounts; }
    void setSteerSensorCounts(uint8_t value) { steerSensorCounts = value; }
    int16_t getWasOffset() const { return wasOffset; }
    void setWasOffset(int16_t value) { wasOffset = value; }
    float getAckermanFix() const { return ackermanFix; }
    void setAckermanFix(float value) { ackermanFix = value; }

    // LED configuration
    uint8_t getLEDBrightness() const { return ledBrightness; }
    void setLEDBrightness(uint8_t value) { 
        ledBrightness = constrain(value, 5, 100); 
    }
    
    // GPS configuration methods
    uint32_t getGPSBaudRate() const { return gpsBaudRate; }
    void setGPSBaudRate(uint32_t value) { gpsBaudRate = value; }
    bool getGPSSyncMode() const { return gpsSyncMode; }
    void setGPSSyncMode(bool value) { gpsSyncMode = value; }
    bool getGPSPassThrough() const { return gpsPassThrough; }
    void setGPSPassThrough(bool value) { gpsPassThrough = value; }
    uint8_t getGPSProtocol() const { return gpsProtocol; }
    void setGPSProtocol(uint8_t value) { gpsProtocol = value; }

    // Machine configuration methods
    uint8_t getSectionCount() const { return sectionCount; }
    void setSectionCount(uint8_t value) { sectionCount = value; }
    bool getHydraulicLift() const { return hydraulicLift; }
    void setHydraulicLift(bool value) { hydraulicLift = value; }
    bool getTramlineControl() const { return tramlineControl; }
    void setTramlineControl(bool value) { tramlineControl = value; }
    uint16_t getWorkWidth() const { return workWidth; }
    void setWorkWidth(uint16_t value) { workWidth = value; }
    uint8_t getRaiseTime() const { return raiseTime; }
    void setRaiseTime(uint8_t value) { raiseTime = value; }
    uint8_t getLowerTime() const { return lowerTime; }
    void setLowerTime(uint8_t value) { lowerTime = value; }
    bool getIsPinActiveHigh() const { return isPinActiveHigh; }
    void setIsPinActiveHigh(bool value) { isPinActiveHigh = value; }

    // KWAS configuration methods
    bool getKWASEnabled() const { return kwasEnabled; }
    void setKWASEnabled(bool value) { kwasEnabled = value; }
    uint8_t getKWASMode() const { return kwasMode; }
    void setKWASMode(uint8_t value) { kwasMode = value; }
    float getKWASGain() const { return kwasGain; }
    void setKWASGain(float value) { kwasGain = value; }
    uint16_t getKWASDeadband() const { return kwasDeadband; }
    void setKWASDeadband(uint16_t value) { kwasDeadband = value; }
    uint8_t getKWASFilterLevel() const { return kwasFilterLevel; }
    void setKWASFilterLevel(uint8_t value) { kwasFilterLevel = value; }

    // INS configuration methods
    bool getINSEnabled() const { return insEnabled; }
    void setINSEnabled(bool value) { insEnabled = value; }
    uint8_t getINSMode() const { return insMode; }
    void setINSMode(uint8_t value) { insMode = value; }
    float getINSHeadingOffset() const { return insHeadingOffset; }
    void setINSHeadingOffset(float value) { insHeadingOffset = value; }
    float getINSRollOffset() const { return insRollOffset; }
    void setINSRollOffset(float value) { insRollOffset = value; }
    float getINSPitchOffset() const { return insPitchOffset; }
    void setINSPitchOffset(float value) { insPitchOffset = value; }
    uint8_t getINSFilterLevel() const { return insFilterLevel; }
    void setINSFilterLevel(uint8_t value) { insFilterLevel = value; }
    bool getINSUseFusion() const { return insUseFusion; }
    void setINSUseFusion(bool value) { insUseFusion = value; }
    float getINSVarianceHeading() const { return insVarianceHeading; }
    void setINSVarianceHeading(float value) { insVarianceHeading = value; }
    float getINSVarianceRoll() const { return insVarianceRoll; }
    void setINSVarianceRoll(float value) { insVarianceRoll = value; }
    float getINSVariancePitch() const { return insVariancePitch; }
    void setINSVariancePitch(float value) { insVariancePitch = value; }

    // Turn sensor configuration methods
    uint8_t getTurnSensorType() const { return turnSensorType; }
    void setTurnSensorType(uint8_t value) { turnSensorType = value; }
    uint8_t getEncoderType() const { return encoderType; }
    void setEncoderType(uint8_t value) { encoderType = value; }
    uint8_t getTurnMaxPulseCount() const { return turnMaxPulseCount; }
    void setTurnMaxPulseCount(uint8_t value) { turnMaxPulseCount = value; }
    uint8_t getPressureThreshold() const { return pressureThreshold; }
    void setPressureThreshold(uint8_t value) { pressureThreshold = value; }
    uint8_t getCurrentThreshold() const { return currentThreshold; }
    void setCurrentThreshold(uint8_t value) { currentThreshold = value; }
    uint16_t getCurrentZeroOffset() const { return currentZeroOffset; }
    void setCurrentZeroOffset(uint16_t value) { currentZeroOffset = value; }

    // EEPROM operations
    void saveSteerConfig();
    void loadSteerConfig();
    void saveSteerSettings();
    void loadSteerSettings();
    void saveGPSConfig();
    void loadGPSConfig();
    void saveMachineConfig();
    void loadMachineConfig();
    void saveKWASConfig();
    void loadKWASConfig();
    void saveINSConfig();
    void loadINSConfig();
    void saveTurnSensorConfig();
    void loadTurnSensorConfig();
    void loadAllConfigs();
    void saveAllConfigs();
    void resetToDefaults();
    bool checkVersion();
    void updateVersion();
};

#endif // CONFIGMANAGER_H_