#include <ws2tcpip.h>
#include <psapi.h>
#include <shlobj.h>
#include <iostream>
#include <cctype>
#include <filesystem>

#pragma comment(lib, "ws2_32.lib")

#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")


#include "../include/server/WebSocketServer.hpp"
#include "../include/server/HTTPServer.hpp"
#include "../include/modules/ProcessManager.hpp"
#include "../include/modules/CameraController.hpp"
#include "../include/modules/CaptureScreen.hpp"
#include "../include/modules/ListApps.hpp"
#include "../include/modules/ListRunningApps.hpp"
#include "../include/modules/ScreenRecorder.hpp"


// namespace fs = std::filesystem;







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
        else if (message.find("capture_screen") != std::string::npos) {
            CaptureScreen::run();
            response = "{\"status\":\"success\",\"message\":\"Screenshot saved as screen.bmp\"}";
        }
        else if (message.find("record_video:") != std::string::npos) {
            int seconds = std::stoi(message.substr(13));
            std::thread([seconds]() {
                ScreenRecorder::record(seconds);
            }).detach();
            response = "{\"status\":\"success\",\"message\":\"Recording started\"}";
        }
        else if (message.find("list_running_windows") != std::string::npos) {
            WindowCollector collector;
            collector.CollectAllWindows();
            response = collector.GetFormattedString();
        }
        else if (message.find("list_start_menu_apps") != std::string::npos) {
            ListStartMenuApps appLister;
            response = appLister.GetAllApps();
        }
        else {
            response = "{\"status\":\"error\",\"message\":\"Unknown command\"}";
        }
        
        server.sendMessage(response);
    }
}

void displayAllLocalIPs() {
    std::cout << "\n📡 AVAILABLE IP ADDRESSES:" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    // Allocate buffer for adapter info
    ULONG bufferSize = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
    
    if (pAddresses == NULL) {
        std::cout << "  ❌ Memory allocation failed" << std::endl;
        return;
    }
    
    // Get adapter addresses
    ULONG result = GetAdaptersAddresses(
        AF_INET,                    // IPv4 only
        GAA_FLAG_SKIP_ANYCAST |     // Skip anycast addresses
        GAA_FLAG_SKIP_MULTICAST |   // Skip multicast addresses
        GAA_FLAG_SKIP_DNS_SERVER,   // Skip DNS server addresses
        NULL,                       // Reserved
        pAddresses,                 // Output buffer
        &bufferSize                 // Size of output buffer
    );
    
    if (result == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, 
                                      NULL, pAddresses, &bufferSize);
    }
    
    if (result == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        bool foundIP = false;
        
        while (pCurrAddresses) {
            // Check if adapter is up and not a loopback or tunnel
            if (pCurrAddresses->OperStatus == IfOperStatusUp) {
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                
                while (pUnicast) {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                        sockaddr_in* pAddr = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                        char ipStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(pAddr->sin_addr), ipStr, INET_ADDRSTRLEN);
                        
                        std::string ip(ipStr);
                        
                        // Filter out loopback and link-local addresses
                        if (ip != "127.0.0.1" && ip.substr(0, 7) != "169.254") {
                            std::cout << " IP found " << ip << "  <-- Use this IP on client device" << std::endl;
                            std::cout << "     Adapter: " << pCurrAddresses->Description << std::endl;
                            foundIP = true;
                        } else if (ip == "127.0.0.1") {
                            std::cout << "  🏠 " << ip << "  (localhost - same machine only)" << std::endl;
                        }
                    }
                    pUnicast = pUnicast->Next;
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
        
        if (!foundIP) {
            std::cout << "  ⚠️  No network IP found. Make sure you're connected to WiFi/Ethernet" << std::endl;
            std::cout << "  💡 Try running 'ipconfig' in Command Prompt to see your IP" << std::endl;
        }
    } else {
        std::cout << "  ❌ Failed to get network adapters (Error: " << result << ")" << std::endl;
        std::cout << "  💡 Try running 'ipconfig' in Command Prompt to see your IP" << std::endl;
    }
    
    free(pAddresses);
    std::cout << "==================================================" << std::endl;
}

int main() {
    WebSocketServer wsServer;
    HTTPServer httpServer;
    
    // Get computer name
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    
    std::cout << "\n" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "   Windows Process Manager - WebSocket Server" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    if (GetComputerNameA(computerName, &size)) {
        std::cout << "Computer Name: " << computerName << std::endl;
    }
    
    // Display all IPs prominently
    displayAllLocalIPs();
    
    std::cout << "\nWebSocket Port: 8080" << std::endl;
    std::cout << "HTTP Port: 8000" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    // Start HTTP Server
    if (!httpServer.start(8000)) {
        std::cerr << "\nFailed to start HTTP server on port 8000" << std::endl;
        return 1;
    }
    std::cout << "\nHTTP server listening on 0.0.0.0:8000" << std::endl;
    
    // Start WebSocket Server
    if (!wsServer.start(8080)) {
        std::cerr << "Failed to start WebSocket server on port 8080" << std::endl;
        return 1;
    }
    
    std::cout << "WebSocket server listening on 0.0.0.0:8080" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "CONNECTION INSTRUCTIONS:" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "1. On THIS device:" << std::endl;
    std::cout << "   Open browser: http://localhost:8000" << std::endl;
    std::cout << "\n2. On OTHER devices (same WiFi):" << std::endl;
    std::cout << "   Open browser: http://<IP-ADDRESS>:8000" << std::endl;
    std::cout << "   (Use one of the IPs shown above)" << std::endl;
    std::cout << "\n3. Make sure Windows Firewall allows ports 8000 & 8080" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "\nWaiting for connections...\n" << std::endl;
    
    // Start HTTP server thread
    std::thread httpThread(&HTTPServer::handleConnections, &httpServer);
    httpThread.detach();
    
    // Handle WebSocket connections
    while (true) {
        if (wsServer.acceptClient()) {
            handleClient(wsServer);
        } else {
            Sleep(1000);
        }
    }
    
    wsServer.close();
    httpServer.stop();
    return 0;
}