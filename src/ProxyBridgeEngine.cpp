#include "ProxyBridgeEngine.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include "resource.h"

ProxyBridgeEngine::ProxyBridgeEngine() {
    hJob = CreateJobObject(NULL, NULL);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
}

ProxyBridgeEngine::~ProxyBridgeEngine() {
    stopProxy();
    if (hJob) {
        CloseHandle(hJob);
    }
}

bool ProxyBridgeEngine::startProxy(const std::string& socksAddr, const std::string& httpAddr, bool sniff, bool body) {
    if (running) return false;
    clearLogs();

    SECURITY_ATTRIBUTES saAttr; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        appendLog("[ERROR] Failed to create stdout pipe.\n");
        return false;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStdOutWrite;
    si.hStdOutput = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hidden window

    ZeroMemory(&pi, sizeof(pi));

    // Dump resource to %TEMP%\gohpts_bridge.exe
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string exePath = std::string(tempPath) + "gohpts_bridge.exe";

    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_GOHPTS_EXE), RT_RCDATA);
    if (!hRes) {
        appendLog("[ERROR] Failed to locate gohpts resource inside the executable!\n");
        return false;
    }
    
    HGLOBAL hData = LoadResource(NULL, hRes);
    DWORD size = SizeofResource(NULL, hRes);
    void* pData = LockResource(hData);
    
    if (pData && size > 0) {
        std::ofstream outfile(exePath, std::ios::binary);
        if (outfile) {
            outfile.write((char*)pData, size);
            outfile.close();
        } else {
            appendLog("[ERROR] Failed to write resource to TEMP directory!\n");
            return false;
        }
    }

    std::stringstream cmd;
    cmd << "\"" << exePath << "\" -s " << socksAddr << " -l " << httpAddr << " -nocolor";
    
    if (sniff) {
        cmd << " -sniff";
    }
    if (body) {
        cmd << " -body";
    }

    std::string cmdLine = cmd.str();
    appendLog("[NFO] Launching: " + cmdLine + "\n");

    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        appendLog("[ERROR] CreateProcess failed!\n");
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdOutRead);
        hStdOutWrite = NULL;
        hStdOutRead = NULL;
        return false;
    }

    if (hJob) {
        AssignProcessToJobObject(hJob, pi.hProcess);
    }
    
    ResumeThread(pi.hThread);

    hProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    
    // Close our copy of the write end of the pipe, so read loop terminates when process exits
    CloseHandle(hStdOutWrite);
    hStdOutWrite = NULL;

    running = true;
    outputThread = std::thread(&ProxyBridgeEngine::readOutputLoop, this);

    return true;
}

void ProxyBridgeEngine::stopProxy() {
    if (!running) return;
    
    running = false;

    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        hProcess = NULL;
    }

    if (hStdOutRead) {
        CloseHandle(hStdOutRead);
        hStdOutRead = NULL;
    }

    if (outputThread.joinable()) {
        outputThread.join();
    }
    
    // Clean up dropped file
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string exePath = std::string(tempPath) + "gohpts_bridge.exe";
    DeleteFileA(exePath.c_str());
    
    appendLog("[NFO] Proxy stopped.\n");
}

bool ProxyBridgeEngine::isRunning() const {
    return running;
}

std::string ProxyBridgeEngine::getLogs() {
    std::lock_guard<std::mutex> lock(logMutex);
    return proxyLogs;
}

void ProxyBridgeEngine::clearLogs() {
    std::lock_guard<std::mutex> lock(logMutex);
    proxyLogs.clear();
}

void ProxyBridgeEngine::appendLog(const std::string& text) {
    std::lock_guard<std::mutex> lock(logMutex);
    proxyLogs += text;
    if (proxyLogs.size() > 10000) { // arbitrary limit so we don't blow up memory
        proxyLogs = proxyLogs.substr(proxyLogs.size() - 8000);
    }
}

void ProxyBridgeEngine::readOutputLoop() {
    DWORD dwRead; 
    char chBuf[4096]; 
    bool bSuccess = FALSE;

    while (running) { 
        bSuccess = ReadFile(hStdOutRead, chBuf, sizeof(chBuf)-1, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) break; 
        
        chBuf[dwRead] = '\0';
        appendLog(std::string(chBuf));
    }
    
    running = false;
    if (hProcess) {
        CloseHandle(hProcess);
        hProcess = NULL;
    }
    if (hStdOutRead) {
        CloseHandle(hStdOutRead);
        hStdOutRead = NULL;
    }
}
