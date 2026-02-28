#ifndef EAPS_PUSHER_H
#define EAPS_PUSHER_H

#include "EapsFFmpegWrap.h"

#include <mutex>
#include <thread>
#include <memory>
#include <functional>
#include <queue>
#include <condition_variable>

namespace eap {
	namespace sma {
		class Pusher;
		using PusherPtr = std::shared_ptr<Pusher>;

		class Pusher
		{
		public:
			using StopCallback = std::function<void(int ret, std::string err_str)>;

		public:
			virtual void setStopCallback(StopCallback stop_callback) = 0;
			virtual void open(std::string id, std::string url, AVRational timebase, AVRational framerate,
				AVCodecParameters codecpar, std::chrono::milliseconds timeout, std::string localaddr="") = 0;
			virtual void close() = 0;
			virtual void pause(int paused) = 0;

			virtual void pushPacket(Packet pkt) = 0;
		};
	}
}

#endif // !EAPS_PUSHER_H