#include "HardwareManager.h"
#include "EventLogger.h"

// Static instance pointer
HardwareManager *HardwareManager::instance = nullptr;

HardwareManager::HardwareManager()
    : isInitialized(false), pwmFrequencyMode(4)
{
    instance = this;
}

HardwareManager::~HardwareManager()
{
    instance = nullptr;
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
    LOG_INFO(EventSource::SYSTEM, "Hardware Manager Initialization starting");

    if (!initializePins())
    {
        LOG_ERROR(EventSource::SYSTEM, "Pin initialization FAILED");
        return false;
    }

    if (!initializePWM())
    {
        LOG_ERROR(EventSource::SYSTEM, "PWM initialization FAILED");
        return false;
    }

    if (!initializeADC())
    {
        LOG_ERROR(EventSource::SYSTEM, "ADC initialization FAILED");
        return false;
    }

    isInitialized = true;
    LOG_INFO(EventSource::SYSTEM, "Hardware initialization SUCCESS");
    return true;
}

bool HardwareManager::initializePins()
{
    LOG_DEBUG(EventSource::SYSTEM, "Configuring pins");

    // Configure output pins
    pinMode(getBuzzerPin(), OUTPUT);
    pinMode(getSleepPin(), OUTPUT);
    pinMode(getPWM1Pin(), OUTPUT);
    pinMode(getPWM2Pin(), OUTPUT);

    // Configure input pins
    // pinMode(getSteerPin(), INPUT_PULLUP);  // Handled by ADProcessor
    pinMode(getKickoutDPin(), INPUT_PULLUP);
    pinMode(getWASSensorPin(), INPUT_DISABLE);
    // pinMode(getWorkPin(), INPUT);  // Handled by ADProcessor
    pinMode(getCurrentPin(), INPUT);
    pinMode(getKickoutAPin(), INPUT);

    // Initialize output states
    digitalWrite(getBuzzerPin(), LOW);
    digitalWrite(getSleepPin(), LOW);
    analogWrite(getPWM1Pin(), 0);
    analogWrite(getPWM2Pin(), 0);

    LOG_DEBUG(EventSource::SYSTEM, "Pin configuration complete");
    return true;
}

bool HardwareManager::initializePWM()
{
    LOG_DEBUG(EventSource::SYSTEM, "Configuring PWM");
    return setPWMFrequency(pwmFrequencyMode);
}

bool HardwareManager::initializeADC()
{
    LOG_DEBUG(EventSource::SYSTEM, "Configuring ADC");

    // Comment out to use Teensy defaults like the test sketch
    //analogReadResolution(12);
    //analogReadAveraging(1);  // No averaging

    LOG_DEBUG(EventSource::SYSTEM, "ADC: Using Teensy defaults");
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

    LOG_DEBUG(EventSource::SYSTEM, "PWM frequency: %i Hz (mode %i)", frequency, pwmFrequencyMode);

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
    LOG_INFO(EventSource::CONFIG, "=== Hardware Manager Status ===");
    LOG_INFO(EventSource::CONFIG, "Initialized: %s", isInitialized ? "YES" : "NO");
    LOG_INFO(EventSource::CONFIG, "CPU Frequency: %i MHz", F_CPU_ACTUAL / 1000000);
    LOG_INFO(EventSource::CONFIG, "PWM Mode: %i", pwmFrequencyMode);

    printPinConfiguration();
    LOG_INFO(EventSource::CONFIG, "===============================");
}

void HardwareManager::printPinConfiguration()
{
    LOG_INFO(EventSource::CONFIG, "--- Pin Configuration ---");
    LOG_INFO(EventSource::CONFIG, "WAS Sensor: A%i", getWASSensorPin() - A0);
    LOG_INFO(EventSource::CONFIG, "Speed Pulse: %i", getSpeedPulsePin());
    LOG_INFO(EventSource::CONFIG, "Buzzer: %i", getBuzzerPin());
    LOG_INFO(EventSource::CONFIG, "Motor Sleep: %i", getSleepPin());
    LOG_INFO(EventSource::CONFIG, "PWM1: %i", getPWM1Pin());
    LOG_INFO(EventSource::CONFIG, "PWM2: %i", getPWM2Pin());
    LOG_INFO(EventSource::CONFIG, "Steer Switch: %i", getSteerPin());
    LOG_INFO(EventSource::CONFIG, "Work Input: A%i", getWorkPin() - A0);
    LOG_INFO(EventSource::CONFIG, "Kickout Digital: %i", getKickoutDPin());
    LOG_INFO(EventSource::CONFIG, "Current Sensor: A%i", getCurrentPin() - A0);
    LOG_INFO(EventSource::CONFIG, "Kickout Analog: A%i", getKickoutAPin() - A0);
}

bool HardwareManager::getInitializationStatus() const
{
    return isInitialized;
}