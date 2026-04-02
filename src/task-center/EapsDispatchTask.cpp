#include "EapsDispatchTask.h"

namespace eap {
	namespace sma {
		DispatchTask::DispatchTask(InitParameter& init_parameter)
			: _init_parameter(init_parameter)
		{
		}

		DispatchTask::~DispatchTask()
		{
		}
		void DispatchTask::snapshot(std::string recordNo, int interval, int total_time)
		{
			_snapshot = true;
			_snapshot_sd = true;
			_recordNo = recordNo;
		}
		void DispatchTask::videoClipRecord(int record_duration, std::string recordNo)
		{
			_record_duration = (record_duration + 1) * 1e3;
			_record = true;
			_record_sd = true;
			_recordNo = recordNo;
		}
		void DispatchTask::fireSearchInfo(const std::string id, const double target_lat, const double target_lon, const double target_alt)
		{
		}
		void DispatchTask::setAirborne45G(const int airborne_45G)
		{
		}
	}
}