#include "EapsDemuxerRtc.h"
#include "Logger.h"

#include <regex>
#include <list>
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

		DemuxerRtcPtr DemuxerRtc::createInstance()
		{
			return DemuxerRtcPtr(new DemuxerRtc());
		}

		DemuxerRtc::DemuxerRtc()
		{
		}

		DemuxerRtc::~DemuxerRtc()
		{
			close();
		}

		void DemuxerRtc::setPacketCallback(PacketCallback packet_callback)
		{
			_packet_callabck = packet_callback;
		}

		void DemuxerRtc::setStopCallback(StopCallback stop_callback)
		{
			_stop_callback = stop_callback;
		}

		void DemuxerRtc::open(std::string url, std::chrono::milliseconds timeout, std::string local_addr)
		{
			_url = url;
			_timeout = timeout;

			int ret{};

			_fmt_ctx = avformat_alloc_context();
			if (!_fmt_ctx) {
				_error_description = "alloc format context failed";
				eap_error(_error_description);
				throw std::bad_alloc();
			}

			_avio_ctx_buffer = (uint8_t*)av_malloc(_avio_ctx_buffer_size);
			if (!_avio_ctx_buffer) {
				ret = AVERROR(ENOMEM);
				_error_description = "Can't alloc avio ctx buffer, error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			auto read_packet_callback = [](void* opaque, uint8_t* buf, int buf_size) {
				DemuxerRtc* thiz = (DemuxerRtc*)opaque;

				std::unique_lock<std::mutex> lock(thiz->_video_buffer_mtx);
				if (thiz->_video_buffer.empty()) {
					thiz->_video_buffer_cv.wait(lock);
				}

				std::vector<uint8_t> buffer{};
				if (!thiz->_video_buffer.empty()) {
					buffer = thiz->_video_buffer.front();
					thiz->_video_buffer.pop();
				}

				lock.unlock();

				if (!buffer.empty()) {
					auto size = FFMIN(buf_size, buffer.size());
					memcpy(buf, buffer.data(), size);
					buf_size = size;
				}

				return buf_size;
			};

			_avio_ctx = avio_alloc_context(_avio_ctx_buffer, _avio_ctx_buffer_size,
				0, this, read_packet_callback, nullptr, nullptr);
			if (!_avio_ctx) {
				ret = AVERROR(ENOMEM);
				_error_description = "alloc avio context failed";
				eap_error(_error_description);
			}
			_fmt_ctx->pb = _avio_ctx;

			auto interrupt_callback = [](void* opaque) -> int {
				DemuxerRtc* _this = (DemuxerRtc*)opaque;

				if (_this->_interrupt_flag) {
					return _this->_interrupt_flag;
				}
				if (!_this->_fmt_opened) {
					auto now = std::chrono::system_clock::now();
					auto duration = std::chrono::duration_cast<
						std::chrono::milliseconds>(now - _this->_start_open_time);
					if (duration >= _this->_timeout) {
						return 1;
					}
				}

				return 0;
			};

			_fmt_ctx->interrupt_callback.callback = interrupt_callback;
			_fmt_ctx->interrupt_callback.opaque = this;

			initRtcReciever();

			_start_open_time = std::chrono::system_clock::now();
#ifdef ENABLE_3588
			const AVInputFormat* input_format = av_find_input_format("H264");
#else
			AVInputFormat* input_format = av_find_input_format("H264");
#endif

			ret = avformat_open_input(&_fmt_ctx, nullptr, input_format, &_fmt_options);
			if (ret != 0) {
				_error_description = "Cannot open input video '" + _url + "', error: " + AVError2String(ret);
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
			/*if (video_stream->time_base.num > 0 && video_stream->time_base.den > 0) {
				_video_timebase = video_stream->time_base;
			}*/

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

			if (_fmt_ctx->bit_rate > 0 && _fmt_ctx->bit_rate != AV_NOPTS_VALUE) {
				_video_bit_rate = _fmt_ctx->bit_rate;
			}
			else {
				_video_bit_rate = 5000000;
			}

			eap_information_printf("Open url '%s' success, video timebase: %d/%d, video frame rate: %d/%d, width: %d, height: %d",
				url.c_str(), _video_timebase.num, _video_timebase.den, _video_frame_rate.num, _video_frame_rate.den,
				_video_codec_par.width, _video_codec_par.height);

			_video_timebase = { 1, 1000 };
			_video_frame_rate = { 30, 1 };

			_read_thread = std::thread([this]() {
				_read_run = true;

				std::chrono::system_clock::time_point _first_read_time{};

				float ts_interval = 0;
				float current_ts = 0;

				int frame_count = 0;

				eap_information_printf("webrtc reciever read start, url: %s", _url.c_str());

				int exit_code = 0;

				for (; _read_run;) {
					AVPacket* pkt_read = av_packet_alloc();

					int ret = av_read_frame(_fmt_ctx, pkt_read);
					if (ret < 0) {
						exit_code = ret;
						eap_error_printf("av read frame failed, error: %s", AVError2String(ret));
						break;
					}
					if (pkt_read->stream_index != _video_stream_index) {
						av_packet_unref(pkt_read);
						continue;
					}

					AVPacket* pkt{};

					if (_fmt_ctx->streams[pkt_read->stream_index]->codecpar->extradata) {
						// 为包数据添加起始码、SPS/PPS等信息
						h264_mp4toannexb(_fmt_ctx, pkt_read, &pkt);

						pkt->pts = pkt_read->pts;
						pkt->dts = pkt_read->dts;
						pkt->duration = pkt_read->duration;
						pkt->pos = pkt_read->pos;
						pkt->flags = pkt_read->flags;

						if (pkt && pkt != pkt_read) {
							av_packet_unref(pkt_read);
						}
					}
					else {
						pkt = pkt_read;
					}

					if (_first_packet) {
						if (pkt->flags & AV_PKT_FLAG_KEY) {
							_start_pts = pkt->pts;

							_first_packet = false;
							_start_read_time = std::chrono::system_clock::now();
							_first_read_time = _start_read_time;
						}
						else {
							continue;
						}
					}

					++frame_count;

					auto now = std::chrono::system_clock::now();
					auto duration = now - _start_read_time;
					if (duration >= std::chrono::milliseconds(1000)) {
						_start_read_time = now;

						auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
							now - _first_read_time).count();

						auto framerate = frame_count / seconds;

						if (frame_count < 40) {
							_video_frame_rate = { 30, 1 };
						}
						else {
							_video_frame_rate = { 50, 1 };
						}

						ts_interval = 1000.f / _video_frame_rate.num;
					}

					current_ts += ts_interval;
					auto ts = current_ts / 1000.f;

					pkt->pts = pkt->dts = ts / av_q2d(_video_timebase);

					if (_packet_callabck) {
						Packet packet(pkt);
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

				eap_error_printf("webrtc reciever read thread exited, url: %s", _url);
			});
		}

		void DemuxerRtc::close(bool is_stop)
		{
			if(is_stop)
				_interrupt_flag = 1;
			_video_buffer_cv.notify_one();

			if (_read_thread.joinable()) {
				_read_run = false;
				_read_thread.join();
			}
			if (_fmt_ctx && _fmt_opened) {
				avformat_close_input(&_fmt_ctx);
				_fmt_ctx = nullptr;
			}

			if (_rtc_reciever) {
				jo_rtc_reciever_close(_rtc_reciever);
				jo_rtc_reciever_destroy(_rtc_reciever);
				_rtc_reciever = nullptr;
			}

			eap_error_printf("webrtc reciever closed, url: %s", _url);
		}

		void DemuxerRtc::seek(float percent)
		{

		}

		AVCodecParameters DemuxerRtc::videoCodecParameters()
		{
			return _video_codec_par;
		}

		AVRational DemuxerRtc::videoStreamTimebase()
		{
			return _video_timebase;
		}

		AVRational DemuxerRtc::videoFrameRate()
		{
			return _video_frame_rate;
		}

		int64_t DemuxerRtc::videoDuration()
		{
			return _video_duration;
		}

		int64_t DemuxerRtc::videoStartTime()
		{
			return _video_start_time;
		}

        void DemuxerRtc::pause(int paused)
        {
        }

		void DemuxerRtc::setTimeoutCallback(TimeoutCallback timeout_callback)
		{
		}

		void DemuxerRtc::setStreamRecoverCallback(StreamRecoverCallback stream_recover_callback)
		{
		}

		void DemuxerRtc::startCache()
		{
		}

		void DemuxerRtc::stopCache()
		{
		}

		bool DemuxerRtc::isTimeout()
		{
			return false;
		}

		bool DemuxerRtc::isReadFrameTimeout(){
			return false;
		}

		int64_t DemuxerRtc::bitRate()
		{
			return _video_bit_rate;
		}

        void DemuxerRtc::initRtcReciever()
		{
			_rtc_reciever = jo_rtc_reciever_create();
			jo_rtc_reciever_set_packet_callback(_rtc_reciever,
				[](uint8_t* packet, size_t bytes, uint32_t timestamp, int flags, void* opaque) {
				DemuxerRtc* thiz = (DemuxerRtc*)opaque;
				thiz->onRtcPacket(packet, bytes, timestamp, flags);
			}, this);
			jo_rtc_reciever_set_connect_state_changed_callback(_rtc_reciever,
				[](const joRtcState state, void* opaque) {
				DemuxerRtc* thiz = (DemuxerRtc*)opaque;

				if (state == JO_RTC_CLOSED || state == JO_RTC_FAILED ||
					state == JO_RTC_DISCONNECTED) {
					thiz->_interrupt_flag = true;
					thiz->_read_run = false;
					eap_error_printf("webrtc reciever closed, state: %d", (int)state);
				}
			}, this);
			JoRtcErrorCode rtc_errcode = jo_rtc_reciever_open(_rtc_reciever, _url.c_str());
			if (rtc_errcode != JO_SUCCEED) {
				_error_description = "webrtc reciever open failed: " + _url + ", error code: " + std::to_string(rtc_errcode);
				eap_error(_error_description);
				throw std::runtime_error(_error_description);
			}
		}

		void DemuxerRtc::onRtcPacket(uint8_t* packet, int bytes, uint32_t timestamp, int flags)
		{
			if (_first_rtp_packet) {
				_first_rtp_pts = timestamp;
				_first_rtp_packet = false;
			}

			std::list<std::vector<uint8_t>> temp_packets{};

			if (!_cache_buffer.empty()) {
				if (bytes + _cache_buffer.size() > kVideoBufferSize) {
					std::vector<uint8_t> temp = std::vector<uint8_t>(kVideoBufferSize);
					auto packet_copying_size = temp.size() - _cache_buffer.size();
					memcpy(temp.data(), _cache_buffer.data(), _cache_buffer.size());
					memcpy(temp.data() + _cache_buffer.size(), packet, packet_copying_size);

					temp_packets.push_back(std::move(temp));

					int bytes_temp = bytes - packet_copying_size;
					uint8_t* packet_temp = ((uint8_t*)packet) + packet_copying_size;

					for (; bytes_temp >= kVideoBufferSize;) {
						temp = std::vector<uint8_t>(kVideoBufferSize);
						memcpy(temp.data(), packet_temp, kVideoBufferSize);

						temp_packets.push_back(std::move(temp));

						bytes_temp -= kVideoBufferSize;
						packet_temp += kVideoBufferSize;
					}

					if (bytes_temp > 0) {
						std::vector<uint8_t> cache_temp(bytes_temp);
						memcpy(cache_temp.data(), packet_temp, cache_temp.size());
						std::swap(_cache_buffer, cache_temp);
					}
				}
				else if (bytes + _cache_buffer.size() == kVideoBufferSize) {
					std::vector<uint8_t> temp = std::vector<uint8_t>(kVideoBufferSize);
					memcpy(temp.data(), _cache_buffer.data(), _cache_buffer.size());
					memcpy(temp.data() + _cache_buffer.size(), packet, bytes);

					temp_packets.push_back(std::move(temp));

					_cache_buffer = std::vector<uint8_t>();
				}
				else {
					std::vector<uint8_t> cache_temp(_cache_buffer.size() + bytes);
					memcpy(cache_temp.data(), _cache_buffer.data(), _cache_buffer.size());
					memcpy(cache_temp.data() + _cache_buffer.size(), packet, bytes);

					std::swap(_cache_buffer, cache_temp);
				}
			}
			else {
				if (bytes > kVideoBufferSize) {
					std::vector<uint8_t> temp = std::vector<uint8_t>(kVideoBufferSize);
					memcpy(temp.data(), packet, kVideoBufferSize);

					int bytes_temp = bytes;
					uint8_t* packet_temp = ((uint8_t*)packet);

					for (; bytes_temp >= kVideoBufferSize;) {
						temp = std::vector<uint8_t>(kVideoBufferSize);
						memcpy(temp.data(), packet_temp, kVideoBufferSize);

						temp_packets.push_back(std::move(temp));

						packet_temp += kVideoBufferSize;
						bytes_temp -= kVideoBufferSize;
					}

					if (bytes_temp > 0) {
						std::vector<uint8_t> cache_temp(bytes_temp);
						memcpy(cache_temp.data(), packet_temp, cache_temp.size());
						std::swap(_cache_buffer, cache_temp);
					}
				}
				else if (bytes == kVideoBufferSize) {
					std::vector<uint8_t> temp = std::vector<uint8_t>(kVideoBufferSize);
					memcpy(temp.data(), packet, kVideoBufferSize);

					temp_packets.push_back(std::move(temp));
				}
				else {
					_cache_buffer = std::vector<uint8_t>(bytes);
					memcpy(_cache_buffer.data(), packet, _cache_buffer.size());
				}
			}
			if (!temp_packets.empty()) {
				for (auto& temp : temp_packets) {
					std::lock_guard<std::mutex> lk(_video_buffer_mtx);
					_video_buffer.push(std::move(temp));
					_video_buffer_cv.notify_one();
				}
			}
		}

		void DemuxerRtc::calcRemainingTime(AVPacket* packet, std::chrono::microseconds& remaining_time)
		{
			auto pts = packet->pts;
			auto dts = packet->dts;
			auto duration = packet->duration;

			AVRational  dst_time_base = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(packet->pts, _video_stream->time_base, dst_time_base);
			//int64_t pts_time = pts * av_q2d(_OutStream->time_base) * AV_TIME_BASE - _StartPts;
			//int64_t now_time = av_gettime_relative() - start_time;
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
	}
}

