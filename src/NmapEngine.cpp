#include "NmapEngine.h"
#include <iostream>
#include <vector>

NmapEngine::NmapEngine() : m_scanning(false), m_hChildStd_OUT_Rd(NULL), m_hChildStd_OUT_Wr(NULL), m_hProcess(NULL) {}

NmapEngine::~NmapEngine() {
    stopScan();
}

bool NmapEngine::startScan(const std::string& target, const std::string& flags) {
    if (m_scanning) return false;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0)) {
        return false;
    }
    if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        return false;
    }

    std::string cmdLine = "nmap " + flags + " " + target;
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = m_hChildStd_OUT_Wr;
    si.hStdOutput = m_hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    // Create process
    std::string fullCmd = "cmd.exe /c " + cmdLine;
    
    // We use a mutable buffer for CreateProcess
    std::vector<char> cmdBuffer(fullCmd.begin(), fullCmd.end());
    cmdBuffer.push_back('\0');

    if (!CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(m_hChildStd_OUT_Rd);
        CloseHandle(m_hChildStd_OUT_Wr);
        m_hChildStd_OUT_Rd = NULL;
        m_hChildStd_OUT_Wr = NULL;
        return false;
    }

    m_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    m_scanning = true;
    m_newOutput.clear();
    m_fullOutput = "Starting NMAP Scan: " + cmdLine + "\n\n";

    m_readThread = std::thread(&NmapEngine::readLoop, this);
    return true;
}

void NmapEngine::stopScan() {
    if (!m_scanning) return;
    m_scanning = false;

    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = NULL;
    }

    if (m_hChildStd_OUT_Wr) {
        CloseHandle(m_hChildStd_OUT_Wr);
        m_hChildStd_OUT_Wr = NULL;
    }

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    if (m_hChildStd_OUT_Rd) {
        CloseHandle(m_hChildStd_OUT_Rd);
        m_hChildStd_OUT_Rd = NULL;
    }
}

void NmapEngine::readLoop() {
    DWORD dwRead;
    CHAR chBuf[4096];
    bool bSuccess = FALSE;

    while (m_scanning) {
        bSuccess = ReadFile(m_hChildStd_OUT_Rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) break;

        chBuf[dwRead] = '\0';
        std::string outStr(chBuf);

        {
            std::lock_guard<std::mutex> lock(m_outputMutex);
            m_newOutput += outStr;
            m_fullOutput += outStr;
        }
    }

    m_scanning = false;
    
    {
        std::lock_guard<std::mutex> lock(m_outputMutex);
        m_newOutput += "\n[SCAN FINISHED]\n";
        m_fullOutput += "\n[SCAN FINISHED]\n";
    }
    
    // Cleanup output write handle so subsequent reads (if any) would fail correctly
    if (m_hChildStd_OUT_Wr) {
        CloseHandle(m_hChildStd_OUT_Wr);
        m_hChildStd_OUT_Wr = NULL;
    }
}

std::string NmapEngine::getOutput() {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    std::string res = m_newOutput;
    m_newOutput.clear();
    return res;
}

std::string NmapEngine::getFullOutput() const {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    return m_fullOutput;
}
