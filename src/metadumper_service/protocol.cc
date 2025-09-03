#include "metadumper_service/protocol.h"

#include <sstream>

namespace safs {
namespace metadumper {

std::string DumpRequest::serialize() const {
	std::ostringstream oss;
	oss << requestId << "\n"
	    << checksum << "\n"
	    << changelogFile << "\n"
	    << outputPath << "\n"
	    << metadataFile << "\n"
	    << storedMetaCopies << "\n";
	return oss.str();
}

DumpRequest DumpRequest::deserialize(const std::string &data) {
	std::istringstream iss(data);
	DumpRequest request;
	std::string line;

	if (std::getline(iss, request.requestId) && std::getline(iss, line)) {
		request.checksum = std::stoull(line);
	}
	std::getline(iss, request.changelogFile);
	std::getline(iss, request.outputPath);
	std::getline(iss, request.metadataFile);
	if (std::getline(iss, line)) { request.storedMetaCopies = std::stoi(line); }

	return request;
}

std::string DumpResponse::serialize() const {
	std::ostringstream oss;
	oss << requestId << "\n"
	    << (success ? "1" : "0") << "\n"
	    << errorMessage << "\n"
	    << outputFile << "\n";
	return oss.str();
}

DumpResponse DumpResponse::deserialize(const std::string &data) {
	std::istringstream iss(data);
	DumpResponse response;
	std::string line;

	std::getline(iss, response.requestId);
	if (std::getline(iss, line)) { response.success = (line == "1"); }
	std::getline(iss, response.errorMessage);
	std::getline(iss, response.outputFile);

	return response;
}

}  // namespace metadumper
}  // namespace safs
