#pragma once

#include <string>
#include <queue>
#include <map>

namespace fits {

	struct JobUnitInfo {
		static const int CAPACITY = 48;
		int uid;
		int model;
		///cost of time
		double COT(double clock);

		JobUnitInfo(int model);
		JobUnitInfo(int uid, int model);
	};

	struct TaskInfo;	//defined in tasks.hpp
	typedef struct JobUnit : JobUnitInfo
	{
		enum State {
			JUS_UNSET = -1,
			JUS_WAITING = 0,	//�ڽ��ϵ�
			JUS_PROCESSING,		//�ڼӹ�λ
			JUS_COMPLETED,		//�ڳ��ϵ�
			JUS_TRANSITING		//������
		} state = JUS_UNSET;

		int ioo = -1;	///index of process
		int process = -1;
		double releaseTime = 0;
		int machineId = -1;
		int daysIndex = 0;
		TaskInfo taskInfo;

		JobUnit(int model, TaskInfo& taskInfo);
		JobUnit(int model, TaskInfo& taskInfo, int progress);
		//@model: model index
		JobUnit(int model, int process);
		JobUnit(int model);
		JobUnit(int uid, int mode, int ioo, int process, /*int releaseTime,*/ /*int machinId,*/ int daysIndex);
		JobUnit();
	} *PJobUnit;

	struct Configuration;
	typedef struct JobsQueue : std::priority_queue <JobUnit, std::vector<JobUnit>, std::greater<>> {
		//����һ������ʱ����ǰ�����ѳ�ʼ�����պ���ȫ��pop������Լ����ʱ��
		void initialize(Configuration& cfg);
	} *PJobsQueue;

	typedef std::map<int/*model*/, std::map<int/*process*/, std::map<int/*uid of job*/, JobUnit>>> JobsOfProcess;

	extern bool operator> (const JobUnit& lhs, const JobUnit& rhs);
}