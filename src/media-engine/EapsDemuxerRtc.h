#ifndef EAPS_DEMUXER_RTC_H
#define EAPS_DEMUXER_RTC_H

#include "EapsDemuxer.h"
#include "jo_rtc.h"

#include <mutex>
#include <thread>
#include <memory>
#include <functional>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace eap {
	namespace sma {
		class DemuxerRtc;
		using DemuxerRtcPtr = std::shared_ptr<DemuxerRtc>;

		class DemuxerRtc : public Demuxer
		{
		public:
			using PacketCallback = std::function<void(Packet)>;
			using StopCallback = std::function<void(int exit_code)>;

		public:
			static DemuxerRtcPtr createInstance();

		public:
			virtual ~DemuxerRtc();

			virtual void setPacketCallback(PacketCallback packet_callback) override;
			virtual void setStopCallback(StopCallback stop_callback) override;
			virtual void open(std::string url, std::chrono::milliseconds timeout, std::string local_addr = "") override;
			virtual void close(bool is_stop=false) override;
			virtual void seek(float percent) override;

			virtual AVCodecParameters videoCodecParameters() override;
			virtual AVRational videoStreamTimebase() override;
			virtual AVRational videoFrameRate() override;
			virtual int64_t videoDuration() override;
			virtual int64_t videoStartTime() override;
			virtual void pause(int paused) override;
			virtual void setTimeoutCallback(TimeoutCallback timeout_callback) override;
			virtual void setStreamRecoverCallback(StreamRecoverCallback stream_recover_callback) override;
			virtual void startCache() override;
			virtual void stopCache() override;
			virtual bool isTimeout() override;
			virtual bool isReadFrameTimeout() override;
			virtual int64_t bitRate() override;
		private:
			void initRtcReciever();
			void onRtcPacket(uint8_t* packet, int bytes, uint32_t timestamp, int flags);
			void calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time);

		private:
			static const std::size_t kVideoBufferSize{ 20480 };

			PacketCallback _packet_callabck{};
			StopCallback _stop_callback{};

			jo_rtc_reciever _rtc_reciever{};

			AVDictionary* _fmt_options{};
			AVFormatContext* _fmt_ctx{};
			AVIOContext* _avio_ctx{};
			uint8_t* _avio_ctx_buffer{};
			size_t _avio_ctx_buffer_size = kVideoBufferSize;
			AVStream* _video_stream{};
			int _video_stream_index{};
			AVCodecParameters _video_codec_par{};
			int64_t _video_start_time{};
			int64_t _video_duration{};
			int64_t _video_bit_rate{};
			AVRational _video_timebase{ 1, 90000 };
			AVRational _video_frame_rate{};

			bool _should_test_frame_rate{};

			std::string _url{};
			std::chrono::milliseconds _timeout{};
			std::chrono::system_clock::time_point _start_open_time{};
			std::chrono::system_clock::time_point _start_read_time{};

			bool _first_packet{ true };
			int64_t _start_pts{};
			int64_t _last_pts{};

			std::string _error_description{};

			bool _fmt_opened{};
			bool _interrupt_flag{};
			bool _is_stoped{};

			bool _read_run{};
			std::thread _read_thread{};

			std::mutex _video_buffer_mtx{};
			std::condition_variable _video_buffer_cv{};
			std::queue<std::vector<uint8_t>> _video_buffer{};

			std::vector<uint8_t> _cache_buffer{};

			bool _first_rtp_packet{ true };
			int64_t _first_rtp_pts{};

		private:
			DemuxerRtc();
			DemuxerRtc(DemuxerRtc& other) = delete;
			DemuxerRtc(DemuxerRtc&& other) = delete;
			DemuxerRtc& operator=(DemuxerRtc& other) = delete;
			DemuxerRtc& operator=(DemuxerRtc&& other) = delete;
		};
	}
}

#endif // !EAPS_DEMUXER_RTC_H


