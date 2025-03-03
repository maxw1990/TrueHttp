#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <map>
#include <signal.h>
#include "json_parser.h"

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 8192

struct HttpRequest {
    std::string method;                     // HTTP method (GET, POST, etc.)
    std::string path;                       // Requested resource path
    std::string host;                       // Host (e.g., example.com)
    int content_length;
    std::map<std::string, std::string> headers;  // Headers as key-value pairs
    std::string body;                        // Request body (for POST, PUT)
    std::string version;

    // Function to format as raw HTTP request (for sending via socket)
    std::string toRawRequest() const {
        std::string request = method + " " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        for (const std::pair<const std::string, std::string>& value : headers) {
            request += value.first + ": " + value.second + "\r\n";
        }
        request += "\r\n";  // End of headers
        request += body;    // Append body if present
        return request;
    }
};

int pollWithRetry(struct pollfd *fds, nfds_t nfds, int timeout) {
    int ret;
    do {
        ret = poll(fds, nfds, timeout);
    } while (ret == -1 && errno == EINTR);  // Retry if interrupted
    return ret;
}

struct HttpResponse {
    int status_code;  // HTTP status code (e.g., 200, 404)
    std::string status_message;  // Status message (e.g., "OK", "Not Found")
    std::map<std::string, std::string> headers;  // Response headers
    std::string body;  // Response body

    // Function to format the response into a raw HTTP response string
    std::string toRawResponse() const {
        std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + status_message + "\r\n";
        
        // Add headers
        for (const std::pair<const std::string, std::string> value : headers) {
            response += value.first + ": " + value.second + "\r\n";
        }

        response += "\r\n";  // End of headers
        response += body;    // Append body if present

        return response;
    }
};

enum HttpStatus{
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_NOT_FOUND = 404,
    HTTP_INTERNAL_ERROR = 500,
    HTTP_FORBIDDEN = 403,
    HTTP_UNAUTHORIZED = 401
};

class Context{
public:
HttpRequest request;
HttpResponse response;
 void sendHttpResponse(int clientFd){
    std::string responseMessage = response.toRawResponse();
    send(clientFd, responseMessage.c_str(), responseMessage.size(), 0);
 }

 void JSON(HttpStatus status, json::JSON& json){
    response.status_code = status;
    const std::string jsonBody = json.as_string();
    response.headers["Content-Length"] = std::to_string(jsonBody.length());
    response.headers["Content-Type"] = "json/application";
    response.body = jsonBody;
 }

 void setResponse(int status_code, json::JSON& json) {
    response.status_code = status_code;

    // Set corresponding status message
    switch (status_code) {
        case 200: response.status_message = "OK"; break;
        case 400: response.status_message = "Bad Request"; break;
        case 404: response.status_message = "Not Found"; break;
        case 500: response.status_message = "Internal Server Error"; break;
        default: response.status_message = "Unknown"; break;
    }
    const std::string jsonBody = json.as_string();
    response.headers["Content-Length"] = std::to_string(jsonBody.length());
    response.headers["Content-Type"] = "json/application";
    response.body = jsonBody;
 }

 void setResponse(int status_code,const std::string& body) {
    response.status_code = status_code;

    // Set corresponding status message
    switch (status_code) {
        case 200: response.status_message = "OK"; break;
        case 400: response.status_message = "Bad Request"; break;
        case 404: response.status_message = "Not Found"; break;
        case 500: response.status_message = "Internal Server Error"; break;
        default: response.status_message = "Unknown"; break;
    }

    // Set default headers
    response.headers["Content-Length"] = std::to_string(body.length());
    response.headers["Content-Type"] = "text/plain";
    response.body = body;  
 };

 HttpRequest parseHttpRequest(const std::string& raw_request) {
    HttpRequest request;
    size_t pos = 0, line_end;

    // 1. Extract request line
    line_end = raw_request.find("\r\n", pos);
    if (line_end == std::string::npos) return request;  // Malformed request

    std::string request_line = raw_request.substr(pos, line_end - pos);
    pos = line_end + 2;  // Move past "\r\n"

    size_t method_end = request_line.find(' ');
    if (method_end == std::string::npos) return request;

    size_t path_end = request_line.find(' ', method_end + 1);
    if (path_end == std::string::npos) return request;

    request.method = request_line.substr(0, method_end);
    request.path = request_line.substr(method_end + 1, path_end - method_end - 1);
    request.version = request_line.substr(path_end + 1);

    // 2. Extract headers
    while ((line_end = raw_request.find("\r\n", pos)) != std::string::npos && line_end > pos) {
        std::string header_line = raw_request.substr(pos, line_end - pos);
        pos = line_end + 2;  // Move past "\r\n"

        size_t colon_pos = header_line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = header_line.substr(0, colon_pos);
            std::string value = header_line.substr(colon_pos + 2);  // Skip ": "
            request.headers[key] = value;
        }
    }

    // 3. Move past empty line (separator between headers and body)
    pos += 2;

    // 4. Extract body (if any)
    if (pos < raw_request.size()) {
        request.body = raw_request.substr(pos);
    }

    return request;
 }

 void setRequest(const char* buffer, const int size){
    std::string rawRequest(buffer, size);
    request = parseHttpRequest(rawRequest);
 };

};

int timeout_ms = 1000;
// Stores partial data for each client
struct client_data_t{
    int fd;                // Client socket
    int buffer_size = 8192;
    char *buffer; // Partial request storage
    int data_len;          // Amount of data stored

    client_data_t(){
        buffer = (char*)malloc(buffer_size);
    }

    // Function to append data, resizing if necessary
    int append(const char *data, int len) {
        if (data_len + len > buffer_size) {
            int new_size = buffer_size * 2 + data_len + len;
            char *new_buffer = (char *)realloc(buffer, new_size);
            if (!new_buffer) {
                return -1;
            }
            buffer = new_buffer;
            buffer_size = new_size;
        }

        // Copy new data into buffer
        memcpy(buffer + data_len, data, len);
        data_len += len;
        //buffer[data_len] = '\0';
        return 0;
    }

    void clearBuffer(){
        memset(buffer,0,buffer_size);
        data_len = 0;
    }
};

void remove_client(struct pollfd fds[], client_data_t clients[], int index, int *nfds) {
    printf("Client (socket %d) disconnected.\n", fds[index].fd);
    close(fds[index].fd);
    fds[index].fd = -1;
    clients[index].fd = -1;
    clients[index].clearBuffer();
    // Adjust nfds if last client is removed
    while (*nfds > 1 && fds[*nfds - 1].fd == -1) {
        (*nfds)--;
    }
}

enum HTTPMETHOD{
    GET = 1,
    POST = 2,
    UNKNOW = 3
};

struct handlerRouting{
    void  (*handler)(Context*);
    std::string address;
    HTTPMETHOD method;
};

class IRouter{
    virtual void run(const char*) = 0;
    virtual void GET(const char* address, void (*handler)(Context *)) = 0;
    virtual void POST(const char* address, void  (*handler)(Context *) ) = 0;
};

class router: IRouter{
    public:
        router(){
    
        };
        ~router(){
            close(server_socket);
        }

        void GET(const char* address, void  (*handler)(Context *) ){
            // append char and function
            handler_list[handlercount] = {handler, address, HTTPMETHOD::GET};
            handlercount++;
        }
        void POST(const char* address, void  (*handler)(Context *) ){
            // append char and function
            handler_list[handlercount] = {handler, address, HTTPMETHOD::POST};
            handlercount++;
        }

        void run(const char* ){
            // run socket
            int client_sock;
            struct sockaddr_in server_addr, client_addr;
            socklen_t client_len = sizeof(client_addr);
            struct pollfd fds[MAX_CLIENTS];
            client_data_t clients[MAX_CLIENTS]; // Tracks partial client data
            int nfds = 1;
        
            // Create server socket
            this->server_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (server_socket == -1) {
                perror("Socket creation failed");
                exit(EXIT_FAILURE);
            }
        
            // Set socket option to allow address reuse
            int opt = 1;
            if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                perror("setsockopt failed");
                close(server_socket);
                exit(EXIT_FAILURE);
            }
        
            // Bind the socket
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(PORT);
        
            if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Bind failed");
                close(server_socket);
                exit(EXIT_FAILURE);
            }
        
            // Start listening
            if (listen(server_socket, 5) < 0) {
                perror("Listen failed");
                close(server_socket);
                exit(EXIT_FAILURE);
            }
        
            printf("Server is listening ppp on port %d...\n", PORT);
        
            // Initialize poll structure and client storage
            fds[0].fd = server_socket;
            fds[0].events = POLLIN;
        
            for (int i = 1; i < MAX_CLIENTS; i++) {
                fds[i].fd = -1;
                clients[i].fd = -1;
                clients[i].data_len = 0;
            }
        
            while (1) {
                int activity = pollWithRetry(fds, nfds, timeout_ms);
                if (activity < 0) {
                    perror("poll() failed");
                    break;
                }
                else if(activity == 0){
                    printf("No data received, retry ...\n");
                    continue;
                }
        
                // Accept new client connections
                if (fds[0].revents & POLLIN) {
                    client_sock = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
                    if (client_sock < 0) {
                        perror("Accept failed");
                        continue;
                    }
        
                    printf("New client connected (socket %d)\n", client_sock);
        
                    // Add client to poll array
                    int i;
                    for (i = 1; i < MAX_CLIENTS; i++) {
                        if (fds[i].fd == -1) {
                            fds[i].fd = client_sock;
                            fds[i].events = POLLIN;
                            clients[i].fd = client_sock;
                            clients[i].data_len = 0;
                            if (i >= nfds) nfds = i + 1;
                            break;
                        }
                    }
        
                    if (i == MAX_CLIENTS) {
                        printf("Max clients reached, rejecting connection.\n");
                        close(client_sock);
                    }
                }
        
                // Handle client data
                for (int i = 1; i < nfds; i++) {
                    if (fds[i].fd == -1) {
                        printf("client %i -1: \n", i);
                        continue;
                    }
        
                    if (fds[i].revents & POLLIN) {
                        char recv_buffer[BUFFER_SIZE];
                        ssize_t bytes_received = recv(clients[i].fd, recv_buffer, sizeof(recv_buffer), 0);
        
                        if (bytes_received <= 0) {  // Client disconnected
                            remove_client(fds, clients, i, &nfds);
                        } else {
                            // Append received data to client's buffe
                            clients[i].append(recv_buffer,bytes_received);
                            printf("%.*s\n", (int)clients[i].data_len, clients[i].buffer);
                            if (is_http_request_complete(clients[i].buffer, clients[i].data_len)) {
                                handle_request(clients[i]);
                            }
                        }
                    }
                }
            }
        }
    private:
    // Checks if HTTP request is complete (headers end with \r\n\r\n)
    bool is_http_request_complete(const char *buffer, int length) {
        const char *separator = "\r\n\r\n";
        if (strncmp(buffer, "GET", 3) == 0) {
            return (strstr(buffer, separator) != NULL);
        }else if(strncmp(buffer, "POST", 4) == 0){
            const char* header_end = strstr(buffer, separator);
            if (!header_end) {
                return false;  // No Content-Length header found
            }
            const char *content_length_pos = strstr(buffer, "Content-Length:");
            if (!content_length_pos) {
                return true;  // No Content-Length header found
            }
            int content_length = atoi(content_length_pos + 15); // Skip "Content-Length: "
            
            int body_size = strlen(header_end + strlen(separator));
            if  (body_size  != content_length)  {
                return false;
            }
            return true;
        }else{
            return false;
        }
    }
    // Handle different HTTP methods (GET, POST, etc.)
    void handle_request(client_data_t client) {
        Context newContext;
        newContext.setRequest(client.buffer, client.data_len);
        // Check if it's a GET request
            bool handlerFound = false;
            if (newContext.request.method == "GET") {
                // here fill new struct Context with header data and body and put it into function?!
                // put in route as well to check which function should be called
                for(int i = 0; i< this->handlercount;i++ ){
                    if(this->handler_list[i].method == HTTPMETHOD::GET){
                        if(handler_list[i].address == newContext.request.path){
                            this->handler_list[i].handler(&newContext);
                            newContext.sendHttpResponse(client.fd);
                            handlerFound = true;
                            break;
                        }
                    }
                }
            }
            // Check if it's a POST request
            else if (newContext.request.method == "POST") {
                for(int i = 0; i< this->handlercount;i++ ){
                    if(this->handler_list[i].method == HTTPMETHOD::POST){
                        if(handler_list[i].address == newContext.request.path){
                            this->handler_list[i].handler(&newContext);
                            newContext.sendHttpResponse(client.fd);
                            handlerFound = true;
                            break;
                        }
                    }
                }
            } else {
                newContext.setResponse(400,"");
                newContext.sendHttpResponse(client.fd);
                handlerFound = true;
            }
            if(!handlerFound){
                newContext.setResponse(400,"");
                newContext.sendHttpResponse(client.fd);
            }
    }
    handlerRouting handler_list[100];
    int handlercount = 0;
    int server_socket;
};

void getHandler(Context* context){
    json::JSON obj;
    obj["value"] = json::Array();
    obj["value"].append("hello",100,false);
    obj["hallo"] = "Hallo Welt TEST";
    obj["newObj"] = json::Object();
    obj["newObj"]["inner"] = false;
    obj["newObj"]["inner2"] = "test";
    obj["newObj"]["inner3"] = 3.2;
    context->setResponse(200, obj.as_string());
}

void postHandler(Context* context){
    json::JSON request = json::JSON::Parse(context->request.body);
    std::string dataField = request["data"].as_string();
    json::JSON dataField2 = request["data5"];
    if(dataField2.IsNull()){
        std::cout <<"missing field"<<std::endl;
    }
    json::JSON postResponse;
    postResponse["Response"] = "OK";
    context->setResponse(200, postResponse);
}


int main() {

    router r;
    r.GET("/", getHandler);
    r.POST("/album",postHandler);
    r.run("localhost:8080");
    return 0;
}