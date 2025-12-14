#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>

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
        // Simple SHA1 - for production use a proper crypto library
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

        std::cout << "WebSocket server listening on port " << port << std::endl;
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

        // Parse WebSocket frame
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
        frame.push_back(0x81); // FIN + text frame

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
                   << ",\"name\":\"" << pe32.szExeFile << "\"}";
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

        // List common application directories
        std::vector<std::string> searchPaths = {
            "C:\\Program Files",
            "C:\\Program Files (x86)"
        };

        bool first = true;
        for (const auto& path : searchPaths) {
            WIN32_FIND_DATAA findData;
            HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
                        strcmp(findData.cFileName, ".") != 0 &&
                        strcmp(findData.cFileName, "..") != 0) {

                        if (!first) ss << ",";
                        first = false;
                        ss << "{\"name\":\"" << findData.cFileName
                           << "\",\"path\":\"" << path << "\\\\" << findData.cFileName << "\"}";
                    }
                } while (FindNextFileA(hFind, &findData));
                FindClose(hFind);
            }
        }

        ss << "]";
        return ss.str();
    }
};

// int main() {
//     WebSocketServer server;

//     if (!server.start(8080)) {
//         std::cerr << "Failed to start server" << std::endl;
//         return 1;
//     }

//     while (true) {
//         std::cout << "Waiting for client..." << std::endl;
//         if (!server.acceptClient()) continue;

//         std::cout << "Client connected!" << std::endl;

//         while (true) {
//             std::string message = server.receiveMessage();
//             if (message.empty()) break;

//             std::cout << "Received: " << message << std::endl;

//             std::string response;
//             if (message.find("list_processes") != std::string::npos) {
//                 response = ProcessManager::listProcesses();
//             }
//             else if (message.find("list_applications") != std::string::npos) {
//                 response = ProcessManager::listApplications();
//             }
//             else if (message.find("start_process:") != std::string::npos) {
//                 std::string path = message.substr(14);
//                 response = ProcessManager::startProcess(path);
//             }
//             else if (message.find("stop_process:") != std::string::npos) {
//                 DWORD pid = std::stoul(message.substr(13));
//                 response = ProcessManager::stopProcess(pid);
//             }
//             else {
//                 response = "{\"status\":\"error\",\"message\":\"Unknown command\"}";
//             }

//             server.sendMessage(response);
//         }

//         std::cout << "Client disconnected" << std::endl;
//     }

//     server.close();
//     return 0;
// }
