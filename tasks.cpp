#include "utils.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"

using namespace fits;

Task::Task(int model, int count) : count(count), job(JobUnitInfo(model)) {}
Task::Task(int model, int count, std::string date) : TaskInfo(date), count(count), job(JobUnitInfo(model)) {}

bool Task::dummy() {
	return count <= 0 && job.model < 0;
}

//static
PTasksManager TasksManager::instance = nullptr;

//static
std::unique_ptr<Task> Task::DummyTask = std::unique_ptr<Task>(new Task(-1, -1));

int
TasksManager::fill(JobsQueue & jobs, int countOfDays/* = 1*/) {
	return fill(jobs, nullptr, countOfDays);
}

int
TasksManager::fill(JobsQueue& jobs, const std::function<void(int/*index of day*/, RequirementTasks&)>& onFilled, int countOfDays/* = 1*/) {
	int result = 0;
	do {
		if (empty()) break;
		auto& tasksOfOneDay = get(progress);
		for (auto& e : tasksOfOneDay) {
			for (int i = 0; i < e.count; ++i) {
				JobUnit job(e.job.model, e, progress);
				job.uid = UID::generate();
				jobs.push(job);
			}
		}

		if (onFilled) {
			onFilled(progress, tasksOfOneDay);
		}

		result += 1;
		progress += 1;
		countOfDays -= 1;
	} while (countOfDays > 0);

	return result;
}
