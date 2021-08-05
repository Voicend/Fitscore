#include "common.hpp"
#include "utils.hpp"
#include "models.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"
#include "machine.hpp"

using namespace fits;

extern std::map<std::string, ShiftMatrix> gShiftMatrixs;

int
fits::MachineRuntimeInfo::getCOTimeCost() {
	PMachine p = dynamic_cast<PMachine>(this);
	if (getCOStatus() == COS_GOING) return gShiftMatrixs[p->name][p->mode][p->COTarget].first * 60;
	else if(status == MS_CO) return gShiftMatrixs[p->name][p->mode][p->COTarget].first * 60;

	return 0;
}