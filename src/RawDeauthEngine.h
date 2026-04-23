// src/RawDeauthEngine.h
#ifndef RAWDEAUTHENGINE_H
#define RAWDEAUTHENGINE_H

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <winsock2.h>
#include <windows.h>
#include <pcap.h>

#pragma pack(push, 1)

// Simplest Radiotap header for injection (8 bytes)
struct RadiotapHeader {
    uint8_t  it_version;     // set to 0
    uint8_t  it_pad;         // set to 0
    uint16_t it_len;         // length of whole radiotap header (8)
    uint32_t it_present;     // fields present (0)
};

// 802.11 MAC Header (24 bytes for Management frames)
struct Dot11MacHeader {
    uint8_t  frame_control[2]; // Frame control (Type/Subtype)
    uint16_t duration_id;      // Duration
    uint8_t  addr1[6];         // Destination MAC (Receiver)
    uint8_t  addr2[6];         // Source MAC (Transmitter)
    uint8_t  addr3[6];         // BSSID
    uint16_t seq_ctrl;         // Sequence control
};

// Deauth body (2 bytes)
struct Dot11DeauthBody {
    uint16_t reason_code;      // e.g., 7
};

#pragma pack(pop)

struct DeauthJob {
    std::string bssid;
    std::string client_mac;
    bool active = false;
    uint32_t packets_sent = 0;
};

class RawDeauthEngine {
public:
    RawDeauthEngine();
    ~RawDeauthEngine();

    bool initialize(const std::string& adapterName);
    void shutdown();

    void startDeauth(const std::string& bssid, const std::string& clientMac);
    void stopDeauth(const std::string& clientMac);
    void stopAllDeauth();

    bool isRunning() const { return m_loopRunning; }
    std::string getStatus() const;
    std::vector<DeauthJob> getActiveJobs() const;

private:
    void sendDeauthPkt(pcap_t* handle, const uint8_t bssid[6], const uint8_t client_mac[6]);
    void deauthLoop();
    void setStatus(const std::string& status);
    bool parseMac(const std::string& macStr, uint8_t outMac[6]);

    pcap_t* m_pcapHandle;
    std::string m_adapterName;
    bool m_initialized;
    
    std::string m_statusMsg;
    mutable std::mutex m_statusMutex;

    mutable std::mutex m_jobsMutex;
    std::map<std::string, DeauthJob> m_jobs; // Keyed by client_mac

    std::thread m_deauthThread;
    std::atomic<bool> m_loopRunning{false};
};

#endif // RAWDEAUTHENGINE_H
