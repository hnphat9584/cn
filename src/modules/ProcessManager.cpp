#include "../../include/modules/ProcessManager.hpp"

std::string ProcessManager::escapeJson(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

std::string ProcessManager::listProcesses() {
    std::stringstream ss;
    ss << "[";
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return "[]";
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    bool first = true;
    if (Process32First(snapshot, &pe32)) {
        do {
            if (!first) ss << ",";
            first = false;
            
            ss << "{\"pid\":" << pe32.th32ProcessID 
                << ",\"name\":\"" << escapeJson(pe32.szExeFile) << "\"}";
        } while (Process32Next(snapshot, &pe32));
    }
    
    CloseHandle(snapshot);
    ss << "]";
    return ss.str();
}

std::string ProcessManager::startProcess(const std::string& path) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessA(NULL, (LPSTR)path.c_str(), NULL, NULL, FALSE, 
                        0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return "{\"status\":\"success\",\"pid\":" + std::to_string(pi.dwProcessId) + "}";
    }
    return "{\"status\":\"error\",\"message\":\"Failed to start process\"}";
}

std::string ProcessManager::stopProcess(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        return "{\"status\":\"error\",\"message\":\"Cannot open process\"}";
    }
    
    bool success = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    
    if (success) {
        return "{\"status\":\"success\"}";
    }
    return "{\"status\":\"error\",\"message\":\"Failed to terminate process\"}";
}

std::string ProcessManager::listApplications() {
    std::stringstream ss;
    ss << "[";
    bool first = true;
    int count = 0;
    
    std::vector<std::string> regPaths = {
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };
    
    for (const auto& regPath : regPaths) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD index = 0;
            char subKeyName[256];
            DWORD subKeyLen = 256;
            
            while (RegEnumKeyExA(hKey, index++, subKeyName, &subKeyLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                HKEY hSubKey;
                if (RegOpenKeyExA(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                    char displayName[512] = {0};
                    char installLocation[512] = {0};
                    char publisher[256] = {0};
                    DWORD dataSize = 512;
                    DWORD type;
                    
                    if (RegQueryValueExA(hSubKey, "DisplayName", NULL, &type, (LPBYTE)displayName, &dataSize) == ERROR_SUCCESS) {
                        std::string nameStr(displayName);
                        if (nameStr.find("Update for") != std::string::npos ||
                            nameStr.find("Security Update") != std::string::npos ||
                            nameStr.find("KB") != std::string::npos) {
                            RegCloseKey(hSubKey);
                            subKeyLen = 256;
                            continue;
                        }
                        
                        dataSize = 256;
                        RegQueryValueExA(hSubKey, "Publisher", NULL, &type, (LPBYTE)publisher, &dataSize);
                        
                        dataSize = 512;
                        RegQueryValueExA(hSubKey, "InstallLocation", NULL, &type, (LPBYTE)installLocation, &dataSize);
                        
                        std::string exePath;
                        if (strlen(installLocation) > 0) {
                            WIN32_FIND_DATAA findData;
                            std::string searchPattern = std::string(installLocation) + "\\*.exe";
                            HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);
                            
                            if (hFind != INVALID_HANDLE_VALUE) {
                                do {
                                    std::string exeName = findData.cFileName;
                                    std::string lowerExeName = exeName;
                                    std::transform(lowerExeName.begin(), lowerExeName.end(), lowerExeName.begin(), ::tolower);
                                    
                                    if (lowerExeName.find("unins") == std::string::npos &&
                                        lowerExeName.find("uninst") == std::string::npos &&
                                        lowerExeName.find("uninstall") == std::string::npos &&
                                        lowerExeName.find("setup") == std::string::npos &&
                                        lowerExeName.find("installer") == std::string::npos) {
                                        exePath = std::string(installLocation) + "\\" + exeName;
                                        break;
                                    }
                                } while (FindNextFileA(hFind, &findData));
                                FindClose(hFind);
                            }
                        }
                        
                        if (!first) ss << ",";
                        first = false;
                        
                        ss << "{\"name\":\"" << escapeJson(displayName);
                        if (strlen(publisher) > 0) {
                            ss << "\",\"publisher\":\"" << escapeJson(publisher);
                        }
                        ss << "\",\"exe\":\"" << (exePath.empty() ? "N/A" : escapeJson(exePath))
                            << "\",\"path\":\"" << (exePath.empty() ? escapeJson(installLocation) : escapeJson(exePath)) << "\"}";
                        
                        count++;
                        if (count >= 150) break;
                    }
                    RegCloseKey(hSubKey);
                }
                subKeyLen = 256;
                if (count >= 150) break;
            }
            RegCloseKey(hKey);
            if (count >= 150) break;
        }
    }
    
    ss << "]";
    return ss.str();
}