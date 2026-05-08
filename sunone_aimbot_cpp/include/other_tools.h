#ifndef OTHER_TOOLS_H
#define OTHER_TOOLS_H

#include <string>
#include <filesystem>
#include <d3d11.h>

// UTF-8 string to std::filesystem::path (correct for Chinese filenames)
inline std::filesystem::path utf8_to_path(const std::string& utf8str)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1, nullptr, 0);
    std::wstring wstr(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1, &wstr[0], wlen);
    return std::filesystem::path(wstr);
}

static inline bool is_base64(unsigned char c);
std::vector<unsigned char> Base64Decode(const std::string& encoded_string);
bool fileExists(const std::string& path);
std::string replace_extension(const std::string& filename, const std::string& new_extension);

std::vector<std::string> getEngineFiles();
std::vector<std::string> getModelFiles();
std::vector<std::string> getOnnxFiles();
std::vector<std::string>::difference_type getModelIndex(std::vector<std::string> engine_models);

std::string intToString(int value);
bool LoadTextureFromFile(const char* filename, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
bool LoadTextureFromMemory(const std::string& imageBase64, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);

std::string get_ghub_version();
int get_active_monitors();
HMONITOR GetMonitorHandleByIndex(int monitorIndex);

std::vector<std::string> getAvailableModels(bool verbose = true);

void welcome_message();
bool checkwin1903();
#endif // OTHER_TOOLS_H