#include "EapsDispatchTaskImplMultiple.h"
#include "EapsMacros.h"
#include "EapsConfig.h"
#ifdef ENABLE_AR
#include "jo_ar_engine_interface.h"
#endif
#ifdef ENABLE_AIRBORNE
#include "img_conv.h"
#else
#ifdef ENABLE_GPU
#include "EapsImageCvtColorCuda.h"
#endif // ENABLE_GPU
#endif // ENABLE_AIRBORNE

#include "Logger.h"
#include "OnceToken.h"
#include "EapsNoticeCenter.h"
#include "EapsMetaDataBasic.h"
#include "HttpClient.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/ThreadPool.h"
#include "Poco/StreamCopier.h"

#include <string>
#include <future>
#include <algorithm>
#include <chrono>
#include <cmath>
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#include <opencv2/opencv.hpp>
#endif

static const std::string video_mark_metadata_file_directory = "video_mark_meta_data/";

namespace eap {
	namespace sma {
		DispatchTaskImplMultiplePtr DispatchTaskImplMultiple::createInstance(InitParameter init_parameter)
		{
			return DispatchTaskImplMultiplePtr(new DispatchTaskImplMultiple(init_parameter));
		}

		DispatchTaskImplMultiple::DispatchTaskImplMultiple(InitParameter init_parameter)
			: DispatchTask(init_parameter)
		{

			for(int i = 0; i <init_parameter._pull_urls.size(); i++){
				if (init_parameter._pull_urls[i].find("webrtc://") == 0 || init_parameter._pull_urls[i].find("webrtcs://") == 0) {
					if(i == 0 )
						_is_pull_rtc = true;
					if(i == 1)
						_is_pull_sd_rtc = true;
				}
				if (init_parameter._push_urls[i].find("webrtc://") == 0 || init_parameter._push_urls[i].find("webrtcs://") == 0) {
					if(i == 0 )
						_is_push_rtc = true;
					if(i == 1 )
						_is_push_sd_rtc = true;
				}
			}
			_pull_urls = init_parameter._pull_urls;
			_push_urls = init_parameter._push_urls;
			_record_file_prefix = init_parameter.record_file_prefix;
			_record_time_str = init_parameter.record_time_str;
			_ar_vector_file = init_parameter.ar_vector_file;
			_ar_settings_file = init_parameter.ar_camera_config;
			_task_id = init_parameter.id;
			_function_mask = init_parameter.func_mask;

			update_func_mask_l();

			_meta_data_processing_pret = MetaDataProcessing::createInstance();
			_meta_data_processing_postproc = MetaDataProcessing::createInstance();
			_meta_data_processing_pret_sd = MetaDataProcessing::createInstance();
			_meta_data_processing_postproc_sd = MetaDataProcessing::createInstance();
#ifdef ENABLE_GPU
			_is_hardware_decode = true;
#endif
		}

		DispatchTaskImplMultiple::~DispatchTaskImplMultiple()
		{
			stop();
		}

		void DispatchTaskImplMultiple::start()
		{
			#ifdef ENABLE_AR
			hdDangerLoopThread();
			#endif
			_hd_thread = std::thread(&DispatchTaskImplMultiple::hdThread, this);
			std::this_thread::sleep_for(std::chrono::milliseconds{50});
			#ifdef ENABLE_AR
			sdDangerLoopThread();
			#endif
			_sd_thread = std::thread(&DispatchTaskImplMultiple::sdThread, this);
		}

		void DispatchTaskImplMultiple::stop()
		{
			_is_manual_stoped = true;
			_danger_photo_loop_thread_run.store(false);
			_danger_photo_loop_thread_run_sd.store(false);
			if (_danger_photo_loop_thread.joinable()) {
				_danger_queue_cv.notify_all();
				_danger_photo_loop_thread.join();
				eap_information("end hd _danger_photo_loop_thread join ++++");
			}
			if (_danger_photo_loop_thread_sd.joinable()) {
				_danger_queue_cv_sd.notify_all();
				_danger_photo_loop_thread_sd.join();
				eap_information("end sd _danger_photo_loop_thread join ++++");
			}
			eap_information( "start multi task stop imterior ++++");
			if (_loop_thread.joinable()) {
				eap_information( "start _loop_thread joinable ++++");
				_loop_run = false;
				_decoded_images_cv.notify_all();
				_loop_thread.join();
				eap_information( "end _loop_thread join ++++");
			}
			if (_loop_sd_thread.joinable()) {
				eap_information( "start _loop_sd_thread joinable ++++");
				_loop_sd_run = false;
				_decoded_sd_images_cv.notify_all();
				_loop_sd_thread.join();
				eap_information( "end _loop_sd_thread join ++++");
			}
			if(_hd_thread.joinable()){
				_hd_thread.join();
				eap_information( "end _hd_thread join ++++");
			}
			if(_sd_thread.joinable()){
				_sd_thread.join();
				eap_information( "end _sd_thread join ++++");
			}

			if (_pusher_rtc) {
				eap_information( "start hd_pusher_rtc destroy ++++");
				_pusher_rtc.reset();
				eap_information( "end hd _pusher_rtc destroy ++++");
			}
			if (_pusher_tradition) {
				eap_information( "start hd _pusher_tradition destroy ++++");
				_pusher_tradition.reset();
				eap_information( "end hd _pusher_tradition destroy ++++");
			}
			if (_pusher_sd_rtc) {
				eap_information( "start _sd_pusher_rtc destroy ++++");
				_pusher_sd_rtc.reset();
				eap_information( "end _sd _pusher_rtc destroy ++++");
			}
			if (_pusher_sd_tradition) {
				eap_information( "start _sd _pusher_tradition destroy ++++");
				_pusher_sd_tradition.reset();
				eap_information( "end _sd _pusher_tradition destroy ++++");
			}

			if (_demuxer_rtc) {
				eap_information( "start hd_demuxer_rtc destroy ++++");
				_demuxer_rtc.reset();
				eap_information( "end hd _demuxer_rtc destroy ++++");
			}
			if (_demuxer_tradition){
				eap_information( "start hd _demuxer_tradition destroy ++++");
				_demuxer_tradition.reset();
				eap_information( "end hd _demuxer_tradition destroy ++++");
			}
			if (_demuxer_sd_rtc) {
				eap_information( "start _sd_demuxer_rtc destroy ++++");
				_demuxer_sd_rtc.reset();
				eap_information( "end _sd _demuxer_rtc destroy ++++");
			}
			if (_demuxer_sd_tradition) {
				eap_information( "start _sd _demuxer_tradition destroy ++++");
				_demuxer_sd_tradition.reset();
				eap_information( "end _sd _demuxer_tradition destroy ++++");
			}

	#ifdef ENABLE_GPU
			if (_decoder) {
				eap_information( "start hd _decoder destroy ++++");
				_decoder.reset();
				while (!_decoded_images.empty())
				{
					auto image = _decoded_images.front();
					_decoded_images.pop();
				}
				eap_information( "end hd _decoder destroy ++++");
			}
			if (_decoder_sd) {
				eap_information( "start sd _decoder destroy ++++");
				_decoder_sd.reset();
				while (!_decoded_sd_images.empty())
				{
					auto image = _decoded_sd_images.front();
					_decoded_sd_images.pop();
				}
				eap_information( "end sd _decoder destroy ++++");
			}
			_encoder_mutex.lock();
			if (_encoder) {
				eap_information( "start hd _encoder stop ++++");
				_encoder->stop();
				_encoder.reset();
				eap_information( "end hd _encoder stop ++++");
			}
			_encoder_mutex.unlock();
			_encoder_sd_mutex.lock();
			if (_encoder_sd) {
				eap_information( "start _sd _encoder stop ++++");
				_encoder_sd->stop();
				_encoder_sd.reset();
				eap_information( "end _sd _encoder stop ++++");
			}
			_encoder_sd_mutex.unlock();
	#endif // ENABLE_GPU

			_meta_data_cache.clear();
			_meta_data_cache_sd.clear();
			if (_meta_data_processing_pret) {
				eap_information( "start _meta_data_processing_pret reset ++++");
				_meta_data_processing_pret.reset();
				eap_information( "end _meta_data_processing_pret reset ++++");
			}
			if (_meta_data_processing_postproc) {
				_meta_data_processing_postproc.reset();
			}
			if (_meta_data_processing_pret_sd) {
				eap_information( "start _meta_data_processing_pret_sd reset ++++");
				_meta_data_processing_pret_sd.reset();
				eap_information( "end _meta_data_processing_pret_sd reset ++++");
			}
			if (_meta_data_processing_postproc_sd) {
				_meta_data_processing_postproc_sd.reset();
			}

			eap_information( "start destroyAIEngine ++++");
			destroyAIEngine();
			destroyAIEngine(false);
			eap_information( "end destroyAIEngine ++++");
			destroyAREngine();
			destroyAREngine(false);
			eap_information( "end destroyAREngine ++++");
			destroyARMarkEngine();
			destroyARMarkEngine(false);
			eap_information( "end destroyARMarkEngine ++++");
	#ifdef ENABLE_GPU
	#ifndef ENABLE_AIRBORNE
			destroyImageEnhancer();
			destroyImageEnhancer(false);
			eap_information( "end destroyImageEnhancer ++++");
			destroyImageStabilizer();
			destroyImageStabilizer(false);
			eap_information( "end destroyImageStabilizer ++++");
	#endif
	#endif // ENABLE_GPU

			_video_mark_frame_count = 0;
			for(int i = 0;i < _pull_urls.size();i++){
				eap_information_printf("dispatch task stoped, pull url: %s, push url: %s",_pull_urls[i], _push_urls[i]);
			}
		}

		void DispatchTaskImplMultiple::updateFuncMask(int new_func_mask, std::string ar_camera, std::string ar_kml)
		{
			std::lock_guard<std::mutex> update_func_lock(_update_func_mutex);
			_function_mask = new_func_mask;
			if (!ar_kml.empty()) {
#ifdef ENABLE_AR
				//更新kml文件，要先销毁原来创建的AREngine
				if(_ar_engine) {
					destroyAREngine();
				}
				if(_ar_engine_sd) {
					destroyAREngine(false);
				}
#endif
				_ar_vector_file = ar_kml;
			}
			if (!ar_camera.empty()) {
				_ar_settings_file = ar_camera;
			}
			_is_update_func = true;
			_is_update_func_sd = true;
			update_func_mask_l();
		}

		void DispatchTaskImplMultiple::clipSnapShotParam(int time_count)
		{
			// TODO
		}

		void DispatchTaskImplMultiple::setSeekPercent(float percent)
		{
			if (!_demuxer_tradition) {//目前只有http和文件作为视频源时才支持跳转
				std::string desc = "hd_demuxer_tradition is null";
				throw std::runtime_error(desc);			
			}
			_demuxer_tradition->seek(percent);
			if (!_demuxer_sd_tradition) {//目前只有http和文件作为视频源时才支持跳转
				std::string desc = "_sd_demuxer_tradition is null";
				throw std::runtime_error(desc);			
			}
			_demuxer_sd_tradition->seek(percent);
		}

		void DispatchTaskImplMultiple::pause(int paused)
		{
			if (!_demuxer_tradition) {//目前只有http和文件作为视频源时才支持暂停
				std::string desc = "hd_demuxer_tradition is null";
				throw std::runtime_error(desc);
			}
			_demuxer_tradition->pause(paused);
			if(_pusher_rtc){
				_pusher_rtc->pause(paused);
			}
			
			if(_pusher_tradition){
				_pusher_tradition->pause(paused);
			}

			if (!_demuxer_sd_tradition) {//目前只有http和文件作为视频源时才支持暂停
				std::string desc = "_sd_demuxer_tradition is null";
				throw std::runtime_error(desc);
			}
			_demuxer_sd_tradition->pause(paused);
		}

		void DispatchTaskImplMultiple::updateArLevelDistance(int level_one_distance, int level_two_distance)
		{
#ifdef ENABLE_AR
			_ar_level_one_distance = level_one_distance;
			_ar_level_two_distance = level_two_distance;
			if (_ar_engine && _ar_level_one_distance > 0 && _ar_level_two_distance > 0) {
				_ar_engine->setWarningArea(_ar_level_one_distance, _ar_level_two_distance);
			}
#endif
		}

		void DispatchTaskImplMultiple::addAnnotationElements(std::string ar_camera_path, std::string annotation_elements_json, bool isHd) 
		{
#ifdef ENABLE_AR
			//每次有新的camera.config过来，都进行更新
			if(isHd){
				if((_ar_settings_file != ar_camera_path) || (!_ar_mark_engine)){
					_ar_settings_file = ar_camera_path;
					_is_update_ar_mark_engine.store(true);
				}
			}
			if(!isHd){
				if((_ar_settings_file != ar_camera_path) || (!_ar_mark_engine_sd)){
					_ar_settings_file = ar_camera_path;
					_is_update_ar_mark_engine_sd.store(true);
				}
			}
			_video_mark_data.push(annotation_elements_json);
			_video_mark_data_status.push(isHd);
			_video_mark_data_sd.push(annotation_elements_json);
			_video_mark_data_sd_status.push(isHd);
#endif
		}

		void DispatchTaskImplMultiple::deleteAnnotationElements(std::string mark_guid, bool isHd)
		{
			_mark_guid = mark_guid;
			_mark_guid_sd = mark_guid;
		}

		std::vector<int64_t> DispatchTaskImplMultiple::getVideoDuration()
		{
			std::vector<int64_t> durations;
			if (_is_pull_rtc) {
				if (_demuxer_rtc) {
					durations.push_back(_demuxer_rtc->videoDuration());
				}	
			}
			else {
				if (_demuxer_tradition) {
					durations.push_back(_demuxer_tradition->videoDuration());
				}
			}
			if (_is_pull_sd_rtc) {
				if (_demuxer_sd_rtc) {
					durations.push_back(_demuxer_sd_rtc->videoDuration());
				}	
			}
			else {
				if (_demuxer_sd_tradition) {
					durations.push_back(_demuxer_sd_tradition->videoDuration());
				}
			}
			return durations;
		}

		void DispatchTaskImplMultiple::updateArTowerHeight(bool is_tower, double tower_height, bool buffer_sync_height)
		{
			//TODO
		}

		void DispatchTaskImplMultiple::updateAiCorPos(int ai_pos_cor)
		{
			//TODO
		}

#ifdef ENABLE_AIRBORNE
		void DispatchTaskImplMultiple::receivePilotData(std::string param_str)
		{
		}
		void DispatchTaskImplMultiple::receivePayloadData(std::string param_str)
		{
		}
#endif
		void receiveVersinDataMulti(std::string version_data)
		{
			// TODO
		}

        std::string DispatchTaskImplMultiple::aiAssistTrack(int track_cmd, int track_pixelpos_x, int track_pixelpos_y)
        {
            return std::string();
        }

        void DispatchTaskImplMultiple::saveSnapShot()
        {
        }

        void DispatchTaskImplMultiple::createAIEngine()
		{
#ifdef ENABLE_GPU 
#ifdef ENABLE_AI
			if (_is_ai_on) {
				if (!_ai_object_detector && (_is_ai_first_create || _is_update_func)) {
					std::string engine_file_full_name{};
					int width{};
					int height{};
					int class_num{};
					double conf_thresh{};
					double nums_thresh{};
					std::string yolo_version{};
					std::string text_encoder_feature{};
					
					try {
						GET_CONFIG(std::string, getString, my_engine_file_full_name, AI::kEngineFileFullName);
						GET_CONFIG(int, getInt, my_width, AI::kModelWidth);
						GET_CONFIG(int, getInt, my_height, AI::kModelHeight);
						GET_CONFIG(int, getInt, my_class_num, AI::kClassNumber);
						GET_CONFIG(double, getDouble, my_conf_thresh, AI::kConfThresh);
						GET_CONFIG(double, getDouble, my_nums_thresh, AI::kNmsThresh);
						GET_CONFIG(std::string, getString, my_yolo_version, AI::kYoloVersion);
						GET_CONFIG(std::string, getString,my_text_encoder_feature, AI::kTextEncoderFeature);
						engine_file_full_name = my_engine_file_full_name;
						width = my_width;
						height = my_height;
						class_num = my_class_num;
						conf_thresh = my_conf_thresh;
						nums_thresh = my_nums_thresh;
						yolo_version = my_yolo_version;
						text_encoder_feature = my_text_encoder_feature;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					_ai_object_detector = joai::ObjectDetectPtr(new joai::ObjectDetect());

					
					auto status = _ai_object_detector->initNetwork(engine_file_full_name,
						width, height, class_num, conf_thresh, nums_thresh, yolo_version);
					
					if (status != joai::ENGINE_SUCCESS) {
						if(!_is_update_func){
							std::string desc = "AI engine init failed, status code: " + std::to_string(status);
							eap_error(desc);
						}
						eap_error("AI func update failed");
						_update_func_err_desc += std::string("AI; ");
						_update_func_result = false;
						_function_mask -= FUNCTION_MASK_AI;//如果是关闭功能，关闭失败的话应该+，但是目前认为不会关闭失败
						_ai_object_detector.reset();
					}
					eap_information( "hd AI engine create success");
				}

				if (!_ai_mot_tracker && (_is_ai_first_create || _is_update_func)) {
					bool track_switch{};
					int track_buff_len{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
						GET_CONFIG(int, getInt, my_track_buff_len, AI::kTrackBufferLength);
						track_switch = my_track_switch;
						track_buff_len = my_track_buff_len;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					if (track_switch) {
						_ai_mot_tracker = joai::MotTrackPtr(new joai::MotTrack(30, track_buff_len, false));
						if (!_ai_mot_tracker) {
							if (!_is_update_func) {
								std::string desc = "AI track create failed";
								throw std::runtime_error(desc);
							}
							
							eap_error( "AI track func update failed");
							_update_func_err_desc += std::string("AI_Track; ");
							_update_func_result = false;
							_ai_mot_tracker.reset();
						}
					}
				}
			}
#endif
#endif // ENABLE_GPU
		}

		void DispatchTaskImplMultiple::createAIEngineSd()
		{
#ifdef ENABLE_GPU
	#ifdef ENABLE_AI
			if (_is_ai_on_sd) {
				if (!_ai_object_detector_sd && (_is_ai_first_create_sd || _is_update_func_sd)) {
					std::string engine_file_full_name{};
					int width{};
					int height{};
					int class_num{};
					float conf_thresh{};
					float nums_thresh{};
					std::string yolo_version{};
					std::string text_encoder_feature{};
					try {
						GET_CONFIG(std::string, getString, my_engine_file_full_name, AI::kEngineFileFullNameAux);
						GET_CONFIG(int, getInt, my_width, AI::kModelWidthAux);
						GET_CONFIG(int, getInt, my_height, AI::kModelHeightAux);
						GET_CONFIG(int, getDouble, my_class_num, AI::kClassNumberAux);
						GET_CONFIG(float, getDouble, my_conf_thresh, AI::kConfThreshAux);
						GET_CONFIG(float, getDouble, my_nums_thresh, AI::kNmsThreshAux);
						GET_CONFIG(std::string, getString, my_yolo_version, AI::kYoloVersionAux);
						GET_CONFIG(std::string, getString,my_text_encoder_feature, AI::kTextEncoderFeatureAux);
						engine_file_full_name = my_engine_file_full_name;
						width = my_width;
						height = my_height;
						class_num = my_class_num;
						conf_thresh = my_conf_thresh;
						nums_thresh = my_nums_thresh;
						yolo_version = my_yolo_version;
						text_encoder_feature = my_text_encoder_feature;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					_ai_object_detector_sd = joai::ObjectDetectPtr(new joai::ObjectDetect());

					
					auto status = _ai_object_detector_sd->initNetwork(engine_file_full_name,
						width, height, class_num, conf_thresh, nums_thresh, yolo_version);
					
					if (status != joai::ENGINE_SUCCESS) {
						if(!_is_update_func_sd){
							std::string desc = "sd AI engine init failed, status code: " + std::to_string(status);
							eap_error(desc);
						}
						eap_error( "sd AI func update failed");
						_update_func_err_desc += std::string("AI; ");
						_update_func_result = false;
						_function_mask -= FUNCTION_MASK_AI;//如果是关闭功能，关闭失败的话应该+，但是目前认为不会关闭失败
						_ai_object_detector_sd.reset();
					}
					eap_information( "sd AI engine create success");
				}

				if (!_ai_mot_tracker_sd && (_is_ai_first_create_sd || _is_update_func_sd)) {
					bool track_switch{};
					int track_buff_len{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
						GET_CONFIG(int, getInt, my_track_buff_len, AI::kTrackBufferLength);
						track_switch = my_track_switch;
						track_buff_len = my_track_buff_len;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					if (track_switch) {
						_ai_mot_tracker_sd = joai::MotTrackPtr(new joai::MotTrack(30, track_buff_len, false));
						if (!_ai_mot_tracker_sd) {
							if (!_is_update_func_sd) {
								std::string desc = "sd AI track create failed";
								throw std::runtime_error(desc);
							}
							
							eap_error( "sd AI track func update failed");
							_update_func_err_desc += std::string("sd AI_Track; ");
							_update_func_result = false;
							_ai_mot_tracker_sd.reset();
						}
					}
				}
			}
	#endif
	#endif // ENABLE_GPU
		}

		void DispatchTaskImplMultiple::createAuxiliaryAIEngine()
		{
			#ifdef ENABLE_GPU
	#ifdef ENABLE_AI
			if (_is_enhanced_ar_on) {
				if (!_aux_ai_object_detector && (_is_aux_ai_first_create || _is_update_func)) {

					std::string engine_file_full_name{};
					int width{};
					int height{};
					int class_num{};
					float conf_thresh{};
					float nums_thresh{};
					std::string yolo_version{};
					std::string text_encoder_feature{};
					try {
						GET_CONFIG(std::string, getString, my_engine_file_full_name, AI::kEngineFileFullNameAux);
						GET_CONFIG(int, getInt, my_width, AI::kModelWidthAux);
						GET_CONFIG(int, getInt, my_height, AI::kModelHeightAux);
						GET_CONFIG(int, getDouble, my_class_num, AI::kClassNumberAux);
						GET_CONFIG(float, getDouble, my_conf_thresh, AI::kConfThreshAux);
						GET_CONFIG(float, getDouble, my_nums_thresh, AI::kNmsThreshAux);
						GET_CONFIG(std::string, getString, my_yolo_version, AI::kYoloVersionAux);
						GET_CONFIG(std::string, getString,my_text_encoder_feature, AI::kTextEncoderFeatureAux);
						engine_file_full_name = my_engine_file_full_name;
						width = my_width;
						height = my_height;
						class_num = my_class_num;
						conf_thresh = my_conf_thresh;
						nums_thresh = my_nums_thresh;
						yolo_version = my_yolo_version;
						text_encoder_feature = my_text_encoder_feature;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					_aux_ai_object_detector = joai::ObjectDetectPtr(new joai::ObjectDetect());

					
					auto status = _aux_ai_object_detector->initNetwork(engine_file_full_name,
						width, height, class_num, conf_thresh, nums_thresh, yolo_version);
					
					if (status != joai::ENGINE_SUCCESS) {
						if(!_is_update_func){
							std::string desc = "Auxiliary AI engine init failed, status code: " + std::to_string(status);
							//throw std::runtime_error(desc);
							eap_error(desc);
						}
						eap_error( "Auxiliary AI func update failed");
						_update_func_err_desc += std::string("Auxiliary AI; ");
						_update_func_result = false;
						_function_mask -= FUNCTION_MASK_ENHANCED_AR;//如果是关闭功能，关闭失败的话应该+，但是目前认为不会关闭失败
						_is_enhanced_ar_on = false;
						_aux_ai_object_detector.reset();
					}
					eap_information("----------------Auxiliary AI engine init success!");
				}

				if (!_aux_ai_mot_tracker && (_is_aux_ai_first_create || _is_update_func)) {

					bool track_switch{};
					int track_buff_len{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitchAux);
						GET_CONFIG(int, getInt, my_track_buff_len, AI::kTrackBufferLengthAux);
						track_switch = my_track_switch;
						track_buff_len = my_track_buff_len;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					if (track_switch) {
						_aux_ai_mot_tracker = joai::MotTrackPtr(new joai::MotTrack(30, track_buff_len, true));
						if (!_aux_ai_mot_tracker) {
							if (!_is_update_func) {
								std::string desc = "Auxiliary AI track create failed";
								//throw std::runtime_error(desc);
								eap_error(desc);
							}
							
							eap_error( "Auxiliary AI track func update failed");
							_update_func_err_desc += std::string("Auxiliary AI_Track; ");
							_update_func_result = false;
							_aux_ai_mot_tracker.reset();
						}
					}
					eap_information("--------------------Auxiliary AI track init success!");
				}
			}
	#endif
	#endif // ENABLE_GPU
		}

		void DispatchTaskImplMultiple::createAuxiliaryAIEngineSd()
		{
	#ifdef ENABLE_GPU
#ifdef ENABLE_AI
			if (_is_enhanced_ar_on_sd) {
				if (!_aux_ai_object_detector_sd && (_is_aux_ai_first_create_sd || _is_update_func_sd)) {
					std::string engine_file_full_name{};
					int width{};
					int height{};
					int class_num{};
					float conf_thresh{};
					float nums_thresh{};
					std::string yolo_version{};
					std::string text_encoder_feature{};
					try {
						GET_CONFIG(std::string, getString, my_engine_file_full_name, AI::kEngineFileFullNameAux);
						GET_CONFIG(int, getInt, my_width, AI::kModelWidthAux);
						GET_CONFIG(int, getInt, my_height, AI::kModelHeightAux);
						GET_CONFIG(int, getDouble, my_class_num, AI::kClassNumberAux);
						GET_CONFIG(float, getDouble, my_conf_thresh, AI::kConfThreshAux);
						GET_CONFIG(float, getDouble, my_nums_thresh, AI::kNmsThreshAux);
						GET_CONFIG(std::string, getString, my_yolo_version, AI::kYoloVersionAux);
						GET_CONFIG(std::string, getString,my_text_encoder_feature, AI::kTextEncoderFeatureAux);
						engine_file_full_name = my_engine_file_full_name;
						width = my_width;
						height = my_height;
						class_num = my_class_num;
						conf_thresh = my_conf_thresh;
						nums_thresh = my_nums_thresh;
						yolo_version = my_yolo_version;
						text_encoder_feature = my_text_encoder_feature;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					_aux_ai_object_detector_sd = joai::ObjectDetectPtr(new joai::ObjectDetect());

					
					auto status = _aux_ai_object_detector_sd->initNetwork(engine_file_full_name,
						width, height, class_num, conf_thresh, nums_thresh, yolo_version);
					
					if (status != joai::ENGINE_SUCCESS) {
						if(!_is_update_func_sd){
							std::string desc = "sd Auxiliary AI engine init failed, status code: " + std::to_string(status);
							//throw std::runtime_error(desc);
							eap_error( desc);
						}
						eap_error( "sd Auxiliary AI func update failed");
						_update_func_err_desc_sd += std::string("Auxiliary AI; ");
						_update_func_result_sd = false;
						_function_mask -= FUNCTION_MASK_ENHANCED_AR;//如果是关闭功能，关闭失败的话应该+，但是目前认为不会关闭失败
						_is_enhanced_ar_on_sd = false;
						_aux_ai_object_detector_sd.reset();
					}
					eap_information("----------------Auxiliary AI engine init success!");
				}

				if (!_aux_ai_mot_tracker_sd && (_is_aux_ai_first_create_sd || _is_update_func_sd)) {
					bool track_switch{};
					int track_buff_len{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitchAux);
						GET_CONFIG(int, getInt, my_track_buff_len, AI::kTrackBufferLengthAux);
						track_switch = my_track_switch;
						track_buff_len = my_track_buff_len;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					if (track_switch) {
						_aux_ai_mot_tracker_sd = joai::MotTrackPtr(new joai::MotTrack(30, track_buff_len, true));
						if (!_aux_ai_mot_tracker_sd) {
							if (!_is_update_func_sd) {
								std::string desc = "sd Auxiliary AI track create failed";
								//throw std::runtime_error(desc);
								eap_error( desc);
							}
							
							eap_error( "sd Auxiliary AI track func update failed");
							_update_func_err_desc_sd += std::string("Auxiliary AI_Track; ");
							_update_func_result_sd = false;
							_aux_ai_mot_tracker_sd.reset();
						}
					}
					eap_information("--------------------sd Auxiliary AI track init success!");
				}
			}
#endif
#endif // ENABLE_GPU
		}

		void DispatchTaskImplMultiple::createAREngine()
		{
#ifdef ENABLE_AR
			if (!_ar_engine && (_is_ar_first_create || _is_update_func) && !_ar_settings_file.empty()) {
				// TODO: 初始化失败情况
				try {
					_ar_engine = joar::create2dArEngine(_ar_vector_file, _ar_settings_file);
					eap_information("createAREngine success ");
				} catch(const std::exception& e) {
					std::string exception_desc = std::string("_ar_engine create fail, exception description: ") + e.what();
					eap_error( exception_desc);
				}
			}   
			if ((_is_ar_on || _is_enhanced_ar_on) && _ar_engine) {
				if (!_ar_engine->start(true)) {
					if(!_is_update_func){
						std::string desc = "AR engine init failed";
						throw std::runtime_error(desc);
					}
					_update_func_err_desc += std::string("AR; ");
					_update_func_result = false;
					_function_mask -= FUNCTION_MASK_AR;
					_ar_engine.reset();
				}
			}     
			if (_ar_engine) {
				_ar_engine->setWarningArea(_ar_level_one_distance, _ar_level_two_distance);
			}
#endif
		}

		void DispatchTaskImplMultiple::createAREngineSd()
		{
#ifdef ENABLE_AR
			if (!_ar_engine_sd && (_is_ar_first_create_sd || _is_update_func_sd) && !_ar_settings_file.empty()) {
				// TODO: 初始化失败情况
				try {
					_ar_engine_sd = joar::create2dArEngine(_ar_vector_file, _ar_settings_file);
					eap_information("sd createAREngine success");
				} catch(const std::exception& e) {
					std::string exception_desc = std::string("sd _ar_engine create fail, exception description: ") + e.what();
					eap_error( exception_desc);
				}
			}   
			if ((_is_ar_on_sd || _is_enhanced_ar_on_sd) && _ar_engine_sd) {
				if (!_ar_engine_sd->start(true)) {
					if(!_is_update_func_sd){
						std::string desc = "sd AR engine init failed";
						throw std::runtime_error(desc);
					}
					_update_func_err_desc_sd += std::string("AR; ");
					_update_func_result_sd = false;
					_function_mask -= FUNCTION_MASK_AR;
					_ar_engine_sd.reset();
				}
			}     
			if (_ar_engine_sd) {
				_ar_engine_sd->setWarningArea(_ar_level_one_distance, _ar_level_two_distance);
			} 
#endif
		}

		void DispatchTaskImplMultiple::createARMarkEngine()
		{
#ifdef ENABLE_AR
			try {
				if (!_ar_mark_engine) {
					_ar_mark_engine = jomarker::createMarkerEngine(_ar_settings_file);
				}
			} catch(const std::exception& e) {
				//标注引擎初始化失败，不能影响其它工作
				std::string exception_desc = std::string("_ar_mark_engine create fail, exception description: ") + e.what();
				eap_error( exception_desc);
				return;
			}
			eap_information( "_ar_mark_engine create success");
#endif
		}

		void DispatchTaskImplMultiple::createARMarkEngineSd()
		{
#ifdef ENABLE_AR
			try {
				if (!_ar_mark_engine_sd) {
					_ar_mark_engine_sd = jomarker::createMarkerEngine(_ar_settings_file);
				}
			} catch(const std::exception& e) {
				//标注引擎初始化失败，不能影响其它工作
				std::string exception_desc = std::string("sd _ar_mark_engine create fail, exception description: ") + e.what();
				eap_error( exception_desc);
				return;
			}
			eap_information( "sd _ar_mark_engine create success");
#endif
		}

	#ifdef ENABLE_GPU
	#ifndef ENABLE_AIRBORNE
		void DispatchTaskImplMultiple::createImageEnhancer()
		{
			if (_is_image_enhancer_on) {
				if (!_enhancer && (_is_image_enhancer_first_create || _is_update_func)) {
					_enhancer = Enhancer::CreateDefogEnhancer(
						_codec_parameter.width, _codec_parameter.height);
					if (!_enhancer) {
						if (!_is_update_func) {
							std::string desc = "hd Enhancer create failed";
							throw std::runtime_error(desc);
						}
						_update_func_err_desc += std::string("hd Enhancer; ");
						eap_error( "hd Enhance func update failed");
						_update_func_result = false;
						_function_mask -= FUNCTION_MASK_DEFOG;
					}
					
				}
			}
		}

		void DispatchTaskImplMultiple::createSdImageEnhancer()
		{
			if (_is_sd_image_enhancer_on) {
				if (!_enhancer_sd && (_is_image_enhancer_first_create || _is_update_func_sd)) {
					_enhancer_sd = Enhancer::CreateDefogEnhancer(
						_codec_parameter_sd.width, _codec_parameter_sd.height);
					if (!_enhancer_sd) {
						if (!_is_update_func_sd) {
							std::string desc = "sd Enhancer create failed";
							throw std::runtime_error(desc);
						}
						_update_func_err_desc += std::string("sd Enhancer; ");
						eap_error( "sd Enhance func update failed");
						_update_func_result = false;
						_function_mask -= FUNCTION_MASK_DEFOG;
					}
					
				}
			}
		}

		void DispatchTaskImplMultiple::createImageStabilizer()
		{
			if (_is_image_stable_on) {
				if (!_stabilizer && (_is_image_stable_first_create || _is_update_func)) {
					/*_stabilizer = VideoStabilizer::createTX1Stabilizer(
						_codec_parameters[i].width, _codec_parameters[i].height);
					if (!_stabilizer) {
						if (!_is_update_func) {
							std::string desc = "Stabilizer run failed";
							throw std::runtime_error(desc);
						}
						eap_error( "stabilize func update failed";
						_update_func_err_desc += std::string("Stabilizer; ");
						_update_func_result = false;
						_function_mask -= FUNCTION_MASK_STABLE;
					} */
				}
			}
			_is_image_stable_on = false;
		}
		void DispatchTaskImplMultiple::createSdImageStabilizer()
		{
			if (_is_image_stable_on_sd) {
				if (!_stabilizer_sd && (_is_sd_image_stable_first_create || _is_update_func_sd)) {
					/*_stabilizer_sd = VideoStabilizer::createTX1Stabilizer(
						_codec_parameter_sd.width, _codec_parameter_sd.height);
					if (!_stabilizer_sd) {
						if (!_is_update_func) {
							std::string desc = "sd Stabilizer run failed";
							throw std::runtime_error(desc);
						}
						eap_error( "sd stabilize func update failed";
						_update_func_err_desc += std::string("Stabilizer; ");
						_update_func_result = false;
						_function_mask -= FUNCTION_MASK_STABLE;
					} */
				}
			}
			_is_image_stable_on_sd = false;
		}
	#endif
	#endif // ENABLE_GPU

		void DispatchTaskImplMultiple::destroyAIEngine(bool is_hd)
		{
	#ifdef ENABLE_GPU
#ifdef ENABLE_AI
			if(is_hd){
					if (_ai_object_detector) {
						_ai_object_detector.reset();
					}

					if (_ai_mot_tracker) {
						_ai_mot_tracker.reset();
					}
			}
			if(!is_hd){
				if (_ai_object_detector_sd) {
					_ai_object_detector_sd.reset();
				}

				if (_ai_mot_tracker_sd) {
					_ai_mot_tracker_sd.reset();
				}
			}
#endif
	#endif // ENABLE_GPU
		}

		void DispatchTaskImplMultiple::destroyAuxiliaryAIEngine(bool is_hd)
		{
	#ifdef ENABLE_GPU
#ifdef ENABLE_AI
			if(is_hd){
				if (_aux_ai_object_detector) {
					_aux_ai_object_detector.reset();
				}

				if (_aux_ai_mot_tracker) {
					_aux_ai_mot_tracker.reset();
				}
			}
			if(!is_hd){
				if (_aux_ai_object_detector_sd) {
					_aux_ai_object_detector_sd.reset();
				}

				if (_aux_ai_mot_tracker_sd) {
					_aux_ai_mot_tracker_sd.reset();
				}
			}
#endif
	#endif // ENABLE_GPU
		}

		void DispatchTaskImplMultiple::destroyAREngine(bool is_hd)
		{
#ifdef ENABLE_AR
			if(is_hd){
				if (_ar_engine) {
					_ar_engine->shutDown();
					_ar_engine.reset();
				}
			}
			if(!is_hd){
				if (_ar_engine_sd) {
					_ar_engine_sd->shutDown();
					_ar_engine_sd.reset();
				}
			}
#endif
		}

		void DispatchTaskImplMultiple::destroyARMarkEngine(bool is_hd)
		{
#ifdef ENABLE_AR
			if(is_hd){
				if (_ar_mark_engine) {
					_ar_mark_engine.reset();
					eap_information( "---destroyARMarkEngine--");
				}
			}
			if(!is_hd){
				if (_ar_mark_engine_sd) {
					_ar_mark_engine_sd.reset();
					eap_information( "---destroyARMarkEngine--sd");
				}
			}
#endif
		}

	#ifdef ENABLE_GPU
	#ifndef ENABLE_AIRBORNE
		void DispatchTaskImplMultiple::destroyImageEnhancer(bool is_hd)
		{
			if(is_hd){
				if(_enhancer){
					_enhancer.reset();
				}
			}
			if(!is_hd){
				if(_enhancer_sd){
				_enhancer_sd.reset();
				}
			}
		}

		void DispatchTaskImplMultiple::destroyImageStabilizer(bool is_hd)
		{
			if(is_hd){
				if (_stabilizer) {
					_stabilizer.reset();
				}
			}
			if(!is_hd){
				if (_stabilizer_sd) {
					_stabilizer_sd.reset();
				}
			}
		}
	#endif
	#endif // ENABLE_GPU

		void DispatchTaskImplMultiple::update_func_mask_l()
		{
			if ((_function_mask & FUNCTION_MASK_AI) == FUNCTION_MASK_AI) {
				_is_ai_on = true;
				_is_ai_on_sd = true;
			} else {
				_is_ai_on = false;
				_is_ai_on_sd = false;
			}
			if ((_function_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR) {
				_is_ar_on = true;
				_is_ar_on_sd = true;
			} else {
				_is_ar_on = false;
				_is_ar_on_sd = false;
			}
			if ((_function_mask & FUNCTION_MASK_DEFOG) == FUNCTION_MASK_DEFOG) {
				_is_image_enhancer_on = true;
				_is_sd_image_enhancer_on = true;
			} else {
				_is_image_enhancer_on = false;
				_is_sd_image_enhancer_on = false;
			}
			if ((_function_mask & FUNCTION_MASK_STABLE) == FUNCTION_MASK_STABLE) {
				_is_image_stable_on = true;
				_is_image_stable_on_sd = true;
			} else {
				_is_image_stable_on = false;
				_is_image_stable_on_sd = false;
			}
			if ((_function_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
				_is_enhanced_ar_on = true;
				_is_enhanced_ar_on_sd = true;
			} else {
				_is_enhanced_ar_on = false;
				_is_enhanced_ar_on_sd = false;
			}
		}
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
		void DispatchTaskImplMultiple::trackingCoordTransform(CodecImagePtr image, cv::Mat M)
		{
			if (!image) {
				return;
			}

			cv::Mat p(3, 1, CV_32FC1);

			p.at<float>(0, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossX;
			p.at<float>(1, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossY;
			p.at<float>(2, 0) = 1;

			cv::Mat result = M * p;
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.ServoCrossX = result.at<float>(0, 0) / result.at<float>(2, 0);
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.ServoCrossY = result.at<float>(1, 0) / result.at<float>(2, 0);

			/////////////////////////////////////////////////////////////////////////////////////////////

			p.at<float>(0, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopX;
			p.at<float>(1, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopY;
			p.at<float>(2, 0) = 1;

			result = M * p;
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.TrackLeftTopX = result.at<float>(0, 0) / result.at<float>(2, 0);
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.TrackLeftTopY = result.at<float>(1, 0) / result.at<float>(2, 0);
		}

		void DispatchTaskImplMultiple::arCoordTransform(CodecImagePtr image, cv::Mat M)
		{
			if (!image) {
				return;
			}

			for (auto& point : image->meta_data.pixel_points) {
				cv::Mat p(3, 1, CV_32FC1);

				p.at<float>(0, 0) = point.x;
				p.at<float>(1, 0) = point.y;
				p.at<float>(2, 0) = 1;

				cv::Mat result = M * p;
				point.x = result.at<float>(0, 0) / result.at<float>(2, 0);
				point.y = result.at<float>(1, 0) / result.at<float>(2, 0);
			}

			for (auto& line : image->meta_data.pixel_lines) {
				for (auto& point : line) {
					cv::Mat p(3, 1, CV_32FC1);

					p.at<float>(0, 0) = point.x;
					p.at<float>(1, 0) = point.y;
					p.at<float>(2, 0) = 1;

					cv::Mat result = M * p;
					point.x = result.at<float>(0, 0) / result.at<float>(2, 0);
					point.y = result.at<float>(1, 0) / result.at<float>(2, 0);
				}
			}
		}

		void DispatchTaskImplMultiple::aiCoordTransform(CodecImagePtr image, cv::Mat M)
		{
			if (!image) {
				return;
			}

			for (int i = 0; i < image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize; ++i) {
				cv::Point point;
				point.x = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX;
				point.y = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY;

				cv::Mat p(3, 1, CV_32FC1);

				p.at<float>(0, 0) = point.x;
				p.at<float>(1, 0) = point.y;
				p.at<float>(2, 0) = 1;

				cv::Mat result = M * p;

				point.x = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX = 
					result.at<float>(0, 0) / result.at<float>(2, 0);
				point.y = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX = 
					result.at<float>(1, 0) / result.at<float>(2, 0);
			}

		}
#endif
		void DispatchTaskImplMultiple::videoMarkMetaDataWrite(ArInfosInternal &ar_infos, int64_t packet_pts)
		{
			//若由于时间戳混乱，导致可能有时间戳相同的帧，相同的帧目前默认只给一帧数据，其它没有标注数据
			_video_mark_frame_count++;
			std::string video_mark_metadata_file_name = std::to_string(packet_pts) + ".json";
			
			Poco::JSON::Object json_all{};
			Poco::JSON::Array json_elements_array{};		
			
			//送进来的没有存储该值
			// json_all["AR_Status"] = ar_infos.ArStatus;
			// json_all["ArTroubleCode"] = ar_infos.ArTroubleCode;
			json_all.set("ElementsNum", ar_infos.ArElementsNum);

			for(auto iter : ar_infos.ArElementsArray)
			{
				Poco::JSON::Object json_element_array{};
				json_element_array.set("Type", iter.Type);
				json_element_array.set("DotQuantity", iter.DotQuantity);
				json_element_array.set("X", iter.X);
				json_element_array.set("Y", iter.Y);
				json_element_array.set("lon", iter.lon);
				json_element_array.set("lat", iter.lat);
				json_element_array.set("HMSL", iter.HMSL);
				json_element_array.set("Category", iter.Category);
				json_element_array.set("CurIndex", iter.CurIndex);
				json_element_array.set("NextIndex", iter.NextIndex);
				json_elements_array.add(json_element_array);
			}
			json_all.set("ElementsArray", json_elements_array);
			std::string json_all_str = eap::sma::jsonToString(json_all);
			//不写文件，直接http发送出去
		}

		void DispatchTaskImplMultiple::videoMarkMetaDataWriteSd(ArInfosInternal &ar_infos, int64_t packet_pts)
		{
			_video_mark_frame_count_sd++;
			std::string video_mark_metadata_file_name = std::to_string(packet_pts) + ".json";
			
			Poco::JSON::Object json_all{};
			Poco::JSON::Array json_elements_array{};	
			//送进来的没有存储该值
			// json_all["AR_Status"] = ar_infos.ArStatus;
			// json_all["ArTroubleCode"] = ar_infos.ArTroubleCode;
			json_all.set("ElementsNum", ar_infos.ArElementsNum);
			for(auto iter : ar_infos.ArElementsArray)
			{
				Poco::JSON::Object json_element_array{};
				json_element_array.set("Type", iter.Type);
				json_element_array.set("DotQuantity", iter.DotQuantity);
				json_element_array.set("X", iter.X);
				json_element_array.set("Y", iter.Y);
				json_element_array.set("lon", iter.lon);
				json_element_array.set("lat", iter.lat);
				json_element_array.set("HMSL", iter.HMSL);
				json_element_array.set("Category", iter.Category);
				json_element_array.set("CurIndex", iter.CurIndex);
				json_element_array.set("NextIndex", iter.NextIndex);
				json_elements_array.add(json_element_array);
			}
			json_all.set("ElementsArray", json_elements_array);
			std::string json_all_str = eap::sma::jsonToString(json_all);
			//不写文件，直接http发送出去
		}

		void DispatchTaskImplMultiple::videoMarkMetaDataRead(bool is_hd)
		{
			//目前标注元数据写完之后把文件都删除了
			//若后续考虑后续可能需要该数据，可以不删除，增加api来进行相关文件删除
			if(is_hd)
				_video_mark_frame_count--;
			else
				_video_mark_frame_count_sd--;
		}
#ifdef ENABLE_AR
		void DispatchTaskImplMultiple::calculatGeodeticToImage(std::list<ArElementsInternal> ar_elements_array, ArInfosInternal &ar_infos, std::string guid, const int64_t &timestamp, 
			const int &img_width, const int &img_height, const jo::JoARMetaDataBasic &meta_data, 
			const ar_point& points, const ar_line_or_region& lines,const ar_line_or_region& regions)
		{
			//物点转像点，等到回调结果再继续
			if (_ar_mark_engine && _ar_mark_engine->reflectGeodeticToImage(timestamp, img_width, img_height, meta_data, points, lines, regions,
			_pixel_points, _pixel_lines, _pixel_regions)) {
				if (!_pixel_points.empty()) {
					for (std::size_t i = 0; i < _pixel_points.size(); ++i) {
						
						ArElementsInternal ar_elements_internal{};
						ar_elements_internal.X = _pixel_points[i].x;
						ar_elements_internal.Y = _pixel_points[i].y;
						ar_elements_internal.lat = points[i].latitude;
						ar_elements_internal.lon = points[i].longitude;
						ar_elements_internal.HMSL = points[i].altitude;
						ar_elements_internal.Guid = guid;

						//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
						//计算完出来后，也要跟送进去的点、线、面顺序一致
						if(!ar_elements_array.empty()){
							auto ar_elements = ar_elements_array.front();
							ar_elements_internal.Type = ar_elements.Type;
							ar_elements_internal.Category = ar_elements.Category;
							ar_elements_internal.CurIndex = ar_elements.CurIndex;
							ar_elements_internal.NextIndex = ar_elements.NextIndex;
							ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
							ar_elements_array.pop_front();
						}
						
						ar_infos.ArElementsArray.push_back(ar_elements_internal);
						ar_infos.ArElementsNum++;
					}
					_pixel_points.clear();
				}
				if (!_pixel_lines.empty()) {
					for (std::size_t i = 0; i < _pixel_lines.size(); ++i) {
						for (std::size_t j = 0; j < _pixel_lines[i].size(); ++j) {
							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = _pixel_lines[i][j].x;
							ar_elements_internal.Y = _pixel_lines[i][j].y;
							ar_elements_internal.lat = lines[i][j].latitude;
							ar_elements_internal.lon = lines[i][j].longitude;
							ar_elements_internal.HMSL = lines[i][j].altitude;
							ar_elements_internal.Guid = guid;

							//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
							//计算完出来后，也要跟送进去的点、线、面顺序一致
							if(!ar_elements_array.empty()){
								auto ar_elements = ar_elements_array.front();
								ar_elements_internal.Type = ar_elements.Type;
								ar_elements_internal.Category = ar_elements.Category;
								ar_elements_internal.CurIndex = ar_elements.CurIndex;
								ar_elements_internal.NextIndex = ar_elements.NextIndex;
								ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
								ar_elements_array.pop_front();
							}

							ar_infos.ArElementsArray.push_back(ar_elements_internal);
							ar_infos.ArElementsNum++;
						}
					}
					_pixel_lines.clear();
				}
				if (!_pixel_regions.empty()) {
					for (std::size_t i = 0; i < _pixel_regions.size(); ++i) {
						for (std::size_t j = 0; j < _pixel_regions[i].size(); ++j) {
							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = _pixel_regions[i][j].x;
							ar_elements_internal.Y = _pixel_regions[i][j].y;
							ar_elements_internal.lat = regions[i][j].latitude;//_pixel_regions中有两个vector，regions中只有一个vector，导致崩溃
							ar_elements_internal.lon = regions[i][j].longitude;
							ar_elements_internal.HMSL = regions[i][j].altitude;
							ar_elements_internal.Guid = guid;

							//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
							//计算完出来后，也要跟送进去的点、线、面顺序一致
							if(!ar_elements_array.empty()){
								auto ar_elements = ar_elements_array.front();
								ar_elements_internal.Type = ar_elements.Type;
								ar_elements_internal.Category = ar_elements.Category;
								ar_elements_internal.CurIndex = ar_elements.CurIndex;
								ar_elements_internal.NextIndex = ar_elements.NextIndex;
								ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
								ar_elements_array.pop_front();
							}

							ar_infos.ArElementsArray.push_back(ar_elements_internal);
							ar_infos.ArElementsNum++;
						}
					}
					_pixel_regions.clear();
				}
			}
		}

		void DispatchTaskImplMultiple::calculatGeodeticToImageSd(std::list<ArElementsInternal> ar_elements_array, ArInfosInternal &ar_infos, std::string guid, const int64_t &timestamp, const int &img_width, const int &img_height, const jo::JoARMetaDataBasic &meta_data, const ar_point &points, const ar_line_or_region &lines, const ar_line_or_region &regions)
		{
			//物点转像点，等到回调结果再继续
			if (_ar_mark_engine_sd && _ar_mark_engine_sd->reflectGeodeticToImage(timestamp, img_width, img_height, meta_data, points, lines, regions,
			_pixel_points_sd, _pixel_lines_sd, _pixel_regions_sd)) {
				if (!_pixel_points_sd.empty()) {
					for (std::size_t i = 0; i < _pixel_points_sd.size(); ++i) {
						
						ArElementsInternal ar_elements_internal{};
						ar_elements_internal.X = _pixel_points_sd[i].x;
						ar_elements_internal.Y = _pixel_points_sd[i].y;
						ar_elements_internal.lat = points[i].latitude;
						ar_elements_internal.lon = points[i].longitude;
						ar_elements_internal.HMSL = points[i].altitude;
						ar_elements_internal.Guid = guid;

						//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
						//计算完出来后，也要跟送进去的点、线、面顺序一致
						if(!ar_elements_array.empty()){
							auto ar_elements = ar_elements_array.front();
							ar_elements_internal.Type = ar_elements.Type;
							ar_elements_internal.Category = ar_elements.Category;
							ar_elements_internal.CurIndex = ar_elements.CurIndex;
							ar_elements_internal.NextIndex = ar_elements.NextIndex;
							ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
							ar_elements_array.pop_front();
						}
						
						ar_infos.ArElementsArray.push_back(ar_elements_internal);
						ar_infos.ArElementsNum++;
					}
					_pixel_points_sd.clear();
				}
				if (!_pixel_lines_sd.empty()) {
					for (std::size_t i = 0; i < _pixel_lines_sd.size(); ++i) {
						for (std::size_t j = 0; j < _pixel_lines_sd[i].size(); ++j) {
							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = _pixel_lines_sd[i][j].x;
							ar_elements_internal.Y = _pixel_lines_sd[i][j].y;
							ar_elements_internal.lat = lines[i][j].latitude;
							ar_elements_internal.lon = lines[i][j].longitude;
							ar_elements_internal.HMSL = lines[i][j].altitude;
							ar_elements_internal.Guid = guid;

							//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
							//计算完出来后，也要跟送进去的点、线、面顺序一致
							if(!ar_elements_array.empty()){
								auto ar_elements = ar_elements_array.front();
								ar_elements_internal.Type = ar_elements.Type;
								ar_elements_internal.Category = ar_elements.Category;
								ar_elements_internal.CurIndex = ar_elements.CurIndex;
								ar_elements_internal.NextIndex = ar_elements.NextIndex;
								ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
								ar_elements_array.pop_front();
							}

							ar_infos.ArElementsArray.push_back(ar_elements_internal);
							ar_infos.ArElementsNum++;
						}
					}
					_pixel_lines_sd.clear();
				}
				if (!_pixel_regions_sd.empty()) {
					for (std::size_t i = 0; i < _pixel_regions_sd.size(); ++i) {
						for (std::size_t j = 0; j < _pixel_regions_sd[i].size(); ++j) {
							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = _pixel_regions_sd[i][j].x;
							ar_elements_internal.Y = _pixel_regions_sd[i][j].y;
							ar_elements_internal.lat = regions[i][j].latitude;//_pixel_regions中有两个vector，regions中只有一个vector，导致崩溃
							ar_elements_internal.lon = regions[i][j].longitude;
							ar_elements_internal.HMSL = regions[i][j].altitude;
							ar_elements_internal.Guid = guid;

							//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
							//计算完出来后，也要跟送进去的点、线、面顺序一致
							if(!ar_elements_array.empty()){
								auto ar_elements = ar_elements_array.front();
								ar_elements_internal.Type = ar_elements.Type;
								ar_elements_internal.Category = ar_elements.Category;
								ar_elements_internal.CurIndex = ar_elements.CurIndex;
								ar_elements_internal.NextIndex = ar_elements.NextIndex;
								ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
								ar_elements_array.pop_front();
							}

							ar_infos.ArElementsArray.push_back(ar_elements_internal);
							ar_infos.ArElementsNum++;
						}
					}
					_pixel_regions_sd.clear();
				}
			}
		}
#endif
		void DispatchTaskImplMultiple::hdThread()
		{
			try{
				eap_information( "-----hdThread start-----");
	#ifdef ENABLE_GPU
	#ifndef ENABLE_AIRBORNE
			void *convert_stream = ImageCvtColor::Instance()->createCudaStream();
	#endif
	#endif // ENABLE_GPU
			auto encoder_packet_callback = [this](Packet packet) {
				std::lock_guard<std::mutex> lock(_pusher_mutex);
				
				auto push_proc = [this](Packet& packet)
				{
					if (_is_push_rtc) {
						if (_pusher_rtc) {
							_pusher_rtc->pushPacket(packet);
						}
					} else {
						if (_pusher_tradition) {
							_pusher_tradition->pushPacket(packet);
						}
					}
				};

				auto meta_data_raw = packet.getSeiBuf();
				if (!meta_data_raw.empty()) {
					JoFmvMetaDataBasic metadata = packet.getMetaDataBasic();
					int meta_data_sei_buffer_size{};
					meta_data_raw = _meta_data_processing_postproc->getSerializedBytesBySetMetaDataBasic(&metadata, &meta_data_sei_buffer_size);
					auto sei_buffer = MetaDataProcessing::seiDataAssemblyH264(
						meta_data_raw.data(), meta_data_raw.size());
					if (!sei_buffer.empty()) {
						AVPacket* pkt_new = av_packet_alloc();
						int new_packet_size = packet->size + sei_buffer.size();
						if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
							int pos = 0;
							memcpy(pkt_new->data, sei_buffer.data(), sei_buffer.size());
							pos += sei_buffer.size();
							memcpy(pkt_new->data + pos, packet->data, packet->size);
							pkt_new->pts = packet->pts;
							pkt_new->dts = packet->dts;
							pkt_new->duration = packet->duration;
							pkt_new->flags = packet->flags;
							
							Packet packet_export(pkt_new);
							packet_export.setSeiBuf(meta_data_raw);
							packet_export.setMetaDataBasic(&packet.getMetaDataBasic());
							packet_export.metaDataValid() = packet.metaDataValid();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
							packet_export.setArPixelPoints(packet.getArPixelPoints());
							packet_export.setArPixelLines(packet.getArPixelLines());
#endif
							packet_export.setCurrentTime(packet.getCurrentTime());
							packet_export.setArMarkInfos(packet.getArInfos());
							packet_export.setArValidPointIndex(packet.getArValidPointIndex());
							packet_export.setArVectorFile(packet.getArVectorFile());
							push_proc(packet_export);
						} else {
							push_proc(packet);
						}
					} else {
						push_proc(packet);
					}
				}
			};

			auto demuxer_packet_callback = [encoder_packet_callback, this](Packet packet) {
				std::lock_guard<std::mutex> lock(_decoder_mutex);			
				if (_meta_data_processing_pret) {
					std::vector<uint8_t> raw_data{};
					auto meta_data = _meta_data_processing_pret->metaDataParseBasic(
						packet->data, packet->size, _codec_parameter.codec_id, raw_data);

					std::shared_ptr<JoFmvMetaDataBasic> meta_data_basic{};
					if (meta_data.first) {
						packet.metaDataValid() = true;
						packet.setMetaDataBasic(meta_data.first);
					}
					if (!raw_data.empty()) {
						packet.setSeiBuf(raw_data);
					}
				}

				if (_record && !_is_recording) {//片段视频录制
					if (_pusher_tradition_recode)
						_pusher_tradition_recode.reset();

					if (!_pusher_tradition_recode) {
						_pusher_tradition_recode = PusherTradition::createInstance();

						GET_CONFIG(std::string, getString, media_server_ip, General::kMediaServerIp);
						_recode_start_time_point = std::chrono::system_clock::now();
						auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(_recode_start_time_point.time_since_epoch()).count();
						long timestamp = static_cast<long>(milliseconds);
						_recode_start_timestamp_str = std::to_string(timestamp);
						auto url = "rtsp://" + media_server_ip + "/" + _id + "/" + _recode_start_timestamp_str;
						_pusher_tradition_recode->open(_id, url, _timebase, _framerate,
							_codec_parameter, std::chrono::milliseconds(3000));
						eap_information_printf("hd recode push url: %s", url);
						_is_recording = true;
						_record = false;
					}
			}

				if (_is_recording && _pusher_tradition_recode) {
					std::lock_guard<std::mutex> lock(_pusher_recode_mutex);
					GET_CONFIG(std::string, getString, media_server_url, General::kMediaServerUrl);
					GET_CONFIG(std::string, getString, media_server_secret, General::kMediaServerSecret);
					auto cli = HttpClient::createInstance();
	
					Poco::JSON::Object json;
					json.set("secret", media_server_secret);
					json.set("type", 1);
					json.set("app", _id);
					json.set("vhost", "__defaultVhost__");
					json.set("stream", _recode_start_timestamp_str);

					auto tt = std::chrono::system_clock::to_time_t
					(_recode_start_time_point);
					struct tm* ptm = localtime(&tt);
					char date[60] = { 0 };
					sprintf(date, "%d-%02d-%02d",
						(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday);
					json.set("period", std::string(date));

					auto json_string = jsonToString(json);
					cli->doHttpRequest(media_server_url + "/index/api/isRecording", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
						if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
							try {
								Poco::JSON::Parser parser;
								auto dval = parser.parse(response);

								auto obj = dval.extract<Poco::JSON::Object::Ptr>();
								int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
								auto status = obj && obj->has("status") ? obj->getValue<bool>("status") : false;
								if (code == 0 && status) {

								}
								else {
									cli->doHttpRequest(media_server_url + "/index/api/startRecord", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
										if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
											Poco::JSON::Parser parser;
											auto dval = parser.parse(response);
											auto obj = dval.extract<Poco::JSON::Object::Ptr>();
											int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
											auto result = obj && obj->has("result") ? obj->getValue<bool>("result") : false;
											if (code == 0 && result) {
												_recode_start_time_point = std::chrono::system_clock::now();
												eap_information("hd startRecord post successed!");
											}
										}
									});
								}
							}
							catch (...) {
								eap_warning("isRecording post failed");
							}
						}
						else {
							eap_warning("isRecording post failed");
						}
					});
					auto push_proc = [this, &cli, json_string](Packet& packet) {
						if (_pusher_tradition_recode) {
							_pusher_tradition_recode->pushPacket(packet);
						}
						auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _recode_start_time_point).count();
						if (duration >= _record_duration) {
							cli->doHttpRequest(media_server_url + "/index/api/stopRecord", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									eap_information_printf("stopRecord post successed!, url: %s", _init_parameter.push_url);
								}
							});
							_is_recording = false;
							if (_pusher_tradition_recode)
								_pusher_tradition_recode.reset();

							cli->doHttpRequest(media_server_url + "/index/api/getMp4RecordFile", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									try {
										Poco::JSON::Parser parser;
										auto dval = parser.parse(response);
										auto obj = dval.extract<Poco::JSON::Object::Ptr>();
										int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
										if (code == 0) {
											GET_CONFIG(std::string, getString, media_server_url, General::kMediaServerUrl);
											auto recode_url = media_server_url;
											auto data = obj && obj->has("data") ? obj->getObject("data") : Poco::JSON::Object::Ptr();
											auto rootPath = data && data->has("rootPath") ? data->getValue<std::string>("rootPath") : "";
											auto path = data && data->has("paths") ? data->getArray("paths")->getElement<std::string>(0) : "";
											recode_url += rootPath.substr(5, rootPath.length() - 5) + path;
											eap_information_printf("recode http url : %s", recode_url);

											Poco::JSON::Object json;
											std::size_t index = _push_urls[0].rfind("/");
											std::size_t second_index = _push_urls[0].rfind("/", index - 1);
											std::string pilot_sn = _push_urls[0].substr(second_index + 1, index - second_index - 1);
											json.set("code", 0);
											json.set("record_url", recode_url);
											json.set("is_hd", true);
											json.set("autopilotSn", pilot_sn);
											json.set("record_no", _recordNo);
											GET_CONFIG(std::string, getString, video_record_url, General::kVideoClipRecordUrl);

											auto cli = HttpClient::createInstance();
											auto json_string = jsonToString(json);
											cli->doHttpRequest(video_record_url + "/flightmonitor/custom/v1/file/addVideo", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
												if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
													try {
														Poco::JSON::Parser parser;
														auto dval = parser.parse(response);

														auto obj = dval.extract<Poco::JSON::Object::Ptr>();
														int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
														if (code != 0) {
															std::string msg = obj && obj->has("code") ? obj->getValue<std::string>("msg") : "";
															eap_error_printf("hd record post failed, http status=%d, return code=%d, msg=%s", (int)status, code, msg);
														}
														else {
															eap_information("hd record post succeed");
														}
													}
													catch (...) {
														eap_warning("addVideo post failed");
													}
												}
												else {
													eap_warning("addVideo post failed");
												}
											});

										}
									}
									catch (...) {
										eap_warning("hd getMp4RecordFile post failed");
									}
								}
							});
							eap_information("hd recode end!");
						}
					};

					auto meta_data_raw = packet.getSeiBuf();
					if (!meta_data_raw.empty()) {
						JoFmvMetaDataBasic metadata = packet.getMetaDataBasic();
						int meta_data_sei_buffer_size{};
						meta_data_raw = _meta_data_processing_postproc->getSerializedBytesBySetMetaDataBasic(&metadata, &meta_data_sei_buffer_size);

						auto sei_buffer = MetaDataProcessing::seiDataAssemblyH264(
							meta_data_raw.data(), meta_data_raw.size());
						if (!sei_buffer.empty()) {
							AVPacket* pkt_new = av_packet_alloc();
							int new_packet_size = packet->size + sei_buffer.size();
							if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
								int pos = 0;
								memcpy(pkt_new->data, sei_buffer.data(), sei_buffer.size());
								pos += sei_buffer.size();
								memcpy(pkt_new->data + pos, packet->data, packet->size);
								pkt_new->pts = packet->pts;
								pkt_new->dts = packet->dts;
								pkt_new->duration = packet->duration;
								pkt_new->flags = packet->flags;

								Packet packet_export(pkt_new);
								packet_export.setSeiBuf(meta_data_raw);
								packet_export.setMetaDataBasic(&packet.getMetaDataBasic());
								packet_export.metaDataValid() = packet.metaDataValid();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
								packet_export.setArPixelPoints(packet.getArPixelPoints());
								packet_export.setArPixelLines(packet.getArPixelLines());
								packet_export.setArPixelWarningL1s(packet.getArPixelWarningL1s());
								packet_export.setArPixelWarningL2s(packet.getArPixelWarningL2s());
#endif
								packet_export.setCurrentTime(packet.getCurrentTime());
								packet_export.setArMarkInfos(packet.getArInfos());
								packet_export.setArValidPointIndex(packet.getArValidPointIndex());
								packet_export.setArVectorFile(packet.getArVectorFile());
								push_proc(packet_export);
							}
							else {
								push_proc(packet);
							}
						}
						else {
							push_proc(packet);
						}
					}
				}

	#ifdef ENABLE_GPU
				if (_decoder) {
					_decoder->pushPacket(packet);
				}
	#else
				std::promise<void> ar_mark_promise{};
				auto ar_mark_future = ar_mark_promise.get_future();
				ThreadPool::defaultPool().start([this, &packet, &ar_mark_promise]()
				{
					//camera.config 有更新，就更新AR标注引擎
					if(_is_update_ar_mark_engine){
						destroyARMarkEngine();
						createARMarkEngine();
						_is_update_ar_mark_engine.store(false);
					}
#ifdef ENABLE_AR
					if(_ar_mark_engine && packet.metaDataValid()){
						Poco::JSON::Object root;			

						JoFmvMetaDataBasic meta_temp = packet.getMetaDataBasic();
						uint64_t timeStamp = meta_temp.CarrierVehiclePosInfo_p.TimeStamp;

						jo::JoARMetaDataBasic ar_meta_data{};
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

						int width = _codec_parameter.width;
						int height = _codec_parameter.height;										

						//先判断是否需要删除
						auto _ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.find(_mark_guid);
						while (_ar_mark_pixel_and_geographic != _ar_mark_pixel_and_geographic_map.end() 
							&& _ar_mark_pixel_and_geographic->first == _mark_guid)
						{
							_ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.erase(_ar_mark_pixel_and_geographic);
						}
						
						_mark_guid = "";

						ArInfosInternal ar_infos_internal_to_push_rtc{};//存储以前的计算结果和当前新的标注要素，用来push rtc
						ArInfosInternal ar_infos_internal_to_store{};//这次如果有新增的标注元素，用这个先存下来，再放在map中

						//前面存的像素转地理，地理坐标每帧都要重新反算新的像素坐标;前面存的地理转像素，地理坐标每帧都要重新反算新的像素坐标
						for(auto iter_map = _ar_mark_pixel_and_geographic_map.begin(); 
							iter_map != _ar_mark_pixel_and_geographic_map.end(); iter_map++)
						{
							//地理坐标转图像坐标
							jo::GeographicPosition tmpPoint{};
							std::vector<jo::GeographicPosition> tmpPointVct{};
							ar_point points{};
							ar_line_or_region lines{};
							ar_line_or_region regions{};
							
							// ar_infos_internal_to_push_rtc.ArElementsNum += iter_map->second.ArElementsNum;
							
							for(auto iter_list : iter_map->second.ArElementsArray)
							{
								tmpPoint.longitude = iter_list.lon;
								tmpPoint.latitude = iter_list.lat;
								tmpPoint.altitude = iter_list.HMSL;

								if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) 
								{
									switch (iter_list.Type) {
									case 0: {								
										points.push_back(tmpPoint);
										break;
									}
									case 1: {
										//添加每条线的各个物点
										if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {									
											tmpPointVct.push_back(tmpPoint);
										}
										//当前线已结束，添加整条线
										if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {									
											lines.push_back(tmpPointVct);
											tmpPointVct.clear();
										}
										break;
									}
									case 2: {
										//添加每个面的各个物点
										if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
											tmpPointVct.push_back(tmpPoint);
										}
										//当前面已结束，添加整个面
										if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
											regions.push_back(tmpPointVct);
											tmpPointVct.clear();
										}
										break;
									}
									default:
										break;
									}
								}
								if (!points.empty() || !lines.empty() || !regions.empty()) 
								{
									calculatGeodeticToImage(iter_map->second.ArElementsArray, ar_infos_internal_to_push_rtc
										, iter_map->first, timeStamp, width, height, ar_meta_data, points, lines, regions);
								}
							}
						}

						float ratio_x = ( _codec_parameter_sd.width == 0 ? 1: (float)_codec_parameter.width/(float)_codec_parameter_sd.width);
						float ratio_y = (_codec_parameter_sd.height == 0 ? 1: (float)_codec_parameter.height/(float)_codec_parameter_sd.height);
						if(ratio_x <= 0)
								ratio_x = 1;
						if(ratio_y <= 0)
							ratio_y = 1;
						while (!_video_mark_data.empty()) {
							std::string mark_data = _video_mark_data.front();
							_video_mark_data.pop();
							auto is_hd = true;
							if(!_video_mark_data_status.empty()){
								is_hd = _video_mark_data_status.front();
								_video_mark_data_status.pop();
							}

							Poco::JSON::Parser parser;
							try {
								Poco::Dynamic::Var result = parser.parse(mark_data);
								root = *result.extract<Poco::JSON::Object::Ptr>();
							}
							catch (const std::exception& e) {
								eap_error(e.what());
							}
							auto elementsArray = root.has("ArElementsArray") ? root.getArray("ArElementsArray"): Poco::JSON::Array::Ptr();
							int elementsArray_size = elementsArray->size();
							if (elementsArray_size <= 0) { continue; }

							jo::GeographicPosition tmpPoint{};
							std::vector<jo::GeographicPosition> tmpPointVct{};
							ar_point points{};
							ar_line_or_region lines{};
							ar_line_or_region regions{};
							
							//透传回云端
							_ar_mark_elements_guid = root.has("Guid") ? root.getValue<std::string>("Guid") : "";					
							std::list<ArElementsInternal> ar_elements_array;
							for (int i = 0; i < (std::min)(elementsArray_size, 1024); ++i) {

								auto elementJs = *elementsArray->getObject(i);

								//图像坐标转地理坐标
								if(elementJs.has("X") && elementJs.has("Y")) {
									cv::Point pixel_point;
									jo::GeographicPosition geoPos{};

									int pixelX = elementJs.getValue<int32_t>("X");
									int pixelY = elementJs.getValue<int32_t>("Y");

									pixel_point.x = pixelX * ((is_hd)? 1: ratio_x);
									pixel_point.y = pixelY * ((is_hd)? 1: ratio_y);
																
									if(_ar_mark_engine){
										_ar_mark_engine->projectImageToGeodetic(timeStamp, width, height, ar_meta_data, pixel_point, geoPos);
									}else{
										eap_error( "_ar_mark_engine->projectImageToGeodetic but _ar_mark_engine is nullptr!----");
									}

									ArElementsInternal ar_elements_internal{};
									ar_elements_internal.X = pixel_point.x;
									ar_elements_internal.Y = pixel_point.y;
									ar_elements_internal.lat = geoPos.latitude;
									ar_elements_internal.lon = geoPos.longitude;
									ar_elements_internal.HMSL = geoPos.altitude;
									ar_elements_internal.Type = elementJs.getValue<int>("Type");
									ar_elements_internal.DotQuantity = elementJs.getValue<int>("DotQuantity");
									ar_elements_internal.Category = elementJs.getValue<int>("Category");
									ar_elements_internal.CurIndex = elementJs.getValue<int>("CurIndex");
									ar_elements_internal.NextIndex = elementJs.getValue<int>("NextIndex");
									ar_elements_internal.Guid = _ar_mark_elements_guid;

									ar_infos_internal_to_store.ArElementsArray.push_back(ar_elements_internal);
									ar_infos_internal_to_store.ArElementsNum++;					
								}
														
								//地理坐标转图像坐标
								if(elementJs.has("lon") && elementJs.has("lat") && elementJs.has("HMSL")) {
									
									ArElementsInternal ar_elements_internal{};
									ar_elements_internal.Category = elementJs.getValue<int>("Category");
									ar_elements_internal.Type = elementJs.getValue<int>("Type");
									ar_elements_internal.DotQuantity = elementJs.getValue<int>("DotQuantity");
									ar_elements_internal.CurIndex = elementJs.getValue<int>("CurIndex");
									ar_elements_internal.NextIndex = elementJs.getValue<int>("NextIndex");
									ar_elements_internal.Guid = _ar_mark_elements_guid;
									ar_elements_array.push_back(ar_elements_internal);

									tmpPoint.longitude = elementJs.getValue<double>("lon");
									tmpPoint.latitude = elementJs.getValue<double>("lat");
									tmpPoint.altitude = elementJs.getValue<double>("HMSL");
									if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) {
										switch(elementJs.getValue<uint32_t>("Type")) {
										case 0: {
											points.push_back(tmpPoint);
											break;
										}
										case 1: {
											//添加每条线的各个物点
											if (elementJs.getValue<int>("CurIndex") != 0 || (elementJs.getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
												tmpPointVct.push_back(tmpPoint);
											}
											//当前线已结束，添加整条线
											if (elementJs.getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
												lines.push_back(tmpPointVct);
												tmpPointVct.clear();
											}
											break;
										}
										case 2: {
											//添加每个面的各个物点
											if (elementJs.getValue<int>("CurIndex") != 0 || (elementJs.getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
												tmpPointVct.push_back(tmpPoint);
											}
											//当前面已结束，添加整个面
											if (elementJs.getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
												regions.push_back(tmpPointVct);
												tmpPointVct.clear();
											}
											break;
										}
										default:
											break;
										}
									}
								}
							}

							//地理坐标转图像坐标(地理坐标存完后一次计算)
							if (!points.empty() || !lines.empty() || !regions.empty()) {
								calculatGeodeticToImage(ar_elements_array, ar_infos_internal_to_store, _ar_mark_elements_guid, 
									timeStamp, width, height, ar_meta_data, points, lines, regions);
							}
							
							//存储到需要push rt的容器中去
							ar_infos_internal_to_push_rtc.ArElementsNum += ar_infos_internal_to_store.ArElementsNum;
							for(auto iter : ar_infos_internal_to_store.ArElementsArray){
								ar_infos_internal_to_push_rtc.ArElementsArray.push_back(iter);
							}

							//存储当前有新增的标注要素
							_ar_mark_pixel_and_geographic_map.insert(std::make_pair(_ar_mark_elements_guid, ar_infos_internal_to_store));
						
							//if (!_is_video_mark) { _is_video_mark.store(true); }
							//videoMarkMetaDataWrite(ar_infos_internal_to_store, packet.getOriginalPts());					
							// break;//???
						}
						packet.setArMarkInfos(ar_infos_internal_to_push_rtc);
					}
#endif
					ar_mark_promise.set_value();
				});

				std::promise<void> ar_promise{};
				auto ar_future = ar_promise.get_future();
				std::lock_guard<std::mutex> update_func_lock(_update_func_mutex);
				if(_is_ar_on){
#ifdef ENABLE_AR
					createAREngine();
					ThreadPool::defaultPool().start([this, &packet, &ar_promise]()
					{
						if (_ar_engine && packet.metaDataValid()) {
							if (_ar_image_compute_state) {
								_ar_image_compute_state = false;

								auto pts = packet->pts;//重写后的时间戳
								auto width = _codec_parameter.width;
								auto height = _codec_parameter.height;
								cv::Mat mat;
								JoFmvMetaDataBasic meta_temp = packet.getMetaDataBasic();

								jo::JoARMetaDataBasic ar_meta_data{};
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

								std::vector<cv::Point> pixel_points{};
								std::vector<std::vector<cv::Point>> pixel_lines{};

								std::vector<cv::Point> tmp_pixel_points{};
								std::vector<std::vector<cv::Point>> tmp_pixel_lines{};

								std::vector<std::vector<cv::Point>> pixel_warning_l1_regions{};
								std::vector<std::vector<cv::Point>> pixel_warning_l2_regions{};
								_ar_engine->frameProcess(true, pts, mat, width, height, ar_meta_data, pixel_points, pixel_lines
									, pixel_warning_l1_regions, pixel_warning_l2_regions);
								for (const auto& iter : pixel_warning_l1_regions) {
									pixel_lines.push_back(iter);
								}
								for (const auto& iter : pixel_warning_l2_regions) {
									pixel_lines.push_back(iter);
								}

								// 筛选去掉屏幕外多余的点
								int i = 0;
								std::queue<int> ar_valid_point_index{}; // 筛选去掉屏幕外多余的点后有效点的索引
								for(const auto &iter : pixel_points){
									if(iter.x <= width  && iter.x >= 0 && iter.y <= height && iter.y >= 0){
										tmp_pixel_points.push_back(iter);
										ar_valid_point_index.push(i);
									}
									++i;
								}
								for(std::size_t index = 0; index < pixel_lines.size(); ++index){
									auto& iter_line = pixel_lines[index];

									std::vector<cv::Point> tmp_points{};
									
									bool is_in_screen{};
									int boundary=width/2;
									
									for(std::size_t i = 0; i < iter_line.size(); ++i) {
										auto& iter = iter_line[i];
										if(iter.x <= width+boundary && iter.x >= 0-boundary && iter.y <= height+boundary && iter.y >= 0-boundary){
											is_in_screen = true;
										}

										if (!is_in_screen) {
											continue;
										}
										is_in_screen = false;
										
										if (i) {
											auto& front_iter = iter_line[i - 1];
											if(front_iter.x > width+boundary || front_iter.x < 0-boundary || front_iter.y > height+boundary || front_iter.y < 0-boundary){
												tmp_points.push_back(front_iter);
											}
										}

										tmp_points.push_back(iter);
										
										if (i < iter_line.size() - 1) {
											auto& back_iter = iter_line[i + 1];
											//警戒线是个环，不能break
											if(back_iter.x > width+boundary || back_iter.x < 0-boundary || back_iter.y > height+boundary || back_iter.y < 0-boundary){
												tmp_points.push_back(back_iter);
											}
										}
									}

									if (tmp_points.empty() && _ar_image_compute_pixel_lines.size() == pixel_lines.size()) {
										auto& tmp_line = _ar_image_compute_pixel_lines[index];
										for (auto &iter : tmp_line) {
											tmp_points.push_back(iter);
										}
										
									}

									tmp_pixel_lines.push_back(tmp_points);
								}

								std::swap(_ar_valid_point_index, ar_valid_point_index);
								std::swap(_ar_image_compute_pixel_points, tmp_pixel_points);
								std::swap(_ar_image_compute_pixel_lines, tmp_pixel_lines);
							}
							else{
								_ar_image_compute_state = true;
							}
							
							packet.setArPixelLines(_ar_image_compute_pixel_lines);
							packet.setArPixelPoints(_ar_image_compute_pixel_points);
							packet.setArValidPointIndex(_ar_valid_point_index);
							if(packet.getArVectorFile()!=_ar_vector_file){
								packet.setArVectorFile(_ar_vector_file);
							}
						}
						ar_promise.set_value();
					});
#endif
				}
				else{
					destroyAREngine();
					ar_promise.set_value();
				}

				if(_is_update_func){
					if(!_update_func_err_desc.empty()){
						_update_func_err_desc += std::string("update failed");
					}

					std::string id_temp = _id;
					NoticeCenter::Instance()->getCenter().postNotification(new FunctionUpdatedNotice( 
						id_temp, _update_func_result, _update_func_err_desc, _function_mask));

					_is_update_func = false;
					_update_func_result = true;
					_update_func_err_desc.clear();
				}

				ar_future.wait_for(std::chrono::seconds(3));
				ar_mark_future.wait_for(std::chrono::seconds(3));

				//设置当前时间点			
				AVRational  dst_time_base = { 1, AV_TIME_BASE };
				int64_t _start_time_stamp = av_rescale_q(_start_time, dst_time_base, _timebase);
				int64_t current_time = (packet->pts - _start_time_stamp) * av_q2d(_timebase) * 1000.f;
				packet.setCurrentTime(current_time);

				encoder_packet_callback(packet);
	#endif // ENABLE_GPU
			};
			
			auto demuxer_stop_callback = [this](int exit_code) {
				_is_demuxer_closed = true;
				if (!_is_manual_stoped) {
					ThreadPool::defaultPool().start([this, exit_code]()
					{
						std::string desc = "demuxer stoped, exit code: " + std::to_string(exit_code)
							+ std::string("; id: ") + _id;
						eap_error( desc);
						_notice_task_id = _id;
						_notice_task_internal_id = _id;
						std::size_t index = _push_urls[0].rfind("/");
						std::size_t second_index = _push_urls[0].rfind("/", index - 1);
						std::string pilot_sn = _push_urls[0].substr(second_index + 1, index - second_index - 1);
						NoticeCenter::Instance()->getCenter().postNotification(
							new TaskStopedNotice(std::string(_notice_task_id), desc, pilot_sn));
					});
				}
			};

	#ifdef ENABLE_GPU
			
			auto decoder_frame_callback = [this](Frame frame) {
				if(!_loop_run){
					_decoded_images_cv.notify_all();
					return;
				}
				{
					std::lock_guard<std::mutex> lock(_decoded_images_mutex);
					if (_decoded_images.size() > 5) {
						eap_error( "decoded images queue is full, drop frame");
						return;
					}
				}//AI、AR等其它功能处理不过来丢帧			
#ifndef ENABLE_AIRBORNE
				cv::cuda::GpuMat bgr24_image_gpu;
				cv::cuda::GpuMat bgr32_image_gpu;
				try {
					bgr24_image_gpu = cv::cuda::GpuMat(frame->height, frame->width, CV_8UC3);
					bgr32_image_gpu = cv::cuda::GpuMat(frame->height, frame->width, CV_8UC4);

					ImageCvtColor::Instance()->nv12ToBgr24(frame, bgr24_image_gpu);
					ImageCvtColor::Instance()->bgr24ToBgr32(bgr24_image_gpu, bgr32_image_gpu);
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					return;
				}
#endif
				CodecImagePtr decode_image = CodecImagePtr(new CodecImage());

#ifdef ENABLE_AIRBORNE
				try {// TODO: 增加判断条件，根据是否需要做ai相关来决定是否进行转换
					cv::Mat bgr240_image(frame->height, frame->width, CV_8UC3);

					cv::Mat yuv_data(frame->height * 3 / 2, frame->width, CV_8UC1);
					int image_size = frame->height * frame->width;
					memcpy(yuv_data.data, frame->data[0],  image_size);
					memcpy(yuv_data.data + image_size, frame->data[1], image_size / 4);
					memcpy(yuv_data.data + image_size * 5 / 4, frame->data[2], image_size / 4);
					
					I4202Bgr((char*)yuv_data.data, (char*)bgr240_image.data, pIn420Dev, pOutBgrDev, yuv_data.cols, yuv_data.rows * 2 / 3, 0, 0, frame->width, frame->height);
					
					if (!bgr240_image.empty()) {
						decode_image->bgr24_image = bgr240_image;
					}
				}
				catch (const std::exception& e) {
					eap_error_printf("frame 2 cpu_mat faile, error description: %s", e.what());
					return;
				}
#else
	#ifdef ENABLE_GPU

				decode_image->bgr24_image = bgr24_image_gpu;
				bgr24_image_gpu.release();
				decode_image->bgr32_image = bgr32_image_gpu;
				bgr32_image_gpu.release();
	#endif
#endif // ENABLE_AIRBORNE

				decode_image->meta_data.pts = frame->pts;
				auto sei_buffer = frame.getSeiBuf();
				if (!sei_buffer.empty()) {
					decode_image->meta_data.meta_data_raw_binary = sei_buffer;
				}
				decode_image->meta_data.meta_data_basic = frame.getMetaDataBasic();
				decode_image->meta_data.meta_data_valid = frame.getMetaDataValid();
				decode_image->meta_data.original_pts = frame.getOriginalPts();

				std::lock_guard<std::mutex> lock(_decoded_images_mutex);
				_decoded_images.push(decode_image);
				_decoded_images_cv.notify_all();
			};
	#endif // ENABLE_GPU
			auto pusher_stop_callback = [this](int ret, std::string err_str) {
				if(_is_demuxer_closed)
					return;

				if(!_loop_run){
					_decoded_images_cv.notify_all();
					return;
				}
				std::lock_guard<std::mutex> lock(_pusher_mutex);
				ThreadPool::defaultPool().start([this, ret, err_str]()
				{
					std::string desc = "pusher stoped, exit code: " + std::to_string(ret) + ", description: " + err_str;
					// 启动重连
					if (_is_push_rtc) {
						if (_pusher_rtc && _loop_run) {
							_pusher_rtc->close();
							_pusher_rtc->open(_id, _push_urls[0], _timebase, _framerate,
							_codec_parameter, std::chrono::milliseconds(3000));
							eap_information("reopen  pusher_rtc");
						}
						
					}
					else {
						if (_pusher_tradition && _loop_run) {
							_pusher_tradition->close();
							_pusher_tradition->open(_id, _push_urls[0], _timebase, _framerate,
							_codec_parameter, std::chrono::milliseconds(3000));
							eap_information("reopen  pusher_tradition");
						}
					}
				});
				if(_loop_run)
					std::this_thread::sleep_for(std::chrono::milliseconds{1000});
			};
			auto video_mark_data_callback = [this](std::string mark_data) {
	#if AR_MARK_USE_DATACHANNEL
				_video_mark_data.push(mark_data);
	#endif
			};

			if (_is_pull_rtc) {
				_demuxer_rtc = DemuxerRtc::createInstance();
				_demuxer_rtc->setPacketCallback(demuxer_packet_callback);
				_demuxer_rtc->setStopCallback(demuxer_stop_callback);
				_demuxer_rtc->open(_pull_urls[0], std::chrono::milliseconds(3000));
				_codec_parameter = _demuxer_rtc->videoCodecParameters();
				_timebase = _demuxer_rtc->videoStreamTimebase();
				_framerate = _demuxer_rtc->videoFrameRate();
				_start_time = _demuxer_rtc->videoStartTime();
				_video_duration = _demuxer_rtc->videoDuration();
			}
			else {
				_demuxer_tradition = DemuxerTradition::createInstance();
				_demuxer_tradition->setPacketCallback(demuxer_packet_callback);
				_demuxer_tradition->setStopCallback(demuxer_stop_callback);
				_demuxer_tradition->open(_pull_urls[0], std::chrono::milliseconds(3000));
				_codec_parameter = _demuxer_tradition->videoCodecParameters();
				_timebase = _demuxer_tradition->videoStreamTimebase();
				_framerate = _demuxer_tradition->videoFrameRate();
				_start_time = _demuxer_tradition->videoStartTime();
				_video_duration = _demuxer_tradition->videoDuration();
			}

	#ifdef ENABLE_GPU
			{
				std::lock_guard<std::mutex> lock(_decoder_mutex);
				_decoder = Decoder::createInstance();
				_decoder->setFrameCallback(decoder_frame_callback);
				_decoder->open(_codec_parameter, _framerate, _pull_urls[0], _is_hardware_decode);
			}
	#endif // ENABLE_GPU
			
			// TODO:
	#ifdef ENABLE_GPU
			if (/*(_function_mask & FUNCTION_MASK_DEFOG) || (_function_mask & FUNCTION_MASK_STABLE)*/true) {
				std::lock_guard<std::mutex> lock(_encoder_mutex);

				EncoderNVENC::InitParameter init_parameter;
				init_parameter.bit_rate = 5000000;
				init_parameter.dst_width = _codec_parameter.width;
				init_parameter.dst_height = _codec_parameter.height;
				init_parameter.time_base = _timebase;
				init_parameter.framerate = _framerate;
				init_parameter.start_time = _start_time;
				
				_encoder = EncoderNVENC::createInstance(init_parameter, encoder_packet_callback);
				_encoder->start();

			}
	#endif // ENABLE_GPU

			if (_is_push_rtc) {
				std::lock_guard<std::mutex> lock(_pusher_mutex);
				_pusher_rtc = PusherRtc::createInstance();
				_pusher_rtc->updateVideoParams(_video_duration);
				_pusher_rtc->setStopCallback(pusher_stop_callback);
				_pusher_rtc->setMarkDataCallback(video_mark_data_callback);
				//TODO: _codec_parameter: 如果编码，则需要用编码器的
				_pusher_rtc->open(_id, _push_urls[0], _timebase, _framerate,
					_codec_parameter, std::chrono::milliseconds(3000));
			}
			else {
				std::lock_guard<std::mutex> lock(_pusher_mutex);

				_pusher_tradition = PusherTradition::createInstance();
				_pusher_tradition->setStopCallback(pusher_stop_callback);
				_pusher_tradition->open(_id, _push_urls[0], _timebase, _framerate,
					_codec_parameter, std::chrono::milliseconds(3000));
			}

			createARMarkEngine();
			createAREngine();

			_is_ar_first_create = false;

#ifdef ENABLE_GPU
			createAIEngine();
#ifndef ENABLE_AIRBORNE
			createImageEnhancer();
			createImageStabilizer();
#endif

			_is_image_enhancer_first_create = false;
			_is_image_stable_first_create = false;
			_is_ai_first_create = false;

			_loop_thread = std::thread([this]() {
				_loop_run = true;
				for (; _loop_run;) {
					std::unique_lock<std::mutex> lock(_decoded_images_mutex);
					if (_decoded_images.empty()) {
						_decoded_images_cv.wait_for(lock, std::chrono::milliseconds(500));
						if (!_loop_run) {
							break;
						}
					}

					if (_decoded_images.empty()) {
						continue;
					}

					auto image = _decoded_images.front();				
					_decoded_images.pop();
					lock.unlock();

					std::lock_guard<std::mutex> update_func_lock(_update_func_mutex);
					// _update_func_mutex.lock();
#ifndef ENABLE_AIRBORNE
					if (_snapshot) { //快照
						//ThreadPool::defaultPool().start([this, image]() {
							GET_CONFIG(std::string, getString, uploadSnapshot, General::kUploadSnapshot);
							std::string pilot_sn{};
							//上传图片
							std::string base64_encoded{};
							cv::Mat bgr24_image_cpu;// 是否需要初始化
							try {
								bgr24_image_cpu.create(image->bgr24_image.size(), image->bgr24_image.type()); // 初始化 cpuImage
								image->bgr24_image.download(bgr24_image_cpu);
								std::vector<uchar> buffer;
								cv::imencode(".jpg", bgr24_image_cpu, buffer, { cv::IMWRITE_JPEG_QUALITY, 80 });
								base64_encoded = "data:image/jpg;base64," + encodeBase64({ buffer.begin(), buffer.end() });
								bgr24_image_cpu.release();

								std::size_t index = _push_urls[0].rfind("/");
								std::size_t second_index = _push_urls[0].rfind("/", index - 1);
								pilot_sn = _push_urls[0].substr(second_index + 1, index - second_index - 1);
							}
							catch (const std::exception& e) {
								bgr24_image_cpu.release();
								eap_error(std::string(e.what()));
								return;
							}
							Poco::JSON::Object json;
							json.set("file", base64_encoded);
							json.set("autopilotSn", pilot_sn);
							json.set("is_hd", true);
							json.set("record_no", _recordNo);
							json.set("code", 0);
							std::string json_string = jsonToString(json);
							auto http_client = HttpClient::createInstance();
							http_client->doHttpRequest(uploadSnapshot + "/flightmonitor/custom/v1/file/addPhoto", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									try {
										Poco::JSON::Parser parser;
										auto dval = parser.parse(response);

										auto obj = dval.extract<Poco::JSON::Object::Ptr>();
										int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
										if (code != 0) {
											std::string msg = obj && obj->has("msg") ? obj->getValue<std::string>("msg") : "";
											eap_error_printf("hd snapshot post failed, http status=%d, return code=%d, msg=%s", status, code, msg);
										}
										else {
											eap_information("hd snapshot post succeed");
										}
									}
									catch (...) {
										eap_warning("hd addPhoto post failed");
									}
								}
								else {
									eap_warning("hd addPhoto post failed");
								}
							});
						//});
						_snapshot = false;
					}
#endif

					// AI AR DEFOG等在这里调度
	#ifdef ENABLE_AI
					std::vector<joai::Result> ai_detect_ret{};

					std::promise<void> ai_promise;//void只用阻塞功能
					auto ai_future = ai_promise.get_future();

					if (_is_ai_on) {
						createAIEngine();
						std::weak_ptr<DispatchTaskImplMultiple> this_weak_ptr = weak_from_this();
						ThreadPool::defaultPool().start([this, &image, &ai_promise, &ai_detect_ret, this_weak_ptr]()
						{
							auto this_shared_ptr = this_weak_ptr.lock();
							if (!this_shared_ptr) {
								ai_promise.set_value();
								return;
							}
							if (!_ai_object_detector) {
								ai_promise.set_value();
								return;
							}

							auto detect_objects = _ai_object_detector->detect(image->bgr24_image);
							image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
								ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = detect_objects.size();
							
							image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
								ImageProcessingBoardInfo_p.AiInfos_p.AiStatus = 1;

							int i = 0;
							for (auto object : detect_objects) {
								ai_detect_ret.push_back(object);

								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX =
									object.Bounding_box.x;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY =
									object.Bounding_box.y;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetWidth =
									object.Bounding_box.width;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetHeight =
									object.Bounding_box.height;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass =
									object.cls;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtConfidence =
									object.confidence;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtSN =
									object.Frame_num;
									
								++i;
							}

							ai_promise.set_value();
						});
					}
					else {
						destroyAIEngine();

						ai_promise.set_value();
					}
	#endif
#ifdef ENABLE_AR
					std::promise<void> ar_promise{};
					auto ar_future = ar_promise.get_future();
					createAREngine();
					if (_is_ar_on || _is_enhanced_ar_on) {
						if(_is_enhanced_ar_on){
							createAuxiliaryAIEngine();
						}
						std::weak_ptr<DispatchTaskImplMultiple> this_weak_ptr = weak_from_this();
						ThreadPool::defaultPool().start([this, &image, &ar_promise, this_weak_ptr]()
						{
							auto this_shared_ptr = this_weak_ptr.lock();
							if (!this_shared_ptr) {
								ar_promise.set_value();
								return;
							}
							executeHdARProcess(image);
							ar_promise.set_value();
						});
					}
					else {
						//destroyAREngine();
#ifdef ENABLE_AI
						if(_aux_ai_object_detector){
							destroyAuxiliaryAIEngine();
						}
#endif
						ar_promise.set_value();
					}
					{
						//TODO: 先把标注拿出来改成同步,相当于也是异步，在主线程中和子线程异步

						//camera.config 有更新，就更新AR标注引擎
						if(_is_update_ar_mark_engine){
							destroyARMarkEngine();
							createARMarkEngine();
							_is_update_ar_mark_engine.store(false);
						}
						executeHdARMarkProcess(image);
					}
#endif
					cv::Mat M;
					bool is_stable_geted{};
					int64_t pts_temp{};
#ifndef ENABLE_AIRBORNE
					if (_is_image_stable_on) {
						createImageStabilizer();
						if (_stabilizer) {
							pts_temp = image->meta_data.pts;

							is_stable_geted = _stabilizer->run(
								image->bgr32_image, image->meta_data.pts, M);
						}
					}
					else {
						destroyImageStabilizer();
						if (!_meta_data_cache.empty()) {
							_meta_data_cache.clear();
						}
					}
#endif
#ifndef ENABLE_AIRBORNE
					if (_is_image_enhancer_on) {
						do {
							if (_is_image_stable_on && !is_stable_geted) {
								break;
							}
							createImageEnhancer();

							if (_enhancer) {
								_enhancer->Enhance(image->bgr32_image.data);
							}
						} while (false);
					}
					else {
						destroyImageEnhancer();
					}
#endif
	#ifdef ENABLE_AI
					std::future_status ai_future_status = ai_future.wait_for(std::chrono::seconds(3));
					if (ai_future_status == std::future_status::ready) {
					} else if (ai_future_status == std::future_status::timeout) {
						eap_warning("---ai_future-hd--Task did not complete within the timeout." );
					} else if (ai_future_status == std::future_status::deferred) {
						eap_warning("---ai_future-hd-Task is deferred.");
					}
	#endif
	#ifdef ENABLE_AR
					std::future_status ar_future_status = ar_future.wait_for(std::chrono::seconds(3));
					if (ar_future_status == std::future_status::ready) {
					} else if (ar_future_status == std::future_status::timeout) {
						eap_warning("---ar_future--hd-Task did not complete within the timeout.");
					} else if (ar_future_status == std::future_status::deferred) {
						eap_warning("---ar_future-hd-Task is deferred.");
					}
	#endif
					if(!_loop_run)
						return;
#ifndef ENABLE_AIRBORNE
					if (_is_image_stable_on) {
						_meta_data_cache[pts_temp] = image->meta_data;
						if (!is_stable_geted) {
							continue;
						}
					}

					if (_is_image_stable_on && !M.empty()) {
						auto meta_data_it = _meta_data_cache.find(image->meta_data.pts);
						if (meta_data_it != _meta_data_cache.end()) {
							image->meta_data = meta_data_it->second;
							_meta_data_cache.erase(meta_data_it->first);
						}
						else {
							image->meta_data.meta_data_valid = false;
						}

						std::promise<void> tracking_transform_promise{};
						std::promise<void> ar_transform_promise{};
						std::promise<void> ai_transform_promise{};

						auto tracking_transform_future = tracking_transform_promise.get_future();
						auto ar_transform_future = ar_transform_promise.get_future();
						auto ai_transform_future = ai_transform_promise.get_future();

						ThreadPool::defaultPool().start(
							[this, &image, &M, &tracking_transform_promise]() {
							trackingCoordTransform(image, M);
							tracking_transform_promise.set_value();
						});

						if (_is_ar_on || _is_enhanced_ar_on) {
							ThreadPool::defaultPool().start(
								[this, &image, &M, &ar_transform_promise]() {
								arCoordTransform(image, M);
								ar_transform_promise.set_value();
							});
						}
						else {
							ar_transform_promise.set_value();
						}
						
						if (_is_ai_on) {
							ThreadPool::defaultPool().start(
								[this, &image, &M, &ai_transform_promise]() {
								aiCoordTransform(image, M);
								ai_transform_promise.set_value();
							});
						}
						else {
							ai_transform_promise.set_value();
						}
						
						tracking_transform_future.wait_for(std::chrono::seconds(3));
						ar_transform_future.wait_for(std::chrono::seconds(3));
						ai_transform_future.wait_for(std::chrono::seconds(3));
						if(!_loop_run)
							return;
					}
	#endif
	#if defined(ENABLE_AI) && defined(ENABLE_AR)			
					auto now = std::chrono::system_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _upload_image_time_point).count();

					bool track_switch{};
					int frequency{};
					std::string danger_photo_server_url{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
						GET_CONFIG(int, getInt, my_frequency, General::kUploadImageTimeDuration);
						GET_CONFIG(std::string, getString, my_danger_photo_server_url, AI::KDangerPhotoServerUrl);
						track_switch = my_track_switch;
						frequency = my_frequency;
						danger_photo_server_url = my_danger_photo_server_url;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}
					
					if (!ai_detect_ret.empty() && _ar_engine && duration > frequency * 1000) {
						_upload_image_time_point = std::chrono::system_clock::now();
						std::lock_guard<std::mutex> lock(_danger_queue_mutex);
						if (_danger_images.size() >= 6) { 
							_danger_images.pop();
							_danger_ai_ret.pop();
						}
						_danger_images.push(image);
						_danger_ai_ret.push(ai_detect_ret);
						_danger_queue_cv.notify_all();
					}
	#endif // !_WIN32
					std::weak_ptr<DispatchTaskImplMultiple> this_weak_ptr = weak_from_this();
					ThreadPool::defaultPool().start([this, image, this_weak_ptr]()
					{
						auto this_shared_ptr = this_weak_ptr.lock();
						if (!this_shared_ptr) {
							return;
						}

						std::lock_guard<std::mutex> lock_encoder(_encoder_mutex);

						if (_encoder) {
							_encoder->updateFrame(*image);//编码太快，现在加了AR计算耗时太多？导致GPU显存上涨
						}
					});
					
					if(_is_update_func){
						if(!_update_func_err_desc.empty()){
							_update_func_err_desc += std::string("update failed");
						}

						std::string id_temp = _id;
						NoticeCenter::Instance()->getCenter().postNotification(new FunctionUpdatedNotice(
							id_temp, _update_func_result, _update_func_err_desc, _function_mask));

						_is_update_func = false;
						_update_func_result = true;
						_update_func_err_desc.clear();
					}
					// _update_func_mutex.unlock();
				}
			});
	#endif // ENABLE_GPU
			}catch(const std::exception& e){
				if(_exception_callback)
					_exception_callback(_id, std::string(e.what()));
			}
		}
		
		void DispatchTaskImplMultiple::sdThread()
		{
			try{
				eap_information( "-----sdThread start-----");
	#ifdef ENABLE_GPU
	#ifndef ENABLE_AIRBORNE
			void *convert_stream = ImageCvtColor::Instance()->createCudaStream();
	#endif
	#endif // ENABLE_GPU
			auto encoder_packet_callback = [this](Packet packet) {
				auto push_proc = [this](Packet& packet)
				{
					if (_is_push_sd_rtc) {
						if (_pusher_sd_rtc) {
							_pusher_sd_rtc->pushPacket(packet);
						}
					}
					else {
						if (_pusher_sd_tradition) {
							_pusher_sd_tradition->pushPacket(packet);
						}
					}
				};

				auto meta_data_raw = packet.getSeiBuf();
				if (!meta_data_raw.empty()) {
					JoFmvMetaDataBasic metadata = packet.getMetaDataBasic();
					int meta_data_sei_buffer_size{};
					meta_data_raw = _meta_data_processing_postproc_sd->getSerializedBytesBySetMetaDataBasic(&metadata, &meta_data_sei_buffer_size);
					auto sei_buffer = MetaDataProcessing::seiDataAssemblyH264(
						meta_data_raw.data(), meta_data_raw.size());
					if (!sei_buffer.empty()) {
						AVPacket* pkt_new = av_packet_alloc();
						int new_packet_size = packet->size + sei_buffer.size();
						if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
							int pos = 0;
							memcpy(pkt_new->data, sei_buffer.data(), sei_buffer.size());
							pos += sei_buffer.size();
							memcpy(pkt_new->data + pos, packet->data, packet->size);
							pkt_new->pts = packet->pts;
							pkt_new->dts = packet->dts;
							pkt_new->duration = packet->duration;
							pkt_new->flags = packet->flags;
							
							Packet packet_export(pkt_new);
							packet_export.setSeiBuf(meta_data_raw);
							packet_export.setMetaDataBasic(&packet.getMetaDataBasic());
							packet_export.metaDataValid() = packet.metaDataValid();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
							packet_export.setArPixelPoints(packet.getArPixelPoints());
							packet_export.setArPixelLines(packet.getArPixelLines());
#endif
							packet_export.setCurrentTime(packet.getCurrentTime());
							packet_export.setArMarkInfos(packet.getArInfos());
							packet_export.setArValidPointIndex(packet.getArValidPointIndex());
							packet_export.setArVectorFile(packet.getArVectorFile());
							push_proc(packet_export);
						}
						else {
							push_proc(packet);
						}
					}
					else {
						push_proc(packet);
					}
				}
			};

			auto demuxer_packet_callback = [encoder_packet_callback, this](Packet packet) {
				std::lock_guard<std::mutex> lock(_decoder_sd_mutex);			
				if (_meta_data_processing_pret_sd) {
					std::vector<uint8_t> raw_data{};
					auto meta_data = _meta_data_processing_pret_sd->metaDataParseBasic(
						packet->data, packet->size, _codec_parameter_sd.codec_id, raw_data);

					std::shared_ptr<JoFmvMetaDataBasic> meta_data_basic{};
					if (meta_data.first) {
						packet.metaDataValid() = true;
						packet.setMetaDataBasic(meta_data.first);
					}
					if (!raw_data.empty()) {
						packet.setSeiBuf(raw_data);
					}
				}

				if (_record_sd && !_is_recording_sd) {//片段视频录制
					if (_pusher_sd_tradition_recode)
						_pusher_sd_tradition_recode.reset();

					if (!_pusher_sd_tradition_recode) {
						_pusher_sd_tradition_recode = PusherTradition::createInstance();

						GET_CONFIG(std::string, getString, media_server_ip, General::kMediaServerIp);
						_recode_sd_start_time_point = std::chrono::system_clock::now();
						auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(_recode_sd_start_time_point.time_since_epoch()).count();
						long timestamp = static_cast<long>(milliseconds);
						_recode_sd_start_timestamp_str = std::to_string(timestamp);
						auto url = "rtsp://" + media_server_ip + "/" + _id + "/" + _recode_sd_start_timestamp_str;
						_pusher_sd_tradition_recode->open(_id, url, _timebase, _framerate,
							_codec_parameter, std::chrono::milliseconds(3000));
						eap_information_printf("sd recode push url: %s", url);
						_is_recording_sd = true;
						_record_sd = false;
					}
				}
				if (_is_recording_sd && _pusher_sd_tradition_recode) {
					std::lock_guard<std::mutex> lock(_pusher_sd_recode_mutex);
					GET_CONFIG(std::string, getString, media_server_url, General::kMediaServerUrl);
					GET_CONFIG(std::string, getString, media_server_secret, General::kMediaServerSecret);
					auto cli = HttpClient::createInstance();
					Poco::JSON::Object json;
					json.set("secret", media_server_secret);
					json.set("type", 1);
					json.set("app", _id);
					json.set("vhost", "__defaultVhost__");
					json.set("stream", _recode_start_timestamp_str);

					auto tt = std::chrono::system_clock::to_time_t
					(_recode_sd_start_time_point);
					struct tm* ptm = localtime(&tt);
					char date[60] = { 0 };
					sprintf(date, "%d-%02d-%02d",
						(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday);
					json.set("period", std::string(date));
					auto json_string = jsonToString(json);
					cli->doHttpRequest(media_server_url + "/index/api/isRecording", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
						if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
							try {
								Poco::JSON::Parser parser;
								auto dval = parser.parse(response);

								auto obj = dval.extract<Poco::JSON::Object::Ptr>();
								int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
								auto status = obj && obj->has("status") ? obj->getValue<bool>("status") : false;
								if (code == 0 && status) {

								}
								else {
									cli->doHttpRequest(media_server_url + "/index/api/startRecord", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
										if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
											Poco::JSON::Parser parser;
											auto dval = parser.parse(response);
											auto obj = dval.extract<Poco::JSON::Object::Ptr>();
											int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
											auto result = obj && obj->has("result") ? obj->getValue<bool>("result") : false;
											if (code == 0 && result) {
												_recode_sd_start_time_point = std::chrono::system_clock::now();
												eap_information("sd startRecord post successed!");
											}
										}
									});
								}
							}
							catch (...) {
								eap_warning("isRecording post failed");
							}
						}
						else {
							eap_warning("isRecording post failed");
						}
					});
					auto push_proc = [this, &cli, json_string](Packet& packet) {
						if (_pusher_sd_tradition_recode) {
							_pusher_sd_tradition_recode->pushPacket(packet);
						}
						auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _recode_sd_start_time_point).count();
						if (duration >= _record_duration * 1.5) {
							cli->doHttpRequest(media_server_url + "/index/api/stopRecord", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									eap_information_printf("sd stopRecord post successed!, url: %s", _init_parameter.push_url);
								}
							});
							_is_recording_sd = false;
							if (_pusher_sd_tradition_recode)
								_pusher_sd_tradition_recode.reset();

							cli->doHttpRequest(media_server_url + "/index/api/getMp4RecordFile", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									try {
										Poco::JSON::Parser parser;
										auto dval = parser.parse(response);
										auto obj = dval.extract<Poco::JSON::Object::Ptr>();
										int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
										if (code == 0) {
											GET_CONFIG(std::string, getString, media_server_url, General::kMediaServerUrl);
											auto recode_url = media_server_url;
											auto data = obj && obj->has("data") ? obj->getObject("data") : Poco::JSON::Object::Ptr();
											auto rootPath = data && data->has("rootPath") ? data->getValue<std::string>("rootPath") : "";
											auto path = data && data->has("paths") ? data->getArray("paths")->getElement<std::string>(0) : "";
											recode_url += rootPath.substr(5, rootPath.length() - 5) + path;
											eap_information_printf("sd recode http url : %s", recode_url);

											Poco::JSON::Object json;
											json.set("code", 0);
											json.set("record_url", recode_url);
											json.set("is_hd", false);
											std::size_t index = _push_urls[1].rfind("/");
											std::size_t second_index = _push_urls[1].rfind("/", index - 1);
											std::string pilot_sn = _push_urls[1].substr(second_index + 1, index - second_index - 1);
											json.set("autopilotSn", pilot_sn);
											json.set("record_no", _recordNo);
											GET_CONFIG(std::string, getString, video_record_url, General::kVideoClipRecordUrl);
											auto cli = HttpClient::createInstance();
											auto json_string = jsonToString(json);
											cli->doHttpRequest(video_record_url + "/flightmonitor/custom/v1/file/addVideo", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
												if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
													try {
														Poco::JSON::Parser parser;
														auto dval = parser.parse(response);

														auto obj = dval.extract<Poco::JSON::Object::Ptr>();
														int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
														if (code != 0) {
															std::string msg = obj && obj->has("code") ? obj->getValue<std::string>("msg") : "";
															eap_error_printf("sd record post failed, http status=%d, return code=%d, msg=%s", (int)status, code, msg);
														}
														else {
															eap_information("sd record post succeed");
														}
													}
													catch (...) {
														eap_warning("sd addVideo post failed");
													}
												}
												else {
													eap_warning("sd addVideo post failed");
												}
											});
										}
									}
									catch (...) {
										eap_warning("getMp4RecordFile post failed");
									}
								}
							});
							eap_information("sd recode end!");
						}
					};

					auto meta_data_raw = packet.getSeiBuf();
					if (!meta_data_raw.empty()) {
						JoFmvMetaDataBasic metadata = packet.getMetaDataBasic();
						int meta_data_sei_buffer_size{};
						meta_data_raw = _meta_data_processing_postproc_sd->getSerializedBytesBySetMetaDataBasic(&metadata, &meta_data_sei_buffer_size);

						auto sei_buffer = MetaDataProcessing::seiDataAssemblyH264(
							meta_data_raw.data(), meta_data_raw.size());
						if (!sei_buffer.empty()) {
							AVPacket* pkt_new = av_packet_alloc();
							int new_packet_size = packet->size + sei_buffer.size();
							if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
								int pos = 0;
								memcpy(pkt_new->data, sei_buffer.data(), sei_buffer.size());
								pos += sei_buffer.size();
								memcpy(pkt_new->data + pos, packet->data, packet->size);
								pkt_new->pts = packet->pts;
								pkt_new->dts = packet->dts;
								pkt_new->duration = packet->duration;
								pkt_new->flags = packet->flags;

								Packet packet_export(pkt_new);
								packet_export.setSeiBuf(meta_data_raw);
								packet_export.setMetaDataBasic(&packet.getMetaDataBasic());
								packet_export.metaDataValid() = packet.metaDataValid();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
								packet_export.setArPixelPoints(packet.getArPixelPoints());
								packet_export.setArPixelLines(packet.getArPixelLines());
								packet_export.setArPixelWarningL1s(packet.getArPixelWarningL1s());
								packet_export.setArPixelWarningL2s(packet.getArPixelWarningL2s());
#endif
								packet_export.setCurrentTime(packet.getCurrentTime());
								packet_export.setArMarkInfos(packet.getArInfos());
								packet_export.setArValidPointIndex(packet.getArValidPointIndex());
								packet_export.setArVectorFile(packet.getArVectorFile());
								push_proc(packet_export);
							}
							else {
								push_proc(packet);
							}
						}
						else {
							push_proc(packet);
						}
					}
				}
	#ifdef ENABLE_GPU
				if (_decoder_sd) {
					_decoder_sd->pushPacket(packet);
				}
	#else
				std::promise<void> ar_mark_promise{};
				auto ar_mark_future = ar_mark_promise.get_future();
				ThreadPool::defaultPool().start([this, &packet, &ar_mark_promise]()
				{
					//camera.config 有更新，就更新AR标注引擎
					if(_is_update_ar_mark_engine_sd){
						destroyARMarkEngine(false);
						createARMarkEngineSd();
						_is_update_ar_mark_engine_sd.store(false);
					}
#ifdef ENABLE_AR
					if(_ar_mark_engine_sd && packet.metaDataValid()){
						Poco::JSON::Object root;			

						JoFmvMetaDataBasic meta_temp = packet.getMetaDataBasic();
						uint64_t timeStamp = meta_temp.CarrierVehiclePosInfo_p.TimeStamp;

						jo::JoARMetaDataBasic ar_meta_data{};
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

						int width = _codec_parameter.width;
						int height = _codec_parameter.height;										

						//先判断是否需要删除
						auto _ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.find(_mark_guid_sd);
						while (_ar_mark_pixel_and_geographic != _ar_mark_pixel_and_geographic_map.end() 
							&& _ar_mark_pixel_and_geographic->first == _mark_guid_sd)
						{
							_ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map_sd.erase(_ar_mark_pixel_and_geographic);
						}
						
						_mark_guid_sd = "";

						ArInfosInternal ar_infos_internal_to_push_rtc{};//存储以前的计算结果和当前新的标注要素，用来push rtc
						ArInfosInternal ar_infos_internal_to_store{};//这次如果有新增的标注元素，用这个先存下来，再放在map中

						//前面存的像素转地理，地理坐标每帧都要重新反算新的像素坐标;前面存的地理转像素，地理坐标每帧都要重新反算新的像素坐标
						for(auto iter_map = _ar_mark_pixel_and_geographic_map_sd.begin(); 
							iter_map != _ar_mark_pixel_and_geographic_map_sd.end(); iter_map++)
						{
							//地理坐标转图像坐标
							jo::GeographicPosition tmpPoint{};
							std::vector<jo::GeographicPosition> tmpPointVct{};
							ar_point points{};
							ar_line_or_region lines{};
							ar_line_or_region regions{};
							
							// ar_infos_internal_to_push_rtc.ArElementsNum += iter_map->second.ArElementsNum;
							
							for(auto iter_list : iter_map->second.ArElementsArray)
							{
								tmpPoint.longitude = iter_list.lon;
								tmpPoint.latitude = iter_list.lat;
								tmpPoint.altitude = iter_list.HMSL;

								if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) 
								{
									switch (iter_list.Type) {
									case 0: {								
										points.push_back(tmpPoint);
										break;
									}
									case 1: {
										//添加每条线的各个物点
										if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {									
											tmpPointVct.push_back(tmpPoint);
										}
										//当前线已结束，添加整条线
										if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {									
											lines.push_back(tmpPointVct);
											tmpPointVct.clear();
										}
										break;
									}
									case 2: {
										//添加每个面的各个物点
										if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
											tmpPointVct.push_back(tmpPoint);
										}
										//当前面已结束，添加整个面
										if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
											regions.push_back(tmpPointVct);
											tmpPointVct.clear();
										}
										break;
									}
									default:
										break;
									}
								}
								if (!points.empty() || !lines.empty() || !regions.empty()) 
								{
									calculatGeodeticToImageSd(iter_map->second.ArElementsArray, ar_infos_internal_to_push_rtc
										, iter_map->first, timeStamp, width, height, ar_meta_data, points, lines, regions);
								}
							}
						}

						while (!_video_mark_data_sd.empty()) {
							std::string mark_data = _video_mark_data_sd.front();
							_video_mark_data_sd.pop();
							auto is_hd = false;
							if(!_video_mark_data_sd_status.empty()){
								is_hd = _video_mark_data_sd_status.front();
								_video_mark_data_sd_status.pop();
							}
							float ratio_x = (!is_hd)? 1: (_codec_parameter.width == 0? 1: (float)_codec_parameter_sd.width/(float)_codec_parameter.width);
							float ratio_y = (!is_hd)? 1: (_codec_parameter.height == 0? 1: (float)_codec_parameter_sd.height/(float)_codec_parameter.height);

							Poco::JSON::Parser parser;
							try {
								Poco::Dynamic::Var result = parser.parse(mark_data);
								root = *result.extract<Poco::JSON::Object::Ptr>();
							}
							catch (const std::exception& e) {
								eap_error(e.what());
							}
							auto elementsArray = root.has("ArElementsArray") ? root.getArray("ArElementsArray"): Poco::JSON::Array::Ptr();
							int elementsArray_size = elementsArray->size();
							if (elementsArray_size <= 0) { continue; }

							jo::GeographicPosition tmpPoint{};
							std::vector<jo::GeographicPosition> tmpPointVct{};
							ar_point points{};
							ar_line_or_region lines{};
							ar_line_or_region regions{};
							
							//透传回云端
							_ar_mark_elements_guid_sd = root.has("Guid") ? root.getValue<std::string>("Guid") : "";					
							std::list<ArElementsInternal> ar_elements_array;
							for (int i = 0; i < (std::min)(elementsArray_size, 1024); ++i) {

								auto elementJs = elementsArray->getObject(i);

								//图像坐标转地理坐标
								if(elementJs->has("X") && elementJs->has("Y")) {
									cv::Point pixel_point;
									jo::GeographicPosition geoPos{};

									int pixelX = elementJs->getValue<int32_t>("X");
									int pixelY = elementJs->getValue<int32_t>("Y");

									pixel_point.x = pixelX * ratio_x;
									pixel_point.y = pixelY * ratio_y;
																
									if(_ar_mark_engine_sd){
										_ar_mark_engine_sd->projectImageToGeodetic(timeStamp, width, height, ar_meta_data, pixel_point, geoPos);
									}else{
										eap_error( "_ar_mark_engine_sd->projectImageToGeodetic but _ar_mark_engine is nullptr!----");
									}

									ArElementsInternal ar_elements_internal{};
									ar_elements_internal.X = pixel_point.x;
									ar_elements_internal.Y = pixel_point.y;
									ar_elements_internal.lat = geoPos.latitude;
									ar_elements_internal.lon = geoPos.longitude;
									ar_elements_internal.HMSL = geoPos.altitude;
									ar_elements_internal.Type = elementJs->getValue<int>("Type");
									ar_elements_internal.DotQuantity = elementJs->getValue<int>("DotQuantity");
									ar_elements_internal.Category = elementJs->getValue<int>("Category");
									ar_elements_internal.CurIndex = elementJs->getValue<int>("CurIndex");
									ar_elements_internal.NextIndex = elementJs->getValue<int>("NextIndex");
									ar_elements_internal.Guid = _ar_mark_elements_guid_sd;

									ar_infos_internal_to_store.ArElementsArray.push_back(ar_elements_internal);
									ar_infos_internal_to_store.ArElementsNum++;					
								}
														
								//地理坐标转图像坐标
								if(elementJs->has("lon") && elementJs->has("lat") && elementJs->has("HMSL")) {
									
									ArElementsInternal ar_elements_internal{};
									ar_elements_internal.Category = elementJs->getValue<int>("Category");
									ar_elements_internal.Type = elementJs->getValue<int>("Type");
									ar_elements_internal.DotQuantity = elementJs->getValue<int>("DotQuantity");
									ar_elements_internal.CurIndex = elementJs->getValue<int>("CurIndex");
									ar_elements_internal.NextIndex = elementJs->getValue<int>("NextIndex");
									ar_elements_internal.Guid = _ar_mark_elements_guid_sd;
									ar_elements_array.push_back(ar_elements_internal);

									tmpPoint.longitude = elementJs->getValue<double>("lon");
									tmpPoint.latitude = elementJs->getValue<double>("lat");
									tmpPoint.altitude = elementJs->getValue<double>("HMSL");
									if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) {
										switch(elementJs->getValue<uint32_t>("Type")) {
										case 0: {
											points.push_back(tmpPoint);
											break;
										}
										case 1: {
											//添加每条线的各个物点
											if (elementJs->getValue<int>("CurIndex") != 0 || (elementJs->getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
												tmpPointVct.push_back(tmpPoint);
											}
											//当前线已结束，添加整条线
											if (elementJs->getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
												lines.push_back(tmpPointVct);
												tmpPointVct.clear();
											}
											break;
										}
										case 2: {
											//添加每个面的各个物点
											if (elementJs->getValue<int>("CurIndex") != 0 || (elementJs->getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
												tmpPointVct.push_back(tmpPoint);
											}
											//当前面已结束，添加整个面
											if (elementJs->getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
												regions.push_back(tmpPointVct);
												tmpPointVct.clear();
											}
											break;
										}
										default:
											break;
										}
									}
								}
							}

							//地理坐标转图像坐标(地理坐标存完后一次计算)
							if (!points.empty() || !lines.empty() || !regions.empty()) {
								calculatGeodeticToImageSd(ar_elements_array, ar_infos_internal_to_store, _ar_mark_elements_guid_sd, 
									timeStamp, width, height, ar_meta_data, points, lines, regions);
							}
							
							//存储到需要push rt的容器中去
							ar_infos_internal_to_push_rtc.ArElementsNum += ar_infos_internal_to_store.ArElementsNum;
							for(auto iter : ar_infos_internal_to_store.ArElementsArray){
								ar_infos_internal_to_push_rtc.ArElementsArray.push_back(iter);
							}

							//存储当前有新增的标注要素
							_ar_mark_pixel_and_geographic_map.insert(std::make_pair(_ar_mark_elements_guid_sd, ar_infos_internal_to_store));
						
							//if (!_is_video_mark) { _is_video_mark.store(true); }
							// videoMarkMetaDataWriteSd(ar_infos_internal_to_store, packet.getOriginalPts());					
							// break;//???
						}
						packet.setArMarkInfos(ar_infos_internal_to_push_rtc);
					}
#endif
					ar_mark_promise.set_value();
				});

				std::promise<void> ar_promise{};
				auto ar_future = ar_promise.get_future();
				std::lock_guard<std::mutex> update_func_lock(_update_func_mutex);
				if(_is_ar_on_sd){
					createAREngineSd();
					ThreadPool::defaultPool().start([this, &packet, &ar_promise]()
					{
#ifdef ENABLE_AR
						if (_ar_engine_sd && packet.metaDataValid()) {
							if (_ar_image_compute_state_sd) {
								_ar_image_compute_state_sd = false;

								auto pts = packet->pts;//重写后的时间戳
								auto width = _codec_parameter.width;
								auto height = _codec_parameter.height;
								cv::Mat mat;
								JoFmvMetaDataBasic meta_temp = packet.getMetaDataBasic();

								jo::JoARMetaDataBasic ar_meta_data{};
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
								ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
								ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

								std::vector<cv::Point> pixel_points{};
								std::vector<std::vector<cv::Point>> pixel_lines{};

								std::vector<cv::Point> tmp_pixel_points{};
								std::vector<std::vector<cv::Point>> tmp_pixel_lines{};

								std::vector<std::vector<cv::Point>> pixel_warning_l1_regions{};
								std::vector<std::vector<cv::Point>> pixel_warning_l2_regions{};
								_ar_engine_sd->frameProcess(true, pts, mat, width, height, ar_meta_data, pixel_points, pixel_lines
									, pixel_warning_l1_regions, pixel_warning_l2_regions);
								for (const auto& iter : pixel_warning_l1_regions) {
									pixel_lines.push_back(iter);
								}
								for (const auto& iter : pixel_warning_l2_regions) {
									pixel_lines.push_back(iter);
								}

								// 筛选去掉屏幕外多余的点
								int i = 0;
								std::queue<int> ar_valid_point_index{}; // 筛选去掉屏幕外多余的点后有效点的索引
								for(const auto &iter : pixel_points){
									if(iter.x <= width  && iter.x >= 0 && iter.y <= height && iter.y >= 0){
										tmp_pixel_points.push_back(iter);
										ar_valid_point_index.push(i);
									}
									++i;
								}
								for(std::size_t index = 0; index < pixel_lines.size(); ++index){
									auto& iter_line = pixel_lines[index];

									std::vector<cv::Point> tmp_points{};
									
									bool is_in_screen{};
									int boundary=width/2;
									
									for(std::size_t i = 0; i < iter_line.size(); ++i) {
										auto& iter = iter_line[i];
										if(iter.x <= width+boundary && iter.x >= 0-boundary && iter.y <= height+boundary && iter.y >= 0-boundary){
											is_in_screen = true;
										}

										if (!is_in_screen) {
											continue;
										}
										is_in_screen = false;
										
										if (i) {
											auto& front_iter = iter_line[i - 1];
											if(front_iter.x > width+boundary || front_iter.x < 0-boundary || front_iter.y > height+boundary || front_iter.y < 0-boundary){
												tmp_points.push_back(front_iter);
											}
										}

										tmp_points.push_back(iter);
										
										if (i < iter_line.size() - 1) {
											auto& back_iter = iter_line[i + 1];
											//警戒线是个环，不能break
											if(back_iter.x > width+boundary || back_iter.x < 0-boundary || back_iter.y > height+boundary || back_iter.y < 0-boundary){
												tmp_points.push_back(back_iter);
											}
										}
									}

									if (tmp_points.empty() && _ar_image_compute_pixel_lines_sd.size() == pixel_lines.size()) {
										auto& tmp_line = _ar_image_compute_pixel_lines_sd[index];
										for (auto &iter : tmp_line) {
											tmp_points.push_back(iter);
										}
										
									}

									tmp_pixel_lines.push_back(tmp_points);
								}

								std::swap(_ar_valid_point_index_sd, ar_valid_point_index);
								std::swap(_ar_image_compute_pixel_points_sd, tmp_pixel_points);
								std::swap(_ar_image_compute_pixel_lines_sd, tmp_pixel_lines);
							}
							else{
								_ar_image_compute_state_sd = true;
							}
							
							packet.setArPixelLines(_ar_image_compute_pixel_lines_sd);
							packet.setArPixelPoints(_ar_image_compute_pixel_points_sd);
							packet.setArValidPointIndex(_ar_valid_point_index_sd);
							if(packet.getArVectorFile()!=_ar_vector_file){
								packet.setArVectorFile(_ar_vector_file);
							}
						}					
#endif
						ar_promise.set_value();
					});
				}
				else{
					destroyAREngine(false);
					ar_promise.set_value();
				}

				if(_is_update_func_sd){
					if(!_update_func_err_desc_sd.empty()){
						_update_func_err_desc_sd += std::string("update failed");
					}

					std::string id_temp = _id;
					NoticeCenter::Instance()->getCenter().postNotification(new FunctionUpdatedNotice( 
						id_temp, _update_func_result_sd, _update_func_err_desc_sd, _function_mask));

					_is_update_func_sd = false;
					_update_func_result_sd = true;
					_update_func_err_desc_sd.clear();
				}

				ar_future.wait_for(std::chrono::seconds(3));
				ar_mark_future.wait_for(std::chrono::seconds(3));

				//设置当前时间点			
				AVRational  dst_time_base = { 1, AV_TIME_BASE };
				int64_t _start_time_stamp = av_rescale_q(_start_time, dst_time_base, _timebase);
				int64_t current_time = (packet->pts - _start_time_stamp) * av_q2d(_timebase) * 1000.f;
				packet.setCurrentTime(current_time);

				encoder_packet_callback(packet);
	#endif // ENABLE_GPU
			};
			
			auto demuxer_stop_callback = [this](int exit_code) {
				_is_demuxer_closed_sd = true;
				if (!_is_manual_stoped) {
					ThreadPool::defaultPool().start([this, exit_code]()
					{
						std::string desc = "sd demuxer stoped, exit code: " + std::to_string(exit_code)
							+ std::string("; id: ") + _id;
						eap_error( desc);
						std::size_t index = _push_urls[0].rfind("/");
						std::size_t second_index = _push_urls[0].rfind("/", index - 1);
						std::string pilot_sn = _push_urls[0].substr(second_index + 1, index - second_index - 1);
						_notice_task_id = _id;
						_notice_task_internal_id = _id;
						NoticeCenter::Instance()->getCenter().postNotification(
							new TaskStopedNotice(_notice_task_id, desc, pilot_sn));
					});
				}
			};

	#ifdef ENABLE_GPU
			
			auto decoder_frame_callback = [this](Frame frame) {
				if(!_loop_sd_run){
					eap_warning("---_loop_sd_run is false");
					_decoded_sd_images_cv.notify_all();
					return;
				}

				{
					std::lock_guard<std::mutex> lock(_decoded_sd_images_mutex);
					if (_decoded_sd_images.size() > 5) {
						eap_error( "sd decoded images queue is full, drop frame");
						return;
					}
				}//AI、AR等其它功能处理不过来丢帧	


				CodecImagePtr decode_image = CodecImagePtr(new CodecImage());
#ifdef ENABLE_AIRBORNE
				try {// TODO: 增加判断条件，根据是否需要做ai相关来决定是否进行转换
					cv::Mat bgr240_image(frame->height, frame->width, CV_8UC3);

					cv::Mat yuv_data(frame->height * 3 / 2, frame->width, CV_8UC1);
					int image_size = frame->height * frame->width;
					memcpy(yuv_data.data, frame->data[0],  image_size);
					memcpy(yuv_data.data + image_size, frame->data[1], image_size / 4);
					memcpy(yuv_data.data + image_size * 5 / 4, frame->data[2], image_size / 4);
					
					I4202Bgr((char*)yuv_data.data, (char*)bgr240_image.data, pIn420Dev, pOutBgrDev, yuv_data.cols, yuv_data.rows * 2 / 3, 0, 0, frame->width, frame->height);
					
					if (!bgr240_image.empty()) {
						decode_image->bgr24_image = bgr240_image;
					}
				}
				catch (const std::exception& e) {
					eap_error_printf("frame 2 cpu_mat faile, error description: %s", e.what());
					return;
				}
#else
	#ifdef ENABLE_GPU
				cv::cuda::GpuMat bgr24_image_gpu;
				cv::cuda::GpuMat bgr32_image_gpu;
				try {
					bgr24_image_gpu = cv::cuda::GpuMat(frame->height, frame->width, CV_8UC3);
					bgr32_image_gpu = cv::cuda::GpuMat(frame->height, frame->width, CV_8UC4);

					ImageCvtColor::Instance()->nv12ToBgr24(frame, bgr24_image_gpu);
					ImageCvtColor::Instance()->bgr24ToBgr32(bgr24_image_gpu, bgr32_image_gpu);
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					return;
				}

				decode_image->bgr24_image = bgr24_image_gpu;
				bgr24_image_gpu.release();
				decode_image->bgr32_image = bgr32_image_gpu;
				bgr32_image_gpu.release();				
	#endif
#endif // ENABLE_AIRBORNE

				decode_image->meta_data.pts = frame->pts;
				auto sei_buffer = frame.getSeiBuf();
				if (!sei_buffer.empty()) {
					decode_image->meta_data.meta_data_raw_binary = sei_buffer;
				}
				decode_image->meta_data.meta_data_basic = frame.getMetaDataBasic();
				decode_image->meta_data.meta_data_valid = frame.getMetaDataValid();
				decode_image->meta_data.original_pts = frame.getOriginalPts();

				std::lock_guard<std::mutex> lock(_decoded_sd_images_mutex);
				_decoded_sd_images.push(decode_image);
				// eap_information( "---sd_decoded_images size push:---" << _decoded_images.size();
				_decoded_sd_images_cv.notify_all();
			};
	#endif // ENABLE_GPU
			auto pusher_stop_callback = [this](int ret, std::string err_str) {
				if(_is_demuxer_closed_sd)
					return;
				if(!_loop_sd_run){
					_decoded_sd_images_cv.notify_all();
					return;
				}
				std::lock_guard<std::mutex> lock(_pusher_sd_mutex);
				ThreadPool::defaultPool().start([this, ret, err_str]()
				{
					std::string desc = "sd pusher stoped, exit code: " + std::to_string(ret) + ", description: " + err_str;
					// 启动重连
					if (_is_push_sd_rtc) {
						if (_pusher_sd_rtc && _loop_sd_run) {
							_pusher_sd_rtc->close();
							_pusher_sd_rtc->open(_id, _push_urls[1], _timebase_sd, _framerate_sd,
							_codec_parameter_sd, std::chrono::milliseconds(3000));
							eap_information("reopen sd pusher_rtc");
						}
						
					}
					else {
						if (_pusher_sd_tradition && _loop_sd_run) {
							_pusher_sd_tradition->close();
							_pusher_sd_tradition->open(_id, _push_urls[1], _timebase_sd, _framerate_sd,
							_codec_parameter_sd, std::chrono::milliseconds(3000));
							eap_information("reopen sd pusher_tradition");
						}
					}
				});
				if(_loop_sd_run)
					std::this_thread::sleep_for(std::chrono::milliseconds{1000});
			};
			auto video_mark_data_callback = [this](std::string mark_data) {
	#if AR_MARK_USE_DATACHANNEL
				_video_mark_data.push(mark_data);
	#endif
			};

			if (_is_pull_sd_rtc) {
				_demuxer_sd_rtc = DemuxerRtc::createInstance();
				_demuxer_sd_rtc->setPacketCallback(demuxer_packet_callback);
				_demuxer_sd_rtc->setStopCallback(demuxer_stop_callback);
				_demuxer_sd_rtc->open(_pull_urls[1], std::chrono::milliseconds(3000));
				_codec_parameter_sd = _demuxer_sd_rtc->videoCodecParameters();
				_timebase_sd = _demuxer_sd_rtc->videoStreamTimebase();
				_framerate_sd = _demuxer_sd_rtc->videoFrameRate();
				_start_time_sd = _demuxer_sd_rtc->videoStartTime();
				_video_duration_sd = _demuxer_sd_rtc->videoDuration();
			}
			else {
				_demuxer_sd_tradition = DemuxerTradition::createInstance();
				_demuxer_sd_tradition->setPacketCallback(demuxer_packet_callback);
				_demuxer_sd_tradition->setStopCallback(demuxer_stop_callback);
				_demuxer_sd_tradition->open(_pull_urls[1], std::chrono::milliseconds(3000));
				_codec_parameter_sd = _demuxer_sd_tradition->videoCodecParameters();
				_timebase_sd = _demuxer_sd_tradition->videoStreamTimebase();
				_framerate_sd = _demuxer_sd_tradition->videoFrameRate();
				_start_time_sd = _demuxer_sd_tradition->videoStartTime();
				_video_duration_sd = _demuxer_sd_tradition->videoDuration();
			}

	#ifdef ENABLE_GPU
			{
				std::lock_guard<std::mutex> lock(_decoder_sd_mutex);
				_decoder_sd = Decoder::createInstance();
				_decoder_sd->setFrameCallback(decoder_frame_callback);
				_decoder_sd->open(_codec_parameter_sd, _framerate_sd, _pull_urls[1], _is_hardware_decode);
			}
	#endif // ENABLE_GPU
			
			// TODO:
	#ifdef ENABLE_GPU
			if (/*(_function_mask & FUNCTION_MASK_DEFOG) || (_function_mask & FUNCTION_MASK_STABLE)*/true) {
				std::lock_guard<std::mutex> lock(_encoder_sd_mutex);

				EncoderNVENC::InitParameter init_parameter;
				init_parameter.bit_rate = 5000000;
				init_parameter.dst_width = _codec_parameter_sd.width;
				init_parameter.dst_height = _codec_parameter_sd.height;
				init_parameter.time_base = _timebase_sd;
				init_parameter.framerate = _framerate_sd;
				init_parameter.start_time = _start_time_sd;
				
				_encoder_sd = EncoderNVENC::createInstance(init_parameter, encoder_packet_callback);
				_encoder_sd->start();

			}
	#endif // ENABLE_GPU

			if (_is_push_sd_rtc) {
				std::lock_guard<std::mutex> lock(_pusher_sd_mutex);
				_pusher_sd_rtc = PusherRtc::createInstance();
				_pusher_sd_rtc->updateVideoParams(_video_duration_sd);
				_pusher_sd_rtc->setStopCallback(pusher_stop_callback);
				_pusher_sd_rtc->setMarkDataCallback(video_mark_data_callback);
				//TODO: _codec_parameter: 如果编码，则需要用编码器的
				_pusher_sd_rtc->open(_id, _push_urls[1], _timebase_sd, _framerate_sd,
					_codec_parameter_sd, std::chrono::milliseconds(3000));
			}
			else {
				std::lock_guard<std::mutex> lock(_pusher_sd_mutex);
				_pusher_sd_tradition = PusherTradition::createInstance();
				_pusher_sd_tradition->setStopCallback(pusher_stop_callback);
				_pusher_sd_tradition->open(_id, _push_urls[1], _timebase_sd, _framerate_sd,
					_codec_parameter_sd, std::chrono::milliseconds(3000));
			}

			std::lock_guard<std::mutex> update_func_lock(_update_func_mutex_sd);
			createARMarkEngineSd();
			createAREngineSd();

			_is_ar_first_create_sd = false;

	#ifdef ENABLE_GPU
			createAIEngineSd();
	#ifndef ENABLE_AIRBORNE
			createSdImageEnhancer();
			createSdImageStabilizer();
	#endif

			_is_sd_image_enhancer_first_create = false;
			_is_sd_image_stable_first_create = false;
			_is_ai_first_create_sd = false;
			_loop_sd_thread = std::thread([this]() {
				_loop_sd_run = true;
				for (; _loop_sd_run;) {
					std::unique_lock<std::mutex> lock(_decoded_sd_images_mutex);
					if (_decoded_sd_images.empty()) {
						_decoded_sd_images_cv.wait_for(lock, std::chrono::milliseconds(500));
						if (!_loop_sd_run) {
							break;
						}
					}
					if (!_loop_sd_run) {
						break;
					}

					if (_decoded_sd_images.empty()) {
						continue;
					}

					auto image = _decoded_sd_images.front();				
					_decoded_sd_images.pop();
					//eap_information( "---_decoded_sd_images size pop:---" << _decoded_sd_images.size();
					lock.unlock();

					std::lock_guard<std::mutex> update_func_lock(_update_func_mutex_sd);
#ifndef ENABLE_AIRBORNE
					if (_snapshot_sd) { //快照
						//ThreadPool::defaultPool().start([this, image]() {
							GET_CONFIG(std::string, getString, uploadSnapshot, General::kUploadSnapshot);
							std::string pilot_sn{};
							//上传图片
							std::string base64_encoded{};
							cv::Mat bgr24_image_cpu;// 是否需要初始化
							try {
								bgr24_image_cpu.create(image->bgr24_image.size(), image->bgr24_image.type()); // 初始化 cpuImage
								image->bgr24_image.download(bgr24_image_cpu);
								std::vector<uchar> buffer;
								cv::imencode(".jpg", bgr24_image_cpu, buffer, { cv::IMWRITE_JPEG_QUALITY, 80 });
								base64_encoded = "data:image/jpg;base64," + encodeBase64({ buffer.begin(), buffer.end() });
								bgr24_image_cpu.release();

								std::size_t index = _push_urls[1].rfind("/");
								std::size_t second_index = _push_urls[1].rfind("/", index - 1);
								pilot_sn = _push_urls[1].substr(second_index + 1, index - second_index - 1);
							}
							catch (const std::exception& e) {
								bgr24_image_cpu.release();
								eap_error(std::string(e.what()));
								return;
							}
							Poco::JSON::Object json;
							json.set("file", base64_encoded);
							json.set("autopilotSn", pilot_sn);
							json.set("is_hd", false);
							json.set("record_no", _recordNo);
							json.set("code", 0);
							auto json_string = jsonToString(json);

							auto http_client = HttpClient::createInstance();
							http_client->doHttpRequest(uploadSnapshot + "/flightmonitor/custom/v1/file/addPhoto", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									try {
										Poco::JSON::Parser parser;
										auto dval = parser.parse(response);

										auto obj = dval.extract<Poco::JSON::Object::Ptr>();
										int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
										if (code != 0) {
											std::string msg = obj && obj->has("msg") ? obj->getValue<std::string>("msg") : "";
											eap_error_printf("sd snapshot post failed, http status=%d, return code=%d, msg=%s", status, code, msg);
										}
										else {
											eap_information("sd snapshot post succeed");
										}
									}
									catch (...) {
										eap_warning("sd addPhoto post failed");
									}
								}
								else {
									eap_warning("sd addPhoto post failed");
								}
							});
						//});
						_snapshot_sd = false;
					}
#endif
					// AI AR DEFOG等在这里调度
	#ifdef ENABLE_AI
					std::vector<joai::Result> ai_detect_ret{};

					std::promise<void> ai_promise;//void只用阻塞功能
					auto ai_future = ai_promise.get_future();

					if (_is_ai_on_sd) {
						createAIEngineSd();
						std::weak_ptr<DispatchTaskImplMultiple> this_weak_ptr = weak_from_this();
						ThreadPool::defaultPool().start([this, &image, &ai_promise, &ai_detect_ret, this_weak_ptr]()
						{
							auto this_shared_ptr = this_weak_ptr.lock();
							if (!this_shared_ptr) {
								ai_promise.set_value();
								return;
							}

							if (!_ai_object_detector_sd) {
								ai_promise.set_value();
								return;
							}
							
							auto detect_objects = _ai_object_detector_sd->detect(image->bgr24_image);
							image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
								ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = detect_objects.size();
							
							image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
								ImageProcessingBoardInfo_p.AiInfos_p.AiStatus = 1;

							int i = 0;
							for (auto object : detect_objects) {
								ai_detect_ret.push_back(object);

								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX =
									object.Bounding_box.x;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY =
									object.Bounding_box.y;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetWidth =
									object.Bounding_box.width;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetHeight =
									object.Bounding_box.height;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass =
									object.cls;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtConfidence =
									object.confidence;
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtSN =
									object.Frame_num;
									
								++i;
							}

							ai_promise.set_value();
						});
					}
					else {
						destroyAIEngine(false);
						ai_promise.set_value();
					}
	#endif
					
					std::promise<void> ar_promise{};
					auto ar_future = ar_promise.get_future();
					createAREngineSd();
					if (_is_ar_on_sd || _is_enhanced_ar_on_sd) {
						if(_is_enhanced_ar_on_sd){
							createAuxiliaryAIEngineSd();
						}
						std::weak_ptr<DispatchTaskImplMultiple> this_weak_ptr = weak_from_this();
						ThreadPool::defaultPool().start([this, &image, &ar_promise, this_weak_ptr]()
						{
							auto this_shared_ptr = this_weak_ptr.lock();
							if (!this_shared_ptr) {
								ar_promise.set_value();
								return;
							}
#ifdef ENABLE_AR
							executeSdARProcess(image);
#endif
							ar_promise.set_value();
							
						});
					}
					else {
#ifdef ENABLE_AI
						if(_aux_ai_object_detector_sd){
							destroyAuxiliaryAIEngine(false);
						}
#endif
						ar_promise.set_value();
					}				
#ifdef ENABLE_AR
					{
						//TODO: 先把标注拿出来改成同步,相当于也是异步，在主线程中和子线程异步

						//camera.config 有更新，就更新AR标注引擎
						if(_is_update_ar_mark_engine_sd){
							destroyARMarkEngine(false);
							createARMarkEngineSd();
							_is_update_ar_mark_engine_sd.store(false);
						}
						executeSdARMarkProcess(image);
					}
#endif
					
					cv::Mat M;
					bool is_stable_geted{};
					int64_t pts_temp{};
#ifndef ENABLE_AIRBORNE
					if (_is_image_stable_on_sd) {
						createSdImageStabilizer();

						if (_stabilizer_sd) {
							pts_temp = image->meta_data.pts;

							is_stable_geted = _stabilizer_sd->run(
								image->bgr32_image, image->meta_data.pts, M);
						}
					}
					else {
						destroyImageStabilizer(false);

						if (!_meta_data_cache_sd.empty()) {
							_meta_data_cache_sd.clear();
						}
					}

					if (_is_sd_image_enhancer_on) {
						do {
							if (_is_image_stable_on_sd && !is_stable_geted) {
								break;
							}
							createSdImageEnhancer();

							if (_enhancer_sd) {
								_enhancer_sd->Enhance(image->bgr32_image.data);
							}
						} while (false);
					}
					else {
						destroyImageEnhancer(false);
					}
	#ifdef ENABLE_AI
					std::future_status ai_future_status = ai_future.wait_for(std::chrono::seconds(3));
					if (ai_future_status == std::future_status::ready) {
					} else if (ai_future_status == std::future_status::timeout) {
						eap_warning("---ai_future--sd-Task did not complete within the timeout." );
					} else if (ai_future_status == std::future_status::deferred) {
						eap_warning("---ai_future-sd-Task is deferred.");
					}
	#endif
					std::future_status ar_future_status = ar_future.wait_for(std::chrono::seconds(3));
					if (ar_future_status == std::future_status::ready) {
					} else if (ar_future_status == std::future_status::timeout) {
						eap_warning("---ar_future-sd--Task did not complete within the timeout.") ;
					} else if (ar_future_status == std::future_status::deferred) {
						eap_warning("---ar_future-sd-Task is deferred.");
					}


					if(!_loop_sd_run)
						return;

					if (_is_image_stable_on_sd) {
						_meta_data_cache_sd[pts_temp] = image->meta_data;
						if (!is_stable_geted) {
							// _update_func_mutex.unlock();
							continue;
						}
					}

					if (_is_image_stable_on_sd && !M.empty()) {
						auto meta_data_it = _meta_data_cache_sd.find(image->meta_data.pts);
						if (meta_data_it != _meta_data_cache_sd.end()) {
							image->meta_data = meta_data_it->second;
							_meta_data_cache_sd.erase(meta_data_it->first);
						}
						else {
							image->meta_data.meta_data_valid = false;
						}

						std::promise<void> tracking_transform_promise{};
						std::promise<void> ar_transform_promise{};
						std::promise<void> ai_transform_promise{};

						auto tracking_transform_future = tracking_transform_promise.get_future();
						auto ar_transform_future = ar_transform_promise.get_future();
						auto ai_transform_future = ai_transform_promise.get_future();

						ThreadPool::defaultPool().start(
							[this, &image, &M, &tracking_transform_promise]() {
							trackingCoordTransform(image, M);
							tracking_transform_promise.set_value();
						});

						if (_is_ar_on_sd || _is_enhanced_ar_on_sd) {
							ThreadPool::defaultPool().start(
								[this, &image, &M, &ar_transform_promise]() {
								arCoordTransform(image, M);
								ar_transform_promise.set_value();
							});
						}
						else {
							ar_transform_promise.set_value();
						}
						
						if (_is_ai_on_sd) {
							ThreadPool::defaultPool().start(
								[this, &image, &M, &ai_transform_promise]() {
								aiCoordTransform(image, M);
								ai_transform_promise.set_value();
							});
						}
						else {
							ai_transform_promise.set_value();
						}
						
						tracking_transform_future.wait_for(std::chrono::seconds(3));
						ar_transform_future.wait_for(std::chrono::seconds(3));
						ai_transform_future.wait_for(std::chrono::seconds(3));
						if(!_loop_sd_run)
							return;
					}
#endif
	#if defined(ENABLE_AI) && defined(ENABLE_AR)				
					auto now = std::chrono::system_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _upload_image_time_point_sd).count();

					bool track_switch{};
					int frequency{};
					std::string danger_photo_server_url{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
						GET_CONFIG(int, getInt, my_frequency, General::kUploadImageTimeDuration);
						GET_CONFIG(std::string, getString, my_danger_photo_server_url, AI::KDangerPhotoServerUrl);
						track_switch = my_track_switch;
						frequency = my_frequency;
						danger_photo_server_url = my_danger_photo_server_url;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}
					
					if (!ai_detect_ret.empty() && _ar_engine_sd && duration > frequency * 1000) {
						_upload_image_time_point_sd = std::chrono::system_clock::now();
						std::lock_guard<std::mutex> lock(_danger_queue_mutex_sd);
						if (_danger_images_sd.size() >= 6) {
							_danger_images_sd.pop();
							_danger_ai_ret_sd.pop();
						}
						_danger_images_sd.push(image);
						_danger_ai_ret_sd.push(ai_detect_ret);
						_danger_queue_cv_sd.notify_all();
					}
	#endif // !_WIN32
					std::weak_ptr<DispatchTaskImplMultiple> this_weak_ptr = weak_from_this();
					ThreadPool::defaultPool().start([this, image, this_weak_ptr]()
					{
						auto this_shared_ptr = this_weak_ptr.lock();
						if (!this_shared_ptr) {
							return;
						}

						std::lock_guard<std::mutex> lock_encoder(_encoder_sd_mutex);

						if (_encoder_sd) {
							_encoder_sd->updateFrame(*image);//编码太快，现在加了AR计算耗时太多？导致GPU显存上涨
						}
					});
					
					if(_is_update_func_sd){
						if(!_update_func_err_desc_sd.empty()){
							_update_func_err_desc_sd += std::string("update failed");
						}

						std::string id_temp = _id;
						NoticeCenter::Instance()->getCenter().postNotification(new FunctionUpdatedNotice(
							id_temp, _update_func_result_sd, _update_func_err_desc_sd, _function_mask));

						_is_update_func_sd = false;
						_update_func_result_sd = true;
						_update_func_err_desc_sd.clear();
					}
					// _update_func_mutex.unlock();
				}
			});
	#endif // ENABLE_GPU
			}catch(const std::exception& e){
				if(_exception_callback)
					_exception_callback(_id, std::string(e.what()));
			}
		}
		#ifdef ENABLE_AR
		void DispatchTaskImplMultiple::hdDangerLoopThread()
		{
#ifdef ENABLE_AI
			_danger_photo_loop_thread = std::thread([this]() {
				if (_is_manual_stoped)
					return;
				_danger_photo_loop_thread_run.store(true);
				for (; _danger_photo_loop_thread_run;) {
					std::unique_lock<std::mutex> lock(_danger_queue_mutex);
					if (_danger_images.empty()) { // 两个队列都是同时往里放数据，判断其中任一即可
						_danger_queue_cv.wait_for(lock, std::chrono::milliseconds(200));
					}

					if (!_danger_photo_loop_thread_run || _is_manual_stoped) {
						break;
					}
					if (_danger_images.empty()) {
						continue;
					}

					auto image = _danger_images.front();
					_danger_images.pop();
					auto ai_detect_ret = _danger_ai_ret.front();
					_danger_ai_ret.pop();
					lock.unlock();
					//上传图片
					std::string base64_encoded{};
					cv::Mat bgr24_image_cpu;
					try {
						image->bgr24_image.download(bgr24_image_cpu);
						std::vector<uchar> buffer;
						cv::imencode(".jpg", bgr24_image_cpu, buffer, { cv::IMWRITE_JPEG_QUALITY, 80 });
						std::string str(buffer.begin(), buffer.end());
						base64_encoded = encodeBase64(str);
						base64_encoded = "data:image/jpg;base64," + base64_encoded;
					}
					catch (const std::exception& e) {
						_upload_image_is_done = true;
						eap_error(e.what());
						continue;
					}
					std::vector<cv::Rect> cv_rect{};
					std::vector<jo::WarningInfo> warning_info{};
					if (image->meta_data.meta_data_valid) {
						for (const auto& ret : ai_detect_ret) {
							cv_rect.push_back(ret.Bounding_box);
						}

						jo::JoARMetaDataBasic ar_meta_data{};
						JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

						int width = image->bgr24_image.cols;
						int height = image->bgr24_image.rows;
						warning_info = _ar_engine->getWarningInfo(cv_rect, width, height, ar_meta_data);
						Poco::JSON::Array json_warning_info_array;
						for (int i = 0; i < std::min(warning_info.size(), ai_detect_ret.size()); ++i) {
							jo::WarningInfo warning_info_elem = warning_info.at(i);

							if (isnanf(warning_info_elem.target_position.longitude) || isnanf(warning_info_elem.target_position.latitude)) {
								continue;
							}
							if (warning_info_elem.target_position.longitude == 0 || warning_info_elem.target_position.latitude == 0) {
								continue;
							}
							Poco::JSON::Object elementJs;
							elementJs.set("warningLevel", warning_info_elem.warning_level);
							Poco::JSON::Array wargetPositionArr, pixelPositionArr;
							wargetPositionArr.add((warning_info_elem.target_position.longitude));
							wargetPositionArr.add((warning_info_elem.target_position.latitude));
							wargetPositionArr.add((warning_info_elem.target_position.altitude));
							elementJs.set("wargetPosition", wargetPositionArr);

							auto object = ai_detect_ret[i];
							elementJs.set("identifyType", object.cls);
							elementJs.set("reliability", object.confidence);
							pixelPositionArr.add(object.Bounding_box.x);
							pixelPositionArr.add(object.Bounding_box.y);
							pixelPositionArr.add(object.Bounding_box.width);
							pixelPositionArr.add(object.Bounding_box.height);
							elementJs.set("pixelPosition", pixelPositionArr);
							json_warning_info_array.add(elementJs);
						}

						if (json_warning_info_array.empty()) {
							eap_warning("hd json_warning_info_array is empty");
							continue;
						}

						std::size_t index = _push_urls[0].rfind("/");
						std::size_t second_index = _push_urls[0].rfind("/", index - 1);
						std::string pilot_sn = _push_urls[0].substr(second_index + 1, index - second_index - 1);

						Poco::JSON::Object json;
						json.set("file", base64_encoded);
						json.set("autopilotSn", pilot_sn);
						json.set("warningInfo", json_warning_info_array);
						json.set("gimbal_pan", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan * 1e-4);
						json.set("gimbal_tilt", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt * 1e-4);
						json.set("gimbal_roll", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll * 1e-4);
						json.set("vehicle_lat", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat * 1e-7);
						json.set("vehicle_lon", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon * 1e-7);
						json.set("vehicle_hmsl", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL * 1e-2);
						json.set("vehicle_heading", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle * 1e-4);
						json.set("vehicle_pitch", meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle * 1e-4);
						json.set("vehicle_roll", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle * 1e-4);
						json.set("frame_roll", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll * 1e-4);
						json.set("frame_pan", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan * 1e-4);
						json.set("frame_tilt", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt * 1e-4);
						json.set("visual_horitontal", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal * 1e-4 * 180.0 / M_PI);
						json.set("tgt_hmsl", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL);

						std::string danger_photo_server_url{};
						try {
							GET_CONFIG(std::string, getString, my_danger_photo_server_url, AI::KDangerPhotoServerUrl);
							danger_photo_server_url = my_danger_photo_server_url;
						}
						catch (const std::exception& e) {
							eap_error_printf("get config throw exception: %s", e.what());
						}
						auto http_client = HttpClient::createInstance();
						http_client->doHttpRequest(danger_photo_server_url + "/dataManagement/v1/fileOperate/addDangerPhoto", jsonToString(json), [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
							if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
								try {
									Poco::JSON::Parser parser;
									auto dval = parser.parse(response);
									auto obj = dval.extract<Poco::JSON::Object::Ptr>();
									int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
									if (code != 200) {
										std::string msg = obj->getValue<std::string>("msg");
										eap_error_printf("addDangerPhoto post failed, http status=%d, return code=%d, msg=%s", (int)status, code, msg);
									} else {
										eap_information("addDangerPhoto post succeed");
									}
								} catch (...) {
									eap_warning("addDangerPhoto post failed");
								}
							} else {
								eap_warning("addDangerPhoto post failed");
							}
						});
					}
				}
			});
#endif //  ENABLE_AI
		}
		
		void DispatchTaskImplMultiple::sdDangerLoopThread()
		{
#ifdef ENABLE_AI
			_danger_photo_loop_thread_sd = std::thread([this]() {
				if (_is_manual_stoped)
					return;
				_danger_photo_loop_thread_run_sd.store(true);
				for (; _danger_photo_loop_thread_run_sd;) {
					std::unique_lock<std::mutex> lock(_danger_queue_mutex_sd);
					if (_danger_images_sd.empty()) { // 两个队列都是同时往里放数据，判断其中任一即可
						_danger_queue_cv_sd.wait_for(lock, std::chrono::milliseconds(200));
					}

					if (!_danger_photo_loop_thread_run_sd) {
						break;
					}

					if (_danger_images_sd.empty()) {
						continue;
					}

					auto image = _danger_images_sd.front();
					_danger_images_sd.pop();
					auto ai_detect_ret = _danger_ai_ret_sd.front();
					_danger_ai_ret_sd.pop();
					lock.unlock();
					std::string danger_photo_server_url{};
					try {
						GET_CONFIG(std::string, getString, my_danger_photo_server_url, AI::KDangerPhotoServerUrl);
						danger_photo_server_url = my_danger_photo_server_url;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}
					auto http_client = HttpClient::createInstance();
					//上传图片
					std::string base64_encoded{};
					cv::Mat bgr24_image_cpu;
					try {
						image->bgr24_image.download(bgr24_image_cpu);
						std::vector<uchar> buffer;
						cv::imencode(".jpg", bgr24_image_cpu, buffer, { cv::IMWRITE_JPEG_QUALITY, 80 });
						std::string str(buffer.begin(), buffer.end());
						base64_encoded = encodeBase64(str);
						base64_encoded = "data:image/jpg;base64," + base64_encoded;
					}
					catch (const std::exception& e) {
						_upload_image_is_done_sd = true;
						eap_error(std::string(e.what()));
						continue;
					}
					std::vector<cv::Rect> cv_rect{};
					std::vector<jo::WarningInfo> warning_info{};

					if (image->meta_data.meta_data_valid) {

						for (const auto& ret : ai_detect_ret) {
							cv_rect.push_back(ret.Bounding_box);
						}

						jo::JoARMetaDataBasic ar_meta_data{};
						JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

						int width = image->bgr24_image.cols;
						int height = image->bgr24_image.rows;
						warning_info = _ar_engine_sd->getWarningInfo(cv_rect, width, height, ar_meta_data);
						Poco::JSON::Array json_warning_info_array;
						for (int i = 0; i < std::min(warning_info.size(), ai_detect_ret.size()); ++i) {
							jo::WarningInfo warning_info_elem = warning_info.at(i);

							if (isnanf(warning_info_elem.target_position.longitude) || isnanf(warning_info_elem.target_position.latitude)) {
								continue;
							}
							if (warning_info_elem.target_position.longitude == 0 || warning_info_elem.target_position.latitude == 0) {
								continue;
							}

							Poco::JSON::Object elementJs;
							elementJs.set("warningLevel", warning_info_elem.warning_level);
							Poco::JSON::Array wargetPositionArr, pixelPositionArr;
							wargetPositionArr.add((warning_info_elem.target_position.longitude));
							wargetPositionArr.add((warning_info_elem.target_position.latitude));
							wargetPositionArr.add((warning_info_elem.target_position.altitude));
							elementJs.set("wargetPosition", wargetPositionArr);

							auto object = ai_detect_ret[i];
							elementJs.set("identifyType", object.cls);
							elementJs.set("reliability", object.confidence);
							pixelPositionArr.add(object.Bounding_box.x);
							pixelPositionArr.add(object.Bounding_box.y);
							pixelPositionArr.add(object.Bounding_box.width);
							pixelPositionArr.add(object.Bounding_box.height);
							elementJs.set("pixelPosition", pixelPositionArr);
							json_warning_info_array.add(elementJs);
						}

						if (json_warning_info_array.empty()) {
							continue;
						}

						std::size_t index = _push_urls[1].rfind("/");
						std::size_t second_index = _push_urls[1].rfind("/", index - 1);
						std::string pilot_sn = _push_urls[1].substr(second_index + 1, index - second_index - 1);

						Poco::JSON::Object json;
						json.set("file", base64_encoded);
						json.set("autopilotSn", pilot_sn);
						json.set("warningInfo", json_warning_info_array);
						json.set("gimbal_pan", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan * 1e-4);
						json.set("gimbal_tilt", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt * 1e-4);
						json.set("gimbal_roll", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll * 1e-4);
						json.set("vehicle_lat", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat * 1e-7);
						json.set("vehicle_lon", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon * 1e-7);
						json.set("vehicle_hmsl", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL * 1e-2);
						json.set("vehicle_heading", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle * 1e-4);
						json.set("vehicle_pitch", meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle * 1e-4);
						json.set("vehicle_roll", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle * 1e-4);
						json.set("frame_roll", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll * 1e-4);
						json.set("frame_pan", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan * 1e-4);
						json.set("frame_tilt", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt * 1e-4);
						json.set("visual_horitontal", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal * 1e-4 * 180.0 / M_PI);
						json.set("tgt_hmsl", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL);

						http_client->doHttpRequest(danger_photo_server_url + "/dataManagement/v1/fileOperate/addDangerPhoto", jsonToString(json), [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
							if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
								try {
									Poco::JSON::Parser parser;
									auto dval = parser.parse(response);

									auto obj = dval.extract<Poco::JSON::Object::Ptr>();
									int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
									if (code != 200) {
										std::string msg = obj->getValue<std::string>("msg");
										eap_error_printf("addDangerPhoto post failed, http status=%d, return code=%d, msg=%s", (int)status, code, msg);
									}
									else {
										eap_information("addDangerPhoto post succeed");
									}
								}
								catch (...) {
									eap_warning("addDangerPhoto post failed");
								}
							}
							else {
								eap_warning("addDangerPhoto post failed");
							}
						});
					}
				}
			});
#endif //  ENABLE_AI		
		}
		
		void DispatchTaskImplMultiple::executeHdARMarkProcess(CodecImagePtr image)
		{
			if (_ar_mark_engine && image->meta_data.meta_data_valid) {
				Poco::JSON::Object root;
				Poco::JSON::Parser reader;

				JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
				uint64_t timeStamp = meta_temp.CarrierVehiclePosInfo_p.TimeStamp;

				jo::JoARMetaDataBasic ar_meta_data{};
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

				int width = _codec_parameter.width;
				int height = _codec_parameter.height;

				//先判断是否需要删除
				auto _ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.find(_mark_guid);
				while (_ar_mark_pixel_and_geographic != _ar_mark_pixel_and_geographic_map.end()
					&& _ar_mark_pixel_and_geographic->first == _mark_guid)
				{
					_ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.erase(_ar_mark_pixel_and_geographic);
				}

				_mark_guid = "";

				ArInfosInternal ar_infos_internal_to_push_rtc{};//存储以前的计算结果和当前新的标注要素，用来push rtc
				ArInfosInternal ar_infos_internal_to_store{};//这次如果有新增的标注元素，用这个先存下来，再放在map中

				//前面存的像素转地理，地理坐标每帧都要重新反算新的像素坐标;前面存的地理转像素，地理坐标每帧都要重新反算新的像素坐标
				for (auto iter_map = _ar_mark_pixel_and_geographic_map.begin();
					iter_map != _ar_mark_pixel_and_geographic_map.end(); iter_map++)
				{
					//地理坐标转图像坐标
					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					ar_point points{};
					ar_line_or_region lines{};
					ar_line_or_region regions{};

					for (auto iter_list : iter_map->second.ArElementsArray)
					{
						tmpPoint.longitude = iter_list.lon;
						tmpPoint.latitude = iter_list.lat;
						tmpPoint.altitude = iter_list.HMSL;

						if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0)
						{
							switch (iter_list.Type) {
							case 0: {
								points.push_back(tmpPoint);
								break;
							}
							case 1: {
								//添加每条线的各个物点
								if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
									tmpPointVct.push_back(tmpPoint);
								}
								//当前线已结束，添加整条线
								if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
									lines.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							case 2: {
								//添加每个面的各个物点
								if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
									tmpPointVct.push_back(tmpPoint);
								}
								//当前面已结束，添加整个面
								if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
									regions.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							default:
								break;
							}
						}
						if (!points.empty() || !lines.empty() || !regions.empty())
						{
							calculatGeodeticToImage(iter_map->second.ArElementsArray, ar_infos_internal_to_push_rtc
								, iter_map->first, timeStamp, width, height, ar_meta_data, points, lines, regions);
						}
					}
				}

				float ratio_x = (_codec_parameter_sd.width == 0 ? 1 : (float)_codec_parameter.width / (float)_codec_parameter_sd.width);
				float ratio_y = (_codec_parameter_sd.height == 0 ? 1 : (float)_codec_parameter.height / (float)_codec_parameter_sd.height);
				if (ratio_x <= 0)
					ratio_x = 1;
				if (ratio_y <= 0)
					ratio_y = 1;
				while (!_video_mark_data.empty()) {
					std::string mark_data = _video_mark_data.front();
					_video_mark_data.pop();
					auto is_hd = true;
					if (!_video_mark_data_status.empty()) {
						is_hd = _video_mark_data_status.front();
						_video_mark_data_status.pop();
					}

					Poco::Dynamic::Var result = reader.parse(mark_data);
					root = *(result.extract<Poco::JSON::Object::Ptr>());
					Poco::JSON::Array elementsArray = root.has("ArElementsArray") ? *(root.getArray("ArElementsArray")) : Poco::JSON::Array();
					int elementsArray_size = elementsArray.size();
					if (elementsArray_size <= 0)
					{
						continue;
					}

					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					ar_point points{};
					ar_line_or_region lines{};
					ar_line_or_region regions{};

					//透传回云端
					_ar_mark_elements_guid = root.has("Guid") ? root.getValue<std::string>("Guid") : "";
					std::list<ArElementsInternal> ar_elements_array;
					for (int i = 0; i < (std::min)(elementsArray_size, 1024); ++i) {

						auto elementJs = elementsArray.getObject(i);

						//图像坐标转地理坐标
						if (elementJs->has("X") && elementJs->has("Y")) {
							cv::Point pixel_point;
							jo::GeographicPosition geoPos{};

							int pixelX = elementJs->getValue<int32_t>("X");
							int pixelY = elementJs->getValue<int32_t>("Y");

							pixel_point.x = pixelX * ((is_hd) ? 1 : ratio_x);
							pixel_point.y = pixelY * ((is_hd) ? 1 : ratio_y);

							if (_ar_mark_engine) {
								_ar_mark_engine->projectImageToGeodetic(timeStamp, width, height, ar_meta_data, pixel_point, geoPos);
							}
							else {
								eap_error("_ar_mark_engine->projectImageToGeodetic but _ar_mark_engine is nullptr!----");
							}

							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = pixel_point.x;
							ar_elements_internal.Y = pixel_point.y;
							ar_elements_internal.lat = geoPos.latitude;
							ar_elements_internal.lon = geoPos.longitude;
							ar_elements_internal.HMSL = geoPos.altitude;
							ar_elements_internal.Type = elementJs->getValue<uint32_t>("Type");
							ar_elements_internal.DotQuantity = elementJs->getValue<uint32_t>("DotQuantity");
							ar_elements_internal.Category = elementJs->getValue<uint32_t>("Category");
							ar_elements_internal.CurIndex = elementJs->getValue<uint32_t>("CurIndex");
							ar_elements_internal.NextIndex = elementJs->getValue<uint32_t>("NextIndex");
							ar_elements_internal.Guid = _ar_mark_elements_guid;

							ar_infos_internal_to_store.ArElementsArray.push_back(ar_elements_internal);
							ar_infos_internal_to_store.ArElementsNum++;
						}

						//地理坐标转图像坐标
						if (elementJs->has("lon") && elementJs->has("lat") && elementJs->has("HMSL")) {

							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.Category = elementJs->getValue<uint32_t>("Category");
							ar_elements_internal.Type = elementJs->getValue<uint32_t>("Type");
							ar_elements_internal.DotQuantity = elementJs->getValue<uint32_t>("DotQuantity");
							ar_elements_internal.CurIndex = elementJs->getValue<uint32_t>("CurIndex");
							ar_elements_internal.NextIndex = elementJs->getValue<uint32_t>("NextIndex");
							ar_elements_internal.Guid = _ar_mark_elements_guid;
							ar_elements_array.push_back(ar_elements_internal);

							tmpPoint.longitude = elementJs->getValue<double>("lon");
							tmpPoint.latitude = elementJs->getValue<double>("lat");
							tmpPoint.altitude = elementJs->getValue<double>("HMSL");
							if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) {
								switch (elementJs->getValue<uint32_t>("Type")) {
								case 0: {
									points.push_back(tmpPoint);
									break;
								}
								case 1: {
									//添加每条线的各个物点
									if (ar_elements_internal.CurIndex != 0 || (ar_elements_internal.CurIndex == 0 && tmpPointVct.empty())) {
										tmpPointVct.push_back(tmpPoint);
									}
									//当前线已结束，添加整条线
									if (ar_elements_internal.NextIndex == 0 && !tmpPointVct.empty()) {
										lines.push_back(tmpPointVct);
										tmpPointVct.clear();
									}
									break;
								}
								case 2: {
									//添加每个面的各个物点
									if (ar_elements_internal.CurIndex != 0 || (ar_elements_internal.CurIndex == 0 && tmpPointVct.empty())) {
										tmpPointVct.push_back(tmpPoint);
									}
									//当前面已结束，添加整个面
									if (ar_elements_internal.NextIndex == 0 && !tmpPointVct.empty()) {
										regions.push_back(tmpPointVct);
										tmpPointVct.clear();
									}
									break;
								}
								default:
									break;
								}
							}
						}
					}

					//地理坐标转图像坐标(地理坐标存完后一次计算)
					if (!points.empty() || !lines.empty() || !regions.empty()) {
						calculatGeodeticToImage(ar_elements_array, ar_infos_internal_to_store, _ar_mark_elements_guid,
							timeStamp, width, height, ar_meta_data, points, lines, regions);
					}

					//存储到需要push rt的容器中去
					ar_infos_internal_to_push_rtc.ArElementsNum += ar_infos_internal_to_store.ArElementsNum;
					for (auto iter : ar_infos_internal_to_store.ArElementsArray) {
						ar_infos_internal_to_push_rtc.ArElementsArray.push_back(iter);
					}

					//存储当前有新增的标注要素
					_ar_mark_pixel_and_geographic_map.insert(std::make_pair(_ar_mark_elements_guid, ar_infos_internal_to_store));
				}
				image->meta_data.ar_mark_info = ar_infos_internal_to_push_rtc;
			}
		}
		void DispatchTaskImplMultiple::executeSdARMarkProcess(CodecImagePtr image)
		{
			if (_ar_mark_engine_sd && image->meta_data.meta_data_valid) {
				Poco::JSON::Object root;

				JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
				uint64_t timeStamp = meta_temp.CarrierVehiclePosInfo_p.TimeStamp;

				jo::JoARMetaDataBasic ar_meta_data{};
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

				int width = _codec_parameter.width;
				int height = _codec_parameter.height;

				//先判断是否需要删除
				auto _ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map_sd.find(_mark_guid_sd);
				while (_ar_mark_pixel_and_geographic != _ar_mark_pixel_and_geographic_map_sd.end()
					&& _ar_mark_pixel_and_geographic->first == _mark_guid_sd)
				{
					_ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map_sd.erase(_ar_mark_pixel_and_geographic);
				}

				_mark_guid_sd = "";

				ArInfosInternal ar_infos_internal_to_push_rtc{};//存储以前的计算结果和当前新的标注要素，用来push rtc
				ArInfosInternal ar_infos_internal_to_store{};//这次如果有新增的标注元素，用这个先存下来，再放在map中

				//前面存的像素转地理，地理坐标每帧都要重新反算新的像素坐标;前面存的地理转像素，地理坐标每帧都要重新反算新的像素坐标
				for (auto iter_map = _ar_mark_pixel_and_geographic_map_sd.begin();
					iter_map != _ar_mark_pixel_and_geographic_map_sd.end(); iter_map++)
				{
					//地理坐标转图像坐标
					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					ar_point points{};
					ar_line_or_region lines{};
					ar_line_or_region regions{};

					// ar_infos_internal_to_push_rtc.ArElementsNum += iter_map->second.ArElementsNum;

					for (auto iter_list : iter_map->second.ArElementsArray)
					{
						tmpPoint.longitude = iter_list.lon;
						tmpPoint.latitude = iter_list.lat;
						tmpPoint.altitude = iter_list.HMSL;

						if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0)
						{
							switch (iter_list.Type) {
							case 0: {
								points.push_back(tmpPoint);
								break;
							}
							case 1: {
								//添加每条线的各个物点
								if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
									tmpPointVct.push_back(tmpPoint);
								}
								//当前线已结束，添加整条线
								if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
									lines.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							case 2: {
								//添加每个面的各个物点
								if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
									tmpPointVct.push_back(tmpPoint);
								}
								//当前面已结束，添加整个面
								if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
									regions.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							default:
								break;
							}
						}
						if (!points.empty() || !lines.empty() || !regions.empty())
						{
							calculatGeodeticToImageSd(iter_map->second.ArElementsArray, ar_infos_internal_to_push_rtc
								, iter_map->first, timeStamp, width, height, ar_meta_data, points, lines, regions);
						}
					}
				}

				while (!_video_mark_data_sd.empty()) {
					std::string mark_data = _video_mark_data_sd.front();
					_video_mark_data_sd.pop();
					auto is_hd = false;
					if (!_video_mark_data_sd_status.empty()) {
						is_hd = _video_mark_data_sd_status.front();
						_video_mark_data_sd_status.pop();
					}
					float ratio_x = (!is_hd) ? 1 : (_codec_parameter.width == 0 ? 1 : (float)_codec_parameter_sd.width / (float)_codec_parameter.width);
					float ratio_y = (!is_hd) ? 1 : (_codec_parameter.height == 0 ? 1 : (float)_codec_parameter_sd.height / (float)_codec_parameter.height);

					Poco::JSON::Parser parser;
					try {
						Poco::Dynamic::Var result = parser.parse(mark_data);
						root = *result.extract<Poco::JSON::Object::Ptr>();
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
					auto elementsArray = root.has("ArElementsArray") ? root.getArray("ArElementsArray") : Poco::JSON::Array::Ptr();
					int elementsArray_size = elementsArray->size();
					if (elementsArray_size <= 0) { continue; }

					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					ar_point points{};
					ar_line_or_region lines{};
					ar_line_or_region regions{};

					//透传回云端
					_ar_mark_elements_guid_sd = root.has("Guid") ? root.getValue<std::string>("Guid") : "";
					std::list<ArElementsInternal> ar_elements_array;
					for (int i = 0; i < (std::min)(elementsArray_size, 1024); ++i) {

						auto elementJs = elementsArray->getObject(i);

						//图像坐标转地理坐标
						if (elementJs->has("X") && elementJs->has("Y")) {
							cv::Point pixel_point;
							jo::GeographicPosition geoPos{};

							int pixelX = elementJs->getValue<int32_t>("X");
							int pixelY = elementJs->getValue<int32_t>("Y");

							pixel_point.x = pixelX * ratio_x;
							pixel_point.y = pixelY * ratio_y;

							if (_ar_mark_engine_sd) {
								_ar_mark_engine_sd->projectImageToGeodetic(timeStamp, width, height, ar_meta_data, pixel_point, geoPos);
							}
							else {
								eap_error("_ar_mark_engine_sd->projectImageToGeodetic but _ar_mark_engine is nullptr!----");
							}

							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = pixel_point.x;
							ar_elements_internal.Y = pixel_point.y;
							ar_elements_internal.lat = geoPos.latitude;
							ar_elements_internal.lon = geoPos.longitude;
							ar_elements_internal.HMSL = geoPos.altitude;
							ar_elements_internal.Type = elementJs->getValue<int>("Type");
							ar_elements_internal.DotQuantity = elementJs->getValue<int>("DotQuantity");
							ar_elements_internal.Category = elementJs->getValue<int>("Category");
							ar_elements_internal.CurIndex = elementJs->getValue<int>("CurIndex");
							ar_elements_internal.NextIndex = elementJs->getValue<int>("NextIndex");
							ar_elements_internal.Guid = _ar_mark_elements_guid_sd;

							ar_infos_internal_to_store.ArElementsArray.push_back(ar_elements_internal);
							ar_infos_internal_to_store.ArElementsNum++;
						}

						//地理坐标转图像坐标
						if (elementJs->has("lon") && elementJs->has("lat") && elementJs->has("HMSL")) {

							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.Category = elementJs->getValue<int>("Category");
							ar_elements_internal.Type = elementJs->getValue<int>("Type");
							ar_elements_internal.DotQuantity = elementJs->getValue<int>("DotQuantity");
							ar_elements_internal.CurIndex = elementJs->getValue<int>("CurIndex");
							ar_elements_internal.NextIndex = elementJs->getValue<int>("NextIndex");
							ar_elements_internal.Guid = _ar_mark_elements_guid_sd;
							ar_elements_array.push_back(ar_elements_internal);

							tmpPoint.longitude = elementJs->getValue<double>("lon");
							tmpPoint.latitude = elementJs->getValue<double>("lat");
							tmpPoint.altitude = elementJs->getValue<double>("HMSL");
							if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) {
								switch (elementJs->getValue<uint32_t>("Type")) {
								case 0: {
									points.push_back(tmpPoint);
									break;
								}
								case 1: {
									//添加每条线的各个物点
									if (elementJs->getValue<int>("CurIndex") != 0 || (elementJs->getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
										tmpPointVct.push_back(tmpPoint);
									}
									//当前线已结束，添加整条线
									if (elementJs->getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
										lines.push_back(tmpPointVct);
										tmpPointVct.clear();
									}
									break;
								}
								case 2: {
									//添加每个面的各个物点
									if (elementJs->getValue<int>("CurIndex") != 0 || (elementJs->getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
										tmpPointVct.push_back(tmpPoint);
									}
									//当前面已结束，添加整个面
									if (elementJs->getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
										regions.push_back(tmpPointVct);
										tmpPointVct.clear();
									}
									break;
								}
								default:
									break;
								}
							}
						}
					}

					//地理坐标转图像坐标(地理坐标存完后一次计算)
					if (!points.empty() || !lines.empty() || !regions.empty()) {
						calculatGeodeticToImageSd(ar_elements_array, ar_infos_internal_to_store, _ar_mark_elements_guid_sd,
							timeStamp, width, height, ar_meta_data, points, lines, regions);
					}

					//存储到需要push rt的容器中去
					ar_infos_internal_to_push_rtc.ArElementsNum += ar_infos_internal_to_store.ArElementsNum;
					for (auto iter : ar_infos_internal_to_store.ArElementsArray) {
						ar_infos_internal_to_push_rtc.ArElementsArray.push_back(iter);
					}

					//存储当前有新增的标注要素
					_ar_mark_pixel_and_geographic_map_sd.insert(std::make_pair(_ar_mark_elements_guid_sd, ar_infos_internal_to_store));

					// videoMarkMetaDataWriteSd(ar_infos_internal_to_store, image->meta_data.original_pts);
				}
				image->meta_data.ar_mark_info = ar_infos_internal_to_push_rtc;
			}
		}
		void DispatchTaskImplMultiple::executeHdARProcess(CodecImagePtr image)
		{
			if (_ar_engine && image->meta_data.meta_data_valid) {
				if (_ar_image_compute_state) {
					_ar_image_compute_state = false;

					std::vector<cv::Rect> cv_rect{};
#ifdef ENABLE_AI
					if (_is_enhanced_ar_on && _aux_ai_object_detector && _aux_ai_mot_tracker) {
						auto detect_objects = _aux_ai_object_detector->detect(image->bgr24_image);
						if (!detect_objects.empty()) {
							std::vector<joai::TrackResult>  track_ret = _aux_ai_mot_tracker->Update(detect_objects);
							for (const auto& ret : track_ret) {
								cv_rect.push_back(ret.Bounding_box);
							}
						}
					}
#endif

					auto pts = image->meta_data.pts;
					auto width = image->bgr24_image.cols;
					auto height = image->bgr24_image.rows;
					cv::Mat mat;
					JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;

					jo::JoARMetaDataBasic ar_meta_data{};
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

					std::vector<cv::Point> pixel_points{};
					std::vector<std::vector<cv::Point>> pixel_lines{};

					std::vector<cv::Point> tmp_pixel_points{};
					std::vector<std::vector<cv::Point>> tmp_pixel_lines{};

					std::vector<std::vector<cv::Point>> pixel_warning_l1_regions{};
					std::vector<std::vector<cv::Point>> pixel_warning_l2_regions{};
					if (_is_enhanced_ar_on) {
						_ar_engine->withAuxiliaryAI(true);
						_ar_engine->inputAuxiliaryAIData(cv_rect);
					}
					_ar_engine->frameProcess(true, pts, mat, width, height, ar_meta_data, pixel_points, pixel_lines
						, pixel_warning_l1_regions, pixel_warning_l2_regions);

					for (const auto& iter : pixel_warning_l1_regions) {
						pixel_lines.push_back(iter);
					}
					for (const auto& iter : pixel_warning_l2_regions) {
						pixel_lines.push_back(iter);
					}

					// 筛选去掉屏幕外多余的点
					int i = 0;
					std::queue<int> ar_valid_point_index{}; // 筛选去掉屏幕外多余的点后有效点的索引
					for (const auto& iter : pixel_points) {
						if (iter.x <= width && iter.x >= 0 && iter.y <= height && iter.y >= 0) {
							tmp_pixel_points.push_back(iter);
							ar_valid_point_index.push(i);
						}
						++i;
					}
					for (std::size_t index = 0; index < pixel_lines.size(); ++index) {
						auto& iter_line = pixel_lines[index];

						std::vector<cv::Point> tmp_points{};

						bool is_in_screen{};
						int boundary = width / 2;

						for (std::size_t i = 0; i < iter_line.size(); ++i) {
							auto& iter = iter_line[i];
							if (iter.x <= width + boundary && iter.x >= 0 - boundary && iter.y <= height + boundary && iter.y >= 0 - boundary) {
								is_in_screen = true;
							}

							if (!is_in_screen) {
								continue;
							}
							is_in_screen = false;

							if (i) {
								auto& front_iter = iter_line[i - 1];
								if (front_iter.x > width + boundary || front_iter.x < 0 - boundary || front_iter.y > height + boundary || front_iter.y < 0 - boundary) {
									tmp_points.push_back(front_iter);
								}
							}

							tmp_points.push_back(iter);

							if (i < iter_line.size() - 1) {
								auto& back_iter = iter_line[i + 1];
								//警戒线是个环，不能break
								if (back_iter.x > width + boundary || back_iter.x < 0 - boundary || back_iter.y > height + boundary || back_iter.y < 0 - boundary) {
									tmp_points.push_back(back_iter);
								}
							}
						}

						if (tmp_points.empty() && _ar_image_compute_pixel_lines.size() == pixel_lines.size()) {
							auto& tmp_line = _ar_image_compute_pixel_lines[index];
							for (auto& iter : tmp_line) {
								tmp_points.push_back(iter);
							}

						}

						tmp_pixel_lines.push_back(tmp_points);
					}

					std::swap(_ar_valid_point_index, ar_valid_point_index);
					std::swap(_ar_image_compute_pixel_points, tmp_pixel_points);
					std::swap(_ar_image_compute_pixel_lines, tmp_pixel_lines);
				}
				else {
					_ar_image_compute_state = true;
				}

				image->meta_data.pixel_points = _ar_image_compute_pixel_points;
				image->meta_data.pixel_lines = _ar_image_compute_pixel_lines;
				image->meta_data.ar_valid_point_index = _ar_valid_point_index;
				if (image->meta_data.ar_vector_file != _ar_vector_file) {
					image->meta_data.ar_vector_file = _ar_vector_file;
				}
			}
		}
		void DispatchTaskImplMultiple::executeSdARProcess(CodecImagePtr image)
		{
			if (_ar_engine_sd && image->meta_data.meta_data_valid) {
				if (_ar_image_compute_state_sd) {
					_ar_image_compute_state_sd = false;

					std::vector<cv::Rect> cv_rect{};
#ifdef ENABLE_AI
					if (_is_enhanced_ar_on_sd && _aux_ai_object_detector_sd && _aux_ai_mot_tracker_sd) {
						auto detect_objects = _aux_ai_object_detector_sd->detect(image->bgr24_image);
						if (!detect_objects.empty()) {
							std::vector<joai::TrackResult>  track_ret = _aux_ai_mot_tracker_sd->Update(detect_objects);
							for (const auto& ret : track_ret) {
								cv_rect.push_back(ret.Bounding_box);
							}
						}
					}
#endif

					auto pts = image->meta_data.pts;
					auto width = image->bgr24_image.cols;
					auto height = image->bgr24_image.rows;
					cv::Mat mat;
					JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
					jo::JoARMetaDataBasic ar_meta_data{};
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

					std::vector<cv::Point> pixel_points{};
					std::vector<std::vector<cv::Point>> pixel_lines{};
					std::vector<cv::Point> tmp_pixel_points{};
					std::vector<std::vector<cv::Point>> tmp_pixel_lines{};
					std::vector<std::vector<cv::Point>> pixel_warning_l1_regions{};
					std::vector<std::vector<cv::Point>> pixel_warning_l2_regions{};
					if (_is_enhanced_ar_on_sd) {
						_ar_engine_sd->withAuxiliaryAI(true);
						_ar_engine_sd->inputAuxiliaryAIData(cv_rect);
					}
					_ar_engine_sd->frameProcess(true, pts, mat, width, height, ar_meta_data, pixel_points, pixel_lines
						, pixel_warning_l1_regions, pixel_warning_l2_regions);

					for (const auto& iter : pixel_warning_l1_regions) {
						pixel_lines.push_back(iter);
					}
					for (const auto& iter : pixel_warning_l2_regions) {
						pixel_lines.push_back(iter);
					}

					// 筛选去掉屏幕外多余的点
					int i = 0;
					std::queue<int> ar_valid_point_index{}; // 筛选去掉屏幕外多余的点后有效点的索引
					for (const auto& iter : pixel_points) {
						if (iter.x <= width && iter.x >= 0 && iter.y <= height && iter.y >= 0) {
							tmp_pixel_points.push_back(iter);
							ar_valid_point_index.push(i);
						}
						++i;
					}
					for (std::size_t index = 0; index < pixel_lines.size(); ++index) {
						auto& iter_line = pixel_lines[index];
						std::vector<cv::Point> tmp_points{};
						bool is_in_screen{};
						int boundary = width / 2;
						for (std::size_t i = 0; i < iter_line.size(); ++i) {
							auto& iter = iter_line[i];
							if (iter.x <= width + boundary && iter.x >= 0 - boundary && iter.y <= height + boundary && iter.y >= 0 - boundary) {
								is_in_screen = true;
							}

							if (!is_in_screen) {
								continue;
							}
							is_in_screen = false;

							if (i) {
								auto& front_iter = iter_line[i - 1];
								if (front_iter.x > width + boundary || front_iter.x < 0 - boundary || front_iter.y > height + boundary || front_iter.y < 0 - boundary) {
									tmp_points.push_back(front_iter);
								}
							}

							tmp_points.push_back(iter);
							if (i < iter_line.size() - 1) {
								auto& back_iter = iter_line[i + 1];
								//警戒线是个环，不能break
								if (back_iter.x > width + boundary || back_iter.x < 0 - boundary || back_iter.y > height + boundary || back_iter.y < 0 - boundary) {
									tmp_points.push_back(back_iter);
								}
							}
						}

						if (tmp_points.empty() && _ar_image_compute_pixel_lines_sd.size() == pixel_lines.size()) {
							auto& tmp_line = _ar_image_compute_pixel_lines_sd[index];
							for (auto& iter : tmp_line) {
								tmp_points.push_back(iter);
							}
						}
						tmp_pixel_lines.push_back(tmp_points);
					}

					std::swap(_ar_valid_point_index_sd, ar_valid_point_index);
					std::swap(_ar_image_compute_pixel_points_sd, tmp_pixel_points);
					std::swap(_ar_image_compute_pixel_lines_sd, tmp_pixel_lines);
				}
				else {
					_ar_image_compute_state_sd = true;
				}

				image->meta_data.pixel_points = _ar_image_compute_pixel_points_sd;
				image->meta_data.pixel_lines = _ar_image_compute_pixel_lines_sd;
				image->meta_data.ar_valid_point_index = _ar_valid_point_index_sd;
				if (image->meta_data.ar_vector_file != _ar_vector_file) {
					image->meta_data.ar_vector_file = _ar_vector_file;
				}
			}
		}
		#endif
	}
}