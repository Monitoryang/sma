#ifndef EAPS_DEMUXER_TRADITION_H
#define EAPS_DEMUXER_TRADITION_H

#include "EapsDemuxer.h"
#include "EapsTimer.h"
#include "EapsTimestampFilter.h"
#include <mutex>
#include <thread>
#include <memory>
#include <functional>
#include <atomic>

namespace eap {
	namespace sma {
		class DemuxerTradition;
		using DemuxerTraditionPtr = std::shared_ptr<DemuxerTradition>;

		class DemuxerTradition : public Demuxer
		{
		public:
			using PacketCallback = std::function<void(Packet)>;
			using StopCallback = std::function<void(int exit_code)>;
			using TimeoutCallback = std::function<void()>;
			using StreamRecoverCallback = std::function<void()>;
		public:
			static DemuxerTraditionPtr createInstance();

		public:
			virtual ~DemuxerTradition();

			virtual void setPacketCallback(PacketCallback packet_callback) override;
			virtual void setStopCallback(StopCallback stop_callback) override;
			virtual void open(std::string url, std::chrono::milliseconds timeout, std::string local_addr = "") override;
			virtual void close(bool is_stop=false) override;
			virtual void seek(float percent) override;
			virtual void pause(int paused) override;

			virtual AVCodecParameters videoCodecParameters() override;
			virtual AVRational videoStreamTimebase() override;
			virtual AVRational videoFrameRate() override;
			virtual int64_t videoDuration() override;
			virtual int64_t videoStartTime() override;

			virtual void setTimeoutCallback(TimeoutCallback timeout_callback) override;
			virtual void setStreamRecoverCallback(StreamRecoverCallback stream_recover_callback) override;
			virtual void startCache() override;
			virtual void stopCache() override;
			virtual bool isTimeout() override;
			virtual bool isReadFrameTimeout() override;
			virtual int64_t bitRate() override;
		private:
			void checkNetwork();
			void seekExecute();
			void calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time);
			static int interruptCallback(void* opaque);

		private:
			using time_point = std::chrono::system_clock::time_point;

			PacketCallback _packet_callabck{};
			StopCallback _stop_callback{};
			TimeoutCallback _timeout_callback{};
			StreamRecoverCallback _stream_recover_callback{};
			std::atomic_bool _is_packet_callback{ true };
			std::atomic_bool _is_timeout{};
			common::Timer _timeout_check_timer{};
			int _read_timeout_cnt{0};

			AVDictionary* _fmt_options{};
			AVFormatContext* _fmt_ctx{};
			AVStream* _video_stream{};
			int _video_stream_index{};
			AVCodecParameters _video_codec_par{};
			int64_t _video_start_time{};
			int64_t _video_duration{};
			int64_t _video_bit_rate{};
			AVRational _video_timebase{};
			AVRational _video_frame_rate{};

			bool _should_test_frame_rate{};
			std::string _url{};
			std::chrono::milliseconds _timeout{};
			bool _is_network{};
			bool _is_udp{};
			bool _is_http{};
			bool _is_rtsp{}; //rtsp、http视频流时状态为true
			time_point _start_open_time{};
			time_point _start_read_time{};
			time_point _first_read_time{};
			std::chrono::steady_clock::time_point _start_read_frame_time{};
			bool _read_frame_timeout{}; //是否读取rtsp、http视频流超时
			bool _first_packet{ true };
			int64_t _start_pts{};
			int64_t _last_pts{};

			std::mutex _seeking_mutex{};
			bool is_seeking_req{};
			float seeking_percent{};

			std::string _error_description{};

			bool _fmt_opened{};
			bool _interrupt_flag{};
			bool _is_stoped{};
			bool _read_run{};
			std::thread _read_thread{};
			std::atomic_bool _paused{};
			std::atomic_bool _last_paused{};
			int _read_pause_return{};
			int _read_play_return{};
			AVPacket* _last_packet{};

			std::mutex _callback_mutex{};
			std::atomic_bool _is_readed_first_frame{ false };
			std::mutex _read_timepoint_mutex{};
			std::mutex _read_thread_mutex{};
			std::chrono::steady_clock::time_point _last_readed_frame_timepoint{};
			bool _is_http_flv_rtmp{};
			bool _is_rewrite_pts{ false };
			bool _first_send_pkt{ true };
			int64_t _last_pkt_pts{};
			std::string _local_addr{};
			std::shared_ptr<TimeStampFilter> _timestamp_fliter{};
		private:
			DemuxerTradition();
			DemuxerTradition(DemuxerTradition& other) = delete;
			DemuxerTradition(DemuxerTradition&& other) = delete;
			DemuxerTradition& operator=(DemuxerTradition& other) = delete;
			DemuxerTradition& operator=(DemuxerTradition&& other) = delete;
		};
	}
}

#endif // !EAPS_DEMUXER_TRADITION_H