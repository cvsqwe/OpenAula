#include "HttpServer.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>


namespace
{

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string urlDecode(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for(size_t i = 0; i < s.size(); i++)
    {
        if(s[i] == '%' && i + 2 < s.size())
        {
            int value = std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
            out += (char)value;
            i += 2;
        }
        else if(s[i] == '+')
        {
            out += ' ';
        }
        else
        {
            out += s[i];
        }
    }

    return out;
}

std::string mimeTypeFor(const std::string& path)
{
    auto dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : toLower(path.substr(dot + 1));

    if(ext == "html") return "text/html; charset=utf-8";
    if(ext == "css")  return "text/css; charset=utf-8";
    if(ext == "js")   return "application/javascript; charset=utf-8";
    if(ext == "json") return "application/json; charset=utf-8";
    if(ext == "svg")  return "image/svg+xml";
    if(ext == "png")  return "image/png";
    if(ext == "ico")  return "image/x-icon";
    if(ext == "woff2") return "font/woff2";

    return "application/octet-stream";
}

std::string statusText(int status)
{
    switch(status)
    {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "OK";
    }
}

}



void HttpServer::get(const std::string& path, HttpHandler handler)
{
    getRoutes[path] = std::move(handler);
}



void HttpServer::post(const std::string& path, HttpHandler handler)
{
    postRoutes[path] = std::move(handler);
}



void HttpServer::serveStatic(const std::string& dir)
{
    staticDir = dir;
}



bool HttpServer::serveStaticFile(const std::string& reqPath, HttpResponse& res) const
{
    if(staticDir.empty())
        return false;

    std::string relative = reqPath == "/" ? "/index.html" : reqPath;

    std::filesystem::path base = std::filesystem::weakly_canonical(staticDir);
    std::filesystem::path full = std::filesystem::weakly_canonical(base / relative.substr(1));

    // don't let ".." escape the web root
    auto [baseEnd, fullMismatch] = std::mismatch(base.begin(), base.end(), full.begin());
    if(baseEnd != base.end())
        return false;

    std::ifstream file(full, std::ios::binary);
    if(!file.is_open())
        return false;

    std::ostringstream contents;
    contents << file.rdbuf();

    res.status = 200;
    res.contentType = mimeTypeFor(full.string());
    res.body = contents.str();
    return true;
}



void HttpServer::handleConnection(int clientFd)
{
    std::string raw;
    char buf[8192];

    size_t headerEnd = std::string::npos;

    // headers first, then the body (Content-Length)
    while(headerEnd == std::string::npos)
    {
        ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
        if(n <= 0) { close(clientFd); return; }

        raw.append(buf, n);
        headerEnd = raw.find("\r\n\r\n");

        if(raw.size() > 1 << 20) { close(clientFd); return; } // 1MB header guard
    }

    std::string headerBlock = raw.substr(0, headerEnd);
    std::string body = raw.substr(headerEnd + 4);

    std::istringstream headerStream(headerBlock);
    std::string requestLine;
    std::getline(headerStream, requestLine);
    if(!requestLine.empty() && requestLine.back() == '\r')
        requestLine.pop_back();

    std::istringstream lineStream(requestLine);
    std::string method, target, version;
    lineStream >> method >> target >> version;

    size_t contentLength = 0;
    std::string line;

    while(std::getline(headerStream, line))
    {
        if(!line.empty() && line.back() == '\r')
            line.pop_back();

        auto colon = line.find(':');
        if(colon == std::string::npos)
            continue;

        std::string key = toLower(line.substr(0, colon));
        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        if(key == "content-length")
            contentLength = (size_t)std::strtoul(value.c_str(), nullptr, 10);
    }

    while(body.size() < contentLength)
    {
        ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
        if(n <= 0) break;
        body.append(buf, n);
    }

    HttpRequest req;
    req.method = method;
    req.body = body;

    auto qpos = target.find('?');
    std::string rawPath = qpos == std::string::npos ? target : target.substr(0, qpos);
    req.path = urlDecode(rawPath);

    if(qpos != std::string::npos)
    {
        std::istringstream qs(target.substr(qpos + 1));
        std::string pair;

        while(std::getline(qs, pair, '&'))
        {
            auto eq = pair.find('=');
            if(eq == std::string::npos)
                continue;

            req.query[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        }
    }

    HttpResponse res;

    if(method == "OPTIONS")
    {
        res.status = 204;
        res.body = "";
    }
    // handlers all load/save the same files, run them one at a time
    else if(method == "GET" && getRoutes.count(req.path))
    {
        std::lock_guard<std::mutex> lock(apiMutex);
        getRoutes.at(req.path)(req, res);
    }
    else if(method == "POST" && postRoutes.count(req.path))
    {
        std::lock_guard<std::mutex> lock(apiMutex);
        postRoutes.at(req.path)(req, res);
    }
    else if(method == "GET" && serveStaticFile(req.path, res))
    {
    }
    else
    {
        res.status = 404;
        res.contentType = "application/json";
        res.body = "{\"error\":\"not found\"}";
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << res.status << " " << statusText(res.status) << "\r\n";
    response << "Content-Type: " << res.contentType << "\r\n";
    response << "Content-Length: " << res.body.size() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Connection: close\r\n\r\n";
    response << res.body;

    std::string out = response.str();
    size_t sent = 0;

    while(sent < out.size())
    {
        ssize_t n = send(clientFd, out.data() + sent, out.size() - sent, 0);
        if(n <= 0) break;
        sent += n;
    }

    close(clientFd);
}



bool HttpServer::listen(const std::string& bindAddr, int port)
{
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverFd < 0)
    {
        std::cerr << "openaula-webd: socket() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    int reuse = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if(bindAddr.empty() || bindAddr == "0.0.0.0")
        addr.sin_addr.s_addr = INADDR_ANY;
    else
        inet_pton(AF_INET, bindAddr.c_str(), &addr.sin_addr);

    if(bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "openaula-webd: bind() to port " << port << " failed: " << std::strerror(errno) << std::endl;
        close(serverFd);
        return false;
    }

    if(::listen(serverFd, 32) < 0)
    {
        std::cerr << "openaula-webd: listen() failed: " << std::strerror(errno) << std::endl;
        close(serverFd);
        return false;
    }

    while(true)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
        if(clientFd < 0)
            continue;

        std::thread(&HttpServer::handleConnection, this, clientFd).detach();
    }
}
