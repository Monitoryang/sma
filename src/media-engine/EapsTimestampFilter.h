#pragma once
#include<cmath>
#if defined(_MSC_VER) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(WIN32)
#include<Eigen/Dense>
#else
#include<eigen3/Eigen/Dense>
#endif
#include<vector>
#include<deque>
#include<mutex>
namespace eap {
	namespace sma {
		class TimeStampFilter {
		public:
			TimeStampFilter(double frame_rate = 30, double time_base = 1.0 / 90000);
			~TimeStampFilter();
			double update(double timestamp);
			//在视频重启或者选择重新开始的时候设置true，也可以直接重新实例化这个类
			void set_first_flag(bool restart);
			inline void set_timestamp_gap(double frame_rate = 30, double time_base = 1 / 90000) {
				_timebase = time_base;
				_framerate = frame_rate;
				_timestamp_gap = 1.0 / _framerate / _timebase;
			}
		private:
			double poly_func(double x, const Eigen::Vector3d& p);
			Eigen::Vector3d least_squares_fit(const std::vector<double>& x_data, const std::vector<double>& y_data);
			double compute_std_dev(const std::vector<double>& window_data);
			double slide_window_fit(std::vector<double>& current_window, double timestamp);
		private:
			std::mutex _data_mtx{};
			std::deque<double> _data{};
			bool _first_round{ true };
			int _window_size{ 10 };
			int _max_length{ _window_size * 3 };
			double _timebase{ 1.0 / 90000 };
			double _framerate{ 30 };
			double _timestamp_gap{ 3000 };
		};
	}
}