#include "RestRpcApi.h"
#include "OnceToken.h"
#include "Logger.h"
#include "Config.h"
#include "EapsUtils.h"
#include "EapsDispatchTask.h"
#include "EapsDispatchCenter.h"
#include "EapsConfig.h"
#include "EapsNoticeCenter.h"
#include "Utils.h"

#include "Poco/File.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/PartHandler.h"
#include "Poco/Net/PartSource.h"
#include "Poco/DirectoryIterator.h"

#include <fstream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <string>

namespace eap {
	namespace sma {
		NoticeCenterPtr NoticeCenter::s_instance{};

		static const std::string ar_file_path = exeDir() + "ar_file/";
		static const std::string ai_engine_path = exeDir() + "ai_file/engine";
		static const std::string ai_onnx_path = exeDir() + "ai_file/onnx";
		static const std::string ai_text_encode_path = exeDir() + "ai_file/text_encoder";
		static std::shared_ptr<std::thread> s_rpc_run_thread{};
		static std::shared_ptr<rest_rpc::rpc_service::rpc_server> s_rpc_server{};
		std::string ai_onnx_path_all;
		std::string ai_file_path_all;
		std::string ai_text_encode_path_all;
		static bool s_open = true;
		std::mutex _task_mutex{};
		static std::mutex _defual_task_mutex;
		bool _exe_status{false};


		static void makeJsonDefault(ApiErr apierr, std::string msg, Poco::JSON::Object& json, std::string id = "")
		{
			json.set("code", static_cast<int>(apierr));
			json.set("msg", msg);
			if (!id.empty()) {
				json.set("id", id);
			}
		}

		static void makeJsonSetConfig(ApiErr apierr, int changed, Poco::JSON::Object& json)
		{
			json.set("code", static_cast<int>(apierr));
			json.set("changed", changed);
		}

		static Poco::JSON::Object makeTaskInfoJson(const DispatchTaskPtr& task)
		{
			Poco::JSON::Object val;
			auto pull_vector = task->getPullUrls();
			auto push_vector = task->getPushUrls();
			if (pull_vector.size()) {
				val.set("pull_url", pull_vector[0]);
				val.set("push_url", push_vector[0]);
				val.set("pull_sd_url", pull_vector[pull_vector.size() - 1]);
				val.set("push_sd_url", push_vector[push_vector.size() - 1]);
			}
			else {
				val.set("pull_url", task->getPullUrl());
				val.set("push_url", task->getPushUrl());
			}
			val.set("ai_enable", task->isAIEnabled());
			val.set("ar_enable", task->isAREnabled());
			val.set("aiassisttrack_enable", task->isAiAssistTrackEnabled());
			val.set("func_mask", task->getFunctionMask());
			val.set("id", task->getId());
			return val;
		}

		static void makeTaskInfo(const DispatchTaskPtr& task, Poco::JSON::Object& val)
		{
			val.set("code", static_cast<int>(ApiErr::Success));
			auto pull_vector = task->getPullUrls();
			auto push_vector = task->getPushUrls();
			if (pull_vector.size()) {
				val.set("pull_url", pull_vector[0]);
				val.set("push_url", push_vector[0]);
				val.set("pull_sd_url", pull_vector[pull_vector.size() - 1]);
				val.set("push_sd_url", push_vector[push_vector.size() - 1]);
			}
			else {
				val.set("pull_url", task->getPullUrl());
				val.set("push_url", task->getPushUrl());
			}
			val.set("ai_enable", task->isAIEnabled());
			val.set("ar_enable", task->isAREnabled());
			val.set("aiassisttrack_enable", task->isAiAssistTrackEnabled());
			val.set("id", task->getId());
			val.set("func_mask", task->getFunctionMask());
		}

		static void osArFile(std::string& file_name, std::string& file_path, std::string& id, std::ofstream& os_stream,
			std::string& suffix, std::string& fileStream)
		{
			file_name = file_path + id + suffix;
			if (fileExist(file_name)) {
				deleteFile(file_name);
			}

			// 打开源文件并读取其内容到缓冲区
			std::ifstream in_file(fileStream, std::ios::binary);
			if (!in_file) {
				eap_warning("无法打开源文件!");
			}
			// 获取文件大小
			in_file.seekg(0, std::ios::end);
			std::streampos file_size = in_file.tellg();
			in_file.seekg(0, std::ios::beg);
			// 为文件内容分配足够的内存
			std::vector<char> buffer(file_size);
			// 读取文件内容到缓冲区
			if (!in_file.read(buffer.data(), file_size)) {
				eap_warning("读取源文件内容时发生错误!");
				in_file.close();
			}
			in_file.close();

			os_stream.open(file_name, std::ios::binary | std::ios::out | std::ios::trunc);
			if (!os_stream.is_open()) {
				throw std::invalid_argument("open ar out stream fail");
			}
			os_stream.write(buffer.data(), file_size);
			os_stream.flush();
			os_stream.close();

			Poco::File file(fileStream);
			try {
				if (file.exists()) {
					file.remove();
				}
			}
			catch (const Poco::Exception& e) {
				// 捕获并处理异常
				std::cerr << "Error removing file: " << e.displayText() << std::endl;
			}
		}

		bool hasSubTree(const Poco::Util::AbstractConfiguration& config, const std::string& key)
		{
			Poco::Util::AbstractConfiguration::Keys keys;
			config.keys(key, keys);
			return !keys.empty();
		}

		Poco::Dynamic::Var getConfigValue(const Poco::Util::AbstractConfiguration& config, const std::string& key)
		{
			try {
				return config.getString(key);
			}
			catch (Poco::NotFoundException&) {
				try {
					return config.getInt(key);
				}
				catch (Poco::NotFoundException&) {
					try {
						return config.getDouble(key);
					}
					catch (Poco::NotFoundException&) {
						try {
							return config.getBool(key);
						}
						catch (Poco::NotFoundException&) {
							// 可以继续添加其他类型的尝试，如日期、数组等
							throw;
						}
					}
				}
			}
		}

		void addNestedKeysToJSONObject(const std::string& prefix, const Poco::Util::AbstractConfiguration& config, Poco::JSON::Object& jsonObj)
		{
			Poco::Util::AbstractConfiguration::Keys keys;
			config.keys(prefix, keys);
			for (const auto& key : keys) {
				std::string nestedKey = prefix.empty() ? key : prefix + "." + key;
				if (hasSubTree(config, nestedKey)) {
					addNestedKeysToJSONObject(nestedKey, config, jsonObj);
				}
				else {
					Poco::Dynamic::Var value = getConfigValue(config, nestedKey);
					jsonObj.set(nestedKey, value);
				}
			}
		}

		static void writeArCameraFile(std::string& ar_vector_file_path, std::string& ar_camera_file_path, std::string& id, std::map<std::string, std::string> ar_file)
		{
			std::string ar_file_path_all = ar_file_path + id + std::string("/");
			bool result = createPath(ar_file_path_all);
			if (!result) {
				throw std::runtime_error("create ar file path fail");
			}

			std::ofstream os;
			std::string kmz_file_name{};
			std::string kml_file_name{};
			std::string ar_vector_file_key = "ar_vector";
			std::string ar_camera_file_key = "ar_camera";

			//camera config
			auto ar_camera_file_iter = ar_file.find(ar_camera_file_key);
			if (ar_file.end() != ar_camera_file_iter) {
				auto ar_camera_file = ar_camera_file_iter->second;
				if (ar_camera_file.empty()) {
					throw std::invalid_argument("camera file is null");
				}
				auto off = ar_camera_file.find_last_of('.');
				std::string suffix = ar_camera_file.substr(off);
				if (suffix != std::string(".config")) {
					throw std::invalid_argument("camera file suffix is not .config");
				}
				osArFile(ar_camera_file_path, ar_file_path_all, id, os, suffix, ar_camera_file);
			}
			else {
				throw std::invalid_argument("not have camera file, can not init ar mark engine");
			}

			//vector file
			auto ar_vector_file_iter = ar_file.find(ar_vector_file_key);
			if (ar_file.end() != ar_vector_file_iter) {
				auto ar_vector_file = ar_vector_file_iter->second;
				if (ar_vector_file.empty()) {
					eap_warning("vector file is null");
				}
				auto off = ar_vector_file.find_last_of('.');
				std::string suffix = ar_vector_file.substr(off);
				if (suffix != std::string(".kml") && suffix != std::string(".kmz")) {
					eap_warning("vector file suffix is not .kml or .kmz");
				}
				osArFile(ar_vector_file_path, ar_file_path_all, id, os, suffix, ar_vector_file);
			}
			else {
				eap_warning("not have vector file");
			}
		}

		//write camera.config  .kml
		static void writeArFile(std::string& ar_vector_file_path, std::string& ar_camera_file_path, std::string& id, std::map<std::string, std::string> ar_file)
		{
			std::string ar_file_path_all = ar_file_path + id;
			bool result = createPath(ar_file_path_all);
			if (!result) {
				throw std::runtime_error("create ar file path fail");
			}

			std::ofstream os;
			std::string kmz_file_name{};
			std::string kml_file_name{};
			std::string ar_vector_file_key = "ar_vector";
			std::string ar_camera_file_key = "ar_camera";

			//camera config
			auto ar_camera_file_iter = ar_file.find(ar_camera_file_key);
			if (ar_file.end() != ar_camera_file_iter) {
				auto ar_camera_file = ar_camera_file_iter->second;
				if (ar_camera_file.empty()) {
					throw std::invalid_argument("camera file is null");
				}
				auto off = ar_camera_file.find_last_of('.');
				std::string suffix = ar_camera_file.substr(off);
				if (suffix != std::string(".config")) {
					throw std::invalid_argument("camera file suffix is not .config");
				}
				osArFile(ar_camera_file_path, ar_file_path_all, id, os, suffix, ar_camera_file);
			}
			else {
				eap_warning("not have camera file, can not init ar mark engine");
			}

			//vector file
			auto ar_vector_file_iter = ar_file.find(ar_vector_file_key);
			if (ar_file.end() != ar_vector_file_iter) {
				auto ar_vector_file = ar_vector_file_iter->second;
				if (ar_vector_file.empty()) {
					eap_warning("vector file is null");
				}
				auto off = ar_vector_file.find_last_of('.');
				std::string suffix = ar_vector_file.substr(off);
				if (suffix != std::string(".kml") || suffix != std::string(".kmz")) {
					eap_warning("vector file suffix is not .kml or .kmz");
				}
				osArFile(ar_vector_file_path, ar_file_path_all, id, os, suffix, ar_vector_file);
			}
			else {
				eap_warning("not have vector file");
			}
		}

		void installRestrpc(unsigned int port)
		{
			s_rpc_server = std::make_shared<rest_rpc::rpc_service::rpc_server>(port, std::thread::hardware_concurrency());

			static std::string api_secret{};
			try {
				GET_CONFIG(std::string, getString, my_api_secret, API::kSecret);
				api_secret = my_api_secret;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kSecret throw exception: %s", e.what());
			}

			NoticeCenter::Instance()->setRpcServer(s_rpc_server.get());
			NoticeCenter::Instance()->addObservers();
			DispatchCenter::Instance()->setRpcServer(s_rpc_server.get());

#ifdef  ENABLE_AIRBORNE
			eap::ThreadPool::defaultPool().start([]()
			{
				while (s_open) {//在机载端的时候，必须默认的两个任务启动成功，才能继续后续操作，然后本微服务才会给主服务上报注册信息
					std::string visual_guid{};
					try {
						GET_CONFIG(std::string, getString, my_visual_guid, Media::kVisualGuid);
						GET_CONFIG(std::string, getString, my_visual_input_url, Media::kVisualInputUrl);
						GET_CONFIG(std::string, getString, my_visual_multicast_url, Media::kVisualMulticastUrl);
						visual_guid = my_visual_guid;
						if(visual_guid.empty() || my_visual_input_url.empty() || my_visual_multicast_url.empty()) {
							eap_information("visual task param is empty!");
							break;
						}
					}
					catch (const std::exception& e) {
						eap_error_printf("get config kVisualGuid throw exception: %s", e.what());
					}
					int64_t duration{};
					try {
						DispatchCenter::Instance()->addDefaultVisualTasks(visual_guid);
						break;
					}
					catch (const std::exception& e) {
						eap_information(std::string(e.what()));
						NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice(visual_guid, ApiErr::Exception, std::string(e.what()), std::to_string(0), std::to_string(0)));
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					}
					NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice(visual_guid, ApiErr::Success, std::string("success"), std::to_string(duration), std::to_string(0)));
				}

				std::string enable_visual_and_infrared{};
				try {
					GET_CONFIG(std::string, getString, my_enable_visual_and_infrared, Media::kEnableVisualAndInfrared);
					enable_visual_and_infrared = my_enable_visual_and_infrared;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kEnableVisualAndInfrared throw exception: %s", e.what());
				}
				if ("2" == enable_visual_and_infrared) {
					while (s_open) {
						std::string infrared_guid{};
						try {
							GET_CONFIG(std::string, getString, my_infrared_guid, Media::kInfraredGuid);
							infrared_guid = my_infrared_guid;
						}
						catch (const std::exception& e) {
							eap_error_printf("get config kInfraredGuid throw exception: %s", e.what());
						}
						int64_t duration{};
						try {
							DispatchCenter::Instance()->addDefaultInfraredTasks(infrared_guid);
							break;
						}
						catch (const std::exception& e) {
							eap_information(std::string(e.what()));
							NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice(infrared_guid, ApiErr::Exception, std::string(e.what()), std::to_string(0), std::to_string(0)));
							std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						}
						NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice(infrared_guid, ApiErr::Success, std::string("success"), std::to_string(duration), std::to_string(0)));
					}
				}
			});
#endif //  ENABLE_AIRBORNE

			s_rpc_server->register_handler("smaReceivePilotData", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr) 
			{
				Poco::JSON::Object params{};
				auto json_ptr = stringToJson(paramsStr);
				if(json_ptr)
					params = *(json_ptr);
				Poco::JSON::Object val{};

				auto id_str = params.has("id")? params.get("id").toString(): "";
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("receivePilotData: args id is empty");
					return;
				}			

				try {
					DispatchCenter::Instance()->receivePilotData(id_str, paramsStr);
				} catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return;
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return;
			});

			s_rpc_server->register_handler("smaReceivePayloadData", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params{};
				auto json_ptr = stringToJson(paramsStr);
				if (json_ptr)
					params = *(json_ptr);
				Poco::JSON::Object val{};

				auto id_str = params.has("id") ? params.get("id").toString() : "";
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("updateFureceivePayloadDatancMask: args id is empty");
					return;				
				}

				try {
					DispatchCenter::Instance()->receivePayloadData(id_str, paramsStr);
				} catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return;
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return;
			});

			//添加原来pilotControl的视频任务
			s_rpc_server->register_handler("smaStartPilotProcess", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr) {
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));

				bool auth_result{};
				std::string auth_desc{};
				std::function<void(bool result, std::string desc)> invoker = [&auth_result, &auth_desc](bool result, std::string desc) {
					auth_result = result;
					auth_desc = desc;
				};
				NoticeCenter::Instance()->getCenter().postNotification(new AddTaskNotice(paramsStr, invoker));
				auto pilotId = params.has("pilotId") ? params.get("pilotId").toString() : "";
				if (pilotId == "") {
					pilotId = eap::configInstance().getString(Vehicle::KPilotId);
				} else {
					eap::configInstance().setString(Vehicle::KPilotId, pilotId);
				}
				val.set("id", pilotId);
				if (!auth_result) {
					makeJsonDefault(ApiErr::AuthFailed, auth_desc, val, pilotId);
					return jsonToString(val);
				}

				auto func_name = params.has("func_name") ? params.get("func_name").toString(): "pushStreamUrl";

				std::string pull_url_str{};
				std::string hd_app{};
				std::string hd_stream{};
				std::string http_port{};
				try {
					GET_CONFIG(std::string, getString, my_pull_url_str, Vehicle::KHdUrlSrc);
					GET_CONFIG(std::string, getString, my_hd_app, Vehicle::KHdApp);
					GET_CONFIG(std::string, getString, my_hd_stream, Vehicle::KHdStream);
					GET_CONFIG(std::string, getString, my_http_port, Vehicle::KHttpPort);
					pull_url_str = my_pull_url_str;
					hd_app = my_hd_app;
					hd_stream = my_hd_stream;
					http_port = my_http_port;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config throw exception: %s", e.what());
				}
				std::string push_url_str = "webrtc://127.0.0.1:"+ http_port + "/" + hd_app + "/" + hd_stream;
				if (func_name == "pushStreamUrl") {
					auto push_url = params.has("push_url") ? params.get("push_url").toString() : "";
					if (!push_url.empty()) {
						push_url_str = push_url;
					}
				} else if (func_name == "addSyncTask") {
					push_url_str = eap::configInstance().getString(Vehicle::KHdPushUrl);
				} else if(func_name == "distributeCfg"){
					pull_url_str = params.has("pull_url") ? params.get("pull_url").toString() : "";
					push_url_str = params.has("push_url") ? params.get("push_url").toString() : "";
					auto httpPort = params.has("httpPort") ? params.get("httpPort").toString() : "";
					auto hdApp = params.has("hdApp") ? params.get("hdApp").toString() : "";
					auto hdStream = params.has("hdStream") ? params.get("hdStream").toString() : "";
					eap::configInstance().setString(Vehicle::KHdApp, hdApp);
					eap::configInstance().setString(Vehicle::KHdStream, hdStream);
					eap::configInstance().setString(Vehicle::KHttpPort, httpPort);
					eap::configInstance().setString(Vehicle::KHdUrlSrc, pull_url_str);
					eap::saveConfig();
				}
				
				if (pull_url_str.empty() || push_url_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args pull or push url mull", val, pilotId);
					eap_error("startProcess: args pull or push url mull");
					return jsonToString(val);
				}
				auto guid = generate_guid(16);
				eap_information_printf("add sync pilot task, in url: %s, out url: %s, guid: %s"
					, pull_url_str, push_url_str, guid);
				try {
					DispatchTask::InitParameter init_paramter_task;
					init_paramter_task.id = guid;
					init_paramter_task.ar_vector_file = "";
					init_paramter_task.ar_camera_config = "";
					init_paramter_task.func_mask = 0;
					init_paramter_task.pull_url = pull_url_str;
					init_paramter_task.push_url = push_url_str;

					std::string record_file_prefix{};
					try {
						GET_CONFIG(std::string, getString, my_record_file_prefix, Media::kRecordFilePrefix);
						record_file_prefix = my_record_file_prefix;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}
					
					init_paramter_task.record_file_prefix = record_file_prefix;
					init_paramter_task.record_time_str = get_current_time_string_second_compact();
					init_paramter_task.is_pilotcontrol_task = false;
					DispatchCenter::Instance()->addTask(init_paramter_task);
				} catch (const std::exception& e) {
					eap_information(std::string(e.what()));
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, guid);
				eap_information_printf("add sync pilot task successed! in url: %s, out url: %s, guid: %s"
					, pull_url_str, push_url_str, guid);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaStartProcess", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, 
				std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("pull_url", "push_url", "func_mask");

				bool auth_result{};
				std::string auth_desc{};
				std::function<void(bool result, std::string desc)> invoker = [&auth_result, &auth_desc](bool result, std::string desc) {
					auth_result = result;
					auth_desc = desc;
				};
				NoticeCenter::Instance()->getCenter().postNotification(new AddTaskNotice(paramsStr, invoker));

				if (!auth_result) {
					makeJsonDefault(ApiErr::AuthFailed, auth_desc, val);
					return jsonToString(val);
				}

				auto pull_url_str = params.get("pull_url").toString();
				auto push_url_str = params.get("push_url").toString();
				auto func_mask_str = params.get("func_mask").toString();

				if (pull_url_str.empty() || push_url_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args pull or push url mull", val);
					eap_error("startProcess: args pull or push url mull");
					return jsonToString(val);
				}

				std::string play_back_address{};
				bool has_play_back_address = params.has("play_back_address");
				if (has_play_back_address) {
					play_back_address = params.get("play_back_address").toString();
				}

				int func_mask{0};
				if (func_mask_str.empty()) {					
					eap_warning("startProcess: args func_mask is empty");					
				} else {
					func_mask = std::atoi(func_mask_str.c_str());
				}
				if (func_mask < 0 || func_mask > 5000) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask invalid", val);
					eap_error("startProcess: args func_mask invalid");
					return jsonToString(val);
				}

				auto guid = generate_guid(16);
				eap_information_printf("add task, in url: %s, out url: %s, guid: %s, func_mask: %s"
					, pull_url_str, push_url_str, guid, func_mask_str);
				
				try {
					std::string ar_vector_file_path{};
					std::string ar_camera_file_path{};
					if(ar_file.size() > 0 ){
						writeArCameraFile(ar_vector_file_path, ar_camera_file_path, guid, ar_file);
					}
					if ((func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR||
			 (func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
						if (ar_vector_file_path.empty() || ar_camera_file_path.empty()) {
							throw std::invalid_argument("open ar func,but no camera.config or vector file");
						}
					}
					DispatchTask::InitParameter init_paramter_task;
					init_paramter_task.id = guid;
					init_paramter_task.ar_vector_file = ar_vector_file_path;
					init_paramter_task.ar_camera_config = ar_camera_file_path;					
					init_paramter_task.func_mask = func_mask;
					init_paramter_task.pull_url = pull_url_str;
					init_paramter_task.push_url = push_url_str;
					
					std::string record_file_prefix{};
					try {
						GET_CONFIG(std::string, getString, my_record_file_prefix, Media::kRecordFilePrefix);
						record_file_prefix = my_record_file_prefix;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					init_paramter_task.record_file_prefix = record_file_prefix;
					init_paramter_task.record_time_str = get_current_time_string_second_compact();
					DispatchCenter::Instance()->addTask(init_paramter_task);
				} catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, guid);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaStartMultiProcess", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, 
				std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("pull_url", "push_url", "pull_sd_url", "push_sd_url", "func_mask");

				bool auth_result{};
				std::string auth_desc{};
				std::function<void(bool result, std::string desc)> invoker = [&auth_result, &auth_desc](bool result, std::string desc) {
					auth_result = result;
					auth_desc = desc;
				};
				NoticeCenter::Instance()->getCenter().postNotification(new AddTaskNotice(paramsStr, invoker));

				if (!auth_result) {
					makeJsonDefault(ApiErr::AuthFailed, auth_desc, val);
					return jsonToString(val);
				}

				auto pull_url_str = params.get("pull_url").toString();
				auto pull_sd_url_str = params.get("pull_sd_url").toString();
				auto push_url_str = params.get("push_url").toString();
				auto push_sd_url_str = params.get("push_sd_url").toString();
				auto func_mask_str = params.get("func_mask").toString();
				std::vector<std::string> pull_url;
				std::vector<std::string> push_url;
				pull_url.push_back(pull_url_str);
				pull_url.push_back(pull_sd_url_str);
				push_url.push_back(push_url_str);
				push_url.push_back(push_sd_url_str);

				if (pull_url_str.empty() || push_url_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args pull or push url mull", val);
					eap_error("startProcess: args pull or push url mull");
					return jsonToString(val);
				}

				std::string play_back_address{};
				bool has_play_back_address = params.has("play_back_address");
				if (has_play_back_address) {
					play_back_address = params.get("play_back_address").toString();
				}

				int func_mask{0};
				if (func_mask_str.empty()) {					
					eap_warning("startProcess: args func_mask is empty");					
				} else {
					func_mask = std::atoi(func_mask_str.c_str());
				}
				if (func_mask < 0 || func_mask > 5000) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask invalid", val);
					eap_error("startProcess: args func_mask invalid");
					return jsonToString(val);
				}

				auto guid = generate_guid(16);
				eap_information_printf("add multi task, in hd url: %s, in sd url: %s, out hd url: %s, out sd url: %s,guid: %s, func_mask: %s"
					, pull_url_str, pull_sd_url_str, push_url_str, push_sd_url_str, guid, func_mask_str);
				
				try {
					std::string ar_vector_file_path{};
					std::string ar_camera_file_path{};
					if(ar_file.size() > 0 ){
						writeArCameraFile(ar_vector_file_path, ar_camera_file_path, guid, ar_file);
					}
					if ((func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR||
			 (func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
						if (ar_vector_file_path.empty() || ar_camera_file_path.empty()) {
							throw std::invalid_argument("open ar func,but no camera.config or vector file");
						}
					}
						
					DispatchTask::InitParameter init_paramter_task;
					init_paramter_task.id = guid;
					init_paramter_task.ar_vector_file = ar_vector_file_path;
					init_paramter_task.ar_camera_config = ar_camera_file_path;					
					init_paramter_task.func_mask = func_mask;
					init_paramter_task._pull_urls = pull_url;
					init_paramter_task._push_urls = push_url;

					std::string record_file_prefix{};
					try {
						GET_CONFIG(std::string, getString, my_record_file_prefix, Media::kRecordFilePrefix);
						record_file_prefix = my_record_file_prefix;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					init_paramter_task.record_file_prefix = record_file_prefix;
					init_paramter_task.record_time_str = get_current_time_string_second_compact();
					DispatchCenter::Instance()->addTaskMultiple(init_paramter_task);
				} catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, guid);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaStopProcess", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id");

				std::string id = params.get("id").toString();
				eap_information_printf("remove task, id: %s", id);

				try {
					auto task = DispatchCenter::Instance()->findTaskId(id);
					if (task) {
						auto ret = DispatchCenter::Instance()->removeTask(id);
						if (ret) {
							eap_information_printf("remove task success, id: %s", id);
							NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(true, id));
						}
						else {
							eap_debug_printf("remove task failed, id: %s", id);
							makeJsonDefault(ApiErr::Exception, "Task remove failed", val);
							NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(false, id));
							return jsonToString(val);
						}
					}
					else {
						eap_debug_printf("remove task failed, id: %s", id);
						makeJsonDefault(ApiErr::InvalidArgs, "Task not existed!", val);
						NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(false, id));
						return jsonToString(val);
					}
					
				} catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(false, id));
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaStopProcess_dji", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id");

				std::string id = params.get("id").toString();
				eap_information_printf("smaStopProcess_dji remove task, id: %s", id);

				try {
					auto task = DispatchCenter::Instance()->findTaskId(id);
					if (task) {
						auto ret = DispatchCenter::Instance()->removeTask(id);
						if (ret) {
							eap_information_printf("smaStopProcess_dji remove task success, id: %s", id);
							NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(true, id));
						}
						else {
							eap_debug_printf("smaStopProcess_dji remove task failed, id: %s", id);
							makeJsonDefault(ApiErr::Exception, "Task remove failed", val);
							NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(false, id));
							return jsonToString(val);
						}
					}
					else {
						eap_debug_printf("smaStopProcess_dji remove task failed, id: %s", id);
						makeJsonDefault(ApiErr::InvalidArgs, "Task not existed!", val);
						NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(false, id));
						return jsonToString(val);
					}
					
				} catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(false, id));
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaStopAllProcess_dji", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				eap_information("smaStopAllProcess_dji remove all tasks");

				try {
					// 收集所有任务ID
					std::vector<std::string> task_ids;
					DispatchCenter::Instance()->for_each_task([&task_ids](const DispatchTaskPtr& task) {
						if (task) {
							task_ids.push_back(task->getId());
						}
					});

					int success_count = 0;
					int fail_count = 0;
					for (const auto& id : task_ids) {
						auto ret = DispatchCenter::Instance()->removeTask(id);
						if (ret) {
							eap_information_printf("smaStopAllProcess_dji remove task success, id: %s", id);
							NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(true, id));
							success_count++;
						}
						else {
							eap_debug_printf("smaStopAllProcess_dji remove task failed, id: %s", id);
							NoticeCenter::Instance()->getCenter().postNotification(new RemoveTaskNotice(false, id));
							fail_count++;
						}
					}
					eap_information_printf("smaStopAllProcess_dji done, success: %d, fail: %d", success_count, fail_count);
				} catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaUpdateFuncMask", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr
				, std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
#ifdef ENABLE_AIRBORNE
				if (!params.has("id")){
					std::string visual_guid = eap::configInstance().has(Media::kVisualGuid) ? eap::configInstance().getString(Media::kVisualGuid) : "";
					params.set("id", visual_guid);
					eap_information_printf("start smaUpdateFuncMask, id: %s", visual_guid);
				}
#else
				CHECK_SMA_ARGS("id", "func_mask");
#endif

				auto id_str = params.get("id").toString();
				auto func_mask_str = params.has("func_mask")? params.get("func_mask").toString(): 0;
				auto time_count_str = params.has("time_count")? params.get("time_count").toString(): "0";
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("updateFuncMask: args id is empty");
					return jsonToString(val);
				}

				if (func_mask_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask is empty", val);
					eap_error("updateFuncMask: args func_mask is empty");
					return jsonToString(val);
				}
				
				int func_mask = std::atoi(func_mask_str.c_str());
				if (func_mask < 0 || func_mask > 5000) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask_str invalid", val);
					eap_error_printf("updateFuncMask: args func_mask_str invalid，funcmask: %d", func_mask);
					return jsonToString(val);
				}
				int time_count = std::atoi(time_count_str.c_str());
				eap_information_printf("update func_mask, id: %s, funcmask: %d, time_count: %d", id_str, func_mask, time_count);
				try {
					std::string ar_vector_file_path{};
					std::string ar_camera_file_path{};
					std::string ar_kml_name = ar_file_path + id_str + "/" + id_str + ".kml";
					std::string ar_kmz_name = ar_file_path + id_str + "/" + id_str + ".kmz";
					std::string ar_camera_name = ar_file_path + id_str + "/" + id_str + ".config";
					if(ar_file.size() > 0 ){
						writeArCameraFile(ar_vector_file_path, ar_camera_file_path, id_str, ar_file);
					}
					
					if (ar_vector_file_path.empty() && fileExist(ar_kml_name)) {
						ar_vector_file_path = ar_kml_name;
					} 
					if (ar_camera_file_path.empty() && fileExist(ar_camera_name)) {
						ar_camera_file_path = ar_camera_name;
					} 
					if (ar_vector_file_path.empty() && fileExist(ar_kmz_name)) {
						ar_vector_file_path = ar_kmz_name;
					}
					if ((func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR||
			 (func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
						if (ar_vector_file_path.empty() || ar_camera_file_path.empty()) {
							throw std::invalid_argument("open ar func,but no camera.config or vector file");
						}
					}
					DispatchCenter::Instance()->updateFuncMask(id_str, func_mask, ar_camera_file_path, ar_vector_file_path, time_count);					
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaUpdateAllFuncMask", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr
				, std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("func_mask");

				auto func_mask_str = params.has("func_mask")? params.get("func_mask").toString(): "0";
				auto time_count_str = params.has("time_count")? params.get("time_count").toString(): "0";

				if (func_mask_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask is empty", val);
					eap_error("updateAllFuncMask: args func_mask is empty");
					return jsonToString(val);
				}
				
				int func_mask = std::atoi(func_mask_str.c_str());
				if (func_mask < 0 || func_mask > 5000) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask_str invalid", val);
					eap_error_printf("updateAllFuncMask: args func_mask_str invalid, funcmask: %d", func_mask);
					return jsonToString(val);
				}
				int time_count = std::atoi(time_count_str.c_str());
				eap_information_printf("updateAllFuncMask, funcmask: %d, time_count: %d", func_mask, time_count);
				try {
					std::string ar_vector_file_path{};
					std::string ar_camera_file_path{};
					if(ar_file.size() > 0){
						// 对于全量更新不处理按id区分的AR文件写入
						for(auto& [key, value] : ar_file){
							if(key.find("vector") != std::string::npos || key.find("kml") != std::string::npos || key.find("kmz") != std::string::npos){
								ar_vector_file_path = value;
							} else if(key.find("camera") != std::string::npos || key.find("config") != std::string::npos){
								ar_camera_file_path = value;
							}
						}
					}
					if ((func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR||
			 (func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
						if (ar_vector_file_path.empty() || ar_camera_file_path.empty()) {
							throw std::invalid_argument("open ar func,but no camera.config or vector file");
						}
					}
					DispatchCenter::Instance()->updateAllFuncMask(func_mask, ar_camera_file_path, ar_vector_file_path, time_count);
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaStartProcess_dji", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, 
				std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("pull_url", "push_url", "func_mask");

				bool auth_result{};
				std::string auth_desc{};
				std::function<void(bool result, std::string desc)> invoker = [&auth_result, &auth_desc](bool result, std::string desc) {
					auth_result = result;
					auth_desc = desc;
				};
				NoticeCenter::Instance()->getCenter().postNotification(new AddTaskNotice(paramsStr, invoker));

				if (!auth_result) {
					makeJsonDefault(ApiErr::AuthFailed, auth_desc, val);
					return jsonToString(val);
				}

				auto pull_url_str = params.get("pull_url").toString();
				auto push_url_str = params.get("push_url").toString();
				auto func_mask_str = params.get("func_mask").toString();

				if (pull_url_str.empty() || push_url_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args pull or push url mull", val);
					eap_error("smaStartProcess_dji: args pull or push url mull");
					return jsonToString(val);
				}

				std::string play_back_address{};
				bool has_play_back_address = params.has("play_back_address");
				if (has_play_back_address) {
					play_back_address = params.get("play_back_address").toString();
				}

				int func_mask{0};
				if (func_mask_str.empty()) {					
					eap_warning("smaStartProcess_dji: args func_mask is empty");					
				} else {
					func_mask = std::atoi(func_mask_str.c_str());
				}
				if (func_mask < 0 || func_mask > 5000) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask invalid", val);
					eap_error("smaStartProcess_dji: args func_mask invalid");
					return jsonToString(val);
				}

				auto guid = generate_guid(16);
				eap_information_printf("smaStartProcess_dji add task, in url: %s, out url: %s, guid: %s, func_mask: %s"
					, pull_url_str, push_url_str, guid, func_mask_str);
				
				try {
					std::string ar_vector_file_path{};
					std::string ar_camera_file_path{};
					if(ar_file.size() > 0 ){
						writeArCameraFile(ar_vector_file_path, ar_camera_file_path, guid, ar_file);
					}
					if ((func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR||
			 (func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
						if (ar_vector_file_path.empty() || ar_camera_file_path.empty()) {
							throw std::invalid_argument("open ar func,but no camera.config or vector file");
						}
					}
					DispatchTask::InitParameter init_paramter_task;
					init_paramter_task.id = guid;
					init_paramter_task.ar_vector_file = ar_vector_file_path;
					init_paramter_task.ar_camera_config = ar_camera_file_path;					
					init_paramter_task.func_mask = func_mask;
					init_paramter_task.pull_url = pull_url_str;
					init_paramter_task.push_url = push_url_str;
					
					std::string record_file_prefix{};
					try {
						GET_CONFIG(std::string, getString, my_record_file_prefix, Media::kRecordFilePrefix);
						record_file_prefix = my_record_file_prefix;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					init_paramter_task.record_file_prefix = record_file_prefix;
					init_paramter_task.record_time_str = get_current_time_string_second_compact();
					DispatchCenter::Instance()->addTask(init_paramter_task);
				} catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, guid);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaUpdateFuncMask_dji", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr
				, std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
#ifdef ENABLE_AIRBORNE
				if (!params.has("id")){
					std::string visual_guid = eap::configInstance().has(Media::kVisualGuid) ? eap::configInstance().getString(Media::kVisualGuid) : "";
					params.set("id", visual_guid);
					eap_information_printf("start smaUpdateFuncMask_dji, id: %s", visual_guid);
				}
#else
				CHECK_SMA_ARGS("id", "func_mask");
#endif

				auto id_str = params.get("id").toString();
				auto func_mask_str = params.has("func_mask")? params.get("func_mask").toString(): 0;
				auto time_count_str = params.has("time_count")? params.get("time_count").toString(): "0";
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaUpdateFuncMask_dji: args id is empty");
					return jsonToString(val);
				}

				if (func_mask_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask is empty", val);
					eap_error("smaUpdateFuncMask_dji: args func_mask is empty");
					return jsonToString(val);
				}
				
				int func_mask = std::atoi(func_mask_str.c_str());
				if (func_mask < 0 || func_mask > 5000) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask_str invalid", val);
					eap_error_printf("smaUpdateFuncMask_dji: args func_mask_str invalid, funcmask: %d", func_mask);
					return jsonToString(val);
				}
				int time_count = std::atoi(time_count_str.c_str());
				eap_information_printf("smaUpdateFuncMask_dji, id: %s, funcmask: %d, time_count: %d", id_str, func_mask, time_count);
				try {
					std::string ar_vector_file_path{};
					std::string ar_camera_file_path{};
					std::string ar_kml_name = ar_file_path + id_str + "/" + id_str + ".kml";
					std::string ar_kmz_name = ar_file_path + id_str + "/" + id_str + ".kmz";
					std::string ar_camera_name = ar_file_path + id_str + "/" + id_str + ".config";
					if(ar_file.size() > 0 ){
						writeArCameraFile(ar_vector_file_path, ar_camera_file_path, id_str, ar_file);
					}
					
					if (ar_vector_file_path.empty() && fileExist(ar_kml_name)) {
						ar_vector_file_path = ar_kml_name;
					} 
					if (ar_camera_file_path.empty() && fileExist(ar_camera_name)) {
						ar_camera_file_path = ar_camera_name;
					} 
					if (ar_vector_file_path.empty() && fileExist(ar_kmz_name)) {
						ar_vector_file_path = ar_kmz_name;
					}
					if ((func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR||
			 (func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
						if (ar_vector_file_path.empty() || ar_camera_file_path.empty()) {
							throw std::invalid_argument("open ar func,but no camera.config or vector file");
						}
					}
					DispatchCenter::Instance()->updateFuncMask(id_str, func_mask, ar_camera_file_path, ar_vector_file_path, time_count);					
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaUpdateAllFuncMask_dji", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr
				, std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("func_mask");

				auto func_mask_str = params.has("func_mask")? params.get("func_mask").toString(): "0";
				auto time_count_str = params.has("time_count")? params.get("time_count").toString(): "0";

				if (func_mask_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask is empty", val);
					eap_error("smaUpdateAllFuncMask_dji: args func_mask is empty");
					return jsonToString(val);
				}
				
				int func_mask = std::atoi(func_mask_str.c_str());
				if (func_mask < 0 || func_mask > 5000) {
					makeJsonDefault(ApiErr::InvalidArgs, "args func_mask_str invalid", val);
					eap_error_printf("smaUpdateAllFuncMask_dji: args func_mask_str invalid, funcmask: %d", func_mask);
					return jsonToString(val);
				}
				int time_count = std::atoi(time_count_str.c_str());
				eap_information_printf("smaUpdateAllFuncMask_dji, funcmask: %d, time_count: %d", func_mask, time_count);
				try {
					std::string ar_vector_file_path{};
					std::string ar_camera_file_path{};
					if(ar_file.size() > 0){
						for(auto& [key, value] : ar_file){
							if(key.find("vector") != std::string::npos || key.find("kml") != std::string::npos || key.find("kmz") != std::string::npos){
								ar_vector_file_path = value;
							} else if(key.find("camera") != std::string::npos || key.find("config") != std::string::npos){
								ar_camera_file_path = value;
							}
						}
					}
					if ((func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR||
			 (func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
						if (ar_vector_file_path.empty() || ar_camera_file_path.empty()) {
							throw std::invalid_argument("open ar func,but no camera.config or vector file");
						}
					}
					DispatchCenter::Instance()->updateAllFuncMask(func_mask, ar_camera_file_path, ar_vector_file_path, time_count);
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaClipSnapShotParam", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr
				, std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
#ifdef ENABLE_AIRBORNE
				if (!params.has("id")){
					std::string visual_guid = eap::configInstance().has(Media::kVisualGuid) ? eap::configInstance().getString(Media::kVisualGuid) : "";
					params.set("id", visual_guid);
					eap_information_printf("start smaClipSnapShotParam, id: %s", visual_guid);
				}
#else
				CHECK_SMA_ARGS("id", "time_count");
#endif

				auto id_str = params.get("id").toString();
				auto time_count_str = params.has("time_count")? params.get("time_count").toString(): 0;

				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaClipSnapShotParam: args id is empty");
					return jsonToString(val);
				}

				if (time_count_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args time_count is empty", val);
					eap_error("smaClipSnapShotParam: args time_count is empty");
					return jsonToString(val);
				}
				
				int time_count = std::atoi(time_count_str.c_str());
				// if (time_count < 0 || time_count > 255) {
				// 	makeJsonDefault(ApiErr::InvalidArgs, "args func_mask_str invalid", val);
				// 	eap_error("smaClipSnapShotParam: args func_mask_str invalid");
				// 	return jsonToString(val);
				// }

				eap_information_printf("smaClipSnapShotParam, id: %s, time_count: %d", id_str, time_count);
				try {
					DispatchCenter::Instance()->clipSnapShotParam(id_str, time_count);
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaGetMediaList", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				Poco::JSON::Array array;
				val.set("code", 0);

				auto callback = [&](const DispatchTaskPtr& task) {
					if (task) {
						array.add(makeTaskInfoJson(task));
						val.set("data", array);
					}
				};
				DispatchCenter::Instance()->for_each_task(callback);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaGetMediaInfo", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id");

				std::string id = params.get("id").toString();

				eap_information_printf("get media info, id: %s", id);

				try {
					auto task = DispatchCenter::Instance()->findTaskId(id);
					if (task) {
						eap_information_printf("get media info, id: %s", id);
						makeTaskInfo(task, val);
					}
					else {
						eap_debug_printf("task not existed, id: %s", id);
						makeJsonDefault(ApiErr::Exception, "task not existed", val, id);
					}
				}
				catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaGetServerConfig", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				Poco::JSON::Object obj;
				addNestedKeysToJSONObject("", configInstance(), obj);

				val.set("code", 0);
				val.set("data", obj);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaGetAIRelated", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("find_engine", "find_onnx");

				std::string find_engine = params.get("find_engine").toString();
				std::string find_onnx = params.get("find_onnx").toString();
				eap_information("get ai related file");

				try {
					if (1 == std::stoi(find_engine)) {
						Poco::JSON::Array engineArr;
						Poco::File file(ai_engine_path);
						if (file.exists()) {
							Poco::DirectoryIterator it(ai_engine_path);
							Poco::DirectoryIterator end;

							while (it != end) {
								if (it->isFile()) {
									auto path = it.path().absolute().toString();
									engineArr.add(path);
								}
								++it;
							}
							val.set("engine", engineArr);
						}
					}

					if (1 == std::stoi(find_onnx)) {
						Poco::JSON::Array onnxArr;
						Poco::File file(ai_engine_path);
						if (file.exists()) {
							Poco::DirectoryIterator it(ai_onnx_path);
							Poco::DirectoryIterator end;

							while (it != end) {
								if (it->isFile()) {
									auto path = it.path().absolute().toString();
									onnxArr.add(path);
								}
								++it;
							}
							val.set("onnx", onnxArr);
						}
					}

					val.set("code", 0);
					bool engine_ret = val.has("engine");
					if (engine_ret) {
						val.set("msg", "success");
					}
					else {
						val.set("msg", "no ai related fill");
					}
				}
				catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaSetServerConfig", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				int changed = 0;
				for (auto pr = params.begin(); pr != params.end(); pr++) {
					if (pr->first == "secret") {
						continue;
					}
					if (!eap::configInstance().has(pr->first)) {
						continue;
					}
					if (eap::configInstance().getString(pr->first) == pr->second.toString()) {
						continue;
					}
					eap::configInstance().setString(pr->first, pr->second.toString());
					++changed;
				}
				if (changed > 0) {
					//NoticeCenter::Instance()->getCenter().postNotification();
					saveConfig();
				}

				makeJsonSetConfig(ApiErr::Success, changed, val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaUploadAIModelFile", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, std::string aiStreamChunk)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				std::string model_file = "";
				auto index = aiStreamChunk.rfind("/");
				if(index != std::string::npos){
					model_file = aiStreamChunk.substr(index+1, aiStreamChunk.length()-index-1);
				}
				if (model_file.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "not have file", val);
					return jsonToString(val);
				}

				bool result = createPath(ai_engine_path);
				if (!result) {
					makeJsonDefault(ApiErr::OtherFailed, "create ai engine path fail", val);
					return jsonToString(val);
				}

				auto time_string = get_current_time_string_second();
				ai_file_path_all = ai_engine_path + "/" + time_string + "_" + model_file;
				if (fileExist(ai_file_path_all)) {
					deleteFile(ai_file_path_all);
				}
				auto ret = moveAndRenameFile(aiStreamChunk, ai_file_path_all);
				if (ret) {
					CHECK_SMA_ARGS("model_width", "model_height", "class_number", "conf_thresh", "nms_thresh", "yolo_version");
					auto str = params.has("model_width") ? params.get("model_width").toString() : "";
					configInstance().setInt(AI::kModelWidth, std::atoi(str.c_str()));
					str = params.has("model_height") ? params.get("model_height").toString() : "";
					configInstance().setInt(AI::kModelHeight, std::atoi(str.c_str()));
					str = params.has("class_number") ? params.get("class_number").toString() : "";
					configInstance().setInt(AI::kClassNumber, std::atoi(str.c_str()));
					str = params.has("conf_thresh") ? params.get("conf_thresh").toString() : "";
					configInstance().setDouble(AI::kConfThresh, std::atof(str.c_str()));
					str = params.has("nms_thresh") ? params.get("nms_thresh").toString() : "";
					configInstance().setDouble(AI::kNmsThresh, std::atof(str.c_str()));
					str = params.has("yolo_version") ? params.get("yolo_version").toString() : "";
					configInstance().setString(AI::kYoloVersion, str);
					str = params.has("track_switch") ? params.get("track_switch").toString() : "";
					configInstance().setString(AI::kTrackSwitch, str);
					str = params.has("track_buff_len") ? params.get("track_buff_len").toString() : "";
					configInstance().setInt(AI::kTrackBufferLength, std::atoi(str.c_str()));

					configInstance().setString(AI::kEngineFileFullName, ai_file_path_all);
					saveConfig();
				}else{
					makeJsonDefault(ApiErr::OtherFailed, "upload ai engine path fail", val);
					return jsonToString(val);
				}

            makeJsonDefault(ApiErr::Success, "success", val);
            return jsonToString(val);
        });

        s_rpc_server->register_handler("smaUploadOpensetAIModelFile", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, std::string /*aiStreamChunk*/)
        {
            Poco::JSON::Object val;
            Poco::JSON::Object params = *(stringToJson(paramsStr));
            CHECK_SMA_SECRET();

            // 不再处理文件流，也不校验 model_file
            // 只根据参数更新 text encoder 相关配置

            try {
                CHECK_SMA_ARGS("openset_conf_thresh", "openset_nms_thresh", "OwlVersion");

                auto str = params.has("openset_conf_thresh") ? params.get("openset_conf_thresh").toString() : "";
                if (!str.empty()) {
                    configInstance().setDouble(AI::kOpensetConfThresh, std::atof(str.c_str()));
                }

                str = params.has("openset_nms_thresh") ? params.get("openset_nms_thresh").toString() : "";
                if (!str.empty()) {
                    configInstance().setDouble(AI::kOpensetNmsThresh, std::atof(str.c_str()));
                }

                str = params.has("OwlVersion") ? params.get("OwlVersion").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kOwlVersion, str);
                }

                // 下面这几个字段，直接写入完整文件路径（插件已经给的是 savePath）
                str = params.has("text_encoder_feature") ? params.get("text_encoder_feature").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kTextEncoderFeature, str);
                }

                str = params.has("text_encoder_onnx") ? params.get("text_encoder_onnx").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kTextEncoderOnnx, str);
                }

                str = params.has("vocab") ? params.get("vocab").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kVocab, str);
                }

                str = params.has("merges") ? params.get("merges").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kMerges, str);
                }

                str = params.has("added_tokens") ? params.get("added_tokens").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kAddedTokens, str);
                }

                str = params.has("special_tokens_map") ? params.get("special_tokens_map").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kSpecialTokensMap, str);
                }

                str = params.has("tokenizer_config") ? params.get("tokenizer_config").toString() : "";
                if (!str.empty()) {
                    configInstance().setString(AI::kTokenizerConfig, str);
                }

                // 不修改 AI::kOpsetEngineFileFullName，沿用已有 engine 配置
                saveConfig();

                makeJsonDefault(ApiErr::Success, "success", val);
                return jsonToString(val);
            }
            catch (const std::exception& e) {
                eap_error(e.what());
                makeJsonDefault(ApiErr::Exception, e.what(), val);
                return jsonToString(val);
            }
        });



			s_rpc_server->register_handler("smaUploadAuxAIModelFile", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, std::string aiStream)
				{
					Poco::JSON::Object val;
					Poco::JSON::Object params = *(stringToJson(paramsStr));
					CHECK_SMA_SECRET();
					CHECK_SMA_ARGS("model_width", "model_height", "class_number", "conf_thresh", "nms_thresh", "yolo_version");
					auto str = params.has("model_width") ? params.get("model_width").toString() : "";
					configInstance().setInt(AI::kModelWidth, std::atoi(str.c_str()));
					str = params.has("model_heitht") ? params.get("model_heitht").toString() : "";
					configInstance().setInt(AI::kModelHeight, std::atoi(str.c_str()));
					str = params.has("class_number") ? params.get("class_number").toString() : "";
					configInstance().setInt(AI::kClassNumber, std::atoi(str.c_str()));
					str = params.has("conf_thresh") ? params.get("conf_thresh").toString() : "";
					configInstance().setDouble(AI::kConfThresh, std::atof(str.c_str()));
					str = params.has("nms_thresh") ? params.get("nms_thresh").toString() : "";
					configInstance().setDouble(AI::kNmsThresh, std::atof(str.c_str()));
					str = params.has("yolo_version") ? params.get("yolo_version").toString() : "";
					configInstance().setString(AI::kYoloVersion, str);
					str = params.has("track_switch") ? params.get("track_switch").toString() : "";
					configInstance().setString(AI::kTrackSwitch, str);
					str = params.has("track_buff_len") ? params.get("track_buff_len").toString() : "";
					configInstance().setString(AI::kTrackBufferLength, str);

					std::string model_file = "";
					auto index = aiStream.rfind("/");
					if(index != std::string::npos){
						model_file = aiStream.substr(index+1, aiStream.length()-index-1);
					}
					if(model_file.size() == 0)
					{
						makeJsonDefault(ApiErr::InvalidArgs, "not have file", val);
						return jsonToString(val);
					}

					bool result = createPath(ai_engine_path);
					if(!result)
					{
						makeJsonDefault(ApiErr::OtherFailed, "create ai engine path fail", val);
						return jsonToString(val);
					}

					auto time_string = get_current_time_string_second();
					ai_file_path_all = ai_engine_path + "/" + time_string + "_" + model_file;
					if(fileExist(ai_file_path_all)) {
						deleteFile(ai_file_path_all);
					}
					auto ret = moveAndRenameFile(aiStream, ai_file_path_all);
					if(ret){
						configInstance().setString(AI::kEngineFileFullNameAux, ai_file_path_all);
						saveConfig();
					}else{
						makeJsonDefault(ApiErr::OtherFailed, "upload ai engine path fail", val);
						return jsonToString(val);
					}

					makeJsonDefault(ApiErr::Success, "success", val);
					return jsonToString(val);
				});

			s_rpc_server->register_handler("smaSetArLevelDistance", [](rest_rpc::rpc_service::rpc_conn conn,
				const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("level_one_distance", "level_two_distance", "id");
				auto level_one_distance_str = params.get("level_one_distance").toString();
				auto level_two_distance_str = params.get("level_two_distance").toString();
				auto id_str = params.get("id").toString();

				int level_one_distance = std::atoi(level_one_distance_str.c_str());
				int level_two_distance = std::atoi(level_two_distance_str.c_str());

				if(level_one_distance <= 0 || level_two_distance <= 0 || level_one_distance >= level_two_distance)
				{
					makeJsonDefault(ApiErr::InvalidArgs, "args level_one_distance or level_two_distance set error", val);
					eap_error("smaSetArLevelDistance: args level_one_distance or level_two_distance set error");
					return jsonToString(val);
				}

				try
				{
					DispatchCenter::Instance()->updateArLevelDistance(id_str, level_one_distance, level_two_distance);
					eap_information("success set ar level_distance");
					eap_information_printf("level_one_distance: %d", level_one_distance);
					eap_information_printf("level_two_distance: %d", level_two_distance);
				}
				catch(const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaAddAnnotationElements", [](rest_rpc::rpc_service::rpc_conn conn, 
				std::string paramsStr, std::map<std::string, std::string> ar_file)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id");

				auto id_str = params.get("id").toString();
				auto is_hd_str = params.has("is_hd")? params.get("is_hd").toString(): "1";

				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaAddAnnotationElements: args id is empty");
					return jsonToString(val);
				}

				try
				{
					//camera.config
					std::string ar_file_path_all = ar_file_path + id_str + std::string("/");
					bool result = createPath(ar_file_path_all);
					if (!result) {
						throw std::runtime_error("create ar file path fail");
					}
					
					std::string ar_camera_file_path{};
					std::ofstream os;

					std::string ar_camera_file_key = "ar_camera";
					auto ar_camera_file_iter = ar_file.find(ar_camera_file_key);
					if (ar_file.end() != ar_camera_file_iter) {
						auto ar_camera_file = ar_camera_file_iter->second;
						if (ar_camera_file.empty()) {
							throw std::invalid_argument("camera file is null");
						}
						auto off = ar_camera_file.find_first_of('.');
						std::string suffix = ar_camera_file.substr(off);
						if (suffix != std::string(".config")) {
							throw std::invalid_argument("camera file suffix is not .config");
						}
						osArFile(ar_camera_file_path, ar_file_path_all, id_str, os, suffix, ar_camera_file);
					}
					else {
						throw std::invalid_argument("not have camera file, can not init ar mark engine");
					}

					std::string mark_json_key = "video_mark_data";
					std::string mark_json{};
					auto mark_json_iter = ar_file.find(mark_json_key);
					if (ar_file.end() != mark_json_iter) {
						mark_json = mark_json_iter->second;
						if (mark_json.empty()) {
							throw std::invalid_argument("mark json is null");
						}
					}
					else {
						throw std::invalid_argument("not have mark json, can not to mark");
					}
					bool is_hd = (is_hd_str == "1")? true : false;
					DispatchCenter::Instance()->addAnnotationElements(id_str, ar_camera_file_path, mark_json, is_hd);
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}
				
				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaDeleteAnnotationElements", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id", "elements_guid");

				auto id_str = params.get("id").toString();
				auto elements_guid_str = params.get("elements_guid").toString();
				auto is_hd_str = params.has("is_hd")? params.get("is_hd").toString(): "1";

				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaDeleteAnnotationElements: args id is empty");
					return jsonToString(val);
				}

				if (elements_guid_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args elements_guid is empty", val);
					eap_error("smaDeleteAnnotationElements: args elements_guid is empty");
					return jsonToString(val);
				}

				try
				{
					bool is_hd = (is_hd_str == "1")? true : false;
					DispatchCenter::Instance()->deleteAnnotationElements(id_str, elements_guid_str, is_hd);
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});
			s_rpc_server->register_handler("smaUploadAITextEncoderFile", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, std::string textEncodeStreamChunk)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				auto index = textEncodeStreamChunk.rfind("/");
				std::string text_encode_file = "";
				if(index != std::string::npos){
					text_encode_file = textEncodeStreamChunk.substr(index+1, textEncodeStreamChunk.length()-index-1);
				}
				if (text_encode_file.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "not have file", val);
					return jsonToString(val);
				}

				bool result = createPath(ai_text_encode_path);
				if (!result) {
					makeJsonDefault(ApiErr::OtherFailed, "create ai text encoder path fail", val);
					return jsonToString(val);
				}
				createPath(ai_engine_path);

				auto time_string = get_current_time_string_second();
				ai_text_encode_path_all = ai_text_encode_path + "/" + time_string + "_" + text_encode_file;
				if (fileExist(ai_text_encode_path_all)) {
					deleteFile(ai_text_encode_path_all);
				}
				auto ret = moveAndRenameFile(textEncodeStreamChunk, ai_text_encode_path_all);
				if (ret) {
					configInstance().setString(AI::kOnnxFileFullName, ai_text_encode_path_all);
					saveConfig();
				}

				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});
			s_rpc_server->register_handler("smaUploadAIOnnxFile", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr, std::string onnxStreamChunk)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				auto index = onnxStreamChunk.rfind("/");
				std::string onnx_file = "";
				if(index != std::string::npos){
					onnx_file = onnxStreamChunk.substr(index+1, onnxStreamChunk.length()-index-1);
				}
				if (onnx_file.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "not have file", val);
					return jsonToString(val);
				}

				bool result = createPath(ai_onnx_path);
				if (!result) {
					makeJsonDefault(ApiErr::OtherFailed, "create ai onnx path fail", val);
					return jsonToString(val);
				}
				createPath(ai_engine_path);

				auto time_string = get_current_time_string_second();
				ai_onnx_path_all = ai_onnx_path + "/" + time_string + "_" + onnx_file;
				if (fileExist(ai_onnx_path_all)) {
					deleteFile(ai_onnx_path_all);
				}
				auto ret = moveAndRenameFile(onnxStreamChunk, ai_onnx_path_all);
				if (ret) {
					configInstance().setString(AI::kOnnxFileFullName, ai_onnx_path_all);
					saveConfig();
				}

				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaStartOnnxtoEngine", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("is_fp16");

				auto is_fp16 = params.has("is_fp16") ? params.get("is_fp16").toString() : "";
				auto inputNamed = params.has("inputName") ? params.get("inputName").toString() : "";
				auto shape = params.has("shape") ? params.get("shape").toString() : "";
				eap_information("start onnx to engine");

				try
				{
					DispatchCenter::Instance()->enableOnnxToEngine(std::stoi(is_fp16), inputNamed, shape);
					eap_information("end onnx to engine");
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaGetOnnxtoEnginePercent", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();

				float onnx_to_engine_percent = DispatchCenter::Instance()->getOnnxToEnginePercent();

				val.set("code", 0);
				val.set("data", onnx_to_engine_percent);

				return jsonToString(val);
			});
	
	        s_rpc_server->register_handler("smaRequestAiAssistTrack", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				// CHECK_SMA_SECRET(); 暂时机载端用，不鉴权
				CHECK_SMA_ARGS("id", "track_cmd", "track_pixelpos_x", "track_pixelpos_y");

				auto id_str = params.get("id").toString();
				auto track_cmd_str = params.get("track_cmd").toString();
				// auto track_id_str = params.get("track_id").toString();
				auto track_pixelpos_x_str = params.get("track_pixelpos_x").toString();
				auto track_pixelpos_y_str = params.get("track_pixelpos_y").toString();

				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaRequestAiAssistTrack: args id is empty");
					return jsonToString(val);
				}
				
				if (track_cmd_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args track_cmd is empty", val);
					eap_error("smaRequestAiAssistTrack: args track_cmd is empty");
					return jsonToString(val);
				}

				// if (track_id_str.empty()) {
				// 	makeJsonDefault(ApiErr::InvalidArgs, "args track_id is empty", val);
				// 	eap_error("smaRequestAiAssistTrack: args track_id is empty");
				// 	return jsonToString(val);
				// }

				if (track_pixelpos_x_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args track_pixelpos_x is empty", val);
					eap_error("smaRequestAiAssistTrack: args track_pixelpos_x is empty");
					return jsonToString(val);
				}

				if (track_pixelpos_y_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args track_pixelpos_y is empty", val);
					eap_error("smaRequestAiAssistTrack: args track_pixelpos_y is empty");
					return jsonToString(val);
				}
				
				int track_cmd = std::atoi(track_cmd_str.c_str());
				// int track_id = std::atoi(track_id_str.c_str());
				int track_pixelpos_x = std::atoi(track_pixelpos_x_str.c_str());
				int track_pixelpos_y = std::atoi(track_pixelpos_y_str.c_str());
				if (track_cmd < 1 || track_cmd > 2) {// 1.基于像素坐标进入跟踪 2.基于目标ID进入跟踪
					makeJsonDefault(ApiErr::InvalidArgs, "args track_cmd invalid", val);
					eap_error("smaRequestAiAssistTrack: args track_cmd invalid");
					return jsonToString(val);
				}
				eap_information_printf("ai assist track, track_cmd: %s", track_cmd_str);

				std::string track_results{};
				try				
				{
					track_results = DispatchCenter::Instance()->aiAssistTrack(id_str, track_cmd, track_pixelpos_x, track_pixelpos_y);	
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				// makeJsonDefault(ApiErr::Success, "success", val, id_str);
				// return jsonToString(val);

				return track_results; // 目前只是机载端，插件中rpc异步调用，所以这里ai 辅助跟踪可以写成同步形式
			});

			s_rpc_server->register_handler("smaSaveSnapShot", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id");

				auto id_str = params.get("id").toString();
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaSaveSnapShot: args id is empty");
					return jsonToString(val);
				}

				auto path_str = params.has("path") ? params.get("path").toString() : "";
				if(!path_str.empty()){
					configInstance().setString(Media::kSnapShotPath, path_str.c_str());
				}
				auto numbers_str = params.has("numbers") ? params.get("numbers").toString() : "";
				if(!numbers_str.empty()){
					configInstance().setInt(Media::kSnapShotNumbers, std::atoi(numbers_str.c_str()));
				}

				try {
					DispatchCenter::Instance()->saveSnapShot(id_str);	
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaUpdateArTowerHeight", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id", "is_tower", "tower_height", "buffer_sync_height");
				auto id_str = params.get("id").toString();
				auto is_tower = params.getValue<bool>("is_tower");
				auto tower_height = params.getValue<double>("tower_height");
				auto buffer_sync_height = params.getValue<bool>("buffer_sync_height");
				
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaUpdateArTowerHeight: args id is empty");
					return jsonToString(val);
				}
				try {
					DispatchCenter::Instance()->updateArTowerHeight(id_str, is_tower, tower_height, buffer_sync_height);	
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);

			});

			//更改AI辅助吸附位置，默认为中心
			s_rpc_server->register_handler("smaUpdateAIPosCor", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id", "ai_pos_cor");
				auto id_str = params.get("id").toString();
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaUpdateAIPosCor: args id is empty");
					return jsonToString(val);
				}
				int ai_pos_cor = params.has("ai_pos_cor")? params.getValue<int>("ai_pos_cor"): 0;
				if (ai_pos_cor != 0 && ai_pos_cor != 1 && ai_pos_cor != 2) {
					makeJsonDefault(ApiErr::InvalidArgs, "args ai_pos_cor invalid", val);
					return jsonToString(val);
				}
				try {
					DispatchCenter::Instance()->updateAiPosCor(id_str, ai_pos_cor);	
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaSetSeekPercent", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id", "percent");
				auto id_str = params.get("id").toString();
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("args id is empty");
					return jsonToString(val);
				}
				auto percent = params.has("percent")? params.getValue<float>("percent"): 0.0;
				try {
					DispatchCenter::Instance()->setSeekPercent(id_str, percent);	
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaSetVideoPause", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id", "pause");
				auto id_str = params.get("id").toString();
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("args id is empty");
					return jsonToString(val);
				}
				auto pause = params.has("pause")? params.getValue<int>("pause"): 0;
				try {
					DispatchCenter::Instance()->pause(id_str, pause);	
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaPlaybackMarkRecord", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id", "playback_address", "video_out_url");
				auto task_id = params.get("id").toString();
				if (task_id.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("args id is empty");
					return jsonToString(val);
				}
				auto directory = params.has("directory")? params.getValue<std::string>("directory"): "";
				auto video_out_url = params.has("video_out_url")? params.getValue<std::string>("video_out_url"): "";
				auto playback_address = params.has("playback_address")? params.getValue<std::string>("playback_address"): "";

				if (playback_address.empty() || video_out_url.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args playback_address or video_out_url invalid", val);
					return jsonToString(val);
				}

				eap_information_printf("video mark record start, playback_adress = %s, video_out_url = %s", playback_address
					, video_out_url);
				try {
					PlaybackAnnotation::InitParam init_param;
					init_param.task_id = task_id;
					init_param.metadata_file_directory = directory;
					init_param.playback_address = playback_address;
					init_param.video_out_url = video_out_url;
					DispatchCenter::Instance()->addTaskRecord(init_param);	
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, task_id);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, task_id);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaGetHeatmapTotal", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				Poco::JSON::Object val;

				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id");

				auto id_str = params.get("id").toString();

				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaGetHeatmapTotal: args id is empty");
					return jsonToString(val);
				}
				try
				{
					auto heatmap_data = DispatchCenter::Instance()->getHeatmapTotal(id_str);	
					Poco::JSON::Array data_array;
					for (const auto& entry : heatmap_data) {
						Poco::JSON::Object entry_obj;
						const auto& coordinates = std::get<0>(entry);

						Poco::JSON::Object center_point;
						center_point.set("lat", coordinates[0]);
						center_point.set("lon", coordinates[1]);
			
						entry_obj.set("center", center_point);
			
						const auto& class_counts = std::get<1>(entry);
						Poco::JSON::Object class_counts_obj;
						for (size_t i = 0; i < class_counts.size(); ++i) {
							class_counts_obj.set("class_" + std::to_string(i), class_counts[i]);
						}

						entry_obj.set("class_counts", class_counts_obj);
						data_array.add(entry_obj);
					}
			
					val.set("heatmap_data", data_array);
					makeJsonDefault(ApiErr::Success, "success", val, id_str);
				}
				catch (const std::exception& e)
				{
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
				}
			
				return jsonToString(val);
			});	

			s_rpc_server->register_handler("smaSnapshot", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				
#ifdef ENABLE_AIRBORNE
				std::string visual_guid = eap::configInstance().has(Media::kVisualGuid) ? eap::configInstance().getString(Media::kVisualGuid) : "";
				params.set("id", visual_guid);
				eap_information_printf("start smaSnapshot, id: %s", visual_guid);
#else
				CHECK_SMA_ARGS("id");
#endif

				auto id_str = params.get("id").toString();
				auto record_no = params.has("record_no") ? params.get("record_no").toString() : "";
				auto interval = params.has("snap_interval") ? params.getValue<uint32_t>("snap_interval"): 0;
				auto total_time = params.has("snap_total_time") ? params.getValue<uint32_t>("snap_total_time"): 0;

				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaSnapshot: args id is empty");
					return jsonToString(val);
				}

				try {
					DispatchCenter::Instance()->snapshot(id_str, record_no, interval, total_time);
				}
				catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("smaVideoClipRecord", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Object params = *(stringToJson(paramsStr));
				CHECK_SMA_SECRET();
				CHECK_SMA_ARGS("id", "record_duration");

				auto id_str = params.get("id").toString();
				auto record_duration = std::atoi(params.get("record_duration").toString().c_str());
				auto record_no = params.has("record_no") ? params.get("record_no").toString() : "";
				if (id_str.empty()) {
					makeJsonDefault(ApiErr::InvalidArgs, "args id is empty", val);
					eap_error("smaVideoClipRecord: args id is empty");
					return jsonToString(val);
				}
				try {
					DispatchCenter::Instance()->videoClipRecord(id_str, record_duration, record_no);
				} catch (const std::exception& e) {
					eap_error(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val, id_str);
					return jsonToString(val);
				}

				makeJsonDefault(ApiErr::Success, "success", val, id_str);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("updatePullUrl", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				Poco::JSON::Parser parser;
				Poco::Dynamic::Var result = parser.parse(paramsStr);
				Poco::JSON::Object params = *(result.extract<Poco::JSON::Object::Ptr>());
				CHECK_SMA_SECRET();

				auto pull_url_str = params.has("pull_url")? params.get("pull_url").toString(): "";
				auto push_url_str = params.has("push_url") ? params.get("push_url").toString(): "";
				auto id = params.has("id") ? params.get("id").toString(): "";
				try {
					std::lock_guard<std::mutex> lock(_task_mutex);
					eap_information_printf("updatePullUrl, in url: %s, push url: %s, id: %s", pull_url_str, push_url_str, id);
					DispatchCenter::Instance()->updatePullUrl(id, pull_url_str, push_url_str);
				}
				catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, id);
				return jsonToString(val);
			});
			//森防项目，第一次火点定位出结果的时候通知云端sma，快照图片给云平台
			s_rpc_server->register_handler("smaFireSearchInfo", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				std::string id{};
				try {
					Poco::JSON::Parser parser;
					Poco::Dynamic::Var result = parser.parse(paramsStr);
					Poco::JSON::Object params = *(result.extract<Poco::JSON::Object::Ptr>());

					auto pilot_id = params.has("autopilotSn") ? params.get("autopilotSn").toString() : "";
					auto target_lat = params.has("targetLatitude") ? params.getValue<double>("targetLatitude") : 0;
					auto target_lon = params.has("targetLongitude") ? params.getValue<double>("targetLongitude") : 0;
					auto target_alt = params.has("targetAltitude") ? params.getValue<double>("targetAltitude") : 0;
					id = DispatchCenter::Instance()->fireSearchInfo(pilot_id, target_lat, target_lon, target_alt);
				}
				catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, id);
				return jsonToString(val);
			});
			//机载链路切换，是否走4、5G链路
			s_rpc_server->register_handler("smaSetAirborne45G", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr)
			{
				Poco::JSON::Object val;
				std::string id{};
				try {
					Poco::JSON::Parser parser;
					Poco::Dynamic::Var result = parser.parse(paramsStr);
					Poco::JSON::Object params = *(result.extract<Poco::JSON::Object::Ptr>());

					std::string visual_guid = eap::configInstance().has(Media::kVisualGuid) ? eap::configInstance().getString(Media::kVisualGuid) : "";
					auto id = params.has("id") ? params.get("id").toString() : visual_guid;
					auto airborne_45G = params.has("linkType") ? params.getValue<int>("linkType") :0;//0是图传，1是4、5G
					DispatchCenter::Instance()->setAirborne45G(id, airborne_45G);
				}
				catch (const std::exception& e) {
					eap_information(e.what());
					makeJsonDefault(ApiErr::Exception, e.what(), val);
					return jsonToString(val);
				}
				makeJsonDefault(ApiErr::Success, "success", val, id);
				return jsonToString(val);
			});

			s_rpc_server->register_handler("switchHangarStream", [](rest_rpc::rpc_service::rpc_conn conn, const std::string& paramsStr,
				std::map<std::string, std::string> ar_file)
				{
					Poco::JSON::Object val;
					Poco::JSON::Object params = *(stringToJson(paramsStr));
					CHECK_SMA_SECRET();
					//CHECK_SMA_ARGS("id");

					bool switchState = params.get("switchState");
					eap_information_printf("switch hangar stream, switchState: %d", (int)switchState);
					eap_information_printf("switch hangar stream, _exe_status: %d", (int)_exe_status);
					if(_exe_status){
						makeJsonDefault(ApiErr::Busy, "device is busy", val);
						return jsonToString(val);
					}

					std::lock_guard<std::mutex> lock(_defual_task_mutex);
					_exe_status = true;
					try {
						auto centerObj = DispatchCenter::Instance();
						if (switchState) {

							auto ret1 = centerObj->startDefalutTask();
							eap_information_printf("startDefalutTask result: %d", (int)ret1);		
						} else 
						{
							auto ret2 = centerObj->stopDefalutTask();
							eap_information_printf("stopDefalutTask result: %d", (int)ret2);
						}
					}
					catch (const std::exception& e) {
						eap_information_printf("----DispatchCenter stop failed, msg: %s", std::string(e.what()));
						makeJsonDefault(ApiErr::Exception, e.what(), val);
						_exe_status = false;
						return jsonToString(val);
					}
					makeJsonDefault(ApiErr::Success, "success", val);
					_exe_status = false;
					return jsonToString(val);
				});

			s_rpc_run_thread = std::shared_ptr<std::thread>(new std::thread([]() {
				s_rpc_server->run();
			}));
		}

		void unInstanceRestRpc()
		{
			s_open = false;
			NoticeCenter::Instance()->removeObservers();
			s_rpc_server.reset();
			if (s_rpc_run_thread && s_rpc_run_thread->joinable()) {
				s_rpc_run_thread->join();
			}
		}
        void initTasks()
        {
			std::string hanger_in1_url_pusher{};
			std::string hanger_in1_url_src{};
			std::string hanger_in2_url_pusher{};
			std::string hanger_in2_url_src{};
			std::string hanger_out_url_pusher{};
			std::string hanger_out_url_src{};
			std::string hd_push_url{};
			std::string sd_push_url{};
			std::string hd_url_src{};
			std::string sd_url_src{};
			try {
				GET_CONFIG(std::string, getString, my_hanger_in1_url_pusher, Vehicle::KHangerIn1UrlPusher);
				GET_CONFIG(std::string, getString, my_hanger_in1_url_src, Vehicle::KHangerIn1UrlSrc);
				GET_CONFIG(std::string, getString, my_hanger_in2_url_pusher, Vehicle::KHangerIn2UrlPusher);
				GET_CONFIG(std::string, getString, my_hanger_in2_url_src, Vehicle::KHangerIn2UrlSrc);
				GET_CONFIG(std::string, getString, my_hanger_out_url_pusher, Vehicle::KHangerOutUrlPusher);
				GET_CONFIG(std::string, getString, my_hanger_out_url_src, Vehicle::KHangerOutUrlSrc);
				GET_CONFIG(std::string, getString, my_hd_push_url, Vehicle::KHdPushUrl);
				GET_CONFIG(std::string, getString, my_sd_push_url, Vehicle::KSdPushUrl);
				GET_CONFIG(std::string, getString, my_hd_url_src, Vehicle::KHdUrlSrc);
				GET_CONFIG(std::string, getString, my_sd_url_src, Vehicle::KSdUrlSrc);
				hanger_in1_url_pusher = my_hanger_in1_url_pusher;
				hanger_in1_url_src = my_hanger_in1_url_src;
				hanger_in2_url_pusher = my_hanger_in2_url_pusher;
				hanger_in2_url_src = my_hanger_in2_url_src;
				hanger_out_url_pusher = my_hanger_out_url_pusher;
				hanger_out_url_src = my_hanger_out_url_src;
				hd_push_url = my_hd_push_url;
				sd_push_url = my_sd_push_url;
				hd_url_src = my_hd_url_src;
				sd_url_src = my_sd_url_src;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config throw exception: %s", e.what());
			}

			std::queue<std::pair<std::string, std::string>> defalut_task_queue;
			defalut_task_queue.push(std::make_pair(hanger_out_url_src, hanger_out_url_pusher));
			defalut_task_queue.push(std::make_pair(hanger_in2_url_src, hanger_in2_url_pusher));
			defalut_task_queue.push(std::make_pair(hanger_in1_url_src, hanger_in1_url_pusher));
			defalut_task_queue.push(std::make_pair(hd_url_src, hd_push_url));
			defalut_task_queue.push(std::make_pair(sd_url_src, sd_push_url));
			while (defalut_task_queue.size()) {
				auto task_info = defalut_task_queue.front();
				defalut_task_queue.pop();
				auto pull_url = task_info.first;
				auto push_url = task_info.second;
				if (!pull_url.empty() && !push_url.empty()) {
					DispatchTask::InitParameter init_paramter;
					init_paramter.id = generate_guid(16);
					try {
						init_paramter.pull_url = pull_url;
						init_paramter.push_url = push_url;
						init_paramter.is_pilotcontrol_task = true;
						ThreadPool::defaultPool().start([init_paramter]() {
							DispatchCenter::Instance()->addTask(init_paramter);
							eap_information_printf("---add config task id: %s, pull url:%s, push url:%s", init_paramter.id, init_paramter.pull_url, init_paramter.push_url);
						});
					}
					catch (const std::exception& e) {
						eap_error_printf("initTasks failed: %s ", std::string(e.what()));
					}catch (...) {
					}
				}
			}

			std::string filepath = exeDir() + "tasks";
			Poco::File dir(filepath);
			if (!dir.exists()) {
				return;
			}
			filepath += "/";
			std::list<std::string> task_paths{};
			listFilesRecursively(filepath, task_paths, "", ".txt");
			for (auto path : task_paths) {
				DispatchTask::InitParameter init_paramter;
				try {
					// 打开文件进行读取
					std::ifstream file(path);
					if (!file.is_open()) {
						eap_error_printf("Failed to open file for reading: %s", path);
						continue;
					}

					// 读取文件内容到字符串
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string jsonString = buffer.str();
					file.close();
					
					Poco::JSON::Parser parser;
					auto jsonData = parser.parse(jsonString);
					auto data_json = *(jsonData.extract<Poco::JSON::Object::Ptr>());
					
					init_paramter.id = data_json.has("id") ? data_json.getValue<std::string>("id") : "";
					init_paramter.func_mask = data_json.has("func_mask") ? data_json.getValue<int>("func_mask") : 0;
					init_paramter.ar_vector_file = data_json.has("ar_vector_file") ? data_json.getValue<std::string>("ar_vector_file") : "";
					init_paramter.ar_camera_config = data_json.has("ar_camera_config") ? data_json.getValue<std::string>("ar_camera_config") : "";
					init_paramter.is_pilotcontrol_task =  data_json.has("is_pilotcontrol_task") ? data_json.getValue<bool>("is_pilotcontrol_task") : false;
					init_paramter.record_file_prefix = data_json.has("record_file_prefix") ? data_json.getValue<std::string>("record_file_prefix") : "";
					init_paramter.record_time_str = data_json.has("record_time_str") ? data_json.getValue<std::string>("record_time_str") : "";
					auto pull_url_array = data_json.has("pull_url_array") ? data_json.getArray("pull_url_array") : Poco::JSON::Array::Ptr();
					auto push_url_array = data_json.has("push_url_array") ? data_json.getArray("push_url_array") : Poco::JSON::Array::Ptr();
					int pull_size = pull_url_array ? pull_url_array->size() : 0;
					int push_size = push_url_array ? push_url_array->size() : 0;
					if (pull_size == 1) {
						init_paramter.pull_url = pull_url_array ? pull_url_array->getElement<std::string>(0) : "";
						init_paramter.push_url = push_url_array ? push_url_array->getElement<std::string>(0) : "";
						eap_information_printf("add task, in url: %s, out url: %s, guid: %s"
							, init_paramter.pull_url, init_paramter.push_url, init_paramter.id);
						DispatchCenter::Instance()->addTask(init_paramter);
					}
					else {
						for (int i = 0; i < pull_size; i++) {
							auto pull_url = pull_url_array ? pull_url_array->getElement<std::string>(i) : "";
							auto push_url = push_url_array ? push_url_array->getElement<std::string>(i) : "";
							init_paramter._pull_urls.push_back(pull_url);
							init_paramter._push_urls.push_back(push_url);
						}
						eap_information_printf("add multi task, in hd url: %s, in sd url: %s, out hd url: %s, out sd url: %s,guid: %s, func_mask: %s"
							, init_paramter._pull_urls[0], init_paramter._pull_urls[1], init_paramter._push_urls[0]
							, init_paramter._push_urls[1], init_paramter.id);
						DispatchCenter::Instance()->addTaskMultiple(init_paramter);
					}
				}
				catch (const std::exception& e) {
					eap_error_printf("---initTasks failed: %s", std::string(e.what()));
					DispatchCenter::Instance()->deleteTaskFile(init_paramter.id);
				}catch (...) {
				}
			}
		}

    }
}