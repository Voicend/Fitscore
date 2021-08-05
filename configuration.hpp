#pragma once

#include <map>
#include <vector>

extern fits::ModelsManager gModels;

namespace fits {
	struct ProductLine;
	typedef std::map<int/*model*/, std::map<int/*process*/, std::map<int/*uid of job*/, JobUnit>>> JobsOfProcess;

	template<class T = PMachine>
	struct BaseConfigration : std::map<int/*第w道序*/, std::map<int/*第x种零件*/, std::vector<T>>> {};

	typedef std::map<int/*第w道序*/, std::map<int/*第x种零件*/, std::map<int/*第z种车床*/, std::vector<PMachine>/*该种车床列表*/>>> ConfigurationMatrix;

	struct Configuration : BaseConfigration<PMachine> {
		Configuration(ConfigurationMatrix matrix, ProductLine& productLine) : pProductLine(&productLine)
		{
			for (auto& e : matrix) {
				int o = e.first;  //道序id
				auto& mapping = e.second;    //分配依据,std::map<int/*第x种零件*/, std::map<int/*第z种车床*/, std::vector<PMachine>/*该种车床列表*/>>

				std::map<int/*第x种零件*/, std::vector<PMachine>> m2m;
				for (auto& e1 : mapping) {
					auto& m2m_mapping = e1.second;   //std::map<int/*第z种车床*/, std::vector<PMachine>/*该种车床列表*/>
					for (auto& e2 : m2m_mapping) {
						for (auto pm : e2.second) {
							///第几种零件换成零件的model-id
							int id = (*pProductLine->pTasks)[e1.first].job.model;
							m2m[id].push_back(pm);
							if (pm->mode < 0) {	///已经有工作的机器不能直接覆盖模式，需要在calculamachineAssigner.pre_best[e_1.first][e_2.first]teTimecostForConfiguration计算换型时间后切换
								pm->mode = id;
							}
						}
					}
				}
				(*this)[o] = m2m;
			}
		}
		Configuration(ProductLine& productLine) : pProductLine(&productLine){
			for (int i = 0; i < productLine.size(); i++) {
				for (auto& machineTemp : productLine[i]) {
					(*this)[i][machineTemp.mode].push_back(&machineTemp);
				}
			}
		}
		Configuration() : pProductLine(nullptr) {};
		PProductLine pProductLine = nullptr;

		void refreshCfgMachinesModeAndToIdle(ProductLine& productLine) {
			for (auto& e1 : *this) {
				auto& machines = productLine[e1.first];
				std::map<int, PMachine> mm = {};
				for (auto& e : machines) {
					mm[e._index] = &e;
				}
				for (auto& e : e1.second) {
					for (auto& e2 : e.second) {
						e2->toIdle = mm[e2->_index]->toIdle;
						e2->mode = mm[e2->_index]->mode;
					}
				}
			}
		}

#if 0
		void refreshCfgMachinesModeAndToIdleWithBestConfig(ConfigurationShadow& bestConfig, ProductLine& productLine) {
			//在枚举仿真之前，需要对枚举产生的最优配置bestConfig同步真实机器信息productLine来开始模拟
			for (auto& e1 : bestConfig) {
				for (auto& e2 : e1.second) {
					for (auto& e3 : e2.second) {
						for (auto machines : productLine) {
							for (auto machine : machines) {
								if (machine._index == e3._index) {
									e3.toIdle = machine.toIdle;
									e3.mode = machine.mode;
								}
							}
						}
					}
				}
			}
			for (auto& e1 : bestConfig) {
				for (auto& e2 : e1.second) {
					for (auto& e3 : e2.second) {
						(*this)[e1.first][e2.first].push_back(&e3);
					}
				}
			}
		}
#endif

		/// 纠正多余的换型
		/// FIXME: 可能有问题
		void correctRedundantCO();

		/// 纠正不合理换型??? @deprecated
		void modifyUnresonable() {
			for (auto& e1 : *this) {
				while (true) {
					int flag = 0;
					for (auto& e2 : e1.second) {
						int testModelId = e2.first;
						for (auto& e3 : e1.second) {
							if (e3.first != testModelId) {
								for (auto& e4 : e3.second) {
									if (e4->mode != e3.first && testModelId == e4->mode) {
										for (auto& e5 : e2.second) {
											if (e5->mode != testModelId) {
												PMachine temp1 = e5;
												e5 = e4;
												e4 = temp1;
												flag = 1;
											}
										}
									}
								}
							}
						}
					}
					if (flag == 0)
						break;
				}
			}
		}

		double getCapacityBymodelId(int id) {
			double minCapacity = (std::numeric_limits<int>::max)() * 1.0;
			for (auto& process : gModels.get(id).processes) {
				//FIXME
				if (process == 3 || process == 5)
					continue;
				//计算每个道序的产能
				double PiecesPerSecond = 0;
				for (auto machine : (*this)[process][id]) {
					PiecesPerSecond += 1 / (machine->clock(id));
				}
				if (PiecesPerSecond < minCapacity)
					minCapacity = PiecesPerSecond;
			}
			return minCapacity;
		}
	};
	struct ConfigurationShadow : BaseConfigration<Machine> {
		std::map<int, int> getConfig() {
			std::map<int, int> config = {};
			for (auto& e1 : *this) {
				for (auto& e2 : e1.second/*map of {model:vector of machines}*/) {
					for (auto& machine : e2.second/*vector of machines*/) {
						config[machine._index] = e2.first/*target mode*/;
					}
				}
			}
			return config;
		}
		std::map<int, int> getConfig(ProductLine& pl, JobsOfProcess jobs) {
			/// 统计每种型号进展到哪个道序
			std::map<int/*model*/, int/*process*/> left = {};
			for (auto& e : jobs) {
				int i = 0;
				for (; i < 10 && jobs[e.first][i].empty(); i++) {}
				left[e.first] = i;
				Utils::log(0, "-- mode %s(%d) left side(process): %d\n", gModels.get(e.first).name.data(), e.first, i);
			}

			///
			std::map<int, int> config = {};
			for (auto& e1 : *this) {
				std::vector<Machine> machinesTemp = pl[e1.first];
				for (auto& e2 : e1.second/*map of {model:vector of machines}*/) {
					bool isModelInProcess = false;
					//如果本道序没有目标换型零件，应该在不打断其他零件的前提下直接换型
					for (auto& machine : pl[e1.first]) {
						isModelInProcess = (machine.mode == e2.first);
						if (isModelInProcess) {
							break;
						}
					}
					for (auto& machine : e2.second/*vector of machines*/) {
						bool isOnlyOneMachineInProcess = false;
						int modeBefore = (pl.machines)[machine._index]->mode;
						int process = machine.process;
						int MachineCountForOriginalModel = 0;
						//判断该机器是否是该道序唯一生产该零件的机器
						//test
						for (auto& machineTemp : machinesTemp) {
							for (auto& e : config) {
								if (machineTemp._index == e.first) {
									machineTemp.mode = e.second;
								}
							}

						}
						for (auto& machineTest : machinesTemp) {
							if (machineTest.mode == modeBefore)
								MachineCountForOriginalModel++;
						}
						if (MachineCountForOriginalModel <= 1) {
							isOnlyOneMachineInProcess = true;
						}
						std::cout << "process:" << machine.process << "machine.mode" << modeBefore << "machine.index:" << machine._index << " MachineCountForOriginalModel:" << MachineCountForOriginalModel << "\n";
						if (!machine.avaliable()) continue;

						else if (modeBefore == machine.mode) continue;	///没换型不需要返回
						else if (process > left[modeBefore]	///但是因为已有任务还没到这个道序 
							&& left[modeBefore] != 10 //该机器空闲
							&& (/*isModelInProcess || */isOnlyOneMachineInProcess)
							&& modeBefore != -1	///并且这个机器没有分配过型号
							) {
#if 1
							Utils::log(2, "config denined: machine %s @ process: %d C/O left[%d(%s)] = %d, from mode = %d -> %d\n",
								machine.fullname().data(),
								process,
								modeBefore, gModels.get(modeBefore).name.data(),
								left[modeBefore],
								modeBefore, e2.first
							);
#endif
							continue;
						}
						config[machine._index] = e2.first/*target mode*/;
					}
				}
			}
			////粗略得处理错误?Q型
			//while (true) {
			//	int isMistake = 0;
			//	for (auto& e1 : config) {
			//		int modeAfter = e1.second;
			//		for (auto& e2 : config) {
			//			int modeBefore = pl.machines[e2.first]->mode;
			//			if (modeAfter == modeBefore) {
			//				int temp = e1.second;
			//				e1.second = e2.second;
			//				e2.second = temp;
			//				isMistake = 1;
			//			}
			//		}
			//	}
			//	//删除这里多余得记录
			//	auto it = config.begin();
			//	while (it != config.end()) {
			//		int modeBefore = (pl.machines)[(*it).first]->mode;
			//		if (modeBefore == (*it).second) {
			//			it = config.erase(it);
			//		}
			//		else
			//			it++;
			//	}
			//	if (isMistake == 0)
			//		break;
			//}

			return config;
		}
		void transferCfgToCfgShadow(Configuration & cfg) {
			*this = {};
			for (auto& e1 : cfg) {
				for (auto& e2 : e1.second) {
					for (auto& e3 : e2.second) {
						(*this)[e1.first][e2.first].push_back(*e3);
					}
				}
			}
		}
		/*piecePerSecond*/
		//FIXME
		double getCapacityBymodelId(int id) {
			double minCapacity = (std::numeric_limits<int>::max)() * 1.0;
			for (auto& process : gModels.get(id).processes) {
				if (process == 3 || process == 5)
					continue;
				//计算每个道序的产能
				double PiecesPerSecond = 0;
				for (auto machine : (*this)[process][id]) {
					PiecesPerSecond += 1 / (machine.clock(id));
				}
				if (PiecesPerSecond < minCapacity)
					minCapacity = PiecesPerSecond;
			}
			return minCapacity;
		}
		void modifyUnreasonable(ProductLine & pl) {
			std::map<int, int> COTableTemp = {};
			for (auto& process : (*this)) {
				while (true) {
					int isAssignmentReasonable = 0;
					for (auto& model : process.second) {
						for (auto& machine : model.second) {
							if (model.first != pl.machines[machine._index]->mode) {
								int modeAfter = model.first;
								for (auto& modelCompare : process.second) {
									for (auto& machineCompare : modelCompare.second) {
										if (pl.machines[machineCompare._index]->mode == modeAfter &&
											pl.machines[machineCompare._index]->mode != modelCompare.first
											) {
											int temp = machine._index;
											machine._index = machineCompare._index;
											machineCompare._index = temp;
											isAssignmentReasonable = 1;
										}
									}
								}

							}
						}
					}
					if (isAssignmentReasonable == 0)break;
				}

			}
		};
	};

	typedef Configuration* PConfiguration;
}