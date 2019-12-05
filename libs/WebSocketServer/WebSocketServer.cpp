#include "WebSocketServer.h"
#include "WebSocketConnection.h"

WebSocketServer::WebSocketServer()
{
}

WebSocketServer::~WebSocketServer()
{
}

bool WebSocketServer::init(NetworkInterface *net, int port)
{
	nsapi_size_or_error_t result;
	result = mTCPSocketServer.open(net);

	mTCPSocketServer.set_blocking(true);

	int ret = mTCPSocketServer.bind(port);
	if (ret != 0) {
		printf("ERROR: Failed to bind %d\r\n", ret);
		return false;
	}
	ret = mTCPSocketServer.listen();
	if (ret != 0) {
		printf("ERROR: Failed to listen %d\r\n", ret);
		return false;
	}

	return true;
}

void WebSocketServer::run()
{
	WebSocketConnection connection(this);

	while (true) {
		// printf("accepting\r\n");
		nsapi_error_t ret = 0;
		TCPSocket *sock = mTCPSocketServer.accept(&ret);
		connection.setTCPSocketConnection(sock);
		if (ret != 0) {
			continue;
		}
		connection.run();
	}
}

void WebSocketServer::setHandler(const char* path, WebSocketHandler* handler)
{
	mHandlers[path] = handler;
}

WebSocketHandler* WebSocketServer::getHandler(const char* path)
{
	WebSocketHandlerContainer::iterator it;

	it = mHandlers.find(path);
	if (it != mHandlers.end()) {
		return it->second;
	}
	return NULL;
}

