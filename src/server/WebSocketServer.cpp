#include "../../include/server/WebSocketServer.hpp"

std::string WebSocketServer::base64_encode(const unsigned char* data, size_t len) {
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

std::string WebSocketServer::sha1(const std::string& input) {
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

bool WebSocketServer::performHandshake() {
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
    
    // Enhanced response with CORS headers
    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: *\r\n"
        "\r\n";
    
    send(clientSocket, response.c_str(), response.length(), 0);
    return true;
}

bool WebSocketServer::start(int port) {
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
    
    return true;
}

bool WebSocketServer::acceptClient() {
    clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) return false;
    
    return performHandshake();
}

std::string WebSocketServer::receiveMessage() {
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

void WebSocketServer::sendMessage(const std::string& message) {
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

void WebSocketServer::close() {
    if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
    if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
    WSACleanup();
}