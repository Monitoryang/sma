#ifndef EAPS_DIPATCH_TASK
#define EAPS_DIPATCH_TASK

#include "EapsMacros.h"
#include "EapsDemuxerTradition.h"
#include "EapsDemuxerRtc.h"
#include "EapsDecoder.h"
#include "EapsEncoderNvenc.h"
#include "EapsPusherTradition.h"
#include "EapsPusherRtc.h"
#include "EapsMuxer.h"
#include "EapsDispatchCommon.h"

namespace eap {
	namespace sma {
		class DispatchTask;
		using DispatchTaskPtr = std::shared_ptr<DispatchTask>;

		class DispatchTask
		{
		public:
			struct InitParameter
			{
				std::string id{};
				std::string pull_url{};
				std::string push_url{};
				int func_mask{};
				std::string ar_vector_file{};				
				std::string ar_camera_config{};
				std::string record_file_prefix{};//在新建任务前就从配置文件中读取后缀，然后以参数形式传入，才好区分hd和sd
				std::string record_time_str{};//在新建任务前就从配置文件中读取后缀，然后以参数形式传入，才好让hd和sd是同一个timestamp
				bool is_pilotcontrol_task{ false };  //是否为pilotControl推流、分发任等默认任务
				bool need_decode{ false };//机载是否需要解码，默认可见光任务需要解码
				//多路视频
				std::vector<std::string> _pull_urls{};
				std::vector<std::string> _push_urls{};
			};
			//板子设备状态回调
			using DeviceInfoCallback = std::function<void(std::string content)>;
			using ExceptionCallback = std::function<void(std::string, std::string)>;
		public:
			DispatchTask(InitParameter& init_parameter);
			virtual ~DispatchTask();

			virtual void setAppStream(std::string in_app, std::string in_stream,
				std::string out_app, std::string out_stream) {
				_in_app = in_app;
				_in_stream = in_stream;
				_out_app = out_app;
				_out_stream = out_stream;
			}

			virtual void setId(std::string id) { _id = id; }

			virtual void setTaskType(TaskType task_type) { _task_type = task_type; }

			virtual std::string getInApp() { return _in_app; }
			virtual std::string getInStream() { return _in_stream; }
			virtual std::string getOutApp() { return _out_app; }
			virtual std::string getOutStream() { return _out_stream; }

			virtual TaskType getTaskType() { return _task_type; }

			virtual std::string getId() { return _id; }

			virtual std::string getPullUrl() { return _init_parameter.pull_url; }
			virtual std::string getPushUrl() { return _init_parameter.push_url; }
			virtual std::vector<std::string> getPullUrls() { return _init_parameter._pull_urls; }
			virtual std::vector<std::string> getPushUrls() { return _init_parameter._push_urls; }
			virtual int64_t getFramerate() { return 0; }
			virtual int getFunctionMask() { return _init_parameter.func_mask; }
			virtual bool isAREnabled() { return ((_init_parameter.func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR) ? true : false; }
			virtual bool isAIEnabled() { return ((_init_parameter.func_mask & FUNCTION_MASK_AI) == FUNCTION_MASK_AI) ? true : false; }
			virtual bool isAiAssistTrackEnabled() { return ((_init_parameter.func_mask & FUNCTION_MASK_AI_ASSIST_TRACK) == FUNCTION_MASK_AI_ASSIST_TRACK) ? true : false; }
			virtual std::string getRecordFilePrefix() { return _init_parameter.record_file_prefix; };
			virtual std::string getRecordTimeStr() { return _init_parameter.record_time_str; };

			virtual void start() = 0;
			virtual void stop() = 0;
			virtual void updateFuncMask(int func_mask, std::string ar_camera_config = std::string(), std::string ar_vector_file = std::string()) = 0;
			virtual void clipSnapShotParam(int time_count) = 0;

#ifdef ENABLE_AIRBORNE
			virtual void receivePilotData(std::string param_str) = 0;
			virtual void receivePayloadData(std::string param_str) = 0;
#endif // ENABLE_AIRBORNE

			virtual void addAnnotationElements(std::string ar_camera_path, std::string annotation_elements_json, bool is_hd=true) = 0;
			virtual void deleteAnnotationElements(std::string mark_guid, bool is_hd=true) = 0;

			virtual std::string aiAssistTrack(int track_cmd, int track_pixelpos_x, int track_pixelpos_y) = 0;
			virtual void saveSnapShot() = 0;
			// virtual std::vector<std::tuple<std::vector<long>, std::vector<int>>> getHeatmapTotal() = 0;

			virtual void updateArLevelDistance(int level_one_distance, int level_two_distance) = 0;
			virtual void updateArTowerHeight(bool is_tower, double tower_height, bool buffer_sync_height) = 0;
			virtual void updateAiCorPos(int ai_pos_cor) = 0;
			virtual std::vector<int64_t> getVideoDuration() = 0;
			virtual void setSeekPercent(float percent) = 0;
			virtual void pause(int paused) = 0;

			virtual void setDeviceInfoCallback(DeviceInfoCallback device_callabck) {
				_device_infos_callback = device_callabck;
			};
			virtual void setExceptionCallback(ExceptionCallback cb){ 		_exception_callback = cb; 
			}
			virtual void snapshot(std::string recordNo, int interval, int total_time);
			virtual void videoClipRecord(int record_duration, std::string recordNo);
			//森防项目，第一次火点定位出结果的时候通知云端sma，快照图片给云平台
			virtual void fireSearchInfo(const std::string id, const double target_lat, const double target_lon, const double target_alt);
			//机载链路切换，是否走4、5G链路
			virtual void setAirborne45G(const int airborne_45G);
		protected:
			InitParameter _init_parameter{};

			std::string _id{};

			TaskType _task_type{};

			std::string _in_app;
			std::string _in_stream;
			std::string _out_app;
			std::string _out_stream;
			DeviceInfoCallback _device_infos_callback{};
			ExceptionCallback _exception_callback{};
			int _record_duration{};  //ms
			bool _snapshot{};
			bool _snapshot_sd{};
			bool _record{};
			bool _record_sd{};
			std::string _recordNo{};
		private:
			DispatchTask(DispatchTask& other) = delete;
			DispatchTask(DispatchTask&& other) = delete;
			DispatchTask& operator=(DispatchTask& other) = delete;
			DispatchTask& operator=(DispatchTask&& other) = delete;
		};
	}
}

#endif // !EAPS_DIPATCH_TASK