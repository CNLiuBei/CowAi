#include "http_client.h"

#include <windows.h>
#include <winhttp.h>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace TGAuth {

HttpClient::HttpClient() = default;

HttpClient::~HttpClient() = default;

void HttpClient::setTimeout(int connect_timeout_ms, int read_timeout_ms) {
    m_connect_timeout_ms = connect_timeout_ms;
    m_read_timeout_ms = read_timeout_ms;
}

void HttpClient::setUserAgent(const std::string& user_agent) {
    m_user_agent = user_agent;
}

HttpResponse HttpClient::get(const std::string& url, const HttpHeaders& headers) {
    return executeRequest("GET", url, "", headers);
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body, const HttpHeaders& headers) {
    return executeRequest("POST", url, body, headers);
}

HttpResponse HttpClient::put(const std::string& url, const std::string& body, const HttpHeaders& headers) {
    return executeRequest("PUT", url, body, headers);
}

HttpResponse HttpClient::del(const std::string& url, const HttpHeaders& headers) {
    return executeRequest("DELETE", url, "", headers);
}

bool HttpClient::parseUrl(
    const std::string& url,
    std::string& host,
    std::string& path,
    int& port,
    bool& is_https
) {
    // 检查协议
    size_t protocol_end = url.find("://");
    if (protocol_end == std::string::npos) {
        return false;
    }

    std::string protocol = url.substr(0, protocol_end);
    if (protocol == "https") {
        is_https = true;
        port = 443;
    } else if (protocol == "http") {
        is_https = false;
        port = 80;
    } else {
        return false;
    }

    // 解析主机和路径
    size_t host_start = protocol_end + 3;
    size_t path_start = url.find('/', host_start);
    
    if (path_start == std::string::npos) {
        host = url.substr(host_start);
        path = "/";
    } else {
        host = url.substr(host_start, path_start - host_start);
        path = url.substr(path_start);
    }

    // 检查是否有端口号
    size_t port_pos = host.find(':');
    if (port_pos != std::string::npos) {
        port = std::stoi(host.substr(port_pos + 1));
        host = host.substr(0, port_pos);
    }

    return true;
}

HttpResponse HttpClient::executeRequest(
    const std::string& method,
    const std::string& url,
    const std::string& body,
    const HttpHeaders& headers
) {
    HttpResponse response;
    response.success = false;

    // 解析 URL
    std::string host, path;
    int port;
    bool is_https;
    
    if (!parseUrl(url, host, path, port, is_https)) {
        response.error_message = "Invalid URL format";
        return response;
    }

    // 转换为宽字符
    std::wstring w_host(host.begin(), host.end());
    std::wstring w_path(path.begin(), path.end());
    std::wstring w_method(method.begin(), method.end());
    std::wstring w_user_agent(m_user_agent.begin(), m_user_agent.end());

    // 初始化 WinHTTP
    HINTERNET hSession = WinHttpOpen(
        w_user_agent.c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        response.error_message = "Failed to initialize WinHTTP";
        return response;
    }

    // 设置超时
    WinHttpSetTimeouts(
        hSession,
        m_connect_timeout_ms,  // DNS 解析超时
        m_connect_timeout_ms,  // 连接超时
        m_read_timeout_ms,     // 发送超时
        m_read_timeout_ms      // 接收超时
    );

    // 连接到服务器
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        w_host.c_str(),
        port,
        0
    );

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        response.error_message = "Failed to connect to server";
        return response;
    }

    // 创建请求
    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        w_method.c_str(),
        w_path.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        response.error_message = "Failed to create request";
        return response;
    }

    // 添加请求头
    for (const auto& header : headers) {
        std::string header_line = header.first + ": " + header.second;
        std::wstring w_header(header_line.begin(), header_line.end());
        WinHttpAddRequestHeaders(
            hRequest,
            w_header.c_str(),
            (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
        );
    }

    // 发送请求
    BOOL result = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
        body.empty() ? 0 : (DWORD)body.length(),
        body.empty() ? 0 : (DWORD)body.length(),
        0
    );

    if (!result) {
        DWORD error = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        std::stringstream ss;
        ss << "Failed to send request, error code: " << error;
        response.error_message = ss.str();
        return response;
    }

    // 接收响应
    result = WinHttpReceiveResponse(hRequest, NULL);
    if (!result) {
        DWORD error = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        std::stringstream ss;
        ss << "Failed to receive response, error code: " << error;
        response.error_message = ss.str();
        return response;
    }

    // 获取状态码
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &size,
        WINHTTP_NO_HEADER_INDEX
    );
    response.status_code = status_code;

    // 读取响应体
    std::string response_body;
    DWORD bytes_available = 0;
    
    do {
        bytes_available = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytes_available)) {
            break;
        }

        if (bytes_available == 0) {
            break;
        }

        std::vector<char> buffer(bytes_available + 1, 0);
        DWORD bytes_read = 0;
        
        if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read)) {
            response_body.append(buffer.data(), bytes_read);
        }
    } while (bytes_available > 0);

    response.body = response_body;
    response.success = true;

    // 清理
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

} // namespace TGAuth
