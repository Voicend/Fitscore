#include "common.hpp"
#include "utils.hpp"
#include "models.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"
#include "machine.hpp"
#include "shiftbook.hpp"
#include "productline.hpp"
#include "configuration.hpp"
#include "simulator.hpp"
#include "mq.hpp"
#include "globlecontext.hpp"

#include <json/json.h>

#pragma comment(lib, "lib/jsoncpp.lib")

using namespace fits;

void fits::Simulator::log(PMachine m, PJobUnit j) {
	GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
//v	fits::MQ& MQS = *fits::MQ::getMQ(gContext.mqHost, gContext.mqUsername, gContext.mqPassword, gContext.mqQueueName, gContext.port);
	Json::Value e;
	e["type"] = 1;
	e["processId"] = m->process;
	e["machineId"] = m->_index;
	e["machineName"] = m->fullname();
	//e["machineMode"] = gModels.get(m->mode).name;	///use model code
	e["machineMode"] = gModels.get(j->model).name;
	//e["targetExt"] = Utils::standard_to_stamp(j->taskInfo.date);
	e["targetExt"] = std::to_string(j->daysIndex);
	e["target"] = j->uid;
	e["targetCode"] = /*"";*/ gModels.get(j->model).name;	///add 0927
	e["time"] = (int)realTime;
	e["id"] = gContext.id;
//v MQS.send("fits.core", e.toStyledString());
//	std::cout << e << '\n';
	std::string s = std::to_string(m->process) + ","
		+ std::to_string(m->_index) + ","
		+ m->name + "_" + m->number + ","
		+ (gModels[j->model].name) + ","
		+ std::to_string(realTime) + ","
		+ std::to_string(j->COT(m->clock(j->model))) + ","
		+ j->taskInfo.date + ","
		+ std::to_string(j->uid) + ","
		+ std::to_string(static_cast<int>(realTime / 86400));
	//+ j->taskInfo.date + gModels[j->model].name;
//	Utils::log("log: %s\n", s.data());
    std::cout<<s<<'\n';
	if (gContext.isProcessDataShown == 1 || gContext.isProcessDataShown == 2) {
		if (of_trayes_log) {
			of_trayes_log << s << "\n";
			of_trayes_log.flush();
		}
	}
	//test
	if (gContext.isProcessDataShown == 1) {
		std::ofstream timeCostPerUnitTest;
		timeCostPerUnitTest.open("timeCostPerUnitTest.csv", std::ofstream::app);
		int lastIndex = gModels[j->model].processes.size() - 1;
		int last = gModels[j->model].processes[lastIndex];
		int first = gModels[j->model].processes[0];
		double timeCostPerUnitInFirstProcess = (*ppl)[0][0].clock(j->model) * JobUnitInfo::CAPACITY;
		if (j->process == last) {
			std::string endTime = std::to_string(realTime);
			std::string ss = std::to_string(j->uid) + "," +
				std::to_string(j->model) + "," +
				endTime;
			timeCostPerUnitTest << ss << "\n";
		}
		if (j->process == first) {
			std::string startTime = std::to_string(realTime - timeCostPerUnitInFirstProcess);
			std::string ss = std::to_string(j->uid) + "," +
				std::to_string(j->model) + "," +
				startTime;
			timeCostPerUnitTest << ss << "\n";
		}
	}
}

int
Simulator::simulate() {
	GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
	auto log_on_machine_status_change = [&](PMachine m)
	{
		if (gContext.isProcessDataShown == 1) {
			statusLogger->write(
				{
						std::to_string(realTime),
						std::to_string(m->_index),
						m->name + "-" + m->number,
						std::to_string(m->process),
						m->getStatusName(),
						std::to_string(m->getCOStatus()),
						std::to_string(m->inputBuffer.size()),
						std::to_string(m->outputBuffer.size()),
						gModels.get(m->mode).name


				});
		}
		if (m->getStatusName() == "C/O") {
			GlobleContextManager& gContext = *GlobleContextManager::getGlobleContextManager();
//v			fits::PMQ MQS = fits::MQ::getMQ(gContext.mqHost, gContext.mqUsername, gContext.mqPassword, gContext.mqQueueName, gContext.port);
			Json::Value e;
			e["type"] = 0;
			e["processId"] = m->process;
			e["machineId"] = m->_index;
			e["machineName"] = m->fullname();
			e["machineMode"] = gModels.get(m->mode).name;
			e["target"] = m->COTarget;
			e["targetCode"] = gModels.get(m->COTarget).name;	///add 0927
			int deviatedRealTime = 0;
			if (realTime - m->toIdle < gContext.offset) {
				deviatedRealTime = (int)realTime + gContext.offset;
				m->toIdle += gContext.offset;
			}
			else {
				deviatedRealTime = (int)realTime;
			}
			e["time"] = deviatedRealTime;
			e["targetExt"] = deviatedRealTime + gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60;//mode->target不换砂轮时间
			e["id"] = gContext.id;
//v			MQS->send("fits.core", e.toStyledString());
//v            std::cout << e << '\n';
		}

	};
	//开始根据配置进行模拟
	PShiftBookManager psbm = ShiftBookManager::getShiftBoobManager();
	bool working = true;
	//test
	int count = 0;
	while (working) {
		//停机表矫正
		if (!psbm->empty()) {
			auto& sbi = psbm->top();
			//将停机时间预警提前到确保前料道加工完
			auto pm = ppl->get(sbi.target);
			double extraTimeCost = 0.;
			if (pm != nullptr) {
				double timeCostPerUnit = pm->clock(pm->mode) * JobUnit::CAPACITY;
				extraTimeCost += timeCostPerUnit * (pm->inputBuffer.size() + 1);
			}
			if (realTime + extraTimeCost >= sbi.beginTime) {
				switch (sbi.type) {
				case ShiftBookItem::SBI_OFF: {
					if (pm) {
						pm->setState(Machine::S_OFFLINE);
						//Utils::log(1, "machine: %s OFF \n", pm->fullname().data());
					}
				} break;
				case ShiftBookItem::SBI_ON: {
					if (pm) {
						pm->setState(Machine::S_ONLINE);
						//Utils::log(1, "machine: %s ON \n", pm->fullname().data());
					}
				} break;
				}
				psbm->pop();
				break;
			}
		}
		/// 从最后道序开始往前遍历
		for (int i = (int)ppl->size() - 1; i >= 0; i--) {
			auto& process = ppl->at(i);
			for (auto& e : process) {
				auto m = &e;
				//如果机器为第一道序则在机器前buffer不满的情况下补满buffer
				if (m->mode >= 0 && m->process == gModels.get(m->mode).processes[0]
					&& m->getState() == Machine::S_ONLINE) {
					if (m->inputBuffer.size() < m->COB() && !JOP[m->mode][-1].empty())
					{
						for (auto& e1 : JOP[m->mode][-1]) {
#if 0
							if (m->status == Machine::MS_CO && m->COTarget == e1.second.model);
							else if (m->generic);
							else if (m->mode == e1.second.model && m->status != Machine::MS_CO);
							else if (m->getCOStatus() == Machine::COS_GOING || m->COTarget == e1.second.model);
							else if (m->mode == e1.second.model && m->getCOStatus() == Machine::COS_GOING) break;
							else {
								break;
							}
#endif
							if (!m->acceptable(e1.second)) break;
//printf("1    :deal with job : %d\n",e1.second.uid);

							working = !moveJobToNextProcess(JOP, e1.second, nullptr, m);

#if 0
							Utils::log(-1, "!!! put job %d %s(%d) on to machine: %s @ %d \n",
								e1.second.uid,
								gModels.get(e1.second.model).name.data(),
								e1.second.model,
								m->fullname().data(),
								m->process);
#endif
							break;
						}
					}
				}
				/*
				 *当时间等于预计的机器空闲时间点时查看 零件能否放到后料道 可以的话状态为0 由下一个判断语句执行，不可以的话
				 *把状态改为3 当前机器里有零件阻塞
				 */
				if (m->status == Machine::MS_WORKING) {
					if (realTime >= m->toIdle) {
						auto e = m->model.front();
						if (m->outputBuffer.size() < m->COB()) {
							m->finishJob(e, (int)realTime);
//v							log(m, &e);
							log_on_machine_status_change(m);
							lastJobReleaseTime = realTime;

						}
						else {
							m->status = Machine::MS_BLOCKING;
							log_on_machine_status_change(m);
						}
					}
				}
				else if (m->status == Machine::MS_CO) {
					if (m->toIdle <= realTime) {
						m->status = Machine::MS_IDLE;
std::cout << m->fullname() << " C/O DONE from " << m->mode << " -> " << m->COTarget<< " wdith input: " << m->inputBuffer.size() <<"     at: " <<realTime<<"\n";
						m->mode = m->COTarget;
						m->COTarget = -1;
						log_on_machine_status_change(m);
					}
				}
				else if (m->status == Machine::MS_BLOCKING) {
					if (m->outputBuffer.size() < m->COB()) {
						auto e = m->model.front();
#if 0
						e.releaseTime = (int)realTime;
						log(m, &e);
						m->model.pop();
						m->outputBuffer.push(e);
						m->toIdle = (int)realTime;
						m->status = Machine::MS_IDLE;
#endif
						//count wait time
						workAndWaitTime[m->_index].second = workAndWaitTime[m->_index].second +
							(realTime > m->toIdle ? realTime - m->toIdle : 0);
						if (gContext.isProcessDataShown == 1) {
							waitLogger->write({
													 std::to_string(m->_index),
													 m->fullname(),
													 std::to_string(m->process),
													 std::to_string(m->toIdle),
													 std::to_string(realTime),
													 std::to_string(realTime - m->toIdle)
							});
						}

						m->finishJob(e, (int)realTime);
//v						log(m, &e);
						log_on_machine_status_change(m);
					}
				}
				else if (m->status == Machine::MS_IDLE) {
					if (m->getCOStatus() == Machine::COS_GOING) {
						if (!m->inputBuffer.empty()) {
							auto& e = m->inputBuffer.front();
							if (e.model == m->COTarget) {
								///note C/O
								coTimeAndCount[m->_index].first = coTimeAndCount[m->_index].first +
									gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60;
								coTimeAndCount[m->_index].second = coTimeAndCount[m->_index].second + 1;
								int outputTime = realTime;
								if (realTime - m->toIdle < gContext.offset) {
									outputTime = realTime + gContext.offset;
								}
								machineCOLogger->write({
															  std::to_string(m->_index),
															  m->fullname(),
															  std::to_string(m->process),
															  std::to_string(outputTime),
															  std::to_string(gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60),
															  std::to_string(m->mode),
															  std::to_string(m->COTarget)
									});
								/// start C/O
								m->changeOver(gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60, (int)realTime);
								log_on_machine_status_change(m);
							}
							else if (e.model == m->mode || m->generic) {
								///count waitTime
								workAndWaitTime[m->_index].second = workAndWaitTime[m->_index].second +
									(realTime > m->toIdle ? realTime - m->toIdle : 0);
								//note waitLoggerm.
								if (realTime > m->toIdle) {
									waitLogger->write({
															 std::to_string(m->_index),
															 m->fullname(),
															 std::to_string(m->process),
															 std::to_string(m->toIdle),
															 std::to_string(realTime),
															 std::to_string(realTime - m->toIdle)
										});
								}
								///count workTime
								double costTime = e.COT(m->clock(e.model));
								workAndWaitTime[m->_index].first = workAndWaitTime[m->_index].first + costTime;
								///note workLogger
								if (gContext.isProcessDataShown == 1) {
									workLogger->write({
															 std::to_string(m->_index),
															 m->fullname(),
															 std::to_string(m->process),
															 std::to_string(realTime),
															 std::to_string(realTime + costTime),
															 std::to_string(costTime)
										});
								}
								m->workOn(e, (int)realTime);
								log_on_machine_status_change(m);
							}
							else {
#if 0							
								Utils::log(-1, "model mis-match: %s @ %d mode: %s(%d) got: %s(%d) \n",
									m->fullname().data(),
									m->process,
									gModels.get(m->mode).name.data(), m->mode,
									gModels.get(e.model).name.data(), e.model);
#endif
								throw "model mismatch!";
							}
						}
						else {
							////count waitTime
							workAndWaitTime[m->_index].second = workAndWaitTime[m->_index].second +
								(realTime > m->toIdle ? realTime - m->toIdle : 0);
							///note waitLogger
							if (realTime > m->toIdle) {
								waitLogger->write({
														 std::to_string(m->_index),
														 m->fullname(),
														 std::to_string(m->process),
														 std::to_string(m->toIdle),
														 std::to_string(realTime),
														 std::to_string(realTime - m->toIdle)
									});
							}

							///note C/O
							coTimeAndCount[m->_index].first = coTimeAndCount[m->_index].first +
								gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60;
							coTimeAndCount[m->_index].second = coTimeAndCount[m->_index].second + 1;
							int outputTime = realTime;
							if (realTime - m->toIdle < gContext.offset) {
								outputTime = realTime + gContext.offset;
							}
							machineCOLogger->write({
														  std::to_string(m->_index),
														  m->fullname(),
														  std::to_string(m->process),
														  std::to_string(outputTime),
														  std::to_string(gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60),
														  std::to_string(m->mode),
														  std::to_string(m->COTarget)
								});
							/// start C/O
							m->changeOver(gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60, (int)realTime);
							log_on_machine_status_change(m);
#if 0
							m->status = Machine::MS_CO;
							m->setCOStatus(Machine::COS_KEEP);
							auto cost = gShiftMatrixs[m->name][m->mode][m->COTarget].first * 60;
							m->toIdle = ((int)realTime) + cost;
#endif
						}
					}
					else if (!m->inputBuffer.empty()) {
						auto& e = m->inputBuffer.front();
						if (e.model == m->mode || m->generic) {
#if 0
							m->status = Machine::MS_WORKING;
							m->toIdle = (int)realTime + e.COT(m->clock(e.model));
							m->model.push(e);
							m->inputBuffer.pop();
#endif
							///count waitTime
							workAndWaitTime[m->_index].second = workAndWaitTime[m->_index].second +
								(realTime > m->toIdle ? realTime - m->toIdle : 0);

							///note waitLogger
							if (realTime > m->toIdle) {
								waitLogger->write({
														 std::to_string(m->_index),
														 m->fullname(),
														 std::to_string(m->process),
														 std::to_string(m->toIdle),
														 std::to_string(realTime),
														 std::to_string(realTime - m->toIdle)
									});
							}
							///count workTime
							double costTime = e.COT(m->clock(e.model));
							workAndWaitTime[m->_index].first = workAndWaitTime[m->_index].first + costTime;
							///note workLogger
							workLogger->write({
													 std::to_string(m->_index),
													 m->fullname(),
													 std::to_string(m->process),
													 std::to_string(realTime),
													 std::to_string(realTime + costTime),
													 std::to_string(costTime)
								});
							log_on_machine_status_change(m);
							m->workOn(e, (int)realTime);
						}
						else if (!m->generic) {
#if 0
							Utils::log(-1, "model mis-match: %s @ %d mode: %s(%d) got: %s(%d) \n",
								m->fullname().data(),
								m->process,
								gModels.get(m->mode).name.data(), m->mode,
								gModels.get(e.model).name.data(), e.model);
#endif
							Utils::log(-1, "inputBuffer jobs dismatch machine mode");
							return -8;
						}
					}
					else {
						/// Wait for pre-processes
					}
				}
#if 1
				//如果机器的后料道不为空，就尝试向下一道序运送零件，如果为最后一道序，则直接pop出来，如果无法向下运则不运
				if (!m->outputBuffer.empty()) {
					auto e0 = m->outputBuffer.front();
					//获取该道序后道序存在该零件的机器
					///FIXME：config中有道序>型号>可用机器，不需要每次都遍历查找
					std::vector<PMachine> macs = {};
#if 0
					std::for_each(ppl->machines.begin(), ppl->machines.end(), [&](PMachine p) {
						if (p->process == m->process
							&& !p->outputBuffer.empty() && p->outputBuffer.front().model == e0.model) {
							macs.push_back(p);
						}
						});
#endif
					int nextProcess = m->process + 1;
					if (ppl->isGenericProcess(nextProcess)) {
						std::for_each(ppl->machines.begin(), ppl->machines.end(), [&](PMachine p) {
							if (p->process == m->process
								&& !p->outputBuffer.empty()) {
								macs.push_back(p);
							}
							});
					}
					else {
						std::for_each(ppl->machines.begin(), ppl->machines.end(), [&](PMachine p) {
							if (p->process == m->process
								&& !p->outputBuffer.empty() && p->outputBuffer.front().model == e0.model) {
								macs.push_back(p);
							}
							});
					}
					//从macs获取零件时间最早的那一个零件向下传
					auto it = std::min_element(macs.begin(), macs.end(),
						[&](PMachine l, PMachine r) { return l->outputBuffer.front().releaseTime < r->outputBuffer.front().releaseTime; });
					PMachine macBest = (it != macs.end() ? *it : nullptr);
					if (!macBest) {
						throw "exception: no machine!";
						continue;
					}

					auto& e1 = macBest->outputBuffer.front();
					auto mi = gModels.get(e1.model);
					const int FINAL = mi.processes.back();
					if (macBest->process == FINAL) {
//printf("2    :deal with job : %d\n",e1.uid);
						working = !moveJobToNextProcess(JOP, e1, macBest, nullptr, true);
						continue;
					}

					const int COUNT = (int)macBest->outputBuffer.size();
					int tries = 0;
					std::queue<JobUnit> push_back = {};
					while (tries < COUNT)
					{
						auto& e1 = macBest->outputBuffer.front();

						//如果机器不是最后一道序查看下一道序是否是通用道序并且不是最后一道序 这块特殊处理
						int next = mi.processes[(long long)e1.ioo + 1];
						{
							std::vector<PMachine> macs2;
							for (auto& mac : (*ppl)[next]) {
#if 0
								if (mac.inputBuffer.size() >= m->COB()) continue;
								else if (mac.status == Machine::MS_CO && mac.COTarget == e1.model) continue;
								else if (mac.mode == e1.model && mac.getCOStatus() == Machine::COS_GOING) continue;
								else if (mac.generic) macs2.push_back(&mac);
								else if (mac.mode == e1.model && mac.status != Machine::MS_CO) macs2.push_back(&mac);
								else if (mac.getCOStatus() == Machine::COS_GOING || mac.COTarget == e1.model) macs2.push_back(&mac);
								else {
									//std::cout << "no machine available \n";
								}
#endif
								if (mac.acceptable(e1)/*exclude offLineMachine*/ && mac.getState() != Machine::S_OFFLINE && mac.getState() != Machine::S_PAUSE)
									macs2.push_back(&mac);
							}

							//选出最早空闲的机器，把零件传下去
							auto it = std::min_element(macs2.begin(), macs2.end(),
								[&](PMachine l, PMachine r) {
									return (l->toIdle + l->getCOTimeCost()) < (r->toIdle + r->getCOTimeCost());
								});
							if (it != macs2.end()) {
//printf("3    :deal with job : %d \n",e1.uid);
								working = !moveJobToNextProcess(JOP, e1, macBest, *it);
								break;
							}
							else if (macBest->generic) {
								push_back.push(e1);
								macBest->outputBuffer.pop();
							}
						}

						if (!macBest->generic) break;
						tries += 1;
					} //end of while: if generic machine, check every job in output queue until one match or all failed
					if (!push_back.empty()) {
						while (!push_back.empty()) {
							macBest->outputBuffer.push(push_back.front());
							push_back.pop();
						}
						//Utils::log(1, "stuck on machine: %s \n", macBest->fullname().data());
					}

					if (tries == COUNT) {
						//Utils::log(-1, "stuck on machine: %s \n", macBest->fullname().data());
					}
				}
				if (!working) break;
#endif
			}	//end of machines in process loop
			if (!working) break;
		}	//end of processes loop
		realTime++;
		count++;
		//int initialCount = realTime > 100 ? 60 : 1;
		int initialCount = gContext.checkTime;
		if (initialCount == count) {
			int intervalTime = gContext.offset;
			for (auto& machine : ppl->machines) {
				if (realTime - std::abs(machine->toIdle) > intervalTime && machine->status == Machine::MS_IDLE && machine->getState() != Machine::S_OFFLINE/* && machine->getState() != Machine::S_PAUSE*/) {
					//if (machine->process <= left[machine->mode] && machine->status == Machine::MS_IDLE) {
					triggerIndexVec.push_back(machine->_index);
				}
			}
			if (!triggerIndexVec.empty()) {
				//std::cout << "realTime: " << realTime << "\n";
				break;
			}
			count = 0;
		}
	}

	for (auto& e : JOP) {
		for (auto& e1 : e.second) {
			for (auto& e2 : e1.second) {
				if (e2.second.machineId >= 0) continue;
				jobs.push(e2.second);
			}
		}
	}

	return 0;

}

