#pragma once

#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace eap
{
	namespace common 
	{
		class Timer
		{
		public:
			Timer() : _expired(true), _try_to_expire(false)
			{ }

			Timer(const Timer& timer)
			{
				_expired = timer._expired.load();
				_try_to_expire = timer._try_to_expire.load();
			}

			~Timer()
			{
				stop();
			}

			void start(int interval, std::function<void()> task)
			{
				// is started, do not start again
				if (_expired == false)
					return;

				// start async timer, launch thread and wait in that thread
				_expired = false;
				std::thread([this, interval, task]()
				{
					int count_10ms = 0;
					int ms = interval;
					if (interval > 10) {
						count_10ms = interval / 10;
						ms = interval % 10;
					}
					while (!_try_to_expire) {
						// sleep every interval and do the task again and again until times up
						for (int i = 0; i < count_10ms; ++i) {
							if (_try_to_expire) {
								break;
							}
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						if (ms > 0) {
							std::this_thread::sleep_for(std::chrono::milliseconds(ms));
						}
						if (!_try_to_expire) {
							task();
						}
					}

					{
						// timer be stopped, update the condition variable expired and wake main thread
						std::lock_guard<std::mutex> locker(_mutex);
						_expired = true;
						_expired_cond.notify_one();
					}
				}).detach();
			}

			void startOnce(int delay, std::function<void()> task)
			{
				std::thread([delay, task]()
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(delay));
					task();
				}).detach();
			}

			void stop()
			{
				// do not stop again
				if (_expired)
					return;

				if (_try_to_expire)
					return;

				// wait until timer 
				_try_to_expire = true; // change this bool value to make timer while loop stop
				{
					std::unique_lock<std::mutex> locker(_mutex);
					_expired_cond.wait(locker, [this] { return _expired == true; });

					// reset the timer
					if (_expired == true)
						_try_to_expire = false;
				}
			}

		private:
			std::atomic<bool> _expired; // timer stopped status
			std::atomic<bool> _try_to_expire; // timer is in stop process
			std::mutex _mutex;
			std::condition_variable _expired_cond;
		};
	}	
}