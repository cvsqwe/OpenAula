#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


// Minimal single-purpose HTTP/1.1 server: exact-path routing for the JSON
// API plus one static-file fallback for the web app itself. Written by
// hand against raw POSIX sockets rather than pulling in a third-party HTTP
// library, matching the rest of this project's preference for small,
// dependency-free daemons (see daemon/main.cpp) - openaula-webd links
// nothing but openaula-core and pthreads, same as openaula-daemon.
//
// Linux-only (uses /proc/self/exe and BSD sockets directly), which is
// already true of the whole project via hidapi-hidraw.
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

    // Any GET that doesn't match a registered route is served from `dir`
    // instead ("/" maps to index.html). Paths are sanitised against
    // traversal outside `dir`.
    void serveStatic(const std::string& dir);

    // Blocks forever, accepting and handling connections (one detached
    // thread per connection - traffic here is a handful of local-network
    // browser tabs, not a workload that needs a thread pool). Returns
    // false if the listening socket couldn't be created/bound.
    bool listen(const std::string& bindAddr, int port);


private:

    void handleConnection(int clientFd);
    bool serveStaticFile(const std::string& path, HttpResponse& res) const;

    std::unordered_map<std::string, HttpHandler> getRoutes;
    std::unordered_map<std::string, HttpHandler> postRoutes;
    std::mutex apiMutex;
    std::string staticDir;
};
