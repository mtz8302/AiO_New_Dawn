#include "IMUProcessor.h"
#include "TM171MinimalParser.h"

// Global instance pointer
IMUProcessor *imuPTR = nullptr;

// Static instance pointer
IMUProcessor *IMUProcessor::instance = nullptr;

IMUProcessor::IMUProcessor()
    : serialMgr(nullptr), detectedType(IMUType::NONE), isInitialized(false),
      bno(nullptr), imuSerial(&Serial4), tm171Parser(nullptr),
      packetsReceived(0), packetsErrors(0)
{
    instance = this;
    imuPTR = this;

    // Initialize current data
    currentData = {0, 0, 0, 0, 0, 0, false};
}

IMUProcessor::~IMUProcessor()
{
    if (bno)
    {
        delete bno;
        bno = nullptr;
    }
    if (tm171Parser)
    {
        delete tm171Parser;
        tm171Parser = nullptr;
    }
    instance = nullptr;
    imuPTR = nullptr;
}

IMUProcessor *IMUProcessor::getInstance()
{
    return instance;
}

void IMUProcessor::init()
{
    if (instance == nullptr)
    {
        new IMUProcessor();
    }
}

bool IMUProcessor::initialize()
{
    Serial.print("\r\n=== IMU Processor Initialization ===");

    // Get SerialManager instance
    serialMgr = SerialManager::getInstance();
    if (!serialMgr)
    {
        Serial.print("\r\n** SerialManager not available **");
        return false;
    }

    // Get detected IMU type
    detectedType = serialMgr->getIMUType();
    Serial.printf("\r\n- Detected IMU: %s", getIMUTypeName());

    // Initialize based on type
    bool result = false;
    switch (detectedType)
    {
    case IMUType::BNO085:
        result = initBNO085();
        break;

    case IMUType::TM171:
        result = initTM171();
        break;

    case IMUType::NONE:
        Serial.print("\r\n- No IMU detected");
        return false;

    default:
        Serial.printf("\r\n- IMU type %s not yet supported", getIMUTypeName());
        return false;
    }

    if (result)
    {
        isInitialized = true;
        Serial.print("\r\n- IMU initialization SUCCESS");
    }
    else
    {
        Serial.print("\r\n** IMU initialization FAILED **");
    }

    return result;
}

bool IMUProcessor::initBNO085()
{
    Serial.print("\r\n- Initializing BNO085 RVC mode");

    bno = new BNO_RVC();
    if (bno->begin(imuSerial))
    {
        Serial.print("\r\n  - BNO085 communication established");
        Serial.printf("\r\n  - Initial data: Yaw=%d, Pitch=%d, Roll=%d",
                      bno->rvcData.yawX10, bno->rvcData.pitchX10, bno->rvcData.rollX10);
        return true;
    }

    delete bno;
    bno = nullptr;
    return false;
}

bool IMUProcessor::initTM171()
{
    Serial.print("\r\n- Initializing TM171");

    // Create TM171 parser
    tm171Parser = new TM171MinimalParser();

    Serial.print("\r\n  - TM171 parser initialized");

    // Clear serial buffer
    while (imuSerial->available())
    {
        imuSerial->read();
    }

    // Wait for data
    Serial.print("\r\n  - Waiting for TM171 data...");
    uint32_t startTime = millis();

    while (millis() - startTime < 1000)
    {
        if (imuSerial->available())
        {
            uint8_t byte = imuSerial->read();
            tm171Parser->processByte(byte);

            if (tm171Parser->packetsReceived > 0)
            {
                Serial.printf("\r\n  - TM171 packets received: %lu", tm171Parser->packetsReceived);
                return true;
            }
        }
    }

    // If we got here, check if any data was received
    if (tm171Parser->packetsError > 0)
    {
        Serial.printf("\r\n  - TM171 CRC errors: %lu (check wiring)", tm171Parser->packetsError);
    }
    else
    {
        Serial.print("\r\n  - No TM171 data received");
    }

    return false;
}

void IMUProcessor::process()
{
    if (!isInitialized)
        return;

    switch (detectedType)
    {
    case IMUType::BNO085:
        processBNO085Data();
        break;

    case IMUType::TM171:
        processTM171Data();
        break;

    default:
        break;
    }
}

void IMUProcessor::processBNO085Data()
{
    if (!bno)
        return;

    // BNO085 read() returns true when new data is available
    if (bno->read())
    {
        // Update current data
        currentData.heading = bno->rvcData.yawX10 / 10.0f;
        currentData.pitch = bno->rvcData.pitchX10 / 10.0f;
        currentData.roll = bno->rvcData.rollX10 / 10.0f;
        currentData.yawRate = bno->rvcData.angVel / 10.0f;
        currentData.quality = bno->isActive ? 10 : 0;
        currentData.timestamp = millis();
        currentData.isValid = true;

        // Update statistics
        packetsReceived++;
        timeSinceLastPacket = 0;
    }
}

void IMUProcessor::processTM171Data()
{
    while (imuSerial->available())
    {
        uint8_t byte = imuSerial->read();
        tm171Parser.addByte(byte);

        // Check if we have new valid data
        if (tm171Parser.isDataValid())
        {
            // Update our angle data
            roll = tm171Parser.getRoll();
            pitch = tm171Parser.getPitch();
            yaw = tm171Parser.getYaw();

            // TM171 yaw is heading in this context
            heading = yaw;

            dataReady = true;
            lastDataTime = millis();
        }
    }
}
const char *IMUProcessor::getIMUTypeName() const
{
    return serialMgr ? serialMgr->getIMUTypeName(detectedType) : "Unknown";
}

void IMUProcessor::printStatus()
{
    Serial.print("\r\n\n=== IMU Processor Status ===");
    Serial.printf("\r\nIMU Type: %s", getIMUTypeName());
    Serial.printf("\r\nInitialized: %s", isInitialized ? "YES" : "NO");
    Serial.printf("\r\nActive: %s", isActive() ? "YES" : "NO");
    Serial.printf("\r\nPackets received: %lu", packetsReceived);
    Serial.printf("\r\nPacket errors: %lu", packetsErrors);
    Serial.printf("\r\nTime since last packet: %lu ms", (uint32_t)timeSinceLastPacket);

    if (currentData.isValid)
    {
        Serial.printf("\r\n\nCurrent Data:");
        Serial.printf("\r\n  Heading: %.1f°", currentData.heading);
        Serial.printf("\r\n  Roll: %.1f°", currentData.roll);
        Serial.printf("\r\n  Pitch: %.1f°", currentData.pitch);
        Serial.printf("\r\n  Yaw Rate: %.1f°/s", currentData.yawRate);
        Serial.printf("\r\n  Quality: %u", currentData.quality);
    }
    else
    {
        Serial.print("\r\n\nNo valid data");
    }

    Serial.print("\r\n=============================\r\n");
}

void IMUProcessor::printCurrentData()
{
    if (currentData.isValid)
    {
        Serial.printf("\r\n%lu IMU: H=%.1f° R=%.1f° P=%.1f° YR=%.1f°/s Q=%u",
                      currentData.timestamp, currentData.heading, currentData.roll,
                      currentData.pitch, currentData.yawRate, currentData.quality);
    }
}

// Add a debug method to IMUProcessor
void IMUProcessor::printDebugInfo()
{
    Serial.println(F("\n=== IMU Processor Debug ==="));
    Serial.print(F("IMU Type: "));
    Serial.println(imuType == IMUType::BNO085 ? F("BNO085") : imuType == IMUType::TM171 ? F("TM171")
                                                                                        : F("None"));
    Serial.print(F("IMU Detected: "));
    Serial.println(imuDetected ? F("Yes") : F("No"));
    Serial.print(F("Data Ready: "));
    Serial.println(dataReady ? F("Yes") : F("No"));

    if (imuType == IMUType::TM171)
    {
        tm171Parser.printDebug();
    }

    if (dataReady)
    {
        Serial.print(F("Roll: "));
        Serial.print(roll, 2);
        Serial.println(F("°"));
        Serial.print(F("Pitch: "));
        Serial.print(pitch, 2);
        Serial.println(F("°"));
        Serial.print(F("Yaw/Heading: "));
        Serial.print(heading, 2);
        Serial.println(F("°"));

        uint32_t timeSinceData = millis() - lastDataTime;
        Serial.print(F("Time since last data: "));
        Serial.print(timeSinceData);
        Serial.println(F(" ms"));
    }
    Serial.println(F("==========================\n"));
}