#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>

class ProxyBridgeEngine {
public:
    ProxyBridgeEngine();
    ~ProxyBridgeEngine();

    bool startProxy(const std::string& socksAddr, const std::string& httpAddr, bool sniff = false, bool body = false);
    void stopProxy();
    bool isRunning() const;

    std::string getLogs();
    void clearLogs();
    void appendLog(const std::string& text);

private:
    std::atomic<bool> running{false};
    HANDLE hProcess = NULL;
    HANDLE hJob = NULL;

    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;

    std::thread outputThread;
    
    std::mutex logMutex;
    std::string proxyLogs;

    void readOutputLoop();
};
