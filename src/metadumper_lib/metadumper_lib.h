#ifndef SAUNAFS_METADUMPER_LIB_H
#define SAUNAFS_METADUMPER_LIB_H

#include "common/platform.h"

#include <string>
#include <vector>

namespace saunafs {
namespace metadumper {

int changelog_checkname(const char *fname);
void meta_version_on_disk(std::string path);

}  // namespace metadumper
}  // namespace saunafs

#endif  // SAUNAFS_METADUMPER_LIB_H
