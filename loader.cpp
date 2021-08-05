#include "common.hpp"
#include "utils.hpp"
#include "models.hpp"
#include "loader.hpp"
#include "taskinfo.hpp"
#include "jobs.hpp"
#include "tasks.hpp"
#include "productline.hpp"
#include "shiftbook.hpp"
#include "globlecontext.hpp"

#include <json/json.h>
#include<json/reader.h>
using namespace fits;

int
ShiftBookLoader::load(ShiftBookManager& matrix, const std::string& filename/* = "./data/ShiftBook.csv"*/) {
    int result = CSVLoader(filename).load([&](int, Row & row) {
        auto& target = row[0];
        int beginTime = std::stoi(row[1]);
        int interval = std::stoi(row[2]);

        /// NOW, support machine ON/OFF only
        matrix.push({ target, ShiftBookItem::SBI_OFF, beginTime });
        matrix.push({ target, ShiftBookItem::SBI_ON, beginTime + interval });

    });
    return result;
}

#if 1
int
COMatrixLoader::load(ProductLineShiftMatrix & matrix, const std::string & filename/* = "./data/ChangeOverMatrix.csv"*/) {
    int result = CSVLoader(filename).load([&](int i, Row & row) {
        if (i == 0) {
            for (int i = 2; i < row.size(); i++) {
                auto value = row[i];
                this->columns[i - 2] = std::stoi(value);
            }
            return;
        }

        ShiftMatrix& machine = matrix[row[0]];	///machine model name, like "Liebherr"
        //ShiftMatrix : std::map<int/*from型号*/, std::map<int/*to型号*/, std::pair<int/*不换砂轮时间*/, int/*换砂轮时间*/>>>
        auto& from = machine[std::stoi(row[1])];
        for (int i = 2; i < row.size(); i++) {
            int to = this->columns[i - 2];
            auto value = row[i];
            if (value.empty()) continue;
            size_t pos = value.find('/');
            if (pos != std::string::npos) {
                auto left = std::stoi(value.substr(0, pos)),
                        right = std::stoi(value.substr(pos + 1));
                from[to] = { left, right };
            }
            else {
                from[to] = { std::stoi(value), 0 };
            }
        }
    });
    return result;
}
#endif

#if 1
int
ModelProcessesMatrixLoader::load(ModelsManager & matrix, const std::string & filename/* = "./data/ModelProcessesMatrix.csv"*/) {
    int result = CSVLoader(filename).load([&](int, Row & row) {
        auto index = std::stoi(row[0]);
        std::vector<int> processes = {};
        for (int i = 2; i < row.size(); i++) {
            if (row[i] == "1") processes.push_back(i - 2);
        }
        matrix.push_back(Model(index, row[1], processes));
    });
    return result;
}
#endif

int
#if 1
ClockMatrixLoader::load(ClockMatrix & matrix, const std::string & filename/* = "./data/MachineClockMatrix.csv"*/) {
    int result = CSVLoader(filename).load([&](int, Row & row) {
        auto nameOfMachineModel = row[0];
        auto& e = matrix[nameOfMachineModel];
        for (int i = 1; i < row.size(); i++) {
            std::string value = row[i];
            if (Utils::trim(value).empty()) value = "-1";
            int modelId = matrix.modelIndexMap[i - 1];
            e[modelId] = std::stod(value);
        }
    });
    return result;
}
#endif

#if 1
int
ProductLineMatrixLoader::load(ProductLine & matrix, const std::string & filename/* = "./data/ProductLineMatrix.json"*/) {
    std::ifstream is(filename);
    if (!is) {
        Utils::log(1, "failed to open requirements file: %s\n", filename.data());
        return -1;
    }
    Json::Value productLineJsonString;
    is >> productLineJsonString;
    is.close();
    for (auto& process : productLineJsonString) {
        std::vector<Machine> processTemp = {};
        for (auto& machine : process["machines"]) {
            bool isGeneric = std::stoi(machine["generic"].asString());
            int processIndex = std::stoi(process["process"].asString());
            std::string machineName = machine["machineName"].asString();
            std::string machineNumber = machine["machineNumber"].asString();
            int bufferSize = std::stoi(machine["bufferSize"].asString());
            if (bufferSize == 0) {
                Utils::log(-1, "machine(%s_&s) bufferCapacity=0\n", &machineName, &machineNumber);
                return -1;
            }
            //if (isGeneric) {
            ////FIXME
            //	//bufferSize = INT_MAX;
            //	bufferSize = 32;
            //}
            std::string  machineStatusToString = machine["status"].asString();
            std::string  machineStateToString = machine["state"].asString();
            int machineState = machineStateToString == "" ? 0 : std::stoi(machineStateToString);
            machineState = machineState == 0 ? 0 : 1;
            int machineStatus = machineStatusToString == "" ? 0 : std::stoi(machine["status"].asString());
            std::string  toIdleToString = machine["toIdle"].asString();
            double toIdle = toIdleToString == "" ? 0 : std::stod(toIdleToString);
            std::string COTargetToString = machine["COTarget"].asString();
            int COTarget = COTargetToString == "" ? -1 : std::stoi(machine["COTarget"].asString());
            if (COTarget != -1 && machineStatus != 2) {
                Utils::log(-1, "machine status is not right for COTarget \n", &machineName, &machineNumber);
                return -1;
            }
            std::string modeToString = machine["mode"].asString();
            int mode = modeToString == "" ? -1 : std::stoi(machine["mode"].asString());

            Machine machineTemp(processIndex, machineNumber, machineName, { bufferSize,bufferSize }, isGeneric);
            machineTemp.setState(Machine::MachineState(machineState));
            if (machineState == Machine::S_ONLINE)
                machineTemp.status = Machine::MachineStatus(machineStatus);
            else
                machineTemp.status = Machine::MS_IDLE;
            machineTemp.COTarget = COTarget;
            machineTemp.toIdle = toIdle;
            machineTemp.mode = mode;

            for (auto& jobUnit : machine["inputBuffer"]) {
                if (machineState != Machine::S_ONLINE)
                    break;
                //int uid = std::stoi(jobUnit["uid"].asString());
                int model = std::stoi(jobUnit["model"].asString());
                int ioo = std::stoi(jobUnit["ioo"].asString());
                int process = std::stoi(jobUnit["process"].asString());
                //int releaseTime = std::stoi(jobUnit["releaseTime"].asString());
                //int machineId = std::stoi(jobUnit["machineId"].asString());
                int daysIndex = std::stoi(jobUnit["daysIndex"].asString());
                int uid = UID::generate();
                JobUnit jobUnitTemp(uid, model, ioo, process, /*releaseTime,*/ /*machineId,*/ daysIndex);
                jobUnitTemp.releaseTime = 0;
                machineTemp.inputBuffer.push(jobUnitTemp);
            }
            for (auto& jobUnit : machine["producting"]) {
                if (machineState != Machine::S_ONLINE)
                    break;
                if (!isGeneric) {
                    //int uid = std::stoi(jobUnit["uid"].asString());
                    int model = std::stoi(jobUnit["model"].asString());
                    int ioo = std::stoi(jobUnit["ioo"].asString());
                    int process = std::stoi(jobUnit["process"].asString());
                    //int releaseTime = std::stoi(jobUnit["releaseTime"].asString());
                    //int machineId = std::stoi(jobUnit["machineId"].asString());
                    int daysIndex = std::stoi(jobUnit["daysIndex"].asString());
                    int uid = UID::generate();
                    JobUnit jobUnitTemp(uid, model, ioo, process, /*releaseTime,*/ /*machineId,*/ daysIndex);
                    jobUnitTemp.releaseTime = 0;
                    machineTemp.model.push(jobUnitTemp);
                }
                else {
                    //int uid = std::stoi(jobUnit["uid"].asString());
                    int model = std::stoi(jobUnit["model"].asString());
                    int ioo = std::stoi(jobUnit["ioo"].asString());
                    int process = std::stoi(jobUnit["process"].asString());
                    //int releaseTime = std::stoi(jobUnit["releaseTime"].asString());
                    //int machineId = std::stoi(jobUnit["machineId"].asString());
                    int daysIndex = std::stoi(jobUnit["daysIndex"].asString());
                    int uid = UID::generate();
                    JobUnit jobUnitTemp(uid, model, ioo, process, /*releaseTime,*/ /*machineId,*/ daysIndex);
                    jobUnitTemp.releaseTime = 0;
                    machineTemp.bufferSize.first++;
                    machineTemp.bufferSize.second++;
                    machineTemp.inputBuffer.push(jobUnitTemp);
                }
            }
            if (!machine["producting"].empty() && machineTemp.getState() == Machine::S_ONLINE)
                machineTemp.status = Machine::MS_WORKING;
            for (auto& jobUnit : machine["outputBuffer"]) {
                //int uid = std::stoi(jobUnit["uid"].asString());
                int model = std::stoi(jobUnit["model"].asString());
                int ioo = std::stoi(jobUnit["ioo"].asString());
                int process = std::stoi(jobUnit["process"].asString());
                //int releaseTime = std::stoi(jobUnit["releaseTime"].asString());
                //int machineId = std::stoi(jobUnit["machineId"].asString());
                int daysIndex = std::stoi(jobUnit["daysIndex"].asString());
                int uid = UID::generate();
                JobUnit jobUnitTemp(uid, model, ioo, process, /*releaseTime,*/ /*machineId,*/ daysIndex);
                jobUnitTemp.releaseTime = 0;
                machineTemp.outputBuffer.push(jobUnitTemp);
            }
            processTemp.push_back(machineTemp);
        }
        matrix.push_back(processTemp);
    }
    return 0;
}
#endif
#if 1
int fits::GlobleContextLoader::load(GlobleContextManager & context, const std::string & filename)
{
    int result = CSVLoader(filename).load([&](int, Row & row) {
        if (row[0] == "MQInfo") {
            context.mqHost = row[1];
            context.mqUsername = row[2];
            context.mqPassword = row[3];
            context.mqQueueName = row[4];
            context.port = std::stoi(row[5]);
        }
        else if (row[0] == "WindowPhase") {
            std::string value = row[1];
            if (Utils::trim(value).empty()) value = "3";
            context.windowPhase = std::stoi(value);

            value = row[2];
            if (Utils::trim(value).empty()) value = "3";
            context.windowMaxPreFetchDays = std::stoi(value);	///最多预取多少天内的任务

            value = row[3];
            if (Utils::trim(value).empty()) value = "237600";
            context.windowMaxDelaySeconds = std::stoi(value);	///最大可延迟交付时间
        }
        else if (row[0] == "fits.core.co.susceptibility") {
            std::string value = row[1];
            if (Utils::trim(value).empty()) value = "1";
            context.coSusceptibility = std::stod(value);	///换型敏感度
        }
        else if (row[0] == "fits.machine.coldtime") {
            std::string value = row[1];
            if (Utils::trim(value).empty()) value = "3600";
            context.offset = std::stoi(value);	///冷却时间
        }
        else if (row[0] == "fits.core.time-length-of-day") {
            std::string value = row[1];
            if (Utils::trim(value).empty()) value = "86400";
            context.workingTimePerDay = std::stoi(value);	///一日干活时间
        }
        else if (row[0] == "fits.simulator.checktime") {
            std::string value = row[1];
            if (Utils::trim(value).empty()) value = "60";
            context.checkTime = std::stoi(value);	///检查?型机器时间
        }
        else if (row[0] == "fits.simulator.endless-loop-waitTime") {
            std::string value = row[1];
            if (Utils::trim(value).empty()) value = "50000";
            context.endlessLoopWaitTime = std::stoi(value);	///跳出循环时间防止卡死
        }
    });
    return result;
}
#endif