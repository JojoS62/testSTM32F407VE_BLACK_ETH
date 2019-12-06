#include "mbed.h"
#include "rtos.h"

#include "http_server.h"
#include "http_response_builder.h"
#include "network-helper.h"
#include "WebsocketHandlers.h"

//#include "TextLCD.h"
#include "threadIO.h"
#include "MQTTThreadedClient.h"

#define SAMPLE_TIME     1000 // milli-sec
#define COMPLETED_FLAG (1UL << 0)
PlatformMutex stdio_mutex;
EventFlags threadFlag;

void print_stats()
{
    mbed_stats_socket_t stats[MBED_CONF_NSAPI_SOCKET_STATS_MAX_COUNT];
    static int num = 0;
    int count;

    memset(stats, 0, sizeof(mbed_stats_socket_t) * MBED_CONF_NSAPI_SOCKET_STATS_MAX_COUNT);
    printf("%-15s%-15s%-15s%-15s%-15s%-15s%-15s\n", "Num", "ID", "State", "Proto", "Sent", "Recv", "Time");
    //while (COMPLETED_FLAG != threadFlag.get()) {
        count = SocketStats::mbed_stats_socket_get_each(&stats[0], MBED_CONF_NSAPI_SOCKET_STATS_MAX_COUNT);
        for (int i = 0; i < count; i++) {
            stdio_mutex.lock();
            printf("\n%-15d", num);
            printf("%-15p", stats[i].reference_id);

            switch (stats[i].state) {
                case SOCK_CLOSED:
                    printf("%-15s", "Closed");
                    break;
                case SOCK_OPEN:
                    printf("%-15s", "Open");
                    break;
                case SOCK_CONNECTED:
                    printf("%-15s", "Connected");
                    break;
                case SOCK_LISTEN:
                    printf("%-15s", "Listen");
                    break;
                default:
                    printf("%-15s", "Error");
                    break;
            }

            if (NSAPI_TCP == stats[i].proto) {
                printf("%-15s", "TCP");
            } else {
                printf("%-15s", "UDP");
            }
            printf("%-15d", stats[i].sent_bytes);
            printf("%-15d", stats[i].recv_bytes);
            printf("%-15lld\n", stats[i].last_change_tick);
            stdio_mutex.unlock();
        }
        num++;
        //ThisThread::sleep_for(SAMPLE_TIME);
    //}
    // Now allow the stats thread to simply exit by itself gracefully.
}

#define USE_HTTPSERVER
//#define USE_WEBSOCKETSERVER
//#define USE_MQTT

#define DEFAULT_STACK_SIZE (4096)

DigitalOut led(LED1);

//ThreadIO threadIO(1000);
Thread msgSender(osPriorityNormal, DEFAULT_STACK_SIZE * 3);

// Requests come in here
void request_handler(ParsedHttpRequest* request, TCPSocket* socket) {

#if 1
    printf("[Http]Request came in: %s %s\n", http_method_str(request->get_method()), request->get_url().c_str());
    
	vector<string*>  headerFields = request->get_headers_fields();
	vector<string*>  headerValues = request->get_headers_values();
    
    for (uint i=0; i < headerFields.size(); i++) {
        printf("[%d]", i);
        printf((headerFields[i])->c_str());
        printf(" : ");
        printf((headerValues[i])->c_str());
        printf("\n");
    }
    fflush(stdout);
#endif

    if (request->get_method() == HTTP_GET && request->get_url() == "/") {
        HttpResponseBuilder builder(200);
        builder.set_header("Content-Type", "text/html; charset=utf-8");

        char response[] = "<html><head><title>Hello from mbed</title></head>"
            "<body>"
                "<h1>mbed webserver</h1>"
                "<button id=\"toggle\">Toggle LED</button>"
                "<script>document.querySelector('#toggle').onclick = function() {"
                    "var x = new XMLHttpRequest(); x.open('POST', '/toggle'); x.send();"
                "}</script>"
            "</body></html>";

        builder.send(socket, response, sizeof(response) - 1);
    }
    else if (request->get_method() == HTTP_POST && request->get_url() == "/toggle") {
//        printf("toggle LED called\n\n");
        led = !led;

        HttpResponseBuilder builder(200);
        builder.send(socket, NULL, 0);
    }
    else {
        HttpResponseBuilder builder(404);
        builder.send(socket, NULL, 0);
    }
}

#ifdef USE_MQTT
using namespace MQTT;
/*  
    MQTT
*/
static const char * clientID = "mbed-sample";
//static const char * userID = "";
//static const char * password = "";
static const char * topic_1 = "mbed-sample";
static const char * topic_2 = "test";

int arrivedcount = 0;

void messageArrived(MessageData& md)
{
    Message &message = md.message;
    printf("Arrived Callback 1 : qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload [%.*s]\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
}

class CallbackTest
{
    public:
    
    CallbackTest()
        : arrivedcount(0)
    {}
    
    void messageArrived(MessageData& md)
    {
        Message &message = md.message;
        printf("Arrived Callback 2 : qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
        printf("Payload [%.*s]\n", message.payloadlen, (char*)message.payload);
        ++arrivedcount;
    }
    
    private:
    
    int arrivedcount;
};
#endif


int main() {
	
    // IO Thread
    //threadIO.start();
    
    // Connect to the network with the default networking interface
    // if you use WiFi: see mbed_app.json for the credentials
    NetworkInterface* network = connect_to_default_network_interface();
    if (!network) {
        printf("Cannot connect to the network, see serial output\n");
        return 1;
    } 
	
    //Thread *thread = new Thread(osPriorityNormal1, 2048);
    //thread->start(print_stats);

#ifdef USE_HTTPSERVER	
    WSHandler wsHandler;

    HttpServer server(network);
     server.setWSHandler("/ws/", &wsHandler);

    nsapi_error_t res = server.start(8080, &request_handler);

    if (res == NSAPI_ERROR_OK) {
        printf("Server is listening at http://%s:8080\n", network->get_ip_address());
    }
    else {
        printf("Server could not be started... %d\n", res);
    }
#endif

#ifdef USE_WEBSOCKETSERVER
    ThreadWebSocketServer   threadWebSocketServer(network, 8081);
    threadWebSocketServer.start();
#endif

#ifdef USE_MQTT
    float version = 0.6;
    CallbackTest testcb;

    printf("HelloMQTT: version is %.2f\n", version);
    MQTTThreadedClient mqtt(network);


    //const char* hostname = "jojosRPi3-1";
    const char* hostname = "192.168.100.28";
    int port = 1883;

    MQTTPacket_connectData logindata = MQTTPacket_connectData_initializer;
    logindata.MQTTVersion = 3;
    logindata.clientID.cstring = (char *) clientID;
    //logindata.username.cstring = (char *) userID;
    //logindata.password.cstring = (char *) password;
    
    mqtt.setConnectionParameters(hostname, port, logindata);
    mqtt.addTopicHandler(topic_1, messageArrived);
    mqtt.addTopicHandler(topic_2, &testcb, &CallbackTest::messageArrived);

    // Start the data producer
    msgSender.start(mbed::callback(&mqtt, &MQTTThreadedClient::startListener));
    
    int i = 0;
    while(true)
    {
        PubMessage message;
        message.qos = QOS0;
        message.id = 123;
        
        strcpy(&message.topic[0], topic_1);
        sprintf(&message.payload[0], "Testing %d", i);
        message.payloadlen = strlen((const char *) &message.payload[0]);
        mqtt.publish(message);
        
        i++;
        //TODO: Nothing here yet ...
        ThisThread::sleep_for(10000);
    }
#endif

    while(true) {
        ThisThread::sleep_for(10000);
    }
}
