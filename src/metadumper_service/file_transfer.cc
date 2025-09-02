#include "metadumper_service/file_transfer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <memory>
#include <sstream>

#include "slogger/slogger.h"

namespace saunafs {
namespace metadumper {

bool FileTransfer::transferFile(const std::string &sourcePath, const std::string &destinationPath) {
	return transferFileChunked(sourcePath, destinationPath);
}

bool FileTransfer::transferFileChunked(const std::string &sourcePath,
                                       const std::string &destinationPath, size_t chunkSize) {
	int sourceFd = open(sourcePath.c_str(), O_RDONLY);
	if (sourceFd == -1) {
		safs_pretty_errlog(LOG_ERR, "Failed to open source file: %s", sourcePath.c_str());
		return false;
	}

	int destFd = open(destinationPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (destFd == -1) {
		safs_pretty_errlog(LOG_ERR, "Failed to open destination file: %s", destinationPath.c_str());
		close(sourceFd);
		return false;
	}

	bool success = copyFileData(sourceFd, destFd, chunkSize);

	close(sourceFd);
	close(destFd);

	if (!success) {
		unlink(destinationPath.c_str());  // Clean up on failure
	}

	return success;
}

bool FileTransfer::atomicFileReplace(const std::string &sourcePath,
                                     const std::string &destinationPath) {
	std::string tempPath = getTempFileName(destinationPath);

	if (!transferFileChunked(sourcePath, tempPath)) { return false; }

	if (rename(tempPath.c_str(), destinationPath.c_str()) != 0) {
		safs_pretty_errlog(LOG_ERR, "Failed to rename temp file to destination: %s",
		                   destinationPath.c_str());
		unlink(tempPath.c_str());
		return false;
	}

	// Remove source file after successful transfer
	unlink(sourcePath.c_str());
	return true;
}

bool FileTransfer::copyFileData(int sourceFd, int destFd, size_t chunkSize) {
	std::unique_ptr<char[]> buffer(new char[chunkSize]);

	while (true) {
		ssize_t bytesRead = read(sourceFd, buffer.get(), chunkSize);
		if (bytesRead == -1) {
			if (errno == EINTR) {
				continue;  // Retry on interrupt
			}
			safs_pretty_errlog(LOG_ERR, "Failed to read from source file");
			return false;
		}

		if (bytesRead == 0) {
			break;  // End of file
		}

		ssize_t bytesWritten = 0;
		while (bytesWritten < bytesRead) {
			ssize_t result = write(destFd, buffer.get() + bytesWritten, bytesRead - bytesWritten);
			if (result == -1) {
				if (errno == EINTR) {
					continue;  // Retry on interrupt
				}
				safs_pretty_errlog(LOG_ERR, "Failed to write to destination file");
				return false;
			}
			bytesWritten += result;
		}
	}

	// Ensure data is written to disk
	if (fsync(destFd) != 0) {
		safs_pretty_errlog(LOG_ERR, "Failed to sync destination file");
		return false;
	}

	return true;
}

std::string FileTransfer::getTempFileName(const std::string &destinationPath) {
	std::ostringstream oss;
	oss << destinationPath << ".tmp." << getpid() << "." << time(nullptr);
	return oss.str();
}

}  // namespace metadumper
}  // namespace saunafs
