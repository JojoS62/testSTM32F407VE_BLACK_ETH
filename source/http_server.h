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

#ifndef HTTP_SERVER_MAX_CONCURRENT
#define HTTP_SERVER_MAX_CONCURRENT      5
#endif

#if HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_SOCKET_MAX
#warning "HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_SOCKET_MAX, HTTPServer needs more sockets for this setting, increase socket count in mbed_app.json"
#endif
#if HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_TCP_SOCKET_MAX
#warning "HTTP_SERVER_MAX_CONCURRENT > MBED_CONF_LWIP_TCP_SOCKET_MAX, HTTPServer needs more TCP sockets for this setting, increase socket count in mbed_app.json"
#endif

typedef HttpResponse ParsedHttpRequest;


class ClientConnection {
public:
    ClientConnection(TCPSocket* socket, Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> handler);

    void start();

private:
    void receive_data();

    TCPSocket* _socket;
    Thread  _threadClientConnection;
    ParsedHttpRequest _response;
    HttpParser _parser;
    uint8_t _recv_buffer[HTTP_RECEIVE_BUFFER_SIZE];
    Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> _handler;
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

private:
    void main();

    typedef struct {
        ClientConnection* clientCon;
        Thread*    thread;
    } socket_thread_metadata_t;

    TCPSocket* server;
    NetworkInterface* _network;
    Thread _threadHTTPServer;
    //vector<TCPSocket*> sockets;
    vector<socket_thread_metadata_t*> socket_threads;
    Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> handler;
};

#endif // __HTTP_SERVER_h__
