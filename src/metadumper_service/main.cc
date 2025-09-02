#include "common/platform.h"

#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#include "common/setup.h"
#include "metadumper_service/metadumper_service.h"
#include "slogger/slogger.h"

static volatile bool gTerminate = false;

void signalHandler(int signal) {
	if (signal == SIGTERM || signal == SIGINT) { gTerminate = true; }
}

void usage(const char *appname) {
	std::cerr << "Usage: " << appname << " [-s socket_path] [-d] [-h]\n"
	          << "  -s socket_path  Unix socket path (default: /var/run/saunafs/metadumper.sock)\n"
	          << "  -d              Run as daemon\n"
	          << "  -h              Show this help\n";
}

int main(int argc, char *argv[]) {
	std::string socketPath = "/var/run/saunafs/metadumper.sock";
	bool daemonize = false;

	// Parse command line arguments
	int opt;
	while ((opt = getopt(argc, argv, "s:dh")) != -1) {
		switch (opt) {
		case 's':
			socketPath = optarg;
			break;
		case 'd':
			daemonize = true;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	// Initialize logging
	safs::drop_all_logs();
	safs::add_log_syslog(safs::log_level::info);
	if (!daemonize) { safs::add_log_stderr(safs::log_level::info); }

	// Setup signal handlers
	signal(SIGTERM, signalHandler);
	signal(SIGINT, signalHandler);
	signal(SIGPIPE, SIG_IGN);

	// Daemonize if requested
	if (daemonize) {
		if (daemon(0, 0) != 0) {
			safs_pretty_errlog(LOG_ERR, "daemon() failed");
			return 1;
		}
	}

	safs_pretty_syslog(LOG_INFO, "Starting SaunaFS Metadumper Service");
	safs_pretty_syslog(LOG_INFO, "Socket path: %s", socketPath.c_str());

	// Create and start the service
	saunafs::metadumper::MetadumperService service(socketPath);

	if (!service.start()) {
		safs_pretty_syslog(LOG_ERR, "Failed to start metadumper service");
		return 1;
	}

	// Main loop
	while (!gTerminate && service.isRunning()) { sleep(1); }

	safs_pretty_syslog(LOG_INFO, "Stopping SaunaFS Metadumper Service");
	service.stop();

	return 0;
}
