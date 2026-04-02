#ifndef EAPS_DIPATCH_TASK_IMPL_MULTI_H
#define EAPS_DIPATCH_TASK_IMPL_MULTI_H
#pragma once

#include "EapsDispatchTask.h"
#include "EapsCommon.h"
#include "EapsMetaDataProcessing.h"

#ifdef ENABLE_GPU
#ifndef ENABLE_AIRBORNE
#include "EapsImageEnhancer.h"
#include "EapsImageStabilizer.h"
#endif
#ifdef ENABLE_AI
#include "jo_ai_object_detect.h"
#include "Track.h"
#endif
#endif // ENABLE_GPU
#ifdef ENABLE_AR
#include "jo_ar_engine_common.h"
#include "jo_ar_meta_data_structure.h"
#endif
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <vector>
#include <memory>
#include <unordered_map>  
#include <variant>
#include <tuple>

#ifdef ENABLE_AI
namespace joai {
	class ObjectDetect;
	using ObjectDetectPtr = std::shared_ptr<ObjectDetect>;

	class MotTrack;
	using MotTrackPtr = std::shared_ptr<MotTrack>;
}
#endif // ENABLE_AI
#ifdef ENABLE_AR
namespace joar {
	class ArEngine;
	using ArEnginePtr = std::shared_ptr<ArEngine>;
}
namespace jomarker {
    class MarkerEngine;
    using ArMarkerEnginePtr = std::shared_ptr<MarkerEngine>;
}
#endif

namespace eap {
	namespace sma {
		class DispatchTaskImplMultiple;
		using DispatchTaskImplMultiplePtr = std::shared_ptr<DispatchTaskImplMultiple>;

#ifdef ENABLE_AR
		using ar_point = std::vector<jo::GeographicPosition>;
		using ar_line_or_region = std::vector<std::vector<jo::GeographicPosition>>;
#endif

		class DispatchTaskImplMultiple : public DispatchTask, public std::enable_shared_from_this<DispatchTaskImplMultiple>
		{
		public:
			static DispatchTaskImplMultiplePtr createInstance(InitParameter init_parameter);

		public:
			virtual ~DispatchTaskImplMultiple();

			//virtual void setArFile(std::string ar_kml, std::string ar_camera) override;		
			virtual void start() override;
			virtual void stop() override;

			virtual void updateFuncMask(int new_func_mask, 
				std::string ar_camera = std::string(), std::string ar_kml = std::string()) override;
			virtual void clipSnapShotParam(int time_count) override;
			virtual void setSeekPercent(float percent) override;
			virtual void pause(int paused) override;
			virtual void updateArLevelDistance(int level_one_distance, int level_two_distance) override;
			virtual void addAnnotationElements(std::string ar_camera_path, std::string annotation_elements_json, bool isHd=true) override;
			virtual void deleteAnnotationElements(std::string mark_guid, bool isHd=true) override;
			virtual std::vector<int64_t> getVideoDuration() override;
			virtual void updateArTowerHeight(bool is_tower, double tower_height, bool buffer_sync_height) override;
			virtual void updateAiCorPos(int ai_pos_cor) override;
			virtual std::string aiAssistTrack(int track_cmd, int track_pixelpos_x, int track_pixelpos_y) override;
			virtual void saveSnapShot() override;
#ifdef ENABLE_AIRBORNE
			virtual void receivePilotData(std::string param_str) override;
			virtual void receivePayloadData(std::string param_str) override;
#endif // ENABLE_AIRBORNE
			static void receiveVersinDataMulti(std::string version_data);

		private:
			void createAIEngine();
			void createAIEngineSd();
			void createAuxiliaryAIEngine();
			void createAuxiliaryAIEngineSd();
			void createAREngine();
			void createAREngineSd();
			void createARMarkEngine();
			void createARMarkEngineSd();
			void createImageEnhancer();
			void createSdImageEnhancer();
			void createImageStabilizer();
			void createSdImageStabilizer();

			void destroyAIEngine(bool is_hd=true);
			void destroyAuxiliaryAIEngine(bool is_hd=true);
			void destroyAREngine(bool is_hd=true);
			void destroyARMarkEngine(bool is_hd=true);
			void destroyImageEnhancer(bool is_hd=true);
			void destroyImageStabilizer(bool is_hd=true);

			void update_func_mask_l();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			void trackingCoordTransform(CodecImagePtr image, cv::Mat M);
			void arCoordTransform(CodecImagePtr image, cv::Mat M);
			void aiCoordTransform(CodecImagePtr image, cv::Mat M);
#endif

			void videoMarkMetaDataWrite(ArInfosInternal &ar_infos, int64_t packet_pts);
			void videoMarkMetaDataWriteSd(ArInfosInternal &ar_infos, int64_t packet_pts);
			void videoMarkMetaDataRead(bool is_hd=true);

#ifdef ENABLE_AR
			void calculatGeodeticToImage(std::list<ArElementsInternal> ar_elements_array, ArInfosInternal &ar_infos, std::string guid, const int64_t &timestamp, const int &img_width, const int &img_height,
				const jo::JoARMetaDataBasic &meta_data, const ar_point& points, const ar_line_or_region& lines,const ar_line_or_region& regions);

			void calculatGeodeticToImageSd(std::list<ArElementsInternal> ar_elements_array, ArInfosInternal &ar_infos, std::string guid, const int64_t &timestamp, const int &img_width, const int &img_height,
			const jo::JoARMetaDataBasic &meta_data, const ar_point& points, const ar_line_or_region& lines,const ar_line_or_region& regions);
#endif

			void hdThread();
			void sdThread();
			void hdDangerLoopThread();
			void sdDangerLoopThread();
			#ifdef ENABLE_AR
			void executeHdARMarkProcess(CodecImagePtr image);
			void executeSdARMarkProcess(CodecImagePtr image);
			void executeHdARProcess(CodecImagePtr image);
			void executeSdARProcess(CodecImagePtr image);
			#endif
		private:
			bool _is_pull_rtc{ false };
			bool _is_pull_sd_rtc{ false };
			bool _is_push_rtc{ false };
			bool _is_push_sd_rtc{ false };

			DemuxerPtr _demuxer_tradition{};
			DemuxerPtr _demuxer_tradition_record{};
			DemuxerRtcPtr _demuxer_rtc{};
			DemuxerPtr _demuxer_sd_tradition{};
			DemuxerPtr _demuxer_sd_tradition_record{};
			DemuxerRtcPtr _demuxer_sd_rtc{};
			bool _is_hardware_decode{};
	#ifdef ENABLE_GPU
			DecoderPtr _decoder{};
			EncoderNVENCPtr _encoder{};
			DecoderPtr _decoder_sd{};
			EncoderNVENCPtr _encoder_sd{};
	#endif // ENABLE_GPU
			PusherPtr _pusher_tradition{};
			PusherRtcPtr _pusher_rtc{};
			PusherPtr _pusher_sd_tradition{};
			PusherRtcPtr _pusher_sd_rtc{};
			//片段视频录制
			PusherPtr _pusher_tradition_recode{};
			PusherPtr _pusher_sd_tradition_recode{};
			std::mutex _pusher_recode_mutex{};
			std::mutex _pusher_sd_recode_mutex{};
			bool _is_recording{};
			bool _is_recording_sd{};
			std::chrono::system_clock::time_point _recode_start_time_point{};
			std::string _recode_start_timestamp_str{};
			std::chrono::system_clock::time_point _recode_sd_start_time_point{};
			std::string _recode_sd_start_timestamp_str{};

			std::mutex _decoder_mutex;
			std::mutex _decoder_sd_mutex;
			std::mutex _encoder_mutex{};
			std::mutex _encoder_sd_mutex{};
			std::mutex _pusher_mutex{};
			std::mutex _pusher_sd_mutex{};
			std::mutex _update_func_mutex{};
			std::mutex _update_func_mutex_sd{};

			AVCodecParameters _codec_parameter{};
			AVRational _timebase{};
			AVRational _framerate{};
			int64_t _start_time{};
			int64_t _video_duration{};
			AVCodecParameters _codec_parameter_sd{};
			AVRational _timebase_sd{};
			AVRational _framerate_sd{};
			int64_t _start_time_sd{};
			int64_t _video_duration_sd{};

			std::thread _loop_thread{};
			std::thread _hd_thread{};
			std::thread _sd_thread{};
			bool _loop_run{};
			std::thread _loop_sd_thread{};
			bool _loop_sd_run{};
			bool _is_demuxer_closed{};
			bool _is_demuxer_closed_sd{};

			std::mutex _decoded_images_mutex{};
			std::mutex _decoded_sd_images_mutex{};
			std::mutex _sync_push_packet_mutex{};
			std::condition_variable _decoded_images_cv{};
			std::condition_variable _decoded_sd_images_cv{};
			std::condition_variable _sync_push_packet_cv{};
			std::queue<CodecImagePtr> _decoded_images{};
			std::queue<CodecImagePtr> _decoded_sd_images{};
			int64_t _sd_packet_pts{};

			std::queue<std::string> _video_mark_data{};
			std::queue<bool> _video_mark_data_status{}; //标注来源队列（true是高清-false是标清标注）
			std::queue<std::string> _video_mark_data_sd{};
			std::queue<bool> _video_mark_data_sd_status{};

			int _ar_level_one_distance{ 0 };
			int _ar_level_two_distance{ 0 };

			int64_t _ave_ai_time{0};
			int64_t _ai_frame_count{0};
			int64_t _ave_ai_time_sd{0};
			int64_t _ai_frame_count_sd{0};

			std::string _record_file_prefix{};
			std::string _record_time_str{};
			std::string _task_id{};

	#ifdef ENABLE_GPU
	#ifndef ENABLE_AIRBORNE
			Enhancer::EnhancerPtr _enhancer{};
			Enhancer::EnhancerPtr _enhancer_sd{};
			VideoStabilizer::StabilizerPtr _stabilizer{};
			VideoStabilizer::StabilizerPtr _stabilizer_sd{};
	#endif

#ifdef ENABLE_AI
			joai::ObjectDetectPtr _ai_object_detector{};
			joai::MotTrackPtr _ai_mot_tracker{};
			joai::ObjectDetectPtr _aux_ai_object_detector{};
			joai::MotTrackPtr _aux_ai_mot_tracker{};
			
			joai::ObjectDetectPtr _ai_object_detector_sd{};
			joai::MotTrackPtr _ai_mot_tracker_sd{};
			joai::ObjectDetectPtr _aux_ai_object_detector_sd{};
			joai::MotTrackPtr _aux_ai_mot_tracker_sd{};

			std::vector<joai::TrackResult> _ai_track_ret{};
			std::vector<joai::TrackResult> _ai_track_ret_sd{};
#endif 
#endif // ENABLE_GPU
#ifdef ENABLE_AR
			joar::ArEnginePtr _ar_engine{};
			jomarker::ArMarkerEnginePtr _ar_mark_engine{};
			joar::ArEnginePtr _ar_engine_sd{};
			jomarker::ArMarkerEnginePtr _ar_mark_engine_sd{};
#endif

			std::map<int64_t, MetaDataWrap> _meta_data_cache{};
			std::map<int64_t, MetaDataWrap> _meta_data_cache_sd{};

			std::string _ar_vector_file{"test.kml"};
			std::string _ar_settings_file{"camera.config"};
			std::atomic_bool _is_update_ar_mark_engine{false};
			std::atomic_bool _is_update_ar_mark_engine_sd{false};

			MetaDataProcessingPtr _meta_data_processing_pret{};
			MetaDataProcessingPtr _meta_data_processing_postproc{};
			MetaDataProcessingPtr _meta_data_processing_pret_sd{};
			MetaDataProcessingPtr _meta_data_processing_postproc_sd{};

			bool _is_image_enhancer_first_create{ true };
			bool _is_sd_image_enhancer_first_create{ true };
			bool _is_image_stable_first_create{ true };
			bool _is_sd_image_stable_first_create{ true };
			bool _is_ai_first_create{ true };
			bool _is_aux_ai_first_create{ true };
			bool _is_ar_first_create{ true };
			bool _is_ai_first_create_sd{ true };
			bool _is_aux_ai_first_create_sd{ true };
			bool _is_ar_first_create_sd{ true };

			bool _is_image_enhancer_on{};
			bool _is_sd_image_enhancer_on{};
			bool _is_image_stable_on_sd{};
			bool _is_image_stable_on{};
			bool _is_ai_on{};
			bool _is_ar_on{};
			bool _is_enhanced_ar_on{};
			bool _is_ai_on_sd{};
			bool _is_ar_on_sd{};
			bool _is_enhanced_ar_on_sd{};
			
			bool _ar_image_compute_state{ true };
			std::queue<int> _ar_valid_point_index{};
			bool _ar_image_compute_state_sd{ true };
			std::queue<int> _ar_valid_point_index_sd{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> _ar_image_compute_pixel_points{};
			std::vector<std::vector<cv::Point>> _ar_image_compute_pixel_lines{};
			std::vector<cv::Point> _ar_image_compute_pixel_points_sd{};
			std::vector<std::vector<cv::Point>> _ar_image_compute_pixel_lines_sd{};
#endif
			std::atomic<bool> _upload_image_is_done{ true };
			std::chrono::system_clock::time_point _upload_image_time_point{};
			std::atomic<bool> _upload_image_is_done_sd{ true };
			std::chrono::system_clock::time_point _upload_image_time_point_sd{};
			
			bool _update_func_result{ true };
			bool _update_func_result_sd{ true };
			std::string _update_func_err_desc{};
			std::string _update_func_err_desc_sd{};
			bool _is_update_func{ false };
			bool _is_update_func_sd{ false };
			bool _is_manual_stoped{};
			std::atomic<int> _function_mask{ 0 };
			std::vector<std::string> _pull_urls{};
			std::vector<std::string> _push_urls{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> _pixel_points{};
			std::vector<std::vector<cv::Point>> _pixel_lines{};
			std::vector<std::vector<cv::Point>> _pixel_regions{};
			std::vector<cv::Point> _pixel_points_sd{};
			std::vector<std::vector<cv::Point>> _pixel_lines_sd{};
			std::vector<std::vector<cv::Point>> _pixel_regions_sd{};
#endif
			AiInfos _lastAiInfos{};
			AiInfos _lastAiInfos_sd{};

			int64_t _video_mark_frame_count{};
			std::string _ar_mark_elements_guid{};
			int64_t _video_mark_frame_count_sd{};
			std::string _ar_mark_elements_guid_sd{};
							
			std::map<std::string, ArInfosInternal> _ar_mark_pixel_and_geographic_map{};
			std::map<std::string, ArInfosInternal> _ar_mark_pixel_and_geographic_map_sd{};
			std::string _mark_guid{};
			std::string _mark_guid_sd{};
			std::string _notice_task_id{};
			std::string _notice_task_internal_id{};

			char* pIn420Dev{};
			char* pOutBgrDev{};
			
			// 隐患回传线程
			std::thread _danger_photo_loop_thread{};
			std::atomic_bool _danger_photo_loop_thread_run{};
			std::queue<CodecImagePtr> _danger_images{};
#ifdef ENABLE_AI
			std::queue<std::vector<joai::Result>> _danger_ai_ret{};
			std::queue<std::vector<joai::Result>> _danger_ai_ret_sd{};
#endif
			std::mutex _danger_queue_mutex{};
			std::condition_variable _danger_queue_cv{};
			std::thread _danger_photo_loop_thread_sd{};
			std::atomic_bool _danger_photo_loop_thread_run_sd{};
			std::queue<CodecImagePtr> _danger_images_sd{};
			
			std::mutex _danger_queue_mutex_sd{};
			std::condition_variable _danger_queue_cv_sd{};
		private:
			DispatchTaskImplMultiple(InitParameter init_parameter);
			DispatchTaskImplMultiple(DispatchTaskImplMultiple& other) = delete;
			DispatchTaskImplMultiple(DispatchTaskImplMultiple&& other) = delete;
			DispatchTaskImplMultiple& operator=(DispatchTaskImplMultiple& other) = delete;
			DispatchTaskImplMultiple& operator=(DispatchTaskImplMultiple&& other) = delete;
		};
	}
}
#endif //!EAPS_DIPATCH_TASK_IMPL_MULTI_H