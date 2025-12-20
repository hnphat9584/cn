#pragma once
#include <winsock2.h>
#include <vector>
#include <string>
#include <cstdint>

// Simple WebSocket implementation with CORS support
class WebSocketServer {
private:
    SOCKET serverSocket;
    SOCKET clientSocket;
    
    std::string base64_encode(const unsigned char* data, size_t len);    
    std::string sha1(const std::string& input);    
    bool performHandshake();
    
public:
    bool start(int port);    
    bool acceptClient();    
    std::string receiveMessage();    
    void sendMessage(const std::string& message);    
    void close();
};