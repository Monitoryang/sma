#include "EapsPusherRtc.h"
#include "Logger.h"
#include "Utils.h"
#include "EapsNoticeCenter.h"
#include <regex>
#include <string>
#include <vector>
#include <stdexcept>
#include <system_error>

namespace eap {
	namespace sma {
		static std::vector<std::string> split(std::string str, std::string pattern)
		{
			std::string::size_type pos;
			std::vector<std::string> result;
			str += pattern;//扩展字符串以方便操作
			auto size = str.size();
			for (int i = 0; i < size; i++)
			{
				pos = str.find(pattern, i);
				if (pos < size)
				{
					std::string s = str.substr(i, pos - i);
					result.push_back(s);
					i = pos + pattern.size() - 1;
				}
			}
			return result;
		}

		PusherRtcPtr PusherRtc::createInstance()
		{
			return PusherRtcPtr(new PusherRtc());
		}

		PusherRtc::PusherRtc()
		{
			_meta_data_assembly_geojsonx = MetaDataAssemblyGeoJsonx::createInstance();
		}

		PusherRtc::~PusherRtc()
		{
			close();
		}

		void PusherRtc::setStopCallback(StopCallback stop_callback)
		{
			_stop_callback = stop_callback;
		}

		void PusherRtc::open(std::string id, std::string url, AVRational timebase, AVRational framerate,
		AVCodecParameters codecpar, std::chrono::milliseconds timeout, std::string localaddr)
		{
			_manual_stop = false;
			_push_stop_cb_state = false;
			_url = url;
			_in_timebase = timebase;
			_video_codec_par = codecpar;
			_open_timeout = timeout;
			_frame_rate = framerate;
			_rtc_sender = jo_rtc_sender_create();

			jo_rtc_sender_set_connect_state_changed_callback(_rtc_sender,
				[](const joRtcState state, void* opaque) {
				PusherRtc* thiz = (PusherRtc*)opaque;
				if (state == JO_RTC_CLOSED || state == JO_RTC_FAILED ||
					state == JO_RTC_DISCONNECTED) {
					thiz->_push_run = false;
					thiz->_packets_cv.notify_all();
					eap_error_printf("webrtc sender connect state changed to %d",(int)state);
				}
			}, this);
			jo_rtc_sender_set_data_channel_opened_callback(_rtc_sender, [](void* opaque) {
				PusherRtc* thiz = (PusherRtc*)opaque;
				thiz->_is_datachannel_opened = true;
				eap_information( "datachannel opened ");
			}, this);
			jo_rtc_sender_set_data_channel_closed_callback(_rtc_sender, [](void* opaque) {
				PusherRtc* thiz = (PusherRtc*)opaque;
				eap_error("datachannel closed");
				//删除pusher，_push_thread线程没有正常退出，重启sma
				if (thiz && thiz->_is_datachannel_opened && thiz->_manual_stop && !thiz->_push_stop_cb_state) {
					eap_error_printf("pusher stop but thread not quit,restart sma, url: %s", thiz->_url);
					Poco::Path current_exe = Poco::Path::self();
					auto process_name = current_exe.getFileName();
					eap::cleanProcessByName(process_name);
				}
				thiz->_is_datachannel_opened = false;
			}, this);
			jo_rtc_sender_set_data_channel_error_callback(_rtc_sender, [](const char* error, void* opaque) {
				PusherRtc* thiz = (PusherRtc*)opaque;
				thiz->_is_datachannel_opened = false;
				eap_error("datachannel error and closed ");
			}, this);
			jo_rtc_sender_set_data_string_callback(_rtc_sender, [](const char* data, void* opaque) {
				PusherRtc* thiz = (PusherRtc*)opaque;
				thiz->_mark_data_callback((std::string)data);
			}, this);
			auto rtc_sender_index = jo_rtc_sender_add_stream(_rtc_sender);
            JoRtcCodecId codec_id = JO_RTC_CODEC_ID_H264;
            if (_video_codec_par.codec_id == AVCodecID::AV_CODEC_ID_HEVC)
                codec_id = JO_RTC_CODEC_ID_H265;
            JoRtcErrorCode rtc_errcode = jo_rtc_sender_open(_rtc_sender, _url.c_str(), codec_id);
			// JoRtcErrorCode rtc_errcode = jo_rtc_sender_open(_rtc_sender, _url.c_str());
			if (rtc_errcode != JO_SUCCEED) {
				_error_description = "webrtc sender open failed, error code: " + std::to_string((int)rtc_errcode) + ", url: " + _url;
				eap_information(_error_description);
				throw std::system_error((int)rtc_errcode, std::system_category(), _error_description);
			}
			eap_information_printf( "webrtc sender open success, url: %s", _url);

			_push_thread = std::thread([this, id]() {
				_push_run = true;
				bool first_packet{ true };
				int64_t start_pts = 0;
				int64_t last_pts = 0;
				std::string meta_data_json_string{};
				std::string pkt_send_duration{};
				std::string pkt_time_send_duration{};
				std::chrono::steady_clock::time_point push_second_time{};
				std::chrono::steady_clock::time_point last_push_time{};

				eap_information_printf( "webrtc sender push start, url: : %s", _url);
				for (; _push_run;) {
					Packet pkt{};
					if(!_paused) {
						std::unique_lock<std::mutex> lock(_packets_mutex);
						if (_packets.empty()) {
							_packets_ready = false;
							_packets_cv.wait(lock, [this] {
								return _packets_ready || !_push_run || _paused;
							});
						}

						if (!_push_run) {
							break;
						}

						if(_packets.empty()){// 暂停时的唤醒
							continue;
						}
						////缓存10帧
						//if (first_packet && _packets.size() < 10) {
						//	continue;
						//}

						pkt = _packets.front();
						_packets.pop();
						lock.unlock();

						if (first_packet) {
							_start_push_time = std::chrono::steady_clock::now();
							push_second_time = std::chrono::steady_clock::now();
							last_push_time = std::chrono::steady_clock::now();
							start_pts = pkt->pts;
							first_packet = false;
						}

						pkt->pts -= start_pts;
						if (pkt->pts && pkt->pts <= last_pts) {
							pkt->pts = last_pts + 1;
							std::string err_msg = "pts between frames is too small";
							NoticeCenter::Instance()->getCenter().postNotification(
								new VideoMsgNotice(id, (int)VideoMsgNotice::VideoCodeType::PtsBetweenLarge, err_msg));
						}
						pkt->dts = pkt->pts;
						last_pts = pkt->pts;

						/*AVRational  dst_time_base = { 1, AV_TIME_BASE };
						int64_t pts_time = av_rescale_q(pkt->pts, _in_timebase, dst_time_base);
						pkt_time_send_duration += std::to_string(pts_time) + "  ";*/

						/*std::chrono::microseconds remaining_time = std::chrono::microseconds(0);
						calcRemainingTime(pkt, remaining_time);
						if (remaining_time > std::chrono::microseconds(1000)) {
							std::this_thread::sleep_for(remaining_time);
						}*/

						try {
							
							/*auto pkt_time = std::chrono::steady_clock::now();
							int64_t pkt_send_time = std::chrono::duration_cast<std::chrono::milliseconds>(
								pkt_time - last_push_time).count();
							int64_t now_time = std::chrono::duration_cast<std::chrono::milliseconds>(
								pkt_time - push_second_time).count();
							pkt_send_duration += std::to_string(pkt_send_time) + "  ";
							last_push_time = pkt_time;
							if (now_time >= 1000) {
								push_second_time = pkt_time;
								eap_information_printf(" pkt_send_duration: %s ", pkt_send_duration);
								pkt_send_duration.clear();
								eap_information_printf(" pkt_time_send_duration: %s ", pkt_time_send_duration);
								pkt_time_send_duration.clear();
							}*/
							writePacket(pkt);
							if (_is_datachannel_opened) {
								if (_meta_data_assembly_geojsonx) {
									if (pkt.metaDataValid()) {
										_meta_data_assembly_geojsonx->updateMetaDataStructure(pkt.getMetaDataBasic());
										_meta_data_assembly_geojsonx->updateArData(pkt.getArValidPointIndex(), pkt.getArVectorFile());
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
										_meta_data_assembly_geojsonx->updateArData(pkt.getArPixelPoints(), pkt.getArPixelLines(), pkt.getArInfos());
										_meta_data_assembly_geojsonx->updateArData(pkt.getArPixelWarningL1s(), pkt.getArPixelWarningL2s());
#endif
										_meta_data_assembly_geojsonx->updateAiHeapmapData(pkt.getAiHeatmapInfos());
										_meta_data_assembly_geojsonx->updateFrameCurrentTime(pkt.getCurrentTime());
										int frame_rate{}, bit_rate{}, video_width{}, video_height{};
										pkt.getVideoParams(frame_rate, bit_rate, video_width, video_height);
										_meta_data_assembly_geojsonx->updateVideoParams2(_video_duration, bit_rate, frame_rate, video_width, video_height);
									}
									// 无论元数据是否有效，都尝试获取assembly字符串（直接路径AI数据不依赖元数据）
									meta_data_json_string = _meta_data_assembly_geojsonx->getAssemblyString();
								}
								if (!meta_data_json_string.empty() && _rtc_sender){
									// eap_information_printf("[DEBUG-DATACHANNEL] sending json length: %d, content: %.500s", 
                                    //     (int)meta_data_json_string.length(), meta_data_json_string.c_str());
									jo_rtc_sender_send_string(_rtc_sender, meta_data_json_string.c_str());
								}
							}
						} catch (std::system_error& e) {
							_push_stop_cb_state = true;
							if (_stop_callback) {
								_stop_callback(e.code().value(), e.what());
							}
							return;
						}
					} else{
						std::string meta_data_json_string_test = "test";
						try {
							if (_is_datachannel_opened) {
								if (!meta_data_json_string_test.empty() && _rtc_sender) {
									eap_warning("only send meta_data_json_string_test");
									jo_rtc_sender_send_string(_rtc_sender, meta_data_json_string_test.c_str());
									std::this_thread::sleep_for(std::chrono::milliseconds(20));
								}
							}
						} catch(std::system_error& e) {
							_push_stop_cb_state = true;
							if (_stop_callback) {
								_stop_callback(e.code().value(), e.what());
							}
							return;
						}

					}								
				}

				eap_error_printf("webrtc sender push thread exited, url: %s", _url);
				_push_stop_cb_state = true;
				if (_stop_callback) {
					_stop_callback(0, "");
				}
			});
		}

		void PusherRtc::close()
		{
			if(!_url.empty()){
				eap_information_printf("start close pusher, rtc url: %s", _url);
				_manual_stop = true;
				_push_run = false;
				if (_push_thread.joinable()) {
					_packets_cv.notify_all();
					_push_thread.join();
					eap_information_printf("close pusher thread successed!, rtc url: %s", _url);
				}
				
				if (_rtc_sender) {
					jo_rtc_sender_close(_rtc_sender);
					jo_rtc_sender_destroy(_rtc_sender);
					_rtc_sender = nullptr;
				}
				eap_information_printf("close pusher finished, rtc url: %s", _url);
				_url = "";
			}
		}

        void PusherRtc::pause(int paused)
        {
			if(1 == paused){
				_paused.store(true);
			}else{
				_paused.store(false);
			}
			
			// 这个锁是为了避免出现，触发暂停，条件变量唤醒之后，推流器的主循环刚好到_packets_cv.wait阻塞住
			std::lock_guard<std::mutex> lock(_packets_mutex);
			_packets_cv.notify_all();
        }

		void PusherRtc::updateAiDetectInfo(const AiInfos& ai_infos)
		{
			if (_meta_data_assembly_geojsonx) {
				_meta_data_assembly_geojsonx->updateAiDetectInfo(ai_infos);
			}
		}

		void PusherRtc::updateVideoSize(int width, int height)
		{
			if (_meta_data_assembly_geojsonx) {
				_meta_data_assembly_geojsonx->updateVideoSize(width, height);
			}
		}

		std::vector<std::tuple<double, double, double>> PusherRtc::calcAiGeoLocations(
			const std::vector<joai::Result>& ai_detect_ret, int img_w, int img_h)
		{
			if (_meta_data_assembly_geojsonx) {
				return _meta_data_assembly_geojsonx->calcAiGeoLocations(ai_detect_ret, img_w, img_h);
			}
			return {};
		}

        void PusherRtc::pushPacket(Packet pkt)
		{
			std::lock_guard<std::mutex> lock(_packets_mutex);
			while (_packets.size() > 50) {
				_packets.pop();
			}
			_packets.push(pkt);
			_packets_ready = true;
			_packets_cv.notify_all();
		}

		void PusherRtc::setMarkDataCallback(MarkDataCallback mark_data_callback)
		{
			_mark_data_callback = mark_data_callback;
		}

		void PusherRtc::updateVideoParams(int64_t video_duration)
		{
			_video_duration = video_duration;
		}

   		int64_t PusherRtc::getPacketsFrontPts()
		{
			int64_t pts = (!_packets.empty())? _packets.front()->pts : 0;
			return pts;
		}

		void PusherRtc::calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time)
		{
			auto pts = packet->pts;
			auto dts = packet->dts;
			auto duration = packet->duration;

			AVRational  dst_time_base = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(packet->pts, _in_timebase, dst_time_base);
			//int64_t pts_time = pts * av_q2d(_OutStream->time_base) * AV_TIME_BASE - _StartPts;
			//int64_t now_time = av_gettime_relative() - start_time;
			int64_t now_time = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - _start_push_time).count();
			auto remaining_time_t = pts_time - now_time;
			if ((remaining_time_t > 0) &&
				(remaining_time_t < AV_TIME_BASE * 10)) {
				remaining_time = std::chrono::microseconds(remaining_time_t);
			}
		}

		void PusherRtc::writePacket(Packet pkt)
		{
			AVRational out_timebase{ 1, 90000 };

			pkt->pts = av_rescale_q_rnd(pkt->pts, _in_timebase, out_timebase,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->dts = av_rescale_q_rnd(pkt->dts, _in_timebase, out_timebase,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->duration = av_rescale_q(pkt->duration, _in_timebase, out_timebase);
			pkt->pos = -1;
			pkt->stream_index = 0;
			jo_rtc_sender_send_media(_rtc_sender, pkt->data, pkt->size, pkt->pts);
		}
	}
}