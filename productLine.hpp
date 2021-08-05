#pragma once

#include <vector>
#include <map>
#include <queue>
#include <string>

#include "machine.hpp"

namespace fits {
	///一个道序的设备集合
	///N个道序组成产线
	///唯一存放machines信息的地方, 其他地方引用
	typedef std::vector<Machine> Process, * PProcess;
	//TODO: extends from public std::map<int, Process> {
	typedef struct ProductLine : public std::vector<Process> {

		//std::map<int, PMachine> machines;
		std::vector<PMachine> machines;

		///更新索引
		void index(PMachine pm) {
			if (pm->_index < 0) {
				pm->_index = (int)machines.size();
				machines.push_back(pm);
				//同时更新每个机器料道和正在加工的零件的ID
				vector<JobUnit> inputBuffer = {};
				while (!pm->inputBuffer.empty()) {
					auto jobTemp = pm->inputBuffer.front();
					pm->inputBuffer.pop();
					inputBuffer.push_back(jobTemp);
				}
				for (auto& jobTemp : inputBuffer) {
					jobTemp.machineId = pm->_index;
					pm->inputBuffer.push(jobTemp);
				}
				vector<JobUnit> outputBuffer = {};
				while (!pm->outputBuffer.empty()) {
					auto jobTemp = pm->outputBuffer.front();
					pm->outputBuffer.pop();
					outputBuffer.push_back(jobTemp);
				}
				for (auto& jobTemp : outputBuffer) {
					jobTemp.machineId = pm->_index;
					pm->outputBuffer.push(jobTemp);
				}
				vector<JobUnit> producting = {};
				while (!pm->model.empty()) {
					auto jobTemp = pm->model.front();
					pm->model.pop();
					producting.push_back(jobTemp);
				}
				for (auto& jobTemp : producting) {
					jobTemp.machineId = pm->_index;
					pm->model.push(jobTemp);
				}
			}
			else {
				machines[pm->_index] = pm;
			}
			indexerByFullname[pm->fullname()] = pm;
		}

		PMachine get(const std::string& fullname) {
			if (indexerByFullname.count(fullname) == 0) return nullptr;
			return indexerByFullname.at(fullname);
		}
		//std::vector<int> genericProcess;
		///将当前的状态保存到@clone中
		void snapshot(std::map<int/*global index*/, MachineRuntimeInfo> & clone) {
			for (auto& e : *this)
				for (auto& m : e) clone[m._index] = m;
		}

		void restore(std::map<int/*global index*/, MachineRuntimeInfo> & clone) {
			for (auto& e : *this)
				for (auto& m : e) dynamic_cast<MachineRuntimeInfo&>(m) = clone[m._index];
		}
		bool judgeCapacityOverLastProcess(int machineProcess, int mode);
		bool isGenericProcess(int process);
		void machineChangeMode(int index, int mode);
		int getLowestCapacityProcess(int modelId);
		double CountToFinishBeforeDeadLine(int deadLine, int realTime, int modelId);
		double CountToFinishBeforeDeadLineInCertainProcess(int deadLine, int realTime, int modelId, int Process);
		double spitByCapacity(int machineId, int mode, int leftCountInProcess);
		double getCapacityInProcess(int modelId, int process);
		double getLowestCapacity(int modelId);
		double getLowestCapacityAfterCO(int modelId);
		double getLowestCapacityAfterCO(int modelId, int machineIndex);
		bool judgeKeyMachineInProcess(int process, int triggerIndex);
		bool judgePauseMachineInProcess(int process, int triggerIndex, int mode);
		std::vector<int> getLowestCapacityProcesses(int modelId);
		std::vector<int> getLowestCapacityProcessesUnsensitive(int modelId);
		void setCapacitySurplusMachineOffLine();
		RequirementTasks * pTasks = nullptr;	///产线上当前分派的任务
	private:
		std::map<std::string/*fullname*/, PMachine> indexerByFullname;
	} *PProductLine;
}