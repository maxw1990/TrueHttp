#define register

#include "router.h"
#include <cstring>
#include <stdio.h>
#include <cstdlib>
#include <cctype>
#include <memory>

#include <sys.types.h>
#include <sys.socket.h>
#include <netinet.in.h>
#include <arpa.inet.h>
#include <sys.poll.h>
#include <signal.h>

int timeout_ms = 2;
#define PORT 8080
int runningServer = true;

const unsigned char asciiTab[256] = {
    32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  13,  32,  32,  32,  32,  32,  32,  32,  10,
    32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
    32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
    32,  32,  32,  32,  32,  32,  32,  32,  91,  46,  60,  40,  43,  124, 38,  32,  32,  32,  32,  32,  32,  32,
    32,  32,  33,  36,  42,  41,  59,  94,  45,  47,  32,  32,  32,  32,  32,  32,  32,  32,  124, 44,  37,  95,
    62,  63,  32,  32,  32,  32,  32,  32,  238, 160, 161, 96,  58,  35,  64,  39,  61,  34,  230, 97,  98,  99,
    100, 101, 102, 103, 104, 105, 164, 165, 228, 163, 229, 168, 169, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    170, 171, 172, 173, 174, 175, 239, 126, 115, 116, 117, 118, 119, 120, 121, 122, 224, 225, 226, 227, 166, 162,
    236, 235, 167, 232, 237, 233, 231, 234, 158, 128, 129, 91,  132, 93,  148, 131, 123, 65,  66,  67,  68,  69,
    70,  71,  72,  73,  149, 136, 137, 138, 139, 140, 125, 74,  75,  76,  77,  78,  79,  80,  81,  82,  141, 142,
    143, 159, 144, 145, 92,  32,  83,  84,  85,  86,  87,  88,  89,  90,  146, 147, 134, 130, 156, 155, 48,  49,
    50,  51,  52,  53,  54,  55,  56,  57,  135, 123, 157, 125, 151, 126};

// ASCII to EBCDIC conversion table (simplified for common characters)
unsigned char ebcdicTab[256] = {
	0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f, 0x16, 0x05, 0x25, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26, 0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
	0x40, 0x5a, 0x7f, 0x7b, 0x5b, 0x6c, 0x50, 0x7d, 0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
	0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
	0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xbb, 0xbc, 0xbd, 0x6a, 0x6d,
	0x4a, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xfb, 0x4f, 0xfd, 0xff, 0x07,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x04, 0x06, 0x08,0x28, 0x29, 0x2A, 0x2b, 0x2c, 0x09, 0x0a, 0x14,
	0x30, 0x31, 0x15, 0x33, 0x34, 0x35, 0x36, 0x17,0x38, 0x39, 0x3a, 0x3b, 0x1a, 0x1b, 0x3e, 0x5f,
	0x41, 0xaa, 0xb0, 0xb1, 0x9f, 0xb2, 0xd0, 0xb5, 0x79, 0xb4, 0x9a, 0x8a, 0xba, 0xca, 0xaf, 0xa1,
	0x90, 0x8f, 0xea, 0xfa, 0xbe, 0xa0, 0xb6, 0xb3, 0x9d, 0xda, 0x9b, 0x8b, 0xb7, 0xb8, 0xb9, 0xab,
	0x64, 0x65, 0x62, 0x66, 0x63, 0x67, 0x9e, 0x68, 0x74, 0x71, 0x72, 0x73, 0x78, 0x75, 0x76, 0x77,
	0xac, 0x69, 0xed, 0xee, 0xeb, 0xef, 0xec, 0xbf, 0x80, 0xe0, 0xfe, 0xdd, 0xfc, 0xad, 0xae, 0x59,
	0x44, 0x45, 0x42, 0x46, 0x43, 0x47, 0x9c, 0x48, 0x54, 0x51, 0x52, 0x53, 0x58, 0x55, 0x56, 0x57,
	0x8c, 0x49, 0xcd, 0xce, 0xcb, 0xcf, 0xcc, 0xe1, 0x70, 0xc0, 0xde, 0xdb, 0xdc, 0x8d, 0x8e, 0xdf
};

// Function to convert ASCII string to EBCDIC string
std::string asciiToEbcdic(const std::string& asciiStr) {
    std::string ebcdicStr;
    for (char ch : asciiStr) {
        ebcdicStr += static_cast<char>(ebcdicTab[static_cast<unsigned char>(ch)]);
    }
    return ebcdicStr;
}

// Function to convert ASCII string to EBCDIC string
std::string ebcdicToAscii(const std::string& ebcdicStr) {
    std::string ascii;
    for (char ch : ebcdicStr) {
        ascii += static_cast<char>(asciiTab[static_cast<unsigned char>(ch)]);
    }
    return ascii;
}

void router::fkt(int sig)
{
    if(sig == SIGINT + SIG_PS){
        printf("K2-Taste gedrueckt\n");
        runningServer = false;
    }
}


struct client_data_t {
    int fd;
    int buffer_size = 8192;
    char *buffer;
    int data_len;

    client_data_t(){
        buffer =(char*)malloc(buffer_size);
    }

    int append(const char*data, int len){
        if(data_len + len > buffer_size){
            int new_size = buffer_size*2+data_len + len;
            char *new_buffer = (char*)realloc(buffer,new_size);
            if(!new_buffer){
                return -1;
            }
            buffer = new_buffer;
            buffer_size = new_size;
        }

        memcpy(buffer + data_len, data, len);
        data_len += len;
        return 0;
    }

    void clearBuffer(){
        memset(buffer,0,buffer_size);
        data_len = 0;
    }

    void freeBuffer(){
        free(buffer);
    }
};

void remove_client(struct pollfd fds[],client_data_t clients[], int index, int *nfds){
    printf("Client (socket %d) disconnected\n", fds[index].fd);
    soc_close(fds[index].fd);
    fds[index].fd = -1;
    clients[index].fd = -1;
    clients[index].clearBuffer();
    while(*nfds > 1 && fds[*nfds-1].fd == -1){
        (*nfds)--;
    }
}

router::router(){

};

router::~router(){
};

void router::POST(const char* address, void  (*handler)(Context *) ){
    // append char and function
    handler_list[handlercount] = {handler, address, HTTPMETHOD::POST};
    handlercount++;
};

void router::GET(const char* address, void  (*handler)(Context *) ){
    // append char and function
    handler_list[handlercount] = {handler, address, HTTPMETHOD::GET};
    handlercount++;
};

void router::run(const char* ){
    int client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct pollfd fds[100];
    client_data_t clients[100];
    int nfds = 1;

    this->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(this->server_socket == -1){
        perror("Socket creation failed\n");
        exit(1);
    }

    int opt = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0){
        perror("setsockopt failed\n");
        close(server_socket);
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(server_socket,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0){
        perror("Bind failed\n");
        close(server_socket);
        exit(1);
    }

    if(listen(server_socket, 5 ) < 0){
        perror("listen failed\n");
        close(server_socket);
        exit(1);
    }

    printf("Server listening on port %i\n", PORT);

    fds[0].fd = server_socket;
    fds[0].events = POLLIN;

    for(int i = 1; i < 100; i++){
        fds[i].fd = -1;
        clients[i].fd = -1;
    }

    while(runningServer){
        int activity = soc_poll(fds, nfds, timeout_ms);
        signal(SIGINT + SIG_PS, router::fkt);
        if(activity < 0){
            perror("poll failed\n");
            break;
        }else if(activity == 0){
            printf("no data received, retry...\n");
            continue;
        }

        if(fds[0].revents & POLLIN){
            client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            if(client_sock < 0){
                perror("acception failed\n");
                continue;
            }

            printf("New client connected (socket %d)\n", client_sock);

            int i;
            for(i = 1; i < 100;i++){
                if(fds[i].fd == -1){
                    fds[i].fd = client_sock;
                    fds[i].events = POLLIN;
                    clients[i].fd = client_sock;
                    if( i >= nfds) nfds = i + 1;
                    break;
                }
            }

            if( i == 100){
                printf("Max clients reached.reject connection\n");
                soc_close(client_sock);
            }
        }

        for(int i=1; i < nfds; i++){
            if(fds[i].fd == -1){
                printf("client %i: -1 \n", i);
                continue;
            }

            if(fds[i].revents & POLLIN){
                char recv_buffer[8192];
                int bytes_received = recv(clients[i].fd,recv_buffer,sizeof(recv_buffer), 0);

                if(bytes_received <= 0){
                    remove_client(fds, clients, i, &nfds);
                }else{
                    // Append received data to client's buffer
                    clients[i].append(recv_buffer,bytes_received);
                    if (is_http_request_complete(clients[i].buffer, clients[i].data_len)) {
                        this->handle_request(&clients[i]);
                        clients[i].clearBuffer();
                    }
                }
            }
        }
    }

    for(int i = 1;i < 100; i++){
        clients[i].freeBuffer();
    }
    for(int i=0; i < nfds; i++){
        printf("closeing socket %i\n", fds[i].fd);
        soc_close(fds[i].fd);
    }
};

// Checks if HTTP request is complete (headers end with \r\n\r\n)
bool router::is_http_request_complete(const char *inputBuffer, int length) {
    std::string translationString(inputBuffer,length);
    std::string inputEbcdi = asciiToEbcdic(translationString);
    const char* buffer = inputEbcdi.c_str();
    const char* httpend = "\x0D\x25\x0D\x25"; // corresponds in ebcdic as \r\n\r\n

    if (strncmp(buffer, "GET", 3) == 0) {
        return (strstr(buffer, httpend) != NULL);
    }else if(strncmp(buffer, "POST", 4) == 0){
        const char* header_end = strstr(buffer, httpend);
        if (!header_end) {
            return false;  // No Content-Length header found
        }
        const char *content_length_pos = strstr(buffer, "Content-Length:");
        if (!content_length_pos) {
            return true;  // No Content-Length header found
        }
        int content_length = atoi(content_length_pos + 15); // Skip "Content-Length: "
        
        int body_size = strlen(header_end + strlen(httpend));
        if  (body_size  != content_length)  {
            return false;
        }
        return true;
    }else{
        return false;
    }
}

 void router::sendHttpResponse(Context* context, int clientFd){
    std::string responseMessage = context->response.toRawResponse();
    std::string outputString = ebcdicToAscii(responseMessage);
    send(clientFd, const_cast<char*>(outputString.c_str()), outputString.size(), 0);
 }

// Handle different HTTP methods (GET, POST, etc.)
void router::handle_request(client_data_t* client) {
    Context newContext;
    std::string inputMessage(client->buffer,client->data_len);
    std::string inputEbcdi = asciiToEbcdic(inputMessage);
    newContext.setRequest(inputEbcdi);

    bool handlerFound = false;
    // put in route as well to check which function should be called
    for(int i = 0; i< this->handlercount;i++ ){
        if(this->handler_list[i].method == newContext.request.method){
            if(handler_list[i].address == newContext.request.path){
                this->handler_list[i].handler(&newContext);
                this->sendHttpResponse(&newContext, client->fd);
                handlerFound = true;
                break;
            }
        }
    }
    if(!handlerFound){
        newContext.setResponse(400,"");
        this->sendHttpResponse(&newContext, client->fd);
    }
}