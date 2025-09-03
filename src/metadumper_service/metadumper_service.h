#ifndef SAUNAFS_METADUMPER_SERVICE_H
#define SAUNAFS_METADUMPER_SERVICE_H

#include "common/platform.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "metadumper_service/protocol.h"

namespace safs {
namespace metadumper {

class MetadumperService {
public:
    MetadumperService(const std::string& socketPath);
    ~MetadumperService();

    bool start();
    void stop();
    void run();
    bool isRunning() const;

private:
    void handleClient(int clientSocket);
    bool processDumpRequest(const DumpRequest& request, DumpResponse& response);
    void cleanupOldFiles();
    void rotateFiles(const std::string& baseFilename, int maxCopies);

    std::string socketPath_;
    int serverSocket_;
    std::atomic<bool> running_;
    std::thread serviceThread_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> activeRequests_;
    mutable std::mutex requestsMutex_;
};

} // namespace metadumper
} // namespace safs

#endif // SAUNAFS_METADUMPER_SERVICE_H
