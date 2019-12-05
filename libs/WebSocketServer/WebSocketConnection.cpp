#include "WebSocketConnection.h"
#include "WebSocketServer.h"
#include "sha1_ws.h"

#define UPGRADE_WEBSOCKET	"Upgrade: websocket"
#define SEC_WEBSOCKET_KEY	"Sec-WebSocket-Key:"
#define MAGIC_NUMBER		"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_ORIGIN           "Origin:"
#define OP_CONT		0x0
#define OP_TEXT		0x1
#define OP_BINARY	0x2
#define OP_CLOSE	0x8
#define OP_PING		0x9
#define OP_PONG		0xA

WebSocketConnection::WebSocketConnection(WebSocketServer* server)
{
	mServer = server;
	_cIsClient = false;
}

WebSocketConnection::~WebSocketConnection()
{
}

void WebSocketConnection::run()
{
	char buf[1024];
	bool isWebSocket = false;

	mConnection->set_blocking(true);

	// while (mConnection->is_connected()) {
	while (1) {
		int ret = mConnection->recv(buf, sizeof(buf));
		if (ret == 0) {
			// printf("No data to receive\r\n");
			continue;
		}
		if (ret < 0) {
			printf("ERROR: Failed to receive %d\r\n", ret);
			break;
		}
		if (!isWebSocket) {
			if (this->handleHTTP(buf, ret, sizeof(buf))) {
				isWebSocket = true;
			} else {
				printf("ERROR: Non websocket\r\n");
				break;
			}
		} else {
			if (!this->handleWebSocket(buf, ret)) {
				break;
			}
		}
	}
	// printf("Closed\r\n");
	mConnection->close();
}

bool WebSocketConnection::handleHTTP(char* buf, int size, int sizeMax)
{
	char* line = &buf[0];
	char key[128];
	bool isUpgradeWebSocket = false;
	bool isSecWebSocketKeyFound = false;
    char *origin = nullptr;

    // check for first 'CR LF CR LF' as delimiter
    // read more data until delimiter is found
    char *ptrEnd;
    while((0 == strstr(buf, "\r\n\r\n")) && (size < sizeMax) ) {
        int sizeRemaining = sizeMax - size;
        int ret = mConnection->recv(&buf[size], sizeRemaining);
		if (ret == 0) {
			// printf("No data to receive\r\n");
			continue;
		}
		if (ret < 0) {
            return false;       // connection was reset
		}
        size += ret;            // add received data
    }

	for (int i = 0; i < size; i++) {
		if (buf[i] == '\r' && i+1 < size && buf[i+1] == '\n') {
			buf[i] = '\0';
			if (strlen(buf) <= 0) {
				break;
			}
			printf("[%s]\r\n", line);
			if (line == &buf[0]) {
				char* method = strtok(buf, " ");
				char* path = strtok(NULL, " ");
				char* version = strtok(NULL, " ");
				// printf("[%s] [%s] [%s]\r\n", method, path, version);
				mHandler = mServer->getHandler(path);
				if (!mHandler) {
					printf("ERROR: Handler not found for %s\r\n", path);
					return false;
				}
			} else if (strncasecmp(line, UPGRADE_WEBSOCKET, strlen(UPGRADE_WEBSOCKET)) == 0) {
				isUpgradeWebSocket = true;
			} else if (strncasecmp(line, SEC_WEBSOCKET_KEY, strlen(SEC_WEBSOCKET_KEY)) == 0) {
				isSecWebSocketKeyFound = true;
				char* ptr = line + strlen(SEC_WEBSOCKET_KEY);
				while (*ptr == ' ') ++ptr;
				strcpy(key, ptr);
			} else if (strncasecmp(line, WS_ORIGIN, strlen(WS_ORIGIN)) == 0) {
				char* ptr = line + strlen(WS_ORIGIN);
				while (*ptr == ' ') ++ptr;
				origin = ptr;
            }
			i += 2;
			line = &buf[i];
		}
	}

	if (isUpgradeWebSocket && isSecWebSocketKeyFound) {
		this->sendUpgradeResponse(key);
		if (mHandler) {
            mHandler->setOrigin(origin);
			mHandler->onOpen(this);
		}
		mPrevFin = true;
		return true;
	}

	return false;
}

bool WebSocketConnection::handleWebSocket(char* buf, int size)
{
	uint8_t* ptr = (uint8_t*)buf;

	bool fin = (*ptr & 0x80) == 0x80;
	uint8_t opcode = *ptr & 0xF;

	if (opcode == OP_PING) {
		*ptr = ((*ptr & 0xF0) | OP_PONG);
		mConnection->send(buf, size);
		return true;
	}
	if (opcode == OP_CLOSE) {
		if (mHandler) {
			mHandler->onClose();
		}
		return false;
	}
	ptr++;

	if (!fin || !mPrevFin) {	
		printf("WARN: Data consists of multiple frame not supported\r\n");
		mPrevFin = fin;
		return true; // not an error, just discard it
	}
	mPrevFin = fin;

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
	if (mHandler) {
		if (opcode == OP_TEXT) {
			data[len] = '\0';
			mHandler->onMessage(data);
		} else if (opcode == OP_BINARY) {
			mHandler->onMessage(data, len);
		}
	}
	return true;
}

char* base64Encode(const uint8_t* data, size_t size,
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

bool WebSocketConnection::sendUpgradeResponse(char* key)
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

    printf(resp);

    int ret = mConnection->send(resp, strlen(resp));
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
uint8_t WebSocketConnection::createHeader(uint8_t * headerPtr, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin) {
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
bool WebSocketConnection::sendFrameHeader(WSopcode_t opcode, size_t length, bool fin) {
    uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
    uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

    uint8_t headerSize = createHeader(&buffer[0], opcode, length, _cIsClient, maskKey, fin);

    if(mConnection->send(&buffer[0], headerSize) != headerSize) {
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
bool WebSocketConnection::sendFrame( WSopcode_t opcode, uint8_t * payload, size_t length, bool fin, bool headerToPayload) {
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
        if(mConnection->send(&payloadPtr[(WEBSOCKETS_MAX_HEADER_SIZE - headerSize)], (length + headerSize)) != (length + headerSize)) {
            ret = false;
        }
    } else {
        // send header
        if(mConnection->send(&buffer[0], headerSize) != headerSize) {
            ret = false;
        }

        if(payloadPtr && length > 0) {
            // send payload
            if(mConnection->send(&payloadPtr[0], length) != length) {
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
