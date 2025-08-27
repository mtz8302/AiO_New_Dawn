#ifndef GPS_TIMING_DIAGNOSTICS_H
#define GPS_TIMING_DIAGNOSTICS_H

#include <Arduino.h>
#include "EventLogger.h"

// Enable GPS timing diagnostics
#define GPS_TIMING_DEBUG

#ifdef GPS_TIMING_DEBUG

struct GPSTimingStats {
    uint32_t lastSentenceTime = 0;
    uint32_t minDelta = UINT32_MAX;
    uint32_t maxDelta = 0;
    uint32_t sumDelta = 0;
    uint16_t count = 0;
    uint16_t lateCount = 0;  // > 110ms for 10Hz
    
    void reset() {
        lastSentenceTime = 0;
        minDelta = UINT32_MAX;
        maxDelta = 0;
        sumDelta = 0;
        count = 0;
        lateCount = 0;
    }
    
    void update(uint32_t currentTime) {
        if (lastSentenceTime > 0) {
            uint32_t delta = currentTime - lastSentenceTime;
            minDelta = min(minDelta, delta);
            maxDelta = max(maxDelta, delta);
            sumDelta += delta;
            count++;
            
            // Consider "late" if > 110ms (for 10Hz GPS)
            if (delta > 110000) {  // microseconds
                lateCount++;
            }
        }
        lastSentenceTime = currentTime;
    }
    
    uint32_t getAverage() const {
        return count > 0 ? sumDelta / count : 0;
    }
    
    float getLatePercentage() const {
        return count > 0 ? (100.0f * lateCount / count) : 0.0f;
    }
};

class GPSTimingDiagnostics {
private:
    static const int REPORT_INTERVAL = 100;  // Report every 100 sentences
    
    // Timing stats for different sentence types
    GPSTimingStats ksxtStats;
    GPSTimingStats ggaStats;
    GPSTimingStats rmcStats;
    GPSTimingStats paogiStats;  // Time between PAOGI transmissions
    
    // Processing time stats
    uint32_t maxParseTime = 0;
    uint32_t sumParseTime = 0;
    uint16_t parseCount = 0;
    
    // Buffer stats
    uint16_t maxBufferDepth = 0;
    uint16_t bufferOverflows = 0;
    
    // Stage timestamps for current sentence
    uint32_t sentenceStartTime = 0;
    uint32_t parseStartTime = 0;
    uint32_t parseEndTime = 0;
    
    int sentenceCounter = 0;

public:
    GPSTimingDiagnostics() {}
    
    // Record sentence arrival
    void recordSentenceStart(const char* sentenceType) {
        sentenceStartTime = micros();
        
        if (strcmp(sentenceType, "KSXT") == 0) {
            ksxtStats.update(sentenceStartTime);
        } else if (strcmp(sentenceType, "GGA") == 0) {
            ggaStats.update(sentenceStartTime);
        } else if (strcmp(sentenceType, "RMC") == 0) {
            rmcStats.update(sentenceStartTime);
        }
        
        sentenceCounter++;
    }
    
    // Record parse timing
    void recordParseStart() {
        parseStartTime = micros();
    }
    
    void recordParseEnd() {
        parseEndTime = micros();
        uint32_t parseTime = parseEndTime - parseStartTime;
        maxParseTime = max(maxParseTime, parseTime);
        sumParseTime += parseTime;
        parseCount++;
    }
    
    // Record PAOGI transmission
    void recordPAOGITransmit() {
        uint32_t currentTime = micros();
        paogiStats.update(currentTime);
    }
    
    // Record buffer statistics
    void recordBufferDepth(uint16_t depth) {
        maxBufferDepth = max(maxBufferDepth, depth);
    }
    
    void recordBufferOverflow() {
        bufferOverflows++;
    }
    
    // Report statistics
    void reportIfNeeded() {
        if (sentenceCounter >= REPORT_INTERVAL) {
            reportStatistics();
            resetStatistics();
        }
    }
    
private:
    void reportStatistics() {
        // KSXT timing report
        if (ksxtStats.count > 0) {
            LOG_ERROR(EventSource::GNSS, 
                "GPS Timing: KSXT avg=%lums min=%lums max=%lums late=%u/%u (%.1f%%)",
                ksxtStats.getAverage() / 1000,
                ksxtStats.minDelta / 1000,
                ksxtStats.maxDelta / 1000,
                ksxtStats.lateCount,
                ksxtStats.count,
                ksxtStats.getLatePercentage()
            );
        }
        
        // GGA timing report
        if (ggaStats.count > 0) {
            LOG_ERROR(EventSource::GNSS,
                "GPS Timing: GGA avg=%lums min=%lums max=%lums late=%u/%u (%.1f%%)",
                ggaStats.getAverage() / 1000,
                ggaStats.minDelta / 1000,
                ggaStats.maxDelta / 1000,
                ggaStats.lateCount,
                ggaStats.count,
                ggaStats.getLatePercentage()
            );
        }
        
        // PAOGI transmission timing
        if (paogiStats.count > 0) {
            LOG_ERROR(EventSource::GNSS,
                "GPS Timing: PAOGI TX avg=%lums min=%lums max=%lums",
                paogiStats.getAverage() / 1000,
                paogiStats.minDelta / 1000,
                paogiStats.maxDelta / 1000
            );
        }
        
        // Buffer statistics
        LOG_ERROR(EventSource::GNSS,
            "GPS Buffer: max_depth=%u bytes, overflows=%u",
            maxBufferDepth,
            bufferOverflows
        );
        
        // Processing statistics
        if (parseCount > 0) {
            LOG_ERROR(EventSource::GNSS,
                "GPS Processing: parse avg=%luus max=%luus",
                sumParseTime / parseCount,
                maxParseTime
            );
        }
    }
    
    void resetStatistics() {
        ksxtStats.reset();
        ggaStats.reset();
        rmcStats.reset();
        paogiStats.reset();
        
        maxParseTime = 0;
        sumParseTime = 0;
        parseCount = 0;
        
        maxBufferDepth = 0;
        bufferOverflows = 0;
        
        sentenceCounter = 0;
    }
};

// Global instance
extern GPSTimingDiagnostics gpsTimingDiag;

#endif // GPS_TIMING_DEBUG

#endif // GPS_TIMING_DIAGNOSTICS_H