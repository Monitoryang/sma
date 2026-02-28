#pragma once
#ifndef EAPS_NOTICE_CENTER_H
#define EAPS_NOTICE_CENTER_H

#include "EapsConfig.h"
#include "Logger.h"
#include "OnceToken.h"
#include "EapsUtils.h"
#include "Config.h"
#include "EapsCommon.h"
#include "ThreadPool.h"
#include "HttpClient.h"
#include "EapsDispatchCenter.h"
#include "Poco/Notification.h"
#include "Poco/NotificationCenter.h"
#include "Poco/Observer.h"
#include "Poco/TaskManager.h"
#include "Poco/ThreadPool.h"
#include "Poco/JSON/Object.h"
#include "Poco/Logger.h"
#include "rest_rpc.hpp"
#include "asio.hpp"

#include <mutex>
#include <memory>
#include <string>
#include <exception>
#include <functional>
#include <unordered_map>
#include <map>

namespace eap {
	namespace sma {
		class AddTaskNotice : public Poco::Notification
		{
		public:
			using InvokerCallback = std::function<void(bool result, std::string desc)>;

			AddTaskNotice(const std::string& params, InvokerCallback& invoker)
				: _params(params), _invoker(invoker) {
			}

			const std::string& getParams() const {
				return _params;
			}

			InvokerCallback getInvokerCallback() {
				return _invoker;
			}

		private:
			InvokerCallback _invoker;
			std::string _params;
		};

		class AISwitchNotice : public Poco::Notification
		{
		public:
			AISwitchNotice(const std::string& id, const bool& ret, const std::string& desc) :_id(id), _ret(ret), _desc(desc) {};

			std::string& getId() {
				return _id;
			}

			std::string& getDesc() {
				return _desc;
			}

			bool& getRet() {
				return _ret;
			}

		private:
			std::string _id;
			bool _ret;
			std::string _desc;

		};

		class AREngineResultNotice : public Poco::Notification
		{
		public:
			AREngineResultNotice(const std::string& id, const bool& ret, const std::string& desc) :_id(id), _ret(ret), _desc(desc) {};

			std::string& getId() {
				return _id;
			}

			std::string& getDesc() {
				return _desc;
			}

			bool& getRet() {
				return _ret;
			}

		private:
			std::string _id;
			bool _ret;
			std::string _desc;

		};

		class AddTaskResultNotice : public Poco::Notification
		{
		public:
			AddTaskResultNotice(const std::string& id, const int& ret, const std::string& desc, const std::string& duration, const std::string& duration_sd)
				:_id(id), _ret(ret), _desc(desc), _duration(duration), _duration_sd(duration_sd){};

			std::string& getId() {
				return _id;
			}

			std::string& getDesc() {
				return _desc;
			}

			std::string& getDuration() {
				return _duration;
			}

			std::string& getSdDuration()
			{
				return _duration_sd;
			}

			int& getRet() {
				return _ret;
			}

		private:
			std::string _id;
			int _ret;
			std::string _desc;
			std::string _duration;
			std::string _duration_sd;
		};

		class PlayBackMarkRecordNotice : public Poco::Notification
		{
		public:
			PlayBackMarkRecordNotice(const std::string& id, const bool& ret, const std::string& desc)
				:_id(id), _ret(ret), _desc(desc) {};

			std::string& getId() {
				return _id;
			}

			std::string& getDesc() {
				return _desc;
			}

			bool& getRet() {
				return _ret;
			}

		private:
			std::string _id;
			bool _ret;
			std::string _desc;
		};

		class RemoveTaskNotice : public Poco::Notification
		{
		public:
			RemoveTaskNotice(const bool& ret, const std::string& id) :_ret(ret), _id(id) {};

			std::string& getId() {
				return _id;
			}

			bool& getRet() {
				return _ret;
			}

		private:
			std::string _id;
			bool _ret;
		};

		class OnnxToEngineResultNotice : public Poco::Notification
		{
		public:
			OnnxToEngineResultNotice(const bool& ret, const std::string& desc) :_ret(ret), _desc(desc){};

			std::string& getDesc() {
				return _desc;
			}

			bool& getRet() {
				return _ret;
			}

		private:
			std::string _desc;
			bool _ret;
		};

		class FunctionUpdatedNotice : public Poco::Notification
		{
		public:
			FunctionUpdatedNotice(const std::string& id, const bool& update_func_result, const std::string& update_func_err_desc, const int& function_mask) 
			:_id(id), _update_func_result(update_func_result), _update_func_err_desc(update_func_err_desc), _function_mask(function_mask){};
			
			std::string& getId() {
				return _id;
			}

			bool& getUpdate_func_result() {
				return _update_func_result;
			}

			std::string& getUpdate_func_err_desc() {
				return _update_func_err_desc;
			}

			int& getFunction_mask() {
				return _function_mask;
			}

		private:
			std::string _id;
			bool _update_func_result;
			std::string _update_func_err_desc;
			int _function_mask;
		};

		class TaskStopedNotice : public Poco::Notification
		{
		public:
			TaskStopedNotice(const std::string& id, const std::string& reason, const std::string& pilot_id) :_id(id), _reason(reason), _pilot_id(pilot_id){};

			std::string& getId() {
				return _id;
			}

			std::string& getReason() {
				return _reason;
			}
			std::string& getPilotId() {
				return _pilot_id;
			}

		private:
			std::string _pilot_id{};
			std::string _id;
			std::string _reason;
		};

		class AddSyncTaskNotice : public Poco::Notification
		{
		public:
			AddSyncTaskNotice(DispatchTask::InitParameter param) : _init_paramter_task(param) {};
			DispatchTask::InitParameter getParam() {
				return _init_paramter_task;
			}
		private:
			DispatchTask::InitParameter _init_paramter_task{};
		};

		class AiAssistTrackResultNotice : public Poco::Notification
		{
		public:
			AiAssistTrackResultNotice(const std::string& id, const AiassistTrackResults& assistTrackResults, const std::string& desc) 
				:_id(id), _assistTrackResults(assistTrackResults), _desc(desc){};

			std::string& getId() {
				return _id;
			}

			const AiassistTrackResults& getAssistTrackResults() {
				return _assistTrackResults;
			}
			
			std::string& getDesc() {
				return _desc;
			}

		private:
			std::string _id;
			AiassistTrackResults _assistTrackResults;
			std::string _desc;
		};

		class SaveSnapShotResultNotice : public Poco::Notification
		{
		public:
			SaveSnapShotResultNotice(const std::string& id, const std::string& desc) 
				:_id(id), _desc(desc){};

			std::string& getId() {
				return _id;
			}
			
			std::string& getDesc() {
				return _desc;
			}

		private:
			std::string _id;
			std::string _desc;
		};
		//反馈视频问题,主要就是视频卡顿和视频断开
		class VideoMsgNotice : public Poco::Notification
		{
		public:
			enum class VideoCodeType {
				PtsBetweenLarge = 1, //两帧之间时间戳之差过大或过小，并且超过一定阈值
				DemuxerOpenFailed, //拉流失败
				PusherOpenFailed, //推流失败
				DecoderOpenFailed, //解码失败
				EncoderOpenFailed, //编码失败
				MuxerOpenFailed, //录像失败
				AIEngineIsNotExisted, //ai engine不存在
				AIEngineFormatErr, //ai engine文件存在但是没有序列化成功
				AIEngineParamsInvalid, //ai engine尺寸或者参数配置错误
				AREngineCreateFailed, //ar engine 创建失败
				AREngineInitFailed, //ar engine 初始化失败
				ARMarkEngineCreateFailed, //ar标注引擎创建失败
				UdpPacketLoss, //UDP丢包
				TcpBlock,         //TCP堵塞
				NetworkJitter,   //网络抖动
				PtsNotInCrement, //时间戳没有递增
			};

			VideoMsgNotice(const std::string id, const int code,  const std::string msg) :_id(id), _code(code), _msg(msg) {};

			std::string& getId() {
				return _id;
			}

			std::string& getMsg() {
				return _msg;
			}

			int getCode() {
				return _code;
			}

		private:
			std::string _id{};
			std::string _msg{};
			int _code{};
		};

		class Target
		{
		public:
			Target() {
			}

			static std::string makeBody(std::multimap<std::string, std::string>& params) {
				Poco::JSON::Object root;
				for (auto& param : params) {
					root.set(param.first, param.second);
				}

				return jsonToString(root);
			}

			std::string serializeAiassistTrackResults(const AiassistTrackResults& results) {
				Poco::JSON::Object jsonObject;
				jsonObject.set("track_cmd", results.track_cmd);
				jsonObject.set("track_pixelpos_x", results.track_pixelpos_x);
				jsonObject.set("track_pixelpos_y", results.track_pixelpos_y);
				jsonObject.set("track_pixelpos_w", results.track_pixelpos_w);
				jsonObject.set("track_pixelpos_h", results.track_pixelpos_h);

				std::stringstream ss;
				Poco::JSON::Stringifier::stringify(jsonObject, ss);
				return ss.str();
			}
			
			//反馈当前视频的一些问题
			void handleVideoMsgNotice(VideoMsgNotice* pNf) {
				auto code = pNf->getCode();
				auto id = pNf->getId();
				auto msg = pNf->getMsg();

				bool hook_enable{};
				std::string hook_video_msg{};
				try {
					if (!eap::configInstance().has(Hook::kOnVideoMsg)) {
						eap::configInstance().setString(Hook::kOnVideoMsg, "");
						eap::saveConfig();
					}
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_video_msg, Hook::kOnVideoMsg);
					hook_enable = my_hook_enable;
					hook_video_msg = my_hook_video_msg;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnAddTask throw exception: %s", e.what());
				}

				if (!hook_enable || hook_video_msg.empty()) {
					return;
				}
				eap::ThreadPool::defaultPool().start([this, code, id, msg, hook_video_msg]() {
					try {
						std::multimap<std::string, std::string> params
						{
							{"id", id},
							{"code",  std::to_string(code)},
							{"msg", msg}
						};

						auto body = makeBody(params);
						webHookApi(hook_video_msg, body);
					}
					catch (const std::exception& e) {
						eap_error(std::string(e.what()));
					}
				});
			}

			void handlePlayBackMarkRecordNotice(PlayBackMarkRecordNotice* pNf) {
				bool hook_enable{};
				static std::string hook_playback_record_result{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_playback_record_result, Hook::kPlayBackMarkRecord);
					hook_enable = my_hook_enable;
					hook_playback_record_result = my_hook_playback_record_result;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kPlayBackMarkRecord throw exception: %s", e.what());
				}

				if (!hook_enable || hook_playback_record_result.empty()) {
					return;
				}
				auto ret = pNf->getRet();
				auto desc = pNf->getDesc();
				auto id = pNf->getId();
				eap::ThreadPool::defaultPool().start([this, ret, desc, id]() {
					try {// TODO: type，可能是URL模式
						std::multimap<std::string, std::string> params
						{
							{"code", ret ? "0" : "-400"},
							{"result", ret ? "true" : "false"},
							{"msg", desc},
							{"id", id}
						};

						auto body = makeBody(params);
						webHookApi(hook_playback_record_result, body);
					}
					catch (const std::exception& e) {

						eap_error(e.what());
					}
				});
			}

			void handleAddTaskNotice(AddTaskNotice* pNf) {
				auto params = pNf->getParams();
				auto invoker = pNf->getInvokerCallback();

				bool hook_enable{};
				std::string hook_add_task{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_add_task, Hook::kOnAddTask);
					hook_enable = my_hook_enable;
					hook_add_task = my_hook_add_task;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnAddTask throw exception: %s", e.what());
				}
				
				if (!hook_enable || hook_add_task.empty()) {
					invoker(true, "success");
					return;
				}

				try {
					webHookApi(hook_add_task, params);
					invoker(true, "success");
				}
				catch (const std::exception& e) {
					invoker(false, e.what());
				}
			}

			void handleAddSyncTaskNotice(AddSyncTaskNotice* pNf)
			{
				auto params = pNf->getParam();
			}

			void handleTaskStopNotice(TaskStopedNotice* pNf)
			{
				bool hook_enable{};
				static std::string hook_task_stoped{};
				auto id = pNf->getId();
				auto reason = pNf->getReason();
				auto pilot_id = pNf->getPilotId();
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_task_stoped, Hook::kOnTaskStoped)
					hook_enable = my_hook_enable;
					hook_task_stoped = my_hook_task_stoped;
					
					DispatchCenter::Instance()->removeTask(id);
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnTaskStoped throw exception: %s", std::string(e.what()));
				}
				
				if (!hook_enable || hook_task_stoped.empty()) {
					return;
				}
				eap::ThreadPool::defaultPool().start([this, reason, id, pilot_id]() {
					try {
						std::multimap<std::string, std::string> params
						{
							{"id", id},
							{"code", "0"},
							{"reason", reason},
							{"pilot_id", pilot_id}
						};

						auto body = makeBody(params);
						webHookApi(hook_task_stoped, body);
					}
					catch (const std::exception& e) {
						eap_error(std::string(e.what()));
					}
				});
			}

			void webHookApi(std::string hook_api, std::string hook_data) {
				try {
					auto http_client = HttpClient::createInstance();
					eap_information_printf("web hoop api, api: %s, data: %s", hook_api, hook_data);
					http_client->doHttpRequest(hook_api, hook_data, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
						if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
							eap_information("web hoop api success");
						}
						else {
							eap_warning("web hoop api failed");
						}
					});
				}
				catch (const std::exception& e) {
					eap_error(e.what());
				}
			}
			
			void handleAISwitchNotice(AISwitchNotice* pNf) {
				bool hook_enable{};
				static std::string ai_switch{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_ai_switch, Hook::kOnAISwitched);
					hook_enable = my_hook_enable;
					ai_switch = my_ai_switch;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnAISwitched throw exception: %s", e.what());
				}

				if (!hook_enable || ai_switch.empty()) {
					return;
				}

				auto ret = pNf->getRet();
				auto desc = pNf->getDesc();
				auto id = pNf->getId();
				eap::ThreadPool::defaultPool().start([this, ret, desc, id]() {
					try {
						std::multimap<std::string, std::string> params
						{
							{"id", id},
							{"code", ret ? "0" : "-400"},
							{"result", ret ? "success" : "failed"},
							{"msg", !desc.empty() ? desc : "success"}
						};

						auto body = makeBody(params);
						webHookApi(ai_switch, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}

			void handleARSwitchNotice(AREngineResultNotice* pNf) {
				bool hook_enable{};
				static std::string ar_switch{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_ar_switch, Hook::kOnCreateArEngineReslut);
					hook_enable = my_hook_enable;
					ar_switch = my_ar_switch;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnCreateArEngineReslut throw exception: %s", e.what());
				}
				
				if (!hook_enable || ar_switch.empty()) {
					return;
				}

				auto ret = pNf->getRet();
				auto desc = pNf->getDesc();
				auto id = pNf->getId();
				eap::ThreadPool::defaultPool().start([this, ret, desc, id]() {
					try {
						std::multimap<std::string, std::string> params
						{
							{"id", id},
							{"code", ret ? "0" : "-400"},
							{"result", ret ? "success" : "failed"},
							{"msg", !desc.empty() ? desc : "success"}
						};

						auto body = makeBody(params);
						webHookApi(ar_switch, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}

			void handleAddTaskResultNotice(AddTaskResultNotice* pNf)
			{
				bool hook_enable{};
				static std::string hook_add_task_result{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_add_task_result, Hook::kOnAddTaskResult);
					hook_enable = my_hook_enable;
					hook_add_task_result = my_hook_add_task_result;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnAddTaskResult throw exception: %s", e.what());
				}
						
				if (!hook_enable || hook_add_task_result.empty()) {
					return;
				}
				auto ret = pNf->getRet();
				auto desc = pNf->getDesc();
				auto id = pNf->getId();
				auto duration = pNf->getDuration();
				auto sd_duration = pNf->getSdDuration();
				eap::ThreadPool::defaultPool().start([this, ret, desc, id, duration, sd_duration]() {
					try {// TODO: type，可能是URL模式
						std::multimap<std::string, std::string> params
						{
							{"code", std::to_string(ret)},
							{"result", (ret == 0) ? "true" : "false"},
							{"duration", duration},
							{"sd_duration", sd_duration},
							{"msg", desc},
							{"id", id}
						};
						
						auto body = makeBody(params);
						webHookApi(hook_add_task_result, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}

			void handleRemoveTaskNotice(RemoveTaskNotice* pNf)
			{
				bool hook_enable{};
				static std::string hook_remove_task{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_remove_task, Hook::kOnRemoveTask);
					hook_enable = my_hook_enable;
					hook_remove_task = my_hook_remove_task;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnRemoveTask throw exception: %s", e.what());
				}

				if (!hook_enable || hook_remove_task.empty()) {
					return;
				}
				auto ret = pNf->getRet();
				auto id = pNf->getId();
				eap::ThreadPool::defaultPool().start([this, ret, id]() {
					try {// TODO: type，可能是URL模式
						std::multimap<std::string, std::string> params
						{
							{"result", ret ? "true" : "false"},
							{"code", ret ? "0" : "-400"},
							{"id", id}
						};

						auto body = makeBody(params);
						webHookApi(hook_remove_task, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}

			void handleOnnxToEngineResultNotice(OnnxToEngineResultNotice* pNf)
			{
				bool hook_enable{};
				static std::string hook_onnx_to_engine_result{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_onnx_to_engine_result, Hook::kOnOnnxToEngineResult);
					hook_enable = my_hook_enable;
					hook_onnx_to_engine_result = my_hook_onnx_to_engine_result;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnOnnxToEngineResult throw exception: %s", e.what());
				}
				
				if (!hook_enable || hook_onnx_to_engine_result.empty()) {
					return;
				}
				auto ret = pNf->getRet();
				auto desc = pNf->getDesc();
				eap::ThreadPool::defaultPool().start([this, ret, desc]() {
					try {// TODO: type，可能是URL模式
						std::multimap<std::string, std::string> params
						{
							{"code", ret ? "0" : "-400"},
							{"result", ret ? "success" : "failed"},
							{"msg", desc},
						};

						auto body = makeBody(params);
						webHookApi(hook_onnx_to_engine_result, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}

			void handleFunctionUpdatedNotice(FunctionUpdatedNotice* pNf)
			{
				bool hook_enable{};
				static std::string hook_function_updated{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_function_updated, Hook::kOnFunctionUpdated);
					hook_enable = my_hook_enable;
					hook_function_updated = my_hook_function_updated;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnFunctionUpdated throw exception: %s", e.what());
				}

				if (!hook_enable || hook_function_updated.empty()) {
					return;
				}
				auto id = pNf->getId();
				auto update_func_result = pNf->getUpdate_func_result();
				auto update_func_err_desc = pNf->getUpdate_func_err_desc();
				auto function_mask = pNf->getFunction_mask();
				eap::ThreadPool::defaultPool().start([this, id, update_func_result, update_func_err_desc, function_mask]() {
					try {
						std::multimap<std::string, std::string> params
						{
							{"id", id},
							{"code", "0"},
							{"result", update_func_result ? "success" : "failed"},
							{"msg", !update_func_err_desc.empty()? update_func_err_desc : "success"},
							{"func_mask", std::to_string(function_mask)}
						};

						auto body = makeBody(params);
						webHookApi(hook_function_updated, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}

			void handleSaveSnapShotResultNotice(SaveSnapShotResultNotice* pNf)
			{
				bool hook_enable{};
				static std::string hook_save_snapshot_result{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_save_snapshot_result, Hook::kOnSaveSnapShotResult);
					hook_enable = my_hook_enable;
					hook_save_snapshot_result = my_hook_save_snapshot_result;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnSaveSnapShotResult throw exception: %s", e.what());
				}

				if (!hook_enable || hook_save_snapshot_result.empty()) {
					return;
				}
				auto id = pNf->getId();
				auto desc = pNf->getDesc();

				eap::ThreadPool::defaultPool().start([this, id, desc]() {
					try {// TODO: type，可能是URL模式
						std::multimap<std::string, std::string> params
						{
							{"id", id},
							{"code", "0"},
							{"msg", desc}
						};

						auto body = makeBody(params);
						webHookApi(hook_save_snapshot_result, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}

			void handleAiAssistTrackResultNotice(AiAssistTrackResultNotice* pNf)
			{
				bool hook_enable{};
				static std::string hook_ai_assist_track_result{};
				try {
					GET_CONFIG(bool, getBool, my_hook_enable, Hook::kEnable);
					GET_CONFIG(std::string, getString, my_hook_ai_assist_track_result, Hook::kOnAiAssistTrackResult);
					hook_enable = my_hook_enable;
					hook_ai_assist_track_result = my_hook_ai_assist_track_result;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnable or kOnAiAssistTrackResult throw exception: %s", e.what());
				}

				if (!hook_enable || hook_ai_assist_track_result.empty()) {
					return;
				}
				auto id = pNf->getId();
				auto assistTrackResults = pNf->getAssistTrackResults();
				auto desc = pNf->getDesc();
				auto serializedResults = serializeAiassistTrackResults(assistTrackResults);

				eap::ThreadPool::defaultPool().start([this, id, serializedResults, desc]() {
					try {// TODO: type，可能是URL模式
						std::multimap<std::string, std::string> params
						{
							{"id", id},
							{"code", "0"},
							{"assistTrackResults", serializedResults},
							{"mag", desc}
						};

						auto body = makeBody(params);
						webHookApi(hook_ai_assist_track_result, body);
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
				});
			}
			
			void setRpcServer(rest_rpc::rpc_service::rpc_server* server)
			{
				_rpc_server = server;
			}

		private:
			rest_rpc::rpc_service::rpc_server* _rpc_server{ NULL };
		};

		class NoticeCenter;
		using NoticeCenterPtr = std::shared_ptr<NoticeCenter>;
		class NoticeCenter {
		public:

			static NoticeCenterPtr &Instance()
			{
				if (!s_instance) {
					s_instance = NoticeCenterPtr(new NoticeCenter());
				}
				return s_instance;
			}

			void setRpcServer(rest_rpc::rpc_service::rpc_server* server)
			{
				_target.setRpcServer(server);
			}

			void addObservers()
			{
				_notice_center.addObserver(Poco::Observer<Target, AddTaskNotice>((_target), &Target::handleAddTaskNotice));
				_notice_center.addObserver(Poco::Observer<Target, TaskStopedNotice>(_target, &Target::handleTaskStopNotice));
				_notice_center.addObserver(Poco::Observer<Target, AddSyncTaskNotice>(_target, &Target::handleAddSyncTaskNotice));
				_notice_center.addObserver(Poco::Observer<Target, AISwitchNotice>(_target, &Target::handleAISwitchNotice));
				_notice_center.addObserver(Poco::Observer<Target, AREngineResultNotice>(_target, &Target::handleARSwitchNotice));
				_notice_center.addObserver(Poco::Observer<Target, AddTaskResultNotice>(_target, &Target::handleAddTaskResultNotice));
				_notice_center.addObserver(Poco::Observer<Target, RemoveTaskNotice>(_target, &Target::handleRemoveTaskNotice));
				_notice_center.addObserver(Poco::Observer<Target, OnnxToEngineResultNotice>(_target, &Target::handleOnnxToEngineResultNotice));
				_notice_center.addObserver(Poco::Observer<Target, FunctionUpdatedNotice>(_target, &Target::handleFunctionUpdatedNotice));
				_notice_center.addObserver(Poco::Observer<Target, AiAssistTrackResultNotice>(_target, &Target::handleAiAssistTrackResultNotice));
				_notice_center.addObserver(Poco::Observer<Target, SaveSnapShotResultNotice>(_target, &Target::handleSaveSnapShotResultNotice));
				_notice_center.addObserver(Poco::Observer<Target, PlayBackMarkRecordNotice>(_target, &Target::handlePlayBackMarkRecordNotice));
				_notice_center.addObserver(Poco::Observer<Target, VideoMsgNotice>(_target, &Target::handleVideoMsgNotice));
			}

			void removeObservers()
			{
				_notice_center.removeObserver(Poco::Observer<Target, AddTaskNotice>(_target, &Target::handleAddTaskNotice));
				_notice_center.removeObserver(Poco::Observer<Target, TaskStopedNotice>(_target, &Target::handleTaskStopNotice));
				_notice_center.removeObserver(Poco::Observer<Target, AISwitchNotice>(_target, &Target::handleAISwitchNotice));
				_notice_center.removeObserver(Poco::Observer<Target, AddSyncTaskNotice>(_target, &Target::handleAddSyncTaskNotice));
				_notice_center.removeObserver(Poco::Observer<Target, AREngineResultNotice>(_target, &Target::handleARSwitchNotice));
				_notice_center.removeObserver(Poco::Observer<Target, AddTaskResultNotice>(_target, &Target::handleAddTaskResultNotice));
				_notice_center.removeObserver(Poco::Observer<Target, RemoveTaskNotice>(_target, &Target::handleRemoveTaskNotice));
				_notice_center.removeObserver(Poco::Observer<Target, OnnxToEngineResultNotice>(_target, &Target::handleOnnxToEngineResultNotice));
				_notice_center.removeObserver(Poco::Observer<Target, FunctionUpdatedNotice>(_target, &Target::handleFunctionUpdatedNotice));
				_notice_center.removeObserver(Poco::Observer<Target, AiAssistTrackResultNotice>(_target, &Target::handleAiAssistTrackResultNotice));
				_notice_center.removeObserver(Poco::Observer<Target, SaveSnapShotResultNotice>(_target, &Target::handleSaveSnapShotResultNotice));
				_notice_center.removeObserver(Poco::Observer<Target, PlayBackMarkRecordNotice>(_target, &Target::handlePlayBackMarkRecordNotice));
				_notice_center.removeObserver(Poco::Observer<Target, VideoMsgNotice>(_target, &Target::handleVideoMsgNotice));
			}

			Poco::NotificationCenter& getCenter()
			{
				return _notice_center;
			}

		private:
			NoticeCenter()
			{
			}

		private:
			Target _target;
			Poco::NotificationCenter _notice_center;
			static NoticeCenterPtr s_instance;
		};
	}
}

#endif // !EAPS_NOTICE_CENTER_H