#ifndef EAPS_ENCODER_NVENC_H
#define EAPS_ENCODER_NVENC_H

#include "EapsFFmpegWrap.h"
#include "EapsCommon.h"

#include <queue>
#include <list>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>

namespace eap {
	namespace sma {
		class EncoderNVENC;
		using EncoderNVENCPtr = std::shared_ptr<EncoderNVENC>;

		class EncoderNVENC
		{
		public:
			using EncodedPacketCallback = std::function<void(Packet packet)>;

			struct InitParameter
			{
				int dst_width{};
				int dst_height{};
				std::uint32_t bit_rate{};
				AVRational framerate{};
				AVRational time_base{};
				int64_t start_time{};

				int keyint{ 5 };
				int profile{ 1 };// 0: baseline, 1: main, 2: high
				
			};

			static EncoderNVENCPtr createInstance(InitParameter& init_parameter,
				EncodedPacketCallback encoded_data_callback);

		public:
			EncoderNVENC(InitParameter& init_parameter,
				EncodedPacketCallback encoded_data_callback);
			~EncoderNVENC();

			virtual void start();
			virtual void stop();

			virtual void updateFrame(Frame frame);
			virtual void updateFrameGpu(Frame frame);
			virtual void updateFrame(CodecImage& frame);

		private:
			void _initialize();
			void _initializeEncoderOptions();
			void _initializeEncoder();
#ifdef ENABLE_AIRBORNE
			static AVFrame* _yuvToAvFrame(const cv::Mat &yuvMat, int width,
                                   int height);
#endif

		private:
			static const size_t kMaxWaitingFillFrameSize = 10;

			int _width{};
			int _height{};

			std::uint32_t _bitrate{};
			AVRational _framerate{};
			AVRational _timebase{};
			int64_t _start_time{};

			int _Keyint{};
			int _Profile{};// 0: baseline, 1: main, 2: high

			std::vector<uintptr_t> _shared_texture_handle{};

			EncodedPacketCallback _encoded_data_callback{};

			AVDictionary* _encoder_options{};
			AVCodecContext* _encoder_context{};
			AVBufferRef *_hw_device_context{};

			std::string _encoder_name{};

			std::mutex _wait_for_encode_frames_mutex{};
			std::condition_variable _wait_for_encode_frames_cv{};
			std::queue<Frame> _wait_for_encode_frames{};

			bool _encode_run{};
			std::thread _encode_thread{};

			std::string _error_description{};

		private:
			EncoderNVENC(EncoderNVENC& other) = delete;
			EncoderNVENC(EncoderNVENC&& other) = delete;
			EncoderNVENC& operator=(EncoderNVENC& other) = delete;
			EncoderNVENC& operator=(EncoderNVENC&& other) = delete;
		};
	}
}

#endif // !EAPS_ENCODER_NVENC_H
