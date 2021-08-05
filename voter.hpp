#pragma once

#include <vector>
#include <map>

extern fits::RequirementTasks gTasks;
extern fits::ConfigurationShadow bestConfig;

namespace fits {

	typedef
		struct TaskBatchVoter {
		//记录task的索引
		std::vector<int> bestBatch, currentBatch, lastBatch;
		double bestTimeCost = std::numeric_limits<int>::max();
		int bestCountOfChangeType = std::numeric_limits<int>::max();
		///snapshot保存所有机器的初始状态
		std::map<int/*global index*/, MachineRuntimeInfo> bestSnapshotOfMachines;

		void vote(std::vector<int>& indexOfTasks/*任务索引数组*/, int countOfParallelable/*最大同时生产数*/) {
			countOfParallelable = (std::min)(static_cast<int>(indexOfTasks.size()), countOfParallelable);
			for (int i = 0; i < static_cast<int>(indexOfTasks.size()); i++) {
				currentBatch.push_back(indexOfTasks[i]);
				if (countOfParallelable == 1) {
					///评估这个组合
					///1.从索引变到task集合

					RequirementTasks batch;
					for (auto e : currentBatch) {
						batch.push_back(gTasks.at(e));
					}

					//在评估组合和时候，如果要停下正在生产的零件转而去做别的零件，以达到以产能差追上换型代价差的做法予以禁止来达到避免错误换型的目的
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
					///计算当前产线状态生产这batch的最佳时间，注意产线状态的保持和更新
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
		void vote2(std::vector<int> & indexOfTasks/*任务索引数组*/, int countOfParallelable/*最大同时生产数*/) {
			countOfParallelable = (std::min)(static_cast<int>(indexOfTasks.size()), countOfParallelable);
			/*for (int i = 0; i < static_cast<int>(indexOfTasks.size()); i++) {
				currentBatch.push_back(indexOfTasks[i]);
				if (countOfParallelable == 1) {*/
				///评估这个组合
				///1.从索引变到task集合

			/*RequirementTasks batch;
			for (auto e : currentBatch) {
				batch.push_back(gTasks.at(e));
			}*/

			RequirementTasks batch;
			batch.push_back(gTasks.at(0));
			batch.push_back(gTasks.at(1));
			batch.push_back(gTasks.at(2));


			//在评估组合和时候，如果要停下正在生产的零件转而去做别的零件，以达到以产能差追上换型代价差的做法予以禁止来达到避免错误换型的目的
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
			///计算当前产线状态生产这batch的最佳时间，注意产线状态的保持和更新
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

	///从当前任务里表中选择最先要同时生产的几个
	typedef
		struct TaskBatchVoter2 {
		void vote(
			/*候选任务数组，从这些任务中选择最多countOfParallelable个同时生产*/
			RequirementTasks& allTasks,
			/*已规划未全部上产线的任务，prevTasks不包含已经全部上产线的批次，*/
			RequirementTasks prevTasks,
			int countOfParallelable/*最大同时生产数*/,
			double now		//当前时间第几秒
		);

		RequirementTasks result;

	} *PTaskBatchVoter2;
}