#ifndef EAPS_PUSHER_H
#define EAPS_PUSHER_H

#include "EapsFFmpegWrap.h"
#include "jo_ai_detect_common.h"

#include <mutex>
#include <thread>
#include <memory>
#include <functional>
#include <queue>
#include <condition_variable>
#include <vector>
#include <tuple>

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
			virtual void updateAiDetectInfo(const AiInfos& /*ai_infos*/) {}

			/** 设置视频图像尺寸，供 AI 像素坐标转地理坐标使用 */
			virtual void updateVideoSize(int /*width*/, int /*height*/) {}

			/** 实时计算 AI 检测各框的地理坐标（lon, lat, alt） */
			virtual std::vector<std::tuple<double, double, double>> calcAiGeoLocations(
				const std::vector<joai::Result>& /*ai_detect_ret*/, int /*img_w*/, int /*img_h*/) { return {}; }
		};
	}
}

#endif // !EAPS_PUSHER_H