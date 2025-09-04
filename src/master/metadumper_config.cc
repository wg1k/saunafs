#include "common/platform.h"

#include "master/metadumper_config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "slogger/slogger.h"
#include "config/cfg.h"

namespace safs {
namespace metadumper {

MetadumperConfig::MetadumperConfig()
    : serviceSocketPath_("/var/run/saunafs/metadumper.sock"),
      serviceEnabled_(true),
      fallbackEnabled_(true),
      useServiceArchitecture_(false),  // Default to false for backward compatibility
      serviceTimeoutMs_(30000),
      maxRetries_(3) {}

MetadumperConfig &MetadumperConfig::getInstance() {
	static MetadumperConfig instance;
	return instance;
}

void MetadumperConfig::load() {
	safs::log_info("[BALDOR] TRACE: MetadumperConfig::load");
	auto socketPath = cfg_get("METADUMPER_SERVICE_SOCKET", "/var/run/saunafs/metadumper.sock");
	if (socketPath.size() > 0) {
		serviceSocketPath_ = socketPath;
		safs::log_info("Using metadumper service socket from config: {}", socketPath);
	}

	auto serviceEnabled = cfg_get("METADUMPER_SERVICE_ENABLED", "true");
	if (serviceEnabled.size() > 0) {
		serviceEnabled_ =
		    (std::string(serviceEnabled) == "1" || std::string(serviceEnabled) == "true");
		safs::log_info("Metadumper service enabled from config: {}", serviceEnabled_ ? "true" : "false");
	}

	auto fallbackEnabled = cfg_get("METADUMPER_FALLBACK_ENABLED", "true");
	if (fallbackEnabled.size() > 0) {
		fallbackEnabled_ =
		    (std::string(fallbackEnabled) == "1" || std::string(fallbackEnabled) == "true");
		safs::log_info("Metadumper fallback enabled from config: {}", fallbackEnabled_ ? "true" : "false");
	}

	auto useServiceArch = cfg_get("METADUMPER_USE_SERVICE_ARCHITECTURE", "false");
	if (useServiceArch.size() > 0) {
		useServiceArchitecture_ =
		    (std::string(useServiceArch) == "1" || std::string(useServiceArch) == "true");
		safs::log_info("Metadumper service architecture from config: {}", useServiceArchitecture_ ? "enabled" : "disabled");
	}

	auto timeout = cfg_get("METADUMPER_SERVICE_TIMEOUT_MS", "30000");
	if (timeout.size() > 0) {
		serviceTimeoutMs_ = std::atoi(timeout.c_str());
		safs::log_info("Metadumper service timeout from config: {} ms", serviceTimeoutMs_);
	}

	auto retries = cfg_get("METADUMPER_MAX_RETRIES", "3");
	if (retries.size() > 0) {
		maxRetries_ = std::atoi(retries.c_str());
		safs::log_info("Metadumper max retries from config: {}", maxRetries_);
	}
}

}  // namespace metadumper
}  // namespace safs
