#ifndef EAPS_PUSHER_TRADITION_H
#define EAPS_PUSHER_TRADITION_H

#include "EapsPusher.h"
#include <atomic>

namespace eap {
	namespace sma {
		class PusherTradition;
		using PusherTraditionPtr = std::shared_ptr<PusherTradition>;

		class PusherTradition : public Pusher
		{
		public:
			using StopCallback = std::function<void(int ret, std::string err_str)>;

		public:
			static PusherTraditionPtr createInstance();

		public:
			~PusherTradition();

			virtual void setStopCallback(StopCallback stop_callback) override;
			virtual void open(std::string id, std::string url, AVRational timebase, AVRational framerate,
				AVCodecParameters codecpar, std::chrono::milliseconds timeout, std::string localaddr = "") override;
			virtual void close() override;
			virtual void pause(int paused) override;

			virtual void pushPacket(Packet pkt) override;

		private:
			void checkFormat();
			void calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time);
			void writePacket(Packet pkt);
			static int interrupt_callback(void* opaque);
			bool ping(const std::string& ip);

		private:
			std::string _url{};
			AVFormatContext* _out_fmt_ctx{};
			std::string _format_name{};
			AVCodecParameters _video_codec_par{};
			AVRational _in_timebase{};
			AVStream* _video_stream{};
			AVRational _frame_rate{};
			AVStream* _klv_stream{};
			int _video_stream_index{ 0 };
			int _klv_stream_index{ 1 };
			bool _is_opened{};
			bool _is_avio_open_called{};
			bool _is_closed{};
			std::chrono::system_clock::time_point _start_open_time{};
			std::chrono::system_clock::time_point _start_push_time{};

			std::mutex _packets_mutex{};
			std::condition_variable _packets_cv{};
			std::queue<Packet> _packets{};
			bool _packets_ready{};

			std::chrono::milliseconds _open_timeout{};

			std::thread _push_thread{};
			std::thread _media_server_thread{}; //判断服务器ip是否可用的线程
			bool _push_run{};
			bool _check_server_run{};

			StopCallback _stop_callback{};

			std::string _error_description{};
			std::atomic_bool _paused{};
			std::chrono::system_clock::time_point _start_read_frame_time{};
			std::chrono::steady_clock::time_point _last_readed_frame_timepoint{};
			bool _is_readed_first_frame{ false };
			std::string _local_addr{};
			int _read_timeout_cnt{ 0 };
			bool _first_packet{};
			std::string _media_server_ip{};
		private:
			PusherTradition();
			PusherTradition(PusherTradition& other) = delete;
			PusherTradition(PusherTradition&& other) = delete;
			PusherTradition& operator=(PusherTradition& other) = delete;
			PusherTradition& operator=(PusherTradition&& other) = delete;
		};
	}
}

#endif // !EAPS_PUSHER_TRADITION_H