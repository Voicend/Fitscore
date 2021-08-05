#pragma once

#include <vector>
#include <map>

extern fits::RequirementTasks gTasks;
extern fits::ConfigurationShadow bestConfig;

namespace fits {

	typedef
		struct TaskBatchVoter {
		//��¼task������
		std::vector<int> bestBatch, currentBatch, lastBatch;
		double bestTimeCost = std::numeric_limits<int>::max();
		int bestCountOfChangeType = std::numeric_limits<int>::max();
		///snapshot�������л����ĳ�ʼ״̬
		std::map<int/*global index*/, MachineRuntimeInfo> bestSnapshotOfMachines;

		void vote(std::vector<int>& indexOfTasks/*������������*/, int countOfParallelable/*���ͬʱ������*/) {
			countOfParallelable = (std::min)(static_cast<int>(indexOfTasks.size()), countOfParallelable);
			for (int i = 0; i < static_cast<int>(indexOfTasks.size()); i++) {
				currentBatch.push_back(indexOfTasks[i]);
				if (countOfParallelable == 1) {
					///����������
					///1.�������䵽task����

					RequirementTasks batch;
					for (auto e : currentBatch) {
						batch.push_back(gTasks.at(e));
					}

					//��������Ϻ�ʱ�����Ҫͣ���������������ת��ȥ�����������Դﵽ�Բ��ܲ�׷�ϻ��ʹ��۲���������Խ�ֹ���ﵽ��������͵�Ŀ��
					std::vector<int> modelIdAll = {};
					for (auto& task : gTasks) {
						modelIdAll.push_back(task.job.model);
					}

					std::vector<int> modelIdCurrent = {};
					for (auto& task : batch) {
						modelIdCurrent.push_back(task.job.model);
					}
					int isResonable = 1;
					for (auto& modelId : lastBatch) {
						bool isModelInTasks = std::count(modelIdAll.begin(), modelIdAll.end(), modelId) > 0;
						bool isModelInCurrentBatch = std::count(modelIdCurrent.begin(), modelIdCurrent.end(), modelId) > 0;
						if (isModelInTasks && !isModelInCurrentBatch) {
							isResonable = 0;
							break;
						}
					}
					if (isResonable == 0)
						machineAssigner.bestTimeCost = std::numeric_limits<double>::max();
					///���㵱ǰ����״̬������batch�����ʱ�䣬ע�����״̬�ı��ֺ͸���
					else {
						for (auto& e : batch) std::cout << e.job.model << " ";
						machineAssigner.assign(batch);
					}

					if (machineAssigner.bestTimeCost < bestTimeCost) {
						bestConfig = machineAssigner.bestConfiguration;
						bestTimeCost = machineAssigner.bestTimeCost;
						bestCountOfChangeType = machineAssigner.countChangeType;
						bestSnapshotOfMachines = machineAssigner.bestSnapshotOfMachines;
						bestBatch = currentBatch;
						std::cout << " selected best LT " << bestTimeCost << " seconds with " << bestCountOfChangeType << " C/Os\n";
					}
					///reset machine config to init status
					machineAssigner.reset();
				}
				else {
					//if ((i + 1) <= countOfParallelable) {
					if ((indexOfTasks.size() - 1 - i) >= 1) {
						std::vector<int> sub(indexOfTasks.begin() + i + 1, indexOfTasks.end());
						if (sub.size() >= (size_t)countOfParallelable - 1)
							vote(sub, countOfParallelable - 1);
					}
				}
				currentBatch.pop_back();
			}
		}
		void vote2(std::vector<int> & indexOfTasks/*������������*/, int countOfParallelable/*���ͬʱ������*/) {
			countOfParallelable = (std::min)(static_cast<int>(indexOfTasks.size()), countOfParallelable);
			/*for (int i = 0; i < static_cast<int>(indexOfTasks.size()); i++) {
				currentBatch.push_back(indexOfTasks[i]);
				if (countOfParallelable == 1) {*/
				///����������
				///1.�������䵽task����

			/*RequirementTasks batch;
			for (auto e : currentBatch) {
				batch.push_back(gTasks.at(e));
			}*/

			RequirementTasks batch;
			batch.push_back(gTasks.at(0));
			batch.push_back(gTasks.at(1));
			batch.push_back(gTasks.at(2));


			//��������Ϻ�ʱ�����Ҫͣ���������������ת��ȥ�����������Դﵽ�Բ��ܲ�׷�ϻ��ʹ��۲���������Խ�ֹ���ﵽ��������͵�Ŀ��
			std::vector<int> modelIdAll = {};
			for (auto& task : gTasks) {
				modelIdAll.push_back(task.job.model);
			}

			std::vector<int> modelIdCurrent = {};
			for (auto& task : batch) {
				modelIdCurrent.push_back(task.job.model);
			}
			int isResonable = 1;
			for (auto& modelId : lastBatch) {
				bool isModelInTasks = std::count(modelIdAll.begin(), modelIdAll.end(), modelId) > 0;
				bool isModelInCurrentBatch = std::count(modelIdCurrent.begin(), modelIdCurrent.end(), modelId) > 0;
				if (isModelInTasks == true && isModelInCurrentBatch == false) {
					isResonable = 0;
					break;
				}
			}
			if (isResonable == 0)
				machineAssigner.bestTimeCost = std::numeric_limits<double>::max();
			///���㵱ǰ����״̬������batch�����ʱ�䣬ע�����״̬�ı��ֺ͸���
			else {
				for (auto& e : batch) std::cout << e.job.model << " ";
				machineAssigner.assign(batch);
			}

			if (machineAssigner.bestTimeCost < bestTimeCost) {
				bestConfig = machineAssigner.bestConfiguration;
				bestTimeCost = machineAssigner.bestTimeCost;
				bestCountOfChangeType = machineAssigner.countChangeType;
				bestSnapshotOfMachines = machineAssigner.bestSnapshotOfMachines;
				bestBatch = currentBatch;
				std::cout << " selected best LT " << bestTimeCost << " seconds with " << bestCountOfChangeType << " C/Os\n";
			}
			///reset machine config to init status
			machineAssigner.reset();
			//	}
			//	else {
			//		//if ((i + 1) <= countOfParallelable) {
			//		if ((indexOfTasks.size() - 1 - i) >= 1) {
			//			std::vector<int> sub(indexOfTasks.begin() + i + 1, indexOfTasks.end());
			//			if (sub.size() >= (size_t)countOfParallelable - 1)
			//				vote(sub, countOfParallelable - 1);
			//		}
			//	}
			//	currentBatch.pop_back();
			//}
		}

		TaskBatchVoter(MachineAssignerForProductLine & machineAssigner) : machineAssigner(machineAssigner) {}

		private:
			MachineAssignerForProductLine& machineAssigner;
	} *PTaskBatchVoter;

	///�ӵ�ǰ���������ѡ������Ҫͬʱ�����ļ���
	typedef
		struct TaskBatchVoter2 {
		void vote(
			/*��ѡ�������飬����Щ������ѡ�����countOfParallelable��ͬʱ����*/
			RequirementTasks& allTasks,
			/*�ѹ滮δȫ���ϲ��ߵ�����prevTasks�������Ѿ�ȫ���ϲ��ߵ����Σ�*/
			RequirementTasks prevTasks,
			int countOfParallelable/*���ͬʱ������*/,
			double now		//��ǰʱ��ڼ���
		);

		RequirementTasks result;

	} *PTaskBatchVoter2;
}