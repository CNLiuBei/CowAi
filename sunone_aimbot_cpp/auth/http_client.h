#pragma once

#include <string>
#include <map>
#include <vector>

namespace TGAuth {

// HTTP 响应结构
struct HttpResponse {
    bool success = false;
    int status_code = 0;
    std::string body;
    std::string error_message;
    std::map<std::string, std::string> headers;
};

// HTTP 请求头类型
using HttpHeaders = std::map<std::string, std::string>;

// HTTP 客户端类 (使用 WinHTTP)
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // 设置超时时间 (毫秒)
    void setTimeout(int connect_timeout_ms, int read_timeout_ms);

    // 设置 User-Agent
    void setUserAgent(const std::string& user_agent);

    // GET 请求
    HttpResponse get(
        const std::string& url,
        const HttpHeaders& headers = {}
    );

    // POST 请求
    HttpResponse post(
        const std::string& url,
        const std::string& body,
        const HttpHeaders& headers = {}
    );

    // PUT 请求
    HttpResponse put(
        const std::string& url,
        const std::string& body,
        const HttpHeaders& headers = {}
    );

    // DELETE 请求
    HttpResponse del(
        const std::string& url,
        const HttpHeaders& headers = {}
    );

private:
    // 执行 HTTP 请求
    HttpResponse executeRequest(
        const std::string& method,
        const std::string& url,
        const std::string& body,
        const HttpHeaders& headers
    );

    // 解析 URL
    bool parseUrl(
        const std::string& url,
        std::string& host,
        std::string& path,
        int& port,
        bool& is_https
    );

    int m_connect_timeout_ms = 10000;  // 10 秒
    int m_read_timeout_ms = 30000;     // 30 秒
    std::string m_user_agent = "TGAuth-Client/1.0";
};

} // namespace TGAuth
