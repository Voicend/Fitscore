#pragma once

#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <limits>

namespace fits {
	struct JobUnitInfo;
	typedef struct Task : public TaskInfo {
		int count;
		int deadline = (std::numeric_limits<int>::max)();	///如86400表示第一天内要完成
		JobUnitInfo job;
		Task(int model, int count);
		Task(int model, int count, std::string date);
		bool dummy();

		static std::unique_ptr<Task> DummyTask;
	} *PTask;

	typedef std::vector<PTask> TaskBatch;

	typedef std::vector<Task> RequirementTasks, * PRequirementTasks;
	typedef std::map<int/*model*/, PTask> RequirementTasksIndexer;

	typedef std::map<std::string/*date*/, RequirementTasks> DailyRequirementTasks;
	typedef std::map<int/*0 based index of day*/, PRequirementTasks> DailyRequirementTasksIndexer;

	/// TODO: 
	typedef struct TasksManager {
		static TasksManager* getTasksManager() {
			return instance ?
				instance :
				instance = new TasksManager();
		}

		void put(const std::string& date, RequirementTasks& tasks) {
			auto &added = this->tasks[date] = tasks;
			int indexOfDay = (int)this->tasks.size() - 1;
			indexer.insert({ indexOfDay, &added });
		}

		RequirementTasks& get(int indexOfDay) {
			if (indexer.count(indexOfDay) <= 0)
				throw "index out of range: " + std::to_string(indexOfDay) + " but size: " + std::to_string(indexer.size());
			
			auto p = indexer[indexOfDay];
			return *p;
		}

		RequirementTasks& get(std::string date) {
			if(tasks.count(date) <= 0) throw "not found: " + date;
			return tasks[date];
		}

		RequirementTasks* front() {
			if (indexer.count(progress) == 0) return nullptr;
			return indexer[progress];
		}

		/*@countOfDays 加载几天?*/
		//@return: 读入的天数
		int fill(JobsQueue& jobs, int countOfDays = 1);

		//@return: 读入天数
		int fill(JobsQueue& jobs, const std::function<void(int/*index of day*/, RequirementTasks&)>& onFilled, int countOfDays = 1);

		int getProgress() { return progress; }

		bool empty() { return progress == indexer.size(); }

	private:
		int progress = 0;		///当前进展到哪天? 已经放入排产队列

		DailyRequirementTasks tasks;
		DailyRequirementTasksIndexer indexer;

		TasksManager() {}
		static TasksManager* instance;
	} *PTasksManager;
}