#ifndef EAPS_PUSHER_RTC_H
#define EAPS_PUSHER_RTC_H

#include "jo_rtc.h"
#include "EapsPusher.h"
//#include "jo_meta_data_processing.h"
//#include "VasMetaDataAssemblyJson.h"
#include "EapsMetaDataAssemblyJGeoJsonx.h"

#include <atomic>

namespace eap {
	namespace sma {
		class PusherRtc;
		using PusherRtcPtr = std::shared_ptr<PusherRtc>;

		class PusherRtc : public Pusher
		{
		public:
			using StopCallback = std::function<void(int ret, std::string err_str)>;
			using MarkDataCallback = std::function<void(std::string mark_data)>;

		public:
			static PusherRtcPtr createInstance();

		public:
			~PusherRtc();

			virtual void setStopCallback(StopCallback stop_callback) override;
			virtual void open(std::string id, std::string url, AVRational timebase, AVRational framerate,
				AVCodecParameters codecpar, std::chrono::milliseconds timeout, std::string localaddr = "") override;
			virtual void close() override;
			virtual void pause(int paused) override;

			virtual void pushPacket(Packet pkt) override;
			virtual void updateAiDetectInfo(const AiInfos& ai_infos) override;
			virtual void updateVideoSize(int width, int height) override;
			virtual std::vector<std::tuple<double, double, double>> calcAiGeoLocations(
				const std::vector<joai::Result>& ai_detect_ret, int img_w, int img_h) override;
			void setMarkDataCallback(MarkDataCallback mark_data_callback);
			void updateVideoParams(int64_t video_duration);
			int64_t getPacketsFrontPts();
		private:
			void calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time);
			void writePacket(Packet pkt);

		private:
			std::string _url{};
			AVCodecParameters _video_codec_par{};
			AVRational _in_timebase{};
			AVRational _frame_rate{};
			MarkDataCallback _mark_data_callback{};
			int64_t _video_duration{};


			std::chrono::milliseconds _open_timeout{};

			std::chrono::steady_clock::time_point _start_push_time{};

			std::mutex _packets_mutex{};
			std::condition_variable _packets_cv{};
			std::queue<Packet> _packets{};
			bool _packets_ready{};

			std::thread _push_thread{};
			bool _push_run{};
			bool _push_stop_cb_state{};//删除pusher，_push_thread线程是否退出的标志位，true代表线程正常退出，false代表没有
			bool _manual_stop{};//是否调用stop接口

			std::atomic_bool _is_datachannel_opened{};
			//MetaDataAssemblyGeoJsonxPtr _meta_data_assembly_geojsonx{};

			StopCallback _stop_callback{};

			std::string _error_description{};

			jo_rtc_sender _rtc_sender{};

			MetaDataAssemblyGeoJsonxPtr _meta_data_assembly_geojsonx{};
			std::atomic_bool _paused{};
		private:
			PusherRtc();
			PusherRtc(PusherRtc& other) = delete;
			PusherRtc(PusherRtc&& other) = delete;
			PusherRtc& operator=(PusherRtc& other) = delete;
			PusherRtc& operator=(PusherRtc&& other) = delete;
		};
	}
}

#endif // !EAPS_PUSHER_RTC_H

