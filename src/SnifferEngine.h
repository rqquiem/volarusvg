// src/SnifferEngine.h
#ifndef SNIFFERENGINE_H
#define SNIFFERENGINE_H

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <winsock2.h>
#include <pcap.h>
#include "PacketHeaders.h"

// ============================================================================
// Sniffed Packet Record
// ============================================================================
struct SniffedPacket {
    uint32_t    index;          // packet number
    double      timestamp;      // seconds since capture start
    std::string srcIp;
    std::string dstIp;
    uint16_t    srcPort;
    uint16_t    dstPort;
    std::string protocol;       // "TCP", "UDP", "ICMP", "ARP", "Other"
    uint32_t    length;         // total frame length
    std::string info;           // human-readable summary
    uint8_t     tcpFlags;       // raw TCP flags (0 if not TCP)

    // Raw data for hex dump (first 256 bytes)
    std::vector<uint8_t> rawData;
};

// ============================================================================
// Per-IP Traffic Statistics
// ============================================================================
struct TrafficStats {
    uint64_t bytesIn  = 0;
    uint64_t bytesOut = 0;
    uint64_t packetsIn  = 0;
    uint64_t packetsOut = 0;

    // Rolling rate (bytes/sec)
    double rateIn  = 0.0;
    double rateOut = 0.0;

    uint64_t _prevBytesIn  = 0;
    uint64_t _prevBytesOut = 0;

    static const int HISTORY_LEN = 60;
    float historyIn[HISTORY_LEN]  = {};
    float historyOut[HISTORY_LEN] = {};
    int historyIdx = 0;
};

// ============================================================================
// Protocol Distribution Counters
// ============================================================================
struct ProtocolStats {
    uint64_t tcp   = 0;
    uint64_t udp   = 0;
    uint64_t icmp  = 0;
    uint64_t arp   = 0;
    uint64_t other = 0;

    uint64_t total() const { return tcp + udp + icmp + arp + other; }
};

// ============================================================================
// Display-level filter
// ============================================================================
struct SnifferFilter {
    std::string protocol;       // "" = all, "TCP", "UDP", "ICMP", "ARP"
    std::string ipFilter;       // "" = all, else match src or dst
    uint16_t    portFilter = 0; // 0 = all
};

// ============================================================================
// Activity Detection — Per-IP browsing/application tracking
// ============================================================================
struct DetectedActivity {
    std::string domain;             // domain name (from DNS/SNI/HTTP)
    std::string category;           // "Web", "Social", "Streaming", "Messaging", etc.
    std::string method;             // "DNS", "SNI", "HTTP"
    std::string destIp;             // destination IP for this domain
    double      firstSeen = 0.0;    // timestamp
    double      lastSeen  = 0.0;
    uint64_t    hitCount  = 0;
    
    // Full URLs captured (HTTP only — HTTPS paths are encrypted)
    static const int MAX_URLS = 50;
    std::vector<std::string> urls;  // most recent URLs
};

struct IPActivity {
    std::string ip;
    std::map<std::string, DetectedActivity> activities;  // keyed by domain
    uint64_t totalDomains = 0;
};

// ============================================================================
// DNS IP→Domain mapping cache
// ============================================================================
struct DnsMapping {
    std::string domain;
    double timestamp;
};

// ============================================================================
// Sniffer Engine
// ============================================================================
class SnifferEngine {
public:
    SnifferEngine();
    ~SnifferEngine();

    bool initialize(const std::string& adapterName, const std::string& localIp);
    void startCapture();
    void stopCapture();
    void pauseCapture();
    void resumeCapture();
    void clearCapture();

    bool isCapturing() const { return m_capturing; }
    bool isPaused() const { return m_paused; }

    // Get filtered packet list (applies display filters)
    std::vector<SniffedPacket> getPackets(const SnifferFilter& filter, int maxCount = 500) const;
    uint32_t getTotalPacketCount() const { return m_packetIndex; }
    double getCaptureRate() const { return m_captureRate; }
    double getCaptureDuration() const;
    uint64_t getTotalBytes() const { return m_totalBytes; }

    // Per-IP traffic stats
    std::map<std::string, TrafficStats> getTrafficStats() const;
    void updateRates();

    // Protocol distribution
    ProtocolStats getProtocolStats() const;

    // ============ Activity Intelligence ============
    // Get activity map: IP → list of detected domains/apps
    std::map<std::string, IPActivity> getActivityMap() const;
    
    // Get DNS reverse mapping: IP → domain
    std::string resolveDomain(const std::string& ip) const;

    // Get all detected domains (for debug/display)
    size_t getTotalDomainsDetected() const;

private:
    void captureLoop();
    void parsePacket(const uint8_t* data, uint32_t length, double timestamp);
    std::string formatTcpFlags(uint8_t flags) const;
    std::string getServiceName(uint16_t port) const;

    // Activity detection
    void parseDnsPacket(const uint8_t* data, uint32_t len, const std::string& srcIp, const std::string& dstIp, double timestamp);
    void parseTlsSni(const uint8_t* payload, uint32_t payloadLen, const std::string& srcIp, const std::string& dstIp, double timestamp);
    void parseHttpHost(const uint8_t* payload, uint32_t payloadLen, const std::string& srcIp, const std::string& dstIp, double timestamp);
    void recordActivity(const std::string& ip, const std::string& domain, const std::string& method, double timestamp, const std::string& destIp = "", const std::string& url = "");
    std::string categorizeDomain(const std::string& domain) const;

    pcap_t* m_handle;
    std::atomic<bool> m_capturing;
    std::atomic<bool> m_paused;
    std::thread m_captureThread;
    std::string m_localIp;

    // Ring buffer of captured packets
    static const size_t MAX_PACKETS = 10000;
    mutable std::mutex m_packetsMutex;
    std::deque<SniffedPacket> m_packets;
    std::atomic<uint32_t> m_packetIndex;

    // Traffic stats per IP
    mutable std::mutex m_statsMutex;
    std::map<std::string, TrafficStats> m_trafficStats;

    // Protocol counters
    ProtocolStats m_protoStats;

    // Capture timing
    std::chrono::steady_clock::time_point m_captureStart;
    std::atomic<double> m_captureRate;
    std::atomic<uint64_t> m_totalBytes;

    // Rate calculation
    uint32_t _prevPacketCount = 0;
    std::chrono::steady_clock::time_point _lastRateCalc;

    // ============ Activity Intelligence Data ============
    mutable std::mutex m_activityMutex;
    std::map<std::string, IPActivity> m_activityMap;    // IP → activity
    std::map<std::string, DnsMapping> m_dnsCache;       // resolved IP → domain name
};

#endif // SNIFFERENGINE_H
