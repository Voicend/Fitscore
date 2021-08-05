#include "common.hpp"
#include "utils.hpp"
#include "models.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"
#include "machine.hpp"
#include "productline.hpp"
#include "configuration.hpp"
#include "assigner.hpp"
#include "voter.hpp"

using namespace fits;

namespace fits {
	/*Productivity Requirement Item*/
	struct PRI {
		PTask task = nullptr;
		double now = 0.;
		
		PRI() {}
		PRI(PTask task, double now) : task(task), now(now) {}

		double PR() const;
	};

	bool operator> (const PRI& lhs, const PRI& rhs) { return lhs.PR() < rhs.PR(); }
}

double
fits::PRI::PR() const{
	return task->count / (double)(task->deadline - now);
}

void
fits::TaskBatchVoter2::vote(RequirementTasks& allTasks, RequirementTasks prevTasks, int countOfParallelable, double now)
{
	for (auto it = prevTasks.begin(); it != prevTasks.end(); (it->count <= 0) ? it = prevTasks.erase(it) : it++) {}

	///1. 根据deadline计算生产需求，deadline前每秒需要生产多少个
	std::priority_queue<PRI, std::vector<PRI>, std::greater<PRI>> PRIs;
	for (auto& e : allTasks) {
		PRIs.push(PRI(&e, now));
	}

	result.clear();
	result.assign(prevTasks.begin(), prevTasks.end());
	for (size_t i = result.size(); i < (size_t)countOfParallelable && (!PRIs.empty()); i++) {
		result.push_back(*PRIs.top().task);
		///std::cout << PRIs.top().task->count << " | " << PRIs.top().task->deadline << " | " << PRIs.top().PR() << "\n";
		PRIs.pop();
	}
}
