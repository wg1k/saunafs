#ifndef SAUNAFS_METADUMPER_CONFIG_H
#define SAUNAFS_METADUMPER_CONFIG_H

#include "common/platform.h"

#include <string>

namespace saunafs {
namespace metadumper {

class MetadumperConfig {
public:
    static MetadumperConfig& getInstance();
    
    void loadFromEnvironment();
    void loadFromFile(const std::string& configFile);
    
    const std::string& getServiceSocketPath() const { return serviceSocketPath_; }
    bool isServiceEnabled() const { return serviceEnabled_; }
    bool isFallbackEnabled() const { return fallbackEnabled_; }
    bool useServiceArchitecture() const { return useServiceArchitecture_; }
    int getServiceTimeoutMs() const { return serviceTimeoutMs_; }
    int getMaxRetries() const { return maxRetries_; }
    
    void setServiceSocketPath(const std::string& path) { serviceSocketPath_ = path; }
    void setServiceEnabled(bool enabled) { serviceEnabled_ = enabled; }
    void setFallbackEnabled(bool enabled) { fallbackEnabled_ = enabled; }
    void setUseServiceArchitecture(bool use) { useServiceArchitecture_ = use; }
    void setServiceTimeoutMs(int timeout) { serviceTimeoutMs_ = timeout; }
    void setMaxRetries(int retries) { maxRetries_ = retries; }

private:
    MetadumperConfig();
    ~MetadumperConfig() = default;
    MetadumperConfig(const MetadumperConfig&) = delete;
    MetadumperConfig& operator=(const MetadumperConfig&) = delete;
    
    std::string serviceSocketPath_;
    bool serviceEnabled_;
    bool fallbackEnabled_;
    bool useServiceArchitecture_;
    int serviceTimeoutMs_;
    int maxRetries_;
};

} // namespace metadumper
} // namespace saunafs

#endif // SAUNAFS_METADUMPER_CONFIG_H

