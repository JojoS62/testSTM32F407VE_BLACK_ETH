/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HTTP_SERVER_h__
#define __HTTP_SERVER_h__

#include "mbed.h"
#include "http_request_parser.h"
#include "http_response.h"
#include "http_response_builder.h"
#include "WebSocketHandler.h"
#include <string>
#include <map>

#ifndef HTTP_SERVER_MAX_CONCURRENT
#define HTTP_SERVER_MAX_CONCURRENT      5
#endif

#if HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_SOCKET_MAX
#warning "HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_SOCKET_MAX, HTTPServer needs more sockets for this setting, increase socket count in mbed_app.json"
#endif
#if HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_TCP_SOCKET_MAX
#warning "HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_TCP_SOCKET_MAX, HTTPServer needs more TCP sockets for this setting, increase socket count in mbed_app.json"
#endif

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




typedef HttpResponse ParsedHttpRequest;
class HttpServer;

class ClientConnection {
public:
    ClientConnection(HttpServer* server, TCPSocket* socket, Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> handler);
    ~ClientConnection();

    void start();

    // Websocket functions
    uint8_t createHeader(uint8_t * buf, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin);
    bool sendFrameHeader(WSopcode_t opcode, size_t length = 0, bool fin = true);
    bool sendFrame(WSopcode_t opcode, uint8_t * payload = NULL, size_t length = 0, bool fin = true, bool headerToPayload = false);

private:
    void receive_data();
    bool handleWebSocket(int size);
    char* base64Encode(const uint8_t* data, size_t size, char* outputBuffer, size_t outputBufferSize);
    bool sendUpgradeResponse(const char* key);
    HttpServer* _server;
    TCPSocket* _socket;
    Thread  _threadClientConnection;
    ParsedHttpRequest _response;
    HttpParser _parser;
    bool _isWebSocket;
    bool _mPrevFin;
    bool _cIsClient;
    uint8_t _recv_buffer[HTTP_RECEIVE_BUFFER_SIZE];
    Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> _handler;
    WebSocketHandler* _webSocketHandler;
};

/**
 * \brief HttpServer implements the logic for setting up an HTTP server.
 */
class HttpServer {
public:
    /**
     * HttpRequest Constructor
     *
     * @param[in] network The network interface
    */
    HttpServer(NetworkInterface* network);

    ~HttpServer();

    /**
     * Start running the server (it will run on it's own thread)
     */
    nsapi_error_t start(uint16_t port, Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> a_handler);

    void setWSHandler(const char* path, WebSocketHandler* handler);
    WebSocketHandler* getWSHandler(const char* path);
    

private:
    void main();
    TCPSocket* server;
    NetworkInterface* _network;
    Thread _threadHTTPServer;
    Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> handler;

#if 0
    void setHTTPHandler(const char* path, WebSocketHandler* handler);
    WebSocketHandler* getHTTPHandler(const char* path);
    
    typedef std::map<std::string, WebSocketHandler*> WebSocketHandlerContainer;
    WebSocketHandlerContainer _HTTPHandlers;
#endif

    typedef std::map<std::string, WebSocketHandler*> WebSocketHandlerContainer;
    WebSocketHandlerContainer _WSHandlers;
};

#endif // __HTTP_SERVER_h__
