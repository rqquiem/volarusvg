// src/KarmaEngine.h
#ifndef KARMAENGINE_H
#define KARMAENGINE_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <wlanapi.h>

#pragma comment(lib, "wlanapi.lib")

// ============================================================================
// WiFi Network Info (from scan)
// ============================================================================
struct WifiNetwork {
    std::string ssid;
    std::string bssid;          // AP MAC address
    int channel = 0;
    int signalQuality = 0;      // 0-100%
    std::string security;       // "Open", "WPA2", "WPA3", etc.
    std::string authAlgo;
    std::string cipherAlgo;
    bool isConnected = false;
};

// ============================================================================
// Probe Request (captured from nearby devices)
// ============================================================================
struct ProbeRequest {
    std::string clientMac;
    std::string ssid;           // What the device is looking for
    int signalStrength = 0;
    std::chrono::steady_clock::time_point timestamp;
    int count = 1;              // How many times probed
};

// ============================================================================
// Connected Client (to our Evil Twin)
// ============================================================================
struct EvilTwinClient {
    std::string mac;
    std::string ip;
    std::string hostname;
    std::chrono::steady_clock::time_point connectedAt;
    uint64_t bytesTransferred = 0;
};

// ============================================================================
// Deauth Target
// ============================================================================
struct DeauthTarget {
    std::string clientMac;
    std::string apBssid;
    int channel = 0;
    int packetsSent = 0;
    bool active = false;
};

// ============================================================================
// Karma Engine — Layer 2 Wireless Deception Framework
// ============================================================================
class KarmaEngine {
public:
    KarmaEngine();
    ~KarmaEngine();

    // Initialize the WiFi handle
    bool initialize();
    void shutdown();

    // WiFi Scanning
    void scanNetworks();
    std::vector<WifiNetwork> getNetworkList() const;
    bool isScanning() const { return m_scanning; }
    int getNetworkCount() const;

    // Probe Request Monitor
    void startProbeMonitor();
    void stopProbeMonitor();
    bool isProbeMonitorActive() const { return m_probeMonitorActive; }
    std::vector<ProbeRequest> getProbeRequests() const;
    void clearProbeRequests();
    int getProbeCount() const;

    // Evil Twin AP
    bool startEvilTwin(const std::string& ssid, const std::string& password = "");
    void stopEvilTwin();
    bool isEvilTwinActive() const { return m_evilTwinActive; }
    std::string getEvilTwinSSID() const { return m_evilTwinSSID; }
    std::vector<EvilTwinClient> getConnectedClients() const;
    int getClientCount() const;

    // Karma Attack (auto-respond to all probes)
    void startKarmaMode();
    void stopKarmaMode();
    bool isKarmaModeActive() const { return m_karmaActive; }

    // Deauth Attack
    void startDeauth(const std::string& targetMac, const std::string& apBssid, int channel);
    void stopDeauth(const std::string& targetMac);
    void stopAllDeauth();
    void deauthAllClients(const std::string& apBssid, int channel);
    bool isDeauthActive() const;
    std::vector<DeauthTarget> getDeauthTargets() const;
    int getDeauthPacketCount() const;

    // Status
    std::string getStatusMessage() const;
    GUID getInterfaceGuid() const { return m_interfaceGuid; }

private:
    // WiFi handle
    HANDLE m_wlanHandle = NULL;
    GUID m_interfaceGuid = {};
    bool m_initialized = false;

    // Network scan
    std::vector<WifiNetwork> m_networks;
    mutable std::mutex m_networkMutex;
    std::atomic<bool> m_scanning{false};

    // Probe monitor
    std::map<std::string, ProbeRequest> m_probeRequests; // keyed by clientMac+ssid
    mutable std::mutex m_probeMutex;
    std::atomic<bool> m_probeMonitorActive{false};
    std::thread m_probeThread;
    void probeMonitorLoop();

    // Evil Twin
    std::atomic<bool> m_evilTwinActive{false};
    std::string m_evilTwinSSID;
    std::string m_evilTwinPassword;
    std::vector<EvilTwinClient> m_clients;
    mutable std::mutex m_clientMutex;

    // Karma Mode
    std::atomic<bool> m_karmaActive{false};
    std::thread m_karmaThread;
    std::set<std::string> m_karmaRespondedSSIDs;
    void karmaLoop();

    // Deauth
    std::map<std::string, DeauthTarget> m_deauthTargets;
    mutable std::mutex m_deauthMutex;
    std::thread m_deauthThread;
    std::atomic<bool> m_deauthRunning{false};
    void deauthLoop();

    // Status
    std::string m_statusMessage;
    mutable std::mutex m_statusMutex;
    void setStatus(const std::string& msg);

    // Helpers
    std::string securityToString(DOT11_AUTH_ALGORITHM auth, DOT11_CIPHER_ALGORITHM cipher) const;
    std::string macToString(const uint8_t mac[6]) const;
    std::string bssidToString(const DOT11_MAC_ADDRESS& mac) const;
};

#endif // KARMAENGINE_H
