#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <Windows.h>
#include <shellapi.h>

#include <string>
#include <fstream>
#include <curl/curl.h>

#include "version.h"

// GitHub Releases API
static const char* GITHUB_API_URL = "https://api.github.com/repos/CNLiuBei/CowAi/releases/latest";

namespace Version {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// 简单的版本比较 (格式: x.y.z 或 vx.y.z)
static bool IsNewerVersion(const std::string& remote, const std::string& local) {
    std::string r = remote, l = local;
    // 去掉开头的 v
    if (!r.empty() && r[0] == 'v') r = r.substr(1);
    if (!l.empty() && l[0] == 'v') l = l.substr(1);
    
    int r1 = 0, r2 = 0, r3 = 0;
    int l1 = 0, l2 = 0, l3 = 0;
    sscanf(r.c_str(), "%d.%d.%d", &r1, &r2, &r3);
    sscanf(l.c_str(), "%d.%d.%d", &l1, &l2, &l3);
    
    if (r1 > l1) return true;
    if (r1 == l1 && r2 > l2) return true;
    if (r1 == l1 && r2 == l2 && r3 > l3) return true;
    return false;
}

// 简单解析JSON字符串值
static std::string ParseJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    
    pos = json.find("\"", pos + 1);
    if (pos == std::string::npos) return "";
    
    size_t start = pos + 1;
    size_t end = start;
    while (end < json.size()) {
        if (json[end] == '"' && json[end - 1] != '\\') break;
        end++;
    }
    
    std::string value = json.substr(start, end - start);
    // 处理转义的换行符
    size_t p;
    while ((p = value.find("\\n")) != std::string::npos) {
        value.replace(p, 2, "\n");
    }
    return value;
}

// 从 GitHub API 响应中解析 exe 下载链接
static std::string ParseExeDownloadUrl(const std::string& json) {
    // 查找 assets 数组中的 .exe 文件
    size_t assetsPos = json.find("\"assets\"");
    if (assetsPos == std::string::npos) return "";
    
    // 查找 browser_download_url 包含 .exe 的
    size_t pos = assetsPos;
    while (pos < json.size()) {
        size_t urlPos = json.find("\"browser_download_url\"", pos);
        if (urlPos == std::string::npos) break;
        
        size_t start = json.find("\"", urlPos + 22) + 1;
        size_t end = json.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string url = json.substr(start, end - start);
            // 检查是否是 .exe 文件
            if (url.find(".exe") != std::string::npos) {
                return url;
            }
        }
        pos = urlPos + 1;
    }
    return "";
}

// 从 URL 中提取文件名
static std::string GetFileNameFromUrl(const std::string& url) {
    size_t pos = url.rfind('/');
    if (pos != std::string::npos) {
        return url.substr(pos + 1);
    }
    return "CowAi_update.exe";
}

// 获取当前 exe 所在目录
static std::string GetExeDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string fullPath(path);
    size_t pos = fullPath.rfind('\\');
    if (pos != std::string::npos) {
        return fullPath.substr(0, pos + 1);
    }
    return "";
}

// 使用 PowerShell 下载文件（更可靠）
static bool DownloadFile(const std::string& url, const std::string& savePath) {
    // 构建 PowerShell 命令
    // 使用 Invoke-WebRequest 下载，-UseBasicParsing 避免IE引擎问题
    std::string psCmd = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"";
    psCmd += "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; ";
    psCmd += "try { ";
    psCmd += "Invoke-WebRequest -Uri '" + url + "' -OutFile '" + savePath + "' -UseBasicParsing; ";
    psCmd += "exit 0 ";
    psCmd += "} catch { exit 1 }\"";
    
    // 创建进程
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // 隐藏窗口
    PROCESS_INFORMATION pi;
    
    if (!CreateProcessA(NULL, (LPSTR)psCmd.c_str(), NULL, NULL, FALSE, 
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    // 等待下载完成（最多10分钟）
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 600000);
    
    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode != 0) {
        DeleteFileA(savePath.c_str());
        return false;
    }
    
    // 验证文件存在且大小合理（至少1MB）
    HANDLE hFile = CreateFileA(savePath.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    CloseHandle(hFile);
    
    if (fileSize.QuadPart < 1024 * 1024) {
        DeleteFileA(savePath.c_str());
        return false;
    }
    
    return true;
}

bool CheckForUpdate(std::string& newVersion, std::string& downloadUrl, std::string& changelog) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    std::string response;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CowAI");
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    
    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    
    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || http_code != 200) {
        return false;
    }
    
    // 解析 GitHub API 响应
    newVersion = ParseJsonString(response, "tag_name");
    downloadUrl = ParseExeDownloadUrl(response);  // 获取 exe 下载链接
    changelog = ParseJsonString(response, "body");
    
    if (newVersion.empty()) {
        return false;
    }
    
    // 比较版本
    return IsNewerVersion(newVersion, APP_VERSION);
}

void ShowUpdateDialog(const std::string& newVersion, const std::string& downloadUrl, const std::string& changelog) {
    // 标题: "CowAI 发现新版本"
    std::wstring title = L"CowAI \x53D1\x73B0\x65B0\x7248\x672C";
    
    // 构建消息
    std::string msg = "\xE6\xA3\x80\xE6\xB5\x8B\xE5\x88\xB0\xE6\x96\xB0\xE7\x89\x88\xE6\x9C\xAC: " + newVersion + "\n";
    msg += "\xE5\xBD\x93\xE5\x89\x8D\xE7\x89\x88\xE6\x9C\xAC: v" + std::string(APP_VERSION) + "\n\n";
    
    if (!changelog.empty() && changelog.length() < 300) {
        msg += "\xE6\x9B\xB4\xE6\x96\xB0\xE5\x86\x85\xE5\xAE\xB9:\n" + changelog + "\n\n";
    }
    
    if (downloadUrl.empty()) {
        // 没有找到 exe 下载链接，提示手动下载
        msg += "\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE8\x87\xAA\xE5\x8A\xA8\xE6\x9B\xB4\xE6\x96\xB0\xE6\x96\x87\xE4\xBB\xB6\xEF\xBC\x8C\xE8\xAF\xB7\xE6\x89\x8B\xE5\x8A\xA8\xE4\xB8\x8B\xE8\xBD\xBD\n";
        msg += "\xE7\x82\xB9\xE5\x87\xBB\xE2\x80\x9C\xE7\xA1\xAE\xE5\xAE\x9A\xE2\x80\x9D\xE5\x89\x8D\xE5\xBE\x80\xE4\xB8\x8B\xE8\xBD\xBD\xE9\xA1\xB5\xE9\x9D\xA2";
        
        int len = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, nullptr, 0);
        std::wstring wmsg(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, &wmsg[0], len);
        
        MessageBoxW(NULL, wmsg.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
        
        // 打开 GitHub Releases 页面
        std::string releasesUrl = "https://github.com/CNLiuBei/CowAi/releases/latest";
        ShellExecuteA(NULL, "open", releasesUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return;
    }
    
    // 询问是否自动下载
    msg += "\xE6\x98\xAF\xE5\x90\xA6\xE8\x87\xAA\xE5\x8A\xA8\xE4\xB8\x8B\xE8\xBD\xBD\xE6\x96\xB0\xE7\x89\x88\xE6\x9C\xAC\xEF\xBC\x9F\n";
    msg += "\xE7\x82\xB9\xE5\x87\xBB\xE2\x80\x9C\xE6\x98\xAF\xE2\x80\x9D\xE8\x87\xAA\xE5\x8A\xA8\xE4\xB8\x8B\xE8\xBD\xBD\xEF\xBC\x8C\xE7\x82\xB9\xE5\x87\xBB\xE2\x80\x9C\xE5\x90\xA6\xE2\x80\x9D\xE6\x89\x8B\xE5\x8A\xA8\xE4\xB8\x8B\xE8\xBD\xBD";
    
    int len = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, nullptr, 0);
    std::wstring wmsg(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, &wmsg[0], len);
    
    int result = MessageBoxW(NULL, wmsg.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION);
    
    if (result == IDYES) {
        // 自动下载
        std::string fileName = GetFileNameFromUrl(downloadUrl);
        std::string savePath = GetExeDirectory() + fileName;
        
        // 显示下载中提示
        std::wstring downloadingTitle = L"CowAI \x6B63\x5728\x4E0B\x8F7D...";
        std::wstring downloadingMsg = L"\x6B63\x5728\x4E0B\x8F7D\x65B0\x7248\x672C\xFF0C\x8BF7\x7A0D\x5019...\n\x4E0B\x8F7D\x5B8C\x6210\x540E\x4F1A\x81EA\x52A8\x63D0\x793A";
        
        // 创建一个简单的进度窗口（这里简化处理，直接下载）
        HWND hWnd = CreateWindowExW(0, L"STATIC", downloadingMsg.c_str(),
            WS_POPUP | WS_VISIBLE | SS_CENTER,
            (GetSystemMetrics(SM_CXSCREEN) - 300) / 2,
            (GetSystemMetrics(SM_CYSCREEN) - 100) / 2,
            300, 100, NULL, NULL, NULL, NULL);
        UpdateWindow(hWnd);
        
        bool success = DownloadFile(downloadUrl, savePath);
        
        DestroyWindow(hWnd);
        
        if (success) {
            // 下载成功
            std::string successMsg = "\xE4\xB8\x8B\xE8\xBD\xBD\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x81\n\n";
            successMsg += "\xE6\x96\x87\xE4\xBB\xB6\xE5\xB7\xB2\xE4\xBF\x9D\xE5\xAD\x98\xE5\x88\xB0: " + savePath + "\n\n";
            successMsg += "\xE8\xAF\xB7\xE5\x85\xB3\xE9\x97\xAD\xE5\xBD\x93\xE5\x89\x8D\xE7\xA8\x8B\xE5\xBA\x8F\xEF\xBC\x8C\xE8\xBF\x90\xE8\xA1\x8C\xE6\x96\xB0\xE7\x89\x88\xE6\x9C\xAC";
            
            len = MultiByteToWideChar(CP_UTF8, 0, successMsg.c_str(), -1, nullptr, 0);
            std::wstring wSuccessMsg(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, successMsg.c_str(), -1, &wSuccessMsg[0], len);
            
            std::wstring successTitle = L"CowAI \x4E0B\x8F7D\x5B8C\x6210";
            MessageBoxW(NULL, wSuccessMsg.c_str(), successTitle.c_str(), MB_OK | MB_ICONINFORMATION);
            
            // 打开文件所在目录并选中文件
            std::string explorerCmd = "/select,\"" + savePath + "\"";
            ShellExecuteA(NULL, "open", "explorer.exe", explorerCmd.c_str(), NULL, SW_SHOWNORMAL);
        } else {
            // 下载失败
            std::wstring failTitle = L"CowAI \x4E0B\x8F7D\x5931\x8D25";
            std::wstring failMsg = L"\x4E0B\x8F7D\x5931\x8D25\xFF0C\x8BF7\x624B\x52A8\x4E0B\x8F7D\n\x70B9\x51FB\x201C\x786E\x5B9A\x201D\x524D\x5F80\x4E0B\x8F7D\x9875\x9762";
            MessageBoxW(NULL, failMsg.c_str(), failTitle.c_str(), MB_OK | MB_ICONERROR);
            
            // 打开 GitHub Releases 页面
            std::string releasesUrl = "https://github.com/CNLiuBei/CowAi/releases/latest";
            ShellExecuteA(NULL, "open", releasesUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
    } else {
        // 手动下载 - 打开 GitHub Releases 页面
        std::string releasesUrl = "https://github.com/CNLiuBei/CowAi/releases/latest";
        ShellExecuteA(NULL, "open", releasesUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

} // namespace Version
