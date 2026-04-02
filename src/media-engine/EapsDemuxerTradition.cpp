#include "EapsDemuxerTradition.h"
#include "Logger.h"
#include "EapsConfig.h"
#include "Utils.h"
#include "Poco/ThreadPool.h"
#include "ThreadPool.h"

#include <stdexcept>
#include <system_error>
#include <mutex>
#include <fstream>

namespace eap {
	namespace sma {
		DemuxerTraditionPtr DemuxerTradition::createInstance()
		{
			return DemuxerTraditionPtr(new DemuxerTradition());
		}

		DemuxerTradition::DemuxerTradition()
		{
		}

		DemuxerTradition::~DemuxerTradition()
		{
			close();
		}

		void DemuxerTradition::setPacketCallback(PacketCallback packet_callback)
		{
			_packet_callabck = packet_callback;
		}

		void DemuxerTradition::setStopCallback(StopCallback stop_callback)
		{
			_stop_callback = stop_callback;
		}

		void DemuxerTradition::open(std::string url, std::chrono::milliseconds timeout, std::string local_addr)
		{
			eap_information_printf("demuxer open url: %s", url);
			_interrupt_flag = false;
			_local_addr = local_addr;
			static  std::once_flag _once_flag{};
			std::call_once(_once_flag, []() {
#ifndef ENABLE_3588
				av_register_all();
#endif // !ENABLE_3588
				avformat_network_init();
			});

			_url = url;
			_timeout = timeout;
			checkNetwork();
			int ret{};
			_fmt_ctx = avformat_alloc_context();
			if (!_fmt_ctx) {
				_error_description = "alloc format context failed";
				eap_error(_error_description);
				throw std::bad_alloc();
			}

			if (_is_network || _is_http) {
				_fmt_ctx->interrupt_callback.callback = interruptCallback;
				_fmt_ctx->interrupt_callback.opaque = this;
				_start_open_time = std::chrono::system_clock::now();
			}

			ret = avformat_open_input(&_fmt_ctx, _url.c_str(), NULL, &_fmt_options);
			if (ret != 0) {
				_error_description = "Cannot open input video '" + _url + "', local_address = " + _local_addr + "', error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}
			_fmt_opened = true;

			if (_interrupt_flag) {
				_error_description = "manual interrupted";
				eap_error(_error_description);
				throw std::system_error(_interrupt_flag, std::system_category(), _error_description);
			}

			ret = avformat_find_stream_info(_fmt_ctx, NULL);
			if (ret < 0) {
				_error_description = "Cannot find input stream information, error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}
			if (_interrupt_flag) {
				_error_description = "manual interrupted";
				eap_error(_error_description);
				throw std::system_error(_interrupt_flag, std::system_category(), _error_description);
			}

			if (_fmt_ctx->start_time != AV_NOPTS_VALUE) {
				_video_start_time = _fmt_ctx->start_time;
			}

			if (_fmt_ctx->duration != AV_NOPTS_VALUE) {
				_video_duration = _fmt_ctx->duration / 1000000LL;
			}

			ret = av_find_best_stream(_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
			if (ret < 0) {
				_error_description = "Cannot find a video stream in the input file, error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}
			_video_stream_index = ret;
			auto video_stream = _fmt_ctx->streams[_video_stream_index];
			if (video_stream->time_base.num > 0 && video_stream->time_base.den > 0) {
				_video_timebase = video_stream->time_base;
			}

			if (video_stream->avg_frame_rate.num > 0 && video_stream->avg_frame_rate.den > 0) {
				_video_frame_rate = video_stream->avg_frame_rate;
			}
			else {
				//_video_frame_rate = { 30, 1 };
				_should_test_frame_rate = true;
			}

			_video_stream = video_stream;

			if (_video_timebase.num <= 0 || _video_timebase.den <= 0) {
				_error_description = "Parse error, timebase num or den is 0";
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			_video_codec_par = *_video_stream->codecpar;
			if (_video_codec_par.width <= 0 || _video_codec_par.height <= 0) {
				_error_description = "Parse error, width or height is 0";
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			if (_fmt_ctx->bit_rate > 0 && _fmt_ctx->bit_rate != AV_NOPTS_VALUE) {
				_video_bit_rate = _fmt_ctx->bit_rate;
			}
			else {
				_video_bit_rate = 5000000;
			}

			if(_should_test_frame_rate && _is_network){
				auto start_t = std::chrono::system_clock::now();
				bool first_read{true};
				int frames{};

				while (true) {
					AVPacket* pkt_r = av_packet_alloc();
					int ret = av_read_frame(_fmt_ctx, pkt_r);
					if(ret < 0){
						_error_description = "av read frame failed for test frame rate, error: " + AVError2String(ret);
						eap_error(_error_description);
						throw std::system_error(ret, std::system_category(), _error_description);
					}
					if(pkt_r->stream_index != _video_stream_index){
						av_packet_unref(pkt_r);
						continue;
					}
					if(first_read){
						first_read = false;
						start_t = std::chrono::system_clock::now();
					}
					++frames;
					auto end_t = std::chrono::system_clock::now();
					if(std::chrono::duration_cast<std::chrono::milliseconds>(end_t - start_t) 
						>= std::chrono::milliseconds(2000)){
						break;
					}
				}

				float frame_count = frames / 2.0;
				if(frame_count < 45){
					_video_frame_rate = {30, 1};
				}
				else{
					_video_frame_rate = {50, 1};
				}
			}

			if(_should_test_frame_rate && !_is_network){
				_video_frame_rate = video_stream->r_frame_rate;
				if(_video_frame_rate.den <= 0
					|| _video_frame_rate.num <= 0){
					_video_frame_rate = {30, 1};
				}
			}

			eap_information_printf("Open url '%s' success, video timebase: %d/%d, video frame rate: %d/%d, width: %d, height: %d, addr: %s",
				_url, _video_timebase.num, _video_timebase.den, _video_frame_rate.num, _video_frame_rate.den,
				_video_codec_par.width, _video_codec_par.height, _local_addr);

			_video_timebase = { 1, 1000 };
			_video_frame_rate = { 30, 1 };
			int visual_input_frame_rate{};
			try {
				GET_CONFIG(int, getInt, my_visual_input_frame_rate, Media::kVisualInputFrameRate);
				visual_input_frame_rate = my_visual_input_frame_rate;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kVisualInputFrameRate throw exception: %s", e.what());
			}
			if(visual_input_frame_rate > 0 && visual_input_frame_rate < 50){
				_video_frame_rate = {visual_input_frame_rate, 1};
				eap_information_printf("set video frame rate to %d", visual_input_frame_rate);
				_is_rewrite_pts = true;
			}

			_read_thread = std::thread([this, video_stream, timeout]() {
				std::int64_t last_pts = 0;
				std::int64_t interval_pts = 0;
				_read_run = true;
				std::chrono::system_clock::time_point first_file_time{};
				bool is_first_file_packet{ true };
				std::chrono::system_clock::time_point _first_read_time{};

				float ts_interval = 0;
				float current_ts = 0;
				int64_t total_bits{0};          // 总比特数
				int packet_count = 0;

				std::string pkt_send_duration{};
				std::string pkt_time_send_duration{};
				std::chrono::steady_clock::time_point push_second_time{};
				std::chrono::steady_clock::time_point last_push_time{};

				eap_information("Start demuxer read thread");

				int exit_code = 0;
				for (; _read_run;) {
					if (!_is_network) {
						if(_paused != _last_paused){
							_last_paused.store(_paused.load());
							if(_paused){
								_read_pause_return = av_read_pause(_fmt_ctx);
							}
							else{
								_read_play_return = av_read_play(_fmt_ctx);
							}
						}
						seekExecute();
						av_usleep(15000);
					}

					AVPacket* pkt_r = av_packet_alloc();
					if(!_paused){
						if (_is_rtsp) {
							_start_read_frame_time = std::chrono::steady_clock::now();
							_read_frame_timeout = false;
						}
						int ret = av_read_frame(_fmt_ctx, pkt_r);
						if (ret != 0) {
							if(_is_http && (ret == AVERROR_INVALIDDATA)){
								std::this_thread::sleep_for(std::chrono::milliseconds(10));
								eap_warning("invalid data, pause recovery recovery recovery recovery recovery recovery recovery +++++++++++ ");
								continue;
							}
							exit_code = ret;
							eap_error_printf("read frame failed, error: %s", AVError2String(ret));
							break;
						}
						// 用作暂停恢复时的开始时间戳
						_last_pkt_pts = pkt_r->pts;
						if(_video_stream_index == pkt_r->stream_index && AV_PKT_FLAG_KEY == pkt_r->flags){
							if(_last_packet){ 
								av_packet_free(&_last_packet);
							}
							_last_packet = av_packet_clone(pkt_r);
						}
					}
					else{
						// 暂停时，一直使用最后一帧。因为http不会重写时间戳，时间戳一直不会变
						if (_last_packet) {
							pkt_r = av_packet_clone(_last_packet);
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(33));// 先暂时按照每秒30帧拷贝

						// 暂停时需要更新开始 开始时间 和 _start_pts，因为如果不更新，那么暂停一段时间恢复后，新来的视频帧的时间戳相对于暂停时的帧的时间戳之差，
						// 远远小于我们自己根据实际时间的计时，就是以最快的速度往外发，导致暂停久了之后恢复会倍速一段时间才恢复
						_start_read_time = std::chrono::system_clock::now();
						AVRational dst_time_base = { 1, AV_TIME_BASE };
						// 应该用暂停前最后一帧的时间戳来更新_start_pts，因为现在的pkt_read是暂停前最近的关键帧
						_start_pts = av_rescale_q(_last_pkt_pts, _video_stream->time_base, dst_time_base);
					}

					if (pkt_r->stream_index != _video_stream_index) {
						av_packet_free(&pkt_r);
						continue;
					}
					_is_readed_first_frame.store(true);
					_read_timepoint_mutex.lock();
					_last_readed_frame_timepoint = std::chrono::steady_clock::now();
					_read_timepoint_mutex.unlock();

					AVPacket* pkt{};
					int64_t original_pts = pkt_r->pts;
					if (_fmt_ctx->streams[pkt_r->stream_index]->codecpar->extradata) {
						if (_is_http_flv_rtmp) {
							pkt = av_packet_clone(pkt_r);
							av_packet_unref(pkt_r);
							av_packet_free(&pkt_r);
						}
						else {
							//为包数据添加起始码、SPS/PPS等信息
							h264_mp4toannexb(_fmt_ctx, pkt_r, &pkt);
							pkt->pts = pkt_r->pts;
							pkt->dts = pkt_r->dts;
							pkt->duration = pkt_r->duration;
							pkt->pos = pkt_r->pos;
							pkt->flags = pkt_r->flags;

							if (pkt && pkt != pkt_r) {
								av_packet_unref(pkt_r);
							}
						}
					}
					else {
						pkt = av_packet_clone(pkt_r);
						av_packet_unref(pkt_r);
						av_packet_free(&pkt_r);
					}

					if (_first_packet) {
						if (pkt->flags & AV_PKT_FLAG_KEY) {
							AVRational dst_time_base = { 1, AV_TIME_BASE };
							_start_pts = av_rescale_q(pkt->pts, _video_stream->time_base, dst_time_base);
							_first_packet = false;
							_first_read_time = std::chrono::system_clock::now();
							_start_read_time = _first_read_time;
							push_second_time = std::chrono::steady_clock::now();
							last_push_time = std::chrono::steady_clock::now();
						} else {
							av_packet_unref(pkt);
							av_packet_free(&pkt);
							continue;
						}
					}

					if(!_is_http){
						if (!_is_network) {
							std::chrono::microseconds remaining_time = std::chrono::microseconds(0);
							calcRemainingTime(pkt, remaining_time);
							if (remaining_time > std::chrono::microseconds(0)) {
								std::this_thread::sleep_for(remaining_time);
							}
							if (is_first_file_packet) {
								first_file_time = std::chrono::system_clock::now();
								is_first_file_packet = false;
							}
							auto now = std::chrono::system_clock::now();
							auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(now - first_file_time).count() / 1000.f;
							pkt->pts = pkt->dts = ts / av_q2d(_video_timebase);
						}
						else {
							if (_is_rewrite_pts) {
								interval_pts = 1 / av_q2d(_video_frame_rate) / av_q2d(_video_timebase);
								// TODO: 目前这种写法，如果中间断开，再重连，重连上来的pkt->pts会远远小于上一次的last_pts，导致muxe时失败。先在muxer中修复
								// 而且如果中间断开，也会出现重连后pkt->pts会远远大于上一次的last_pts，导致最终录像播放时，直接跳一大段。先在muxer中修复
								if (!_first_send_pkt) {
									pkt->pts = pkt->dts = last_pts + interval_pts;
								}
								else {
									// 避免第一帧时间戳异常
									pkt->pts = pkt->dts = 0;
									_first_send_pkt = false;
								}
								last_pts = pkt->pts;
#ifdef WIN32
								//星图webrtc视频流计算视频码率
								auto pkt_time = std::chrono::steady_clock::now();
								int64_t now_time = std::chrono::duration_cast<std::chrono::milliseconds>(
									pkt_time - push_second_time).count();
								total_bits += pkt->size * 8;
								if (now_time >= 1000) {
									//计算实时码率
									push_second_time = pkt_time;
									_video_bit_rate = total_bits;
									//eap_information_printf(" pkt_bit rate: %d ", (int)total_bits);
									total_bits = 0;
								}
#endif
								auto new_pkt_duration = av_rescale_q(pkt->duration, video_stream->time_base, _video_timebase);
								pkt->duration = new_pkt_duration;
							}
							else {
								// 不重写时间戳，需要改成统一_video_timebase的时间基
								av_packet_rescale_ts(pkt, video_stream->time_base, _video_timebase);
							}
						}
					}
					else{
						// 由于http 拉流速度可能存在异常，增加等待时间，以保证按照正确时间点推出
						std::chrono::microseconds remaining_time = std::chrono::microseconds(0);
						calcRemainingTime(pkt, remaining_time);
						if (remaining_time > std::chrono::microseconds(0)) {
							std::this_thread::sleep_for(remaining_time);
						}
					}

					if (_packet_callabck && _is_packet_callback) {
						Packet packet(pkt);
						packet.setOriginalPts(original_pts);
						_packet_callabck(packet);
					}
					else {
						av_packet_unref(pkt);
						continue;
					}
				}

				if (_stop_callback) {
					_stop_callback(exit_code);
				}

				eap_error_printf("demuxer read thread exited, url: %s, read_frame_timeout: %d", _url, (int)_read_frame_timeout);
			});

			if (_is_network) {
				_timeout_check_timer.start(2000, [this]()
				{
					if (_is_readed_first_frame && _read_run) {
						_read_timepoint_mutex.lock();
						auto now_t = std::chrono::steady_clock::now();
						auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
							now_t - _last_readed_frame_timepoint);
						_read_timepoint_mutex.unlock();
						if (_is_timeout) {
							if (elapsed < std::chrono::milliseconds(3000)) {
								_is_timeout = false;
								if (_stream_recover_callback) {
									_stream_recover_callback();
								}
							}
						}
						else {
							if (elapsed > std::chrono::milliseconds(3000)) {
								_is_timeout = true;
								if (_timeout_callback) {
									_timeout_callback();
								}
							}
						}
						if (elapsed > std::chrono::milliseconds(3000)) {
							_read_timeout_cnt++;
#ifdef ENABLE_AIRBORNE
							eap_information_printf("demuxer read frame timeout 3s, pull url: %s", _url);
							if (_read_timeout_cnt > 30) {
								//1分钟没有拉到吊舱视频流，重启sma程序
								Poco::Path current_exe = Poco::Path::self();
								auto process_name = current_exe.getFileName();
								eap::cleanProcessByName(process_name);
							}
#endif
						}
						else {
							_read_timeout_cnt = 0;
						}
					}
				});
			}
		}

		void DemuxerTradition::close(bool is_stop)
		{
			if(!_url.empty()){
				_timeout = std::chrono::milliseconds(0);
				if(is_stop){
					eap_warning_printf("demuxer manal close, url: %s", _url);
					_interrupt_flag = true;
				}

				if (_read_thread.joinable()) {
					_read_run = false;
					_read_thread.join();
				}

				if (_fmt_ctx && _fmt_opened) {
					_fmt_opened = false;
					avformat_close_input(&_fmt_ctx);
					_fmt_ctx = nullptr;
				}
				if (_timestamp_fliter) {
					_timestamp_fliter.reset();
					_timestamp_fliter = nullptr;
				}
			
				eap_error_printf("demuxer closed, url: %s, addr: %s", _url, _local_addr);
				_first_packet = true;
				_url = "";
			}
		}

		void DemuxerTradition::seek(float percent)
		{
			std::lock_guard<std::mutex> lock(_seeking_mutex);

			seeking_percent = percent;
			is_seeking_req = true;
		}

        void DemuxerTradition::pause(int paused)
        {
			if(1 == paused){
				_paused.store(true);
			}else{
				_paused.store(false);
			}
        }

        AVCodecParameters DemuxerTradition::videoCodecParameters()
		{
			return _video_codec_par;
		}

		AVRational DemuxerTradition::videoStreamTimebase()
		{
			return _video_timebase;
		}

		AVRational DemuxerTradition::videoFrameRate()
		{
			return _video_frame_rate;
		}

		int64_t DemuxerTradition::videoDuration()
		{
			return _video_duration;
		}

		int64_t DemuxerTradition::videoStartTime()
		{
			return _video_start_time;
		}

		void DemuxerTradition::setTimeoutCallback(TimeoutCallback timeout_callback)
		{
		}

		void DemuxerTradition::setStreamRecoverCallback(StreamRecoverCallback stream_recover_callback)
		{
		}

		void DemuxerTradition::startCache()
		{
			_is_packet_callback.store(true);
		}

		void DemuxerTradition::stopCache()
		{
			_is_packet_callback.store(false);
		}

		bool DemuxerTradition::isTimeout()
		{
			return _is_timeout;
		}

        bool DemuxerTradition::isReadFrameTimeout()
        {
            return _read_frame_timeout;
        }

		int64_t DemuxerTradition::bitRate()
		{
			return _video_bit_rate;
		}

		static bool isFlv(const std::string& file_name)
		{
			std::ifstream file(file_name, std::ios::binary);
			if (!file.is_open()) {
				eap_error_printf("flv file open fail: %s", file_name);
				return false;
			}

			char sig_nature[3];
			file.read(sig_nature, 3);
			if (sig_nature[0] != 'F' || sig_nature[1] != 'L' || sig_nature[2] != 'V') {
				eap_information_printf("current video file is not flv, url: %s", file_name);
				return false;
			}

			file.close();
			return true;
		}
        void DemuxerTradition::checkNetwork()
		{
			std::string mp4_suffix(".mp4");
			std::string ts_suffix(".ts");
			std::string flv_suffix(".flv");

			std::string mp4_live_suffix(".live.mp4");
			std::string ts_live_suffix(".live.ts");
			std::string flv_live_suffix(".live.flv");

			auto check_is_live = [&](std::string& url)
			{
				if (url.rfind(mp4_suffix) == url.length() - mp4_suffix.length()) {
					if (url.rfind(mp4_live_suffix) == url.length() - mp4_live_suffix.length()) {
						return true;
					}
					else {
						return false;
					}
				}
				else if (url.rfind(ts_suffix) == url.length() - ts_suffix.length()) {
					if (url.rfind(ts_live_suffix) == url.length() - ts_live_suffix.length()) {
						return true;
					}
					else {
						return false;
					}
				}
				else if (url.rfind(flv_suffix) == url.length() - flv_suffix.length()) {
					if (url.rfind(flv_live_suffix) == url.length() - flv_live_suffix.length()) {
						return true;
					}
					else {
						return false;
					}
				}
				else {
					return true;
				}
			};

			if (_url.find("udp://") == 0) {
				_is_network = true;
				_is_udp = true;
				av_dict_set(&_fmt_options, "fflags", "nobuffer", 0);
				av_dict_set(&_fmt_options, "buffer_size", "1024000", 0);
				av_dict_set(&_fmt_options, "reuse", "1", 0);
				av_dict_set(&_fmt_options, "reuse_socket", "1", 0);
				av_dict_set(&_fmt_options, "analyzeduration", "10000000", 0);
				av_dict_set(&_fmt_options, "probesize", "50000000", 0);
				if (!_local_addr.empty()) {
					av_dict_set(&_fmt_options, "localaddr", _local_addr.c_str(), 0);
					eap_information_printf("localaddr: %s, pull url: %s", _local_addr, _url);
				}
			}
			else if (_url.find("rtsp://") == 0) {
				_is_network = check_is_live(_url);
				_is_rtsp = true;
				av_dict_set(&_fmt_options, "fflags", "nobuffer", 0);// 减少缓冲
				av_dict_set(&_fmt_options, "rtsp_transport", "tcp", 0);
				av_dict_set(&_fmt_options, "analyzeduration", "3000000", 0);// 3秒探测
				av_dict_set(&_fmt_options, "stimeout", "5000000", 0); // 5秒超时（单位：微秒）
				av_dict_set(&_fmt_options, "buffer_size", "1024000", 0); // 1MB 缓冲区
				//max_delay + reorder_queue_size 可以减少缓冲卡顿
				av_dict_set(&_fmt_options, "max_delay", "1000000", 0);  // 1000ms 最大延迟
				av_dict_set(&_fmt_options, "reorder_queue_size", "0", 0);  //RTP 数据包重排序缓冲区大小,禁用重排序
			}
			else if (_url.find("rtmp://") == 0) {
				_is_network = check_is_live(_url);
				_is_http_flv_rtmp = true;
				av_dict_set(&_fmt_options, "fflags", "nobuffer", 0);
				av_dict_set(&_fmt_options, "probesize", "5000000", 0);
			}
			else if (_url.find("http://") == 0) {
				_is_network = check_is_live(_url);
				_is_http = true;
				_is_rtsp = true;
				_is_http_flv_rtmp = true;
				av_dict_set(&_fmt_options, "fflags", "nobuffer", 0);
				av_dict_set(&_fmt_options, "probesize", "5000000", 0);
			}
			else if (isFlv(_url))
			{
				_is_http_flv_rtmp = true;

				av_dict_set(&_fmt_options, "fflags", "nobuffer", 0);
				av_dict_set(&_fmt_options, "probesize", "5000000", 0);
			}
		}

		void DemuxerTradition::seekExecute()
		{
			std::lock_guard<std::mutex> lock(_seeking_mutex);

			if (!is_seeking_req) {
				return;
			}
			is_seeking_req = false;

			int seek_flags = 0;
			int64_t seek_pos = 0;
			int64_t seek_rel = 0;
			if (seeking_percent > 0 && seeking_percent <= 100) {
				int64_t ts = ((double)seeking_percent / 100) * _fmt_ctx->duration;
				if (_fmt_ctx->start_time != AV_NOPTS_VALUE)
					ts += _fmt_ctx->start_time;

				seek_pos = ts;
				seek_rel = 0;
				seek_flags &= ~AVSEEK_FLAG_BYTE;

				int64_t seek_target = seek_pos;
				int64_t seek_min = seek_rel > 0 ? seek_target - seek_rel + 2 : INT64_MIN;
				int64_t seek_max = seek_rel < 0 ? seek_target - seek_rel - 2 : INT64_MAX;

				int ret = avformat_seek_file(_fmt_ctx, -1, seek_min, seek_target, seek_max, seek_flags);
				if (ret < 0) {
					eap_error_printf("error while seeking, error: %s", AVError2String(ret));
				}
				else {
					_first_packet = true;
				}
			}
		}

		void DemuxerTradition::calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time)
		{
			auto pts = packet->pts;
			auto dts = packet->dts;
			auto duration = packet->duration;

			AVRational  dst_time_base = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(packet->pts, _video_stream->time_base, dst_time_base);
			int64_t now_time = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now() - _start_read_time).count();
			auto remaining_time_t = pts_time - now_time;
			if ((remaining_time_t > 0) &&
				(remaining_time_t < AV_TIME_BASE * 10)) {
				//printf("sleep time: %lld\n", remaining_time_t);
				//av_usleep(remaining_time_t);
				remaining_time = std::chrono::microseconds(remaining_time_t);
			}
		}

		int DemuxerTradition::interruptCallback(void* opaque)
		{
			DemuxerTradition* _this = (DemuxerTradition*)opaque;
			if (!_this)
				return 1;
			if (_this->_interrupt_flag) {
				_this->_interrupt_flag = false;
				eap_error_printf("demuxer _interrupt_flag, url: %s", _this->_url);
				return 1;
			}
			if (!_this->_fmt_opened) {
				auto now = std::chrono::system_clock::now();
				auto duration = std::chrono::duration_cast<
					std::chrono::milliseconds>(now - _this->_start_open_time);
				if (duration >= _this->_timeout) {
					eap_error_printf("demuxer open timeout, url: %s", _this->_url);
					return 1;
				}
			}
			// rtsp偶现original流没断，但是拉不到流，av_read_frame也不报错退出
			if (!_this->_first_packet && _this->_is_rtsp) {
				auto now = std::chrono::steady_clock::now();
				auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _this->_start_read_frame_time).count();
				if (duration >= 5000) {
					_this->_read_frame_timeout = true;
					eap_error_printf("demuxer have no frame >= 5 sec, url: %s", _this->_url);
					return 1;
				}
			}

			return 0;
		}
	}
}
