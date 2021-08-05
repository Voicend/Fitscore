#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>

namespace fits {
	typedef struct Model {
		int id = 0;
		const std::string name;
		///���ͺ���Ҫ�����ĵ���
		std::vector<int> processes;

		Model(int id, const std::string& name) : Model(id, name, {}) {
		}

		Model(int id, const std::string& name, std::vector<int> processes) : id(id), name(name), processes(processes) {
		}
	} *PModel;

	typedef struct ModelsManager : public std::vector<Model> {
		///����model.id����
		Model& get(int id) {
			auto it = std::find_if(begin(), end(), [&](Model & m) { return m.id == id; });
			if (it != end()) return *it;
			return *InvalidModel;
		}
		///����model.name����
		Model& get(const std::string & name) {
			auto it = std::find_if(begin(), end(), [&](Model & m) { return m.name == name; });
			if (it != end()) return *it;
			return *InvalidModel;
		}
		Model& operator[](int id) {
			return get(id);
		}
	private:
		static std::unique_ptr<Model> InvalidModel;
	} *PModelsManager;
}