#include "EapsMuxer.h"

#include "Logger.h"

#include <stdexcept>
#include <system_error>

namespace eap {
	namespace sma {
		MuxerPtr Muxer::createInstance()
		{
			return MuxerPtr(new Muxer());
		}

		Muxer::Muxer()
		{

		}

		Muxer::~Muxer()
		{
			close();
		}

		void Muxer::open(std::string file_name, AVRational timebase, AVCodecParameters codecpar, MuxedDataType muxed_data_type)
		{
			_file_name = file_name;
			_muxed_data_type = muxed_data_type;
			_in_timebase = timebase;
			_video_codec_par = codecpar;

			// TODO: 删除
			_muxed_data_type = MuxedDataType::MuxedDataType_TS;

			int ret{ };

			_format_name = "mpegts";
			if ((ret = avformat_alloc_output_context2(
				&_out_fmt_ctx, nullptr, "mpegts", _file_name.c_str())) < 0) {
				_error_description = "alloc output foramt "
					" context failed, error description: " + AVError2String(ret) +
					", out format name: " + "mpegts, " + "file name: " + _file_name;
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			_video_stream = avformat_new_stream(_out_fmt_ctx, nullptr);
			if (!_video_stream) {
				_error_description = "avforamt new stream failed";
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			_video_stream->id = _out_fmt_ctx->nb_streams - 1;
			_video_stream->time_base = timebase;
			_video_stream->id = 0;
			_video_stream->avg_frame_rate = { 30, 1 };

			ret = avcodec_parameters_copy(_video_stream->codecpar, &_video_codec_par);
			if (ret < 0) {
				_error_description = "avcodec_parameters_copy codec_par fail";
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}
			//_video_stream->codecpar->codec_type = _video_codec_par.codec_type;
			//_video_stream->codecpar->codec_id = _video_codec_par.codec_id;
			//_video_stream->codecpar->codec_tag = _video_codec_par.codec_tag;
			//_video_stream->codecpar->width = _video_codec_par.width;
			//_video_stream->codecpar->height = _video_codec_par.height;
			//_video_stream->codecpar->bit_rate = _video_codec_par.bit_rate;
			//_video_stream->codecpar->sample_aspect_ratio = { 0, 1 };

			if (_muxed_data_type == MuxedDataType::MuxedDataType_FMV_TS) {
				_klv_stream = avformat_new_stream(_out_fmt_ctx, nullptr);
				_klv_stream->codecpar->codec_type = AVMEDIA_TYPE_DATA;
				_klv_stream->codecpar->codec_id = AV_CODEC_ID_SMPTE_KLV;
				_klv_stream->codecpar->codec_tag = 1096174667;
				_klv_stream->time_base = AVRational{ 50, 1 };
				_klv_stream->id = _out_fmt_ctx->nb_streams - 1;
			}

			if (!(_out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
				ret = avio_open(&_out_fmt_ctx->pb, _file_name.c_str(), AVIO_FLAG_WRITE);
				if (ret < 0) {
					if (_out_fmt_ctx) {
						avformat_free_context(_out_fmt_ctx);
						_out_fmt_ctx = nullptr;
					}
					_error_description = "avio open failed, error description: " + AVError2String(ret);
					eap_error(_error_description);
					throw std::system_error(ret, std::system_category(), _error_description);
				}
				_is_call_aviopen = true;
			}

			if ((ret = avformat_write_header(_out_fmt_ctx, nullptr)) < 0) {
				_error_description = "write header failed, error description: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			_is_opened = true;

			eap_information_printf("create muxer, format name: %s, file name: %s", _format_name, file_name);

			_push_thread = std::thread([this, file_name]() {
				_push_run = true;

				bool first_packet{ true };
				int64_t start_pts = 0;
				int64_t last_pts = 0;
				int64_t current_ts = 0;

				eap_information("start muxer thread");

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

					if (_packets.empty()) {
						continue;
					}

					pkt = _packets.front();
					_packets.pop();
					lock.unlock();

					if (first_packet) {
						start_pts = pkt->pts;
						_start_write_time = std::chrono::system_clock::now();
						first_packet = false;
					}

					pkt->pts -= start_pts;
					pkt->dts = pkt->pts;

					try {
						// 由于时间戳可能距离变化，导致计算的等待时间并不准确
						std::chrono::microseconds remaining_time = std::chrono::microseconds(0);
						calcRemainingTime(pkt, remaining_time);
						if (remaining_time > std::chrono::microseconds(0)) {
							std::this_thread::sleep_for(remaining_time);
						}
						writePacket(pkt);
					}
					catch (std::system_error& e) {
						break;
					}
				}
				eap_error_printf("muxer thread exited, file name: %s", file_name);
			});
		}

		void Muxer::close()
		{
			_is_opened = false;
			if (_push_thread.joinable()) {
				_push_run = false;
				_packets_cv.notify_all();
				_push_thread.join();
			}

			if (_out_fmt_ctx)
				av_write_trailer(_out_fmt_ctx);
			if (_out_fmt_ctx && !(_out_fmt_ctx->flags & AVFMT_NOFILE) && _is_call_aviopen) {
				avio_closep(&_out_fmt_ctx->pb);
			}

			avformat_free_context(_out_fmt_ctx);
			_out_fmt_ctx = nullptr;

			if (_out_buf) {
				delete[] _out_buf;
				_out_buf = nullptr;
			}

			eap_error("close muxer");
		}

		void Muxer::pushPacket(Packet pkt)
		{
			std::lock_guard<std::mutex> lock(_packets_mutex);
			if (_packets.size() > 100) {
				eap_information("muxer already have 100 packets! pop");
				_packets.pop();
			}
			_packets.push(pkt);
			_packets_ready = true;
			_packets_cv.notify_all();
		}

		void Muxer::writePacket(Packet pkt)
		{
			pkt->pts = av_rescale_q_rnd(pkt->pts, _in_timebase, _video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->dts = av_rescale_q_rnd(pkt->dts, _in_timebase, _video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->duration = av_rescale_q(pkt->duration, _in_timebase, _video_stream->time_base);
			pkt->pos = -1;
			pkt->stream_index = 0;
			// 当时间戳不合适时，累加的帧间时间戳
			//static int64_t interval_pts = 1 / av_q2d(_video_stream->avg_frame_rate) / av_q2d(_video_stream->time_base);
			if(pkt->pts <= _last_pts)
			{
				pkt->pts = pkt->dts = _last_pts + 1;
			}
			_last_pts=pkt->pts;
			int ret = av_interleaved_write_frame(_out_fmt_ctx, pkt);
			if (ret < 0) {
				if (ret == AVERROR(EINVAL)) {
					_error_description = "write packet failed, file name: " + _file_name + ", error description: " + AVError2String(ret);
					eap_error(_error_description);
				}
				else {
					_error_description = "write packet failed,  file name: " + _file_name + ", error description: " + AVError2String(ret);
					eap_error(_error_description);
					throw std::system_error(ret, std::system_category(), _error_description);
				}
			}
		}
		void Muxer::calcRemainingTime(Packet packet, std::chrono::microseconds& remaining_time)
		{
			auto pts = packet->pts;
			auto dts = packet->dts;
			auto duration = packet->duration;

			AVRational  dst_time_base = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(packet->pts, _in_timebase, dst_time_base);
			int64_t now_time = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now() - _start_write_time).count();
			auto remaining_time_t = pts_time - now_time;
			if ((remaining_time_t > 0) &&
				(remaining_time_t < AV_TIME_BASE * 10)) {
				remaining_time = std::chrono::microseconds(remaining_time_t);
			}
		}
	}
}
