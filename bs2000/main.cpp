#include <string>
#include "router.h"
#include <iostream>
#ifdef __unix__
    #include "nlohmann/json.h"
#else
    #include "json.h"
#endif

void getHandler(Context* context){
    nlohmann::json obj;
    obj["value"] = nlohmann::json::array({"hello",100,false});
    obj["hallo"] = "Hallo Welt TEST";
    obj["newObj"] = nlohmann::json::object();
    obj["newObj"]["inner"] = false;
    obj["newObj"]["inner2"] = "test";
    obj["newObj"]["inner3"] = 3.2;
    context->setResponse(200, obj.dump());
}

void postHandler(Context* context){
    nlohmann::json request = nlohmann::json::parse(context->request.body);
    if(request.is_null()){
        context->setResponse(400,"");
    }
    std::string dataField = request["data"].dump();
    nlohmann::json dataField2 = request["data5"];
    if(dataField2.is_null()){
        std::cout <<"missing field"<<std::endl;
    }
    nlohmann::json postResponse;
    postResponse["Response"] = "OK";
    context->setResponse(200, postResponse);
}

void getHtmlHandler(Context * c){
    c->HTML(HTTP_OK,"<!DOCTYPE html><html><body>Hello, World!</body></html>");
}

void showHTMLPage(Context * c){
    c->LoadHTML(HTTP_OK, "HELLOWORLD.HTML");
}

int main(){
    router r;
    r.GET("/", getHandler);
    r.POST("/album",postHandler);
    r.GET("/html",getHtmlHandler);
    r.GET("/Hello",showHTMLPage);
    r.run("localhost:8080");
    return 0;
}