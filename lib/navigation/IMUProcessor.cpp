#include "IMUProcessor.h"
#include "TM171AiOParser.h"
#include "PGNUtils.h"


// Static instance pointer
IMUProcessor *IMUProcessor::instance = nullptr;

IMUProcessor::IMUProcessor()
    : serialMgr(nullptr), detectedType(IMUType::NONE), isInitialized(false),
      bno(nullptr), imuSerial(&Serial4), tm171Parser(nullptr),
      packetsReceived(0), packetsErrors(0)
{
    instance = this;

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
    tm171Parser = new TM171AiOParser();
    Serial.print("\r\n  - TM171 AiO parser created");

    // Make sure serial port is properly initialized
    if (!imuSerial)
    {
        Serial.print("\r\n  - ERROR: IMU serial port is NULL!");
        return false;
    }

    // Clear serial buffer
    while (imuSerial->available())
    {
        imuSerial->read();
    }

    // For TM171, we'll consider it initialized even without initial data
    // The device may need configuration or time to start
    Serial.print("\r\n  - TM171 initialization complete");
    Serial.print("\r\n  - Waiting for RPY packets (Object ID 0x23)...");

    return true;
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
    if (!tm171Parser)
        return;

    while (imuSerial->available())
    {
        uint8_t byte = imuSerial->read();
        tm171Parser->processByte(byte);

        // Check if we have new valid data
        if (tm171Parser->isDataValid())
        {
            // Update current data structure
            currentData.heading = tm171Parser->getYaw();
            currentData.pitch = tm171Parser->getPitch();
            currentData.roll = tm171Parser->getRoll();
            currentData.yawRate = 0;  // TM171 doesn't provide yaw rate
            currentData.quality = 10; // Assume good quality if data is valid
            currentData.timestamp = millis();
            currentData.isValid = true;

            // Update statistics from parser
            packetsReceived = tm171Parser->totalPackets;
            packetsErrors = tm171Parser->crcErrors;
            timeSinceLastPacket = 0;
        }
    }

    // Update time since last packet
    if (tm171Parser->getTimeSinceLastValid() > 100)
    {
        currentData.isValid = false;
        currentData.quality = 0;
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

    Serial.print("\r\n=============================");

    // If TM171, print parser debug info
    if (detectedType == IMUType::TM171 && tm171Parser)
    {
        tm171Parser->printStats();
    }

    Serial.println();
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

// PGN Support Implementation

// External reference to NetworkBase send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

void IMUProcessor::registerPGNCallbacks()
{
    Serial.print("\r\n[IMUProcessor] Attempting to register PGN callbacks...");
    
    // Get PGNProcessor instance and register for IMU messages
    PGNProcessor* pgnProcessor = PGNProcessor::instance;
    if (pgnProcessor)
    {
        // Register for IMU PGN (121/0x79) - this also makes us receive broadcasts
        // Even though we register for 121, we'll still receive broadcast PGNs like Hello (200)
        bool success = pgnProcessor->registerCallback(IMU_SOURCE_ID, handleHelloPGN, "IMU Handler");
        Serial.printf("\r\n[IMUProcessor] Registration %s for PGN %d", success ? "SUCCESS" : "FAILED", IMU_SOURCE_ID);
    }
    else
    {
        Serial.print("\r\n[IMUProcessor] ERROR: PGNProcessor instance is NULL!");
    }
}

// Static callback for broadcast PGNs (like Hello)
void IMUProcessor::handleHelloPGN(uint8_t pgn, const uint8_t* data, size_t len)
{
    // Check if this is a Hello PGN
    if (pgn == 200)
    {
        // When we receive a Hello from AgIO, we should respond
        // Using the same format as V6-NG
        
        Serial.print("\r\n[IMUProcessor] Received Hello PGN, sending reply");
        
        // IMU Hello reply from V6-NG: {128, 129, 121, 121, 5, 0, 0, 0, 0, 0, 71}
        uint8_t helloFromIMU[] = {128, 129, 121, 121, 5, 0, 0, 0, 0, 0, 71};
        
        // Send the reply
        sendUDPbytes(helloFromIMU, sizeof(helloFromIMU));
    }
    // Future: Handle other broadcast PGNs here (like Scan Request)
}

void IMUProcessor::sendIMUData()
{
    if (!currentData.isValid)
        return;
        
    // PGN 211 (0xD3) format:
    // 0x80, 0x81, 0x79, 0xD3, 8, Heading_Lo, Heading_Hi, Roll_Lo, Roll_Hi, Gyro_Lo, Gyro_Hi, 0, 0, CRC
    
    // Convert float values to int16 (*10 for one decimal place precision)
    int16_t headingX10 = (int16_t)(currentData.heading * 10);
    int16_t rollX10 = (int16_t)(currentData.roll * 10);
    int16_t gyroX10 = (int16_t)(currentData.yawRate * 10);  // yaw rate as gyro
    
    uint8_t imuData[] = {
        0x80, 0x81,                     // PGN header
        IMU_SOURCE_ID,                  // Source: 0x79 (121)
        IMU_PGN_DATA,                   // PGN: 0xD3 (211)
        8,                              // Data length
        (uint8_t)(headingX10 & 0xFF),   // Heading low byte
        (uint8_t)(headingX10 >> 8),     // Heading high byte
        (uint8_t)(rollX10 & 0xFF),      // Roll low byte
        (uint8_t)(rollX10 >> 8),        // Roll high byte
        (uint8_t)(gyroX10 & 0xFF),      // Gyro low byte
        (uint8_t)(gyroX10 >> 8),        // Gyro high byte
        0,                              // Reserved
        0,                              // Reserved
        0                               // CRC placeholder
    };
    
    // Calculate and set CRC
    calculateAndSetCRC(imuData, sizeof(imuData));
    
    // Send the data
    sendUDPbytes(imuData, sizeof(imuData));
}