#include "EapsDecoder.h"
//#include "EapsMetaDataStructure.h"
#include "jo_meta_data_structure.h"
#include "EapsMacros.h"
#include "Logger.h"
#include <list>
#include <stdexcept>
#include <system_error>

namespace eap {
	namespace sma {
		AVPixelFormat Decoder::_HwPixFmt{};

		DecoderPtr Decoder::createInstance()
		{
			return DecoderPtr(new Decoder());
		}

		Decoder::Decoder()
		{

		}

		Decoder::~Decoder()
		{
			close();
		}

		void Decoder::setFrameCallback(FrameCallback frame_callback)
		{
			_frame_callback = frame_callback;
		}

		void Decoder::open(AVCodecParameters codecpar, AVRational framerate, const std::string url, bool is_hardware)
		{
			_decoder_par = codecpar;
			_url = url;
			_is_hardware = is_hardware;

			int ret{};

			AVHWDeviceType type{};
			if (_is_hardware) {
				//查找是否支持cuda硬件编码加速器
				type = av_hwdevice_find_type_by_name("cuda");
				if (type == AV_HWDEVICE_TYPE_NONE) {
					_error_description = "Device type cuda is not supported.";
					eap_error(_error_description);
					eap_error("Available device types:");
					while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
						eap_information_printf(" %s", av_hwdevice_get_type_name(type));

					throw std::invalid_argument(_error_description);
				}

				if (AVCodecID::AV_CODEC_ID_H264 == _decoder_par.codec_id) {
					_decode_name = "h264_cuvid";
				}
				else if (AVCodecID::AV_CODEC_ID_HEVC == _decoder_par.codec_id) {
					_decode_name = "hevc_cuvid";
				}
				else {
					_error_description = "codec is not support";
					eap_error(_error_description);
					throw std::invalid_argument(_error_description);
				}
#ifdef ENABLE_AIRBORNE
				_decode_name = "h264_nvmpi";
#endif
			}
#ifdef ENABLE_3588
			const AVCodec* codec;
#else
			AVCodec* codec;
#endif
			if(!_decode_name.empty())
				codec = avcodec_find_decoder_by_name(_decode_name.c_str());
			else
				codec = avcodec_find_decoder(_decoder_par.codec_id);
			if (!codec) {
				_error_description = "Can not find decoder, codec_id: " + std::to_string((int)_decoder_par.codec_id);
				eap_error(_error_description);
				throw std::invalid_argument(_error_description);
			}

			//查找是否有解码器支持cuda加速
			if (_is_hardware) {
#ifndef ENABLE_AIRBORNE
				for (int i = 0; ; i++) {
					const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
					if (!config) {
						_error_description = std::string("Decoder ") + std::string(codec->name)
							+ std::string(" does not support device type ") + std::string(av_hwdevice_get_type_name(type));
						throw std::invalid_argument(_error_description);
					}
					if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
						config->device_type == type) {
						_HwPixFmt = config->pix_fmt;
						break;
					}
				}
#endif
			}

			if (!(_decoder_ctx = avcodec_alloc_context3(codec))) {
				throw std::bad_alloc();
			}
#ifndef ENABLE_AIRBORNE
			ret = avcodec_parameters_to_context(_decoder_ctx, &_decoder_par);
			if (ret < 0) {
				_error_description = "Parameters to context failed. error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}
#else
			_decoder_ctx->time_base = av_inv_q(framerate);
			_decoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
			_decoder_ctx->width = _decoder_par.width;
			_decoder_ctx->height = _decoder_par.height;
			_decoder_ctx->gop_size = framerate.num /** 5*/;
			auto bit_rate = 5000000;
			_decoder_ctx->bit_rate = bit_rate;
			_decoder_ctx->rc_min_rate = bit_rate;//恒定比特率
			_decoder_ctx->rc_max_rate = bit_rate;
			_decoder_ctx->bit_rate_tolerance = bit_rate;
			_decoder_ctx->rc_buffer_size = bit_rate;
			_decoder_ctx->rc_initial_buffer_occupancy =
			_decoder_ctx->rc_buffer_size * 3 / 4;
			_decoder_ctx->qmin = 1;
			_decoder_ctx->qmax = 35;
			_decoder_ctx->max_b_frames = 0;
#endif

			if (_is_hardware) {
#ifndef ENABLE_AIRBORNE
				_decoder_ctx->get_format = _GetHwFormat;
				//初始化硬件解码器上下文
				int err = 0;
				if ((err = _HwDecoderInit(_decoder_ctx, type)) < 0) {
					_error_description = "Failed to create specified HW device.";
					eap_error(_error_description);
					throw std::system_error(err, std::system_category(), _error_description);
				}
#endif
			}

			AVDictionary* dict = nullptr;
			av_dict_set(&dict, "threads", "auto", 0);
			av_dict_set(&dict, "zerolatency", "1", 0);
			av_dict_set(&dict, "strict", "-2", 0);

			if (codec->capabilities & AV_CODEC_CAP_TRUNCATED) {
				/* we do not send complete frames */
				_decoder_ctx->flags |= AV_CODEC_FLAG_TRUNCATED;
			}

			ret = avcodec_open2(_decoder_ctx, codec, &dict);
			if (ret < 0) {
				_error_description = "could not open codec, error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(ret, std::system_category(), _error_description);
			}

			eap_information_printf("created decoder, codec id: %d", (int)_decoder_par.codec_id);

			_decode_thread = std::thread([this]() {
				_decode_run = true;
				// TODO: 同步到播放器和SDK
				std::list<Packet> meta_data_waiting{};
				eap_information("decode thread started");
				for (; _decode_run;) {
					Packet pkt{};

					std::unique_lock<std::mutex> lock(_packets_mutex);
					if (_packets.empty()) {
						_packets_ready = false;
						_packets_cv.wait(lock, [this] {
							return _packets_ready || !_decode_run;
						});
					}

					if (!_decode_run) { break; }

					pkt = _packets.front();
					_packets.pop();
					lock.unlock();

					int ret = avcodec_send_packet(_decoder_ctx, pkt);
					if (ret < 0) {
						eap_error_printf("Error sending a packet for decoding, error: %s", AVError2String(ret));
						continue;
					}
					meta_data_waiting.push_back(pkt);

					while (ret >= 0) {
						AVFrame* frame = av_frame_alloc();
						ret = avcodec_receive_frame(_decoder_ctx, frame);
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
							av_frame_free(&frame);
							break;
						}else if (ret < 0) {
							av_frame_free(&frame);
							eap_error_printf("Error during decoding, error: %s",
								AVError2String(ret).c_str());
							break;
						}

						Packet min_pts_packet;
						if (!meta_data_waiting.empty()) {
							auto ite = meta_data_waiting.begin();
							min_pts_packet = *ite;
							int64_t min_pts_redus = (*ite)->pts;

							// 遍历队列，查找pts差值最小的
							for (auto it = meta_data_waiting.begin(); it != meta_data_waiting.end(); ++it) {
								auto pts_redus = std::abs(frame->pts - (*it)->pts);
								if (pts_redus < min_pts_redus) {
									ite = it;
									min_pts_redus = pts_redus;
								}
								else if (0 == pts_redus) {
									ite = it;
									break;
								}
							}
							// 如果找到了，则把它之前的都删掉
							if (ite != meta_data_waiting.begin()) {
								min_pts_packet = *ite;
								meta_data_waiting.erase(meta_data_waiting.begin(), ite);
							}
						}

						if (frame) {
							Frame vas_frame(frame);
							if (_frame_callback) {
								vas_frame.setMetaDataBasic(min_pts_packet.getMetaDataBasic());
								vas_frame.setMetaDataValid(min_pts_packet.metaDataValid());
								vas_frame.setSeiBuf(min_pts_packet.getSeiBuf());
								vas_frame.setCurrentTime(min_pts_packet.getCurrentTime());
								vas_frame.setOriginalPts(min_pts_packet.getOriginalPts());

								_frame_callback(vas_frame);
							}
							else {
								av_frame_unref(frame);
								frame = nullptr;
							}
						}
					}
				}
				eap_information_printf("decode thread exited, url: %s", _url);// TODO: URL
			});
		}

		void Decoder::close()
		{
			_decode_run = false;
			if (_decode_thread.joinable()) {
				_packets_cv.notify_all();
				_decode_thread.join();
			}
			
			if (_decoder_ctx) {
				avcodec_free_context(&_decoder_ctx);
				_decoder_ctx = nullptr;
			}
			if (nullptr != _HwDeviceCtx) {
				av_buffer_unref(&_HwDeviceCtx);
				_HwDeviceCtx = nullptr;
				eap_error("free _HwDeviceCtx");
			}
			clearFrameQueue();
					
			eap_error_printf("decoder closed, url: %s", _url);
		}

		//void Decoder::flushDecoder()
		//{
		//	_is_flush_decoder.store(true);
		//}

		void Decoder::pushPacket(Packet& pkt)
		{
			std::lock_guard<std::mutex> lock(_packets_mutex);
			while (_packets.size() > 50) {
				_packets.pop();
			}
			_packets.push(pkt);
			_packets_ready = true;
			_packets_cv.notify_all();
		}

        bool Decoder::isDecode()
        {
            return _decode_run;
        }

		void Decoder::clearFrameQueue()
		{
			while (!_packets.empty())
			{
				_packets.pop();
			}
		}

        AVFrame* Decoder::convertExportFrame(AVFrame* frame)
		{
			AVFrame* frame_exp = av_frame_alloc();
			frame_exp->width = frame->width;
			frame_exp->height = frame->height;
			frame_exp->format = AV_PIX_FMT_BGR24;
			av_frame_get_buffer(frame_exp, 1);

			_sws_ctx = sws_getCachedContext(_sws_ctx,
				frame->width, frame->height, (AVPixelFormat)frame->format,
				frame_exp->width, frame_exp->height, (AVPixelFormat)frame_exp->format,
				0, nullptr, nullptr, nullptr);

			if (_sws_ctx) {
				sws_scale(_sws_ctx, frame->data, frame->linesize,
					0, frame->height, frame_exp->data, frame_exp->linesize);

				return frame_exp;
			}
			else {
				av_frame_free(&frame_exp);
				return nullptr;
			}
		}
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
		cv::Mat Decoder::avframe2cvmat(AVFrame *avframe)
		{
			_sws_ctx = sws_getCachedContext(_sws_ctx, avframe->width, avframe->height, (enum AVPixelFormat)avframe->format,
				avframe->width, avframe->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

			cv::Mat mat;
			mat.create(cv::Size(avframe->width, avframe->height), CV_8UC3);
			AVFrame *bgr24frame = av_frame_alloc();
			//bgr24frame->data[0] = (uint8_t *)mat.data;
			av_image_fill_arrays(bgr24frame->data, bgr24frame->linesize, mat.data, AV_PIX_FMT_BGR24, avframe->width, avframe->height, 1);
			//avpicture_fill((AVPicture *)bgr24frame, bgr24frame->data[0], AV_PIX_FMT_BGR24, avframe->width, avframe->height);
			sws_scale(_sws_ctx,
				(const uint8_t* const*)avframe->data, avframe->linesize,
				0, avframe->height, // from cols=0,all rows trans
				bgr24frame->data, bgr24frame->linesize);

			av_free(bgr24frame);

			return mat;
		}
#endif
		int Decoder::_HwDecoderInit(AVCodecContext * ctx, const AVHWDeviceType type)
		{
			int err = 0;

			if ((err = av_hwdevice_ctx_create(&_HwDeviceCtx, type,
				NULL, NULL, 0)) < 0) {
				fprintf(stderr, "Failed to create specified HW device.\n");
				return err;
			}
			ctx->hw_device_ctx = av_buffer_ref(_HwDeviceCtx);

			return err;
		}

		AVPixelFormat Decoder::_GetHwFormat(AVCodecContext * ctx, const AVPixelFormat *pix_fmts)
		{
			const enum AVPixelFormat *p;

			for (p = pix_fmts; *p != -1; p++) {
				if (*p == _HwPixFmt)
					return *p;
			}

			eap_error("Failed to get HW surface format.");

			return AV_PIX_FMT_NONE;
		}
	}
}

