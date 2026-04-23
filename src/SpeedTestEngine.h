// src/SpeedTestEngine.h
#ifndef SPEEDTESTENGINE_H
#define SPEEDTESTENGINE_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

struct SpeedTestResult {
    double downloadMbps = 0.0;
    double uploadMbps = 0.0;
    double latencyMs = 0.0;
    double jitterMs = 0.0;
    uint64_t bytesDownloaded = 0;
    double durationSec = 0.0;
    std::string server;
    std::string status;         // "idle", "testing", "complete", "error"
    double progress = 0.0;      // 0.0 - 1.0
    
    // Comparison data
    double theoreticalMaxMbps = 0.0;   // From adapter link speed
    double efficiencyPercent = 0.0;    // download / theoretical * 100
};

class SpeedTestEngine {
public:
    SpeedTestEngine();
    ~SpeedTestEngine();

    void startTest();
    void stopTest();
    bool isTesting() const { return m_testing; }
    
    SpeedTestResult getResult() const;
    
    // History of past tests
    std::vector<SpeedTestResult> getHistory() const;
    void clearHistory();

    // Set the adapter's theoretical max speed (from NetworkInfo)
    void setLinkSpeed(const std::string& linkSpeedStr);

private:
    void testWorker();
    double measureLatency(const std::string& host, int port);
    double measureDownload(const std::string& url, uint64_t& bytesOut);
    
    std::atomic<bool> m_testing;
    std::thread m_testThread;
    
    mutable std::mutex m_resultMutex;
    SpeedTestResult m_currentResult;
    std::vector<SpeedTestResult> m_history;
    double m_theoreticalMax;
};

#endif // SPEEDTESTENGINE_H
