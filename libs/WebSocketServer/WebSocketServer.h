#ifndef _WEB_SOCKET_SERVER_H_
#define _WEB_SOCKET_SERVER_H_

#include "mbed.h"
#include "WebSocketHandler.h"
#include <string>
#include <map>

class WebSocketServer
{
public:
    WebSocketServer();
    virtual ~WebSocketServer();

    bool init(NetworkInterface *net, int port);
    void run();
    void setHandler(const char* path, WebSocketHandler* handler);
    WebSocketHandler* getHandler(const char* path);

private:
    typedef std::map<std::string, WebSocketHandler*> WebSocketHandlerContainer;

    TCPSocket mTCPSocketServer;
    WebSocketHandlerContainer mHandlers;
};

#endif
