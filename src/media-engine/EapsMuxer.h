#ifndef EAPS_MUXER_OBJECTS_H
#define EAPS_MUXER_OBJECTS_H

#include "EapsFFmpegWrap.h"
#include "EapsCommon.h"

#include <string>
#include <queue>
#include <list>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace eap {
	namespace sma {
		class Muxer;
		using MuxerPtr = std::shared_ptr<Muxer>;

		class Muxer
		{
		public:
			enum class MuxedDataType
			{
				MuxedDataType_FLV,
				MuxedDataType_TS,
				MuxedDataType_FMV_TS,
				MuxedDataType_FMP4
			};

		public:
			static MuxerPtr createInstance();

		public:
			~Muxer();

			void open(std::string file_name, AVRational timebase, AVCodecParameters codecpar,
				MuxedDataType muxed_data_type = MuxedDataType::MuxedDataType_FMV_TS);
			void close();

			void pushPacket(Packet pkt);

		private:
			void writePacket(Packet pkt);
			void calcRemainingTime(Packet packet, std::chrono::microseconds& remaining_time);
		private:
			int64_t _last_pts{};
			std::string _file_name{};

			MuxedDataType _muxed_data_type{};

			AVFormatContext* _out_fmt_ctx{};
			AVIOContext* _io_context_out{ };
			uint8_t* _out_buf{ };
			std::string _format_name{};
			AVOutputFormat* _output_format{};
			AVCodecParameters _video_codec_par{};
			AVRational _in_timebase{};
			AVStream* _video_stream{};
			AVStream* _klv_stream{};
			int _video_stream_index{ 0 };
			int _klv_stream_index{ 1 };
			bool _is_call_aviopen{};
			bool _is_opened{};
			std::chrono::system_clock::time_point _start_write_time{};

			std::mutex _packets_mutex{};
			std::condition_variable _packets_cv{};
			std::queue<Packet> _packets{};
			bool _packets_ready{};

			std::thread _push_thread{};
			bool _push_run{};

			std::string _error_description{};

		private:
			Muxer();
			Muxer(Muxer& other) = delete;
			Muxer(Muxer&& other) = delete;
			Muxer& operator=(Muxer& other) = delete;
			Muxer& operator=(Muxer&& other) = delete;
		};
	}
}

#endif // !EAPS_MUXER_OBJECTS_H
