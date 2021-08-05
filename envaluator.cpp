#include "common.hpp"
#include "utils.hpp"
#include "models.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"
#include "machine.hpp"
#include "productline.hpp"
#include "configuration.hpp"
#include "envaluator.hpp"

using namespace fits;

int
fits::EnvaluatorWithULB::envalute(JobsQueue& jobs)
{
	return 0;
}

int
fits::EnvaluatorByProductivity::envalute(RequirementTasks& tasks) {
	return 0;
}
int
fits::EnvaluatorWithCOPlans::envalute(std::vector<std::pair<int, int>>& COPlans) {
	/*???????????*/

	/*??????????*/

	/*???????????*/

	/*?????????????????*/


	return 0;
}