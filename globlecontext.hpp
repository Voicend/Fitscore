#pragma once

#include <map>
#include <vector>
#include <queue>
#include <string>


namespace fits {
	typedef struct GlobleContextManager {
		static GlobleContextManager* getGlobleContextManager() {
			return instance ? instance : instance = new GlobleContextManager();
		}
	public:
		std::string mqHost = "127.0.0.1";
		std::string mqUsername;
		std::string mqPassword;
		std::string mqQueueName = "fits.core";
		int port = 5672;
		int windowPhase = 3;
		int windowMaxPreFetchDays = 3;
		int windowMaxDelaySeconds = 79200 * 3;
		int workingTimePerDay = 86400;
		int checkTime = 60;
		int endlessLoopWaitTime = 500000;
		/// ªª–Õ√Ù∏–∂»
		double coSusceptibility = 1.0;
		int offset = 3600;
		std::string id;
		int isProcessDataShown = 0;
	private:
		GlobleContextManager() {}
		static GlobleContextManager* instance;

	} *PGlobleContextManager;
}