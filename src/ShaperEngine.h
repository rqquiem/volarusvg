// src/ShaperEngine.h
#ifndef SHAPERENGINE_H
#define SHAPERENGINE_H

#include <string>
#include <map>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include "windivert.h"

// ============================================================================
// Token Bucket for bandwidth throttling
// ============================================================================
struct TokenBucket {
    double capacity;
    double tokens;
    double refillRate; // bytes per second
    std::chrono::steady_clock::time_point lastRefill;

    TokenBucket() : capacity(0), tokens(0), refillRate(0) {}
    TokenBucket(double rate) : capacity(rate * 2), tokens(rate * 2), refillRate(rate), 
                               lastRefill(std::chrono::steady_clock::now()) {}

    bool consume(size_t bytes) {
        if (refillRate <= 0) return false; 
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastRefill).count();
        tokens = (std::min)(capacity, tokens + (elapsed * refillRate));
        lastRefill = now;

        if (tokens >= bytes) {
            tokens -= bytes;
            return true;
        }
        return false;
    }
};

// ============================================================================
// Traffic Policy modes
// ============================================================================
enum class TrafficMode {
    NORMAL,     // Passthrough — no modification
    THROTTLE,   // Bandwidth limited via token bucket
    DELAY,      // Packets held for N ms before forwarding
    CUT         // All packets dropped silently
};

struct TrafficPolicy {
    TrafficMode mode = TrafficMode::NORMAL;
    int kbps = 0;       // For THROTTLE mode (KB/s)
    int delayMs = 0;    // For DELAY mode (milliseconds)
};

// ============================================================================
// Delayed packet (queued for later forwarding)
// ============================================================================
struct DelayedPacket {
    std::vector<uint8_t> data;
    WINDIVERT_ADDRESS addr;
    std::chrono::steady_clock::time_point releaseTime;
};

// ============================================================================
// Shaper Engine
// ============================================================================
class ShaperEngine {
public:
    ShaperEngine();
    ~ShaperEngine();

    void start();
    void stop();

    // Set the local IP so we can protect our own traffic
    void setLocalIp(const std::string& ip) { 
        std::lock_guard<std::mutex> lock(m_policyMutex);
        m_localIp = ip; 
    }

    // Legacy interface
    void setThrottle(const std::string& ip, int kbps);

    // Policy interface
    void setPolicy(const std::string& ip, const TrafficPolicy& policy);
    void clearPolicy(const std::string& ip);
    void clearAllPolicies();  // Instant clear — no lag
    TrafficPolicy getPolicy(const std::string& ip) const;
    bool hasPolicies() const;

private:
    void processingLoop();
    void delayFlushLoop();
    
    HANDLE m_handle;
    std::atomic<bool> m_running;
    std::thread m_worker;
    std::thread m_delayWorker;
    
    // Per-IP policies
    std::map<std::string, TrafficPolicy> m_policies;
    std::map<std::string, TokenBucket> m_buckets;
    mutable std::mutex m_policyMutex;
    
    std::string m_localIp;

    // Delay queue
    std::deque<DelayedPacket> m_delayQueue;
    std::mutex m_delayMutex;
};

#endif // SHAPERENGINE_H
