#pragma once

#include <map>
#include <vector>
#include <queue>
#include <string>

namespace fits {
	typedef struct ShiftBookItem {
		std::string target;	///�������fullname
		int beginTime = 0;
		int interval = 0;
		int endTime() { return beginTime + interval; }
		enum Type {
			SBI_UNSET = -1,
			SBI_ON = 0,		///����
			SBI_OFF,		///����
		} type = SBI_UNSET;

		ShiftBookItem() {}

		ShiftBookItem(const std::string& target, Type type, int beginTime) 
			: target(target), type(type), beginTime(beginTime) {}
	} *PShiftBookItem;

	extern bool operator< (const ShiftBookItem& lhs, const ShiftBookItem& rhs);

	typedef struct ShiftBookManager : std::priority_queue<ShiftBookItem> {
		static ShiftBookManager* getShiftBoobManager() {
			return instance ? instance : instance = new ShiftBookManager();
		}

	private:
		ShiftBookManager() {}
		static ShiftBookManager* instance;

	} *PShiftBookManager;
}
