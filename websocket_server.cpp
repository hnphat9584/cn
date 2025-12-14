#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cctype>

#pragma comment(lib, "ws2_32.lib")

// Simple WebSocket implementation
class WebSocketServer {
private:
    SOCKET serverSocket;
    SOCKET clientSocket;
    
    std::string base64_encode(const unsigned char* data, size_t len) {
        static const char* base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        std::string ret;
        int i = 0, j = 0;
        unsigned char char_array_3[3], char_array_4[4];
        
        while (len--) {
            char_array_3[i++] = *(data++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;
                for(i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }
        if (i) {
            for(j = i; j < 3; j++) char_array_3[j] = '\0';
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            for (j = 0; j < i + 1; j++) ret += base64_chars[char_array_4[j]];
            while(i++ < 3) ret += '=';
        }
        return ret;
    }
    
    std::string sha1(const std::string& input) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        BYTE hash[20];
        DWORD hashLen = 20;
        
        CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
        CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
        CryptHashData(hHash, (BYTE*)input.c_str(), input.length(), 0);
        CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        
        return std::string((char*)hash, 20);
    }
    
    bool performHandshake() {
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) return false;
        
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        
        size_t keyPos = request.find("Sec-WebSocket-Key: ");
        if (keyPos == std::string::npos) return false;
        
        keyPos += 19;
        size_t keyEnd = request.find("\r\n", keyPos);
        std::string key = request.substr(keyPos, keyEnd - keyPos);
        
        std::string acceptKey = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string hash = sha1(acceptKey);
        std::string accept = base64_encode((unsigned char*)hash.c_str(), hash.length());
        
        std::string response = 
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
        
        send(clientSocket, response.c_str(), response.length(), 0);
        return true;
    }
    
public:
    bool start(int port) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
        
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) return false;
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return false;
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) return false;
        
        return true;
    }
    
    bool acceptClient() {
        clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) return false;
        
        return performHandshake();
    }
    
    std::string receiveMessage() {
        unsigned char buffer[4096];
        int bytesReceived = recv(clientSocket, (char*)buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) return "";
        
        bool fin = (buffer[0] & 0x80) != 0;
        int opcode = buffer[0] & 0x0F;
        bool masked = (buffer[1] & 0x80) != 0;
        uint64_t payloadLen = buffer[1] & 0x7F;
        
        int maskStart = 2;
        if (payloadLen == 126) {
            payloadLen = (buffer[2] << 8) | buffer[3];
            maskStart = 4;
        }
        
        unsigned char mask[4];
        if (masked) {
            memcpy(mask, &buffer[maskStart], 4);
            maskStart += 4;
        }
        
        std::string payload;
        for (uint64_t i = 0; i < payloadLen; i++) {
            payload += (masked ? (buffer[maskStart + i] ^ mask[i % 4]) : buffer[maskStart + i]);
        }
        
        return payload;
    }
    
    void sendMessage(const std::string& message) {
        std::vector<unsigned char> frame;
        frame.push_back(0x81);
        
        if (message.length() <= 125) {
            frame.push_back(message.length());
        } else if (message.length() <= 65535) {
            frame.push_back(126);
            frame.push_back((message.length() >> 8) & 0xFF);
            frame.push_back(message.length() & 0xFF);
        }
        
        for (char c : message) frame.push_back(c);
        
        send(clientSocket, (char*)frame.data(), frame.size(), 0);
    }
    
    void close() {
        if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
        if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
        WSACleanup();
    }
};

// Process and Application Management
class ProcessManager {
private:
    static std::string escapeJson(const std::string& str) {
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
    
public:
    static std::string listProcesses() {
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
    
    static std::string startProcess(const std::string& path) {
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
    
    static std::string stopProcess(DWORD pid) {
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
    
    static std::string listApplications() {
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
                                
                                if (exePath.empty()) {
                                    std::vector<std::string> subDirs = {"bin", "x64", "x86"};
                                    for (const auto& subDir : subDirs) {
                                        std::string subPath = std::string(installLocation) + "\\" + subDir;
                                        searchPattern = subPath + "\\*.exe";
                                        hFind = FindFirstFileA(searchPattern.c_str(), &findData);
                                        
                                        if (hFind != INVALID_HANDLE_VALUE) {
                                            do {
                                                std::string exeName = findData.cFileName;
                                                std::string lowerExeName = exeName;
                                                std::transform(lowerExeName.begin(), lowerExeName.end(), lowerExeName.begin(), ::tolower);
                                                
                                                if (lowerExeName.find("unins") == std::string::npos &&
                                                    lowerExeName.find("uninst") == std::string::npos &&
                                                    lowerExeName.find("uninstall") == std::string::npos) {
                                                    exePath = subPath + "\\" + exeName;
                                                    break;
                                                }
                                            } while (FindNextFileA(hFind, &findData));
                                            FindClose(hFind);
                                            if (!exePath.empty()) break;
                                        }
                                    }
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
        
        std::vector<std::pair<std::string, std::string>> commonApps = {
            {"Calculator", "calc.exe"},
            {"Notepad", "notepad.exe"},
            {"Paint", "mspaint.exe"},
            {"Command Prompt", "cmd.exe"},
            {"PowerShell", "powershell.exe"},
            {"Task Manager", "taskmgr.exe"},
            {"File Explorer", "explorer.exe"},
            {"Control Panel", "control.exe"},
            {"Character Map", "charmap.exe"},
            {"Snipping Tool", "SnippingTool.exe"},
            {"WordPad", "wordpad.exe"}
        };
        
        for (const auto& app : commonApps) {
            if (!first) ss << ",";
            first = false;
            ss << "{\"name\":\"" << escapeJson(app.first)
               << "\",\"exe\":\"" << escapeJson(app.second)
               << "\",\"path\":\"" << escapeJson(app.second) << "\"}";
            count++;
        }
        
        ss << "]";
        std::cout << "Found " << count << " applications" << std::endl;
        return ss.str();
    }
};

void handleClient(WebSocketServer& server) {
    std::cout << "Client connected!" << std::endl;
    
    while (true) {
        std::string message = server.receiveMessage();
        if (message.empty()) {
            std::cout << "Client disconnected" << std::endl;
            break;
        }
        
        std::cout << "Received: " << message << std::endl;
        
        std::string response;
        if (message.find("list_processes") != std::string::npos) {
            response = ProcessManager::listProcesses();
        }
        else if (message.find("list_applications") != std::string::npos) {
            response = ProcessManager::listApplications();
        }
        else if (message.find("start_process:") != std::string::npos) {
            std::string path = message.substr(14);
            response = ProcessManager::startProcess(path);
        }
        else if (message.find("stop_process:") != std::string::npos) {
            DWORD pid = std::stoul(message.substr(13));
            response = ProcessManager::stopProcess(pid);
        }
        else {
            response = "{\"status\":\"error\",\"message\":\"Unknown command\"}";
        }
        
        server.sendMessage(response);
    }
}

int main() {
    WebSocketServer server;
    
    char hostName[256];
    gethostname(hostName, sizeof(hostName));
    struct hostent* host = gethostbyname(hostName);
    
    std::cout << "==================================================" << std::endl;
    std::cout << "   Windows Process Manager WebSocket Server" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Server Name: " << hostName << std::endl;
    
    if (host != NULL) {
        for (int i = 0; host->h_addr_list[i] != NULL; i++) {
            struct in_addr addr;
            memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
            std::cout << "IP Address " << (i+1) << ": " << inet_ntoa(addr) << std::endl;
        }
    }
    
    std::cout << "Port: 8080" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    if (!server.start(8080)) {
        std::cerr << "Failed to start server on port 8080" << std::endl;
        std::cerr << "Make sure:" << std::endl;
        std::cerr << "  1. Port 8080 is not in use" << std::endl;
        std::cerr << "  2. Firewall allows connections on port 8080" << std::endl;
        std::cerr << "  3. Run as Administrator if needed" << std::endl;
        return 1;
    }
    
    std::cout << "Server listening on 0.0.0.0:8080" << std::endl;
    std::cout << "Clients can connect from any device on the LAN" << std::endl;
    std::cout << "Waiting for connections..." << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    
    while (true) {
        if (server.acceptClient()) {
            handleClient(server);
        } else {
            std::cerr << "Failed to accept client, waiting..." << std::endl;
            Sleep(1000);
        }
    }
    
    server.close();
    return 0;
}