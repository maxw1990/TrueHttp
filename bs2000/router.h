#ifndef ROUTER_H
#define ROUTER_H
#pragma once
#include "http_define.h"
#include <string>

struct client_data_t;

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

class router: public IRouter{
    public:
        router();
        ~router();

        virtual void GET(const char* address, void  (*handler)(Context *) ) override;
        virtual void POST(const char* address, void  (*handler)(Context *) ) override;
        virtual void run(const char* ) override;
    private:
    bool is_http_request_complete(const char *buffer, int length);
    void handle_request(client_data_t* client);
    void sendHttpResponse(Context* context, int clientFd);
    static void fkt(int sig);
    handlerRouting handler_list[100];
    int handlercount = 0;
    int server_socket;
};
#endif