#pragma once

#include <map>
#include <string>

#ifndef SECONDS_PER_DAY
#endif

namespace fits {
	///���;���
	typedef struct ShiftMatrix : std::map<int/*from�ͺ�*/, std::map<int/*to�ͺ�*/, std::pair<int/*����ɰ��ʱ��*/, int/*��ɰ��ʱ��*/>>> {
		void set(int/*�����ͺ�*/from, std::map<int/*to�ͺ�*/, std::pair<int/*����ɰ��ʱ��*/, int/*��ɰ��ʱ��*/>>& toMatrix) {
			(*this)[from] = toMatrix;
		}
	} *PShiftMatrix;
	
	typedef struct MachineClockMatrix : public std::map<int, double> {
	} *PMachineClockMatrix;

	typedef struct ClockMatrix : public std::map<std::string, MachineClockMatrix> {
		std::map<int, int> modelIndexMap;
	} *PClockMatrix;
}