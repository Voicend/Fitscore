#pragma once
#include"globlecontext.hpp"

#include <vector>
#include <list>
#include <cmath>

extern fits::ModelsManager gModels;
extern std::map<std::string, fits::ShiftMatrix> gShiftMatrixs;
//记录机器的等待时间，加工时间
extern std::map<int, std::pair<double, double>> workAndWaitTime;
//记录机器的换型时间，换型次数
extern std::map<int, std::pair<double, int>> coTimeAndCount;


namespace fits {
	template<typename T = std::vector<PJobUnit>>
	struct BaseSimulator {
		BaseSimulator(PProductLine ppl) : ppl(ppl) {}
		PProductLine ppl;
		T jobs;

		virtual void update(T jobs) = 0;
	};

	class Simulator : public BaseSimulator<JobsQueue> {
	protected:
		void log(PMachine m, PJobUnit j);
		/*
		return: true - stop working; false - go on
		*/
		//bool isCSVShown = false;
		bool moveJobToNextProcess(JobsOfProcess& JOP, JobUnit& which, PMachine fromMachine, PMachine toMachine, bool finalProcess = false) {
			GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
			auto j = which;
			if (fromMachine && !fromMachine->outputBuffer.empty())
				fromMachine->outputBuffer.pop();

			int from = j.process;
			j.machineId = toMachine ? toMachine->_index : -1;
			if (!finalProcess && toMachine) {
				j.ioo += 1;
				j.process = gModels[j.model].processes[j.ioo];
				toMachine->inputBuffer.push(j);
#if 0
				Utils::log(0, "== put %s(%d) on machine %s @ %d {status: %s, c/ostatus: %d}\n",
					gModels.get(j.model).name.data(), j.model,
					toMachine->fullname().data(),
					j.process,
					toMachine->getStatusName().data(),
					toMachine->getCOStatus());
#endif
				JOP[j.model][j.process].insert({ j.uid, j });
			}
			JOP[j.model][from].erase(j.uid);
//printf("finish%d at %d\n",j.uid,(int)this->realTime);
			if (JOP[j.model][-1].empty()) {
				for (int i = 0; i < from; i++) {
					if (!JOP[j.model][i].empty()) return false;
				}

				if (fromMachine
					&& fromMachine->model.empty()	///生产位空
					&& fromMachine->outputBuffer.empty() /// 出料口空
					&& (fromMachine->inputBuffer.empty() || fromMachine->inputBuffer.front().model != fromMachine->mode)	///还有未生产的
					) {

					/// debug
					for (auto& e : JOP) {
						//Utils::log(1, "!!!!!! model: %s(%d), processes: ", gModels.get(e.first).name.data(), e.first);
						for (auto& p : e.second) {
							//Utils::log(1, " {%d, %d},", p.first, p.second.size());
						}
						//Utils::log("\n");
					}
					/// debug end

					///*only* last tray fire the trigger, on one machine. 同道序机器之间节拍不同
#if 0
					Utils::log(1, "Yee! model(%d):%s left machine: %s on process: %d\n",
						j.model,
						gModels[j.model].name.data(),
						(fromMachine ? (fromMachine->name + fromMachine->number) : "NaM").data(),
						from);
#endif
					triggerIndex = fromMachine->_index;
					fromMachine->toIdle += gContext.offset;
					triggerIndexVec = {};
					triggerIndexVec.push_back(triggerIndex);
					return true;
				}
			}
			return false;
		}

		std::unique_ptr<CSVWriter>
			statusLogger = std::make_unique<FakeWriter>(),
			machineCOLogger = std::make_unique<FakeWriter>(),
			waitLogger = std::make_unique<FakeWriter>(),
			workLogger = std::make_unique<FakeWriter>();

	public:
		std::ofstream of_trayes_log;

		int triggerIndex = -1;
		std::vector<int> triggerIndexVec = {};
		//Jobs Of Process
		JobsOfProcess JOP = {};
		double realTime = 0.;
		double lastJobReleaseTime = 0;

		Simulator(PProductLine ppl) : BaseSimulator(ppl) {
			///std::ofstream::out | std::ofstream::app);
			GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
			//isCSVShown = gContext.isProcessDataShown;
			if (gContext.isProcessDataShown) {
				of_trayes_log.open("trayes_log.csv", std::ofstream::out);
				statusLogger.reset(new CSVWriter("status_log.csv", { "TIME", "IoM", "MACHINE", "PROCESS", "STATUS", "C/O STATUS", "IN-SIZE", "OUT-SIZE", "MODE" }, false));
				machineCOLogger.reset(new CSVWriter("co_log.csv", { "INDEX", "MACHINE", "PROCESS","TIME","CHANGETIME","MODE","CHANGEMODE" }, false));
				waitLogger.reset(new CSVWriter("wait_log.csv", { "INDEX", "MACHINE", "PROCESS","WAITTIMESTART","WAITTIMEEND","COSTTIME" }, false));
				workLogger.reset(new CSVWriter("work_log.csv", { "INDEX", "MACHINE", "PROCESS","WORKTIMESTART","WORKTIMEEND","COSTTIME" }, false));
			}
		}
		virtual ~Simulator() {
			if (of_trayes_log) {
				of_trayes_log.flush();
				of_trayes_log.close();
			}
		}

		Simulator& setup(std::map<int/*index of machine*/, int/*model*/> & configs) {
			for (auto e : configs) {
				auto pm = ppl->machines[e.first];
#if 0
				Utils::log(2, "### config: machine: %s (idx:%d)%s@%d %s(%d) -> %s(%d) !!!\n",
					pm->getStatusName().data(),
					pm->_index,
					pm->fullname().data(),
					pm->process,
					gModels.get(pm->mode).name.data(), pm->mode,
					gModels.get(e.second).name.data(), e.second);
#endif
				if(pm->_index==3)
				    int a;
				if (pm->mode < 0) pm->mode = e.second;
				else if (pm->getCOStatus() == Machine::COS_GOING)
				{
					///已经准备换型的暂不参与规划
					///FIXME：其实虽然已经准备换型，但是还没进换后型号的也可以重规划
#if 1
					Utils::log(-2, "machine  %d  %s will C/O from %s(%d) to %s(%d), discard to %s(%d) \n",
						pm->_index,
						pm->fullname().data(),
						gModels.get(pm->mode).name.data(), pm->mode,
						gModels.get(pm->COTarget).name.data(), pm->COTarget,
						gModels.get(e.second).name.data(), e.second);
#endif
				}
				else if (pm->status != Machine::MS_CO && pm->mode != e.second) {
					pm->setCOStatus(Machine::COS_GOING);
					pm->COTarget = e.second;
				}
			}

			return *this;
		}
void printjop(JobsOfProcess jop){
printf("---------------------------------------------------\n");
for(auto i : jop){
printf("%d:\n",i.first);
for(auto j : i.second){
printf("%d: ",j.first);
int a = 0, b = 0;
for(auto k : j.second){
if(k.second.daysIndex==0)a++;
else b++;
}
printf("(%d,%d)\n",a,b);
}
}
}
void printjopjobs(JobsOfProcess jop){
std::vector<JobUnit> jobstmp;
for(auto &i:JOP){
for(auto &i2: i.second){
for(auto &i3: i2.second)
jobstmp.push_back(i3.second);
}
}
std::sort(jobstmp.begin(),jobstmp.end(),[](JobUnit j1, JobUnit j2){return j1.uid<j2.uid;});
for(auto i : jobstmp){
printf("%d: %d/%d/%d/%s\n",i.uid,i.machineId,
i.model,i.process,i.taskInfo.date.c_str());
}
}
		void update(JobsQueue jobs) override {
			const size_t TOTAL = jobs.size();

			for (size_t i = 0; i < TOTAL; i++) {
				auto& e = jobs.top();
				int p = e.ioo < 0 ? -1 : gModels[e.model].processes[e.ioo];
				JOP[e.model][p][e.uid] = e;
				jobs.pop();
			}
			/// debug
			/*for (auto& e : JOP) {
				Utils::log(1, "model: %s(%d), processes: ", gModels.get(e.first).name.data(), e.first);
				for (auto& p : e.second) {
					Utils::log(1, " {%d, %d},", p.first, p.second.size());
				}
				Utils::log("\n");
			}*/
			/// debug end

			while (!this->jobs.empty()) this->jobs.pop();
		}

		virtual int simulate();
	private:

	};
}