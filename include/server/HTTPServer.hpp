#include <winsock2.h>
#include <string>
#include <thread>
#include <sstream>
#include <fstream>

// Simple HTTP Server for serving the client HTML
class HTTPServer {
private:
    SOCKET serverSocket;
    bool running;
    
public:
    HTTPServer() : serverSocket(INVALID_SOCKET), running(false) {}
    
    bool start(int port);    
    void handleConnections();    
    void handleClient(SOCKET clientSocket);    
    void stop();
};