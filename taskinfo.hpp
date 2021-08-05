#pragma once

#include <string>

namespace fits {
	typedef struct TaskInfo {
		int IOD = 0;	///Index Of Day
		std::string date;

		TaskInfo() {}
		TaskInfo(std::string date) : date(date) {}
	} *PTaskInfo;
}