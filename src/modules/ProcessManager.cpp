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
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    
    bool first = true;
    if (Process32FirstW(snapshot, &pe32)) {
        do {
            if (!first) ss << ",";
            first = false;

            std::string name = utf8_from_wstring(pe32.szExeFile);

            ss << "{\"pid\":" << "pe32.th32ProcessID" 
                << ",\"name\":\"" << "escapeJson(name)" << "\"}";
        } while (Process32NextW(snapshot, &pe32));
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

    std::vector<std::wstring> regPaths = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };

    for (const auto& regPath : regPaths) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;

        DWORD index = 0;
        wchar_t subKeyName[256];
        DWORD subKeyLen;

        while (true) {
            subKeyLen = 256;
            if (RegEnumKeyExW(hKey, index++, subKeyName, &subKeyLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                break;

            HKEY hSubKey;
            if (RegOpenKeyExW(hKey, subKeyName, 0, KEY_READ, &hSubKey) != ERROR_SUCCESS)
                continue;

            wchar_t displayNameW[512] = {};
            wchar_t installLocationW[512] = {};
            wchar_t publisherW[256] = {};
            DWORD type, size;

            size = sizeof(displayNameW);
            if (RegQueryValueExW(hSubKey, L"DisplayName", NULL, &type, (LPBYTE)displayNameW, &size) != ERROR_SUCCESS) {
                RegCloseKey(hSubKey);
                continue;
            }

            std::wstring displayNameStr(displayNameW);
            if (displayNameStr.find(L"Update for") != std::wstring::npos ||
                displayNameStr.find(L"Security Update") != std::wstring::npos ||
                displayNameStr.find(L"KB") != std::wstring::npos) {
                RegCloseKey(hSubKey);
                continue;
            }

            size = sizeof(publisherW);
            RegQueryValueExW(hSubKey, L"Publisher", NULL, &type, (LPBYTE)publisherW, &size);

            size = sizeof(installLocationW);
            RegQueryValueExW(hSubKey, L"InstallLocation", NULL, &type, (LPBYTE)installLocationW, &size);

            std::wstring exePathW;
            if (wcslen(installLocationW) > 0) {
                WIN32_FIND_DATAW findData;
                std::wstring searchPattern = std::wstring(installLocationW) + L"\\*.exe";
                HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);

                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        std::wstring exeName = findData.cFileName;
                        std::wstring lower = exeName;
                        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

                        if (lower.find(L"unins") == std::wstring::npos &&
                            lower.find(L"setup") == std::wstring::npos &&
                            lower.find(L"installer") == std::wstring::npos) {
                            exePathW = std::wstring(installLocationW) + L"\\" + exeName;
                            break;
                        }
                    } while (FindNextFileW(hFind, &findData));
                    FindClose(hFind);
                }
            }

            std::string name = utf8_from_wstring(displayNameW);
            std::string publisher = utf8_from_wstring(publisherW);
            std::string exePath = exePathW.empty()
                ? "N/A"
                : utf8_from_wstring(exePathW);
            std::string installPath = utf8_from_wstring(installLocationW);

            if (!first) ss << ",";
            first = false;

            ss << "{"
               << "\"name\":\"" << escapeJson(name) << "\"";

            if (!publisher.empty())
                ss << ",\"publisher\":\"" << escapeJson(publisher) << "\"";

            ss << ",\"exe\":\"" << escapeJson(exePath) << "\""
               << ",\"path\":\"" << escapeJson(exePath == "N/A" ? installPath : exePath) << "\""
               << "}";

            RegCloseKey(hSubKey);

            if (++count >= 150) break;
        }

        RegCloseKey(hKey);
        if (count >= 150) break;
    }

    ss << "]";
    return ss.str();
}


std::string ProcessManager::utf8_from_wstring(const std::wstring& w) {
    if (w.empty()) return {};

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        w.c_str(),
        (int)w.size(),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    std::string result(size, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        w.c_str(),
        (int)w.size(),
        &result[0],   // 🔥 FIX Ở ĐÂY
        size,
        nullptr,
        nullptr
    );

    return result;
}
