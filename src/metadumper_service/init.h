/*
   Copyright 2023      Leil Storage OÜ

   SaunaFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   SaunaFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with SaunaFS  If not, see <http://www.gnu.org/licenses/>.
 */

 #include "common/platform.h"

 #include <memory>
 #include <vector>

 #include "common/event_loop.h"
 #include "common/random.h"
 #include "common/run_tab.h"
 #include "config/cfg.h"
 #include "metadumper_service/metadumper_service.h"
 #include "slogger/slogger.h"

 using namespace safs::metadumper;

 namespace {
	 std::unique_ptr<MetadumperService> gMetadumperService;
 }

 inline int metadumper_service_init() {
	 try {
		 std::string socketPath = cfg_get("METADUMPER_SERVICE_SOCKET",
										 "/var/run/saunafs/metadumper.sock");

		 gMetadumperService = std::make_unique<MetadumperService>(socketPath);

		 if (!gMetadumperService->start()) {
			 safs_pretty_syslog(LOG_ERR, "Failed to start metadumper service");
			 return -1;
		 }

		 safs_pretty_syslog(LOG_INFO, "Metadumper service initialized on socket: %s",
						   socketPath.c_str());

		 // Register cleanup function
		 eventloop_destructregister([]() {
			 if (gMetadumperService) {
				 gMetadumperService->stop();
				 gMetadumperService.reset();
			 }
		 });

		 return 0;
	 } catch (const std::exception& e) {
		 safs_pretty_syslog(LOG_ERR, "Failed to initialize metadumper service: %s", e.what());
		 return -1;
	 }
 }

 /// Functions to call before normal startup
 inline const std::vector<RunTab> earlyRunTabs = {};

 /// Functions to call during normal startup
 inline const std::vector<RunTab> runTabs = {
	 RunTab{rnd_init, "random generator"},
	 RunTab{metadumper_service_init, "metadumper service"}
 };

 /// Functions to call delayed after the initialization is correct
 inline const std::vector<RunTab> lateRunTabs = {};
