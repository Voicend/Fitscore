#pragma once

namespace fits {
	template<typename T>
	struct BaseEnvaluator {
		virtual int envalute(T& jobs) = 0;
		virtual ~BaseEnvaluator() {}

		BaseEnvaluator(PProductLine ppl, PConfiguration configuration)
			: ppl(ppl), configuration(configuration) {}
		BaseEnvaluator(PProductLine ppl, std::pair<int, int> configuration)
			: ppl(ppl) {}
	private:
		PProductLine ppl;
		PConfiguration configuration;

	};

	/// Un-Limited-Buffer
	typedef struct EnvaluatorWithULB : public BaseEnvaluator<JobsQueue> {
		EnvaluatorWithULB(PProductLine ppl, PConfiguration configuration)
			: BaseEnvaluator(ppl, configuration) {}

		// ͨ�� BaseEnvaluator �̳�
		virtual int envalute(JobsQueue& jobs) override;
	} *PEnvaluatorWithULB;

	typedef struct EnvaluatorByProductivity : public BaseEnvaluator<RequirementTasks> {
		EnvaluatorByProductivity(PProductLine ppl, PConfiguration configuration)
			: BaseEnvaluator(ppl, configuration) {}

		// ͨ�� BaseEnvaluator �̳�
		virtual int envalute(RequirementTasks& tasks) override;
	} *PEnvaluatorByProductivity;

	//������ƽ������һϵ�л��ͷ�������ѡ��������
	typedef struct EnvaluatorWithCOPlans : public BaseEnvaluator< std::vector<std::pair<int, int>> > {
		EnvaluatorWithCOPlans(PProductLine ppl, std::pair<int, int> configuration)
			: BaseEnvaluator(ppl, configuration) {}

		// ͨ�� BaseEnvaluator �̳�
		virtual int envalute(std::vector<std::pair<int, int>>& COPlans) override;
	} *PEnvaluatorWithCOPlans;
}