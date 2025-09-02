#include "master/metadumper_config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "slogger/slogger.h"

namespace saunafs {
namespace metadumper {

MetadumperConfig::MetadumperConfig()
    : serviceSocketPath_("/var/run/saunafs/metadumper.sock"),
      serviceEnabled_(true),
      fallbackEnabled_(true),
      useServiceArchitecture_(false),  // Default to false for backward compatibility
      serviceTimeoutMs_(30000),
      maxRetries_(3) {
}

MetadumperConfig& MetadumperConfig::getInstance() {
    static MetadumperConfig instance;
    return instance;
}

void MetadumperConfig::loadFromEnvironment() {
    const char* socketPath = getenv("METADUMPER_SERVICE_SOCKET");
    if (socketPath) {
        serviceSocketPath_ = socketPath;
        safs_pretty_syslog(LOG_INFO, "Using metadumper service socket from environment: %s", socketPath);
    }
    
    const char* serviceEnabled = getenv("METADUMPER_SERVICE_ENABLED");
    if (serviceEnabled) {
        serviceEnabled_ = (std::string(serviceEnabled) == "1" || std::string(serviceEnabled) == "true");
        safs_pretty_syslog(LOG_INFO, "Metadumper service enabled from environment: %s", serviceEnabled_ ? "true" : "false");
    }
    
    const char* fallbackEnabled = getenv("METADUMPER_FALLBACK_ENABLED");
    if (fallbackEnabled) {
        fallbackEnabled_ = (std::string(fallbackEnabled) == "1" || std::string(fallbackEnabled) == "true");
        safs_pretty_syslog(LOG_INFO, "Metadumper fallback enabled from environment: %s", fallbackEnabled_ ? "true" : "false");
    }
    
    const char* useServiceArch = getenv("METADUMPER_USE_SERVICE_ARCHITECTURE");
    if (useServiceArch) {
        useServiceArchitecture_ = (std::string(useServiceArch) == "1" || std::string(useServiceArch) == "true");
        safs_pretty_syslog(LOG_INFO, "Metadumper service architecture from environment: %s", useServiceArchitecture_ ? "enabled" : "disabled");
    }
    
    const char* timeout = getenv("METADUMPER_SERVICE_TIMEOUT_MS");
    if (timeout) {
        serviceTimeoutMs_ = std::atoi(timeout);
        safs_pretty_syslog(LOG_INFO, "Metadumper service timeout from environment: %d ms", serviceTimeoutMs_);
    }
    
    const char* retries = getenv("METADUMPER_MAX_RETRIES");
    if (retries) {
        maxRetries_ = std::atoi(retries);
        safs_pretty_syslog(LOG_INFO, "Metadumper max retries from environment: %d", maxRetries_);
    }
}

void MetadumperConfig::loadFromFile(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        safs_pretty_syslog(LOG_WARNING, "Could not open metadumper config file: %s", configFile.c_str());
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (key == "METADUMPER_SERVICE_SOCKET") {
                serviceSocketPath_ = value;
            } else if (key == "METADUMPER_SERVICE_ENABLED") {
                serviceEnabled_ = (value == "1" || value == "true");
            } else if (key == "METADUMPER_FALLBACK_ENABLED") {
                fallbackEnabled_ = (value == "1" || value == "true");
            } else if (key == "METADUMPER_USE_SERVICE_ARCHITECTURE") {
                useServiceArchitecture_ = (value == "1" || value == "true");
            } else if (key == "METADUMPER_SERVICE_TIMEOUT_MS") {
                serviceTimeoutMs_ = std::atoi(value.c_str());
            } else if (key == "METADUMPER_MAX_RETRIES") {
                maxRetries_ = std::atoi(value.c_str());
            }
        }
    }
    
    safs_pretty_syslog(LOG_INFO, "Loaded metadumper configuration from file: %s", configFile.c_str());
}

} // namespace metadumper
} // namespace saunafs

