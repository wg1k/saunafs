#include "common/platform.h"

#include "master/metadumper_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <random>
#include <sstream>

#include "config/cfg.h"
#include "slogger/slogger.h"

namespace safs {
namespace metadumper {

MetadumperClient::MetadumperClient(const std::string &socketPath)
    : socketPath_(socketPath), socket_(-1), connected_(false) {}

MetadumperClient::~MetadumperClient() { disconnect(); }

bool MetadumperClient::isServiceAvailable() {
	safs::log_info("[BALDOR] TRACE: MetadumperClient::isServiceAvailable");
	if (connected_) {
		safs::log_info("[BALDOR] TRACE: MetadumperClient::isServiceAvailable: connected");
		return true;
	}

	bool available = connectToService();
	if (available) {
		safs::log_info("[BALDOR] TRACE: MetadumperClient::isServiceAvailable: connected");
		disconnect();  // Just testing availability
	}
	safs::log_info("[BALDOR] TRACE: MetadumperClient::isServiceAvailable: available = {}", available);
	return available;
}

std::string MetadumperClient::sendDumpRequest(uint64_t checksum, const std::string &changelogFile,
                                              const std::string &outputPath,
                                              const std::string &metadataFile,
                                              int storedMetaCopies) {
	safs::log_info("[BALDOR] TRACE: MetadumperClient::sendDumpRequest");
	if (!connectToService()) { return ""; }

	DumpRequest request;
	request.requestId = generateRequestId();
	request.checksum = checksum;
	request.changelogFile = changelogFile;
	request.outputPath = outputPath;
	request.metadataFile = cfg_get("DATA_PATH", DATA_PATH) + "/" + metadataFile;
	request.storedMetaCopies = storedMetaCopies;

	std::string requestData = request.serialize();

	if (send(socket_, requestData.c_str(), requestData.length(), 0) == -1) {
		safs::log_error_code(errno, "Failed to send dump request");
		disconnect();
		return "";
	}

	return request.requestId;
}

bool MetadumperClient::pollStatus(const std::string &requestId, DumpResponse &response) {
	(void)requestId;  // Currently unused, as we handle one request at a time
	safs::log_info("[BALDOR] TRACE: MetadumperClient::pollStatus");
	if (!connected_) {
		safs::log_info("[BALDOR] TRACE: MetadumperClient::pollStatus: not connected");
		return false;
	}

	char buffer[4096];
	ssize_t bytesRead = recv(socket_, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);

	if (bytesRead == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return false;  // No data available yet
		}
		safs::log_error_code(errno, "Failed to receive dump response");
		disconnect();
		return false;
	}

	if (bytesRead == 0) {
		disconnect();
		return false;
	}

	buffer[bytesRead] = '\0';

	try {
		response = DumpResponse::deserialize(std::string(buffer));
		disconnect();  // Request completed
		return true;
	} catch (const std::exception &e) {
		safs::log_exception(e, "Failed to deserialize dump response");
		disconnect();
		return false;
	}
}

bool MetadumperClient::connectToService() {
	if (connected_) { return true; }

	socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_ == -1) {
		safs::log_error_code(errno, "Failed to create socket");
		return false;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

	if (connect(socket_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		safs::log_error_code(errno, "Failed to connect to metadumper service");
		close(socket_);
		socket_ = -1;
		return false;
	}

	connected_ = true;
	safs::log_info("[BALDOR] TRACE: MetadumperClient::connectToService: connected");
	return true;
}

void MetadumperClient::disconnect() {
	if (socket_ != -1) {
		close(socket_);
		socket_ = -1;
	}
	connected_ = false;
	safs::log_info("[BALDOR] TRACE: MetadumperClient::disconnect");
}

std::string MetadumperClient::generateRequestId() {
	safs::log_info("[BALDOR] TRACE: MetadumperClient::generateRequestId");
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<> dis(0, 15);

	std::ostringstream oss;
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);

	oss << time_t << "_";

	for (int i = 0; i < 8; ++i) { oss << std::hex << dis(gen); }

	return oss.str();
}

}  // namespace metadumper
}  // namespace safs
