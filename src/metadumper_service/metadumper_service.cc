#include "metadumper_service/metadumper_service.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <mutex>

#include "common/cwrap.h"
#include "common/rotate_files.h"
#include "common/setup.h"
#include "master/changelog.h"
#include "master/chunks.h"
#include "master/hstring_memstorage.h"
#include "master/hstring_storage.h"
#include "master/metadata_backend_common.h"
#include "master/metadata_backend_file.h"
#include "master/metadata_backend_interface.h"
#include "master/restore.h"
#include "metarestore/merger.h"
#include "slogger/slogger.h"

#include "master/filesystem.h"

namespace saunafs {
namespace metadumper {

MetadumperService::MetadumperService(const std::string &socketPath)
    : socketPath_(socketPath), serverSocket_(-1), running_(false) {}

MetadumperService::~MetadumperService() { stop(); }

bool MetadumperService::start() {
	if (running_.load()) { return false; }

	serverSocket_ = socket(AF_UNIX, SOCK_STREAM, 0);
	if (serverSocket_ == -1) {
		safs_pretty_errlog(LOG_ERR, "socket creation failed");
		return false;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

	// Remove existing socket file
	unlink(socketPath_.c_str());

	if (bind(serverSocket_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		safs_pretty_errlog(LOG_ERR, "socket bind failed");
		close(serverSocket_);
		return false;
	}

	if (listen(serverSocket_, 5) == -1) {
		safs_pretty_errlog(LOG_ERR, "socket listen failed");
		close(serverSocket_);
		return false;
	}

	running_.store(true);
	serverThread_ = std::thread(&MetadumperService::serverLoop, this);

	safs_pretty_syslog(LOG_INFO, "Metadumper service started on socket: %s", socketPath_.c_str());
	return true;
}

void MetadumperService::stop() {
	if (!running_.load()) { return; }

	running_.store(false);

	if (serverSocket_ != -1) {
		close(serverSocket_);
		serverSocket_ = -1;
	}

	if (serverThread_.joinable()) { serverThread_.join(); }

	unlink(socketPath_.c_str());
	safs_pretty_syslog(LOG_INFO, "Metadumper service stopped");
}

bool MetadumperService::isRunning() const { return running_.load(); }

void MetadumperService::serverLoop() {
	while (running_.load()) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(serverSocket_, &readfds);

		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int result = select(serverSocket_ + 1, &readfds, nullptr, nullptr, &timeout);
		if (result == -1) {
			if (errno != EINTR) { safs_pretty_errlog(LOG_ERR, "select failed in server loop"); }
			continue;
		}

		if (result > 0 && FD_ISSET(serverSocket_, &readfds)) {
			int clientSocket = accept(serverSocket_, nullptr, nullptr);
			if (clientSocket != -1) {
				std::thread clientThread(&MetadumperService::handleClient, this, clientSocket);
				clientThread.detach();
			}
		}

		cleanupOldFiles();
	}
}

void MetadumperService::handleClient(int clientSocket) {
	char buffer[4096];
	ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

	if (bytesRead <= 0) {
		close(clientSocket);
		return;
	}

	buffer[bytesRead] = '\0';

	try {
		DumpRequest request = DumpRequest::deserialize(std::string(buffer));
		DumpResponse response;

		bool success = processDumpRequest(request, response);
		response.requestId = request.requestId;
		response.success = success;

		std::string responseData = response.serialize();
		send(clientSocket, responseData.c_str(), responseData.length(), 0);
	} catch (const std::exception &e) {
		DumpResponse errorResponse;
		errorResponse.success = false;
		errorResponse.errorMessage = e.what();
		std::string responseData = errorResponse.serialize();
		send(clientSocket, responseData.c_str(), responseData.length(), 0);
	}

	close(clientSocket);
}

bool MetadumperService::processDumpRequest(const DumpRequest &request, DumpResponse &response) {
	try {
		// Initialize the filesystem backend
		hstorage::Storage::reset(new hstorage::MemStorage());
		if (!gMetadataBackend) { gMetadataBackend = std::make_unique<MetadataBackendFile>(); }

		// Initialize filesystem
		if (fs_init(request.metadataFile.c_str(), 1, true) != 0) {
			response.errorMessage =
			    "Failed to initialize filesystem from metadata file: " + request.metadataFile;
			safs_pretty_syslog(LOG_ERR, "Failed to initialize filesystem from: %s",
			                   request.metadataFile.c_str());
			return false;
		}

		if (fs_getversion() == 0) {
			response.errorMessage = "Invalid metadata version (0)";
			safs_pretty_syslog(LOG_ERR, "Invalid metadata version (0)");
			return false;
		}

		// Process changelog files
		std::vector<std::string> filenames;
		if (!request.changelogFile.empty() && access(request.changelogFile.c_str(), F_OK) == 0) {
			filenames.push_back(request.changelogFile);
		} else if (!request.changelogFile.empty()) {
			safs_pretty_syslog(LOG_WARNING, "Changelog file not accessible: %s",
			                   request.changelogFile.c_str());
		}

		merger_start(filenames, 10000);
		uint8_t status = merger_loop();

		if (status != SAUNAFS_STATUS_OK) {
			response.errorMessage = "Merge operation failed with status: " + std::to_string(status);
			safs_pretty_syslog(LOG_ERR, "Merge operation failed with status: %d", status);
			return false;
		}

		// Verify checksum
		uint64_t calculatedChecksum = fs_checksum(ChecksumMode::kForceRecalculate);
		if (calculatedChecksum != request.checksum) {
			response.errorMessage = "Checksum mismatch: expected " +
			                        std::to_string(request.checksum) + ", calculated " +
			                        std::to_string(calculatedChecksum);
			safs_pretty_syslog(LOG_ERR,
			                   "Checksum mismatch: expected %" PRIu64 ", calculated %" PRIu64,
			                   request.checksum, calculatedChecksum);
			return false;
		}

		// Generate timestamped output filename
		auto now = std::chrono::system_clock::now();
		auto time_t = std::chrono::system_clock::to_time_t(now);
		struct tm *tm_info = localtime(&time_t);

		char timestamp[32];
		strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

		std::string outputFile = request.outputPath + "/metadata.mfs." + timestamp;

		// Create backup copies if needed
		if (request.storedMetaCopies > 0) {
			try {
				rotateFiles(outputFile, request.storedMetaCopies);
			} catch (const std::exception &e) {
				safs_pretty_syslog(LOG_WARNING, "Failed to rotate backup files: %s", e.what());
			}
		}

		// Save metadata
		try {
			fs_term(outputFile.c_str(), true);
		} catch (const std::exception &e) {
			response.errorMessage = "Failed to save metadata file: " + std::string(e.what());
			safs_pretty_syslog(LOG_ERR, "Failed to save metadata file: %s", e.what());
			return false;
		}

		// If the request specifies a different output path, transfer the file
		if (request.outputPath != outputFile.substr(0, outputFile.find_last_of("/"))) {
			std::string finalOutputFile =
			    request.outputPath + "/" + outputFile.substr(outputFile.find_last_of("/") + 1);
			if (!FileTransfer::atomicFileReplace(outputFile, finalOutputFile)) {
				response.errorMessage = "Failed to transfer output file to requested location";
				safs_pretty_syslog(LOG_ERR, "Failed to transfer output file to: %s",
				                   finalOutputFile.c_str());
				return false;
			}
			response.outputFile = finalOutputFile;
		} else {
			response.outputFile = outputFile;
		}

		safs_pretty_syslog(LOG_INFO, "Successfully dumped metadata to: %s",
		                   response.outputFile.c_str());
		return true;

	} catch (const std::exception &e) {
		response.errorMessage = std::string("Exception during dump: ") + e.what();
		safs_pretty_syslog(LOG_ERR, "Exception during dump: %s", e.what());
		return false;
	} catch (...) {
		response.errorMessage = "Unknown exception during dump";
		safs_pretty_syslog(LOG_ERR, "Unknown exception during dump");
		return false;
	}
}

void MetadumperService::cleanupOldFiles() {
	// Implement cleanup logic for old dump files if needed
	// This could be based on age or number of files to keep
}

}  // namespace metadumper
}  // namespace saunafs
