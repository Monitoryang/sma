#ifndef EAPS_DEMUXER_H
#define EAPS_DEMUXER_H

#include "EapsFFmpegWrap.h"

#include <mutex>
#include <thread>
#include <memory>
#include <functional>

namespace eap {
	namespace sma {
		class Demuxer;
		using DemuxerPtr = std::shared_ptr<Demuxer>;

		class Demuxer
		{
		public:
			using PacketCallback = std::function<void(Packet)>;
			using StopCallback = std::function<void(int exit_code)>;
			using TimeoutCallback = std::function<void()>;
			using StreamRecoverCallback = std::function<void()>;

		public:
			virtual void setPacketCallback(PacketCallback packet_callback) = 0;
			virtual void setStopCallback(StopCallback stop_callback) = 0;
			virtual void open(std::string url, std::chrono::milliseconds timeout, std::string local_addr="") = 0;
			virtual void close(bool is_stop=false) = 0;
			virtual void seek(float percent) = 0;
			virtual void pause(int paused) = 0;
			virtual void setTimeoutCallback(TimeoutCallback timeout_callback) = 0;
			virtual void setStreamRecoverCallback(StreamRecoverCallback stream_recover_callback) = 0;

			virtual AVCodecParameters videoCodecParameters() = 0;
			virtual AVRational videoStreamTimebase() = 0;
			virtual AVRational videoFrameRate() = 0;
			virtual int64_t videoDuration() = 0;
			virtual int64_t videoStartTime() = 0;
			virtual void startCache() = 0;;
			virtual void stopCache() = 0;
			virtual bool isTimeout() = 0;
			virtual bool isReadFrameTimeout() = 0;
			virtual int64_t bitRate() = 0;
		};
	}
}

#endif // !EAPS_DEMUXER_H