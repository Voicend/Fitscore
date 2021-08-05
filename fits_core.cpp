#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <queue>
#include <ctime>
#include <sstream>
#include <utility>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <thread>
#include <chrono>

#include "common.hpp"
#include "utils.hpp"

#include "models.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"
#include "shiftbook.hpp"
#include "productline.hpp"
#include "configuration.hpp"
#include "simulator.hpp"
#include "assigner.hpp"
#include "voter.hpp"
#include "loader.hpp"
#include "mq.hpp"
#include"globlecontext.hpp"

#include <json/json.h>
#include<json/reader.h>


/// test for rabbitmq-c
extern "C" {
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include "3rd/utils.h"
}
#pragma comment(lib, "lib/jsoncpp.lib")
#pragma comment(lib, "./lib/librabbitmq.4.lib")
#pragma comment(lib, "ws2_32.lib")
/// test end

using namespace fits;

namespace fits {
	bool operator> (const JobUnit& lhs, const JobUnit& rhs) {
	    return rhs.releaseTime < lhs.releaseTime;
	}

	bool operator< (const ShiftBookItem& lhs, const ShiftBookItem& rhs) {
		return lhs.beginTime > rhs.beginTime;
	}

	//static
	PShiftBookManager ShiftBookManager::instance = nullptr;
	PGlobleContextManager GlobleContextManager::instance = nullptr;
}

void printHelp() {
	///FITSCore.exe Junex30.csv > >(tee stdout.log) 2> >(tee stderr.log >&2)
	std::cout << "fits-core input-file.csv   id \n";
}

void printStartupInfo() {
	std::cout << "fits-core starting ...\n";
}
//ȫ�ֻ���״̬�ͻ�������״̬
ProductLine gProductLine;
///�����ͺ�

fits::ModelsManager gModels;
//MQ�����Ϣ
std::map<std::string, ShiftMatrix> gShiftMatrixs;
//��¼�����ĵȴ�ʱ�䣬�ӹ�ʱ��
std::map<int, std::pair<double, double>> workAndWaitTime;
//��¼�����Ļ���ʱ��ʹ���
std::map<int, std::pair<double, int>> coTimeAndCount;
//��¼��������
std::vector<std::string> machineCO;
//��¼�����ϵ��Ĵ�С
std::map<std::string, std::pair<int/*�Զ�*/, int/*�ֶ�*/>> gBufferSizeMatrix;
//static
std::unique_ptr<Model> ModelsManager::InvalidModel = std::unique_ptr<Model>(new Model(-1, "invalid"));

std::map<int/*ȫ�ֻ���index*/, std::map<int/*model-id*/, double/*�����clock*/>> Machine::calculatedTickMatrix = { };

//static
fits::PMQ fits::MQ::instance = nullptr;

///ȫ����Ҫ�ŵļƻ�
RequirementTasks gTasks = {};
long long gMaxIdOfJob = 0ll;
fits::JobsOfProcess jop = {};
std::map<int/*����index*/, std::pair<long/*offLineStartTime*/, long/*offLineEndTime*/>> outageTable = {};

typedef std::map<std::string/*date*/, RequirementTasks> DailyRequirementTasks;
typedef std::vector<RequirementTasks> DaysRequirementTasksIndex;

//��ʼ������
ConfigurationShadow bestConfig;
//�жϸõ����Ƿ���ͨ�õ���
bool ProductLine::isGenericProcess(int process) {
	/*if (std::count((*this).genericProcess.begin(), (*this).genericProcess.end(), process) > 0)
		return true;
	else
		return false;*/
	if (process >= 0 && process < (*this).size())
		return (*this)[process][0].generic;
	else
		return false;
}
//����index������?Q��
void ProductLine::machineChangeMode(int index, int mode) {
	for (auto& machines : (*this)) {
		for (auto& machine : machines) {
			if (machine._index == index) {
                machine.mode = mode;
            }
		}
	}
};
//���������߲���Ԥ���ڽ���ʱ��֮ǰ������ɶ��ٿ����
double ProductLine::CountToFinishBeforeDeadLineInCertainProcess(int deadLine, int realTime, int modelId, int _process) {
	if (deadLine < realTime)
		return -1;
	std::map<int/*machine.toIdle*/, int/*machine.index*/> COTable = {};
	std::vector<std::pair<int/*time period*/, double/*capacity*/>> capasityTable = {};
	int processIndex = 0;
	for (auto& process : (*this)) {
		if (processIndex != _process) {
			processIndex++;
			continue;
		}
		for (auto& machine : process) {
                if (machine.COTarget == modelId && machine.COTarget != machine.mode && machine.isMachineOnLine()) {
				if (machine.toIdle < realTime)
					COTable[realTime + gShiftMatrixs[machine.name][machine.mode][modelId].first * 60] = machine._index;
				else
					COTable[machine.toIdle + gShiftMatrixs[machine.name][machine.mode][modelId].first * 60] = machine._index;
			}
		}
		processIndex++;
	}
	ProductLine productLineTemp = (*this);
	int lastTimePoint = realTime;
	double capacity = productLineTemp.getCapacityInProcess(modelId, _process);
	double finishedModels = 0;
	int firstFlag = 0;
	for (auto& e : COTable) {
		/*if (firstFlag != 0) {
			finishedModels += (e.first - lastTimePoint) * capacity;
			productLineTemp.machineChangeMode(e.second, modelId);
			capacity = productLineTemp.getLowestCapacity(modelId);
			lastTimePoint = e.first;
		}
		firstFlag = 1;*/
		finishedModels += (e.first - lastTimePoint) * capacity;
		productLineTemp.machineChangeMode(e.second, modelId);
		capacity = productLineTemp.getCapacityInProcess(modelId, _process);
		lastTimePoint = e.first;
	}
	finishedModels += (deadLine - lastTimePoint) * capacity;
	return finishedModels / JobUnitInfo::CAPACITY;

}

//���������߲���Ԥ���ڽ���ʱ��֮ǰ������ɶ��ٿ����
double ProductLine::CountToFinishBeforeDeadLine(int deadLine, int realTime, int modelId) {
	if (deadLine < realTime)
		return -1;
	std::map<int/*machine.toIdle*/, int/*machine.index*/> COTable = {};
	std::vector<std::pair<int/*time period*/, double/*capacity*/>> capasityTable = {};
	for (auto& process : (*this)) {
		for (auto& machine : process) {
			if (machine.COTarget == modelId && machine.COTarget != machine.mode && machine.isMachineOnLine()) {
				if (machine.toIdle < realTime)
					COTable[realTime + gShiftMatrixs[machine.name][machine.mode][modelId].first * 60] = machine._index;
				else
					COTable[machine.toIdle + gShiftMatrixs[machine.name][machine.mode][modelId].first * 60] = machine._index;
			}
		}
	}
	ProductLine productLineTemp = (*this);
	int lastTimePoint = realTime;
	double capacity = productLineTemp.getLowestCapacity(modelId);
	double finishedModels = 0;
	int firstFlag = 0;
	for (auto& e : COTable) {
		/*if (firstFlag != 0) {
			finishedModels += (e.first - lastTimePoint) * capacity;
			productLineTemp.machineChangeMode(e.second, modelId);
			capacity = productLineTemp.getLowestCapacity(modelId);
			lastTimePoint = e.first;
		}
		firstFlag = 1;*/

		finishedModels += (e.first - lastTimePoint) * capacity;
		productLineTemp.machineChangeMode(e.second, modelId);
		capacity = productLineTemp.getLowestCapacity(modelId);
		lastTimePoint = e.first;
	}
	finishedModels += (deadLine - lastTimePoint) * capacity;
	return finishedModels / JobUnitInfo::CAPACITY;

}
//��ȡ���������
double ProductLine::getLowestCapacity(int modelId) {
	//FIXME
	double minCapacity = DBL_MAX;
	for (auto& process : gModels[modelId].processes) {
		if ((*this).isGenericProcess(process))
			continue;
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			if (machine.mode == modelId && machine.isMachineOnLine())
				PiecesPerSecond += (1 / (machine.clock(modelId)));
		}
		if (PiecesPerSecond < minCapacity)
			minCapacity = PiecesPerSecond;
	}

	return minCapacity;
}
//��ȡ�������
double ProductLine::getCapacityInProcess(int modelId, int process) {
	double PiecesPerSecond = 0;
	for (auto& machine : (*this)[process]) {
		if ((machine.mode == modelId /*|| machine.COTarget == modelId*/) && machine.getState() == Machine::S_ONLINE)
			PiecesPerSecond += 1 / (machine.clock(modelId));
	}

	return PiecesPerSecond;
}
//��ȡƿ������
std::vector<int> ProductLine::getLowestCapacityProcesses(int modelId) {
	//FIXME
	std::vector<int> minCapacityProcesses = {};
	double minCapacity = DBL_MAX;
	for (auto& process : gModels[modelId].processes) {
		if ((*this).isGenericProcess(process))
			continue;
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			if (/*(*/machine.mode == modelId /*|| machine.COTarget == modelId)*/ && machine.isMachineOnLine())
				PiecesPerSecond += (1 / (machine.clock(modelId)));
		}
		if (PiecesPerSecond < minCapacity)
			minCapacity = PiecesPerSecond;
	}
	for (auto& process : gModels[modelId].processes) {
		if ((*this).isGenericProcess(process))
			continue;
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			if ((machine.mode == modelId || machine.COTarget == modelId) && machine.isMachineOnLine())
				PiecesPerSecond += 1 / (machine.clock(modelId));
		}
		if (PiecesPerSecond == minCapacity)
			minCapacityProcesses.push_back(process);
	}

	return minCapacityProcesses;
}
//�жϸ�̨�����Ƿ��Ǹõ���Ψһһ̨������������Ļ���
bool ProductLine::judgeKeyMachineInProcess(int process, int machineIndex) {
	bool isOnlyMachineInProcess = true;
	bool isModelsLeftBefore = false;
	int mode = gProductLine.machines[machineIndex]->mode;
	//�ж��Ƿ��ǵ���Ψһһ̨����
	for (auto& machine : (*this)[process]) {
		if (machine._index != machineIndex && machine.mode == mode && machine.getState() == Machine::S_ONLINE) {
			isOnlyMachineInProcess = false;
			break;
		}
	}
	//�жϵ�������ǰ�Ƿ���δ�ӹ����
	if (process == 0) {
		for (auto& e1 : jop) {
			if (e1.first == mode) {
				for (auto& e2 : e1.second) {
					if (e2.first <= process && e2.second.size() > 0) {
						isModelsLeftBefore = true;
						break;
					}
				}
				break;
			}
		}
	}
	else {
		for (auto& e1 : jop) {
			if (e1.first == mode) {
				for (auto& e2 : e1.second) {
					if (e2.first == -1)
						continue;
					if (e2.first <= process && e2.second.size() > 0) {
						isModelsLeftBefore = true;
						break;
					}
				}
				break;
			}
		}
	}
    return isOnlyMachineInProcess && isModelsLeftBefore;
}
bool ProductLine::judgePauseMachineInProcess(int process, int machineIndex, int mode) {
	for (auto& machine : (*this)[process]) {
		if (machine.getState() == Machine::S_PAUSE && machine._index != machineIndex && machine.mode == mode) {
			return true;
		}
	}
	return false;
}
#if 0
std::vector<int> ProductLine::getLowestCapacityProcessesUnsensitive(int modelId) {
	//FIXME
	std::vector<int> minCapacityProcesses = {};
	double minCapacity = DBL_MAX;
	double maxCapacity = 0.;
	for (auto& process : gModels[modelId].processes) {
		if ((*this).isGenericProcess(process))
			continue;
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			if (machine.mode == modelId || machine.COTarget == modelId)
				PiecesPerSecond += (1 / (machine.clock(modelId)));
		}
		if (PiecesPerSecond < minCapacity)
			minCapacity = PiecesPerSecond;
		if (PiecesPerSecond > maxCapacity)
			maxCapacity = PiecesPerSecond;
	}
	double medianCapacity = (minCapacity + maxCapacity) / 2;
	for (auto& process : gModels[modelId].processes) {
		if ((*this).isGenericProcess(process))
			continue;
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			if (machine.mode == modelId || machine.COTarget == modelId)
				PiecesPerSecond += 1 / (machine.clock(modelId));
		}
		if (PiecesPerSecond == minCapacity && PiecesPerSecond < medianCapacity /** 0.9*/)
			minCapacityProcesses.push_back(process);
	}

	return minCapacityProcesses;
}

double ProductLine::getLowestCapacityAfterCO(int modelId, int machineIndex) {
	//FIXME
	double minCapacity = DBL_MAX;
	for (auto& process : gModels[modelId].processes) {
		if ((*this).isGenericProcess(process))
			continue;
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			if (machine._index == machineIndex)
				PiecesPerSecond += (1 / (machine.clock(modelId)));
			else if (machine.mode == modelId || machine.COTarget == modelId)
				PiecesPerSecond += (1 / (machine.clock(modelId)));
		}
		if (PiecesPerSecond < minCapacity)
			minCapacity = PiecesPerSecond;
	}

	return minCapacity;
}

double ProductLine::getLowestCapacityAfterCO(int modelId) {
	//FIXME
	double minCapacity = DBL_MAX;
	for (auto& process : gModels[modelId].processes) {
		if ((*this).isGenericProcess(process))
			continue;
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			if (machine.mode == modelId || machine.COTarget == modelId)
				PiecesPerSecond += (1 / (machine.clock(modelId)));
		}
		if (PiecesPerSecond < minCapacity)
			minCapacity = PiecesPerSecond;
	}

	return minCapacity;
}

double ProductLine::getCapacityInProcess(int modelId, int process) {
	double PiecesPerSecond = 0;
	for (auto& machine : (*this)[process]) {
		if (machine.mode == modelId || machine.COTarget == modelId)
			PiecesPerSecond += 1 / (machine.clock(modelId));
	}

	return PiecesPerSecond;
}

int ProductLine::getLowestCapacityProcess(int modelId) {
	double minCapacity = DBL_MAX;
	int lowestCapacityProcess = -1;
	for (auto& process : gModels.get(modelId).processes) {
		//FIXME
		if ((*this).isGenericProcess(process))
			continue;
		//����ÿ������Ĳ���
		double PiecesPerSecond = 0;
		for (auto& machine : (*this)[process]) {
			PiecesPerSecond += 1 / (machine.clock(modelId));
		}
		if (PiecesPerSecond < minCapacity) {
			minCapacity = PiecesPerSecond;
			lowestCapacityProcess = process;
		}
	}
	return lowestCapacityProcess;
}

double ProductLine::spitByCapacity(int machineId, int mode, int leftCountInProcess) {
	int machineProcess = (*this).machines[machineId]->process;
	std::vector<Machine> machinesInProcess = {};
	for (auto& machine : (*this)[machineProcess]) {
		if (mode == machine.mode || mode == machine.COTarget) {
			machinesInProcess.push_back(machine);
		}
	}
	double otherMachineCapacity = 0;
	double machineCapacity = 1 / (*this).machines[machineId]->clock(mode);
	for (auto& machine : machinesInProcess) {
		if (machine._index != machineId) {
			otherMachineCapacity += 1 / machine.clock(mode);
		}
	}
	return leftCountInProcess * machineCapacity / (machineCapacity + otherMachineCapacity);
}

#endif
void ProductLine::setCapacitySurplusMachineOffLine() {
	for (auto& machinesInProcess : *this) {
		for (auto& machine : machinesInProcess) {
			if (machine.generic || machine.getState() == Machine::S_OFFLINE)
				continue;
			std::vector<int> capacityVec = (*this).getLowestCapacityProcesses(machine.mode);
			if (std::count(capacityVec.begin(), capacityVec.end(), machine.process) == 0) {
				ProductLine productLineTemp = (*this);
				for (auto& e : productLineTemp) {
					for (auto& machineTemp : e) {
						if (machineTemp._index == machine._index) {
							machineTemp.mode = -1;
						}
					}
				}
				std::vector<int> capacityVec2 = productLineTemp.getLowestCapacityProcesses(machine.mode);
				if (std::count(capacityVec2.begin(), capacityVec2.end(), machine.process) == 0) {
					//machine.setState(Machine::S_PAUSE);
//					std::cout << "machine_index:" << machine._index << "\n";
				}
			}
		}
	}
}
bool ProductLine::judgeCapacityOverLastProcess(int machineProcess, int mode) {
	int processIndex = 0;
	for (int index = 0; index < gModels[mode].processes.size(); index++) {
		if (gModels[mode].processes[index] == machineProcess) {
			processIndex = index - 1;
			bool isProcessGeneric = gProductLine.isGenericProcess(processIndex);
			if (isProcessGeneric)
				processIndex--;
			break;
		}
	}
	if (processIndex < 0)
		return false;
	int lastProcess = gModels[mode].processes[processIndex];
	int capacityInProcess = gProductLine.getCapacityInProcess(mode, machineProcess);
	int capacityInLastProcess = gProductLine.getCapacityInProcess(mode, lastProcess);
    return capacityInProcess > capacityInLastProcess;
}

void fits::JobsQueue::initialize(Configuration& cfg) {
	std::vector<JobUnit> jobsTempBuffer = {};
	while (!(*this).empty()) {
		auto eTemp = (*this).top();
		(*this).pop();
		int flag = 0;
		//FIXME:����д��
		for (auto e : cfg[0]) {
			if (eTemp.model == e.first) {
				flag = 1;
				break;
			}
		}
		if (flag == 1)
			jobsTempBuffer.push_back(eTemp);
	}
	for (auto eTemp : jobsTempBuffer)
		(*this).push(eTemp);
}
fits::JobsQueue jobs;

//@cfg: ��������
#if 0
inline double
calculateTimecostForConfiguration(Configuration& cfg) {
	//ǿ���޸Ĳ�����Ļ��ͣ�����������ʱ��ķ���
	//cfg.modifyUnresonable();
	//cfg.correctRedundantCO();

	//toIdle�ĳ�ʼ��
	cfg.refreshCfgMachinesModeAndToIdle(gProductLine);
	//��ʱ����һ��jobsTemp����ģ��
	JobsQueue jobsTemp = {};
	if (jop.empty())
		jobsTemp = jobs;
	for (auto& e1 : jop) {
		for (auto& e2 : e1.second) {
			for (auto& jobsUnit : e2.second) {
				jobsTemp.push(jobsUnit.second);
			}
		}
	}
	//����һ������ʱ����ǰ�����ѳ�ʼ�����պ���ȫ��pop������Լ����ʱ��
	jobsTemp.initialize(cfg);
	double result = 0.;
	gCountOfChangeOvers = 0;
	/// ��ʼģ��
	while (!jobsTemp.empty()) {    ///�������յ�����Ƴ�
		///�ҵ���С��releaseTime��Ӧ��JobUnit
		auto e = jobsTemp.top();
		const int FINAL = (int)gModels[e.model].processes.size();
		if (e.ioo < FINAL - 1) e.ioo += 1;	///���������������ÿ�����򣬸���������ȡ�¸�����
		int no = gModels[e.model].processes[e.ioo];
		jobsTemp.pop();
		///���ÿ���������������idle��С�Ļ���
		auto& machines = cfg[no][e.model];
		///�豸��˳����cfg�ṹ�йأ�����sort����ı�˳��
		auto& m = *std::min_element(machines.begin(), machines.end(), [](const PMachine m1, const PMachine m2) { return m1->toIdle < m2->toIdle; });
		///add mode switch time
		//���ҵ��Ļ���������ͺŲ�ͬ�����͵�ʱ��������ڸû��������Ŷ�
		//���ͬ�����ж�������ӹ�һ�����������֣�Ӧ���ж���������Ƿ��Ҫ���ͣ�����ǵĻ���Ӧ���١��ȵȡ�
		if (machines.size() > 1 && m->mode != e.model) {
			int isLeftModelNumTooSmall = 0;
			std::map<int, int> testModelCount = {};
			std::vector<JobUnit> temp = {};
			while (!jobsTemp.empty()) {
				JobUnit eTemp = jobsTemp.top();
				jobsTemp.pop();
				temp.push_back(eTemp);
				testModelCount[eTemp.model]++;
			}
			for (auto& eTemp : temp)
				jobsTemp.push(eTemp);
			temp = {};
			for (auto& eTestNum : testModelCount) {
				if (eTestNum.second < 10) {
					isLeftModelNumTooSmall = 1;
					break;
				}
			}
			if (isLeftModelNumTooSmall == 1) {
				int abandonInex = m->_index;
				int machineToIdle = std::numeric_limits<int>::max();
				for (auto& machine : machines) {
					if (machine->toIdle < machineToIdle && machine->_index != abandonInex) {
						m = machine;
					}
				}
			}
		}
		if (m->mode != e.model && !m->generic) {
			gCountOfChangeOvers++;
			auto cost = gShiftMatrixs[m->name][m->mode][e.model].first * 60;
			m->toIdle += cost;
			m->mode = e.model;
			e.ioo--;
			jobsTemp.push(e);
			continue;
		}
		///����i�����(e)�ָ���j̨����(m)
		///����i.release_time <= j.idle_time
		if (e.releaseTime < m->toIdle) {
			e.releaseTime = m->toIdle = m->toIdle + e.COT(m->clock(e.model));		///COT������ǰ����ã�����Ҫÿ�ζ�����48
		}
		else {
			e.releaseTime = m->toIdle = e.releaseTime + e.COT(m->clock(e.model));
		}
		e.process = m->process;
		if (e.ioo == FINAL - 1) ///���һ����
		{
			result = std::max(e.releaseTime, result);
		}
		else {
			jobsTemp.push(e);
		}
	}

	return result;
}
#endif
#if 1
inline std::pair<double/*time*/, int/*co*/>
evaluateTimeCostByCapacity(Configuration& cfg) {
	/// �������Ϊ��ͬ��ʱ��� ��ͬ��ʱ��β�ͬ��cfg
	std::map<int/*?Q��ʱ���*/, std::vector<std::pair<PMachine/*?Q�ͻ���*/, int/*?Q���ͺ�*/>>> COTableWithTimePointForMachine = {};
	std::map<int/*?Q��ʱ���*/, double/*����*/> COTableWithTimePointForCfgShadow = {};
	std::vector<std::pair<double/*?Q��ʱ���*/, double/*����*/>> COTableWithTimePeriodForCfgShadow = {};
	std::map<int/*mode*/, double/*timePoint*/> timeIndex = {};
	std::map<int/*mode*/, double/*count*/> tasksIndex = {};
	double endCOTime = 0.;
	//ǿ���޸Ĳ�����Ļ��ͣ�����������ʱ��ķ���
	//cfg.modifyUnresonable();
	//cfg.correctRedundantCO();

	//toIdle�ĳ�ʼ��
	cfg.refreshCfgMachinesModeAndToIdle(gProductLine);
	//��ʱ����һ��jobsTemp����ģ��
	JobsQueue jobsTemp = {};
	if (jop.empty())
		jobsTemp = jobs;
	for (auto& e1 : jop) {
		for (auto& e2 : e1.second) {
			for (auto& jobsUnit : e2.second) {
				jobsTemp.push(jobsUnit.second);
			}
		}
	}
	//jobsTempToTasks
	while (!jobsTemp.empty()) {
		auto& eTemp = jobsTemp.top();
		if (cfg[0].count(eTemp.model) > 0) {
			tasksIndex[eTemp.model] += 1;
		}
		jobsTemp.pop();
	}
	//����һ������ʱ����ǰ�����ѳ�ʼ�����պ���ȫ��pop������Լ����ʱ��
	int countOfChangeOvers = 0;

	ConfigurationShadow cfgShadow = {};
	for (auto& e1 : cfg) {
		for (auto& e2 : e1.second) {
			for (auto& machine : e2.second) {
				int modeBefore = machine->mode;
				int modeAfter = e2.first;
				if (modeBefore != modeAfter) {
					int COT = gShiftMatrixs[machine->name][modeBefore][modeAfter].first * 60;
					COTableWithTimePointForMachine[COT].push_back({ machine,modeAfter });
				}
			}
		}
	}
	countOfChangeOvers = (int)COTableWithTimePointForMachine.size();

	double maxTime = 0.;
	for (auto& taskIndex : tasksIndex) {
		COTableWithTimePointForCfgShadow[0] = cfg.getCapacityBymodelId(taskIndex.first);
		for (auto& cos : COTableWithTimePointForMachine) {
			for (auto& co : cos.second) {
				co.first->mode = co.second;
			}
			//��¼״̬
			COTableWithTimePointForCfgShadow[cos.first] = cfg.getCapacityBymodelId(taskIndex.first);
		}
		/*for (auto it = COTableWithTimePointForCfgShadow.begin();it != COTableWithTimePointForCfgShadow.end();it++) {
			it++;
			if (it == COTableWithTimePointForCfgShadow.end()) {
				it--;
				endCOTime = (*it).first;
				break;
			}
			double timePeriodEnd = (*it).first;
			it--;
			double timePeriodBegin = (*it).first;
			double timePeriod = timePeriodEnd - timePeriodBegin;
			COTableWithTimePeriodForCfgShadow.push_back({ timePeriod ,(*it).second });
		}*/
		double startTime = 0;
		double timePeriod = 0;
		double capacity = 0;
		for (auto& e : COTableWithTimePointForCfgShadow) {
			timePeriod = e.first - startTime;
			if (timePeriod == 0) {
				capacity = e.second;
				continue;
			}
			else {
				COTableWithTimePeriodForCfgShadow.emplace_back( timePeriod ,capacity );
				double countDec = timePeriod * capacity;
				taskIndex.second -= countDec;
			}
			capacity = e.second;
		}
		double finalCapacity = capacity;
		double timeLeft = finalCapacity == 0 ? DBL_MAX : taskIndex.second * JobUnit::CAPACITY / finalCapacity;
		timeLeft < 0 ? 0 : timeLeft;
		double finalTime = timeLeft + endCOTime;
		if (finalTime > maxTime)
			maxTime = finalTime;
	}
	return { maxTime,countOfChangeOvers };
}
#endif

#if 1
inline int
refresh(int simulatorRealTime, std::map<int/*modelId*/, std::map<int /*process*/, int/*minIndex*/ >>& minDayIndexEachModel) {
	GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
	auto ptm = TasksManager::getTasksManager();
	/// ����gTasks /// ����jop��ͳ��model������
	std::map<int, int> counting = {};
	for (auto& e1 : jop) {///model
		auto model = e1.first;
		for (auto& e2 : e1.second) {///process
			auto& m = e2.second;
			if (!m.empty()) {
				counting[model] += (int)m.size();
			}
		}

		auto it = std::find_if(gTasks.begin(), gTasks.end(), [&](Task& t) { return t.job.model == model; });
		if (it != gTasks.end()) {
			it->count = counting[model];
			if (it->count == 0) {
				it = gTasks.erase(it);
			}
		}
	}

#if 0
	int indexOfDay = (int)simulatorRealTime / 86400;

	///���ܶ���̫��
	if (!jobs.empty()
		&& gTasks.size() > 1
		&& ptm->getProgress() - 1 > indexOfDay) {
		return 0;
	}

	int count = (ptm->getProgress() == 0 ? 2 : 1);	///init load need 2 days

	int i = 0;
	for (; i < count
		|| gTasks.size() < 2
		|| jobs.empty()
		; i++) {
		auto state = ptm->fill(jobs, [&](int i, RequirementTasks& daily) {
			///update gTasks
			for (auto& e : daily) {
				auto it = std::find_if(gTasks.begin(), gTasks.end(), [&](Task& t) { return t.job.model == e.job.model; });
				if (it != gTasks.end()) {
					it->count += e.count;
					///update deadline
					it->deadline = e.deadline;
				}
				else gTasks.push_back(e);
			}
			Utils::log(1, "Loaded day %s to gTasks(jobs)! \n", daily.empty() ? "NaD" : daily.front().date);
			});

		if (!state) break;
	}
	Utils::log(1, "Loaded %d days to gTasks(jobs)! \n", i);
	Utils::log(1, "gTasks count : %d before compact! ", gTasks.size());
	for (auto it = gTasks.begin(); it != gTasks.end();) {
		if (it->count <= 0) {
			it = gTasks.erase(it);
		}
		else {
			it++;
		}
	}
	Utils::log(1, ", %d after compact! \n", gTasks.size());
#endif

	/// ��֤gTasks�������������
	std::map<std::string/*date*/, int> countOfDays = {};
	RequirementTasks tasks = gTasks;
	int minOfIOD = std::numeric_limits<int>::max();
if(simulatorRealTime==409594)
    int a;
	do {
		for (auto it = tasks.begin(); it != tasks.end(); (it->count <= 0) ? it = tasks.erase(it) : it++) {}
		/*for (auto& e : tasks) {
			minOfIOD = std::min(e.IOD, minOfIOD);
		}

		RequirementTasks* front = ptm->front();*/
		//test
		int isFirstProcessMachinesIdle = 0;
		for (auto& machine : gProductLine[0]) {
			if (machine.status == Machine::MS_IDLE && std::abs(machine.toIdle - simulatorRealTime) > gContext.offset) {
				isFirstProcessMachinesIdle = 1;
			}
		}
		//����jop ���ƴ�����
		minDayIndexEachModel = {};
		/*voicend
		for (int i = 0; i < gModels.size(); i++) {
			for (int j = 0; j < gModels[i].processes.size(); j++)
				minDayIndexEachModel[i][gModels[i].processes[j]] = std::numeric_limits<int>::max();
		}*/
        for (auto i : gModels) {
            for (int j = 0; j < i.processes.size(); j++)
                minDayIndexEachModel[i.id][i.processes[j]] = std::numeric_limits<int>::max();
        }
		std::map<int, int> testDayWindow = {};
		for (auto& e1 : jop) {
			for (auto& e2 : e1.second) {
				for (auto& e3 : e2.second) {
					int minIndex = (std::min)(minDayIndexEachModel[e3.second.model][e3.second.process], e3.second.daysIndex);
					minDayIndexEachModel[e3.second.model][e3.second.process] = minIndex;
					testDayWindow[e3.second.daysIndex] = 1;
				}
			}
		}
		int minIndex = std::numeric_limits<int>::max();
		int maxIndex = 0;
		for (auto& e1 : testDayWindow) {
			minIndex = (std::min)(e1.first, minIndex);
			maxIndex = (std::max)(e1.first, maxIndex);
		}
		if (((maxIndex - minIndex >= gContext.windowPhase) && (isFirstProcessMachinesIdle != 0)) || (maxIndex - minIndex >= gContext.windowMaxPreFetchDays)) {
			break;
		}
		/*if (front && (((*front)[0].IOD - minOfIOD) > 2) && isFirstProcessMachinesIdle == 0) {
			break;
		}*/

		auto state = ptm->fill(jobs, [&](int i, RequirementTasks& daily) {
			///update gTasks
			for (auto& e : daily) {
				tasks.push_back(e);
			}
			//update jop
			auto jobsTemp = jobs;
			const size_t TOTAL = jobsTemp.size();
			for (size_t i = 0; i < TOTAL; i++) {
				auto& e = jobsTemp.top();
				int p = e.ioo < 0 ? -1 : gModels[e.model].processes[e.ioo];
				jop[e.model][p][e.uid] = e;
				jobsTemp.pop();
			}
			Utils::log(1, "Loaded day %s to gTasks(jobs)! \n", daily.empty() ? "NaD" : daily.front().date.data());
			});

		for (auto it = tasks.begin(); it != tasks.end(); (it->count <= 0) ? it = tasks.erase(it) : it++) {}
		for (auto& e : tasks) {
			countOfDays[e.date] += 1;
		}

		if (!state) {
			//Utils::log(0, "All passed !! ^_^\n");
			break;
		}
	} while (tasks.size() != 0);

	for (auto& e : tasks) {
		auto it = std::find_if(gTasks.begin(), gTasks.end(), [&](Task& t) { return t.job.model == e.job.model; });
		if (it != gTasks.end()) {
			it->count += e.count;
			///update deadline
			it->deadline = std::max(e.deadline, it->deadline);
		}
		else gTasks.push_back(e);
	}

	return 0;
}
#endif
#if 1
void fits::MachineAssignerForProductLine::assign(int w/*����*/, int z, int x, std::vector<PMachine> y) {

	//����Ҫ�ĵ�������
	auto& orders = gModels[tasks[x].job.model].processes;
	bool skip = (std::count(orders.begin(), orders.end(), w) <= 0);
	if (x < static_cast<int>(tasks.size())) {
		auto& orders = gModels[tasks[x].job.model].processes;
		skip = (std::count(orders.begin(), orders.end(), w) <= 0);
	}

	if (x == static_cast<int>(tasks.size() - 1)) // if(x=k)then
	{

		if (!skip) {
			///�����ֳ�����ʣ��y���������x�����
			///����������x����ĳ����ŷ���
			std::vector<PMachine> matched;
			for (auto candidate : y)
				if (candidate->support(tasks[x].job.model))
					matched.push_back(candidate);

			assignment[w][x][z] = matched;
		}
		//std::cout << "+assign o:" << w << " model:" << x << " m:" << z << " c:" << y.size() << "\n";
		if (z == A[w].size() - 1) //if(z=n)
		{


			///����Ƿ�ÿһ������ܹ����ٷ��䵽��һ̨����
			bool good = isAllModelSatisfiedForOrderN(w);
			///���'��'����һ����Ч�ķ���,������һ����Ч�ķ���
			if (good) //if 'Yes' then
			{
				///if w�������һ������ then
				if (w < static_cast<int>(A.size() - 1)) {
					assignment[w + 1] = {};
					assign(w + 1, 0, 0, A[w + 1][0]);
				}
				else {
					///�����ڸ������µ�������ʱ��
					Configuration configuration(assignment, *pProductLine);    //assignmentת��ΪConfiguration
					//���ӶԶ��໻�͵Ĵ���
					//��ʱ���һ��������������÷���
					int cot = evaluateTimeCostByCapacity(configuration).first;
					int COCount = evaluateTimeCostByCapacity(configuration).second;
					//MOD
					/*std::ofstream os(outputfilename_test,std::ios::app);
					os << cot << "," << count_changeType << std::endl;*/
					//MOD
					//if (cot < bestTimeCost) {

					if ((cot < bestTimeCost/* - 60.*/) || (std::fabs(cot - bestTimeCost) < 5. /*300.*/ && COCount < countChangeType)) {
						bestConfiguration = {};
						for (auto& e1 : configuration) {
							for (auto& e2 : e1.second) {
								for (auto& e3 : e2.second) {
									bestConfiguration[e1.first][e2.first].push_back(*e3);
								}
							}
						}
						/// save assignment into best!
						best = assignment;
						bestTimeCost = cot;
						countChangeType = COCount;
						gProductLine.snapshot(bestSnapshotOfMachines);
					}
					gProductLine.restore(snapshotOfMachines);

				}
			}
			else
			{
				//std::cout << "discard\n";
			}
		}
		else
		{
			assignment[w][z + 1] = {};
			assign(w, z + 1, 0, A[w][z + 1]);
		}
	}
	else
	{
		if (skip) {
			assign(w, z, x + 1, y);	///����ȫ������һ�����
		}
		else {
			///�Ȱ�ͨ���豸�����
			auto genericed = std::vector<PMachine>(), ungenericed = std::vector<PMachine>();
			///genericedΪtrue ���Ǹû�����ͨ�õ�
			for (auto m : y) m->generic ? genericed.push_back(m) : ungenericed.push_back(m);

			for (int i = 0; i < static_cast<int>(ungenericed.size() + 1); i++) //for i=0 to y
			{
				///�����ֳ�����į�������x�����
				///+����������x����ĳ����ŷ���
				std::vector<PMachine> matched;
				for (auto candidate : std::vector<PMachine>(ungenericed.begin(), ungenericed.begin() + i)) if (candidate->support(tasks[x].job.model)) matched.push_back(candidate);
				if (!genericed.empty()) matched.insert(matched.end(), genericed.begin(), genericed.end());
				auto r = assignment[w][x][z] = matched;

				auto elsed = std::vector<PMachine>(ungenericed.begin() + i, ungenericed.end());
				if (!genericed.empty()) elsed.insert(elsed.end(), genericed.begin(), genericed.end());
				assign(w, z, x + 1, elsed);
			}
		}
	}
}
#endif


/// TODO: ����DailyTasksLoader
inline static int parseRequirement2(
	const std::string filenameOfRequirements
) {
	std::ifstream is(filenameOfRequirements);
	if (!is) {
		Utils::log(1, "failed to open requirements file: %s\n", filenameOfRequirements.data());
		return -1;
	}

	std::vector<std::string> days;
	std::vector<char> buffer(1024, 0);
	is.getline(buffer.data(), buffer.capacity());
	std::string line(buffer.data());
	auto pos = std::string::npos;
	std::regex reg("\\d{4}/\\d{1,2}/\\d{1,2}");
	while (std::string::npos != (pos = line.find_first_of(','))) {
		std::string day = line.substr(0, pos);
		line = line.substr(pos + 1);
		if (day.size() > 0 && std::regex_match(day, reg)) days.push_back(day);
	}
	if (line.size() > 0) days.push_back(line);

	///MODEL NAME, COUNT
	std::map<std::string, std::vector<int>> mapping;
	while (!is.eof()) {
		std::memset(buffer.data(), 0, buffer.capacity());
		is.getline(buffer.data(), buffer.capacity());
		if (buffer.at(0) == '#') continue;

		std::string n;
		std::string line(buffer.data());
		while (std::string::npos != (pos = line.find_first_of(','))) {
			std::string e = line.substr(0, pos);
			e = Utils::trim(e);
			if (n.empty()) {
				n = e;
				mapping[e] = {};
			}
			else {
				mapping[n].push_back(std::atoi(e.c_str()));
			}
			line = line.substr(pos + 1);
		}
		if (line.size() > 0) mapping[n].push_back(std::atoi(line.c_str()));
	}

	///fill daysTasks
#if 0
	//��ȡ��ǰʱ�䣬��������ʱ��С�ڵ�ǰʱ������������԰���
	time_t testTimestamp = time(NULL);
	struct tm timeNow;
	localtime_s(&timeNow, &testTimestamp);
	int yearNow = timeNow.tm_year + 1900;
	int monthNow = timeNow.tm_mon + 1;
	int dayNow = timeNow.tm_mday;
#endif

	for (int i = 0; i < days.size(); i++) {
		std::string d = days.at(i);
		//������ʱ����н��� �ȵ�ǰʱ��ҪС�Ļ� �Ͳ�����ΪӦ�ô��������
#if 0
		std::regex re("(\\d+)(\\D+)(\\d+)(\\D+)(\\d+)");
		std::smatch s2;
		std::regex_search(d, s2, re);
		int yearForModel = std::stoi(s2[1]);
		int monthForModel = std::stoi(s2[3]);
		int dayForModel = std::stoi(s2[5]);
		int isTimeOlderThanTimeNow = 0;
		if (yearForModel < yearNow)
			isTimeOlderThanTimeNow = 1;
		else if (yearForModel == yearNow && monthForModel < monthNow)
			isTimeOlderThanTimeNow = 1;
		else if (monthForModel == monthNow && dayForModel < dayNow)
			isTimeOlderThanTimeNow = 1;
		if (isTimeOlderThanTimeNow == 1) {
			Utils::log(1, "skip task of %s\n", d.data());
			continue;
		}
#endif
		std::replace(d.begin(), d.end(), '\r', '\0');
		RequirementTasks tasks = {};
		for (auto& e : mapping) {
			int id = gModels.get(e.first).id;
			if (id < 0) {
				Utils::log(2, "un-supported model: [%s]\n", e.first.data());
				continue;
			}
			int cnt = 0;
			if (e.second.size() > i) {
				cnt = e.second.at(i) / JobUnit::CAPACITY + ((e.second.at(i) % JobUnit::CAPACITY) > 0 ? 1 : 0);
			}
			Task t(id, cnt, d);
			t.IOD = i;
			GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
			t.deadline = (i + gContext.windowPhase - 1) * gContext.workingTimePerDay;	///FIXME: �ż���ô�죿
			tasks.push_back(t);
		}

		TasksManager::getTasksManager()->put(d, tasks);
	}

	return 0;
}

///@matrix��[out] typedef std::map<int/*i������*/, std::map<int/*j�ֳ���*/, std::vector<PMachine>/*�������б�*/>>;
void
makeMachineMatrixFromProductLine(MachineMatrix& matrix, ProductLine& line) {
	int o = 0;  ///�ڼ�����
	for (auto& process : line) {
		std::map<std::string, std::vector<PMachine>> mapOfKinds;
		for (auto& machine : process) {
			if (machine.avaliable())
				mapOfKinds[machine.name].push_back(&machine);
		}
		std::map<int/*j�ֳ���*/, std::vector<PMachine>/*�������б�*/> mapOfMachineList;
		int j = 0;  ///��j�ֳ���
		for (auto p : mapOfKinds) {
			mapOfMachineList[j++] = p.second;
		}
		matrix[o] = mapOfMachineList;
		o++;
	}
};

static
inline void printExecutionDetails(time_t t0) {
	//��������ʱ��
	Utils::log(1, "all passed!    total: %d sec!\n", std::time(nullptr) - t0);
	//std::cout << "total: " << std::time(nullptr) - t0 << " sec!\n";
}
std::pair<int, double> getCOMode(int triggerIndex, int realTime, std::map<int/*modelId*/, std::map<int /*process*/, int/*minIndex*/ >>& minDayIndexEachModel) {
	if (gProductLine.machines[triggerIndex]->generic)
		return { -1, -1. };
	/*��gTasks�е������Ϊδ�ӹ������ӹ������ڼӹ�
	δ�ӹ�����������û��һ̨�ʺϸ�����Ļ���
	���ӹ�����������������һ̨�ʺϸ�����Ļ����Ҹ�����������߲�û���γ�ͨ·
	���ڼӹ�������������һ���������ͨ·
	*/
	std::vector<std::pair<int/*model*/, std::map<int/*process*/, int/*1 or 0*/>>> waitInLine = {};//���ӹ�
	std::vector<std::pair<int/*model*/, std::map<int/*process*/, int/*1 or 0*/>>> offLine = {};//δ�ӹ�
	std::vector<std::pair<int/*model*/, std::map<int/*process*/, int/*1 or 0*/>>> onLine = {};//���ڼӹ�
	int machineProcess = gProductLine.machines[triggerIndex]->process;
	std::map<int/*model*/, int/*process*/> left = {};
	int originalMode = gProductLine.machines[triggerIndex]->mode;

	////��������������ÿ�ζ�ѭ�����������������е������������
	//std::map<int /*modelId*/,std::map<int /*maxDayIndex*/, std::vector<double> /*vec[0] ������ܣ��������� vec[0] ������ܣ������Σ� */>> modelInfoMap = {};
	//for (auto& model : gTasks) {
	//	int minDayIndex = minDayIndexEachModel[model.job.model][machineProcess];
	//	int requiredCountForEarliestForToMode = 0;
	//	int requiredCountForAllForToMode = 0;
	//	int maxIndexForToMode = 0;
	//	for (auto& e : jop[model.job.model]) {
	//		if (e.first > machineProcess)
	//			break;
	//		for (auto& modelMap : e.second) {
	//			if (modelMap.second.daysIndex == minDayIndex /*|| modelMap.second.daysIndex == minDayIndex + 1 || modelMap.second.daysIndex == minDayIndex + 2*/) {
	//				requiredCountForEarliestForToMode++;
	//			}
	//			if (modelMap.second.daysIndex > maxIndexForToMode)
	//				maxIndexForToMode = modelMap.second.daysIndex;
	//			requiredCountForAllForToMode++;
	//		}
	//	}
	//	modelInfoMap[model.job.model][maxIndexForToMode].push_back(requiredCountForEarliestForToMode);
	//	modelInfoMap[model.job.model][maxIndexForToMode].push_back(requiredCountForAllForToMode);
	//}


	//�Ƚ���ͬ����ֳɴ��ӹ���δ�ӹ������ڼӹ�����
	for (auto& task : gTasks) {
		int mode = task.job.model;
		/*�������*/
		std::map<int/*process*/, int/*1 or 0*/> testMap = {};
		for (auto& i : gModels[mode].processes) {
			if (gProductLine.isGenericProcess(i)) {
				testMap[i] = 2;
				continue;
			}
			for (auto& machine : gProductLine[i]) {
				if (((machine.mode == mode && machine.status != Machine::MS_CO) || (machine.COTarget == mode && machine.status == Machine::MS_CO)) && machine.isMachineOnLine()) {
					testMap[i] = 1;
					break;
				}
				testMap[i] = 0;
			}
		}

		for (auto& e : jop) {
			int i = -1;
			for (; i < 10 && jop[e.first][i].empty(); i++) {}
			left[e.first] = i;
			//��left[e.first]=10,������Ѿ���ȫ�뿪�������ˣ������ֵ���д���
			if (left[e.first] == 10)
				left[e.first] = -2;
		}
		int clearProcessCount = 0;
		int genericProcessCount = 0;
		int firstObstructedProcess = -1;
		int passingProcessCount = 0;
		for (auto& e : testMap) {
			if (e.second == 1) {
				clearProcessCount++;
			}
			if (e.second == 2) {
				genericProcessCount++;
			}

			else if (firstObstructedProcess == -1 && !gProductLine.isGenericProcess(e.first)) {
				firstObstructedProcess = e.first;
			}
			/*	else if (!gProductLine.isGenericProcess(e.first)
					&& count(gModels[mode].processes.begin(), gModels[mode].processes.end(), e.first) > 0 && e.first > firstObstructedProcess) {
					firstObstructedProcess = e.first;
				}*/
		}
		passingProcessCount = clearProcessCount + genericProcessCount;
		if (clearProcessCount == 0) {
			offLine.push_back({ mode,testMap });
		}
		//����������������˵�һ���򣬴�ʱֻҪ���漸����������Ӧ������������������ڼӹ�
		else if (passingProcessCount != gModels[mode].processes.size()
			&& left[mode] <= firstObstructedProcess) {
			waitInLine.push_back({ mode,testMap });
		}
		else {
			onLine.push_back({ mode,testMap });
		}
	}
	int finalMode = -1;
	double bestOffset = -1.;
	/*
	���ȼ�˳�����ȿ��Ƿ�?Q�ͳɴ��ӹ����ٿ�δ�ӹ���������ڼӹ������
	*/
	/*
	����д��ӹ���������û����Ƿ��ڿ�ȱ������
	����ж�������ѡ����ѡ��?Q��ʱ�����ٵ�
	*/
	if (waitInLine.size() != 0) {
		int modeChoose = -1;
		double minCOTime = DBL_MAX;
		for (auto& e1 : waitInLine) {
			double COtime = gShiftMatrixs[gProductLine.machines[triggerIndex]->name][gProductLine.machines[triggerIndex]->mode][e1.first].first;
			if (e1.second[machineProcess] == 0 && machineProcess >= left[e1.first] && COtime < minCOTime) {
				modeChoose = e1.first;
				minCOTime = COtime;
			}
		}
		finalMode = modeChoose;
	}
	/*
	�����δ�ӹ������ֻ�ڵ�һ������
	���ж���δ�ӹ���ѡ��?Q��ʱ���ٵ�
	*/
	if (offLine.size() != 0 && finalMode == -1 && machineProcess == 0) {
		int modeChoose = -1;
		double minCOTime = DBL_MAX;
		for (auto& e1 : offLine) {
			double COtime = gShiftMatrixs[gProductLine.machines[triggerIndex]->name][gProductLine.machines[triggerIndex]->mode][e1.first].first;
			if (e1.second[machineProcess] == 0 && COtime < minCOTime) {
				modeChoose = e1.first;
				minCOTime = COtime;
			}
		}
		finalMode = modeChoose;
	}
	/*
	��������ڼӹ��������������Ƿ����ڽ���ʱ��֮ǰ��ɣ�
	�����ܣ�������?Q�ͺ��ܷ����
	���ж��������ѡ����������������
	*/
	else if (onLine.size() != 0 && finalMode == -1) {
		int modeChoose = -1;
		int maxCount = 0;
		//����?��?index,��ʱ������������
		std::map<int/*modelId*/, int/*modelCount*/> tasksIndex = {};
		for (auto& e0 : gTasks) {
			tasksIndex[e0.job.model] = e0.count;
		}
		double minActualDivDemand = DBL_MAX;
		//FIXME
		for (auto& e1 : onLine) {
			//�������Ѿ�ͨ���˸õ��򣬻��߻��������������������������迼��
			if (machineProcess < left[e1.first] || (gProductLine.machines[triggerIndex]->mode == e1.first && gProductLine.machines[triggerIndex]->getState() == Machine::S_ONLINE))
				continue;
			//ƿ�����򼯺ϣ��������ΪĿǰ������ȫ��?Q�ͽ������״̬�Ĳ���
			std::vector<int> capacityVec = gProductLine.getLowestCapacityProcesses(e1.first);
			bool isInTime = true;
			GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
			//FIXME
			int minDayIndex = minDayIndexEachModel[e1.first][machineProcess];
			int requiredCountForEarliestForToMode = 0;
			int requiredCountForAllForToMode = 0;
			int requiredCountForAllForFromMode = 0;
			int maxIndexForToMode = 0;
			int maxIndexForFromMode = 0;
			for (auto& e : jop[e1.first]) {
				if (e.first > machineProcess)
					break;
				for (auto& modelMap : e.second) {
					if (modelMap.second.daysIndex == minDayIndex /*|| modelMap.second.daysIndex == minDayIndex + 1 || modelMap.second.daysIndex == minDayIndex + 2*/) {
						requiredCountForEarliestForToMode++;
					}
					if (modelMap.second.daysIndex > maxIndexForToMode)
						maxIndexForToMode = modelMap.second.daysIndex;
					requiredCountForAllForToMode++;
				}
			}
			for (auto& e : jop[originalMode]) {
				if (e.first > machineProcess)
					break;
				for (auto& modelMap : e.second) {
					if (modelMap.second.daysIndex > maxIndexForFromMode)
						maxIndexForFromMode = modelMap.second.daysIndex;
					requiredCountForAllForFromMode++;
				}
			}

			int minDeadLineForEarlistForToMode = gContext.windowMaxDelaySeconds + minDayIndex * gContext.workingTimePerDay;
			int minDeadLineForAllForToMode = gContext.windowMaxDelaySeconds + maxIndexForToMode * gContext.workingTimePerDay;
			int minDeadLineForEarlistForFromMode = gContext.windowMaxDelaySeconds + maxIndexForFromMode * gContext.workingTimePerDay;
			//���������ڽ���֮ǰ�������������ٿ����
			double CountToFinishBeforeDeadLineForEarliestForToMode = gProductLine.CountToFinishBeforeDeadLine(minDeadLineForEarlistForToMode, realTime, e1.first);
			double CountToFinishBeforeDeadLineForAllforToMode = gProductLine.CountToFinishBeforeDeadLine(minDeadLineForAllForToMode, realTime, e1.first);
			double CountToFinishBeforeDeadLineForAllforFromMode = gProductLine.CountToFinishBeforeDeadLine(minDeadLineForEarlistForFromMode, realTime, originalMode);

			//����֮��Ĳ���Ԥ��
			ProductLine productLineTemp = gProductLine;
			for (auto& process : productLineTemp) {
				for (auto& machine : process) {
					if (machine._index == triggerIndex) {
						machine.COTarget = e1.first;
						machine.setState(Machine::S_ONLINE);
					}
				}
			}
			double CountToFinishBeforeDeadLineForAllForToModeAfterCO = productLineTemp.CountToFinishBeforeDeadLine(minDeadLineForAllForToMode, realTime, e1.first);
			double CountToFinishBeforeDeadLineForAllForTFromModeAfterCO = productLineTemp.CountToFinishBeforeDeadLine(minDeadLineForEarlistForFromMode, realTime, originalMode);

			/*����֮ǰĿ����ܵ�ƫ�ưٷֱ�*/
			double offsetPercentageForToModeBeforeCO = std::fabs(CountToFinishBeforeDeadLineForAllforToMode - requiredCountForAllForToMode) / requiredCountForAllForToMode;
			double offsetPercentageForFromModeBeforeCo = std::fabs(CountToFinishBeforeDeadLineForAllforFromMode - requiredCountForAllForFromMode) / requiredCountForAllForFromMode;
			/*����֮��Ŀ����ܵ�ƫ�ưٷֱ�*/
			double offsetPercentageForToModeAfterCO = std::fabs(CountToFinishBeforeDeadLineForAllForToModeAfterCO - requiredCountForAllForToMode) / requiredCountForAllForToMode;
			double offsetPercentageForFromModeAfterCo = std::fabs(CountToFinishBeforeDeadLineForAllForTFromModeAfterCO - requiredCountForAllForFromMode) / requiredCountForAllForFromMode;
			double diffPercentage = offsetPercentageForToModeAfterCO + offsetPercentageForFromModeAfterCo - (offsetPercentageForToModeBeforeCO + offsetPercentageForFromModeBeforeCo);
			//���罻������������������
			if (requiredCountForEarliestForToMode * gContext.coSusceptibility > CountToFinishBeforeDeadLineForEarliestForToMode ||
				requiredCountForAllForToMode * gContext.coSusceptibility > CountToFinishBeforeDeadLineForAllforToMode
				) {
				isInTime = false;
			}
			if (std::count(capacityVec.begin(), capacityVec.end(), machineProcess) > 0 && !isInTime) {
				if (tasksIndex[e1.first] > maxCount) {
					maxCount = tasksIndex[e1.first];
					modeChoose = e1.first;
					bestOffset = diffPercentage;
				}


			}

		}
		finalMode = modeChoose;
	}

	return { finalMode,bestOffset };
}

//�÷�����ΪgetCOMode�Ĳ��䣬�ܽ�һ�����ڶ���������ų����ȼ�������֮ǰ��������ѭ������һ����������?Q�;Ͳ��ٹ����������Ƿ������
std::pair<int /*machineindex*/, int /*COMode*/> getMostSuitbleCOMachineInMachines(std::vector<int> triggerIndexVec, int realTime, std::map<int/*modelId*/, std::map<int /*process*/, int/*minIndex*/ >>& minDayIndexEachModel) {
	GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
	//��ʼ��map�洢ÿһ̨��?Q�ͻ����Ľ��
	std::map<int/*?Q��ԭ��index*/, std::vector<std::pair<int /*machineindex*/, int /*COMode*/>>> COMap = {};
	COMap[0] = {};
	COMap[1] = {};
	COMap[2] = {};

	/*��gTasks�е������Ϊδ�ӹ������ӹ������ڼӹ�
	δ�ӹ�����������û��һ̨�ʺϸ�����Ļ���
	���ӹ�����������������һ̨�ʺϸ�����Ļ����Ҹ�����������߲�û���γ�ͨ·
	���ڼӹ�������������һ���������ͨ·
	*/
	std::vector<std::pair<int/*model*/, std::map<int/*process*/, int/*1 or 0*/>>> waitInLine = {};//���ӹ�
	std::vector<std::pair<int/*model*/, std::map<int/*process*/, int/*1 or 0*/>>> offLine = {};//δ�ӹ�
	std::vector<std::pair<int/*model*/, std::map<int/*process*/, int/*1 or 0*/>>> onLine = {};//���ڼӹ�

	//����?��?index,��ʱ������������
	std::map<int/*modelId*/, int/*modelCount*/> tasksIndex = {};
	for (auto& e0 : gTasks) {
		tasksIndex[e0.job.model] = e0.count;
	}
	std::map<int/*model*/, int/*process*/> left = {};
	//�Ƚ���ͬ����ֳɴ��ӹ���δ�ӹ������ڼӹ�����
	for (auto& task : gTasks) {
		int mode = task.job.model;
		/*�������*/
		std::map<int/*process*/, int/*1 or 0*/> testMap = {};
		for (auto& i : gModels[mode].processes) {
			if (gProductLine.isGenericProcess(i)) {
				testMap[i] = 2;
				continue;
			}
			for (auto& machine : gProductLine[i]) {
				if (((machine.mode == mode && machine.status != Machine::MS_CO) || (machine.COTarget == mode && machine.status == Machine::MS_CO)) && machine.isMachineOnLine()) {
					testMap[i] = 1;
					break;
				}
				testMap[i] = 0;
			}
		}

		for (auto& e : jop) {
			int i = -1;
			for (; i < 10 && jop[e.first][i].empty(); i++) {}
			left[e.first] = i;
			//��left[e.first]=10,������Ѿ���ȫ�뿪�������ˣ������ֵ���д���
			if (left[e.first] == 10)
				left[e.first] = -2;
		}
		int clearProcessCount = 0;
		int genericProcessCount = 0;
		int firstObstructedProcess = -1;
		int passingProcessCount = 0;
		for (auto& e : testMap) {
			if (e.second == 1) {
				clearProcessCount++;
			}
			if (e.second == 2) {
				genericProcessCount++;
			}

			else if (firstObstructedProcess == -1 && !gProductLine.isGenericProcess(e.first)) {
				firstObstructedProcess = e.first;
			}
			/*	else if (!gProductLine.isGenericProcess(e.first)
					&& count(gModels[mode].processes.begin(), gModels[mode].processes.end(), e.first) > 0 && e.first > firstObstructedProcess) {
					firstObstructedProcess = e.first;
				}*/
		}
		passingProcessCount = clearProcessCount + genericProcessCount;
		if (clearProcessCount == 0) {
			offLine.push_back({ mode,testMap });
		}
		//����������������˵�һ���򣬴�ʱֻҪ���漸����������Ӧ������������������ڼӹ�
		else if (passingProcessCount != gModels[mode].processes.size()
			&& left[mode] <= firstObstructedProcess) {
			waitInLine.push_back({ mode,testMap });
		}
		else {
			onLine.push_back({ mode,testMap });
		}
	}

	for (auto& triggerIndex : triggerIndexVec) {
		if (gProductLine.machines[triggerIndex]->generic)
			continue;
		int machineProcess = gProductLine.machines[triggerIndex]->process;
		int finalMode = -1;
		int COtype = -1;
		/*
		���ȼ�˳�����ȿ��Ƿ�?Q�ͳɴ��ӹ����ٿ�δ�ӹ���������ڼӹ������
		*/
		/*
		����д��ӹ���������û����Ƿ��ڿ�ȱ������
		����ж�������ѡ����ѡ��?Q��ʱ�����ٵ�
		*/
		if (waitInLine.size() != 0) {
			int modeChoose = -1;
			double minCOTime = DBL_MAX;
			for (auto& e1 : waitInLine) {
				double COtime = gShiftMatrixs[gProductLine.machines[triggerIndex]->name][gProductLine.machines[triggerIndex]->mode][e1.first].first;
				if (e1.second[machineProcess] == 0 && machineProcess >= left[e1.first] && COtime < minCOTime) {
					modeChoose = e1.first;
					minCOTime = COtime;
				}
			}
			finalMode = modeChoose;
			COtype = 0;
		}
		/*
		�����δ�ӹ������ֻ�ڵ�һ������
		���ж���δ�ӹ���ѡ��?Q��ʱ���ٵ�
		*/
		if (offLine.size() != 0 && finalMode == -1 && machineProcess == 0) {
			int modeChoose = -1;
			double minCOTime = DBL_MAX;
			for (auto& e1 : offLine) {
				double COtime = gShiftMatrixs[gProductLine.machines[triggerIndex]->name][gProductLine.machines[triggerIndex]->mode][e1.first].first;
				if (e1.second[machineProcess] == 0 && COtime < minCOTime) {
					modeChoose = e1.first;
					minCOTime = COtime;
				}
			}
			finalMode = modeChoose;
			COtype = 1;
		}
		/*
		��������ڼӹ��������������Ƿ����ڽ���ʱ��֮ǰ��ɣ�
		�����ܣ�������?Q�ͺ��ܷ����
		���ж��������ѡ����������������
		*/
		else if (onLine.size() != 0 && finalMode == -1) {
			int modeChoose = -1;
			int maxCount = 0;

			double minActualDivDemand = DBL_MAX;
			//FIXME
			for (auto& e1 : onLine) {
				//�������Ѿ�ͨ���˸õ��򣬻��߻��������������������������迼��
				if (machineProcess < left[e1.first] || (gProductLine.machines[triggerIndex]->mode == e1.first && gProductLine.machines[triggerIndex]->getState() == Machine::S_ONLINE))
					continue;
				//ƿ�����򼯺ϣ��������ΪĿǰ������ȫ��?Q�ͽ������״̬�Ĳ���
				std::vector<int> capacityVec = gProductLine.getLowestCapacityProcesses(e1.first);
				bool isInTime = true;

				//FIXME
				int minDayIndex = minDayIndexEachModel[e1.first][machineProcess];

				int requiredCountForEarliest = 0;
				int requiredCountForAll = 0;
				int maxIndex = 0;
				for (auto& e : jop[e1.first]) {
					if (e.first > machineProcess)
						break;
					for (auto& modelMap : e.second) {
						if (modelMap.second.daysIndex == minDayIndex /*|| modelMap.second.daysIndex == minDayIndex + 1 || modelMap.second.daysIndex == minDayIndex + 2*/) {
							requiredCountForEarliest++;
						}
						if (modelMap.second.daysIndex > maxIndex)
							maxIndex = modelMap.second.daysIndex;
						requiredCountForAll++;
					}
				}
				int minDeadLineForEarlist = gContext.windowMaxDelaySeconds + minDayIndex * gContext.workingTimePerDay;
				int minDeadLineForAll = gContext.windowMaxDelaySeconds + maxIndex * gContext.workingTimePerDay;
				//�ڽ���֮ǰ�������������ٿ����
				double CountToFinishBeforeDeadLineForEarliest = gProductLine.CountToFinishBeforeDeadLine(minDeadLineForEarlist, realTime, e1.first);
				double CountToFinishBeforeDeadLineForAll = gProductLine.CountToFinishBeforeDeadLine(minDeadLineForAll, realTime, e1.first);
				//���罻������������������
				if (requiredCountForEarliest * gContext.coSusceptibility > CountToFinishBeforeDeadLineForEarliest ||
					requiredCountForAll * gContext.coSusceptibility > CountToFinishBeforeDeadLineForAll
					) {
					isInTime = false;
				}
				if (std::count(capacityVec.begin(), capacityVec.end(), machineProcess) > 0 && !isInTime) {
					if (tasksIndex[e1.first] > maxCount) {
						maxCount = tasksIndex[e1.first];
						modeChoose = e1.first;
					}


				}

			}
			finalMode = modeChoose;
			COtype = 2;
		}
		int originalMode = gProductLine.machines[triggerIndex]->mode;
		if (finalMode != -1)
			COMap[COtype].push_back({ triggerIndex,finalMode });
	}
	std::vector<std::pair<int, int>> candidate = {};
	if (COMap[0].size() != 0) {
		int selectedMode = -1;
		double minCOTime = DBL_MAX;
		for (auto& e1 : COMap[0]) {
			int modeChoose = -1;
			double COtime = gShiftMatrixs[gProductLine.machines[e1.first]->name][gProductLine.machines[e1.first]->mode][e1.second].first;
			if (COtime < minCOTime || COtime == minCOTime) {
				minCOTime = COtime;
				if (selectedMode == e1.second) {
					candidate.push_back({ e1.first, e1.second });
				}
				else {
					selectedMode = e1.second;
					candidate = {};
					candidate.push_back({ e1.first, e1.second });
				}
			}
		}
	}
	else if (COMap[1].size() != 0 && candidate.size() == 0) {
		int selectedMode = -1;
		double minCOTime = DBL_MAX;
		for (auto& e1 : COMap[1]) {
			int modeChoose = -1;
			double COtime = gShiftMatrixs[gProductLine.machines[e1.first]->name][gProductLine.machines[e1.first]->mode][e1.second].first;
			if (COtime < minCOTime || COtime == minCOTime) {
				minCOTime = COtime;
				if (selectedMode == e1.second) {
					candidate.push_back({ e1.first, e1.second });
				}
				else {
					selectedMode = e1.second;
					candidate = {};
					candidate.push_back({ e1.first, e1.second });
				}
			}
		}
	}
	else if (COMap[2].size() != 0 && candidate.size() == 0) {
		//������������ȼ�
		/*int selectedMode = -1;
		int modeChoose = -1;
		int finalMachineIndex = -1;
		double maxCount = -1;
		for (auto& e1 : COMap[0]) {
			int modeChoose = -1;
			double modelCount = tasksIndex[e1.second];
			if (modelCount > maxCount || modelCount == maxCount) {
				modelCount = maxCount;
				if (selectedMode == e1.second) {
					candidate.push_back({ e1.first, e1.second });
				}
				else {
					selectedMode = e1.second;
					candidate.push_back = {};
					candidate.push_back({ e1.first, e1.second });
				}
			}
		}*/
		//��?Q��ʱ�����ȼ�
		int selectedMode = -1;
		double minCOTime = DBL_MAX;
		for (auto& e1 : COMap[2]) {
			int modeChoose = -1;
			double COtime = gShiftMatrixs[gProductLine.machines[e1.first]->name][gProductLine.machines[e1.first]->mode][e1.second].first;
			if (COtime < minCOTime || COtime == minCOTime) {
				minCOTime = COtime;
				if (selectedMode == e1.second) {
					candidate.push_back({ e1.first, e1.second });
				}
				else {
					selectedMode = e1.second;
					candidate = {};
					candidate.push_back({ e1.first, e1.second });
				}
			}
		}
	}

	//���ݲ���ƽ��˼· ��candidateѡ��?Q�ͺ�Ĳ�����ӽ�Ŀ����ܵ�һ������
	int COMode = -1;
	int finalMachineIndex = -1;
	double minGap = DBL_MAX;



	for (auto& e1 : candidate) {
		/*����û����ǵ�����Ψһһ̨������������������Ҹû���ǰ�滹��δ������������򲻿��ǶԸû���?Q��*/
		bool isKeyMachineInProcess = gProductLine.judgeKeyMachineInProcess(gProductLine.machines[e1.first]->process, e1.first);
		if (isKeyMachineInProcess)
			continue;
		int machineProcess = gProductLine.machines[e1.first]->process;
		int minDayIndex = minDayIndexEachModel[e1.second][machineProcess];
		int requiredCountForEarliest = 0;
		int requiredCountForAll = 0;
		int maxIndex = 0;
		for (auto& e : jop[e1.second]) {
			if (e.first > machineProcess)
				break;
			for (auto& modelMap : e.second) {
				if (modelMap.second.daysIndex == minDayIndex /*|| modelMap.second.daysIndex == minDayIndex + 1 || modelMap.second.daysIndex == minDayIndex + 2*/) {
					requiredCountForEarliest++;
				}
				if (modelMap.second.daysIndex > maxIndex)
					maxIndex = modelMap.second.daysIndex;
				requiredCountForAll++;
			}
		}
		int minDeadLineForAll = gContext.windowMaxDelaySeconds + maxIndex * gContext.workingTimePerDay;
		//�������ڽ���֮ǰ�������������ٿ����
		ProductLine productLineTemp = gProductLine;
		for (auto& process : productLineTemp) {
			for (auto& machine : process) {
				if (machine._index == e1.first) {
					machine.COTarget = e1.second;
					machine.setState(Machine::S_ONLINE);
				}
			}
		}
		double CountToFinishBeforeDeadLineForAll = productLineTemp.CountToFinishBeforeDeadLine(minDeadLineForAll, realTime, e1.second);
		//���罻������������������

		double gap = std::fabs(CountToFinishBeforeDeadLineForAll - requiredCountForAll * gContext.coSusceptibility);
		if (gap < minGap) {
			minGap = gap;
			finalMachineIndex = e1.first;
			COMode = e1.second;
		}
	}
	return{ finalMachineIndex,COMode };
}
// �÷�����ΪgetCOMode�Ĳ��䣬�ܽ�һ�����ڶ���������ų����ȼ�������֮ǰ��������ѭ������һ����������?Q�;Ͳ��ٹ����������Ƿ������
std::pair<int /*machineindex*/, int /*COMode*/> getMostSuitbleCOMachineInMachines2(std::vector<int> triggerIndexVec, int realTime, std::map<int/*modelId*/, std::map<int /*process*/, int/*minIndex*/ >>& minDayIndexEachModel) {
	GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
	int finalMachineIndex = -1;
	int COMode = -1;
	double minDiff = DBL_MAX;
	for (auto& triggerIndex : triggerIndexVec) {
		if (gProductLine.machines[triggerIndex]->generic) {
			continue;
		}
		int originalMode = gProductLine.machines[triggerIndex]->mode;
		int machineProcess = gProductLine.machines[triggerIndex]->process;
		for (auto& task : gTasks) {
			int COTomode = task.job.model;
			if (COTomode == originalMode || !gProductLine.machines[triggerIndex]->support(COTomode))
				continue;
			//�������������õ���Ҳ��
			if (std::count(gModels[COTomode].processes.begin(), gModels[COTomode].processes.end(), machineProcess) == 0) {
				continue;
			}
			//����û���Ŀǰ����ӹ�������Ĳ��ܾ��Ѿ�����ǰ��һ�����򣨲���ͨ�õ��򣩵Ĳ��ܣ���Ҳ�����Կ���
			bool isCapacityOverLastProcess = gProductLine.judgeCapacityOverLastProcess(machineProcess, COTomode);
			if (isCapacityOverLastProcess)
				continue;
			//��������˲��ܻ���������Ҳ��
			/*bool canMachineCo2Model = gShiftMatrixs[gProductLine.machines[triggerIndex]->name][gProductLine.machines[triggerIndex]->mode][COTomode].first <= 0 ? false : true;
			if (canMachineCo2Model) {
				continue;
			}*/
			int minDayIndex = minDayIndexEachModel[COTomode][machineProcess];
			int requiredCountForEarliestForToMode = 0;
			int requiredCountForAllForToMode = 0;
			int requiredCountForAllForFromMode = 0;
			int maxIndexForToMode = 0;
			int maxIndexForFromMode = 0;
			for (auto& e : jop[COTomode]) {
				if (e.first > machineProcess)
					break;
				for (auto& modelMap : e.second) {
					if (modelMap.second.daysIndex == minDayIndex /*|| modelMap.second.daysIndex == minDayIndex + 1 || modelMap.second.daysIndex == minDayIndex + 2*/) {
						requiredCountForEarliestForToMode++;
					}
					if (modelMap.second.daysIndex > maxIndexForToMode)
						maxIndexForToMode = modelMap.second.daysIndex;
					requiredCountForAllForToMode++;
				}
			}
			for (auto& e : jop[originalMode]) {
				if (e.first > machineProcess)
					break;
				for (auto& modelMap : e.second) {
					if (modelMap.second.daysIndex > maxIndexForFromMode)
						maxIndexForFromMode = modelMap.second.daysIndex;
					requiredCountForAllForFromMode++;
				}
			}
			//����ƫ����
			//fixme:Ӧ���ڳ�ʼ��map��������
			minDayIndex++;
			maxIndexForToMode++;
			maxIndexForFromMode++;

			int minDeadLineForEarlistForToMode = gContext.windowMaxDelaySeconds + minDayIndex * gContext.workingTimePerDay;
			int minDeadLineForAllForToMode = gContext.windowMaxDelaySeconds + maxIndexForToMode * gContext.workingTimePerDay;
			int minDeadLineForAllForFromMode = gContext.windowMaxDelaySeconds + maxIndexForFromMode * gContext.workingTimePerDay;
			//���������ڽ���֮ǰ�������������ٿ����

			//test
			minDeadLineForAllForToMode = minDeadLineForAllForToMode < realTime ? realTime + gContext.workingTimePerDay : minDeadLineForAllForToMode;
			minDeadLineForAllForFromMode = minDeadLineForAllForFromMode < realTime ? realTime + gContext.workingTimePerDay : minDeadLineForAllForFromMode;

			double CountToFinishBeforeDeadLineForEarliestForToMode = gProductLine.CountToFinishBeforeDeadLineInCertainProcess(minDeadLineForEarlistForToMode, realTime, COTomode, machineProcess);
			double CountToFinishBeforeDeadLineForAllforToMode = gProductLine.CountToFinishBeforeDeadLineInCertainProcess(minDeadLineForAllForToMode, realTime, COTomode, machineProcess);
			double CountToFinishBeforeDeadLineForAllforFromMode = gProductLine.CountToFinishBeforeDeadLineInCertainProcess(minDeadLineForAllForFromMode, realTime, originalMode, machineProcess);

			//����֮��Ĳ���Ԥ��
			ProductLine productLineTemp = gProductLine;
			for (auto& process : productLineTemp) {
				for (auto& machine : process) {
					if (machine._index == triggerIndex) {
						machine.COTarget = COTomode;
						machine.setState(Machine::S_ONLINE);
					}
				}
			}
			double CountToFinishBeforeDeadLineForAllForToModeAfterCO = productLineTemp.CountToFinishBeforeDeadLineInCertainProcess(minDeadLineForAllForToMode, realTime, COTomode, machineProcess);
			double CountToFinishBeforeDeadLineForAllForTFromModeAfterCO = productLineTemp.CountToFinishBeforeDeadLineInCertainProcess(minDeadLineForAllForFromMode, realTime, originalMode, machineProcess);

			/*����֮ǰĿ����ܵ�ƫ�ưٷֱ�*/
			double offsetPercentageForToModeBeforeCO = requiredCountForAllForToMode == 0 ? 0 :
				(CountToFinishBeforeDeadLineForAllforToMode == 0 ? 0 : std::fabs(CountToFinishBeforeDeadLineForAllforToMode - requiredCountForAllForToMode) / requiredCountForAllForToMode);
			double offsetPerfcentageForFromModeBeforeCo = requiredCountForAllForFromMode == 0 ? 0 : std::fabs(CountToFinishBeforeDeadLineForAllforFromMode - requiredCountForAllForFromMode) / requiredCountForAllForFromMode;
			/*����֮��Ŀ����ܵ�ƫ�ưٷֱ�*/
			double offsetPercentageForToModeAfterCO = requiredCountForAllForToMode == DBL_MAX ? 0 : std::fabs(CountToFinishBeforeDeadLineForAllForToModeAfterCO - requiredCountForAllForToMode) / requiredCountForAllForToMode;
			double offsetPercentageForFromModeAfterCo = requiredCountForAllForFromMode == 0 ? 0 : std::fabs(CountToFinishBeforeDeadLineForAllForTFromModeAfterCO - requiredCountForAllForFromMode) / requiredCountForAllForFromMode;
			/*double diffPercentage = offsetPercentageForToModeAfterCO + offsetPercentageForFromModeAfterCo - (offsetPercentageForToModeBeforeCO + offsetPercentageForFromModeBeforeCo);*/
			double diffPercentage = offsetPercentageForToModeAfterCO - offsetPercentageForToModeBeforeCO;
			//����������û���κλ������������������Ǵ˵�����һ�������Ƿ���������������߸õ������׵���
			if (offsetPercentageForToModeBeforeCO == 0) {
				int isProcessFirstProcess = 0;
				int isLastProcessMachinesFits = 0;
				int isLastProcessMachineOutputBufferFits = 0;
				int isFirstProcessMachinesFits = 0;
				//�ȼ�������Ƿ����׵���
				if (gModels[COTomode].processes[0] == machineProcess) {
					isProcessFirstProcess = 1;
				}
				else {

#if 1
					//�����һ�����Ƿ��л���������߽�Ҫ��������
					int firstProcessIndex = gModels[COTomode].processes[0];
					for (auto& machineInFirstProcess : gProductLine[firstProcessIndex]) {
						if (machineInFirstProcess.mode == COTomode || machineInFirstProcess.COTarget == COTomode) {
							isFirstProcessMachinesFits = 1;
							break;
						}
					}
#endif
					//������һ�����Ƿ��л���������߽�Ҫ��������
					int processIndex = 0;
					int lastBufferProcess = 0;
					for (int index = 0; index < gModels[COTomode].processes.size(); index++) {
						if (gModels[COTomode].processes[index] == machineProcess) {
							processIndex = index - 1;
							lastBufferProcess = index - 1;
							bool isProcessGeneric = gProductLine.isGenericProcess(processIndex);
							if (isProcessGeneric)
								processIndex--;
							break;
						}
					}

					int lastProcess = gModels[COTomode].processes[processIndex];
					for (auto& machine : gProductLine[lastProcess]) {
						if (machine.mode == COTomode || machine.COTarget == COTomode) {
							isLastProcessMachinesFits = 1;
							break;
						}
					}

					//�����Ƿ�����ǰ������ϵ������Ϊ��ʱ����������õ��������ƥ��
					for (auto& machine : gProductLine[lastBufferProcess]) {
						std::queue<JobUnit> bufferTest = machine.outputBuffer;
						while (!bufferTest.empty()) {
							JobUnit jobTest = bufferTest.front();
							if (jobTest.model == COTomode) {
								isLastProcessMachineOutputBufferFits = 1;
								break;
							}
							bufferTest.pop();
						}
					}
				}
				//FIXME
				if (isProcessFirstProcess == 1 || isFirstProcessMachinesFits == 1 || isLastProcessMachineOutputBufferFits == 1) {
					diffPercentage = offsetPercentageForToModeAfterCO - 1000;
				}
				else {
					diffPercentage = 0;
				}
			}
			//FIXME
			if (diffPercentage < -0.1 && ((diffPercentage < minDiff
				//FIXME
				/*&& requiredCountForAllForToMode>gContext.minEvaluateCount*/
				//TODO
				&& CountToFinishBeforeDeadLineForAllForToModeAfterCO - CountToFinishBeforeDeadLineForAllforToMode>10)
				|| (offsetPercentageForToModeBeforeCO > 0.9))
				) {
				finalMachineIndex = triggerIndex;
				COMode = COTomode;
				minDiff = diffPercentage;
			}

			/*if (diffPercentage < -0.1 && diffPercentage < minDiff) {
				finalMachineIndex = triggerIndex;
				COMode = COTomode;
				minDiff = diffPercentage;
			}*/
		}
	}

	return{finalMachineIndex,COMode };
}
void printjop(JobsOfProcess JOP){
    printf("---------------------------------------------------\n");
    for(auto i : JOP){
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
void printtasks(){
    for(auto t:gTasks){
        printf("(%d,%d)",t.job.model,t.count);
    }
    printf("\n");
}
void printmachines(){
    for(auto i : gProductLine.machines){
        printf("\nMachine %d: ",i->_index);
        printf("\nInputbuffer: ");
        while(!i->inputBuffer.empty()){
            printf("%d, ",i->inputBuffer.front().uid);
            i->inputBuffer.pop();
        }
        printf("\nOutputbuffer: ");
        while(!i->outputBuffer.empty()){
            printf("%d, ",i->outputBuffer.front().uid);
            i->outputBuffer.pop();
        }
        printf("\nmodel: ");
        while(!i->model.empty()){
            printf("%d, ",i->model.front().uid);
            i->model.pop();
        }
    }
}
void printjobs(){
    while(!jobs.empty()){
        JobUnit j = jobs.top();
        printf("%d : model:%d/process:%d/daysindex:%d\n",j.uid,j.model,j.process,j.daysIndex);
        jobs.pop();
    }
}
void printconfig(double realTime, std::map<int,int>config)
{
    printf("%f:\n",realTime);
    for(auto i : config)
        printf("(%d->%d)",i.first,i.second);
    printf("\n");
}

int main(int argc, char* argv[]) {
	GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
	printStartupInfo();
#if 0//voicend
	if (argc < 3) {
		printHelp();
		exit(0);
	}
	std::string inputFilename = argv[1];
	gContext.id = argv[2];
	if (argc > 3) {
		gContext.isProcessDataShown = std::stoi(argv[3]);
	}

	Utils::log(0, "- input file: %s\n", inputFilename.data());
	Utils::log(0, "- id: %s\n", gContext.id.data());
#endif
    std :: string inputFilename = "/Users/voicend/CLionProjects/fits-core-0729-03df129f617c9101a32020bc815c5026c765672efits-core.git/FITSCore/data/input.csv";
	int ok = fits::COMatrixLoader().load(gShiftMatrixs);
	if (0 != ok) {
		Utils::log(-1, "Failed to load ChangeOver Matrix\n");
		return -1;
	}

	ok = fits::ModelProcessesMatrixLoader().load(gModels);
	if (0 != ok) {
		Utils::log(-1, "Failed to load Models and Processes Matrix\n");
		return -2;
	}
	//����һ��model id��֮���ļ�����index��ӳ���ϵ
	std::map<int, int> modelIndexMap;
	for (int i = 0; i < gModels.size(); i++) {
		int id = gModels.at(i).id;
		modelIndexMap[i] = id;
	}

	//�����ֳ���Ϣ�ͻ�������
	ok = fits::ProductLineMatrixLoader().load(gProductLine);
	if (0 != ok) {
		Utils::log(-1, "Failed to ProductLine Matrix\n");
		return -3;
	}

	{
		ClockMatrix clocks;
		clocks.modelIndexMap = modelIndexMap;
		ok = fits::ClockMatrixLoader().load(clocks);
		if (0 != ok) {
			Utils::log(-1, "Failed to load Machines and Clocks Matrix\n");
			return -4;
		}
		for (auto& process : gProductLine) {
			for (auto& m : process) {
				m.clocks = clocks[m.name];
				gProductLine.index(&m);
			}
		}
		//���ֳ���Ϣ���ؽ�jobs
		for (auto& machines : gProductLine) {
			for (auto& machine : machines) {
				if (machine.getState() != Machine::S_ONLINE)
					continue;
				auto inputBufferQueueTemp = machine.inputBuffer;
				auto outputBufferQueueTemp = machine.outputBuffer;
				auto productingBufferQueueTemp = machine.model;
				//�����ݴ� ����machine.statusΪMachine::working�����  û���������
				if (productingBufferQueueTemp.empty() && machine.status == Machine::MS_WORKING) {
					machine.status = Machine::MS_IDLE;
				}
				while (!inputBufferQueueTemp.empty()) {
					JobUnit jobTemp = inputBufferQueueTemp.front();
					inputBufferQueueTemp.pop();
					jobs.push(jobTemp);
				}
				while (!outputBufferQueueTemp.empty()) {
					JobUnit jobTemp = outputBufferQueueTemp.front();
					outputBufferQueueTemp.pop();
					jobs.push(jobTemp);
				}
				while (!productingBufferQueueTemp.empty()) {
					JobUnit jobTemp = productingBufferQueueTemp.front();
					productingBufferQueueTemp.pop();
					jobs.push(jobTemp);
				}
			}
		}
		//���ֳ���Ϣ���ؽ�JOP
		auto jobsTemp = jobs;
		const size_t TOTAL = jobsTemp.size();
		for (size_t i = 0; i < TOTAL; i++) {
			auto& e = jobsTemp.top();
			int p = e.ioo < 0 ? -1 : gModels[e.model].processes[e.ioo];
			jop[e.model][p][e.uid] = e;
			jobsTemp.pop();
		}
		//���ֳ���Ϣ���ؽ�gtasks
		jobsTemp = jobs;
		std::map<int/*model*/, int/*count*/ > gTasksTestBuffer = {};
		while (!jobsTemp.empty()) {
			auto jobUnitTemp = jobsTemp.top();
			jobsTemp.pop();
			gTasksTestBuffer[jobUnitTemp.model]++;
		}
		for (auto& e : gTasksTestBuffer) {
			gTasks.push_back(Task(e.first, e.second));
		}
	}

	/// load shift book
	ok = fits::ShiftBookLoader().load(*ShiftBookManager::getShiftBoobManager());
	if (0 != ok) {
		Utils::log(-1, "Failed to load Shift book\n");
		return -5;
	}
	//load settings
	ok = fits::GlobleContextLoader().load(gContext);
	if (0 != ok) {
		Utils::log(-1, "Failed to load globle context parameters in settings\n");
		return -6;
	}
	int state = 0;
	auto t0 = std::time(0);
	//��ʼ��MQ
	//fits::MQ& MQSRead = *fits::MQ::getMQ(gContext.mqHost, gContext.mqUsername, gContext.mqPassword, gContext.mqQueueName, gContext.port);
	//MQSRead.init();
	//MQSRead.consume();
	//MQSRead.deinit();
#if 0//voicend
	fits::MQ& MQS = *fits::MQ::getMQ(gContext.mqHost, gContext.mqUsername, gContext.mqPassword, gContext.mqQueueName, gContext.port);
	int sc = MQS.init();
	if (0 != sc) {
		Utils::log(2, "init mq fail: %d", sc);
		return -7;
	}
	Utils::log(1, "successfully connect to mq!\n\thostName:%s\n\tuserName:%s\n\tpasswords:%s\n\tqueueName:%s\n\tport:%d\n",
		gContext.mqHost.data(),
		gContext.mqUsername.data(),
		gContext.mqPassword.data(),
		gContext.mqQueueName.data(),
		gContext.port);
#endif
	Utils::log(1, "start simulating...\n");
	do {
		/// ����daily����
		state = parseRequirement2(inputFilename);
		if (state != 0) {
			Utils::log(-1, "filed in parseRequirement2: %d \n", state);
			break;
		}

		/*���������������ÿһ��������������ڵڼ��죬
		�����жϿ��л���Ϊ�˼��������������Ҫ��Ŀǰ����ܹ���ʱ���������жϣ�
		�ñ�����Ϊ�˷�ֹ�ظ�����jop*/
		std::map<int/*modelId*/, std::map<int /*process*/, int/*minIndex*/ >> minDayIndexEachModel = {};

		state = refresh(0, minDayIndexEachModel);

		if (state != 0) {
			Utils::log(-1, "failed in init refresh: %d \n", state);
			break;
		}
		Simulator simulator(&gProductLine);
		/*
		���ڴ�Ŵ������¹滮���߿��л�����index,
		һ�δ���ֻ�����¹滮һ̨�������������û������¹滮���Ϊ������?Q�ͣ�
		���ڼ����м���Ѱ����һ��������ֱ���ҵ���Ҫ?Q�͵Ļ���
		*/
		std::vector<int> triggerIndexVec = {};

		auto timeStatistics = time(nullptr);
		std::map<int, int> config = {};
		/*
		�ϰ汾ö���㷨�����ڳ�ʼ��
		��ʼ�����룬ʵ�ʲ��߲���Ҫ��
		��Ϊ�㷨�ص㣬��Ҫ�������������滮��
		���Բ�ͬ�ĳ�ʼ��״̬��LTӰ��޴�
		���µĳ�ʼ��������һ���̶����ó�ʼ������mode=-1������£�
		LT���ݺÿ�һ��
		*/
#if 0
		MachineMatrix matrix;
		ProductLine productLineTemp = gProductLine;
		makeMachineMatrixFromProductLine(matrix, productLineTemp);
		MachineAssignerForProductLine assigner(matrix, productLineTemp);
		TaskBatchVoter voter(assigner);
		std::vector<int> IndexOfTasks;
		for (auto& e : gTasks) IndexOfTasks.push_back(IndexOfTasks.size());
		voter.vote2(IndexOfTasks, gProductLine[0].size());
		config = bestConfig.getConfig();
#endif
#if  0

		config[16] = -1;


#endif 
		gProductLine.setCapacitySurplusMachineOffLine();
		while (!gTasks.empty()) {
			triggerIndexVec = simulator.triggerIndexVec;
#if 0
			if (triggerIndexVec.size() != 0) {
				for (auto& triggerIndex : triggerIndexVec) {
					int mode = getCOMode(triggerIndex, simulator.realTime, minDayIndexEachModel).first;
					bool isKeyMachineInProcess = gProductLine.judgeKeyMachineInProcess(gProductLine.machines[triggerIndex]->process, triggerIndex);
					bool isPauseMachineInProcess = gProductLine.judgePauseMachineInProcess(gProductLine.machines[triggerIndex]->process, triggerIndex, mode);
					/*���������޺���?Q����������������һ������*/
					/*����û����ǵ�����Ψһһ̨������������������Ҹû���ǰ�滹��δ������������򲻿��ǶԸû���?Q��*/
					if (mode == -1 || isKeyMachineInProcess || isPauseMachineInProcess) {
						continue;
					}

					else {
						gProductLine.machines[triggerIndex]->setState(Machine::S_ONLINE);
						config = {};
						config[triggerIndex] = mode;
						break;
					}
				}
				simulator.triggerIndexVec = {};
			}
#endif
#if 1
			std::pair<int, int> COInfo = getMostSuitbleCOMachineInMachines2(triggerIndexVec, simulator.realTime, minDayIndexEachModel);
			//test
			/*gProductLine.setCapacitySurplusMachineOffLine();*/
			if (COInfo.first != -1) {
				config = {};
				config[COInfo.first] = COInfo.second;
				gProductLine.machines[COInfo.first]->setState(Machine::S_ONLINE);
//				printconfig(simulator.realTime,config);
			}
			simulator.triggerIndexVec = {};
#endif
			/*�������񲢿�ʼ����*/
			simulator.update(jobs);
			simulator.setup(config);
			int ok = simulator.simulate();
			if (ok < 0)
				return ok;
			//�ж��Ƿ����ס������������ѭ��
			if (simulator.realTime - simulator.lastJobReleaseTime > gContext.endlessLoopWaitTime) {
				Utils::log(-1, "jump out endless loop\n JOP:\n");
				//typedef std::map<int/*model*/, std::map<int/*process*/, std::map<int/*uid of job*/, JobUnit>>> JobsOfProcess;
				for (auto& e1 : jop) {
					for (auto& e2 : e1.second) {
						Utils::log(-1, "\tmodel: %d, process: %d, count: %d\n", e1.first, e2.first, e2.second.size());
					}
				}
				return -9;
			}

			/*��������¼*/
			jobs = simulator.jobs;
            jop = simulator.JOP;
			triggerIndexVec = simulator.triggerIndexVec;
			//��������ö��ǰ����ÿһ��������״̬��Ϣ��jobs״̬��Ϣ����Ҫ��������������Ϣ���и���
			refresh((int)simulator.realTime, minDayIndexEachModel);
		}
		///write workTime waitTime csv file
		if (gContext.isProcessDataShown) {
			CSVWriter waitAndWordTimeLogger = { "wait_worktime_co_log.csv", { "MACHINE","INDEX","WORKTIME", "WAITTIME","CHANGETIME","CHANGECOUNT","OEE" }, false };
			for (auto wwt : workAndWaitTime) {
				double oee = wwt.second.first / (wwt.second.first + wwt.second.second);
				if (gContext.isProcessDataShown) {
					waitAndWordTimeLogger.write({
														gProductLine.machines[wwt.first]->fullname(),
														std::to_string(wwt.first),
														std::to_string(wwt.second.first),
														std::to_string(wwt.second.second),
														std::to_string(coTimeAndCount[wwt.first].first),
														std::to_string(coTimeAndCount[wwt.first].second),
														std::to_string(oee)
						});
				}
			}
		}
		printExecutionDetails(t0);
	} while (false);
	std::cout << "stoped with state: " << state << "\n";
	Json::Value e;
	e["type"] = -1;
	e["id"] = gContext.id;
    std::cout << e.toStyledString();
#if 0 //voicend
	MQS.send("fits.core", e.toStyledString());
	MQS.deinit();
#endif
	//std::this_thread::sleep_for(std::chrono::seconds(10));
	//system("pause");
	return 0;
}