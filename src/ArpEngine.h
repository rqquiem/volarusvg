// src/ArpEngine.h
#ifndef ARPENGINE_H
#define ARPENGINE_H

#include <string>
#include <vector>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <pcap.h>
#include "PacketHeaders.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// Network Environment Info
// ============================================================================
struct NetworkInfo {
    std::string adapterType;
    std::string adapterDesc;
    std::string ssid;
    std::string subnetBase;
    int prefixLength = 24;
    uint32_t subnetMask = 0;
    bool dhcpEnabled = false;
    std::string dns1;
    std::string dns2;
    std::string linkSpeed;
    uint64_t linkSpeedBps = 0;
    std::string macAddress;
};

struct DeviceInfo {
    std::string ip;
    std::string mac;
    std::string hostname;
    std::string vendor;
    std::string firstSeen;
    std::string os;
    bool isPoisoned = false;
    bool isCut = false;
    bool isThrottled = false;
    bool isDelayed = false;
    bool isBlocked = false;   // auto-blocked by auto-block feature
};

class ArpEngine {
public:
    ArpEngine();
    ~ArpEngine();

    bool initialize();
    void scanSubnet();
    std::vector<DeviceInfo> getDeviceList();
    bool isScanning() const { return m_scanning; }
    
    // Single-target poisoning
    void startSpoofing(const std::string& targetIp, const std::string& targetMac);
    void startVoidSpoofing(const std::string& targetIp, const std::string& targetMac);
    void stopSpoofing(const std::string& targetIp);
    void stopAllSpoofing();
    
    // Proper ARP table restore
    void resetPoison(const std::string& targetIp);
    void resetAllPoison();
    bool isGatewayMacKnown() const { return m_gatewayMacKnown; }
    
    // ==================== NEW FEATURES ====================
    
    // Shield: protect own machine with gratuitous ARP defense
    void startShield();
    void stopShield();
    bool isShieldActive() const { return m_shieldActive; }

    // Poison All: ARP poison every device on the network
    void poisonAll();
    
    // Turbo Mode: cut all others, claim full bandwidth for self
    void startTurboMode();
    void stopTurboMode();
    bool isTurboActive() const { return m_turboActive; }

    // Auto-Block: automatically block new devices that join the network
    void startAutoBlock();
    void stopAutoBlock();
    bool isAutoBlockActive() const { return m_autoBlockActive; }
    int  getBlockedCount() const;
    std::vector<std::string> getBlockedDevices() const;
    void unblockDevice(const std::string& ip);

    // =======================================================
    
    std::string getLocalIp() const { return m_localIp; }
    std::string getGatewayIp() const { return m_gatewayIp; }
    std::string getAdapterName() const { return m_adapterName; }
    int getDeviceCount() const;

    NetworkInfo getNetworkInfo() const { return m_netInfo; }
    void setDeviceFlags(const std::string& ip, bool poisoned, bool cut, bool throttled, bool delayed);
    void updateDeviceHostname(const std::string& ip, const std::string& hostname, const std::string& os);

private:
    void listenerLoop();
    void spoofingLoop();
    void scanWorker();
    void shieldLoop();
    void sendArpRequest(pcap_t* handle, uint32_t targetIpNet);
    void sendArpReply(pcap_t* handle, uint32_t targetIpNet, const uint8_t* targetMac, uint32_t spoofIpNet);
    void sendArpReplyWithMac(pcap_t* handle, uint32_t targetIpNet, const uint8_t* targetMac, uint32_t senderIpNet, const uint8_t* senderMac);
    void sendGratuitousArp();
    std::string lookupVendor(const uint8_t mac[6]) const;
    
    void detectNetworkEnvironment(IP_ADAPTER_ADDRESSES* adapter);
    void detectWifiSSID();
    uint32_t prefixToMask(int prefixLength) const;

    std::atomic<bool> m_running;
    std::atomic<bool> m_scanning;
    std::thread m_listenerThread;
    std::thread m_spoofThread;
    std::thread m_scanThread;
    std::thread m_shieldThread;
    
    std::string m_localIp;
    std::string m_gatewayIp;
    std::string m_adapterName;
    std::string m_adapterGuid;
    uint32_t m_localIpNet;
    uint32_t m_gatewayIpNet;
    uint32_t m_subnetMask;
    int m_prefixLength;
    uint8_t m_localMac[6];
    uint8_t m_gatewayMac[6];
    bool m_gatewayMacKnown;
    
    pcap_t* m_recvHandle;
    pcap_t* m_sendHandle;

    std::map<std::string, DeviceInfo> m_devices;
    std::mutex m_devicesMutex;

    struct PoisonTarget {
        std::string ip;
        uint8_t mac[6];
        bool isVoid = false;
    };
    std::map<std::string, PoisonTarget> m_poisonTargets;
    std::mutex m_poisonMutex;

    NetworkInfo m_netInfo;

    // Shield state
    std::atomic<bool> m_shieldActive;

    // Turbo mode state
    std::atomic<bool> m_turboActive;

    // Auto-block state
    std::atomic<bool> m_autoBlockActive;
    std::set<std::string> m_knownDevices;    // IPs that were present when auto-block started
    std::set<std::string> m_blockedDevices;  // IPs that were auto-blocked
    std::mutex m_blockMutex;
};

#endif // ARPENGINE_H
