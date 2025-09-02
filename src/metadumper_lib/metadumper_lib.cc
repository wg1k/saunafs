#include "metadumper_lib/metadumper_lib.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "common/cwrap.h"
#include "common/rotate_files.h"
#include "common/setup.h"
#include "master/changelog.h"
#include "master/chunks.h"
#include "master/filesystem.h"
#include "master/hstring_memstorage.h"
#include "master/hstring_storage.h"
#include "master/metadata_backend_common.h"
#include "master/metadata_backend_file.h"
#include "master/metadata_backend_interface.h"
#include "master/restore.h"
#include "metarestore/merger.h"
#include "slogger/slogger.h"

namespace saunafs {
namespace metadumper {

int changelog_checkname(const char *fname) {
	const char *ptr = fname;
	std::string testName[] = {kChangelogFilename, kChangelogMlFilename};

	for (const std::string &s : testName) {
		if (strncmp(ptr, s.c_str(), s.length()) == 0) {
			ptr += s.length();
			if (*ptr == '.') {
				++ptr;
				while (isdigit(*ptr)) { ++ptr; }
				if (*ptr == 0) { return 1; }
			} else if (*ptr == 0) {
				return 1;
			}
		}
	}

	std::string testNameOld[] = {"changelog.", "changelog_ml.", "changelog_ml_back."};

	for (const std::string &s : testNameOld) {
		if (strncmp(ptr, s.c_str(), s.length()) == 0) {
			ptr += s.length();
			if (isdigit(*ptr)) {
				while (isdigit(*ptr)) { ++ptr; }
				if (strcmp(ptr, ".sfs") == 0) { return 1; }
			}
		}
	}
	return 0;
}

void meta_version_on_disk(std::string path) {
	static const std::string changelogs[] = {std::string(kChangelogFilename) + ".2",
	                                         std::string(kChangelogFilename) + ".1",
	                                         kChangelogFilename};
	uint64_t metadata_version;

	try {
		metadata_version = gMetadataBackend->getVersion(path + "/" + kMetadataFilename);
		// check if it is a new installation
		if (metadata_version == 0) {
			printf("1\n");
			return;
		}
	} catch (MetadataCheckException &ex) {
		safs_pretty_syslog(LOG_WARNING, "malformed metadata file: %s", ex.what());
		printf("0\n");
		return;
	}

	bool oldExists = false;
	for (const std::string &s : changelogs) {
		std::string fullFileName = path + "/" + s;
		try {
			if (fs::exists(fullFileName)) {
				oldExists = true;
				uint64_t first = changelogGetFirstLogVersion(fullFileName);
				uint64_t last = changelogGetLastLogVersion(fullFileName);
				if (last >= first && first <= metadata_version) {
					if (last >= metadata_version) { metadata_version = last + 1; }
				} else {
					printf("0\n");
					return;
				}
			} else if (oldExists && fullFileName != kChangelogFilename) {
				safs_pretty_syslog(LOG_WARNING, "changelog `%s\' missing", fullFileName.c_str());
			}
		} catch (FilesystemException &ex) {
			safs_pretty_syslog(LOG_WARNING, "exception while fs:exists: %s", ex.what());
			printf("0\n");
			return;
		}
	}

	printf("%" PRIu64 "\n", metadata_version);
}

}  // namespace metadumper
}  // namespace saunafs
