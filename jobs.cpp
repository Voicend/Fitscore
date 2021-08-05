#include "taskinfo.hpp"
#include "jobs.hpp"

using namespace fits;

///cost of time
double JobUnitInfo::COT(double clock) { return clock * CAPACITY; }

JobUnitInfo::JobUnitInfo(int model) : JobUnitInfo(0, model) {}
JobUnitInfo::JobUnitInfo(int uid, int model) : uid(uid), model(model) {}


fits::JobUnit::JobUnit(int model, fits::TaskInfo& taskInfo)
	: fits::JobUnitInfo(model),
	releaseTime(0),
	taskInfo(taskInfo) {}
fits::JobUnit::JobUnit(int model, fits::TaskInfo& taskInfo, int progress)
	: fits::JobUnitInfo(model),
	releaseTime(0),
	taskInfo(taskInfo),
	daysIndex(progress) {}
JobUnit::JobUnit(int model, int process) : JobUnitInfo(model), process(process), releaseTime(0) {}

JobUnit::JobUnit(int model) : JobUnit(model, -1) {}

JobUnit::JobUnit(int uid, int model, int ioo, int process, /*int releaseTime,*/ /*int machineId,*/ int daysIndex) :
	JobUnitInfo(uid, model), ioo(ioo), process(process), releaseTime(releaseTime), machineId(machineId), daysIndex(daysIndex) {
}

JobUnit::JobUnit() : JobUnit(-1) {}