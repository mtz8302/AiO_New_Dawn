#include "HardwareManager.h"

// Global instance pointer
HardwareManager *hardwarePTR = nullptr;

// Static instance pointer
HardwareManager *HardwareManager::instance = nullptr;

HardwareManager::HardwareManager()
    : isInitialized(false), pwmFrequencyMode(4)
{
    instance = this;
    hardwarePTR = this;
}

HardwareManager::~HardwareManager()
{
    instance = nullptr;
    hardwarePTR = nullptr;
}

HardwareManager *HardwareManager::getInstance()
{
    return instance;
}

void HardwareManager::init()
{
    if (instance == nullptr)
    {
        new HardwareManager();
    }
}

bool HardwareManager::initialize()
{
    return initializeHardware();
}

bool HardwareManager::initializeHardware()
{
    Serial.print("\r\n=== Hardware Manager Initialization ===");

    if (!initializePins())
    {
        Serial.print("\r\n** Pin initialization FAILED **");
        return false;
    }

    if (!initializePWM())
    {
        Serial.print("\r\n** PWM initialization FAILED **");
        return false;
    }

    if (!initializeADC())
    {
        Serial.print("\r\n** ADC initialization FAILED **");
        return false;
    }

    isInitialized = true;
    Serial.print("\r\n- Hardware initialization SUCCESS");
    return true;
}

bool HardwareManager::initializePins()
{
    Serial.print("\r\n- Configuring pins");

    // Configure output pins
    pinMode(getBuzzerPin(), OUTPUT);
    pinMode(getSleepPin(), OUTPUT);
    pinMode(getPWM1Pin(), OUTPUT);
    pinMode(getPWM2Pin(), OUTPUT);

    // Configure input pins
    pinMode(getSteerPin(), INPUT_PULLUP);
    pinMode(getKickoutDPin(), INPUT_PULLUP);
    pinMode(getWASSensorPin(), INPUT_DISABLE);
    pinMode(getWorkPin(), INPUT);
    pinMode(getCurrentPin(), INPUT);
    pinMode(getKickoutAPin(), INPUT);

    // Initialize output states
    digitalWrite(getBuzzerPin(), LOW);
    digitalWrite(getSleepPin(), LOW);
    analogWrite(getPWM1Pin(), 0);
    analogWrite(getPWM2Pin(), 0);

    Serial.print("\r\n  - Pin configuration complete");
    return true;
}

bool HardwareManager::initializePWM()
{
    Serial.print("\r\n- Configuring PWM");
    return setPWMFrequency(pwmFrequencyMode);
}

bool HardwareManager::initializeADC()
{
    Serial.print("\r\n- Configuring ADC");

    analogReadResolution(12);
    analogReadAveraging(16);

    Serial.print("\r\n  - ADC: 12-bit resolution, 16x averaging");
    return true;
}

bool HardwareManager::setPWMFrequency(uint8_t mode)
{
    pwmFrequencyMode = mode;

    uint16_t frequency;
    switch (mode)
    {
    case 0:
        frequency = 490;
        break;
    case 1:
        frequency = 122;
        break;
    case 2:
        frequency = 3921;
        break;
    case 3:
        frequency = 9155;
        break;
    case 4:
        frequency = 18310;
        break;
    default:
        frequency = 18310;
        pwmFrequencyMode = 4;
        break;
    }

    analogWriteFrequency(getPWM1Pin(), frequency);
    analogWriteFrequency(getPWM2Pin(), frequency);

    Serial.printf("\r\n  - PWM frequency: %i Hz (mode %i)", frequency, pwmFrequencyMode);

    return true;
}

// Pin access methods that return the #define values from pcb.h
uint8_t HardwareManager::getWASSensorPin() const { return WAS_SENSOR_PIN; }
uint8_t HardwareManager::getSpeedPulsePin() const { return SPEEDPULSE_PIN; }
uint8_t HardwareManager::getSpeedPulse10Pin() const { return SPEEDPULSE10_PIN; }
uint8_t HardwareManager::getBuzzerPin() const { return BUZZER; }
uint8_t HardwareManager::getSleepPin() const { return SLEEP_PIN; }
uint8_t HardwareManager::getPWM1Pin() const { return PWM1_PIN; }
uint8_t HardwareManager::getPWM2Pin() const { return PWM2_PIN; }
uint8_t HardwareManager::getSteerPin() const { return STEER_PIN; }
uint8_t HardwareManager::getWorkPin() const { return WORK_PIN; }
uint8_t HardwareManager::getKickoutDPin() const { return KICKOUT_D_PIN; }
uint8_t HardwareManager::getCurrentPin() const { return CURRENT_PIN; }
uint8_t HardwareManager::getKickoutAPin() const { return KICKOUT_A_PIN; }

void HardwareManager::enableBuzzer()
{
    digitalWrite(getBuzzerPin(), HIGH);
}

void HardwareManager::disableBuzzer()
{
    digitalWrite(getBuzzerPin(), LOW);
}

void HardwareManager::enableSteerMotor()
{
    digitalWrite(getSleepPin(), HIGH);
}

void HardwareManager::disableSteerMotor()
{
    digitalWrite(getSleepPin(), LOW);
}

void HardwareManager::printHardwareStatus()
{
    Serial.print("\r\n\n=== Hardware Manager Status ===");
    Serial.printf("\r\nInitialized: %s", isInitialized ? "YES" : "NO");
    Serial.printf("\r\nCPU Frequency: %i MHz", F_CPU_ACTUAL / 1000000);
    Serial.printf("\r\nPWM Mode: %i", pwmFrequencyMode);

    printPinConfiguration();
    Serial.print("\r\n===============================\r\n");
}

void HardwareManager::printPinConfiguration()
{
    Serial.print("\r\n\n--- Pin Configuration ---");
    Serial.printf("\r\nWAS Sensor: A%i", getWASSensorPin() - A0);
    Serial.printf("\r\nSpeed Pulse: %i", getSpeedPulsePin());
    Serial.printf("\r\nBuzzer: %i", getBuzzerPin());
    Serial.printf("\r\nMotor Sleep: %i", getSleepPin());
    Serial.printf("\r\nPWM1: %i", getPWM1Pin());
    Serial.printf("\r\nPWM2: %i", getPWM2Pin());
    Serial.printf("\r\nSteer Switch: %i", getSteerPin());
    Serial.printf("\r\nWork Input: A%i", getWorkPin() - A0);
    Serial.printf("\r\nKickout Digital: %i", getKickoutDPin());
    Serial.printf("\r\nCurrent Sensor: A%i", getCurrentPin() - A0);
    Serial.printf("\r\nKickout Analog: A%i", getKickoutAPin() - A0);
}

bool HardwareManager::getInitializationStatus() const
{
    return isInitialized;
}