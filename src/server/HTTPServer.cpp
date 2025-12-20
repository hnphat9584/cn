#include "../../include/server/HTTPServer.hpp"

bool HTTPServer::start(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) return false;
    
    // Allow address reuse
    int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return false;
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) return false;
    
    running = true;
    return true;
}



void HTTPServer::handleConnections() {
    while (running) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;
        
        // Handle in a separate thread
        std::thread(&HTTPServer::handleClient, this, clientSocket).detach();
    }
}

void HTTPServer::handleClient(SOCKET clientSocket) {
    char buffer[4096] = {0};
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        
        // Parse HTTP request
        std::string method, path;
        std::istringstream iss(request);
        iss >> method >> path;
        
        // Default to index.html if root is requested
        if (path == "/") path = "/index.html";
        
        // Remove leading slash for file path
        std::string filePath = "." + path;
        
        // Only serve HTML files
        if (path.find("..") == std::string::npos && 
            (path.find(".html") != std::string::npos || path.find(".htm") != std::string::npos)) {
            
            // Try to read the file
            std::ifstream file(filePath, std::ios::binary);
            if (file.is_open()) {
                // File found
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                file.close();
                
                std::string response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(content.length()) + "\r\n"
                    "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
                    "Pragma: no-cache\r\n"
                    "Expires: 0\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type\r\n"
                    "Connection: close\r\n"
                    "\r\n" + content;
                
                send(clientSocket, response.c_str(), response.length(), 0);
            } else {
                // File not found
                std::string response = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: 156\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "<html><head><title>404 Not Found</title></head>"
                    "<body><h1>404 Not Found</h1>"
                    "<p>The requested file was not found.</p>"
                    "<p>Make sure index.html is in the same directory as websocket_server.exe</p>"
                    "</body></html>";
                
                send(clientSocket, response.c_str(), response.length(), 0);
            }
        } else {
            // Invalid request
            std::string response = 
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 116\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<html><head><title>403 Forbidden</title></head>"
                "<body><h1>403 Forbidden</h1>"
                "<p>Only HTML files can be served.</p></body></html>";
            
            send(clientSocket, response.c_str(), response.length(), 0);
        }
    }
    
    closesocket(clientSocket);
}

void HTTPServer::stop() {
    running = false;
    if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
}