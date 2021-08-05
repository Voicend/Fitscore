#pragma once

#include <map>
#include <string>

#ifndef SECONDS_PER_DAY
#endif

namespace fits {
	///换型矩阵
	typedef struct ShiftMatrix : std::map<int/*from型号*/, std::map<int/*to型号*/, std::pair<int/*不换砂轮时间*/, int/*换砂轮时间*/>>> {
		void set(int/*机器型号*/from, std::map<int/*to型号*/, std::pair<int/*不换砂轮时间*/, int/*换砂轮时间*/>>& toMatrix) {
			(*this)[from] = toMatrix;
		}
	} *PShiftMatrix;
	
	typedef struct MachineClockMatrix : public std::map<int, double> {
	} *PMachineClockMatrix;

	typedef struct ClockMatrix : public std::map<std::string, MachineClockMatrix> {
		std::map<int, int> modelIndexMap;
	} *PClockMatrix;
}