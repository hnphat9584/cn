#pragma once
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <sstream>
#include <vector>

// Process and Application Management
class ProcessManager {
private:
    static std::string escapeJson(const std::string& str);    
public:
    static std::string listProcesses();    
    static std::string startProcess(const std::string& path);    
    static std::string stopProcess(DWORD pid);    
    static std::string listApplications();
};