#ifndef SAUNAFS_METADUMPER_FILE_TRANSFER_H
#define SAUNAFS_METADUMPER_FILE_TRANSFER_H

#include "common/platform.h"

#include <string>

namespace saunafs {
namespace metadumper {

class FileTransfer {
public:
	static bool transferFile(const std::string &sourcePath, const std::string &destinationPath);
	static bool transferFileChunked(const std::string &sourcePath,
	                                const std::string &destinationPath,
	                                size_t chunkSize = 64 * 1024);
	static bool atomicFileReplace(const std::string &sourcePath,
	                              const std::string &destinationPath);

private:
	static bool copyFileData(int sourceFd, int destFd, size_t chunkSize);
	static std::string getTempFileName(const std::string &destinationPath);
};

}  // namespace metadumper
}  // namespace saunafs

#endif  // SAUNAFS_METADUMPER_FILE_TRANSFER_H
