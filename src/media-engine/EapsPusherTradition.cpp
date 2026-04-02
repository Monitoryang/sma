#include "EapsPusherTradition.h"
#include "Logger.h"
#include "EapsNoticeCenter.h"
#include "Utils.h"
#include "Poco/Net/ICMPClient.h"
#include <stdexcept>
#include <system_error>
#include <regex>

namespace eap {
	namespace sma {
		PusherTraditionPtr PusherTradition::createInstance()
		{
			return PusherTraditionPtr(new PusherTradition());
		}

		PusherTradition::PusherTradition()
		{
		}

		PusherTradition::~PusherTradition()
		{
			eap_information("~PusherTradition");
			_check_server_run = false;
			if (_media_server_thread.joinable()) {
				_media_server_thread.join();
				_media_server_ip = "";
			}
			close();
		}

		void PusherTradition::setStopCallback(StopCallback stop_callback)
		{
			_stop_callback = stop_callback;
		}

		void PusherTradition::open(std::string id, std::string url, AVRational timebase, AVRational framerate,
			AVCodecParameters codecpar, std::chrono::milliseconds timeout, std::string localaddr)
		{

			_url = url;
			_in_timebase = timebase;
			_video_codec_par = codecpar;
			_open_timeout = timeout;
			_frame_rate = framerate;
			_local_addr = localaddr;
			_is_closed = false;

			checkFormat();

			int ret{};
			if ((ret = avformat_alloc_output_context2(&_out_fmt_ctx, nullptr,
				(_format_name.empty() ? nullptr : _format_name.c_str()), _url.c_str())) < 0) {
				_error_description = "Alloc output format "
					"context failed, error description: " + AVError2String(ret) +
					", out format name: " + _format_name + ", out url: " + _url;
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			_out_fmt_ctx->interrupt_callback.callback = interrupt_callback;
			_out_fmt_ctx->interrupt_callback.opaque = this;

			_video_stream = avformat_new_stream(_out_fmt_ctx, nullptr);
			if (!_video_stream) {
				_error_description = "AVForamt new stream failed";
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			_video_stream->id = _out_fmt_ctx->nb_streams - 1;
			*_video_stream->codecpar = _video_codec_par;
			_video_stream->codecpar->extradata = (uint8_t*)av_malloc(_video_codec_par.extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
			memcpy(_video_stream->codecpar->extradata, _video_codec_par.extradata, _video_codec_par.extradata_size);
			_video_stream->codecpar->extradata_size = _video_codec_par.extradata_size;
			_video_stream->codecpar->sample_aspect_ratio = { 0, 1 };
			_video_stream->codecpar->codec_tag = 0;// 清除特定的格式相关标签，让输出格式决定

			_video_stream->time_base = timebase;
			_video_stream->id = 0;
			_video_stream->avg_frame_rate = { 30, 1 };

			if (!(_out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
				ret = avio_open(&_out_fmt_ctx->pb, _url.c_str(), AVIO_FLAG_WRITE);
				if (ret < 0) {
					if (_out_fmt_ctx) {
						avformat_free_context(_out_fmt_ctx);
						_out_fmt_ctx = nullptr;
					}
					_error_description = "av io open failed, error description: " + AVError2String(ret) +
						", url: " + _url;
					eap_error(_error_description);
					throw std::system_error(ret, std::system_category(), _error_description);
				}
				_is_avio_open_called = true;
			}

			_start_open_time = std::chrono::system_clock::now();

			AVDictionary* options{};
			if (_url.find("rtsp://") == 0) {
				av_dict_set(&options, "rtsp_transport", "tcp", 0);
				av_dict_set(&options, "stimeout", "2000000", 0); // 设置超时为 2 秒
				eap_information_printf("pusher rtsp url: %s", _url);
			}/*else if(_url.find("rtmp://") == 0) {
				av_dict_set(&options, "flvflags", "no_duration_filesize", 0);
				av_dict_set(&options, "flvflags", "no_sequence_end", 0);
			}*/
			std::string visual_url = eap::configInstance().has(Media::kVisualMulticastUrl) ? eap::configInstance().getString(Media::kVisualMulticastUrl) : "";
			if (visual_url == _url && !_local_addr.empty() && _url.find("udp://") == 0) {
				av_dict_set(&options, "localaddr", _local_addr.c_str(), 0);
				eap_information_printf("localaddr: %s, push url: %s", _local_addr, _url);
			}
			//if (_url.find("srt://") == 0) {
			//	av_dict_set(&options, "latency", "200", 0);// 直播场景建议150-400ms
			//	av_dict_set(&options, "payload_size", "1316", 0);//适应标准MTU
			//	av_dict_set(&options, "maxbw", "10000000", 0);
			//	av_dict_set(&options, "congestion", "live", 0); // 拥塞控制策略 
			//	av_dict_set(&options, "messageapi", "1", 0);  // 保证消息完整性
			//	av_dict_set(&options, "sndbuf", "4194304", 0);//缓冲区
			//	av_dict_set(&options, "rcvbuf", "4194304", 0);
			//	av_dict_set(&options, "fec", "rows:1,cols:4", 0);//FEC (前向纠错) 配置
			//	av_dict_set(&options, "enforced_encryption", "0", 0);
			//	av_dict_set(&options, "tsbpddelay", "200", 0);// 时间戳处理延迟
			//	av_dict_set(&options, "snddropdelay", "200", 0);// 发送超时丢弃
			//}

			if ((ret = avformat_write_header(_out_fmt_ctx, &options)) < 0) {
				_error_description = "muxer write header failed, error description: " + AVError2String(ret) +
					", url: " + _url;
				eap_error(_error_description);

				if (_out_fmt_ctx && _out_fmt_ctx->pb) {
					if (_is_avio_open_called) {
						avio_closep(&_out_fmt_ctx->pb);
						_is_avio_open_called = false;
					}
					else {
						av_freep(_out_fmt_ctx->pb);
						_out_fmt_ctx->pb = nullptr;
					}
				}

				if(_out_fmt_ctx){
					avformat_free_context(_out_fmt_ctx);
					_out_fmt_ctx = nullptr;
				}
				
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			eap_information_printf("open pusher, url: %s", _url);

			_is_opened = true;

			_push_thread = std::thread([this, id]() {
				_push_run = true;

				bool first_packet{ true };
				int64_t start_pts = 0;
				int64_t last_pts = 0;

				for (; _push_run;) {
					Packet pkt{};

					std::unique_lock<std::mutex> lock(_packets_mutex);
					if (_packets.empty()) {
						_packets_ready = false;
						_packets_cv.wait(lock, [this] {
							return _packets_ready || !_push_run;
						});
					}

					if (!_push_run) {
						break;
					}

					// 缓存10帧，做平滑
					// if (first_packet && _packets.size() < 10) {
					// 	continue;
					// }

					pkt = _packets.front();
					_packets.pop();
					lock.unlock();

					if (first_packet) {
						_start_push_time = std::chrono::system_clock::now();
						start_pts = pkt->pts;
						first_packet = false;
						_first_packet = true;
					}
					
					//if (pkt->pts <= last_pts) {
					//	pkt->pts = last_pts + 5;
					//	//eap_warning_printf("packet pts is less than last pts, pts: %d, last_pts: %d", (int)pkt->pts, (int)last_pts);
					//	std::string err_msg = "pts between frames is too small";
					//	NoticeCenter::Instance()->getCenter().postNotification(
					//		new VideoMsgNotice(id, (int)VideoMsgNotice::VideoCodeType::PtsBetweenLarge, err_msg));
					//}
					pkt->pts -= start_pts;
					// 当时间戳不合适时，累加的帧间时间戳
					if (pkt->pts <= last_pts)
					{
						pkt->pts = last_pts + 2;
					}
					last_pts = pkt->pts;
					pkt->dts = pkt->pts;

					try {
						/*std::chrono::microseconds remaining_time = std::chrono::microseconds(0);
						calcRemainingTime(pkt, remaining_time);
						if (remaining_time > std::chrono::microseconds(0)) {
							std::this_thread::sleep_for(remaining_time);
						}*/

						if (_format_name == "rtsp") {
							_start_read_frame_time = std::chrono::system_clock::now();
						}
						writePacket(pkt);
						_last_readed_frame_timepoint = std::chrono::steady_clock::now();
						_is_readed_first_frame = true;
					}
					catch (std::system_error& e) {
						if (_stop_callback) {
							_stop_callback(e.code().value(), e.what());
						}
						return;
					}
				}

				//av_write_trailer(_out_fmt_ctx);
				eap_information_printf("pusher push thread exited, url: %s", _url);

				if (_stop_callback) {
					_stop_callback(0, "");
				}
			});
			if (!_check_server_run) {
				_check_server_run = true;
				_media_server_thread = std::thread([this]() {
					std::string visual_url = eap::configInstance().has(Media::kVisualMulticastUrl) ? eap::configInstance().getString(Media::kVisualMulticastUrl) : "";
					if (!_media_server_ip.empty()) {
						for (; _check_server_run;) {
							if (!ping(_media_server_ip)) {
								eap_error_printf("ping failed, ip: %s, url: %s", _media_server_ip, _url);
							}
							std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						}
					}
					else if (visual_url == _url) {
#ifdef ENABLE_AIRBORNE
						//机载推流udp组播，定频查看推流是否卡住
						for (; _check_server_run;) {
							if (_is_readed_first_frame && _push_run) {
								auto now_t = std::chrono::steady_clock::now();
								auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
									now_t - _last_readed_frame_timepoint);
								if (elapsed < std::chrono::milliseconds(3000)) {
									_read_timeout_cnt = 0;
								}
								else {
									_read_timeout_cnt++;
									eap_information_printf("pusher write frame timeout 3s, pull url: %s", _url);
									if (_read_timeout_cnt > 35) {
										//70s没有推流成功，重启sma程序
										Poco::Path current_exe = Poco::Path::self();
										auto process_name = current_exe.getFileName();
										eap::cleanProcessByName(process_name);
									}
								}
							}
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
#endif
					}
				});
			}
		}

		void PusherTradition::close()
		{
			if(!_url.empty()){
				eap_error_printf("start close pusher, url: %s", _url);
				if (_push_thread.joinable()) {
					_push_run = false;
					_packets_cv.notify_all();
					_push_thread.join();
				}
				
				if (_out_fmt_ctx) {
					av_write_trailer(_out_fmt_ctx);

					if (_out_fmt_ctx && !(_out_fmt_ctx->oformat->flags & AVFMT_NOFILE)){
						avio_closep(&_out_fmt_ctx->pb);
					}        				

					// 关闭AVIOContext
					if (_out_fmt_ctx->pb) {
						if (_is_avio_open_called) {
							avio_closep(&_out_fmt_ctx->pb);
							_is_avio_open_called = false;
						} else {
							av_freep(_out_fmt_ctx->pb);
							// 注意：av_freep 会将指针置为 NULL，所以不需要再手动设置 _out_fmt_ctx->pb = nullptr
						}
					}

					// 释放格式上下文
					avformat_free_context(_out_fmt_ctx);
					_out_fmt_ctx = nullptr;
				}
#if 0
				if (_out_fmt_ctx && _out_fmt_ctx->pb) {
					if (_is_avio_open_called) {
						avio_closep(&_out_fmt_ctx->pb);
						_is_avio_open_called = false;
					}
					else {
						av_freep(_out_fmt_ctx->pb);
						_out_fmt_ctx->pb = nullptr;
					}
				}
				if (_out_fmt_ctx) {

					// 释放格式上下文
					avformat_free_context(_out_fmt_ctx);
					_out_fmt_ctx = nullptr;
				}
#endif
				_is_opened = false;
				_is_closed = true;
				_first_packet = false;

				eap_error_printf("close pusher, url: %s", _url);
			}
		}

        void PusherTradition::pause(int paused)
        {
			if(1 == paused){
				_paused.store(true);
			}else{
				_paused.store(false);
			}
        }

        void PusherTradition::pushPacket(Packet pkt)
		{
			std::lock_guard<std::mutex> lock(_packets_mutex);
			while (_packets.size() > 50) {
				_packets.pop();
			}
			_packets.push(pkt);
			_packets_ready = true;
			_packets_cv.notify_all();
		}

		void PusherTradition::checkFormat()
		{
			if (0 == _url.find("rtmp://")) {
				_format_name = "flv";
			}
			else if (0 == _url.find("udp://") || 0 == _url.find("srt://")) {
				_format_name = "mpegts";
			}
			else if (0 == _url.find("rtsp://")) {
				_format_name = "rtsp";
			}

			// 正则匹配 rtsp://, rtmp:// 或 srt:// 后的 IP（IPv4 格式）
			std::regex pattern(R"((?:rtsp|rtmp|srt)://([0-9]{1,3}(?:\.[0-9]{1,3}){3})(?::[0-9]+)?)");
			std::smatch match;
			if (std::regex_search(_url, match, pattern)) {
				if (match.size() > 1) {
					_media_server_ip = match[1].str();
					eap_information_printf("_media_server_ip: %s, url: %s", _media_server_ip, _url);
				}
			}
		}

		void PusherTradition::calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time)
		{
			auto pts = packet->pts;
			auto dts = packet->dts;
			auto duration = packet->duration;

			AVRational  dst_time_base = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(packet->pts, _in_timebase, dst_time_base);
			int64_t now_time = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now() - _start_push_time).count();
			auto remaining_time_t = pts_time - now_time;
			if ((remaining_time_t > 0) &&
				(remaining_time_t < AV_TIME_BASE * 10)) {
				remaining_time = std::chrono::microseconds(remaining_time_t);
			}
		}

		void PusherTradition::writePacket(Packet pkt)
		{
			pkt->pts = av_rescale_q_rnd(pkt->pts, _in_timebase, _video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->dts = av_rescale_q_rnd(pkt->dts, _in_timebase, _video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->duration = av_rescale_q(pkt->duration, _in_timebase, _video_stream->time_base);
			pkt->pos = -1;
			pkt->stream_index = 0;
			//printf("ts: %lf\n", pkt->pts * av_q2d(_video_stream->time_base));

			int ret = av_write_frame(_out_fmt_ctx, pkt);
			if (ret < 0) {
				if (ret == AVERROR(EINVAL)) {
					_error_description = "Write packet failed, error description: " + AVError2String(ret) + " ,push_url: "+ _url;
					eap_error(_error_description);
				}
				else {
					_error_description = "Write packet failed, error description: " + AVError2String(ret) + " ,push_url: " + _url;
					eap_error(_error_description);
					throw std::system_error(ret, std::system_category(), _error_description);
				}
			}
		}

		int PusherTradition::interrupt_callback(void* opaque)
		{
			PusherTradition* _this = (PusherTradition*)opaque;
			if (_this->_is_closed) {
				eap_information_printf("interrupt_callback, url: %s", _this->_url);
				_this->_is_closed = false;
				return 1;
			}
			//超时没有推流成功
			if (!_this->_is_opened) {
				auto now = std::chrono::system_clock::now();
				auto d = std::chrono::duration_cast<std::chrono::milliseconds>(now - _this->_start_open_time);
				if (d >= _this->_open_timeout) {
					return 1;
				}
			}

			// rtsp偶现original流没断，但是拉不到流，av_read_frame也不报错退出
			if (_this->_is_opened && _this->_first_packet && _this->_format_name == "rtsp") {
				auto now_rtsp = std::chrono::system_clock::now();
				auto duration_rtsp = std::chrono::duration_cast<std::chrono::milliseconds>(now_rtsp - _this->_start_read_frame_time).count();
				if (duration_rtsp >= 8000) {
					eap_error_printf("pusher rtsp have no frame >= 8 sec, url: %s", _this->_url);
					return 1;
				}
			}

			return 0;
		}
		bool PusherTradition::ping(const std::string& ip)
		{
			try {
				int ret{};
				bool ret_state{};
#if defined(__linux__) || defined(__linux)
				std::string cmd = "ping -c 1 -W 2 " + ip + " > /dev/null 2>&1";
				ret = std::system(cmd.c_str());
				ret_state = (ret == 0 ? true : false);
#else
				ret = Poco::Net::ICMPClient::pingIPv4(ip, 1, 48, 128, 2000);
				ret_state = (ret == 1);
#endif
				return ret_state;
			}
			catch (const std::exception& e) {
				eap_error_printf("ping failed: %s, ip: %s", std::string(e.what()), ip);
			}
			return false;
		}
	}
}