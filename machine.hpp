#pragma once

#include <queue>
#include <string>
#include <map>
#include <iostream>

namespace fits {

	//�ƻ�������ʱ����״̬��ȡ����
	typedef struct MachineRuntimeInfo {
		//0����̬��1����״̬��2����̬, 3 blocking
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

		//Ԥ�ڶ������Ŀ���ʱ��
		double toIdle = 0;

		//��ǰ�Ĺ���ģʽ����Ʒ�ͺ�����
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
		//��¼�����Ƿ�׼������0Ϊ��׼����1Ϊ׼��
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

		int COTarget = -1;	//������Ҫ����ʱ��Ч
		int getCOTimeCost();

		MachineRuntimeInfo(int mode = 0, int toIdle = 0) : toIdle(toIdle), mode(mode) {}

		enum MachineState {
			S_ONLINE = 0,
			S_OFFLINE,
			S_PAUSE,		///���ߵ�������
		};
		MachineState getState() { return state; }
		void setState(MachineState state) { this->state = state; }

	private:
		MachineState state;

	} *PMachineRuntimeInfo;

	//�豸���������������������ԡ���Ϊ��ص�����
	//����ʱ�仯�����ԣ���MachineRuntimeInfo��
	typedef struct Machine : public MachineRuntimeInfo
	{
		///������
		int process = 0;
		std::string number;
		std::string name;   //model name
		std::pair<int/*�Զ�*/, int/*�ֶ�*/> bufferSize;
		///�豸��������ʱ�����
		MachineClockMatrix clocks;
		bool generic = false;

		std::queue<JobUnit> inputBuffer;
		std::queue<JobUnit> outputBuffer;
		//��ǰ����������� FIXME: Ӧ��ֻ��������ǰ�����ͺŵļ�¼��������MachineRuntimeInfo�У���������λӦ������������
		std::queue<JobUnit> model;

		std::string fullname() {
			return name + "_" + number;
		}

		Machine(int process, const std::string& number, const std::string& name, MachineClockMatrix& clocks, std::pair<int/*�Զ�*/, int/*�ֶ�*/> bufferSize, bool generic = false) :
			MachineRuntimeInfo(-1),
			process(process),
			number(number),
			name(name),
			clocks(clocks),
			bufferSize(bufferSize),
			generic(generic)
		{}

		Machine(int process, const std::string& number, const std::string& name, std::pair<int/*�Զ�*/, int/*�ֶ�*/> bufferSize, bool generic = false) :
			MachineRuntimeInfo(-1),
			process(process),
			number(number),
			name(name),
			bufferSize(bufferSize),
			generic(generic)
		{}

		static std::map<int/*ȫ�ֻ���index*/, std::map<int/*model-id*/, double/*�����clock*/>> calculatedTickMatrix;

		///��ȡ�û�������modelId�ͺŵĽ��ģ�ͬ .clocks[modelId]
		double clock(int modelId) {
			auto& value = calculatedTickMatrix[_index][modelId];
			if (value > 0) return value;

			MachineClockMatrix::iterator it = clocks.find(modelId);
			if (it == clocks.end()) return -1;
			return value = it->second;
		}

		///�����Ƿ��ܹ�����model�ͺ�(ͨ�����ͺ�֧��Ҳ��)
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

		///��ʼ�ӹ�@job
		int workOn(JobUnit & job, int now) {
			status = Machine::MS_WORKING;
			toIdle = now + job.COT(clock(job.model));
			model.push(job);
			inputBuffer.pop();
			return 0;
		}

		///ִ�л���
		int changeOver(int cost, int now) {
			status = Machine::MS_CO;
			setCOStatus(Machine::COS_KEEP);
			toIdle = now + cost;
			/*std::cout << fullname() << " ready to C/O from " << mode << " -> " << COTarget
				<< " with input " << inputBuffer.size() << ",type "
				<< (inputBuffer.empty() ? -1 : inputBuffer.front().model) << "\n";*/
			return 0;
		}

		/*�Ͽ�Ӽӹ�λ��ɽ�����ϵ�*/
		int finishJob(JobUnit & job, int now) {
			job.releaseTime = now;
			outputBuffer.push(job);
			model.pop();
			toIdle = now;
			status = Machine::MS_IDLE;
			return 0;
		}
		/*�жϻ����Ƿ�ͣ��*/
		bool isMachineOnLine() {
			if ((*this).getState() != S_OFFLINE && (*this).getState() != S_PAUSE) {
				return true;
			}
			return false;
		}

		int _index = -1;	///global index
	} *PMachine;
}