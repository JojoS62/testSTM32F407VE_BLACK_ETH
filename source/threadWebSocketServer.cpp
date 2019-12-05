/* 
 * Copyright (c) 2019 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "threadWebSocketServer.h"

#define STACKSIZE   (6 * 1024)
#define THREADNAME  "WebSocketServer"


/*
    Websocket Test
*/

class WSHandler: public WebSocketHandler
{
public:
    virtual void onMessage(char* text);
    virtual void onMessage(char* data, size_t size);
    virtual void onOpen(WebSocketConnection *webSocketConnection);
    virtual void onClose();
};

void WSHandler::onMessage(char* text)
{
    printf("TEXT: [%s]\r\n", text);
    const char msg[] = "hello world\r\n";
    _webSocketConnection->sendFrame(WSop_text, (uint8_t*)msg, sizeof(msg));
}

void WSHandler::onMessage(char* data, size_t size)
{
    int8_t lv = data[0];
    int8_t rv = data[1];
 
    printf("[%d/%d]\r\n", lv, rv);
}

void WSHandler::onOpen(WebSocketConnection *webSocketConnection)
{
    WebSocketHandler::onOpen(webSocketConnection);

    printf("websocket opened\r\n");
}
 
void WSHandler::onClose()
{
    printf("websocket closed\r\n");
}
 

ThreadWebSocketServer::ThreadWebSocketServer(NetworkInterface* network, int portNo) :
    _thread(osPriorityNormal, STACKSIZE, nullptr, THREADNAME)
{
    _network = network;
    _portNo = portNo;
}

/*
    start() : starts the thread
*/
void ThreadWebSocketServer::start()
{
    _running = true;
    _thread.start( callback(this, &ThreadWebSocketServer::myThreadFn) );
}


/*
    start() : starts the thread
*/
void ThreadWebSocketServer::myThreadFn()
{
    // thread local objects
    // take care of thread stacksize !

    WebSocketServer ws_server;
    WSHandler handler;

    if (!ws_server.init(_network, _portNo)) {
        printf("Failed to init server\r\n");
    }

    ws_server.setHandler("/ws/", &handler);
    ws_server.run();
}

