#pragma once

#include <queue>
#include <string>
#include <map>
#include <iostream>

namespace fits {

	//计划把运行时会变的状态提取出来
	typedef struct MachineRuntimeInfo {
		//0空闲态，1生产状态，2换型态, 3 blocking
		enum MachineStatus {
			MS_IDLE = 0,
			MS_WORKING,
			MS_CO,
			MS_BLOCKING
		} status = MS_IDLE;
		/* return is avaliable for re-plan
		*/
		bool avaliable() {
            return !(status == MS_CO);
        }
		virtual bool acceptable(JobUnit & job) {
			if (getCOStatus() == COS_GOING) {
				return (COTarget == job.model);
			}
			else if (status == MS_CO) {
				return (COTarget == job.model);
			}
			else if (mode == job.model) {
				return true;
			}
			return false;
		}

		//预期多少秒后的空闲时间
		double toIdle = 0;

		//当前的工作模式，产品型号索引
		int mode = -1;
		std::string getStatusName() {
			static std::map<MachineStatus, std::string> table = {
				{MS_IDLE, "MS_IDLE"},
				{MS_WORKING, "MS_WORKING"},
				{MS_CO, "C/O"},
				{MS_BLOCKING, "MS_BLOCKING"},
			};
			auto& result = table[(MachineStatus)status];
			return result.empty() ? "NaS" : result;
		}
		//记录机器是否准备换型0为不准备，1为准备
		int changeStatus = 0;
		enum ChangeOverStatus {
			COS_KEEP = 0,
			COS_GOING
		};
		ChangeOverStatus getCOStatus() {
			return (ChangeOverStatus)changeStatus;
		}
		void setCOStatus(ChangeOverStatus coStatus) {
			changeStatus = coStatus;
		}

		int COTarget = -1;	//仅当需要换型时有效
		int getCOTimeCost();

		MachineRuntimeInfo(int mode = 0, int toIdle = 0) : toIdle(toIdle), mode(mode) {}

		enum MachineState {
			S_ONLINE = 0,
			S_OFFLINE,
			S_PAUSE,		///在线但不工作
		};
		MachineState getState() { return state; }
		void setState(MachineState state) { this->state = state; }

	private:
		MachineState state;

	} *PMachineRuntimeInfo;

	//设备，此类仅包含与物理机属性、行为相关的内容
	//运行时变化的属性，在MachineRuntimeInfo中
	typedef struct Machine : public MachineRuntimeInfo
	{
		///道序编号
		int process = 0;
		std::string number;
		std::string name;   //model name
		std::pair<int/*自动*/, int/*手动*/> bufferSize;
		///设备生产节拍时间矩阵
		MachineClockMatrix clocks;
		bool generic = false;

		std::queue<JobUnit> inputBuffer;
		std::queue<JobUnit> outputBuffer;
		//当前在生产的零件 FIXME: 应该只用来做当前生产型号的记录，并放在MachineRuntimeInfo中；代表生产位应该用其他名命
		std::queue<JobUnit> model;

		std::string fullname() {
			return name + "_" + number;
		}

		Machine(int process, const std::string& number, const std::string& name, MachineClockMatrix& clocks, std::pair<int/*自动*/, int/*手动*/> bufferSize, bool generic = false) :
			MachineRuntimeInfo(-1),
			process(process),
			number(number),
			name(name),
			clocks(clocks),
			bufferSize(bufferSize),
			generic(generic)
		{}

		Machine(int process, const std::string& number, const std::string& name, std::pair<int/*自动*/, int/*手动*/> bufferSize, bool generic = false) :
			MachineRuntimeInfo(-1),
			process(process),
			number(number),
			name(name),
			bufferSize(bufferSize),
			generic(generic)
		{}

		static std::map<int/*全局机器index*/, std::map<int/*model-id*/, double/*计算的clock*/>> calculatedTickMatrix;

		///获取该机器生产modelId型号的节拍，同 .clocks[modelId]
		double clock(int modelId) {
			auto& value = calculatedTickMatrix[_index][modelId];
			if (value > 0) return value;

			MachineClockMatrix::iterator it = clocks.find(modelId);
			if (it == clocks.end()) return -1;
			return value = it->second;
		}

		///机器是否能够生产model型号(通过换型后支持也算)
		bool support(int model) {
			return generic || (clock(model) >= 0);
		}

		virtual bool acceptable(JobUnit & job) override {
			if (inputBuffer.size() >= COB()) {
				return false;
			}
			else if (MachineRuntimeInfo::acceptable(job)) {
				return true;
			}
			else if (generic) {
				return true;
			}
			return false;
		}

		//Capacity Of Buffer
		int COB() {
			//return (*this).generic ? INT_MAX : (*this).bufferSize.first;
			return (*this).bufferSize.first;
		}

		///开始加工@job
		int workOn(JobUnit & job, int now) {
			status = Machine::MS_WORKING;
			toIdle = now + job.COT(clock(job.model));
			model.push(job);
			inputBuffer.pop();
			return 0;
		}

		///执行换型
		int changeOver(int cost, int now) {
			status = Machine::MS_CO;
			setCOStatus(Machine::COS_KEEP);
			toIdle = now + cost;
			/*std::cout << fullname() << " ready to C/O from " << mode << " -> " << COTarget
				<< " with input " << inputBuffer.size() << ",type "
				<< (inputBuffer.empty() ? -1 : inputBuffer.front().model) << "\n";*/
			return 0;
		}

		/*料框从加工位完成进入出料道*/
		int finishJob(JobUnit & job, int now) {
			job.releaseTime = now;
			outputBuffer.push(job);
			model.pop();
			toIdle = now;
			status = Machine::MS_IDLE;
			return 0;
		}
		/*判断机器是否停机*/
		bool isMachineOnLine() {
			if ((*this).getState() != S_OFFLINE && (*this).getState() != S_PAUSE) {
				return true;
			}
			return false;
		}

		int _index = -1;	///global index
	} *PMachine;
}