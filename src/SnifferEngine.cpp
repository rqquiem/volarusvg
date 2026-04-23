// src/SnifferEngine.cpp
#include "SnifferEngine.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <cctype>

SnifferEngine::SnifferEngine()
    : m_handle(nullptr), m_capturing(false), m_paused(false),
      m_packetIndex(0), m_captureRate(0.0), m_totalBytes(0) {}

SnifferEngine::~SnifferEngine() {
    stopCapture();
}

bool SnifferEngine::initialize(const std::string& adapterName, const std::string& localIp) {
    m_localIp = localIp;
    char err[PCAP_ERRBUF_SIZE];
    m_handle = pcap_open_live(adapterName.c_str(), 65536, 1, 1, err);
    if (!m_handle) {
        std::cerr << "[SnifferEngine] pcap_open_live failed: " << err << std::endl;
        return false;
    }
    return true;
}

void SnifferEngine::startCapture() {
    if (m_capturing) return;
    m_capturing = true;
    m_paused = false;
    m_packetIndex = 0;
    m_totalBytes = 0;
    m_captureRate = 0.0;
    _prevPacketCount = 0;
    m_protoStats = {};

    {
        std::lock_guard<std::mutex> lock(m_packetsMutex);
        m_packets.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_trafficStats.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_activityMutex);
        m_activityMap.clear();
        m_dnsCache.clear();
    }

    m_captureStart = std::chrono::steady_clock::now();
    _lastRateCalc = m_captureStart;
    m_captureThread = std::thread(&SnifferEngine::captureLoop, this);
}

void SnifferEngine::stopCapture() {
    m_capturing = false;
    m_paused = false;
    if (m_handle) {
        pcap_breakloop(m_handle);
    }
    if (m_captureThread.joinable()) m_captureThread.join();
    if (m_handle) {
        pcap_close(m_handle);
        m_handle = nullptr;
    }
}

void SnifferEngine::pauseCapture() { m_paused = true; }
void SnifferEngine::resumeCapture() { m_paused = false; }

void SnifferEngine::clearCapture() {
    std::lock_guard<std::mutex> lock(m_packetsMutex);
    m_packets.clear();
    m_packetIndex = 0;
    m_totalBytes = 0;
    m_protoStats = {};
    {
        std::lock_guard<std::mutex> lock2(m_statsMutex);
        m_trafficStats.clear();
    }
    {
        std::lock_guard<std::mutex> lock2(m_activityMutex);
        m_activityMap.clear();
        m_dnsCache.clear();
    }
}

double SnifferEngine::getCaptureDuration() const {
    if (!m_capturing) return 0.0;
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - m_captureStart).count();
}

void SnifferEngine::captureLoop() {
    struct pcap_pkthdr* header;
    const u_char* data;

    while (m_capturing) {
        int res = pcap_next_ex(m_handle, &header, &data);
        if (res <= 0) {
            if (res == -2) break;
            continue;
        }
        if (m_paused) continue;

        auto now = std::chrono::steady_clock::now();
        double ts = std::chrono::duration<double>(now - m_captureStart).count();
        parsePacket(data, header->caplen, ts);
    }
}

void SnifferEngine::parsePacket(const uint8_t* data, uint32_t length, double timestamp) {
    if (length < sizeof(EthernetHeader)) return;

    const EthernetHeader* eth = (const EthernetHeader*)data;
    uint16_t etherType = ntohs(eth->type);

    SniffedPacket pkt;
    pkt.index = ++m_packetIndex;
    pkt.timestamp = timestamp;
    pkt.length = length;
    pkt.srcPort = 0;
    pkt.dstPort = 0;
    pkt.tcpFlags = 0;

    size_t rawLen = (length > 256) ? 256 : length;
    pkt.rawData.assign(data, data + rawLen);

    const uint8_t* payload = data + sizeof(EthernetHeader);
    uint32_t remaining = length - sizeof(EthernetHeader);

    if (etherType == ETHERTYPE_ARP) {
        pkt.protocol = "ARP";
        m_protoStats.arp++;

        if (remaining >= sizeof(ArpHeader)) {
            const ArpHeader* arp = (const ArpHeader*)payload;
            struct in_addr a;
            a.S_un.S_addr = arp->sender_ip;
            pkt.srcIp = inet_ntoa(a);
            a.S_un.S_addr = arp->target_ip;
            pkt.dstIp = inet_ntoa(a);

            if (ntohs(arp->opcode) == 1)
                pkt.info = "Who has " + pkt.dstIp + "? Tell " + pkt.srcIp;
            else if (ntohs(arp->opcode) == 2) {
                pkt.info = pkt.srcIp + " is at ";
                std::stringstream ss;
                for (int i = 0; i < 6; ++i)
                    ss << std::hex << std::setw(2) << std::setfill('0') << (int)arp->sender_mac[i] << (i < 5 ? ":" : "");
                pkt.info += ss.str();
            } else
                pkt.info = "ARP opcode " + std::to_string(ntohs(arp->opcode));
        } else {
            pkt.srcIp = "N/A";
            pkt.dstIp = "N/A";
            pkt.info = "ARP (truncated)";
        }

    } else if (etherType == ETHERTYPE_IPV4) {
        if (remaining < sizeof(IPv4Header)) {
            pkt.protocol = "IPv4";
            pkt.srcIp = "?";
            pkt.dstIp = "?";
            pkt.info = "IPv4 (truncated)";
            m_protoStats.other++;
        } else {
            const IPv4Header* ip = (const IPv4Header*)payload;
            struct in_addr a;
            a.S_un.S_addr = ip->src_addr;
            pkt.srcIp = inet_ntoa(a);
            a.S_un.S_addr = ip->dst_addr;
            pkt.dstIp = inet_ntoa(a);

            uint8_t ihl = ip->headerLength();
            const uint8_t* l4 = payload + ihl;
            uint32_t l4Remaining = remaining - ihl;

            switch (ip->protocol) {
            case IPPROTO_TCP_: {
                pkt.protocol = "TCP";
                m_protoStats.tcp++;
                if (l4Remaining >= sizeof(TcpHeader)) {
                    const TcpHeader* tcp = (const TcpHeader*)l4;
                    pkt.srcPort = ntohs(tcp->src_port);
                    pkt.dstPort = ntohs(tcp->dst_port);
                    pkt.tcpFlags = tcp->flags;

                    std::string flags = formatTcpFlags(tcp->flags);
                    std::string svc = getServiceName(pkt.dstPort);
                    if (svc.empty()) svc = getServiceName(pkt.srcPort);

                    pkt.info = std::to_string(pkt.srcPort) + " -> " + std::to_string(pkt.dstPort);
                    pkt.info += " [" + flags + "]";
                    pkt.info += " Seq=" + std::to_string(ntohl(tcp->seq_num));
                    if (tcp->isACK()) pkt.info += " Ack=" + std::to_string(ntohl(tcp->ack_num));
                    pkt.info += " Win=" + std::to_string(ntohs(tcp->window));
                    if (!svc.empty()) pkt.info += " (" + svc + ")";

                    // ===== Activity Intelligence: Parse TCP payloads =====
                    uint8_t tcpHeaderLen = tcp->headerLength();
                    if (l4Remaining > tcpHeaderLen) {
                        const uint8_t* tcpPayload = l4 + tcpHeaderLen;
                        uint32_t tcpPayloadLen = l4Remaining - tcpHeaderLen;

                        // TLS ClientHello → extract SNI
                        if (pkt.dstPort == 443 || pkt.dstPort == 8443 || pkt.dstPort == 993 || pkt.dstPort == 995 || pkt.dstPort == 465) {
                            parseTlsSni(tcpPayload, tcpPayloadLen, pkt.srcIp, pkt.dstIp, timestamp);
                        }
                        // HTTP → extract Host header
                        if (pkt.dstPort == 80 || pkt.dstPort == 8080 || pkt.dstPort == 8000) {
                            parseHttpHost(tcpPayload, tcpPayloadLen, pkt.srcIp, pkt.dstIp, timestamp);
                        }
                    }
                } else {
                    pkt.info = "TCP (truncated)";
                }
                break;
            }
            case IPPROTO_UDP_: {
                pkt.protocol = "UDP";
                m_protoStats.udp++;
                if (l4Remaining >= sizeof(UdpHeader)) {
                    const UdpHeader* udp = (const UdpHeader*)l4;
                    pkt.srcPort = ntohs(udp->src_port);
                    pkt.dstPort = ntohs(udp->dst_port);
                    uint16_t udpLen = ntohs(udp->length);

                    std::string svc = getServiceName(pkt.dstPort);
                    if (svc.empty()) svc = getServiceName(pkt.srcPort);

                    pkt.info = std::to_string(pkt.srcPort) + " -> " + std::to_string(pkt.dstPort);
                    pkt.info += " Len=" + std::to_string(udpLen);
                    if (!svc.empty()) pkt.info += " (" + svc + ")";

                    // ===== Activity Intelligence: DNS packet parsing =====
                    if (pkt.srcPort == 53 || pkt.dstPort == 53) {
                        const uint8_t* dnsData = l4 + sizeof(UdpHeader);
                        uint32_t dnsLen = l4Remaining - sizeof(UdpHeader);
                        parseDnsPacket(dnsData, dnsLen, pkt.srcIp, pkt.dstIp, timestamp);
                    }
                } else {
                    pkt.info = "UDP (truncated)";
                }
                break;
            }
            case IPPROTO_ICMP_: {
                pkt.protocol = "ICMP";
                m_protoStats.icmp++;
                if (l4Remaining >= sizeof(IcmpHeader)) {
                    const IcmpHeader* icmp = (const IcmpHeader*)l4;
                    switch (icmp->type) {
                    case 0:  pkt.info = "Echo Reply"; break;
                    case 3:  pkt.info = "Destination Unreachable (code=" + std::to_string(icmp->code) + ")"; break;
                    case 8:  pkt.info = "Echo Request"; break;
                    case 11: pkt.info = "Time Exceeded"; break;
                    default: pkt.info = "Type=" + std::to_string(icmp->type) + " Code=" + std::to_string(icmp->code); break;
                    }
                } else {
                    pkt.info = "ICMP (truncated)";
                }
                break;
            }
            default:
                pkt.protocol = "IPv4/" + std::to_string(ip->protocol);
                m_protoStats.other++;
                pkt.info = "Protocol " + std::to_string(ip->protocol);
                break;
            }

            // Update per-IP traffic stats
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                bool isIncoming = (pkt.dstIp == m_localIp);
                bool isOutgoing = (pkt.srcIp == m_localIp);

                std::string remoteIp = isOutgoing ? pkt.dstIp : pkt.srcIp;
                auto& stats = m_trafficStats[remoteIp];

                if (isIncoming) {
                    stats.bytesIn += length;
                    stats.packetsIn++;
                } else if (isOutgoing) {
                    stats.bytesOut += length;
                    stats.packetsOut++;
                } else {
                    stats.bytesIn += length;
                    stats.packetsIn++;
                }
            }
        }
    } else {
        pkt.protocol = "0x" + ([&]() {
            std::stringstream ss;
            ss << std::hex << std::setw(4) << std::setfill('0') << etherType;
            return ss.str();
        })();
        pkt.srcIp = "N/A";
        pkt.dstIp = "N/A";
        pkt.info = "EtherType 0x" + ([&]() {
            std::stringstream ss;
            ss << std::hex << std::setw(4) << std::setfill('0') << etherType;
            return ss.str();
        })();
        m_protoStats.other++;
    }

    m_totalBytes += length;

    {
        std::lock_guard<std::mutex> lock(m_packetsMutex);
        m_packets.push_back(std::move(pkt));
        while (m_packets.size() > MAX_PACKETS)
            m_packets.pop_front();
    }
}

// ============================================================================
// DNS Packet Parser — extract domain→IP mappings from DNS responses
// ============================================================================
void SnifferEngine::parseDnsPacket(const uint8_t* data, uint32_t len, const std::string& srcIp, const std::string& dstIp, double timestamp) {
    // DNS header: 12 bytes minimum
    if (len < 12) return;

    uint16_t flags = (data[2] << 8) | data[3];
    bool isResponse = (flags & 0x8000) != 0;
    
    uint16_t qdCount = (data[4] << 8) | data[5];
    uint16_t anCount = (data[6] << 8) | data[7];

    // We want DNS queries (for the domain name) and responses (for IP→domain mapping)
    uint32_t offset = 12;

    // Parse question section to extract queried domain
    std::string queriedDomain;
    for (uint16_t q = 0; q < qdCount && offset < len; ++q) {
        std::string domain;
        // Read domain name labels
        while (offset < len && data[offset] != 0) {
            if ((data[offset] & 0xC0) == 0xC0) {
                // Compression pointer — follow it
                if (offset + 1 >= len) break;
                uint16_t ptr = ((data[offset] & 0x3F) << 8) | data[offset + 1];
                offset += 2;
                // Read from pointer location
                while (ptr < len && data[ptr] != 0) {
                    if ((data[ptr] & 0xC0) == 0xC0) break; // nested compression, bail
                    uint8_t labelLen = data[ptr];
                    ptr++;
                    if (ptr + labelLen > len) break;
                    if (!domain.empty()) domain += ".";
                    domain.append((const char*)(data + ptr), labelLen);
                    ptr += labelLen;
                }
                goto doneLabel;
            }
            uint8_t labelLen = data[offset];
            offset++;
            if (offset + labelLen > len) break;
            if (!domain.empty()) domain += ".";
            domain.append((const char*)(data + offset), labelLen);
            offset += labelLen;
        }
        if (offset < len) offset++; // skip null terminator
        doneLabel:
        offset += 4; // skip QTYPE (2) + QCLASS (2)
        if (q == 0) queriedDomain = domain;
    }

    // For DNS queries: record that the requester is looking up this domain
    if (!isResponse && !queriedDomain.empty()) {
        // The source IP is the device making the query
        recordActivity(srcIp, queriedDomain, "DNS", timestamp);
        return;
    }

    // For DNS responses: parse answer records to build IP→domain cache
    if (isResponse && anCount > 0 && !queriedDomain.empty()) {
        for (uint16_t a = 0; a < anCount && offset < len; ++a) {
            // Skip name (could be compressed)
            if (offset >= len) break;
            if ((data[offset] & 0xC0) == 0xC0) {
                offset += 2; // compression pointer
            } else {
                while (offset < len && data[offset] != 0) {
                    offset += data[offset] + 1;
                }
                if (offset < len) offset++; // null terminator
            }

            if (offset + 10 > len) break;
            uint16_t rtype = (data[offset] << 8) | data[offset + 1];
            // Skip RCLASS (2) + TTL (4)
            offset += 8;
            uint16_t rdlen = (data[offset] << 8) | data[offset + 1];
            offset += 2;

            if (rtype == 1 && rdlen == 4 && offset + 4 <= len) {
                // A record — IPv4 address
                struct in_addr addr;
                memcpy(&addr, data + offset, 4);
                std::string resolvedIp = inet_ntoa(addr);

                // Cache this IP→domain mapping
                {
                    std::lock_guard<std::mutex> lock(m_activityMutex);
                    m_dnsCache[resolvedIp] = { queriedDomain, timestamp };
                }
            }
            offset += rdlen;
        }

        // Also record the activity for the device that made the query (dst of response)
        recordActivity(dstIp, queriedDomain, "DNS", timestamp);
    }
}

// ============================================================================
// TLS SNI Extractor — parse ClientHello to get server hostname
// ============================================================================
void SnifferEngine::parseTlsSni(const uint8_t* payload, uint32_t payloadLen, const std::string& srcIp, const std::string& dstIp, double timestamp) {
    // TLS Record: content_type(1) + version(2) + length(2) + data
    if (payloadLen < 6) return;

    uint8_t contentType = payload[0];
    if (contentType != 0x16) return; // 0x16 = Handshake

    uint16_t recordLen = (payload[3] << 8) | payload[4];
    uint32_t pos = 5;

    // Handshake header: type(1) + length(3)
    if (pos + 4 > payloadLen) return;
    uint8_t handshakeType = payload[pos];
    if (handshakeType != 0x01) return; // 0x01 = ClientHello

    pos += 4; // skip type + 3-byte length

    // ClientHello: version(2) + random(32) + session_id_len(1) + ...
    if (pos + 34 > payloadLen) return;
    pos += 34; // version + random

    // Session ID
    if (pos >= payloadLen) return;
    uint8_t sessionIdLen = payload[pos];
    pos += 1 + sessionIdLen;

    // Cipher Suites
    if (pos + 2 > payloadLen) return;
    uint16_t cipherSuitesLen = (payload[pos] << 8) | payload[pos + 1];
    pos += 2 + cipherSuitesLen;

    // Compression Methods
    if (pos >= payloadLen) return;
    uint8_t compressionLen = payload[pos];
    pos += 1 + compressionLen;

    // Extensions
    if (pos + 2 > payloadLen) return;
    uint16_t extensionsLen = (payload[pos] << 8) | payload[pos + 1];
    pos += 2;

    uint32_t extensionsEnd = pos + extensionsLen;
    if (extensionsEnd > payloadLen) extensionsEnd = payloadLen;

    while (pos + 4 <= extensionsEnd) {
        uint16_t extType = (payload[pos] << 8) | payload[pos + 1];
        uint16_t extLen  = (payload[pos + 2] << 8) | payload[pos + 3];
        pos += 4;

        if (extType == 0x0000 && extLen > 0) { // SNI extension
            // SNI list: total_length(2) + type(1) + name_length(2) + name
            if (pos + 5 <= extensionsEnd) {
                // uint16_t sniListLen = (payload[pos] << 8) | payload[pos + 1];
                uint8_t sniType = payload[pos + 2];
                uint16_t sniLen = (payload[pos + 3] << 8) | payload[pos + 4];

                if (sniType == 0x00 && pos + 5 + sniLen <= extensionsEnd && sniLen > 0 && sniLen < 256) {
                    std::string hostname((const char*)(payload + pos + 5), sniLen);
                    
                    // Validate: hostname should contain printable chars and at least one dot
                    bool valid = hostname.find('.') != std::string::npos;
                    for (char c : hostname) {
                        if (!isprint((unsigned char)c)) { valid = false; break; }
                    }
                    
                    if (valid) {
                        recordActivity(srcIp, hostname, "SNI", timestamp, dstIp, "https://" + hostname + "/");
                        // Also cache the destination IP → hostname
                        {
                            std::lock_guard<std::mutex> lock(m_activityMutex);
                            m_dnsCache[dstIp] = { hostname, timestamp };
                        }
                    }
                }
            }
            break; // Found SNI, no need to continue
        }

        pos += extLen;
    }
}

// ============================================================================
// HTTP Host Header Extractor
// ============================================================================
void SnifferEngine::parseHttpHost(const uint8_t* payload, uint32_t payloadLen, const std::string& srcIp, const std::string& dstIp, double timestamp) {
    if (payloadLen < 16) return;
    
    // Check if it starts with a known HTTP method
    bool isHttp = false;
    std::string httpMethod;
    const char* methods[] = { "GET ", "POST ", "PUT ", "DELETE ", "HEAD ", "OPTIONS ", "PATCH ", "CONNECT " };
    for (const char* m : methods) {
        size_t mlen = strlen(m);
        if (payloadLen >= mlen && memcmp(payload, m, mlen) == 0) {
            isHttp = true;
            httpMethod = std::string(m, mlen - 1); // strip trailing space
            break;
        }
    }
    if (!isHttp) return;

    std::string data((const char*)payload, (std::min)(payloadLen, (uint32_t)4096));
    
    // Extract the request URI from the first line: "GET /path/to/page HTTP/1.1"
    std::string requestPath;
    size_t firstLineEnd = data.find("\r\n");
    if (firstLineEnd != std::string::npos) {
        std::string firstLine = data.substr(0, firstLineEnd);
        // Find the path between method and HTTP/x.x
        size_t pathStart = firstLine.find(' ');
        if (pathStart != std::string::npos) {
            pathStart++;
            size_t pathEnd = firstLine.find(' ', pathStart);
            if (pathEnd != std::string::npos) {
                requestPath = firstLine.substr(pathStart, pathEnd - pathStart);
            }
        }
    }

    // Find "Host:" header
    std::string host;
    std::string hostWithPort;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t lineEnd = data.find("\r\n", pos);
        if (lineEnd == std::string::npos) break;
        
        std::string line = data.substr(pos, lineEnd - pos);
        pos = lineEnd + 2;
        
        if (line.empty()) break;
        
        if (line.size() > 5) {
            std::string prefix = line.substr(0, 5);
            for (char& c : prefix) c = (char)tolower((unsigned char)c);
            if (prefix == "host:") {
                std::string h = line.substr(5);
                size_t start = h.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    hostWithPort = h.substr(start);
                    host = hostWithPort;
                    size_t colonPos = host.find(':');
                    if (colonPos != std::string::npos) {
                        host = host.substr(0, colonPos);
                    }
                }
                break;
            }
        }
    }

    if (!host.empty()) {
        // Build the full URL
        std::string fullUrl = "http://" + hostWithPort;
        if (!requestPath.empty() && requestPath != "/") {
            fullUrl += requestPath;
        } else {
            fullUrl += "/";
        }

        recordActivity(srcIp, host, "HTTP", timestamp, dstIp, fullUrl);
        {
            std::lock_guard<std::mutex> lock(m_activityMutex);
            m_dnsCache[dstIp] = { host, timestamp };
        }
    }
}

// ============================================================================
// Record detected activity for an IP
// ============================================================================
void SnifferEngine::recordActivity(const std::string& ip, const std::string& domain, const std::string& method, double timestamp, const std::string& destIp, const std::string& url) {
    if (domain.empty()) return;
    
    // Normalize domain to lowercase
    std::string normalized = domain;
    for (char& c : normalized) c = (char)tolower((unsigned char)c);
    
    // Skip internal/local domains
    if (normalized.find(".local") != std::string::npos) return;
    if (normalized.find(".arpa") != std::string::npos) return;
    if (normalized.find(".lan") != std::string::npos) return;
    if (normalized == "localhost") return;

    std::string category = categorizeDomain(normalized);

    std::lock_guard<std::mutex> lock(m_activityMutex);
    auto& ipAct = m_activityMap[ip];
    ipAct.ip = ip;
    
    auto& act = ipAct.activities[normalized];
    if (act.domain.empty()) {
        act.domain = normalized;
        act.category = category;
        act.method = method;
        act.firstSeen = timestamp;
        ipAct.totalDomains++;
    }
    if (!destIp.empty()) act.destIp = destIp;
    act.lastSeen = timestamp;
    act.hitCount++;
    // Upgrade method priority: HTTP < DNS < SNI
    if (method == "SNI" || (method == "DNS" && act.method == "HTTP")) {
        act.method = method;
    }
    // Store URL if provided
    if (!url.empty()) {
        // Avoid duplicates of the exact same URL
        bool isDup = false;
        for (auto& u : act.urls) {
            if (u == url) { isDup = true; break; }
        }
        if (!isDup) {
            act.urls.push_back(url);
            // Keep only the most recent MAX_URLS
            while ((int)act.urls.size() > DetectedActivity::MAX_URLS) {
                act.urls.erase(act.urls.begin());
            }
        }
    }
}

// ============================================================================
// Domain Categorization
// ============================================================================
std::string SnifferEngine::categorizeDomain(const std::string& domain) const {
    // Social Media
    if (domain.find("facebook.com") != std::string::npos ||
        domain.find("fbcdn.net") != std::string::npos ||
        domain.find("instagram.com") != std::string::npos ||
        domain.find("twitter.com") != std::string::npos ||
        domain.find("x.com") != std::string::npos ||
        domain.find("tiktok.com") != std::string::npos ||
        domain.find("ttwstatic.com") != std::string::npos ||
        domain.find("snapchat.com") != std::string::npos ||
        domain.find("linkedin.com") != std::string::npos ||
        domain.find("reddit.com") != std::string::npos ||
        domain.find("threads.net") != std::string::npos ||
        domain.find("pinterest.com") != std::string::npos)
        return "Social";

    // Video Streaming
    if (domain.find("youtube.com") != std::string::npos ||
        domain.find("ytimg.com") != std::string::npos ||
        domain.find("googlevideo.com") != std::string::npos ||
        domain.find("netflix.com") != std::string::npos ||
        domain.find("nflxvideo.net") != std::string::npos ||
        domain.find("twitch.tv") != std::string::npos ||
        domain.find("disneyplus.com") != std::string::npos ||
        domain.find("hulu.com") != std::string::npos ||
        domain.find("primevideo.com") != std::string::npos ||
        domain.find("vimeo.com") != std::string::npos)
        return "Streaming";

    // Music
    if (domain.find("spotify.com") != std::string::npos ||
        domain.find("scdn.co") != std::string::npos ||
        domain.find("apple.com/music") != std::string::npos ||
        domain.find("soundcloud.com") != std::string::npos ||
        domain.find("deezer.com") != std::string::npos ||
        domain.find("pandora.com") != std::string::npos)
        return "Music";

    // Messaging
    if (domain.find("whatsapp.com") != std::string::npos ||
        domain.find("whatsapp.net") != std::string::npos ||
        domain.find("telegram.org") != std::string::npos ||
        domain.find("t.me") != std::string::npos ||
        domain.find("signal.org") != std::string::npos ||
        domain.find("discord.com") != std::string::npos ||
        domain.find("discord.gg") != std::string::npos ||
        domain.find("discordapp.com") != std::string::npos ||
        domain.find("slack.com") != std::string::npos ||
        domain.find("messenger.com") != std::string::npos ||
        domain.find("line.me") != std::string::npos ||
        domain.find("viber.com") != std::string::npos)
        return "Messaging";

    // Gaming
    if (domain.find("steampowered.com") != std::string::npos ||
        domain.find("steamcommunity.com") != std::string::npos ||
        domain.find("steamcontent.com") != std::string::npos ||
        domain.find("epicgames.com") != std::string::npos ||
        domain.find("riotgames.com") != std::string::npos ||
        domain.find("valve.net") != std::string::npos ||
        domain.find("ea.com") != std::string::npos ||
        domain.find("xbox.com") != std::string::npos ||
        domain.find("playstation.com") != std::string::npos ||
        domain.find("garena.com") != std::string::npos ||
        domain.find("roblox.com") != std::string::npos ||
        domain.find("mihoyo.com") != std::string::npos ||
        domain.find("hoyoverse.com") != std::string::npos)
        return "Gaming";

    // Search / Productivity
    if (domain.find("google.com") != std::string::npos ||
        domain.find("googleapis.com") != std::string::npos ||
        domain.find("gstatic.com") != std::string::npos ||
        domain.find("bing.com") != std::string::npos ||
        domain.find("duckduckgo.com") != std::string::npos ||
        domain.find("yahoo.com") != std::string::npos ||
        domain.find("baidu.com") != std::string::npos)
        return "Search";

    // Cloud / Storage
    if (domain.find("dropbox.com") != std::string::npos ||
        domain.find("drive.google.com") != std::string::npos ||
        domain.find("onedrive.live.com") != std::string::npos ||
        domain.find("icloud.com") != std::string::npos ||
        domain.find("amazonaws.com") != std::string::npos ||
        domain.find("cloudfront.net") != std::string::npos ||
        domain.find("azure.") != std::string::npos)
        return "Cloud";

    // Email
    if (domain.find("gmail.com") != std::string::npos ||
        domain.find("outlook.com") != std::string::npos ||
        domain.find("outlook.office365.com") != std::string::npos ||
        domain.find("mail.") != std::string::npos ||
        domain.find("protonmail.com") != std::string::npos)
        return "Email";

    // Shopping
    if (domain.find("amazon.") != std::string::npos ||
        domain.find("shopee.") != std::string::npos ||
        domain.find("lazada.") != std::string::npos ||
        domain.find("ebay.com") != std::string::npos ||
        domain.find("tokopedia.com") != std::string::npos ||
        domain.find("bukalapak.com") != std::string::npos ||
        domain.find("aliexpress.com") != std::string::npos)
        return "Shopping";

    // News
    if (domain.find("cnn.com") != std::string::npos ||
        domain.find("bbc.") != std::string::npos ||
        domain.find("reuters.com") != std::string::npos ||
        domain.find("nytimes.com") != std::string::npos ||
        domain.find("kompas.com") != std::string::npos ||
        domain.find("detik.com") != std::string::npos)
        return "News";

    // CDN / Infrastructure (don't hide, but categorize)
    if (domain.find("akamai") != std::string::npos ||
        domain.find("cloudflare") != std::string::npos ||
        domain.find("fastly") != std::string::npos ||
        domain.find("cdn.") != std::string::npos)
        return "CDN";

    // Microsoft services
    if (domain.find("microsoft.com") != std::string::npos ||
        domain.find("windows.com") != std::string::npos ||
        domain.find("office.com") != std::string::npos ||
        domain.find("live.com") != std::string::npos ||
        domain.find("msn.com") != std::string::npos)
        return "Microsoft";

    // Apple services
    if (domain.find("apple.com") != std::string::npos ||
        domain.find("icloud.com") != std::string::npos ||
        domain.find("apple-dns.net") != std::string::npos ||
        domain.find("mzstatic.com") != std::string::npos)
        return "Apple";

    // Ads / Tracking
    if (domain.find("doubleclick.net") != std::string::npos ||
        domain.find("googlesyndication.com") != std::string::npos ||
        domain.find("googleadservices.com") != std::string::npos ||
        domain.find("ads.") != std::string::npos ||
        domain.find("tracker.") != std::string::npos ||
        domain.find("analytics.") != std::string::npos ||
        domain.find("adsrvr.org") != std::string::npos)
        return "Ads/Tracking";

    return "Web";
}

// ============================================================================
// Public API for activity data
// ============================================================================
std::map<std::string, IPActivity> SnifferEngine::getActivityMap() const {
    std::lock_guard<std::mutex> lock(m_activityMutex);
    return m_activityMap;
}

std::string SnifferEngine::resolveDomain(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(m_activityMutex);
    auto it = m_dnsCache.find(ip);
    if (it != m_dnsCache.end()) return it->second.domain;
    return "";
}

size_t SnifferEngine::getTotalDomainsDetected() const {
    std::lock_guard<std::mutex> lock(m_activityMutex);
    size_t total = 0;
    for (const auto& [ip, act] : m_activityMap) {
        total += act.activities.size();
    }
    return total;
}

// ============================================================================
// Existing helpers
// ============================================================================
std::string SnifferEngine::formatTcpFlags(uint8_t flags) const {
    std::string s;
    if (flags & 0x02) s += "SYN ";
    if (flags & 0x10) s += "ACK ";
    if (flags & 0x01) s += "FIN ";
    if (flags & 0x04) s += "RST ";
    if (flags & 0x08) s += "PSH ";
    if (flags & 0x20) s += "URG ";
    if (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

std::string SnifferEngine::getServiceName(uint16_t port) const {
    switch (port) {
    case 20:   return "FTP-Data";
    case 21:   return "FTP";
    case 22:   return "SSH";
    case 23:   return "Telnet";
    case 25:   return "SMTP";
    case 53:   return "DNS";
    case 67:   return "DHCP-S";
    case 68:   return "DHCP-C";
    case 80:   return "HTTP";
    case 110:  return "POP3";
    case 143:  return "IMAP";
    case 443:  return "HTTPS";
    case 445:  return "SMB";
    case 993:  return "IMAPS";
    case 995:  return "POP3S";
    case 1080: return "SOCKS";
    case 1433: return "MSSQL";
    case 1883: return "MQTT";
    case 3306: return "MySQL";
    case 3389: return "RDP";
    case 5060: return "SIP";
    case 5222: return "XMPP";
    case 5353: return "mDNS";
    case 5432: return "PostgreSQL";
    case 5900: return "VNC";
    case 6379: return "Redis";
    case 8080: return "HTTP-Alt";
    case 8443: return "HTTPS-Alt";
    case 8883: return "MQTT-TLS";
    case 27017: return "MongoDB";
    default: return "";
    }
}

std::vector<SniffedPacket> SnifferEngine::getPackets(const SnifferFilter& filter, int maxCount) const {
    std::lock_guard<std::mutex> lock(m_packetsMutex);
    std::vector<SniffedPacket> result;
    result.reserve(maxCount);

    int count = 0;
    for (auto it = m_packets.rbegin(); it != m_packets.rend() && count < maxCount; ++it) {
        if (!filter.protocol.empty() && it->protocol != filter.protocol) continue;
        if (!filter.ipFilter.empty()) {
            if (it->srcIp.find(filter.ipFilter) == std::string::npos &&
                it->dstIp.find(filter.ipFilter) == std::string::npos) continue;
        }
        if (filter.portFilter != 0) {
            if (it->srcPort != filter.portFilter && it->dstPort != filter.portFilter) continue;
        }
        result.push_back(*it);
        count++;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

std::map<std::string, TrafficStats> SnifferEngine::getTrafficStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_trafficStats;
}

ProtocolStats SnifferEngine::getProtocolStats() const {
    return m_protoStats;
}

void SnifferEngine::updateRates() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - _lastRateCalc).count();
    if (elapsed < 0.5) return;

    uint32_t currentCount = m_packetIndex.load();
    m_captureRate = (currentCount - _prevPacketCount) / elapsed;
    _prevPacketCount = currentCount;
    _lastRateCalc = now;

    std::lock_guard<std::mutex> lock(m_statsMutex);
    for (auto& [ip, stats] : m_trafficStats) {
        stats.rateIn = (stats.bytesIn - stats._prevBytesIn) / elapsed;
        stats.rateOut = (stats.bytesOut - stats._prevBytesOut) / elapsed;
        stats._prevBytesIn = stats.bytesIn;
        stats._prevBytesOut = stats.bytesOut;

        stats.historyIn[stats.historyIdx % TrafficStats::HISTORY_LEN] = (float)stats.rateIn;
        stats.historyOut[stats.historyIdx % TrafficStats::HISTORY_LEN] = (float)stats.rateOut;
        stats.historyIdx++;
    }
}
