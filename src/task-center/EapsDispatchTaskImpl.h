#ifndef EAPS_DIPATCH_TASK_IMPL_H
#define EAPS_DIPATCH_TASK_IMPL_H

#include "EapsDispatchTask.h"
#include "EapsCommon.h"
#include "jo_meta_data_structure.h"
#include "EapsMetaDataProcessing.h"
#include "EapsFileManager.h"
#include "ThreadPool.h"
#include "EapsUdpSocket.h"
#include "EapsDemuxerReactor.h"
#ifdef ENABLE_GPU
#ifndef ENABLE_AIRBORNE
#include "EapsImageEnhancer.h"
#include "EapsImageStabilizer.h"
#endif
#ifdef ENABLE_AI
#include "jo_ai_object_detect.h"
#include "Track.h"
#endif
#ifdef ENABLE_VISION_ENHANCE
#include "jo_vision_enhance.h"
#endif
#ifdef ENABLE_STABLIZE
#include "Stablize.h"
#endif
#endif // ENABLE_GPU
#ifdef ENABLE_AR
#include "jo_ar_engine_common.h"
#include "jo_ar_meta_data_structure.h"
#endif
#include "EapsTcpServer.h"
#ifdef ENABLE_AIRBORNE
#include "LinuxStorageDevManage.h"
#ifdef ENABLE_PIO
#include "pioPublish.h"
#endif 
#endif // ENABLE_AIRBORNE
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#include <opencv2/core.hpp>
#endif
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <vector>
#include <memory>
#include <list>
#include <iterator>
#include <cmath>
#include "ReadAIconfig.hpp"

#define TARGET_BOX_RATE_DEFAULT (2.5f)

#ifdef ENABLE_AI
namespace joai {
	class ObjectDetect;
	using ObjectDetectPtr = std::shared_ptr<ObjectDetect>;

	class MotTrack;
	using MotTrackPtr = std::shared_ptr<MotTrack>;
}
#endif
#ifdef ENABLE_VISION_ENHANCE
namespace jovisionoptimizer{
	class VisionEnhance;
	using VisionEnhancePtr = std::shared_ptr<VisionEnhance>;
}
#endif
#ifdef ENABLE_AR
namespace joar {
	class ArEngine;
	using ArEnginePtr = std::shared_ptr<ArEngine>;
}
namespace jomarker
{
	class MarkerEngine;
	using ArMarkerEnginePtr = std::shared_ptr<MarkerEngine>;
}
#endif

namespace eap {
	// 确保结构体是POD类型，无虚函数/指针，可安全共享
    constexpr uint32_t PICTRUE_DATA_MAX_SIZE = 6220800;
    constexpr uint32_t TAGET_BOX_NUM = 10;
#ifdef ENABLE_AIRBORNE
    // 按照1字节对齐
    struct __attribute__((packed)) PictrueDataHead {
        uint8_t sync0;
    	uint8_t sync1;
        uint8_t type;          // 0:红外 1：可见光 
    	uint16_t width;        // 图像宽度
        uint16_t height;       // 图像高度
    	uint8_t gimbal_mode;   // 吊舱工作模式状态 0：待机模式 1：搜索模式 2：跟踪模式 3：投弹模式 4：导引头模式 5-255预留
    	uint32_t frame_id;     // 帧序号
    	uint64_t timestamp;    // 时戳（微妙）
    	uint8_t reserved[16];          // 预留
    };
    
    // 按照1字节对齐
    struct __attribute__((packed)) TargetBoxInfo {
        uint16_t x;            // 目标中心x坐标
    	uint16_t y;            // 目标中心y坐标
    	uint16_t w;            // 目标宽度
    	uint16_t h;            // 目标高度
    	uint16_t area;         // 目标面积
    	uint16_t temperature;  // 目标温度
    	uint16_t label;        // 目标类别
    	uint16_t score;        // 置信度*100
    };
    
    // 按照1字节对齐
    struct __attribute__((packed)) LockFreeSharedData {
        // 版本号：偶数=数据完整，奇数=写入中
        std::atomic<uint32_t> version;
    	PictrueDataHead head;
        TargetBoxInfo box[TAGET_BOX_NUM];
        char data[PICTRUE_DATA_MAX_SIZE];
    };

    // 共享内存总大小（结构体大小向上对齐，避免内存不足）
    constexpr size_t SHARED_MEM_SIZE = sizeof(LockFreeSharedData);
    // 共享内存名称
    constexpr const char* SHARED_MEM_NAME = "LockFreeSharedMem";
#endif
	namespace sma {
		struct Engines;
		using EnginesPtr = std::shared_ptr<Engines>;
		using namespace eap::common;
		class DispatchTaskImpl;
		using DispatchTaskImplPtr = std::shared_ptr<DispatchTaskImpl>;
#ifdef ENABLE_AR
		using ar_point = std::vector<jo::GeographicPosition>;
		using ar_line_or_region = std::vector<std::vector<jo::GeographicPosition>>;
#endif
		class DispatchTaskImpl : public DispatchTask, public std::enable_shared_from_this<DispatchTaskImpl>
		{
		public:
			using StreamRecoverCallback = std::function<void(int type_code)>;
			static DispatchTaskImplPtr createInstance(InitParameter init_parameter);
			struct _networkCheckResult
			{
				common::NetWorkChecking::NetworkAdapterType type{};
				common::NetWorkChecking::NetworkAdapterStatus new_status{};
				std::string address{};
				std::string name{};
				int index{};
			};

#pragma pack(push, 1)
			struct frame_header
			{
				uint8_t sync0;
				uint8_t sync1;
				uint8_t dest;
				uint8_t source;
				uint8_t msgId;
				uint32_t length;
			};
			struct frameSeiMsg {
				uint64_t SyncTimeEO;//吊舱时间戳
				uint64_t SyncTimeUAV;//飞控时间戳
				int32_t PodFramePan; //吊舱框架角-偏航
				int32_t PodFrameTilt;//吊舱框架角-俯仰
				int32_t PodFrameRoll;//吊舱框架角-滚转
				int32_t PodPan;//吊舱姿态角-偏航
				int32_t PodTilt;//吊舱姿态角-俯仰
				int32_t PodRoll;//吊舱姿态角-滚转
				int32_t UAVPan;//飞机姿态角-偏航
				int32_t UAVTilt;//飞机姿态角-俯仰
				int32_t UAVRoll;//飞机姿态角-滚转
				int32_t UAVLat;//飞机纬度
				int32_t UAVLon;//飞机经度
				int32_t UAVHeight;//飞机海拔高度
				int32_t TgtHeight;//视点中心海拔高度
			};

#pragma pack(pop)

		public:
			virtual ~DispatchTaskImpl();

			virtual void start() override;
			virtual void stop() override;
			virtual void updateFuncMask(int func_mask, std::string ar_camera_config = std::string(), std::string ar_vector_file = std::string()) override;
			virtual void clipSnapShotParam(int time_count) override;
#ifdef ENABLE_AIRBORNE	
		    void sendPictureData(LockFreeSharedData* shared_data, const std::string &send_data);

			virtual void receivePilotData(std::string param_str) override;
			virtual void receivePayloadData(std::string param_str) override;
			virtual void snapshot(std::string recordNo, int interval, int total_time) override;
			void tcpServerListen();
			// resize
			int fastYuv420pResize(AVFrame* src_frame, AVFrame*& dst_frame);
#endif // ENABLE_AIRBORNE

			static void receiveVersinData(std::string version_data);
			virtual void addAnnotationElements(std::string ar_camera_path, std::string annotation_elements_json, bool is_hd=true) override;
			virtual void deleteAnnotationElements(std::string mark_guid, bool is_hd=true) override;

			virtual std::string aiAssistTrack(int track_cmd, int track_pixelpos_x, int track_pixelpos_y) override;
			virtual void saveSnapShot() override;
			// virtual std::vector<std::tuple<std::vector<long>, std::vector<int>>> getHeatmapTotal() override;

			virtual void updateArLevelDistance(int level_one_distance, int level_two_distance) override;

			virtual void updateArTowerHeight(bool is_tower, double tower_height, bool buffer_sync_height) override;
			virtual void setSeekPercent(float percent) override;
			virtual void pause(int paused) override;
			virtual std::vector<int64_t> getVideoDuration() override;
			virtual void updateAiCorPos(int ai_pos_cor) override;
			//森防项目，第一次火点定位出结果的时候通知云端sma，快照图片给云平台
			virtual void fireSearchInfo(const std::string id, const double target_lat, const double target_lon, const double target_alt) override;
			//机载链路切换，是否走4、5G链路
			virtual void setAirborne45G(const int airborne_45G) override;
		
		private:
			void createPusher(std::string url);
			void destroyPusher();
			void createMuxer();
			void destroyMuxer();
			void createClipMuxer();
			void destroyClipMuxer();
			void clipMuxerRecordTimer();
			void createDecoder(Decoder::FrameCallback frame_callback);
			void createStablizer();
#ifdef ENABLE_GPU

			void createEncoder(EncoderNVENC::EncodedPacketCallback encoder_packet_callback);
			void destroyEncoder();//只在最后销毁
#ifndef ENABLE_AIRBORNE
			void createImageEnhancer();
			void destroyImageEnhancer();
#endif
#endif
			void destroyDecoder();//只在最后销毁
			void destroyDemuxer();
			void destroyStablizer();

#ifdef ENABLE_AI
			void createAIEngine();
#ifdef ENABLE_OPENSET_DETECTION
			void createOpensetAIEngine();
			void destroyOpensetAIEngine();
#endif 
#endif
#ifdef ENABLE_VISION_ENHANCE
			void createVisionEnhancer();
#endif
#ifdef ENABLE_AR
			void createAREngine();
			void createARMarkEngine();
			void createAuxiliaryAIEngine();
			void calculatGeodeticToImage(std::list<ArElementsInternal> ar_elements_array, ArInfosInternal& ar_infos, std::string guid, const int64_t& timestamp, const int& img_width, const int& img_height, const jo::JoARMetaDataBasic& meta_data, const std::vector<jo::GeographicPosition>& points, const std::vector<std::vector<jo::GeographicPosition>>& lines, const std::vector<std::vector<jo::GeographicPosition>>& regions);
			void videoMarkMetaDataWrite(ArInfosInternal& ar_infos, int64_t packet_pts);
#endif
#ifdef ENABLE_AI
			void executeAIProcess(CodecImagePtr image, bool is_process);
#endif
#ifdef ENABLE_AR
			void executeARProcess(CodecImagePtr image);
			void executeARMarkProcess(CodecImagePtr image);

			void executeARProcess(Packet& packet);
			void executeARMarkProcess(Packet& packet);
#endif
#ifdef ENABLE_AI
			void destroyAIEngine();
			void destroyAuxiliaryAIEngine();
#endif

#ifdef ENABLE_VISION_ENHANCE
			void destroyVisionEnhanceEngine();
#endif 

#ifdef ENABLE_AR
			void destroyAREngine();
			void destroyARMarkEngine();
#endif			

			void pushPacket(Packet packet);
			void pusherStopCallback(std::string url, int ret, std::string err_str);

			void SdcartMount(std::string device_name, std::string MountPoint);
			void quittrackingSetParams();
			void updateFuncmaskL();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			void trackingCoordTransform(CodecImagePtr image, const cv::Mat& M);
			void arCoordTransform(CodecImagePtr image, const cv::Mat& M);
			void aiCoordTransform(CodecImagePtr image, const cv::Mat& M);
#endif

			void convertToMetaDataBasic(std::shared_ptr<JoFmvMetaDataBasic> meta_data, PayloadData close_payload_data, PilotData close_pilot_data);
#ifdef ENABLE_PIO
			void convertToMetaDataBasic(std::shared_ptr<JoFmvMetaDataBasic> meta_data, IrAiMsg ir_ai_msg);
#endif
			PayloadData findClosePayload(std::map<int64_t, PayloadData> payload_data_map, int64_t now_time_stamp);
			PilotData findClosePilot(std::map<int64_t, PilotData> pilot_data_map, int64_t now_time_stamp);
			int getCpuTemperature();

			/**
             * @brief	: 使用udp发送数据到ar座椅
             */
            void sendDataToUdp();

			/**
			 * @brief	: 解析元数据，获取飞机姿态
			 */
			void parseMetaData(const JoFmvMetaDataBasic* meta_data);

			//udp组播多网卡遍历
			void multiNetWorkCardFiltering(std::string url, std::chrono::milliseconds time_out, int frame_rate,
				Demuxer::StopCallback stop_callback, Demuxer::PacketCallback packet_callback);
			DemuxerReactorPtr creatDemuxerReactor(std::string url, std::chrono::milliseconds time_out, int frame_rate
				, Demuxer::StopCallback stop_callback, Demuxer::PacketCallback packet_callback, common::NetWorkChecking::AdapterInfo adapter_info, int i);

			void reactorLoopThread();
			void checkNetworkAdapterStatusChange(std::function<void(_networkCheckResult result)> callback);
			void dangerLoopThread();
			void recordLoopThread();
			bool openDemuxer(std::string& error_msg);
			bool openPusher();
			void openAirPusher(); //机载用5G推流
			void updateTaskFuncmask(bool is_update_task_file=false); //更新录像状态等功能掩码，记录到任务和配置文件里，程序异常重启后可以接着之前的funcmask工作
		private:
			struct PusherObject
			{
				std::mutex _pusher_mutex{};
				std::string _url{};
				std::string _air_url{};
				bool _is_rtc{};
				PusherPtr _pusher{};
				PusherPtr _air_pusher{}; //机载用5G推流
			};
			using PusherObjectPtr = std::shared_ptr<PusherObject>;

		private:
			eap::ThreadPool::Ptr _task_manager{};

			std::atomic<int> _clip_video_record_time_count{};

			bool _is_pull_rtc{};
			bool _is_pull_udp{}; //拉取udp组播视频流+多网卡
			
			bool _is_push_rtc{};
			bool _is_hardware_decode{};

			MetaDataProcessingPtr _meta_data_processor{};
			bool _is_image_enhancer_first_create{true};
			bool _is_image_stable_first_create{true};
			DemuxerPtr _demuxer{};

			DecoderPtr _decoder{};
#ifdef ENABLE_GPU
			EncoderNVENCPtr _encoder{};
			std::mutex _encoder_mutex{};
			#ifdef ENABLE_STABLIZE
			std::shared_ptr<stablizer> _stablizer{};
			#endif
#ifndef ENABLE_AIRBORNE
			Enhancer::EnhancerPtr _enhancer{};
			VideoStabilizer::StabilizerPtr _stabilizer{};
			cv::cuda::GpuMat _snapshot_image{};
#endif
#endif // ENABLE_GPU

#ifdef ENABLE_AIRBORNE
			std::shared_ptr<LinuxStorageDevManage> _linux_storage_dev_manage{};
			bool _enable_encode{};
			//给智能控制部发图片和千决sei数据
			std::shared_ptr<JoTcpServer> _tcp_server{};
			std::thread _tcpserver_io_run_thread{};
			std::atomic_bool _TcpServerIoRunThreadRun{};
			std::thread _tcpserver_listen_thread{};
			std::atomic_bool _tcpserver_listen_thread_run{};
			std::queue<AVFrame*> _tcp_send_frame_q{};
			std::queue<FRAME_POS_Qianjue> _tcp_send_sei_q{};
			std::mutex _tcp_send_frame_mutex{};
			uint8_t _send_frame_count{};
			frame_header _frame_header{};
			SwsContext* _SwsCtx{};

#endif

#ifdef ENABLE_PIO
			PioPublish::Ptr _pio_pubilsh{};
#endif ENABLE_PIO

			std::string device_name = "mmcblk1p13"; // TODO: 改成配置文件
			std::string mount_point = "/mnt/sdcard"; // TODO: 改成配置文件
			std::string device_path = "/dev";
			std::string snapshot_file_name_hd_;
			std::string snapshot_file_name_sd_;
			std::uint32_t snapshot_number = 1;
			std::mutex snapshot_mutex_;

			//片段视频录制
			PusherPtr _pusher_tradition_recode{};
			std::mutex _pusher_recode_mutex{};
			bool _is_recording{};
			bool _is_start_recording{}; //是否真正开启录制
			std::chrono::system_clock::time_point _recode_start_time_point{};
			std::string _recode_start_timestamp_str{};

			uint8_t osd_flags = 0;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			cv::Point ap_DetRegCenter;
			cv::Point ap_TrackRegCenter;
#endif
			float target_box_rate_ = TARGET_BOX_RATE_DEFAULT;

			//bool is_record_ = false;
			bool exec_snapshot_hd_ = false;
			bool exec_snapshot_sd_ = false;

			int HD_or_SD_ = 1;
			int track_HD_or_SD_ = 2;     //!< 跟踪状态，0x00：锁定 ；0x01:脱锁，进入速率半自主；0x02:跟踪状态不佳，但仍是跟踪态，仅关闭RTT;
			int track_status_pitot_ = 0;// 0x00:未发现目标；0x01:发现目标。简单逻辑：若飞机当前处于RTT模式，无论有没有发现目标，均发送0x00; 若飞机当前未处于RTT模式，当检测到目标时，发送0x01, 未检测到目标时，发送0x00;
			bool is_tracking_ = false;
			bool is_detecting_ = true;
			int  tracking_init_flag_ = 1;
			int  hf_tracking_state = 0;
			bool First_track_flag = true;
			bool aided_tracking = false; // AI辅助跟踪

			int servo_cross_flag_ = 0;
			bool AIPreStatus = false;

			std::string _mux_file_name{};
			MuxerPtr _muxer{};
			MuxerPtr _clip_muxer{};
			std::mutex _muxer_mutex{};
			std::mutex _clip_muxer_mutex{};
			std::mutex _pushers_mutex{};
			std::mutex _pushers_air_mutex{};
			PusherObjectPtr _pusher{};
			EnginesPtr _engines;
			AVCodecParameters _codec_parameter{};
			AVRational _timebase{};
			AVRational _framerate{};
			int _bit_rate{};
			int _airborne_45G{ 0 };//机载是否有4、5G，默认没有(0是图传，1是4、5G)；机载4、5G的时候不推udp组播视频流，只推流媒体服务器那个视频流;走图传的时候只推udp组播视频流
			int64_t _start_time{};

			std::thread _loop_thread_new{};
			bool _loop_run_new{};			
			std::thread _loop_thread_main{};
			std::atomic_bool _loop_thread_main_run{false};
			std::thread _loop_thread_get_cpu_temp{};
			std::atomic_bool _loop_thread_get_cpu_temp_run{false};

			// 隐患回传线程
			std::thread _danger_photo_loop_thread{};
			std::atomic_bool _danger_photo_loop_thread_run{};
			std::queue<CodecImagePtr> _danger_images{};

			//视频录制
			std::thread _record_loop_thread{};
			std::atomic_bool _record_loop_thread_run{};
			std::queue<Packet> _record_packets{};
			std::mutex _record_queue_mutex{};
			std::condition_variable _record_queue_cv{};
#ifdef ENABLE_AI
			std::queue<std::vector<joai::Result>> _danger_ai_ret{};
#endif
			std::mutex _danger_queue_mutex{};
			std::condition_variable _danger_queue_cv{};

			std::queue<Packet> _wait_meta_data_packet_q{};
			
			std::mutex _wait_meta_data_packet_q_mutex{};
			std::condition_variable _wait_meta_data_packet_q_cv{};
			std::queue<Packet> _bearer_meta_data_packet_q{};
			std::mutex _bearer_meta_data_packet_q_mutex{};

			std::mutex _decoded_images_mutex{};
			std::mutex _decoded_images_assist_track_mutex{};
			std::mutex _decoded_images_snapshot_mutex{};
			std::condition_variable _decoded_images_cv{};
			std::queue<CodecImagePtr> _decoded_images{};
			std::queue<CodecImagePtr> _decoded_images_assist_track{};
			std::queue<CodecImagePtr> _decoded_images_snapshot{};

			bool _is_update_ai_func{ false };
			bool _is_update_ar_func{ false };
			bool _is_manual_stoped{};
			std::atomic_bool _is_demuxer_closed{}; //demuxer stop callback的时候为true
			bool _is_start_finished{ false };//start函数结束
			bool _is_demuxer_opened{ false };
			JoFmvMetaDataBasic _last_metadata{};
			
			std::mutex _ar_related_mutex{};
			//录像、AI、AR、快照，功能关闭时默认都成功
			//打开时，录像、快照都默认成功，AI、AR分具体情况
			std::atomic<int> _func_mask{ 0 };
			std::atomic_bool _is_update_func{ false };
			std::string _update_func_err_desc{};
			bool _update_func_result{ true };
			bool _is_ai_first_create{ true };
			bool _is_openset_ai_first_create{ true };
			bool _is_ai_assist_track_first_create{ true };
			bool _is_ar_first_create{ true };
			bool _is_aux_ai_first_create{ true };
			bool _is_pilotcontrol_task{ false }; //是否为默认任务，true 推拉流失败一直重复推拉流直到成功
			bool _is_stable_first_create{true};

			//for sengfang: ai process
#ifdef ENABLE_AI
			std::atomic_bool _is_process_ai{true};
			std::vector<joai::Result> _sengfang_detect_objects{};
			float _fire_conf_thresh{0.4};
			float _smoke_conf_thresh{0.4};
			std::string prompt{};
			std::atomic<bool> _openset_ai_creating{false};
			// 在类的私有成员区域添加：
			// std::vector<joai::Result> _cached_openset_detect_ret;  // OpenSet AI结果缓存
			// std::mutex _openset_ai_result_mutex;                    // 保护缓存的互斥锁
			// std::atomic<bool> _openset_ai_result_ready{false};     // 标记是否有有效结果
			// std::atomic<bool> _openset_ai_running{false}; // 防止并发调用TensorRT
#endif
			std::vector<HeatmapData> heatmap_cache;
			const int cache_limit = 100; // 缓存 100 帧后写入文件
			
			bool _is_image_enhancer_on{};
			bool _is_image_stable_on{};
			std::atomic_bool _is_ai_on{ false };
			std::atomic_bool _is_openset_ai_on{false};
			std::atomic_bool _ai_status{ false };// AI是否打开的状态
			std::atomic_bool _is_ar_on{ false };
			std::atomic_bool _is_enhanced_ar_on{ false };
			std::atomic_bool _is_defog_on{ false };
			std::atomic_bool _is_stable_on{ false };
			std::atomic_bool _is_video_record_on{ false };
			std::atomic_bool _is_clip_video_record_on{ false };
			std::atomic_bool _is_snap_shot_on{ false };
			std::atomic_bool _is_ai_assist_track_on{ false };

			bool _ar_image_compute_state{ true };
			bool _is_should_throw_image{false};

			MetaDataProcessingPtr _meta_data_processing_pret{};
			MetaDataProcessingPtr _meta_data_processing_after{};
			std::map<int64_t, MetaDataWrap> _meta_data_cache{};

			std::string _ar_camera_config{};
			std::string _ar_vector_file{};
			std::atomic_bool _is_update_ar_mark_engine{ false };
			std::map<std::string, ArInfosInternal> _ar_mark_pixel_and_geographic_map{};
			std::queue<std::string> _video_mark_data{};
			std::string _mark_guid{};
			int64_t _video_mark_frame_count{};
			std::string _ar_mark_elements_guid{};

			int _ar_level_one_distance{};
			int _ar_level_two_distance{};

			double _ar_tower_height{0};
			bool _ar_is_tower = {1};
			bool _ar_buffer_sync_height = {0};
#ifdef ENABLE_AR
			jo::AIPosCor _ai_pos_cor{};
#endif
			std::queue<int> _ar_valid_point_index{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> _ar_image_compute_pixel_points{};
			std::vector<std::vector<cv::Point>> _ar_image_compute_pixel_lines{};
			std::vector<std::vector<cv::Point>> _ar_image_compute_pixel_warning_l1_regions{};
			std::vector<std::vector<cv::Point>> _ar_image_compute_pixel_warning_l2_regions{};
			std::vector<cv::Point> _pixel_points{};
			std::vector<std::vector<cv::Point>> _pixel_lines{};
			std::vector<std::vector<cv::Point>> _pixel_regions{};
#endif
			std::atomic<bool> _upload_image_is_done{true};
			std::chrono::system_clock::time_point _upload_image_time_point{};

			using upload_deduplication_map = std::map<int,std::pair<double,double>>;
			upload_deduplication_map _upload_dm_cls_lon_lat{};

			std::mutex _receive_pilot_data_mutex{};
			std::map<int64_t, PilotData> _pilot_data_map{};
			std::mutex _receive_payload_data_mutex{};
			std::map<int64_t, PayloadData> _payload_data_map{};
			PayloadData _payload_data{};//载荷元数据
			PilotData _pilot_data{};

			bool _is_should_flush_decoder{ true };

			std::string _record_file_prefix{};
			std::string _record_time_str{};
			std::string _task_id{};
			std::string _pull_url{};
			std::string _push_url{};
			std::vector<std::string> _push_urls{};//机载5g推流地址
			std::uint32_t _seq{};
			std::string _pilot_id{};

			std::shared_ptr<std::thread> _reactor_thread_ptr{};
			uint32_t _reactor_thread_looptimes{};
			std::atomic_bool _is_stop_reactor_loop{};
			common::NetWorkChecking::NetworkAdapterType _current_network_adapter_type;
			std::vector<DemuxerReactorPtr> _lan_demuxer_reactors{};
			std::vector<DemuxerReactorPtr> _wlan_demuxer_reactors{};
			std::int32_t _current_network_index{ -1 };
			std::string _adapter_ip{};
			uint8_t _adapter_num{};
			std::mutex _network_adapter_switch_mutex{};
			StreamRecoverCallback _stream_timeout_recover_callback{};
			std::chrono::milliseconds _open_timeout{};
			Demuxer::StopCallback _demuxer_stop_callback{};
			Demuxer::PacketCallback _demuxer_packet_callback{};

#ifdef ENABLE_AIRBORNE
			std::atomic<int> _opt_sensor_cmd{};
			std::atomic<int> _snap_num{};
			std::atomic<int> _sd_flag{};
			std::atomic<int> _sd_memory{};

			char* pIn420Dev{};
			char* pOutBgrDev{};
#endif
			static JoEdgeVersion _joedge_version;

        private:
            // 六轴 ar 座椅相关
            bool _ar_seat_thread_run{};
            std::shared_ptr<std::thread> _ar_seat_thread{};
            UdpSocket::Ptr _udp_socket{};

			int _apmode{};
			bool _is_first_get_value{ true };
			bool _is_forecast{};
			// 姿态角
			float _drone_pitch{};
			float _drone_pitch_latest{};
			float _drone_roll{};
			float _drone_roll_latest{};

			int _reopen_pusher_cnt{ 0 };
			double _target_lat{};
			double _target_lon{};
			double _target_alt{};
		private:
			DispatchTaskImpl(InitParameter init_parameter);
			DispatchTaskImpl(DispatchTaskImpl& other) = delete;
			DispatchTaskImpl(DispatchTaskImpl&& other) = delete;
			DispatchTaskImpl& operator=(DispatchTaskImpl& other) = delete;
			DispatchTaskImpl& operator=(DispatchTaskImpl&& other) = delete;
		};
	}
}

#endif // !EAPS_DIPATCH_TASK_IMPL_H
