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
#include "sha1_ws.h"

#define MAGIC_NUMBER		"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_ORIGIN           "Origin:"
#define OP_CONT		0x0
#define OP_TEXT		0x1
#define OP_BINARY	0x2
#define OP_CLOSE	0x8
#define OP_PING		0x9
#define OP_PONG		0xA



ClientConnection::ClientConnection(HttpServer* server, TCPSocket* socket, Callback<void(ParsedHttpRequest* request, TCPSocket* socket)> handler) :
    _threadClientConnection(osPriorityNormal, 2*1024, nullptr, "HTTPClientThread"),
    _parser(&_response, HTTP_REQUEST) 
{ 
    _isWebSocket = false;
    _server = server;
    _socket = socket; 
    _handler = handler; 
    _cIsClient = false;
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
    bool socketIsOpen = true;
    while(socketIsOpen) {
        nsapi_size_or_error_t recv_ret;
        while ((recv_ret = _socket->recv(_recv_buffer, HTTP_RECEIVE_BUFFER_SIZE)) > 0) {
            // Websocket must not be parsed
            if (_isWebSocket) {
                break;
            }

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
        
        if (recv_ret > 0) {
            if (!_isWebSocket) {
                if (_response.get_Upgrade()) {
                    //HttpResponseBuilder builder(101);
                    vector<string*>  headerFields = _response.get_headers_fields();
                    vector<string*>  headerValues = _response.get_headers_values();
                    
                    bool upgradeWebsocketfound = false;
                    uint i = 0;
                    while(!upgradeWebsocketfound && (i < headerFields.size())) {
                        if (*(headerFields[i]) == "Upgrade" && *(headerValues[i]) == "websocket") {
                            upgradeWebsocketfound = true;
                        } else {
                            i++;
                        }

                    }

                    i = 0;
                    bool secWebsocketKeyFound = false;
                    const char* secWebsocketKey;
                    while(!secWebsocketKeyFound && (i < headerFields.size())) {
                        if (*(headerFields[i]) == "Sec-WebSocket-Key") {
                            secWebsocketKey = headerValues[i]->c_str();
                            secWebsocketKeyFound = true;
                        } else {
                            i++;
                        }
                    }

                    _webSocketHandler = _server->getWSHandler(_response.get_url().c_str());

                    if (upgradeWebsocketfound && secWebsocketKeyFound && _webSocketHandler) {
                        _isWebSocket = sendUpgradeResponse(secWebsocketKey);
                        if (_webSocketHandler) {
                            //mHandler->setOrigin(origin);
                            _webSocketHandler->onOpen(this);
                        }
                    }
                } else {
                    _parser.finish();
                    _handler(&_response, _socket);
                }
            } else {
                _isWebSocket = handleWebSocket(recv_ret);
            }
        }

        if (!_isWebSocket) {
            // close socket. Because allocated by accept(), it will be deleted by itself
            _socket->close();
            socketIsOpen = false;
        }
    }
}

bool ClientConnection::handleWebSocket(int size)
{
	uint8_t* ptr = _recv_buffer;

	bool fin = (*ptr & 0x80) == 0x80;
	uint8_t opcode = *ptr & 0xF;

	if (opcode == OP_PING) {
		*ptr = ((*ptr & 0xF0) | OP_PONG);
		_socket->send(_recv_buffer, size);
		return true;
	}
	if (opcode == OP_CLOSE) {
		if (_webSocketHandler) {
			_webSocketHandler->onClose();
		}
		return false;
	}
	ptr++;

	if (!fin || !_mPrevFin) {	
		printf("WARN: Data consists of multiple frame not supported\r\n");
		_mPrevFin = fin;
		return true; // not an error, just discard it
	}
	_mPrevFin = fin;

	bool mask = (*ptr & 0x80) == 0x80;
	uint8_t len = *ptr & 0x7F;
	ptr++;
	
	if (len > 125) {
		printf("WARN: Extended payload length not supported\r\n");
		return true; // not an error, just discard it
	}

	char* data;
	if (mask) {
		char* maskingKey = (char*)ptr;
		data = (char*)(ptr + 4);
		for (int i = 0; i < len; i++) {
        	data[i] = data[i] ^ maskingKey[(i % 4)];
        }
	} else {
		data = (char*)ptr;
	}
	if (_webSocketHandler) {
		if (opcode == OP_TEXT) {
			data[len] = '\0';
			_webSocketHandler->onMessage(data);
		} else if (opcode == OP_BINARY) {
			_webSocketHandler->onMessage(data, len);
		}
	}
	return true;
}

char* ClientConnection::base64Encode(const uint8_t* data, size_t size,
                   char* outputBuffer, size_t outputBufferSize)
{
	static char encodingTable[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	                               'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	                               'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	                               'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	                               'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	                               'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	                               'w', 'x', 'y', 'z', '0', '1', '2', '3',
	                               '4', '5', '6', '7', '8', '9', '+', '/'};
    size_t outputLength = 4 * ((size + 2) / 3);
    if (outputBufferSize - 1 < outputLength) { // -1 for NUL
    	return NULL;
    }

    for (size_t i = 0, j = 0; i < size; /* nothing */) {
        uint32_t octet1 = i < size ? (unsigned char)data[i++] : 0;
        uint32_t octet2 = i < size ? (unsigned char)data[i++] : 0;
        uint32_t octet3 = i < size ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet1 << 0x10) + (octet2 << 0x08) + octet3;

        outputBuffer[j++] = encodingTable[(triple >> 3 * 6) & 0x3F];
        outputBuffer[j++] = encodingTable[(triple >> 2 * 6) & 0x3F];
        outputBuffer[j++] = encodingTable[(triple >> 1 * 6) & 0x3F];
        outputBuffer[j++] = encodingTable[(triple >> 0 * 6) & 0x3F];
    }

	static int padTable[] = { 0, 2, 1 };
	int paddingCount = padTable[size % 3];

    for (int i = 0; i < paddingCount; i++) {
        outputBuffer[outputLength - 1 - i] = '=';
    }
    outputBuffer[outputLength] = '\0'; // NUL

    return outputBuffer;
}

bool ClientConnection::sendUpgradeResponse(const char* key)
{
	char buf[128];

	if (strlen(key) + sizeof(MAGIC_NUMBER) > sizeof(buf)) {
		return false;
	}
	strcpy(buf, key);
	strcat(buf, MAGIC_NUMBER);

    uint8_t hash[20];
	SHA1Context sha;
    SHA1Reset(&sha);
    SHA1Input(&sha, (unsigned char*)buf, strlen(buf));
    SHA1Result(&sha, (uint8_t*)hash);

	char encoded[30];
    base64Encode(hash, 20, encoded, sizeof(encoded));

    char resp[] = "HTTP/1.1 101 Switching Protocols\r\n" \
	    "Upgrade: websocket\r\n" \
    	"Connection: Upgrade\r\n" \
    	"Sec-WebSocket-Accept: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n\r\n";
    char* ptr = strstr(resp, "XXXXX");
    strcpy(ptr, encoded);
    strcpy(ptr+strlen(encoded), "\r\n\r\n");

    //printf(resp);

    int ret = _socket->send(resp, strlen(resp));
    if (ret < 0) {
    	printf("ERROR: Failed to send response\r\n");
    	return false;
    }

    return true;
}


/*
 * @param buf uint8_t *         ptr to the buffer for writing
 * @param opcode WSopcode_t
 * @param length size_t         length of the payload
 * @param mask bool             add dummy mask to the frame (needed for web browser)
 * @param maskkey uint8_t[4]    key used for payload
 * @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
 */
uint8_t ClientConnection::createHeader(uint8_t * headerPtr, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin) {
    uint8_t headerSize;
    // calculate header Size
    if(length < 126) {
        headerSize = 2;
    } else if(length < 0xFFFF) {
        headerSize = 4;
    } else {
        headerSize = 10;
    }

    if(mask) {
        headerSize += 4;
    }

    // create header

    // byte 0
    *headerPtr = 0x00;
    if(fin) {
        *headerPtr |= (1 << 7);    ///< set Fin
    }
    *headerPtr |= opcode;    ///< set opcode
    headerPtr++;

    // byte 1
    *headerPtr = 0x00;
    if(mask) {
        *headerPtr |= (1 << 7);    ///< set mask
    }

    if(length < 126) {
        *headerPtr |= length;
        headerPtr++;
    } else if(length < 0xFFFF) {
        *headerPtr |= 126;
        headerPtr++;
        *headerPtr = ((length >> 8) & 0xFF);
        headerPtr++;
        *headerPtr = (length & 0xFF);
        headerPtr++;
    } else {
        // Normally we never get here (to less memory)
        *headerPtr |= 127;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = 0x00;
        headerPtr++;
        *headerPtr = ((length >> 24) & 0xFF);
        headerPtr++;
        *headerPtr = ((length >> 16) & 0xFF);
        headerPtr++;
        *headerPtr = ((length >> 8) & 0xFF);
        headerPtr++;
        *headerPtr = (length & 0xFF);
        headerPtr++;
    }

    if(mask) {
        *headerPtr = maskKey[0];
        headerPtr++;
        *headerPtr = maskKey[1];
        headerPtr++;
        *headerPtr = maskKey[2];
        headerPtr++;
        *headerPtr = maskKey[3];
        headerPtr++;
    }
    return headerSize;
}

/**
 *
 * @param client WSclient_t *   ptr to the client struct
 * @param opcode WSopcode_t
 * @param length size_t         length of the payload
 * @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
 * @return true if ok
 */
bool ClientConnection::sendFrameHeader(WSopcode_t opcode, size_t length, bool fin) {
    uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
    uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

    uint8_t headerSize = createHeader(&buffer[0], opcode, length, _cIsClient, maskKey, fin);

    if(_socket->send(&buffer[0], headerSize) != headerSize) {
        return false;
    }

    return true;
}

/**
 *
 * @param client WSclient_t *   ptr to the client struct
 * @param opcode WSopcode_t
 * @param payload uint8_t *     ptr to the payload
 * @param length size_t         length of the payload
 * @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
 * @param headerToPayload bool  set true if the payload has reserved 14 Byte at the beginning to dynamically add the Header (payload neet to be in RAM!)
 * @return true if ok
 */
bool ClientConnection::sendFrame( WSopcode_t opcode, uint8_t * payload, size_t length, bool fin, bool headerToPayload) {
    if (0) {  // Todo: isConnected()    (client->tcp && !client->tcp->connected()) {
        DEBUG_WEBSOCKETS("[WS][sendFrame] not Connected!?\n");
        return false;
    }

    if (0) {  // Todo: isWCSconnected  (client->status != WSC_CONNECTED) {
        DEBUG_WEBSOCKETS("[WS][sendFrame] not in WSC_CONNECTED state!?\n");
        return false;
    }

    DEBUG_WEBSOCKETS("[WS][sendFrame] ------- send message frame -------\n");
    DEBUG_WEBSOCKETS("[WS][sendFrame] fin: %u opCode: %u mask: %u length: %u headerToPayload: %u\n", fin, opcode, _cIsClient, length, headerToPayload);

    if(opcode == WSop_text) {
        DEBUG_WEBSOCKETS("[WS][sendFrame] text: %s\n", (payload + (headerToPayload ? 14 : 0)));
    }

    uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
    uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

    uint8_t headerSize;
    uint8_t * headerPtr;
    uint8_t * payloadPtr = payload;
    bool useInternBuffer = false;
    bool ret             = true;

    // calculate header Size
    if(length < 126) {
        headerSize = 2;
    } else if(length < 0xFFFF) {
        headerSize = 4;
    } else {
        headerSize = 10;
    }

    if(_cIsClient) {
        headerSize += 4;
    }

//#ifdef WEBSOCKETS_USE_BIG_MEM
    // only for ESP since AVR has less HEAP
    // try to send data in one TCP package (only if some free Heap is there)
    if(!headerToPayload && ((length > 0) && (length < 1400)) ) { // Todo: && (GET_FREE_HEAP > 6000)) {
        DEBUG_WEBSOCKETS("[WS][sendFrame] pack to one TCP package...\n");
        uint8_t * dataPtr = (uint8_t *)malloc(length + WEBSOCKETS_MAX_HEADER_SIZE);
        if(dataPtr) {
            memcpy((dataPtr + WEBSOCKETS_MAX_HEADER_SIZE), payload, length);
            headerToPayload = true;
            useInternBuffer = true;
            payloadPtr      = dataPtr;
        }
    }
//#endif

    // set Header Pointer
    if(headerToPayload) {
        // calculate offset in payload
        headerPtr = (payloadPtr + (WEBSOCKETS_MAX_HEADER_SIZE - headerSize));
    } else {
        headerPtr = &buffer[0];
    }

    if(_cIsClient && useInternBuffer) {
        // if we use a Intern Buffer we can modify the data
        // by this fact its possible the do the masking
        for(uint8_t x = 0; x < sizeof(maskKey); x++) {
            maskKey[x] = random() & 0xff;
        }
    }

    createHeader(headerPtr, opcode, length, _cIsClient, maskKey, fin);

    if(useInternBuffer) {
        uint8_t * dataMaskPtr;

        if(headerToPayload) {
            dataMaskPtr = (payloadPtr + WEBSOCKETS_MAX_HEADER_SIZE);
        } else {
            dataMaskPtr = payloadPtr;
        }

        for(size_t x = 0; x < length; x++) {
            dataMaskPtr[x] = (dataMaskPtr[x] ^ maskKey[x % 4]);
        }
    }

    if(headerToPayload) {
        // header has be added to payload
        // payload is forced to reserved 14 Byte but we may not need all based on the length and mask settings
        // offset in payload is calculatetd 14 - headerSize
        if(_socket->send(&payloadPtr[(WEBSOCKETS_MAX_HEADER_SIZE - headerSize)], (length + headerSize)) != (length + headerSize)) {
            ret = false;
        }
    } else {
        // send header
        if(_socket->send(&buffer[0], headerSize) != headerSize) {
            ret = false;
        }

        if(payloadPtr && length > 0) {
            // send payload
            if(_socket->send(&payloadPtr[0], length) != length) {
                ret = false;
            }
        }
    }

    //DEBUG_WEBSOCKETS("[WS][sendFrame] sending Frame Done (%luus).\n", (micros() - start));

//#ifdef WEBSOCKETS_USE_BIG_MEM
    if(useInternBuffer && payloadPtr) {
        free(payloadPtr);
    }
//#endif

    return ret;
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
            ClientConnection *clientCon = new ClientConnection(this, clt_sock, handler);
            clientCon->start();
        }
    }
}

void HttpServer::setWSHandler(const char* path, WebSocketHandler* handler)
{
	_WSHandlers[path] = handler;
}

WebSocketHandler* HttpServer::getWSHandler(const char* path)
{
	WebSocketHandlerContainer::iterator it;

	it = _WSHandlers.find(path);
	if (it != _WSHandlers.end()) {
		return it->second;
	}
	return NULL;
}
