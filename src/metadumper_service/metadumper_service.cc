#include "metadumper_service/metadumper_service.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include "common/cwrap.h"
#include "common/rotate_files.h"
#include "slogger/slogger.h"

namespace safs {
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
	serviceThread_ = std::thread(&MetadumperService::run, this);

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

	if (serviceThread_.joinable()) { serviceThread_.join(); }

	unlink(socketPath_.c_str());
	safs_pretty_syslog(LOG_INFO, "Metadumper service stopped");
}

bool MetadumperService::isRunning() const { return running_.load(); }

void MetadumperService::run() {
    while (running_.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int result = select(serverSocket_ + 1, &readfds, nullptr, nullptr, &timeout);
        if (result == -1) {
            if (errno != EINTR) {
                safs_pretty_errlog(LOG_ERR, "select failed in server loop");
            }
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

		// Call sfsmetarestore tool to perform the actual dump
		std::string checksumStringified = std::to_string(request.checksum);
		std::string storedMetaCopies = std::to_string(request.storedMetaCopies);

		// Find sfsmetarestore binary path
		std::string metarestorePath = "/usr/sbin/sfsmetarestore";
		if (access(metarestorePath.c_str(), X_OK) != 0) {
			// Try alternative paths
			metarestorePath = "/usr/local/sbin/sfsmetarestore";
			if (access(metarestorePath.c_str(), X_OK) != 0) {
				metarestorePath = "sfsmetarestore";  // Hope it's in PATH
			}
		}

		// Prepare arguments for sfsmetarestore
		std::vector<std::string> args = {metarestorePath, "-m", request.metadataFile, "-o",
		                                 outputFile,      "-k", checksumStringified,  "-B",
		                                 storedMetaCopies};

		// Add changelog file if provided
		if (!request.changelogFile.empty() && access(request.changelogFile.c_str(), F_OK) == 0) {
			args.push_back("-#");
			args.push_back(request.changelogFile);
		}

		// Convert to char* array for execv
		std::vector<char *> argv;
		for (const auto &arg : args) { argv.push_back(const_cast<char *>(arg.c_str())); }
		argv.push_back(nullptr);

		safs_pretty_syslog(LOG_INFO, "Executing sfsmetarestore for dump request %s",
		                   request.requestId.c_str());

		// Create pipe for communication with child process
		int pipeFd[2];
		if (pipe(pipeFd) != 0) {
			response.errorMessage = "Failed to create pipe for sfsmetarestore communication";
			safs_pretty_syslog(LOG_ERR, "Failed to create pipe: %s", strerror(errno));
			return false;
		}

		// Fork and execute sfsmetarestore
		pid_t pid = fork();
		if (pid == -1) {
			close(pipeFd[0]);
			close(pipeFd[1]);
			response.errorMessage = "Failed to fork process for sfsmetarestore";
			safs_pretty_syslog(LOG_ERR, "Failed to fork: %s", strerror(errno));
			return false;
		} else if (pid == 0) {
			// Child process
			close(pipeFd[0]);  // Close read end

			// Redirect stdout to pipe
			if (dup2(pipeFd[1], STDOUT_FILENO) == -1) {
				safs_pretty_syslog(LOG_ERR, "Failed to redirect stdout: %s", strerror(errno));
				exit(1);
			}
			close(pipeFd[1]);

			// Set nice value for lower priority
			if (nice(10) == -1) {
				safs_pretty_syslog(LOG_WARNING, "Failed to set nice value: %s", strerror(errno));
			}

			// Execute sfsmetarestore
			execv(metarestorePath.c_str(), argv.data());
			safs_pretty_syslog(LOG_ERR, "Failed to execute sfsmetarestore: %s", strerror(errno));
			exit(1);
		} else {
			// Parent process
			close(pipeFd[1]);  // Close write end

			// Read output from child process
			char buffer[1024];
			std::string output;
			ssize_t bytesRead;

			while ((bytesRead = read(pipeFd[0], buffer, sizeof(buffer) - 1)) > 0) {
				buffer[bytesRead] = '\0';
				output += buffer;
			}
			close(pipeFd[0]);

			// Wait for child process to complete
			int status;
			if (waitpid(pid, &status, 0) == -1) {
				response.errorMessage = "Failed to wait for sfsmetarestore process";
				safs_pretty_syslog(LOG_ERR, "Failed to wait for child process: %s",
				                   strerror(errno));
				return false;
			}

			// Check exit status
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
				// Success
				response.outputFile = outputFile;
				safs_pretty_syslog(LOG_INFO, "Successfully completed dump request %s, output: %s",
				                   request.requestId.c_str(), outputFile.c_str());

				// Parse output for any additional information
				if (!output.empty() && output.find("OK") != std::string::npos) {
					safs_pretty_syslog(LOG_INFO, "sfsmetarestore output: %s", output.c_str());
				}

				return true;
			} else {
				// Failure
				response.errorMessage =
				    "sfsmetarestore failed with exit code: " + std::to_string(WEXITSTATUS(status));
				if (!output.empty()) { response.errorMessage += ", output: " + output; }
				safs_pretty_syslog(LOG_ERR, "sfsmetarestore failed for request %s: %s",
				                   request.requestId.c_str(), response.errorMessage.c_str());

				// Clean up failed output file
				unlink(outputFile.c_str());
				return false;
			}
		}

	} catch (const std::exception &e) {
		response.errorMessage =
		    std::string("Exception during sfsmetarestore execution: ") + e.what();
		safs_pretty_syslog(LOG_ERR, "Exception during dump request %s: %s",
		                   request.requestId.c_str(), e.what());
		return false;
	} catch (...) {
		response.errorMessage = "Unknown exception during sfsmetarestore execution";
		safs_pretty_syslog(LOG_ERR, "Unknown exception during dump request %s",
		                   request.requestId.c_str());
		return false;
	}
}
void MetadumperService::rotateFiles(const std::string &baseFilename, int maxCopies) {
	if (maxCopies <= 0) return;

	// Remove the oldest backup if it exists
	std::string oldestBackup = baseFilename + "." + std::to_string(maxCopies);
	unlink(oldestBackup.c_str());

	// Rotate existing backups
	for (int i = maxCopies - 1; i >= 1; i--) {
		std::string oldName = baseFilename + "." + std::to_string(i);
		std::string newName = baseFilename + "." + std::to_string(i + 1);
		rename(oldName.c_str(), newName.c_str());
	}

	// Move current file to .1
	if (access(baseFilename.c_str(), F_OK) == 0) {
		std::string firstBackup = baseFilename + ".1";
		rename(baseFilename.c_str(), firstBackup.c_str());
	}
}

void MetadumperService::cleanupOldFiles() {
	// Implement cleanup logic for old dump files if needed
}  // This could be based on age or number of files to keep

}  // namespace metadumper
}  // namespace safs
