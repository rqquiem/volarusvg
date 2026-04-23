// src/SslStripEngine.h
#ifndef SSLSTRIPENGINE_H
#define SSLSTRIPENGINE_H

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include "windivert.h"
#include "SnifferEngine.h"
#include "ArpEngine.h"

// ============================================================================
// Captured HTTP request/response for display
// ============================================================================
struct HttpCapture {
    uint64_t    id;
    double      timestamp;
    std::string srcIp;
    std::string dstIp;
    uint16_t    srcPort;
    uint16_t    dstPort;
    std::string method;
    std::string fullUrl;
    std::string host;
    std::string path;
    std::string userAgent;
    std::string contentType;
    std::string referer;
    std::string cookie;
    uint32_t    contentLength;
    std::string postData;
    bool        isResponse;
    int         statusCode;
    bool        wasStripped;    // Was this URL downgraded from HTTPS?
};

// ============================================================================
// Bandwidth ranking entry
// ============================================================================
struct BandwidthRank {
    std::string ip;
    std::string vendor;
    uint64_t    totalBytes;
    double      rateMbps;
    double      percentOfTotal;
    bool        isLocal;
};

// ============================================================================
// NAT mapping for transparent redirect (port 443 → local proxy)
// ============================================================================
struct NatEntry {
    uint32_t clientIp;
    uint16_t clientPort;
    uint32_t origServerIp;
    uint16_t origServerPort;
    std::chrono::steady_clock::time_point created;
};

// ============================================================================
// SSL Strip + MITM HTTP Proxy Engine
// ============================================================================
class SslStripEngine {
public:
    SslStripEngine();
    ~SslStripEngine();

    // Start/stop the interception engine
    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    // SSL Strip mode: MITM proxy that downgrades HTTPS→HTTP
    void enableSslStrip(bool enable);
    bool isSslStripEnabled() const { return m_sslStripEnabled; }

    // Get captured HTTP traffic
    std::vector<HttpCapture> getCaptures(int maxCount = 200) const;
    void clearCaptures();
    size_t getCaptureCount() const;

    // Stats
    uint64_t getHttpPacketsProcessed() const { return m_httpPackets; }
    uint64_t getStrippedCount() const { return m_strippedCount; }
    uint64_t getProxiedCount() const { return m_proxiedCount; }

    // Bandwidth ranking
    static std::vector<BandwidthRank> buildBandwidthRanking(
        const std::map<std::string, TrafficStats>& trafficStats,
        const std::string& localIp,
        const std::map<std::string, DeviceInfo>& devices);

private:
    // HTTP interception (port 80) — captures and modifies plaintext HTTP
    void httpInterceptionLoop();
    
    // MITM proxy thread — listens for redirected connections and proxies HTTPS
    void proxyListenerLoop();
    void handleProxyConnection(SOCKET clientSock, uint32_t origServerIp, uint16_t origServerPort);
    
    // NAT redirection — WinDivert redirects port 443 SYN to our local proxy
    void natRedirectLoop();
    
    void parseHttpRequest(const uint8_t* payload, uint32_t len, 
                          const std::string& srcIp, const std::string& dstIp,
                          uint16_t srcPort, uint16_t dstPort, double ts);
    void parseHttpResponse(const uint8_t* payload, uint32_t len,
                           const std::string& srcIp, const std::string& dstIp,
                           uint16_t srcPort, uint16_t dstPort, double ts);
    int  stripHttpsFromPayload(uint8_t* payload, uint32_t& len, uint32_t maxLen);
    
    void addCapture(HttpCapture&& cap);

    HANDLE m_httpHandle;        // WinDivert for HTTP (port 80) interception
    HANDLE m_natHandle;         // WinDivert for HTTPS NAT redirect
    std::atomic<bool> m_running;
    std::atomic<bool> m_sslStripEnabled;
    std::thread m_httpWorker;
    std::thread m_natWorker;
    std::thread m_proxyWorker;
    
    // MITM proxy
    SOCKET m_proxySocket;
    uint16_t m_proxyPort;
    
    // NAT table
    std::map<uint64_t, NatEntry> m_natTable;  // key = clientIp:clientPort hash
    std::mutex m_natMutex;
    
    mutable std::mutex m_captureMutex;
    std::deque<HttpCapture> m_captures;
    static const size_t MAX_CAPTURES = 5000;
    std::atomic<uint64_t> m_captureId;
    
    std::atomic<uint64_t> m_httpPackets;
    std::atomic<uint64_t> m_strippedCount;
    std::atomic<uint64_t> m_proxiedCount;
    
    std::chrono::steady_clock::time_point m_startTime;

    std::set<std::string> m_strippedDomains;
    std::mutex m_stripMutex;
};

#endif // SSLSTRIPENGINE_H
