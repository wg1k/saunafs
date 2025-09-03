#ifndef SAUNAFS_METADUMPER_PROTOCOL_H
#define SAUNAFS_METADUMPER_PROTOCOL_H

#include "common/platform.h"

#include <cstdint>
#include <string>

namespace safs {
namespace metadumper {

struct DumpRequest {
	std::string requestId;
	uint64_t checksum;
	std::string changelogFile;
	std::string outputPath;
	std::string metadataFile;
	int storedMetaCopies;

	std::string serialize() const;
	static DumpRequest deserialize(const std::string &data);
};

struct DumpResponse {
	std::string requestId;
	bool success;
	std::string errorMessage;
	std::string outputFile;

	std::string serialize() const;
	static DumpResponse deserialize(const std::string &data);
};

}  // namespace metadumper
}  // namespace safs

#endif  // SAUNAFS_METADUMPER_PROTOCOL_H
