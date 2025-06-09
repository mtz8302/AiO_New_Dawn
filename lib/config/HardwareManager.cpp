#include "HardwareManager.h"
#include "pcb.h"

// Static instance pointer
HardwareManager *HardwareManager::instance = nullptr;

HardwareManager::HardwareManager()
{
    instance = this;
    isInitialized = false;
    pwmFrequencyMode = 4; // Default to 18310Hz

    // Initialize buffer pointers to nullptr
    gps1RxBuffer = nullptr;
    gps1TxBuffer = nullptr;
    gps2RxBuffer = nullptr;
    gps2TxBuffer = nullptr;
    rtkRxBuffer = nullptr;
    rs232TxBuffer = nullptr;
    esp32RxBuffer = nullptr;
    esp32TxBuffer = nullptr;
}

HardwareManager::~HardwareManager()
{
    deallocateSerialBuffers();
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

bool HardwareManager::initializeHardware()
{
    Serial.print("\r\n\nHardware Manager initialization");

    bool success = true;

    success &= allocateSerialBuffers();
    success &= initializePins();
    success &= initializeSerial();
    success &= initializePWM();
    success &= initializeADC();

    if (success)
    {
        isInitialized = true;
        Serial.print("\r\n- Hardware initialization complete");
    }
    else
    {
        Serial.print("\r\n- ** Hardware initialization FAILED **");
    }

    return success;
}

bool HardwareManager::initializePins()
{
    Serial.print("\r\n- Configuring pins");

    configureDigitalPins();
    configureAnalogPins();
    configurePWMPins();

    return true;
}

bool HardwareManager::initializeSerial()
{
    Serial.print("\r\n- Initializing serial ports");

    // GPS1 Serial
    SerialGPS1.begin(BAUD_GPS);
    SerialGPS1.addMemoryForRead(gps1RxBuffer, GPS_BUFFER_SIZE);
    SerialGPS1.addMemoryForWrite(gps1TxBuffer, GPS_BUFFER_SIZE);

    // GPS2 Serial
    SerialGPS2.begin(BAUD_GPS);
    SerialGPS2.addMemoryForRead(gps2RxBuffer, GPS_BUFFER_SIZE);
    SerialGPS2.addMemoryForWrite(gps2TxBuffer, GPS_BUFFER_SIZE);

    // RTK Radio Serial
    SerialRTK.begin(BAUD_RTK);
    SerialRTK.addMemoryForRead(rtkRxBuffer, RTK_BUFFER_SIZE);

    // RS232 Serial
    SerialRS232.begin(BAUD_RS232);
    SerialRS232.addMemoryForWrite(rs232TxBuffer, RS232_BUFFER_SIZE);

    // ESP32 Serial
    SerialESP32.begin(BAUD_ESP32);
    SerialESP32.addMemoryForRead(esp32RxBuffer, ESP32_BUFFER_SIZE);
    SerialESP32.addMemoryForWrite(esp32TxBuffer, ESP32_BUFFER_SIZE);

    // IMU Serial
    SerialIMU->begin(BAUD_IMU);

    Serial.printf("\r\n  - SerialGPS1/GPS2: %i baud", BAUD_GPS);
    Serial.printf("\r\n  - SerialRTK: %i baud", BAUD_RTK);
    Serial.printf("\r\n  - SerialRS232: %i baud", BAUD_RS232);
    Serial.printf("\r\n  - SerialESP32: %i baud", BAUD_ESP32);
    Serial.printf("\r\n  - SerialIMU: %i baud", BAUD_IMU);

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
    printSerialConfiguration();
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

void HardwareManager::printSerialConfiguration()
{
    Serial.print("\r\n\n--- Serial Configuration ---");
    Serial.printf("\r\nSerialGPS1 (Serial5): %i baud", BAUD_GPS);
    Serial.printf("\r\nSerialGPS2 (Serial8): %i baud", BAUD_GPS);
    Serial.printf("\r\nSerialRTK (Serial3): %i baud", BAUD_RTK);
    Serial.printf("\r\nSerialRS232 (Serial7): %i baud", BAUD_RS232);
    Serial.printf("\r\nSerialESP32 (Serial2): %i baud", BAUD_ESP32);
    Serial.printf("\r\nSerialIMU (Serial4): %i baud", BAUD_IMU);
}

bool HardwareManager::allocateSerialBuffers()
{
    Serial.print("\r\n- Allocating serial buffers");

    // Allocate GPS1 buffers
    gps1RxBuffer = new uint8_t[GPS_BUFFER_SIZE];
    gps1TxBuffer = new uint8_t[GPS_BUFFER_SIZE];

    // Allocate GPS2 buffers
    gps2RxBuffer = new uint8_t[GPS_BUFFER_SIZE];
    gps2TxBuffer = new uint8_t[GPS_BUFFER_SIZE];

    // Allocate RTK buffer
    rtkRxBuffer = new uint8_t[RTK_BUFFER_SIZE];

    // Allocate RS232 buffer
    rs232TxBuffer = new uint8_t[RS232_BUFFER_SIZE];

    // Allocate ESP32 buffers
    esp32RxBuffer = new uint8_t[ESP32_BUFFER_SIZE];
    esp32TxBuffer = new uint8_t[ESP32_BUFFER_SIZE];

    // Check if all allocations succeeded
    if (!gps1RxBuffer || !gps1TxBuffer || !gps2RxBuffer || !gps2TxBuffer ||
        !rtkRxBuffer || !rs232TxBuffer || !esp32RxBuffer || !esp32TxBuffer)
    {
        Serial.print("\r\n  - ** Buffer allocation FAILED **");
        deallocateSerialBuffers();
        return false;
    }

    Serial.printf("\r\n  - GPS buffers: %i bytes each", GPS_BUFFER_SIZE);
    Serial.printf("\r\n  - RTK buffer: %i bytes", RTK_BUFFER_SIZE);
    Serial.printf("\r\n  - RS232 buffer: %i bytes", RS232_BUFFER_SIZE);
    Serial.printf("\r\n  - ESP32 buffers: %i bytes each", ESP32_BUFFER_SIZE);

    return true;
}

void HardwareManager::deallocateSerialBuffers()
{
    delete[] gps1RxBuffer;
    gps1RxBuffer = nullptr;
    delete[] gps1TxBuffer;
    gps1TxBuffer = nullptr;
    delete[] gps2RxBuffer;
    gps2RxBuffer = nullptr;
    delete[] gps2TxBuffer;
    gps2TxBuffer = nullptr;
    delete[] rtkRxBuffer;
    rtkRxBuffer = nullptr;
    delete[] rs232TxBuffer;
    rs232TxBuffer = nullptr;
    delete[] esp32RxBuffer;
    esp32RxBuffer = nullptr;
    delete[] esp32TxBuffer;
    esp32TxBuffer = nullptr;
}

void HardwareManager::configureDigitalPins()
{
    // Buzzer output
    pinMode(getBuzzerPin(), OUTPUT);
    digitalWrite(getBuzzerPin(), LOW);

    // Motor sleep control
    pinMode(getSleepPin(), OUTPUT);
    digitalWrite(getSleepPin(), LOW); // Keep motor asleep initially

    // Input pins with pullups
    pinMode(getSteerPin(), INPUT_PULLUP);
    pinMode(getKickoutDPin(), INPUT_PULLUP);
}

void HardwareManager::configureAnalogPins()
{
    // Disable pullup/pulldown resistors for analog inputs
    pinMode(getWASSensorPin(), INPUT_DISABLE);
    pinMode(getWorkPin(), INPUT_DISABLE);
    pinMode(getCurrentPin(), INPUT_DISABLE);
    pinMode(getKickoutAPin(), INPUT_DISABLE);
}

void HardwareManager::configurePWMPins()
{
    // PWM pins are configured automatically when analogWrite is called
    // Just ensure they start at 0
    analogWrite(getPWM1Pin(), 0);
    analogWrite(getPWM2Pin(), 0);
}