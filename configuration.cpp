#include "common.hpp"
#include "utils.hpp"
#include "models.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"
#include "machine.hpp"
#include "productline.hpp"
#include "configuration.hpp"

using namespace fits;

void
Configuration::correctRedundantCO() {
	
	typedef std::vector<std::pair<PMachine, int/*to model*/>> COMachines;

	//1. find out all C/O-going machines
	for (auto& e : *this) {
		auto process = e.first;
		auto &mapOfModelMachines = e.second;

		COMachines coms = {};
		for (auto& m : mapOfModelMachines) {
			auto toModel = m.first;
			auto& machines = m.second;
			/// if m in machines 's mode != toModel, it will C/O
			for (auto& m : machines) {
				if (m->mode != toModel) coms.push_back({ m, toModel });
			}
		}

		bool hasSwitched = false;
		do {
			///2. 同道序是否有其他机器换成"我"的mode, "我"换成别人的model
			for (int i = 0; i < coms.size(); i++) {
				auto& p = coms.at(i);
				auto me = p.first;	//"我"
				auto myTarget = p.second;

				for (int i2 = i; i2 < coms.size(); i2++) {
					auto& p2 = coms.at(i2);
					auto other = p2.first;
					auto otherTarget = p2.second;

					if (otherTarget == me->mode) {
                        hasSwitched = true;
                                ///switch
						std::swap(me, other);
					}
					else if (myTarget == other->mode) {
                        hasSwitched = true;
						///switch
						std::swap(me, other);
					}
				}
			}
		} while (hasSwitched);
	}
}