#ifndef _WEB_SOCKET_CONNECTION_H_
#define _WEB_SOCKET_CONNECTION_H_

#include "mbed.h"
#include "WebSocketHandler.h"
#include <string>
#include <map>

#ifndef NODEBUG_WEBSOCKETS
#define DEBUG_WEBSOCKETS(...) printf(__VA_ARGS__)
#endif

// max size of the WS Message Header
#define WEBSOCKETS_MAX_HEADER_SIZE (14)

typedef enum {
    WSC_NOT_CONNECTED,
    WSC_HEADER,
    WSC_CONNECTED
} WSclientsStatus_t;

typedef enum {
    WStype_ERROR,
    WStype_DISCONNECTED,
    WStype_CONNECTED,
    WStype_TEXT,
    WStype_BIN,
    WStype_FRAGMENT_TEXT_START,
    WStype_FRAGMENT_BIN_START,
    WStype_FRAGMENT,
    WStype_FRAGMENT_FIN,
    WStype_PING,
    WStype_PONG,
} WStype_t;

typedef enum {
    WSop_continuation = 0x00,    ///< %x0 denotes a continuation frame
    WSop_text         = 0x01,    ///< %x1 denotes a text frame
    WSop_binary       = 0x02,    ///< %x2 denotes a binary frame
                                 ///< %x3-7 are reserved for further non-control frames
    WSop_close = 0x08,           ///< %x8 denotes a connection close
    WSop_ping  = 0x09,           ///< %x9 denotes a ping
    WSop_pong  = 0x0A            ///< %xA denotes a pong
                                 ///< %xB-F are reserved for further control frames
} WSopcode_t;

typedef struct {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;

    WSopcode_t opCode;
    bool mask;

    size_t payloadLen;

    uint8_t * maskKey;
} WSMessageHeader_t;



class WebSocketServer;

class WebSocketConnection
{
public:
    WebSocketConnection(WebSocketServer* server);
    virtual ~WebSocketConnection();

    void run();
    //TCPSocket& getTCPSocketConnection() { return mConnection; }
    void setTCPSocketConnection(TCPSocket *sock) { mConnection = sock; }

    uint8_t createHeader(uint8_t * buf, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin);
    bool sendFrameHeader(WSopcode_t opcode, size_t length = 0, bool fin = true);
    bool sendFrame(WSopcode_t opcode, uint8_t * payload = NULL, size_t length = 0, bool fin = true, bool headerToPayload = false);


private:
    bool handleHTTP(char* buf, int size, int sizeMax);
    bool handleWebSocket(char* buf, int size);
    bool sendUpgradeResponse(char* key);


    WebSocketServer* mServer;
    TCPSocket *mConnection;
    WebSocketHandler* mHandler;
    bool mPrevFin;
    bool _cIsClient;
};

#endif
