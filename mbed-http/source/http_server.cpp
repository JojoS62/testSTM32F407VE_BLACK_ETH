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


/**
 * HttpRequest Constructor
 *
 * @param[in] network The network interface
*/
HttpServer::HttpServer(NetworkInterface* network, int nWorkerThreads, int nWebSocketsMax)  :
    _threadHTTPServer(osPriorityNormal, 2*1024, nullptr, "HTTPServerThread") { 
    _network = network;
    _nWebSockets = 0;
    _nWebSocketsMax = nWebSocketsMax;
    _nWorkerThreads = nWorkerThreads;
}

HttpServer::~HttpServer() {
}

/**
 * Start running the server (it will run on it's own thread)
 */
nsapi_error_t HttpServer::start(uint16_t port, Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> a_handler) {
    _handler = a_handler;

    // create client connections
    // needs RAM for buffers!
    _clientConnections.reserve(_nWorkerThreads);
    for(int i=0; i < _nWorkerThreads; i++) {
        ClientConnection *clientCon = new ClientConnection(this, _handler);
        MBED_ASSERT(clientCon);
        _clientConnections.push_back(clientCon);
    }

    // create server socket and start to listen
    _serverSocket = new TCPSocket();
    MBED_ASSERT(_serverSocket);
    nsapi_error_t ret;

    ret = _serverSocket->open(_network);
    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }

    ret = _serverSocket->bind(port);
    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }

    _serverSocket->listen(HTTP_SERVER_MAX_CONCURRENT); // max. concurrent connections...

    _threadHTTPServer.start(callback(this, &HttpServer::main));

    return NSAPI_ERROR_OK;
}


void HttpServer::main() {
    while (1) {
        nsapi_error_t accept_res = -1;
        TCPSocket* clt_sock = _serverSocket->accept(&accept_res);
        if (accept_res == NSAPI_ERROR_OK) {
            // find idle client connection
            vector<ClientConnection*>::iterator it = _clientConnections.begin();
            bool isIdle = (*it)->isIdle();
            while (!isIdle && (it < _clientConnections.end()))
            {
                it++;
                isIdle = (*it)->isIdle();
            }
            
            if (isIdle) {
                (*it)->start(clt_sock);
            } else
            {
                clt_sock->close();               // no idel connections, close. Todo: wait with timeout
            }
            
        }
    }
}

void HttpServer::setWSHandler(const char* path, CreateHandlerFn handler)
{
	_WSHandlers[path] = handler;
}

CreateHandlerFn HttpServer::getWSHandler(const char* path)
{
	WebSocketHandlerContainer::iterator it;

	it = _WSHandlers.find(path);
	if (it != _WSHandlers.end()) {
		return it->second;
	}
	return NULL;
}
