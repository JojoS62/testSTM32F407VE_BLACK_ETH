#ifndef _WEB_SOCKET_HANDLER_H_
#define _WEB_SOCKET_HANDLER_H_

class WebSocketConnection;

class WebSocketHandler
{
public:
    virtual void onOpen(WebSocketConnection *webSocketConnection) { _webSocketConnection = webSocketConnection; };
    virtual void onClose() {};
    // to receive text message
    virtual void onMessage(char* text) {};
    // to receive binary message
    virtual void onMessage(char* data, size_t size) {};
    virtual void onError() {};
    virtual void setOrigin(char* origin) { /* use strcpy to copy originstring if needed */ };

protected:
    WebSocketConnection *_webSocketConnection;
};

#endif
