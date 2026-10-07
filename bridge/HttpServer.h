#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


// tiny HTTP/1.1 server: exact-path routes for the API, static files for the rest
struct HttpRequest
{
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> query;
    std::string body;
};

struct HttpResponse
{
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
};

using HttpHandler = std::function<void(const HttpRequest&, HttpResponse&)>;


class HttpServer
{
public:

    void get(const std::string& path, HttpHandler handler);
    void post(const std::string& path, HttpHandler handler);

    // unmatched GETs are served from dir ("/" -> index.html)
    void serveStatic(const std::string& dir);

    // blocks forever, one thread per connection
    bool listen(const std::string& bindAddr, int port);


private:

    void handleConnection(int clientFd);
    bool serveStaticFile(const std::string& path, HttpResponse& res) const;

    std::unordered_map<std::string, HttpHandler> getRoutes;
    std::unordered_map<std::string, HttpHandler> postRoutes;
    std::mutex apiMutex;
    std::string staticDir;
};
