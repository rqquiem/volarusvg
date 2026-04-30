#ifndef NMAPENGINE_H
#define NMAPENGINE_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>

class NmapEngine {
public:
    NmapEngine();
    ~NmapEngine();

    bool startScan(const std::string& target, const std::string& flags);
    void stopScan();
    bool isScanning() const { return m_scanning; }

    std::string getOutput(); // Consumes new output
    std::string getFullOutput() const;

private:
    void readLoop();

    std::atomic<bool> m_scanning;
    HANDLE m_hChildStd_OUT_Rd;
    HANDLE m_hChildStd_OUT_Wr;
    HANDLE m_hProcess;
    
    std::thread m_readThread;

    mutable std::mutex m_outputMutex;
    std::string m_newOutput;
    std::string m_fullOutput;
};

#endif // NMAPENGINE_H
