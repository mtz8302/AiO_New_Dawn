// WheelAngleFusion.cpp - Implementation of sensor fusion for wheel angle
#include "WheelAngleFusion.h"
#include "KeyaCANDriver.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h"
#include <cmath>

// Global instance pointer
WheelAngleFusion* wheelAngleFusionPtr = nullptr;

WheelAngleFusion::WheelAngleFusion() :
    keyaDriver(nullptr),
    gnssProcessor(nullptr),
    imuProcessor(nullptr),
    fusedAngle(0.0f),
    predictedAngle(0.0f),
    uncertainty(10.0f),
    kalmanGain(0.0f),
    encoderAngle(0.0f),
    gpsAngle(0.0f),
    gpsAngleValid(false),
    lastEncoderPosition(0),
    encoderOffset(0),
    vehicleSpeed(0.0f),
    headingRate(0.0f),
    lastHeading(0.0f),
    measurementVariance(1.0f),
    varianceBuffer(nullptr),
    varianceIndex(0),
    varianceCount(0),
    encoderDrift(0.0f),
    driftRate(0.0f),
    driftStartTime(0),
    lastUpdateTime(0),
    lastGPSTime(0),
    calibrationMode(false),
    calibrationMinAngle(-40.0f),
    calibrationMaxAngle(40.0f),
    calibrationMinPosition(0),
    calibrationMaxPosition(65535)
{
    // Set global pointer
    wheelAngleFusionPtr = this;
}

bool WheelAngleFusion::init(KeyaCANDriver* keya, GNSSProcessor* gnss, IMUProcessor* imu) {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing WheelAngleFusion");
    
    // Store sensor interfaces
    keyaDriver = keya;
    gnssProcessor = gnss;
    imuProcessor = imu;
    
    // Validate interfaces
    if (!keyaDriver) {
        LOG_ERROR(EventSource::AUTOSTEER, "WheelAngleFusion: No Keya driver provided");
        return false;
    }
    
    if (!gnssProcessor && !imuProcessor) {
        LOG_ERROR(EventSource::AUTOSTEER, "WheelAngleFusion: No heading rate source (GPS or IMU)");
        return false;
    }
    
    // Allocate variance buffer
    if (varianceBuffer) {
        delete[] varianceBuffer;
    }
    varianceBuffer = new float[config.varianceBufferSize];
    if (!varianceBuffer) {
        LOG_ERROR(EventSource::AUTOSTEER, "WheelAngleFusion: Failed to allocate variance buffer");
        return false;
    }
    
    // Initialize variance buffer
    for (uint16_t i = 0; i < config.varianceBufferSize; i++) {
        varianceBuffer[i] = 0.0f;
    }
    
    // Initialize Kalman filter state
    fusedAngle = 0.0f;
    uncertainty = config.initialUncertainty;
    
    // Initialize timing
    lastUpdateTime = millis();
    lastGPSTime = millis();
    driftStartTime = millis();
    
    LOG_INFO(EventSource::AUTOSTEER, "WheelAngleFusion initialized successfully");
    LOG_INFO(EventSource::AUTOSTEER, "  Wheelbase: %.2f m", config.wheelbase);
    LOG_INFO(EventSource::AUTOSTEER, "  Counts/degree: %.1f", config.countsPerDegree);
    LOG_INFO(EventSource::AUTOSTEER, "  Min GPS speed: %.1f m/s", config.minSpeedForGPS);
    
    return true;
}

void WheelAngleFusion::update(float dt) {
    // Update timing
    uint32_t now = millis();
    lastUpdateTime = now;
    
    // Update sensor angles
    updateEncoderAngle();
    updateGPSAngle();
    
    // Run Kalman filter
    updateKalmanFilter(dt);
    
    // Update adaptive parameters
    if (isValidGPSConditions()) {
        updateVariance();
    }
    
    // Update drift compensation
    if (config.enableDriftCompensation) {
        updateDriftCompensation(dt);
    }
}

void WheelAngleFusion::updateEncoderAngle() {
    // TODO: Get motor position from KeyaCANDriver
    // For now, just placeholder
    encoderAngle = 0.0f;
    
    LOG_DEBUG(EventSource::AUTOSTEER, "Encoder angle: %.2f°", encoderAngle);
}

void WheelAngleFusion::updateGPSAngle() {
    // TODO: Calculate GPS angle from heading rate and speed
    // For now, just placeholder
    gpsAngle = 0.0f;
    gpsAngleValid = false;
    
    if (gpsAngleValid) {
        LOG_DEBUG(EventSource::AUTOSTEER, "GPS angle: %.2f°", gpsAngle);
    }
}

void WheelAngleFusion::updateKalmanFilter(float dt) {
    // Prediction step
    // TODO: Implement prediction using encoder delta
    predictedAngle = fusedAngle;
    float predictedUncertainty = uncertainty + config.processNoise;
    
    // Update step
    if (gpsAngleValid) {
        // Calculate Kalman gain with adaptive measurement noise
        float adaptiveR = config.measurementNoise * measurementVariance;
        kalmanGain = predictedUncertainty / (predictedUncertainty + adaptiveR);
        
        // Update estimate
        fusedAngle = predictedAngle + kalmanGain * (gpsAngle - predictedAngle);
        uncertainty = (1.0f - kalmanGain) * predictedUncertainty;
        
        LOG_DEBUG(EventSource::AUTOSTEER, "Kalman update: K=%.3f, fused=%.2f°", 
                  kalmanGain, fusedAngle);
    } else {
        // No GPS update, just use prediction
        fusedAngle = predictedAngle;
        uncertainty = predictedUncertainty;
    }
}

void WheelAngleFusion::updateVariance() {
    // TODO: Implement adaptive variance calculation
    // For now, use fixed variance
    measurementVariance = 1.0f;
}

void WheelAngleFusion::updateDriftCompensation(float dt) {
    // TODO: Implement drift compensation
    // For now, no compensation
    encoderDrift = 0.0f;
    driftRate = 0.0f;
}

float WheelAngleFusion::calculateGPSAngleFromHeadingRate(float headingRate, float speed) {
    // Ackermann steering geometry
    // wheel_angle = atan(heading_rate * wheelbase / speed) * RAD_TO_DEG
    
    if (speed < config.minSpeedForGPS) {
        return 0.0f;
    }
    
    float headingRateRad = headingRate * DEG_TO_RAD;
    float angleRad = atan(headingRateRad * config.wheelbase / speed);
    float angleDeg = angleRad * RAD_TO_DEG;
    
    // Sanity check
    if (angleDeg > config.maxSteeringAngle) {
        angleDeg = config.maxSteeringAngle;
    } else if (angleDeg < -config.maxSteeringAngle) {
        angleDeg = -config.maxSteeringAngle;
    }
    
    return angleDeg;
}

bool WheelAngleFusion::isValidGPSConditions() const {
    // Check if conditions are suitable for GPS angle calculation
    return (vehicleSpeed >= config.minSpeedForGPS &&
            abs(headingRate) < config.maxHeadingRate &&
            abs(fusedAngle) < 30.0f);  // Relatively straight driving
}

bool WheelAngleFusion::isHealthy() const {
    // Check overall system health
    uint32_t now = millis();
    
    // Check if we're getting updates
    if (now - lastUpdateTime > 1000) {
        return false;  // No updates for 1 second
    }
    
    // Check if uncertainty is reasonable
    if (uncertainty > 50.0f) {
        return false;  // Too uncertain
    }
    
    // Check if angle is reasonable
    if (abs(fusedAngle) > config.maxSteeringAngle * 1.5f) {
        return false;  // Angle out of bounds
    }
    
    return true;
}

void WheelAngleFusion::startCalibration() {
    LOG_INFO(EventSource::AUTOSTEER, "Starting wheel angle fusion calibration");
    calibrationMode = true;
    calibrationMinAngle = 999.0f;
    calibrationMaxAngle = -999.0f;
    calibrationMinPosition = INT32_MAX;
    calibrationMaxPosition = INT32_MIN;
}

void WheelAngleFusion::stopCalibration() {
    if (!calibrationMode) return;
    
    calibrationMode = false;
    
    // Calculate counts per degree from calibration data
    if (calibrationMaxAngle > calibrationMinAngle && 
        calibrationMaxPosition > calibrationMinPosition) {
        
        float angleRange = calibrationMaxAngle - calibrationMinAngle;
        int32_t positionRange = calibrationMaxPosition - calibrationMinPosition;
        
        config.countsPerDegree = (float)positionRange / angleRange;
        
        LOG_INFO(EventSource::AUTOSTEER, "Calibration complete:");
        LOG_INFO(EventSource::AUTOSTEER, "  Angle range: %.1f° to %.1f°", 
                 calibrationMinAngle, calibrationMaxAngle);
        LOG_INFO(EventSource::AUTOSTEER, "  Position range: %d to %d", 
                 calibrationMinPosition, calibrationMaxPosition);
        LOG_INFO(EventSource::AUTOSTEER, "  Counts per degree: %.2f", config.countsPerDegree);
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "Calibration failed - insufficient data");
    }
}

void WheelAngleFusion::setEncoderCenter() {
    // TODO: Get current encoder position and set as center
    LOG_INFO(EventSource::AUTOSTEER, "Encoder center position set");
}

void WheelAngleFusion::reset() {
    LOG_INFO(EventSource::AUTOSTEER, "Resetting wheel angle fusion");
    
    // Reset Kalman filter
    fusedAngle = 0.0f;
    predictedAngle = 0.0f;
    uncertainty = config.initialUncertainty;
    kalmanGain = 0.0f;
    
    // Reset sensor angles
    encoderAngle = 0.0f;
    gpsAngle = 0.0f;
    gpsAngleValid = false;
    
    // Reset drift compensation
    encoderDrift = 0.0f;
    driftRate = 0.0f;
    driftStartTime = millis();
    
    // Clear variance buffer
    for (uint16_t i = 0; i < config.varianceBufferSize; i++) {
        varianceBuffer[i] = 0.0f;
    }
    varianceIndex = 0;
    varianceCount = 0;
    measurementVariance = 1.0f;
}

void WheelAngleFusion::resetDriftCompensation() {
    LOG_INFO(EventSource::AUTOSTEER, "Resetting drift compensation");
    encoderDrift = 0.0f;
    driftRate = 0.0f;
    driftStartTime = millis();
}