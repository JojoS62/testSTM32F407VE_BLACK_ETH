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

#include "http_server.h"


ClientConnection::ClientConnection(TCPSocket* socket, Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> handler) :
    _threadClientConnection(osPriorityNormal, 2*1024, nullptr, "HTTPClientThread"),
    _parser(&_response, HTTP_REQUEST) 
{ 
    _socket = socket; 
    _handler = handler; 
};

ClientConnection::~ClientConnection() {
    _handler = nullptr;
};

void ClientConnection::start() {
    _threadClientConnection.start(callback(this, &ClientConnection::receive_data));

    _threadClientConnection.join();     // wait for thread to terminate

    delete this;    // kill myself
}

void ClientConnection::receive_data() {
    // TCPSocket::recv is called until we don't have any data anymore
    nsapi_size_or_error_t recv_ret;
    while ((recv_ret = _socket->recv(_recv_buffer, HTTP_RECEIVE_BUFFER_SIZE)) > 0) {
        // Pass the chunk into the http_parser
        int nparsed = _parser.execute((const char*)_recv_buffer, recv_ret);
        if (nparsed != recv_ret) {
            printf("Parsing failed... parsed %d bytes, received %d bytes\n", nparsed, recv_ret);
            recv_ret = -2101;
            break;
        }

        if (_response.is_message_complete()) {
            break;
        }
    }

    // When done, call parser.finish()
    _parser.finish();

    if (recv_ret > 0) {
        // Let user application handle the request, if user needs a handle to response they need to memcpy themselves
        if (recv_ret > 0) {
            _handler(&_response, _socket);
        }
    }

    // close socket. Because allocated by accept(), it will be deleted by itself
    _socket->close();
}


/**
 * HttpRequest Constructor
 *
 * @param[in] network The network interface
*/
HttpServer::HttpServer(NetworkInterface* network)  :
    _threadHTTPServer(osPriorityNormal, 2*1024, nullptr, "HTTPServerThread") { 
    _network = network;
}

HttpServer::~HttpServer() {
}

/**
 * Start running the server (it will run on it's own thread)
 */
nsapi_error_t HttpServer::start(uint16_t port, Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> a_handler) {
    server = new TCPSocket();

    nsapi_error_t ret;

    ret = server->open(_network);
    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }

    ret = server->bind(port);
    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }

    server->listen(HTTP_SERVER_MAX_CONCURRENT); // max. concurrent connections...

    handler = a_handler;

    _threadHTTPServer.start(callback(this, &HttpServer::main));

    return NSAPI_ERROR_OK;
}


void HttpServer::main() {
    while (1) {
        nsapi_error_t accept_res = -1;
        TCPSocket* clt_sock = server->accept(&accept_res);
        if (accept_res == NSAPI_ERROR_OK) {
            // and start listening for events there
            ClientConnection *clientCon = new ClientConnection(clt_sock, handler);
            clientCon->start();
        }
    }
}

