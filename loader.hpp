#pragma once

#include <string>
#include <map>

namespace fits {
	template<typename Target>
	struct Loader {
		virtual ~Loader() {};
		virtual int load(Target& target, const std::string& filename) = 0;
	};

	struct ShiftMatrix;
	typedef std::map<std::string, ShiftMatrix> ProductLineShiftMatrix;
	struct COMatrixLoader : public Loader<ProductLineShiftMatrix> {
		std::map<int, int> columns;	///用来记录列index和(to)换型目标model的ID
		virtual int load(ProductLineShiftMatrix& matrix, const std::string& filename = "/Users/voicend/CLionProjects/fits-core-0729-03df129f617c9101a32020bc815c5026c765672efits-core.git/FITSCore/data/ChangeOverMatrix.csv") override;
	};

	struct Model;
	struct ModelsManager;
	struct ModelProcessesMatrixLoader : public Loader<ModelsManager> {
		virtual int load(ModelsManager& matrix, const std::string& filename = "/Users/voicend/CLionProjects/fits-core-0729-03df129f617c9101a32020bc815c5026c765672efits-core.git/FITSCore/data/ModelProcessesMatrix.csv") override;
	};

	struct ClockMatrix;
	struct ClockMatrixLoader : public Loader<ClockMatrix> {
		virtual int load(ClockMatrix& matrix, const std::string& filename = "/Users/voicend/CLionProjects/fits-core-0729-03df129f617c9101a32020bc815c5026c765672efits-core.git/FITSCore/data/MachineClockMatrix.csv") override;
	};

	struct ProductLine;
	struct ProductLineMatrixLoader : public Loader<ProductLine> {
		virtual int load(ProductLine& matrix, const std::string& filename = "/Users/voicend/CLionProjects/fits-core-0729-03df129f617c9101a32020bc815c5026c765672efits-core.git/FITSCore/data/ProductLineMatrix.json") override;
	};

	struct DailyTasksLoader : public Loader<int> {
		virtual int load(int& matrix, const std::string& filename) override;
	};

	struct ShiftBookManager;
	struct ShiftBookLoader : public Loader<ShiftBookManager> {
		virtual int load(ShiftBookManager& matrix, const std::string& filename = "/Users/voicend/CLionProjects/fits-core-0729-03df129f617c9101a32020bc815c5026c765672efits-core.git/FITSCore/data/ShiftBook.csv") override;
	};
	struct GlobleContextManager;
	struct GlobleContextLoader : public Loader<GlobleContextManager> {
		virtual int load(GlobleContextManager& matrix, const std::string& filename = "/Users/voicend/CLionProjects/fits-core-0729-03df129f617c9101a32020bc815c5026c765672efits-core.git/FITSCore/data/Settings.csv") override;
	};
}