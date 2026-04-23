// src/SpeedTestEngine.cpp
#include "SpeedTestEngine.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "wininet.lib")

SpeedTestEngine::SpeedTestEngine() : m_testing(false), m_theoreticalMax(0.0) {}

SpeedTestEngine::~SpeedTestEngine() {
    stopTest();
}

void SpeedTestEngine::setLinkSpeed(const std::string& linkSpeedStr) {
    // Parse "100 Mbps" → 100.0
    double val = 0;
    if (sscanf_s(linkSpeedStr.c_str(), "%lf", &val) == 1) {
        m_theoreticalMax = val;
    }
}

SpeedTestResult SpeedTestEngine::getResult() const {
    std::lock_guard<std::mutex> lock(m_resultMutex);
    return m_currentResult;
}

std::vector<SpeedTestResult> SpeedTestEngine::getHistory() const {
    std::lock_guard<std::mutex> lock(m_resultMutex);
    return m_history;
}

void SpeedTestEngine::clearHistory() {
    std::lock_guard<std::mutex> lock(m_resultMutex);
    m_history.clear();
}

void SpeedTestEngine::startTest() {
    if (m_testing) return;
    m_testing = true;
    if (m_testThread.joinable()) m_testThread.join();
    m_testThread = std::thread(&SpeedTestEngine::testWorker, this);
}

void SpeedTestEngine::stopTest() {
    m_testing = false;
    if (m_testThread.joinable()) m_testThread.join();
}

// ============================================================================
// Measure latency via TCP connect to a host
// ============================================================================
double SpeedTestEngine::measureLatency(const std::string& host, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return -1.0;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    
    // Resolve hostname
    struct addrinfo hints = {}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) {
        closesocket(sock);
        return -1.0;
    }
    memcpy(&addr.sin_addr, &((struct sockaddr_in*)result->ai_addr)->sin_addr, sizeof(struct in_addr));
    freeaddrinfo(result);

    // Set non-blocking for timeout
    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);

    auto start = std::chrono::high_resolution_clock::now();
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);
    struct timeval tv = { 3, 0 }; // 3 second timeout
    
    double latency = -1.0;
    if (select(0, nullptr, &writefds, nullptr, &tv) > 0) {
        auto end = std::chrono::high_resolution_clock::now();
        latency = std::chrono::duration<double, std::milli>(end - start).count();
    }
    
    closesocket(sock);
    return latency;
}

// ============================================================================
// Download a URL and measure throughput
// ============================================================================
double SpeedTestEngine::measureDownload(const std::string& url, uint64_t& bytesOut) {
    bytesOut = 0;
    
    HINTERNET hInet = InternetOpenA("WundaSpeedTest/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInet) return 0.0;
    
    HINTERNET hUrl = InternetOpenUrlA(hInet, url.c_str(), nullptr, 0,
        INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI, 0);
    if (!hUrl) {
        InternetCloseHandle(hInet);
        return 0.0;
    }

    char buffer[65536];
    DWORD bytesRead = 0;
    uint64_t totalBytes = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    while (m_testing) {
        if (!InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) || bytesRead == 0)
            break;
        totalBytes += bytesRead;
        
        // Update progress (estimate based on typical test file sizes)
        {
            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_currentResult.bytesDownloaded = totalBytes;
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed > 0) {
                m_currentResult.downloadMbps = (totalBytes * 8.0) / (elapsed * 1000000.0);
            }
            // Cap progress at 95% during download
            m_currentResult.progress = (std::min)(0.95, elapsed / 15.0);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInet);
    
    bytesOut = totalBytes;
    if (elapsed > 0) {
        return (totalBytes * 8.0) / (elapsed * 1000000.0); // Mbps
    }
    return 0.0;
}

// ============================================================================
// Main test worker thread
// ============================================================================
void SpeedTestEngine::testWorker() {
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_currentResult = SpeedTestResult();
        m_currentResult.status = "testing";
        m_currentResult.progress = 0.0;
    }

    // Test servers — try multiple endpoints for reliability
    struct TestServer {
        std::string name;
        std::string pingHost;
        std::string downloadUrl;
    };
    
    std::vector<TestServer> servers = {
        { "Cloudflare", "speed.cloudflare.com", "https://speed.cloudflare.com/__down?bytes=25000000" }
    };

    // ---- Phase 1: Latency test ----
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_currentResult.status = "measuring latency...";
        m_currentResult.progress = 0.05;
    }

    double bestLatency = 999999.0;
    int bestServer = 0;
    
    for (int i = 0; i < (int)servers.size() && m_testing; ++i) {
        // Take 3 pings, use best
        double minPing = 999999.0;
        for (int p = 0; p < 3 && m_testing; ++p) {
            double lat = measureLatency(servers[i].pingHost, 80);
            if (lat > 0 && lat < minPing) minPing = lat;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (minPing < bestLatency) {
            bestLatency = minPing;
            bestServer = i;
        }
    }

    if (!m_testing) return;

    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_currentResult.latencyMs = bestLatency;
        m_currentResult.server = servers[bestServer].name;
        m_currentResult.progress = 0.15;
    }

    // ---- Phase 2: Jitter test (latency variation) ----
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_currentResult.status = "measuring jitter...";
    }
    
    std::vector<double> pings;
    for (int i = 0; i < 5 && m_testing; ++i) {
        double lat = measureLatency(servers[bestServer].pingHost, 80);
        if (lat > 0) pings.push_back(lat);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    double jitter = 0.0;
    if (pings.size() > 1) {
        for (size_t i = 1; i < pings.size(); ++i) {
            jitter += std::abs(pings[i] - pings[i-1]);
        }
        jitter /= (pings.size() - 1);
    }

    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_currentResult.jitterMs = jitter;
        m_currentResult.progress = 0.25;
    }

    // ---- Phase 3: Download speed test ----
    if (!m_testing) return;

    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_currentResult.status = "downloading...";
    }

    uint64_t totalBytes = 0;
    double downloadMbps = measureDownload(servers[bestServer].downloadUrl, totalBytes);

    // If primary server gave very little data, try a fallback
    if (totalBytes < 100000 && m_testing) {
        // Fallback: download from alternative source
        std::string fallbackUrl = "http://speedtest.tele2.net/10MB.zip";
        downloadMbps = measureDownload(fallbackUrl, totalBytes);
        if (totalBytes > 100000) {
            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_currentResult.server += " (fallback)";
        }
    }

    if (!m_testing) return;

    // ---- Finalize results ----
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_currentResult.downloadMbps = downloadMbps;
        m_currentResult.bytesDownloaded = totalBytes;
        m_currentResult.theoreticalMaxMbps = m_theoreticalMax;
        
        if (m_theoreticalMax > 0) {
            m_currentResult.efficiencyPercent = (downloadMbps / m_theoreticalMax) * 100.0;
        }
        
        m_currentResult.progress = 1.0;
        m_currentResult.status = "complete";
        
        // Add to history
        m_history.push_back(m_currentResult);
        if (m_history.size() > 20) m_history.erase(m_history.begin());
    }

    m_testing = false;
}
