#include "EapsEncoderNvenc.h"
#ifndef ENABLE_AIRBORNE
#ifdef ENABLE_GPU
#include "EapsImageCvtColorCuda.h"
#endif
#endif
#include "EapsMetaDataProcessing.h"

#include "Logger.h"

#include <stdexcept>
#include <system_error>

namespace eap {
	namespace sma {
		EncoderNVENC::EncoderNVENC(InitParameter& init_parameter,
			EncodedPacketCallback encoded_data_callback)
		{
			_width = init_parameter.dst_width;
			_height = init_parameter.dst_height;
			_bitrate = init_parameter.bit_rate;
			_framerate = init_parameter.framerate;
			_timebase = init_parameter.time_base;
			_Keyint = init_parameter.keyint;
			_Profile = init_parameter.profile;
			_encoded_data_callback = encoded_data_callback;
			_start_time = init_parameter.start_time;

			_initialize();
		}

		EncoderNVENCPtr EncoderNVENC::createInstance(InitParameter & init_parameter, EncodedPacketCallback encoded_data_callback)
		{
			return EncoderNVENCPtr(new EncoderNVENC(init_parameter, encoded_data_callback));
		}

		EncoderNVENC::~EncoderNVENC()
		{
			if (_encode_thread.joinable()) {
				_encode_run = false;
				_wait_for_encode_frames_cv.notify_all();
				_encode_thread.join();
			}

			_wait_for_encode_frames_mutex.lock();
			for (; !_wait_for_encode_frames.empty();) {
				_wait_for_encode_frames.pop();
			}
			_wait_for_encode_frames_mutex.unlock();

			if (_encoder_context) {
				avcodec_free_context(&_encoder_context);
			}
			// TODO:
			 if (_hw_device_context) {
			 	av_buffer_unref(&_hw_device_context);
				_hw_device_context = nullptr;
				eap_error("free _hw_device_context");
			 }

			eap_error("close encoder");
		}
// static FILE *h264_file = NULL;
		void EncoderNVENC::start()
		{
			_encode_thread = std::thread([this]() {
				int ret;
				_encode_run = true;
				eap_information("encode thread started");
				std::list<MetaDataWrap> meta_data_waiting{};

				for (; _encode_run;) {
					std::unique_lock<std::mutex> lock(_wait_for_encode_frames_mutex);
					if (_wait_for_encode_frames.empty()) {
						_wait_for_encode_frames_cv.wait(lock);
					}
					if (!_encode_run) {
						continue;
					}
					if (_wait_for_encode_frames.empty()) {
						continue;
					}

					auto frame = _wait_for_encode_frames.front();
					_wait_for_encode_frames.pop();

					lock.unlock();

					MetaDataWrap meta_data{};
					meta_data.pts = frame->pts;
					meta_data.meta_data_raw_binary = frame.getSeiBuf();
					meta_data.meta_data_valid = frame.getMetaDataValid();
					meta_data.meta_data_basic = frame.getMetaDataBasic();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
					meta_data.pixel_points = frame.getArPixelPoints();
					meta_data.pixel_lines = frame.getArPixelLines();
					meta_data.pixel_warning_l1_regions = frame.getArPixelWarningL1s();
					meta_data.pixel_warning_l2_regions = frame.getArPixelWarningL2s();
#endif
					meta_data.ar_mark_info = frame.getArInfos();
					meta_data.ai_heatmap_info = frame.getAiHeatmapInfos();
					meta_data.ar_valid_point_index = frame.getArValidPointIndex();
					meta_data.ar_vector_file = frame.getArVectorFile();

					meta_data_waiting.push_back(meta_data);

					if ((ret = avcodec_send_frame(_encoder_context, frame)) < 0) {
						eap_error_printf("Error during encoding, error code: %s", AVError2String(ret));
						continue;
					}

					for (; _encode_run;) {
						AVPacket* packet = av_packet_alloc();
						av_init_packet(packet);
						packet->data = nullptr;
						packet->size = 0;

						ret = avcodec_receive_packet(_encoder_context, packet);
						if (ret != 0)
							break;

						packet->stream_index = 0;

						if (_encoded_data_callback) {
							MetaDataWrap min_pts_meta_data;

							if (!meta_data_waiting.empty()) {
								auto ite = meta_data_waiting.begin();
								min_pts_meta_data = *ite;
								int64_t min_pts_redus = ite->pts;
								for (auto it = meta_data_waiting.begin(); it != meta_data_waiting.end(); ++it) {
									auto pts_redus = std::abs(packet->pts - it->pts);
									if (pts_redus < min_pts_redus) {
										ite = it;
										min_pts_redus = pts_redus;
									}
									else if (0 == pts_redus) {
										ite = it;
										break;
									}
								}
								if (ite != meta_data_waiting.begin()) {
									min_pts_meta_data = *ite;
									meta_data_waiting.erase(meta_data_waiting.begin(), ite);
								}
							}

							Packet packet_export(packet);
							packet_export.setMetaDataBasic(&min_pts_meta_data.meta_data_basic);
							packet_export.setSeiBuf(min_pts_meta_data.meta_data_raw_binary);
							packet_export.metaDataValid() = min_pts_meta_data.meta_data_valid;
							packet_export.setArMarkInfos(min_pts_meta_data.ar_mark_info);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
							packet_export.setArPixelPoints(min_pts_meta_data.pixel_points);
							packet_export.setArPixelLines(min_pts_meta_data.pixel_lines);
							packet_export.setArPixelWarningL1s(min_pts_meta_data.pixel_warning_l1_regions);
							packet_export.setArPixelWarningL2s(min_pts_meta_data.pixel_warning_l2_regions);
#endif
							packet_export.setArValidPointIndex(min_pts_meta_data.ar_valid_point_index);
							packet_export.setArVectorFile(min_pts_meta_data.ar_vector_file);
							packet_export.setAiHeatmapInfos(min_pts_meta_data.ai_heatmap_info);

							packet_export->pts = av_rescale_q(packet->pts, av_inv_q(_framerate), _timebase);

							AVRational  dst_time_base = { 1, AV_TIME_BASE };
							int64_t _start_time_stamp = av_rescale_q(_start_time, dst_time_base, _timebase);
							int64_t current_time = (packet_export->pts - _start_time_stamp) * av_q2d(_timebase) * 1000.f;
							packet_export.setCurrentTime(current_time);

							_encoded_data_callback(packet_export);

						}
					}
				}
				eap_information("Encoding thread exited");
			});
		}

		void EncoderNVENC::stop()
		{
			if (_encode_thread.joinable()) {
				_encode_run = false;
				_wait_for_encode_frames_cv.notify_all();
				_encode_thread.join();
			}
		}

		void EncoderNVENC::updateFrame(Frame frame)
		{
			AVFrame* hw_frame = av_frame_alloc();
			int ret = av_hwframe_get_buffer(_encoder_context->hw_frames_ctx, hw_frame, 0);
			if (ret != 0) {
				av_frame_free(&hw_frame);
				eap_error_printf("av_hwframe_get_buffer failed, error: %s", AVError2String(ret));
				return;
			}
			ret = av_hwframe_transfer_data(hw_frame, frame, 0);
			if (ret != 0) {
				av_frame_free(&hw_frame);
				eap_error_printf("av_hwframe_transfer_data failed, error: %s", AVError2String(ret));
				return;
			}

			hw_frame->pts = av_rescale_q(frame->pts, _timebase, av_inv_q(_framerate));

			Frame vas_frame_hw(hw_frame);

			vas_frame_hw.setMetaDataBasic(frame.getMetaDataBasic());
			auto sei_buffer = frame.getSeiBuf();
			vas_frame_hw.setSeiBuf(sei_buffer);
			vas_frame_hw.setMetaDataValid(frame.getMetaDataValid());
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			vas_frame_hw.setArPixelPoints(frame.getArPixelPoints());
			vas_frame_hw.setArPixelLines(frame.getArPixelLines());
#endif
			std::lock_guard<std::mutex> lock(_wait_for_encode_frames_mutex);

			_wait_for_encode_frames.push(vas_frame_hw);
			_wait_for_encode_frames_cv.notify_all();
		}

		void EncoderNVENC::updateFrameGpu(Frame frame)
		{
			// TODO:
		}

		void EncoderNVENC::updateFrame(CodecImage& frame)
		{
#ifdef ENABLE_AIRBORNE
			AVFrame* hw_frame = av_frame_alloc();
			hw_frame=EncoderNVENC::_yuvToAvFrame(frame.bgr24_image, frame.bgr24_image.cols, frame.bgr24_image.rows * 2 / 3);
#else
			AVFrame* hw_frame = av_frame_alloc();
			int ret = av_hwframe_get_buffer(_encoder_context->hw_frames_ctx, hw_frame, 0);
			if (ret != 0) {
				av_frame_free(&hw_frame);
				eap_error_printf("av_hwframe_get_buffer failed, error: %s", AVError2String(ret));
				return;
			}
#endif//ENABLE_AIRBORNE

#ifndef ENABLE_AIRBORNE
#ifdef ENABLE_GPU
			ImageCvtColor::Instance()->bgr32MatToFrameCopy(frame.bgr32_image, hw_frame);
			frame.bgr24_image.release();
			frame.bgr32_image.release();
#endif
#endif
			hw_frame->pts = av_rescale_q(frame.meta_data.pts, _timebase, av_inv_q(_framerate));

			Frame vas_frame_hw(hw_frame);

			vas_frame_hw.setMetaDataBasic(frame.meta_data.meta_data_basic);
			vas_frame_hw.setSeiBuf(frame.meta_data.meta_data_raw_binary);
			vas_frame_hw.setMetaDataValid(frame.meta_data.meta_data_valid);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			vas_frame_hw.setArPixelPoints(frame.meta_data.pixel_points);
			vas_frame_hw.setArPixelLines(frame.meta_data.pixel_lines);
			vas_frame_hw.setArPixelWarningL1s(frame.meta_data.pixel_warning_l1_regions);
			vas_frame_hw.setArPixelWarningL2s(frame.meta_data.pixel_warning_l2_regions);
#endif
			vas_frame_hw.setArMarkInfos(frame.meta_data.ar_mark_info);
			vas_frame_hw.setArValidPointIndex(frame.meta_data.ar_valid_point_index);
			vas_frame_hw.setArVectorFile(frame.meta_data.ar_vector_file);
			vas_frame_hw.setAiHeatmapInfos(frame.meta_data.ai_heatmap_info);
			
			std::lock_guard<std::mutex> lock(_wait_for_encode_frames_mutex);
			_wait_for_encode_frames.push(vas_frame_hw);
			_wait_for_encode_frames_cv.notify_all();
		}

		void EncoderNVENC::_initialize()
		{
			_initializeEncoderOptions();
			_initializeEncoder();
		}

		void EncoderNVENC::_initializeEncoderOptions()
		{
			#ifndef ENABLE_AIRBORNE
			av_dict_set(&_encoder_options, "profile", "main", 0);
			av_dict_set(&_encoder_options, "rc", "cbr", 0);
			av_dict_set(&_encoder_options, "cbr", "true", 0);
			#else
			av_dict_set(&_encoder_options,
              "preset",
              "ultrafast",
              0); // 设置预设为 ultrafast
			av_dict_set(&_encoder_options,
						"num_capture_buffers",
						"5",
						0); // 设置捕获缓冲区数量为 5
			av_dict_set(&_encoder_options, "profile", "main", 0);
			av_dict_set(&_encoder_options, "rc", "cbr", 0);
			av_dict_set(&_encoder_options, "cbr", "true", 0); // 设置编码模式为恒定比特率
			#endif
		}

		void EncoderNVENC::_initializeEncoder()
		{
			int ret;
#ifdef ENABLE_3588
			const AVCodec* enc_codec{};
#else
			AVCodec* enc_codec{};
#endif

#ifndef ENABLE_AIRBORNE
			_encoder_name = "h264_nvenc";
#else
			_encoder_name = "h264_nvmpi";
#endif

#ifndef ENABLE_AIRBORNE

			if (!(enc_codec = avcodec_find_encoder_by_name(_encoder_name.c_str()))) {
				_error_description = "Could not find encoder h264_nvenc";
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			if (!(_encoder_context = avcodec_alloc_context3(enc_codec))) {
				_error_description = "Failed to alloc encoder context";
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			ret = av_hwdevice_ctx_create(&_hw_device_context, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
			if (ret < 0) {
				_error_description = "Failed to alloc encoder context, error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			//    _encoder_context->hw_device_ctx = av_buffer_ref(_hw_device_context);
			//    if (!_encoder_context->hw_device_ctx) {
			//        JAV_THROW_EXCEPTION_FROM_ERRORCODE("A hardware frame ctx reference create failed.", ErrorCode::AVCodecError);
			//    }

			AVBufferRef *hw_frames_ref = av_hwframe_ctx_alloc(_hw_device_context);
			if (!hw_frames_ref) {
				_error_description = "Failed to alloc CUDA frame context";
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			AVHWFramesContext *frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
			frames_ctx->width = _width;
			frames_ctx->height = _height;
			frames_ctx->format = AV_PIX_FMT_CUDA;
			frames_ctx->sw_format = AV_PIX_FMT_RGB0;
			frames_ctx->initial_pool_size = 10;

			if ((ret = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
				av_buffer_unref(&hw_frames_ref);

				_error_description = "Failed to initialize CUDA frame context, error: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			_encoder_context->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
			if (!_encoder_context->hw_frames_ctx) {
				_error_description = "A hardware frame ctx reference create failed";
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			av_buffer_unref(&hw_frames_ref);

			/* set AVCodecContext Parameters for encoder, here we keep them stay
			 * the same as decoder.
			 * xxx: now the sample can't handle resolution change case.
			 */
			_encoder_context->time_base = av_inv_q(_framerate);
			_encoder_context->pix_fmt = AV_PIX_FMT_CUDA;
			_encoder_context->width = _width;
			_encoder_context->height = _height;
			_encoder_context->gop_size = _framerate.num /** 5*/;
			_encoder_context->bit_rate = _bitrate;
			_encoder_context->rc_min_rate = _bitrate;
			_encoder_context->rc_max_rate = _bitrate;
			_encoder_context->bit_rate_tolerance = _bitrate;
			_encoder_context->rc_buffer_size = _bitrate;
			_encoder_context->rc_initial_buffer_occupancy = _encoder_context->rc_buffer_size * 3 / 4;
			_encoder_context->qmin = 1;
			_encoder_context->qmax = 51;
			_encoder_context->max_b_frames = 0;

			if ((ret = avcodec_open2(_encoder_context, enc_codec, &_encoder_options)) != 0) {
				_error_description = "Failed to open encode codec, Error code: " +
					std::to_string(ret) + ", desc: " + AVError2String(ret);
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			eap_information("nvenc encoder created");
#else
			if (!(enc_codec = avcodec_find_encoder_by_name(_encoder_name.c_str()))){
				_error_description = "Could not find encoder h264_nvmpi";
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			if (!(_encoder_context = avcodec_alloc_context3(enc_codec))){
				_error_description = "Could not allocate h264_nvmpi video codec context ";
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

			_encoder_context->time_base = av_inv_q(_framerate);
			_encoder_context->pix_fmt = AV_PIX_FMT_YUV420P;
			_encoder_context->width = _width;
			_encoder_context->height = _height;
			_encoder_context->gop_size = _framerate.num /** 5*/;
			_encoder_context->bit_rate = _bitrate;
			_encoder_context->rc_min_rate = _bitrate;//恒定比特率
			_encoder_context->rc_max_rate = _bitrate;
			_encoder_context->bit_rate_tolerance = _bitrate;
			_encoder_context->rc_buffer_size = _bitrate;
			_encoder_context->rc_initial_buffer_occupancy =
				_encoder_context->rc_buffer_size * 3 / 4;
			_encoder_context->qmin = 1;
			_encoder_context->qmax = 35;
			_encoder_context->max_b_frames = 0;
			_encoder_context->flags|= AV_CODEC_FLAG_LOW_DELAY;

			if ((ret = avcodec_open2(_encoder_context, enc_codec, &_encoder_options)) !=0){
				_error_description = "Failed to open h264_nvmpi encode codec ";
				eap_error(_error_description);
				throw std::system_error(std::make_error_code(std::errc::function_not_supported), _error_description);
			}

#endif

		}
#ifdef ENABLE_AIRBORNE
AVFrame *EncoderNVENC::_yuvToAvFrame(const cv::Mat &yuvMat, int width,
                                   int height)
{
  // 创建 AVFrame
  AVFrame *av_frame = av_frame_alloc();
  if (!av_frame)
  {
    // 处理分配失败的情况
    return nullptr;
  }

  av_frame->width = width;
  av_frame->height = height;
  av_frame->format = AV_PIX_FMT_YUV420P;

  int ret = av_frame_get_buffer(av_frame, 0);
  if (ret < 0)
  {

    av_frame_free(&av_frame);
    return nullptr;
  }

  ret = av_frame_make_writable(av_frame);
  if (ret < 0)
  {
    av_frame_free(&av_frame);
    return nullptr;
  }

  int frame_size = width * height;

  // 把起始地址赋值av_frame->data[i]
  cv::Mat Y = cv::Mat(cv::Size(av_frame->linesize[0], height), CV_8UC1,
                      av_frame->data[0]);
  cv::Mat U = cv::Mat(cv::Size(av_frame->linesize[1], height / 2), CV_8UC1,
                      av_frame->data[1]);
  cv::Mat V = cv::Mat(cv::Size(av_frame->linesize[2], height / 2), CV_8UC1,
                      av_frame->data[2]);

  cv::Mat y = cv::Mat(cv::Size(width, height), CV_8UC1, yuvMat.data);
  cv::Mat u = cv::Mat(cv::Size(width / 2, height / 2), CV_8UC1,
                      yuvMat.data + width * height);
  cv::Mat v = cv::Mat(cv::Size(width / 2, height / 2), CV_8UC1,
                      yuvMat.data + width * height * 5 / 4);

  y.copyTo(Y(cv::Rect(0, 0, width, height)));
  // u.copyTo(U(cv::Rect((av_frame->linesize[1] - width / 2) / 2, 0, width / 2,
  //                     height / 2)));
  // v.copyTo(V(cv::Rect((av_frame->linesize[2] - width / 2) / 2, 0, width / 2,
  //                     height / 2)));
  u.copyTo(U(cv::Rect(0, 0, width / 2, height / 2)));
  v.copyTo(V(cv::Rect(0, 0, width / 2, height / 2)));
  // cv::imwrite("u.jpg", U);
  // cv::imwrite("v.jpg", V);

  return av_frame;
}
#endif
    }
}
