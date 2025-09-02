#ifndef SAUNAFS_METADUMPER_SERVICE_H
#define SAUNAFS_METADUMPER_SERVICE_H

#include "common/platform.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "metadumper_service/file_transfer.h"
#include "metadumper_service/protocol.h"

namespace saunafs {
namespace metadumper {

class MetadumperService {
public:
	MetadumperService(const std::string &socketPath);
	~MetadumperService();

	bool start();
	void stop();
	bool isRunning() const;

private:
	void serverLoop();
	void handleClient(int clientSocket);
	bool processDumpRequest(const DumpRequest &request, DumpResponse &response);
	void cleanupOldFiles();

	std::string socketPath_;
	int serverSocket_;
	std::atomic<bool> running_;
	std::thread serverThread_;
	std::queue<DumpRequest> requestQueue_;
	std::unordered_map<std::string, std::string> activeRequests_;
	mutable std::mutex requestsMutex_;
};

}  // namespace metadumper
}  // namespace saunafs

#endif  // SAUNAFS_METADUMPER_SERVICE_H
