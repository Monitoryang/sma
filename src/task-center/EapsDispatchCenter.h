#ifndef EAPS_DISPATCH_CENTER
#define EAPS_DISPATCH_CENTER

#include "EapsDispatchTaskImpl.h"
#include "EapsDispatchTaskImplMultiple.h"
#include "EapsPlaybackAnnotation.h"
#include "rest_rpc.hpp"
#include <atomic>
#include <list>
#include <deque>
#include <unordered_map>
#include <functional>

namespace eap {
	namespace sma {
		using DispatchTaskMap = std::unordered_map<std::string, DispatchTaskPtr>;
		using RecordTaskMap = std::unordered_map<std::string, PlaybackAnnotationPtr>;

		class DispatchCenter;
		using DispatchCenterPtr = std::shared_ptr<DispatchCenter>;

		class DispatchCenter
		{
		public:
			static DispatchCenterPtr Instance();

			static void cudaWarmUp();

		public:
			virtual ~DispatchCenter();

			void start();
			void stop();

			// void addDefaultTasks(std::string& visual_guid, std::string& infrared_guid);
			void stopDefaultMonitorTask();
			void addDefaultVisualTasks(std::string& visual_guid);
			void addDefaultInfraredTasks(std::string& infrared_guid);
			void addTask(DispatchTask::InitParameter init_paramter);
			void addTaskMultiple(DispatchTask::InitParameter init_paramter);
			void addTaskRecord(PlaybackAnnotation::InitParam init_paramter);
			bool removeTask(std::string id);

			void updateFuncMask(std::string id, int function_mask, std::string ar_camera_config = std::string(), std::string ar_vector_file = std::string(), int time_count=0);
			void updateTaskInfo(std::string id, int function_mask, std::string ar_camera_config = std::string(), std::string ar_vector_file = std::string());
			void receivePilotData(std::string id, std::string param_str);
			void receivePayloadData(std::string id, std::string param_str);
			void receiveVersinData(std::string version_data);
			void clipSnapShotParam(std::string id, int time_count);

			void addAnnotationElements(std::string id, std::string ar_camera_path, std::string annotation_elements_json, bool isHd=true);
			void deleteAnnotationElements(std::string id, std::string mark_guid, bool isHd=true);
			void enableOnnxToEngine(bool is_fp16 = false,std::string inputNamed="", std::string shape="");
			float getOnnxToEnginePercent();
			std::string aiAssistTrack(std::string id, int track_cmd, int track_pixelpos_x, int track_pixelpos_y);
			void saveSnapShot(std::string id);
			std::vector<std::tuple<std::vector<long>, std::vector<int>>> getHeatmapTotal(std::string id);

			void updateArLevelDistance(std::string id, int level_one_distance, int level_two_distance);
			void updateArTowerHeight(std::string id, bool is_tower, double tower_height, bool buffer_sync_height);
			void updateAiPosCor(std::string id, int ai_pos_cor);
			void setSeekPercent(std::string id, float percent);
			void pause(std::string id, int paused);

			DispatchTaskPtr findTaskPullUrl(const std::string pull_url);
			bool findTaskUrl(const std::string pull_url, const std::string push_url, DispatchTaskPtr task_ptr= nullptr);
			bool findTaskMultiple(const std::vector<std::string> pull_urls, const std::vector<std::string> push_urls);
			DispatchTaskPtr findTaskId(const std::string id);

			void for_each_task(const std::function<void(const DispatchTaskPtr &task)> &cb);
			void setRpcServer(rest_rpc::rpc_service::rpc_server* server);
			void saveTaskInfo(DispatchTask::InitParameter init_paramter);
			void deleteTaskFile(std::string id);
			void removeTaskUrl(const std::string pull_url, const std::string push_url);
			int getTaskCount();
			void snapshot(std::string id, std::string recordNo, int interval, int total_time);
			void videoClipRecord(std::string id, int record_duration, std::string recordNo);
			//更新拉流 推流地址，删除原任务，重新创建
			void updatePullUrl(const std::string id, const std::string pull_url, std::string push_url = "webrtc://127.0.0.1:3456/main/hd");
			//森防项目，第一次火点定位出结果的时候通知云端sma，快照图片给云平台
			std::string fireSearchInfo(const std::string id, const double target_lat, const double target_lon, const double target_alt);
			//机载链路切换，是否走4、5G链路
			void setAirborne45G(std::string id, const int airborne_45G);

			bool startDefalutTask();
			bool stopDefalutTask();
private:
			bool allStringNonEmpty(const std::vector<std::string> strs);
			void clearTasksFile(); //星图启动或者停止时清空tasks文件夹里的视频任务文件

private:
			std::mutex _task_file_mutex{};
			std::mutex _task_mutex{};
			std::mutex _task_mutex_multiple{};
			std::mutex _record_task_mutex{};
			DispatchTaskMap _dispatch_task_map{};
			DispatchTaskMap _dispatch_task_map_default{};
			DispatchTaskMap _dispatch_task_map_multiple{};
			RecordTaskMap _record_task_map{};
			float _onnx_to_engine_percent{};
			rest_rpc::rpc_service::rpc_server* _rpc_server{ NULL };
			static std::mutex s_inst_mutex;
			static DispatchCenterPtr s_instance;
			std::set<std::pair<std::string, std::string>> _tasks_url_set;//key task pull urls,value task push urls

		private:
			DispatchCenter();
			DispatchCenter(DispatchCenter& other) = delete;
			DispatchCenter(DispatchCenter&& other) = delete;
			DispatchCenter& operator=(DispatchCenter& other) = delete;
			DispatchCenter& operator=(DispatchCenter&& other) = delete;
		};
	}
}

#endif // !EAPS_DISPATCH_CENTER