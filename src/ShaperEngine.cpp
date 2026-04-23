// src/ShaperEngine.cpp
#include "ShaperEngine.h"
#include <iostream>
#include <algorithm>

ShaperEngine::ShaperEngine() : m_handle(INVALID_HANDLE_VALUE), m_running(false) {}

ShaperEngine::~ShaperEngine() {
    stop();
}

void ShaperEngine::start() {
    if (m_running) return;
    m_handle = WinDivertOpen("ip", WINDIVERT_LAYER_NETWORK, 0, 0);
    if (m_handle == INVALID_HANDLE_VALUE) {
        std::cerr << "WinDivert failed. Run as Admin." << std::endl;
        return;
    }
    m_running = true;
    m_worker = std::thread(&ShaperEngine::processingLoop, this);
    m_delayWorker = std::thread(&ShaperEngine::delayFlushLoop, this);
}

void ShaperEngine::stop() {
    m_running = false;
    if (m_handle != INVALID_HANDLE_VALUE) {
        WinDivertClose(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
    if (m_worker.joinable()) m_worker.join();
    if (m_delayWorker.joinable()) m_delayWorker.join();
}

void ShaperEngine::setThrottle(const std::string& ip, int kbps) {
    if (kbps == -1) {
        clearPolicy(ip);
    } else if (kbps == 0) {
        TrafficPolicy p;
        p.mode = TrafficMode::CUT;
        setPolicy(ip, p);
    } else {
        TrafficPolicy p;
        p.mode = TrafficMode::THROTTLE;
        p.kbps = kbps;
        setPolicy(ip, p);
    }
}

void ShaperEngine::setPolicy(const std::string& ip, const TrafficPolicy& policy) {
    std::lock_guard<std::mutex> lock(m_policyMutex);
    m_policies[ip] = policy;
    if (policy.mode == TrafficMode::THROTTLE) {
        m_buckets[ip] = TokenBucket(policy.kbps * 1024.0);
    } else {
        m_buckets.erase(ip);
    }
}

void ShaperEngine::clearPolicy(const std::string& ip) {
    std::lock_guard<std::mutex> lock(m_policyMutex);
    m_policies.erase(ip);
    m_buckets.erase(ip);
}

void ShaperEngine::clearAllPolicies() {
    std::lock_guard<std::mutex> lock(m_policyMutex);
    m_policies.clear();
    m_buckets.clear();
    
    // Also clear delay queue
    std::lock_guard<std::mutex> dlock(m_delayMutex);
    m_delayQueue.clear();
}

TrafficPolicy ShaperEngine::getPolicy(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(m_policyMutex);
    auto it = m_policies.find(ip);
    if (it != m_policies.end()) return it->second;
    return TrafficPolicy{}; // NORMAL
}

bool ShaperEngine::hasPolicies() const {
    std::lock_guard<std::mutex> lock(m_policyMutex);
    return !m_policies.empty();
}

void ShaperEngine::processingLoop() {
    unsigned char packet[0xFFFF];
    UINT packetLen;
    WINDIVERT_ADDRESS addr;

    while (m_running) {
        if (!WinDivertRecv(m_handle, packet, sizeof(packet), &packetLen, &addr)) continue;

        // FAST PATH: If no policies are active, forward immediately
        // This avoids ALL string parsing overhead when just poisoning for monitoring
        {
            std::lock_guard<std::mutex> lock(m_policyMutex);
            if (m_policies.empty()) {
                WinDivertSend(m_handle, packet, packetLen, nullptr, &addr);
                continue;
            }
        }

        PWINDIVERT_IPHDR ipHdr = nullptr;
        WinDivertHelperParsePacket(packet, packetLen, &ipHdr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        
        bool shouldForward = true;

        if (ipHdr) {
            char src[16], dst[16];
            WinDivertHelperFormatIPv4Address(ipHdr->SrcAddr, src, sizeof(src));
            WinDivertHelperFormatIPv4Address(ipHdr->DstAddr, dst, sizeof(dst));

            std::string srcStr(src);
            std::string dstStr(dst);

            std::lock_guard<std::mutex> lock(m_policyMutex);

            // CRITICAL: Never apply policies to our own machine's traffic
            bool isMine = (!m_localIp.empty()) && 
                          (srcStr == m_localIp || dstStr == m_localIp);

            if (!isMine) {
                // This is intercepted victim traffic — apply policies
                TrafficMode activeMode = TrafficMode::NORMAL;
                int activeDelay = 0;
                std::string policyIp;

                auto srcIt = m_policies.find(srcStr);
                if (srcIt != m_policies.end()) {
                    activeMode = srcIt->second.mode;
                    activeDelay = srcIt->second.delayMs;
                    policyIp = srcStr;
                }
                auto dstIt = m_policies.find(dstStr);
                if (dstIt != m_policies.end() && dstIt->second.mode > activeMode) {
                    activeMode = dstIt->second.mode;
                    activeDelay = dstIt->second.delayMs;
                    policyIp = dstStr;
                }

                switch (activeMode) {
                    case TrafficMode::CUT:
                        shouldForward = false;
                        break;
                    case TrafficMode::THROTTLE:
                        if (m_buckets.count(policyIp) && !m_buckets[policyIp].consume(packetLen)) {
                            shouldForward = false;
                        }
                        break;
                    case TrafficMode::DELAY:
                        if (activeDelay > 0) {
                            DelayedPacket dp;
                            dp.data.assign(packet, packet + packetLen);
                            dp.addr = addr;
                            dp.releaseTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(activeDelay);
                            {
                                std::lock_guard<std::mutex> dlock(m_delayMutex);
                                m_delayQueue.push_back(std::move(dp));
                            }
                            shouldForward = false;
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        if (shouldForward) {
            WinDivertSend(m_handle, packet, packetLen, nullptr, &addr);
        }
    }
}

void ShaperEngine::delayFlushLoop() {
    while (m_running) {
        auto now = std::chrono::steady_clock::now();
        
        std::lock_guard<std::mutex> lock(m_delayMutex);
        while (!m_delayQueue.empty()) {
            auto& front = m_delayQueue.front();
            if (now >= front.releaseTime) {
                if (m_handle != INVALID_HANDLE_VALUE) {
                    WinDivertSend(m_handle, front.data.data(), (UINT)front.data.size(), nullptr, &front.addr);
                }
                m_delayQueue.pop_front();
            } else {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
