# TrueHTTP

Simple middleware router for C++.

Usage:

```c++
int main() {

    router r;
    r.GET("/", getHandler);
    r.POST("/album",postHandler);
    r.run("localhost:8080");
    return 0;
}
```
