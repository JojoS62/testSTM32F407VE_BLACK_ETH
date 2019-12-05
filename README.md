# testSTM32F407VE_BLACK_ETH
HTTP and Websocket Server project

This is a working copy of a HTTP server component for mbed-os 5.

# Current state: Memory leaking
currently, for each new request a ClientConnection object is created. A thread for handling the request/response is started. It seems that this thread does not free its memory.
ClientConnection delete gets called, Thread terminate gets called, where is the leak?