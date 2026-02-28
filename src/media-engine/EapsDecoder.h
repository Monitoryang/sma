#ifndef EAPS_DECODER_H
#define EAPS_DECODER_H

#include "EapsFFmpegWrap.h"
#include "EapsCommon.h"

#include <mutex>
#include <thread>
#include <memory>
#include <functional>
#include <queue>
#include <condition_variable>

namespace eap {
	namespace sma {
		class Decoder;
		using DecoderPtr = std::shared_ptr<Decoder>;

		class Decoder
		{
		public:
			using FrameCallback = std::function<void(Frame)>;

		public:
			static DecoderPtr createInstance();

		public:
			~Decoder();

			void setFrameCallback(FrameCallback frame_callback);
			void open(AVCodecParameters codecpar, AVRational framerate, const std::string url, bool is_hardware = false);
			void close();
			//void flushDecoder();
			void pushPacket(Packet& pkt);
			bool isDecode();
			void clearFrameQueue();
			AVFrame* convertExportFrame(AVFrame* frame);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			cv::Mat avframe2cvmat(AVFrame *avframe);
#endif
		private:
			int _HwDecoderInit(AVCodecContext *ctx, const AVHWDeviceType type);
			static AVPixelFormat _GetHwFormat(AVCodecContext *ctx,
				const AVPixelFormat *pix_fmts);

		private:
			std::string _url{};

			bool _is_hardware{};

			AVCodecContext* _decoder_ctx{};
			AVCodecParameters _decoder_par{};
			SwsContext* _sws_ctx{};
			AVBufferRef* _HwDeviceCtx{};
			static AVPixelFormat _HwPixFmt;

			FrameCallback _frame_callback{};

			std::mutex _packets_mutex{};
			std::condition_variable _packets_cv{};
			std::queue<Packet> _packets{};
			bool _packets_ready{};

			std::thread _decode_thread{};
			bool _decode_run{};

			std::string _error_description{};
			std::string _decode_name{};
			//std::atomic_bool _is_flush_decoder{};

		private:
			Decoder();
			Decoder(Decoder& other) = delete;
			Decoder(Decoder&& other) = delete;
			Decoder& operator=(Decoder& other) = delete;
			Decoder& operator=(Decoder&& other) = delete;
		};
	}
}

#endif // !EAPS_DECODER_H

