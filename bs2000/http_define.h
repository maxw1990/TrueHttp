#ifndef HTTP_DEFINE_H
#define HTTP_DEFINE_H
#pragma once
#include <cstdio>  // For fopen, fread, fclose
#include <string>
#include <map>
#include <stdlib.h>
#ifdef __unix__
    #include "nlohmann/json.h"
#else
    #include "json.h"
#endif

enum HTTPMETHOD{
    GET = 1,
    POST = 2,
    UNKNOW = 3
};

struct HttpRequest {
    HTTPMETHOD method = HTTPMETHOD::GET;    // HTTP method (GET, POST, etc.)
    std::string path;                       // Requested resource path
    std::string host;                       // Host (e.g., example.com)
    int content_length;
    std::map<std::string, std::string> headers;  // Headers as key-value pairs
    std::string body;                        // Request body (for POST, PUT)
    std::string version;

    static std::string httpMethodToString(HTTPMETHOD method){
        switch (method)
        {
        case HTTPMETHOD::GET:
            return "GET";
            break;
        case HTTPMETHOD::POST:
            return "POST";
        default:
            return "GET";
            break;
        }
    }

    static HTTPMETHOD stringToHttpMethod(std::string method){
        if(method.compare("GET") == 0){
            return HTTPMETHOD::GET;
        }else if(method.compare("POST") == 0){
            return HTTPMETHOD::POST;
        }else{
            return HTTPMETHOD::UNKNOW;
        }
    }
    // Function to format as raw HTTP request (for sending via socket)
    std::string toRawRequest() const {
        std::string request = HttpRequest::httpMethodToString(method) + " " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        for (const std::pair<const std::string, std::string>& value : headers) {
            request += value.first + ": " + value.second + "\r\n";
        }
        request += "\r\n";  // End of headers
        request += body;    // Append body if present
        return request;
    }
};

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

 void JSON(HttpStatus status, nlohmann::json& json){
    response.status_code = status;
    const std::string jsonBody = json.dump();
    response.headers["Content-Length"] = std::to_string(jsonBody.length());
    response.headers["Content-Type"] = "json/application";
    response.body = jsonBody;
 }

 void HTML(HttpStatus status, const std::string& body){
    response.status_code = status;
    // Set default headers
    response.headers["Content-Length"] = std::to_string(body.length());
    response.headers["Content-Type"] = "text/html";
    response.headers["Connection"] = "close";
    response.body = body;
 }

 int LoadHTML(HttpStatus status, const std::string& htmlFileName){
    response.status_code = status;
    // Set default headers
    FILE* file = fopen(htmlFileName.c_str(), "rb"); // Open file in binary mode

    if (!file) {
        perror("Error opening file!");
        response.status_code = HTTP_INTERNAL_ERROR;
        response.headers["Content-Type"] = "text/html";
        response.headers["Connection"] = "close";
        return 1;
    }

    // Move to the end to determine file size
    fseek(file, 0, SEEK_END);
    std::size_t size = ftell(file);
    rewind(file); // Move back to the start

    // Read the file into a string
    std::string fileContent(size, '\0'); // Allocate space
    fread(&fileContent[0], 1, size, file); // Read data into string

    fclose(file); // Close file

    response.headers["Content-Length"] = std::to_string(fileContent.length());
    response.headers["Content-Type"] = "text/html";
    response.headers["Connection"] = "close";
    response.body = fileContent;
    return 0;
 }

 void setResponse(int status_code, nlohmann::json& json) {
    response.status_code = status_code;

    // Set corresponding status message
    switch (status_code) {
        case 200: response.status_message = "OK"; break;
        case 400: response.status_message = "Bad Request"; break;
        case 404: response.status_message = "Not Found"; break;
        case 500: response.status_message = "Internal Server Error"; break;
        default: response.status_message = "Unknown"; break;
    }
    const std::string jsonBody = json.dump();
    response.headers["Content-Length"] = std::to_string(jsonBody.length());
    response.headers["Content-Type"] = "json/application";
    response.headers["Connection"] = "close";
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
    response.headers["Connection"] = "close";
    response.body = body;  
 };

 HttpRequest parseHttpRequest(const std::string& raw_request) {
    HttpRequest request;
    size_t pos = 0, line_end;

    // 1. Extract request line
    line_end = raw_request.find("\x0D\x25", pos); // find \r\n ebcdic
    if (line_end == std::string::npos) return request;  // Malformed request

    std::string request_line = raw_request.substr(pos, line_end - pos);
    pos = line_end + 2;  // Move past "\r\n"

    size_t method_end = request_line.find(' ');
    if (method_end == std::string::npos) return request;

    size_t path_end = request_line.find(' ', method_end + 1);
    if (path_end == std::string::npos) return request;

    request.method = HttpRequest::stringToHttpMethod(request_line.substr(0, method_end));
    request.path = request_line.substr(method_end + 1, path_end - method_end - 1);
    request.version = request_line.substr(path_end + 1);

    // 2. Extract headers
    while ((line_end = raw_request.find("\x0D\x25", pos)) != std::string::npos && line_end > pos) {
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

 void setRequest(const std::string& rawRequest){
    request = parseHttpRequest(rawRequest);
 };

};
#endif