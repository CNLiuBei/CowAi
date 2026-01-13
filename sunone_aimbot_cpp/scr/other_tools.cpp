#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <unordered_set>
#include <tchar.h>
#include <thread>
#include <mutex>
#include <atomic>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <VersionHelpers.h>

#include "other_tools.h"
#include "config.h"
#include "sunone_aimbot_cpp.h"

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static inline bool is_base64(unsigned char c)
{
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::vector<unsigned char> Base64Decode(const std::string& encoded_string)
{
    int in_len = static_cast<int>(encoded_string.size());
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(static_cast<unsigned char>(encoded_string[in_])))
    {
        char_array_4[i++] = static_cast<unsigned char>(encoded_string[in_]); in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

            char_array_3[0] = static_cast<unsigned char>((char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4));
            char_array_3[1] = static_cast<unsigned char>(((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));
            char_array_3[2] = static_cast<unsigned char>(((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i)
    {
        int j;
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

        char_array_3[0] = static_cast<unsigned char>((char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4));
        char_array_3[1] = static_cast<unsigned char>(((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));
        char_array_3[2] = static_cast<unsigned char>(((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

        for (j = 0; (j < i - 1); j++)
            ret.push_back(char_array_3[j]);
    }

    return ret;
}

bool fileExists(const std::string& path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string replace_extension(const std::string& filename, const std::string& new_extension)
{
    size_t last_dot = filename.find_last_of(".");
    if (last_dot == std::string::npos)
    {
        return filename + new_extension;
    }
    else
    {
        return filename.substr(0, last_dot) + new_extension;
    }
}

std::string intToString(int value)
{
    return std::to_string(value);
}

std::vector<std::string> getModelFiles()
{
    std::vector<std::string> modelsFiles;

    // Check if models directory exists
    if (!std::filesystem::exists("models"))
    {
        std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0 'models' \xE7\x9B\xAE\xE5\xBD\x95!" << std::endl;
        return modelsFiles;
    }

    for (const auto& entry : std::filesystem::directory_iterator("models/"))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".engine" ||
            entry.is_regular_file() && entry.path().extension() == ".onnx")
        {
            modelsFiles.push_back(entry.path().filename().string());
        }
    }
    return modelsFiles;
}

std::vector<std::string> getEngineFiles()
{
    std::vector<std::string> engineFiles;

    // Check if models directory exists
    if (!std::filesystem::exists("models"))
    {
        return engineFiles;
    }

    for (const auto& entry : std::filesystem::directory_iterator("models/"))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".engine" ||
            entry.is_regular_file() && entry.path().extension() == ".onnx")
        {
            engineFiles.push_back(entry.path().filename().string());
        }
    }
    return engineFiles;
}

std::vector<std::string> getOnnxFiles()
{
    std::vector<std::string> onnxFiles;

    // Check if models directory exists
    if (!std::filesystem::exists("models"))
    {
        return onnxFiles;
    }

    for (const auto& entry : std::filesystem::directory_iterator("models/"))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".onnx")
        {
            onnxFiles.push_back(entry.path().filename().string());
        }
    }
    return onnxFiles;
}

std::vector<std::string>::difference_type getModelIndex(std::vector<std::string> engine_models)
{
    auto it = std::find(engine_models.begin(), engine_models.end(), config.ai_model);

    if (it != engine_models.end())
    {
        return std::distance(engine_models.begin(), it);
    }
    else
    {
        return 0; // not found
    }
}

bool LoadTextureFromFile(const char* filename, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    int image_width = 0;
    int image_height = 0;
    int channels = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, &channels, 4);
    if (image_data == NULL)
        return false;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    device->CreateTexture2D(&desc, &subResource, &pTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    device->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);

    return true;
}

bool LoadTextureFromMemory(const std::string& imageBase64, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    std::vector<unsigned char> decodedData = Base64Decode(imageBase64);

    int image_width = 0;
    int image_height = 0;
    int channels = 0;

    unsigned char* image_data = stbi_load_from_memory(
        decodedData.data(),
        static_cast<int>(decodedData.size()),
        &image_width, &image_height, &channels, 4);

    if (image_data == NULL)
    {
        std::cerr << "Can't load image trom memory." << std::endl;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    device->CreateTexture2D(&desc, &subResource, &pTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    device->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);

    return true;
}

std::string get_ghub_version()
{
    std::string line;
    std::ifstream in("C:\\Program Files\\LGHUB\\version");
    if (in.is_open())
    {
        while (std::getline(in, line)) { }
    }
    in.close();

    if (line.data())
    {
        return line;
    }

	return "unknown";
}

bool contains_tensorrt(const std::string& path)
{
    std::string lowercase_path = path;
    std::transform(lowercase_path.begin(), lowercase_path.end(), lowercase_path.begin(), ::tolower);
    return lowercase_path.find("tensorrt") != std::string::npos;
}

int get_active_monitors()
{
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)))
    {
        return -1;
    }

    int monitorCount = 0;

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        IDXGIOutput* output = nullptr;
        for (UINT j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; ++j)
        {
            monitorCount++;
            output->Release();
        }
        adapter->Release();
    }

    factory->Release();
    return monitorCount;
}

HMONITOR GetMonitorHandleByIndex(int monitorIndex)
{
    struct MonitorSearch
    {
        int targetIndex;
        int currentIndex;
        HMONITOR targetMonitor;
    };

    MonitorSearch search = { monitorIndex, 0, nullptr };

    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL
        {
            MonitorSearch* search = reinterpret_cast<MonitorSearch*>(lParam);
            if (search->currentIndex == search->targetIndex)
            {
                search->targetMonitor = hMonitor;
                return FALSE;
            }
            search->currentIndex++;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));

    return search.targetMonitor;
}

std::vector<std::string> getAvailableModels(bool verbose)
{
    std::vector<std::string> availableModels;
    
    if (verbose)
    {
        std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE6\xAD\xA3\xE5\x9C\xA8\xE6\x89\xAB\xE6\x8F\x8F models \xE7\x9B\xAE\xE5\xBD\x95..." << std::endl;
        std::cout.flush();
    }
    
    // Check if models directory exists
    if (!std::filesystem::exists("models"))
    {
        if (verbose)
        {
            std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE9\x94\x99\xE8\xAF\xAF: \xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0 'models' \xE7\x9B\xAE\xE5\xBD\x95!" << std::endl;
            std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE8\xAF\xB7\xE5\x88\x9B\xE5\xBB\xBA 'models' \xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\xE5\xB9\xB6\xE6\x94\xBE\xE5\x85\xA5 .engine \xE6\x88\x96 .onnx \xE6\x96\x87\xE4\xBB\xB6" << std::endl;
        }
        return availableModels;
    }
    
    // Check if models directory is actually a directory
    if (!std::filesystem::is_directory("models"))
    {
        if (verbose)
        {
            std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE9\x94\x99\xE8\xAF\xAF: 'models' \xE4\xB8\x8D\xE6\x98\xAF\xE4\xB8\x80\xE4\xB8\xAA\xE7\x9B\xAE\xE5\xBD\x95!" << std::endl;
        }
        return availableModels;
    }
    
    std::vector<std::string> engineFiles = getEngineFiles();
    std::vector<std::string> onnxFiles = getOnnxFiles();

    std::set<std::string> engineModels;
    for (const auto& file : engineFiles)
    {
        engineModels.insert(std::filesystem::path(file).stem().string());
    }

    // Add engine files first (preferred)
    for (const auto& file : engineFiles)
    {
        availableModels.push_back(file);
    }

    // Add onnx files that don't have corresponding engine
    for (const auto& file : onnxFiles)
    {
        std::string modelName = std::filesystem::path(file).stem().string();
        if (engineModels.find(modelName) == engineModels.end())
        {
            availableModels.push_back(file);
        }
    }
    
    // Print scan results
    if (verbose)
    {
        if (availableModels.empty())
        {
            std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE8\xAD\xA6\xE5\x91\x8A: 'models' \xE7\x9B\xAE\xE5\xBD\x95\xE4\xB8\xAD\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE6\xA8\xA1\xE5\x9E\x8B\xE6\x96\x87\xE4\xBB\xB6!" << std::endl;
            std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE6\x94\xAF\xE6\x8C\x81\xE7\x9A\x84\xE6\xA0\xBC\xE5\xBC\x8F: .engine, .onnx" << std::endl;
        }
        else
        {
            std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE6\x89\xBE\xE5\x88\xB0 " << availableModels.size() << " \xE4\xB8\xAA\xE6\xA8\xA1\xE5\x9E\x8B:" << std::endl;
            for (size_t i = 0; i < availableModels.size(); i++)
            {
                std::string modelPath = "models/" + availableModels[i];
                uintmax_t fileSize = 0;
                try {
                    fileSize = std::filesystem::file_size(modelPath);
                } catch (...) {}
                
                std::cout << "  [" << (i + 1) << "] " << availableModels[i];
                if (fileSize > 0)
                {
                    if (fileSize >= 1024 * 1024)
                        std::cout << " (" << (fileSize / (1024 * 1024)) << " MB)";
                    else
                        std::cout << " (" << (fileSize / 1024) << " KB)";
                }
                std::cout << std::endl;
            }
            std::cout.flush();
        }
    }

    return availableModels;
}

bool checkwin1903()
{
    return IsWindows10OrGreater() && IsWindowsVersionOrGreater(10, 0, 18362);
}

void welcome_message()
{
    std::string targeting = config.joinStrings(config.button_targeting);
    std::string exitKey = config.joinStrings(config.button_exit);
    std::string pauseKey = config.joinStrings(config.button_pause);
    std::string reloadKey = config.joinStrings(config.button_reload_config);
    std::string overlayKey = config.joinStrings(config.button_open_overlay);
    
    std::cout << "\n\n=== " << "\xE8\x87\xAA\xE7\x9E\x84\xE5\xB7\xB2\xE5\x90\xAF\xE5\x8A\xA8" << " ===" << std::endl;
    
    if (!targeting.empty() && targeting != "None") {
        std::cout << targeting << " -> " << "\xE7\x9E\x84\xE5\x87\x86" << std::endl;
    }
    
    if (!exitKey.empty() && exitKey != "None") {
        std::cout << exitKey << " -> " << "\xE9\x80\x80\xE5\x87\xBA" << std::endl;
    }
    
    if (!pauseKey.empty() && pauseKey != "None") {
        std::cout << pauseKey << " -> " << "\xE6\x9A\x82\xE5\x81\x9C" << std::endl;
    }
    
    if (!reloadKey.empty() && reloadKey != "None") {
        std::cout << reloadKey << " -> " << "\xE9\x87\x8D\xE8\xBD\xBD\xE9\x85\x8D\xE7\xBD\xAE" << std::endl;
    }
    
    if (!overlayKey.empty() && overlayKey != "None") {
        std::cout << overlayKey << " -> " << "\xE6\x8E\xA7\xE5\x88\xB6\xE9\x9D\xA2\xE6\x9D\xBF" << std::endl;
    }
    
    std::cout.flush();
}