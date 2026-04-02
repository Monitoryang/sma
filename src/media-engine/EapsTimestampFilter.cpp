#include "EapsTimestampFilter.h"
#include<numeric>
#include<iostream>
namespace eap {
	namespace sma {
		TimeStampFilter::TimeStampFilter(double frame_rate, double time_base)
		{
			_timebase = time_base;
			_framerate = frame_rate;
			_timestamp_gap = 1.0 / _framerate / _timebase;
		}
		TimeStampFilter::~TimeStampFilter()
		{
		}
		double TimeStampFilter::update(double timestamp)
		{
			std::lock_guard<std::mutex> lock(_data_mtx);
			double change_ts = timestamp;

			//对首帧进行处理,解决首帧出现异常极小或者大值对拟合曲线的影响
			if (_first_round && _data.empty()) {
				if (timestamp >= INT64_MAX || timestamp <= INT64_MIN) {
					change_ts = 0;
				}
			}
			//解决中途的异常值
			if (!_data.empty() && (abs(_data.back() - timestamp) > _timestamp_gap * 10)) {
				change_ts = _data.back() + _timestamp_gap;
			}

			// 添加新数据点
			if (_data.size() == _max_length) {
				_data.pop_front();
			}
			_data.emplace_back(change_ts);

			// 确保只在第一轮后设置 first_round 为 false
			if (_data.size() == _window_size) {
				_first_round = false;
			}

			// 获取当前窗口数据
			std::vector<double> current_window;
			if (_data.size() >= _window_size) {
				current_window.assign(_data.end() - _window_size, _data.end());
			}
			else {
				current_window.assign(_data.begin(), _data.end());
			}
			return slide_window_fit(current_window, timestamp);
		}
		void TimeStampFilter::set_first_flag(bool restart)
		{
			std::lock_guard<std::mutex> lock(_data_mtx);
			_data.clear();
			_first_round = true;
		}
		double TimeStampFilter::poly_func(double x, const Eigen::Vector3d& p)
		{
			return p(0) * pow(x, 2) + p(1) * x + p(2);
		}
		Eigen::Vector3d TimeStampFilter::least_squares_fit(const std::vector<double>& x_data, const std::vector<double>& y_data)
		{
			int n = x_data.size();
			Eigen::MatrixXd A(n, 3);
			Eigen::VectorXd b(n);
			for (int i = 0; i < n; ++i) {
				double x = x_data[i];
				A(i, 0) = pow(x, 2);
				A(i, 1) = x;
				A(i, 2) = 1.0;
				b(i) = y_data[i];
			}
			Eigen::VectorXd p = A.colPivHouseholderQr().solve(b);
			return Eigen::Vector3d(p(0), p(1), p(2));
		}
		double TimeStampFilter::compute_std_dev(const std::vector<double>& window_data)
		{
			if (window_data.empty()) { return 0.0; }
			double mean = std::accumulate(window_data.begin(), window_data.end(), 0.0) / window_data.size();
			double variance = std::accumulate(window_data.begin(), window_data.end(), 0.0,
				[&mean](double acc, double val) {
				return acc + pow(val - mean, 2);
			}) / window_data.size();
			return sqrt(variance);
		}

		double TimeStampFilter::slide_window_fit(std::vector<double>& current_window, double timestamp)
		{
			std::vector<double> x_data(current_window.size());
			// 创建 x 数据
			for (size_t i = 0; i < current_window.size(); ++i) {
				x_data[i] = static_cast<double>(i);
			}
			if (current_window.size() >= 3) {
				std::vector<double> y_data(current_window.begin(), current_window.end());
				// 进行多项式拟合
				Eigen::Vector3d popt = least_squares_fit(x_data, y_data);
				std::vector<double> fitted_y(current_window.size());

				// 计算拟合后的 y 值
				for (size_t i = 0; i < current_window.size(); ++i) {
					fitted_y[i] = this->poly_func(x_data[i], popt);
				}

				// 检测并替换异常值
				double current_window_std = compute_std_dev(current_window);
				for (size_t i = 0; i < y_data.size(); ++i) {
					//1.96 是标准正态分布中对应于 95 % 置信区间的 Z 分数（即约 95 % 的数据点位于平均值 ±1.96 标准差的范围内）
					//处理两帧之间间隔相邻异常近的情况
					if (abs(y_data[i] - fitted_y[i]) > 1.96 * current_window_std ||
						(i > 0 && abs(y_data[i] - y_data[i - 1]) < _timestamp_gap * 5 / 6)) {
						//size_t index = _data.size() - current_window.size() + i;
						//_corrected_data[index] = fitted_y[i];
						current_window[i] = fitted_y[i];
					}
				}
			}
			else {
				//std::cout << "Not enough data points for fitting" << std::endl;
				if (current_window.size() > 1) {
					if (abs(current_window[0] - current_window[1]) < _timestamp_gap * 5 / 6) {
						current_window[1] = current_window[0] + _timestamp_gap;
					}
				}
			}
			//if (timestamp != current_window.back()) {
			//	std::cout << "origin: " << timestamp << " fitted: " << current_window.back() << std::endl;
			//}
			return current_window.back();
		}
	}
}
