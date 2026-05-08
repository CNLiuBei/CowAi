#pragma once

#include <string>
#include <map>

// Configuration mapping utilities for DLL interface
namespace ConfigMapping {
    // Map configuration keys to values
    std::map<std::string, std::string> getConfigMap();

    // Set configuration from map
    void setConfigFromMap(const std::map<std::string, std::string>& configMap);

    // Get single config value
    std::string getConfigValue(const std::string& key);

    // Set single config value
    void setConfigValue(const std::string& key, const std::string& value);
}
