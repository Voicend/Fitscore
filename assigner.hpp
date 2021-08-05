#pragma once

namespace fits {

    typedef std::map<int/*i道工序*/, std::map<int/*j种车床*/, std::vector<PMachine>/*的引用列表*/>> MachineMatrix;

    /**
 * 假设当前工序有N种车床，用矩阵A[1...N]来表示, A[i]表示第i种车床的数量
 * 要加工K种零件, 枚举出把这些车床分配给这K种零件的所有方案(且保证每种零件至少有一台车床加工)
 */
    struct MachineAssignerForProductLine {
        //std::map<int/*i道工序*/, std::map<int/*j种车床*/, int/*的数目*/>>
        MachineMatrix A;   //第i道工序下第j种车床的数目
        //std::map<int/*第w道序*/, std::map<int/*第x种零件*/, std::map<int/*第z种车床*/, int/*该种车床个数*/>>>
        //MOD
        bool check = false;
        ConfigurationMatrix assignment, best,bestTest;
        int K;  //零件种类数
        double bestTimeCost,bestTimeCostTest;   //已知最短时间
        int countChangeType = 0;
        int countChangeTypeTest = 0;
        RequirementTasks tasks;	///当前assign的任务
        PProductLine pProductLine;
        ConfigurationShadow bestConfiguration;
        ///snapshot保存所有机器的初始状态
        std::map<int/*global index*/, MachineRuntimeInfo> snapshotOfMachines, bestSnapshotOfMachines;

        MachineAssignerForProductLine(MachineMatrix& matrixOfMachines, ProductLine& productLine)
                : A(matrixOfMachines), K(0), bestTimeCost(std::numeric_limits<int>::max()), countChangeType(0), pProductLine(&productLine)
        {
            pProductLine->snapshot(snapshotOfMachines);
        }

        ///reset machines to init status
        void reset() {
            pProductLine->restore(snapshotOfMachines);
            assignment.clear();
            best.clear();
            bestTimeCost = std::numeric_limits<int>::max();
        }

        void assign(RequirementTasks& tasks) {
            /*if (tasks.size() > 2) {*/
            K = static_cast<int>(tasks.size());
            assignment.clear();
            this->tasks = tasks;
            pProductLine->pTasks = &this->tasks;
            assign(0, 0, 0, A[0][0]);
        }

    private:
        //排列组合
        std::vector<std::vector<int>> record;
        void comb(std::vector<int> result, int  allSize, int start, int count) {
            if (result.size() == 0)
                return;
            for (int i = start; i < allSize + 1 - count; i++) {
                result[(long long)count - 1] = i;
                if (count - 1 == 0)
                    record.push_back(result);
                else
                    comb(result, allSize, i + 1, count - 1);
            }
        }

        //@w：第几道序
        //@z: 第几种车床
        //@x：第几种零件
        //@y：该种车床还剩下的数量
        //void assign(int w, int z, int x, int y) {
        void assign(int w, int z, int x, std::vector<PMachine> y);
    private:
        //@skip: 某些零件可以跳过某些道序
        bool isAllModelSatisfiedForOrderN(int w) {
            for (int i = 0; i < K; i++) {
                ///不需要的道序跳过
                auto& processes = gModels[tasks[i].job.model].processes;
                bool skip = (std::count(processes.begin(), processes.end(), w) <= 0);
                if (skip) {
                    //std::cout << processes.size() << "\n";
                    continue;
                }

                int occupied = 0;
                for (auto& km : assignment[w][i])	//每种车床几台
                {
                    occupied += (int)km.second.size();
                }
                if (!occupied) {
                    //std::cout << std::string("assignment[w][i]:") << assignment[w][i].size() <<",w:" << w << ",i:" << i << "\n";
                    return false;
                }
            }

            return true;
        }
    };
}