#ifndef SAUNAFS_METADUMPER_CLIENT_H
#define SAUNAFS_METADUMPER_CLIENT_H

#include "common/platform.h"

#include <chrono>
#include <memory>
#include <string>

#include "metadumper_service/protocol.h"

namespace saunafs {
namespace metadumper {

class MetadumperClient {
public:
    MetadumperClient(const std::string& socketPath);
    ~MetadumperClient();

    bool isServiceAvailable();
    std::string sendDumpRequest(uint64_t checksum, const std::string& changelogFile,
                               const std::string& outputPath, const std::string& metadataFile,
                               int storedMetaCopies);
    bool pollStatus(const std::string& requestId, DumpResponse& response);

private:
    bool connectToService();
    void disconnect();
    std::string generateRequestId();

    std::string socketPath_;
    int socket_;
    bool connected_;
};

} // namespace metadumper
} // namespace saunafs

#endif // SAUNAFS_METADUMPER_CLIENT_H

