#include "StreamMediaApplicationServer.h"
#include "OnceToken.h"
#include "Config.h"
#include "Logger.h"
#include "ApiCommon.h"
#include "HttpClient.h"

#include "rest_rpc.hpp"
#include "asio.hpp"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net//HTTPRequestHandlerFactory.h"
#include "Poco/Net/PartHandler.h"
#include <Poco/Net/FilePartSource.h>
#include <Poco/Net/StringPartSource.h>
#include "Poco/StreamCopier.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include <fstream>
#include <string>
#include <iostream>

using namespace Poco;
namespace eap
{
#ifdef ENABLE_AIRBORNE
	typedef boost::system::error_code asio_error_codes;
#else
	typedef asio::error_code asio_error_codes;
#endif

	static std::string s_plugin_guid("{681DBCD6-9F8D-4482-951A-B8A73E0E3865}");
	static const std::string param_form_name = "param_json";
	static const std::string ar_vector_form_name = "ar_vector";
	static const std::string ar_camera_form_name = "ar_camera";
	static const std::string ai_model_file_form_name = "ai_model_file";
	static const std::string ai_onnx_file_form_name = "ai_onnx_file";
	static const std::string ar_annotation_elements_form_name = "ar_annotation_elements";

	// 自定义PartHandler类
	class MyPartHandler : public Poco::Net::PartHandler
	{
		std::map<std::string, std::string>& _fields;
	public:
		MyPartHandler(std::map<std::string, std::string>& fields) : _fields(fields) {}

		std::string extractFilenameFromContentDisposition(std::string name ,const std::string& contentDisposition) {
			// 找到 filename= 的位置
			size_t filenamePos = contentDisposition.find(name);
			if (filenamePos == std::string::npos) {
				return ""; // 没有找到 filename=，返回空字符串
			}

			filenamePos += name.size();

			// 检查是否以引号开头
			bool isQuoted = (contentDisposition[filenamePos] == '"');
			if (isQuoted) {
				++filenamePos; // 跳过开头的引号

				// 找到结束引号的位置
				size_t endQuotePos = contentDisposition.find('"', filenamePos);
				if (endQuotePos == std::string::npos) {
					return ""; // 没有找到结束引号，返回空字符串
				}

				// 提取引号内的内容
				std::string filename = contentDisposition.substr(filenamePos, endQuotePos - filenamePos);
				return filename;
			}
			else {
				// 如果没有引号，则找到下一个分号或字符串末尾作为结束位置
				size_t endPos = contentDisposition.find(';', filenamePos);
				if (endPos == std::string::npos) {
					endPos = contentDisposition.length();
				}

				// 提取文件名
				std::string filename = contentDisposition.substr(filenamePos, endPos - filenamePos);
				return filename;
			}
		}

		virtual void handlePart(const Poco::Net::MessageHeader& header, std::istream& stream)
		{
			// 假设我们处理的是文件
			if (header.has("Content-Disposition") && header["Content-Disposition"].find("filename=") != std::string::npos)
			{
				std::string key_name = extractFilenameFromContentDisposition("name=", header["Content-Disposition"]);
				std::string file_name = extractFilenameFromContentDisposition("filename=", header["Content-Disposition"]);
				Poco::Path current_exe = Poco::Path::self();
				auto current_path = current_exe.parent();
				std::string savePath = current_path.toString() + file_name;
				// 打开文件以写入数据
				std::ofstream os(savePath, std::ios::binary);
				Poco::StreamCopier::copyStream(stream, os);
				os.close();
				_fields[key_name] = savePath;
				eap_information_printf("key_name: %s, savePath: %s", key_name, savePath);
			}
		}
	};

	class StreamMediaApplicationServerImpl
	{
	public:
		std::shared_ptr<rest_rpc::rpc_client> _rpc_client{};
	};

	StreamMediaApplicationServer::Ptr StreamMediaApplicationServer::createInstance()
	{
		return Ptr(new StreamMediaApplicationServer());
	}

	StreamMediaApplicationServer::StreamMediaApplicationServer()
	{
		_impl = std::make_shared<StreamMediaApplicationServerImpl>();
	}

	StreamMediaApplicationServer::~StreamMediaApplicationServer()
	{
		if (_mqttclient) {
			std::string topic{};
			std::string user_id = getUserGuid();
#ifdef ENABLE_CLOUD
			topic = "/joeap/sma/" + user_id + "/#";
			_mqttclient->Unsubscribe(topic);
#else
			topic = "/joeap/sma/" + user_id + "/" + _station_sn + "/cmd";
			_mqttclient->Unsubscribe(topic);
			std::string cloud_user_id = "FBE242B9-B226-4401-A340-36ABD4D753AD";
			topic = "/joeap/sma/" + cloud_user_id + "/" + _station_sn + "/cmd";
			_mqttclient->Unsubscribe(topic);
#endif
		}
		_impl.reset();
	}

	void StreamMediaApplicationServer::start(WebApiBasic::Ptr web_api, MqttClient::Ptr mqtt, std::string ip, std::uint16_t port)
	{
		PluginBase::start(web_api, mqtt, ip, port);

		_impl->_rpc_client = std::make_shared<rest_rpc::rpc_client>();
		try
		{
			_impl->_rpc_client->enable_auto_reconnect();
			_impl->_rpc_client->enable_auto_heartbeat();
			auto result = _impl->_rpc_client->connect(ip, port);
			if (!result) {
				eap_error_printf("connect to sma micro service failed, address: %s, port: %d", ip, (int)port);
			}

			installApi();

			//TODO: 订阅多个publish出来的消息，分别调 _event_callback
			_impl->_rpc_client->subscribe("mrpAddRPRecord", [this](std::string_view data) {
				std::string res_data{};
				try
				{
					res_data = std::string(data);

				}
				catch(const std::exception& e)
				{
					eap_error_printf("push message to mission_back, exception: %s", e.what());
				}
				catch(...)
				{
					eap_error("push message to mission_back throw exception, but is not std exception");
				}

				if (_event_callabck) {
					// 需要使用插件返回的消息
					_event_callabck(res_data);
				}
			});
		}
		catch (const std::exception& e)
		{
			eap_error_printf("connect to sma micro service failed, address: %s, port: %d, reason: %s",
				ip, (int)port, std::string(e.what()));
		}
	}

	void StreamMediaApplicationServer::stop()
	{
		if(_impl && _impl->_rpc_client)
			_impl->_rpc_client.reset();
	}

	void StreamMediaApplicationServer::pushMessage(std::string message)
	{
		// 如果传进来的信息有误，导致无法准确调用接口，但是存在from_guid，可以把错误信息封装一下返回去
		// msg_id 是调用方发出来的，如果本微服务收到的msg_id是本服务自己的msg_id范围内的值，那证明这条消息是之前调用接口的返回值
		// seq 如果调用方同一个msg_id发了多次调用请求，那么seq用来回复消息的时候，回复的是哪一次
		auto encapsulation_callback_message = [this](std::string from_guid, std::string to_take_interface,
			std::string msg_id, std::string seq, std::string ret)
		{
			if (to_take_interface == "switchLink")
				return;
			eap_warning_printf("to_take_interface is: %s", to_take_interface);
			eap_warning_printf("return other server data is: %s", ret);

			Poco::JSON::Object::Ptr send_json = new Poco::JSON::Object;
			send_json->set("guid", eap::s_plugin_guid);
			send_json->set("api", to_take_interface);//如果是返 请求方的接口返回值 ，这个interface就填原来请求方填的interface
			send_json->set("msg_id", msg_id);//请求方的msg_id
			send_json->set("seq", seq);
			Poco::JSON::Object data_json;
			try {
				Poco::JSON::Parser parser{};
				Poco::Dynamic::Var result = parser.parse(ret);
				data_json = *(result.extract<Poco::JSON::Object::Ptr>());
			}catch (std::exception &exp) {
				data_json.set("msg", exp.what());
				data_json.set("code", -400);
			}
			send_json->set("data", data_json);

			Poco::JSON::Array::Ptr to_guid_array = new Poco::JSON::Array;
			to_guid_array->add(from_guid);
			send_json->set("to", to_guid_array);

			if (_event_callabck) {
				_event_callabck(StreamMediaApplicationServer::jsonToString(send_json).c_str());
			}
		};

		Poco::JSON::Parser parser{};
		Poco::Dynamic::Var result = parser.parse(message);
		Poco::JSON::Object::Ptr params = result.extract<Poco::JSON::Object::Ptr>();

		std::string from_guid{};
		std::string to_take_interface{};
		std::string msg_id{};
		std::string seq{};
		Poco::JSON::Object::Ptr data_json = new Poco::JSON::Object;

		if (params->has("guid")) {
			from_guid = params->get("guid").toString();
		}
		else {
			eap_error("no from_guid");
			return;
		}

		if (params->has("api")) {
			to_take_interface = params->get("api").toString();
		}
		else {
			std::string error_msg = "no api to take";

			data_json->set("msg", error_msg);								
			encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
				, StreamMediaApplicationServer::jsonToString(data_json).c_str());

			eap_error(error_msg);
			return;
		}

		if (params->has("msg_id")) {
			msg_id = params->get("msg_id").toString();
		}
		else {
			std::string error_msg = "no msg_id";

			data_json->set("msg", error_msg);
			encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
				, StreamMediaApplicationServer::jsonToString(data_json));

			eap_error(error_msg);
			return;
		}

		if (params->has("seq")) {
			seq = params->get("seq").toString();
		}
		else {
			std::string error_msg = "no seq";

			data_json->set("msg", error_msg);
			encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
				, StreamMediaApplicationServer::jsonToString(data_json));

			eap_error(error_msg);
			return;
		}

		if (params->has("to")) {
			Poco::JSON::Array::Ptr to_array = params->getArray("to");
			if (to_array) {
				for (size_t i = 0; i < to_array->size(); i++) {
					std::string to_guid = to_array->getElement<std::string>(i);
					if (eap::s_plugin_guid == to_guid) {
						break;
					}

					if (i == (to_array->size() - 1)) {
						std::string error_msg = "to's  have no guid equal StreamMediApplicationServer";
						data_json->set("msg", error_msg);
						encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
							, StreamMediaApplicationServer::jsonToString(data_json));
						eap_error(error_msg);
						return;
					}
				}
			}
			else {
				std::string error_msg = "to is not array";
				data_json->set("msg", error_msg);
				encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
					, StreamMediaApplicationServer::jsonToString(data_json));
				eap_error(error_msg);
				return;
			}
		}
		else {
			std::string error_msg = "no to";
			data_json->set("msg", error_msg);
			encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
				, StreamMediaApplicationServer::jsonToString(data_json));
			eap_error(error_msg);
			return;
		}

		if (params->has("data") && params->isObject("data")) {
			auto from_data_json = params->getObject("data");
			if (to_take_interface == "smaRequestAiAssistTrack") {//AI辅助跟踪
				auto ret = smaRequestAiAssistTrack(StreamMediaApplicationServer::jsonToString(from_data_json)
					, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
			}
			else if (to_take_interface == "smaSaveSnapShot") {//快照
				auto ret = smaSaveSnapShot(StreamMediaApplicationServer::jsonToString(from_data_json)
					, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
			}
			else if (to_take_interface == "smaReceivePilotData") {//机载端接收飞控相关元数据
				auto ret = smaReceivePilotData(StreamMediaApplicationServer::jsonToString(from_data_json)
					, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
			}
			else if (to_take_interface == "smaReceivePayloadData") {//机载端接收载荷相关元数据
				auto ret = smaReceivePayloadData(StreamMediaApplicationServer::jsonToString(from_data_json)
					, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
			}
			else if (to_take_interface == "smaSnapshot") {//巫山项目机载端接收其他微服务发过来的快照消息
				auto ret = smaSnapshot(StreamMediaApplicationServer::jsonToString(from_data_json)
					, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
			}
			else if(to_take_interface == "smaUpdateFuncMask"){//巫山项目机载端接收其他微服务发过来的更新funcmask消息
				std::map<std::string, std::string> files;
				auto ret = smaUpdateFuncMask(StreamMediaApplicationServer::jsonToString(from_data_json)
					,files, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
			}
			else if(to_take_interface == "smaClipSnapShotParam"){
				auto ret = smaClipSnapShotParam(StreamMediaApplicationServer::jsonToString(from_data_json)
					, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
			}
			else if (to_take_interface == "switchLink") {
#ifdef ENABLE_AIRBORNE
				auto ret = smaSetAirborne45G(StreamMediaApplicationServer::jsonToString(from_data_json)
					, encapsulation_callback_message, from_guid, to_take_interface, msg_id, seq);
#endif
			}
			else {
				std::string error_msg = "have no api to match";
				eap_error(error_msg);
				data_json->set("msg", error_msg);
				encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
					, StreamMediaApplicationServer::jsonToString(data_json));
			}
		}
		else {
			std::string error_msg = "no data or data is not json object";
			data_json->set("msg", error_msg);
			encapsulation_callback_message(from_guid, to_take_interface, msg_id, seq
				, StreamMediaApplicationServer::jsonToString(data_json));
			eap_error(error_msg);
			return;
		}		
	}

	std::string StreamMediaApplicationServer::smaReceivePilotData(std::string param_str
		, RpcClientCallback rcp_client_callback , std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call<1000*30>("smaReceivePilotData", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				// if (!err_code) {
				// 	rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				// }
				// else {
				// 	eap_error_printf("smaReceivePilotData async call failed, error description: %s", err_code.message());
				// }
			}, param_str);
		}
		else {
			_impl->_rpc_client->call("smaReceivePilotData", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaReceivePayloadData(std::string param_str
		, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		/*std::string paydata = param_str;
		JSON::Object::Ptr jsonObject = stringToJson(paydata);
		auto PayloadData_object = jsonObject && jsonObject->has("PayloadData") ? jsonObject->getObject("PayloadData") : JSON::Object::Ptr();
		double visulmul = PayloadData_object && PayloadData_object->has("VisualFOVHMul") ? PayloadData_object->getValue<double>("VisualFOVHMul") : 0;
		std::cout << "---&&&&&&-----sma plugin receive visual mul: " << visulmul << std::endl;*/

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaReceivePayloadData", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				// if (!err_code) {
				// 	rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				// }
				// else {
				// 	eap_error_printf("smaReceivePayloadData async call failed, error description: %s", err_code.message());
				// }
			}, param_str);
		}
		else {
			_impl->_rpc_client->call("smaReceivePayloadData", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaStartProcess(std::string param_str
		, std::map<std::string, std::string> ar_file, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaStartProcess", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaStartProcess async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaStartProcess", param_str, ar_file);
		}
		return "";
	}

    std::string StreamMediaApplicationServer::smaStartMultiProcess(std::string param_str, std::map<std::string, std::string> ar_file, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
    {
        if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaStartMultiProcess", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaStartMultiProcess async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaStartMultiProcess", param_str, ar_file);
		}
		return "";
    }

    std::string StreamMediaApplicationServer::smaStopProcess(std::string param_str, RpcClientCallback rcp_client_callback
		, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaStopProcess", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaStopProcess async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaStopProcess", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaUpdateFuncMask(std::string param_str
		, std::map<std::string, std::string> ar_file, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaUpdateFuncMask", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaUpdateFuncMask async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaUpdateFuncMask", param_str, ar_file);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaClipSnapShotParam(std::string param_str, RpcClientCallback rcp_client_callback
		, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaClipSnapShotParam", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaClipSnapShotParam async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaClipSnapShotParam", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaGetMediaList(std::string param_str, RpcClientCallback rcp_client_callback
		, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaGetMediaList", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaGetMediaList async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaGetMediaList", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaGetMediaInfo(std::string param_str, RpcClientCallback rcp_client_callback
		, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaGetMediaInfo", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaGetMediaInfo async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaGetMediaInfo", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaGetServerConfig(std::string param_str, RpcClientCallback rcp_client_callback
		, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaGetServerConfig", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaGetServerConfig async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaGetServerConfig", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaGetAIRelated(std::string param_str, RpcClientCallback rcp_client_callback
		, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaGetAIRelated", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaGetAIRelated async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaGetAIRelated", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaSetServerConfig(std::string param_str, RpcClientCallback rcp_client_callback
		, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaSetServerConfig", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaSetServerConfig async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaSetServerConfig", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaUploadAIModelFile(std::string param_str
		, std::string aiStream, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaUploadAIModelFile", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaUploadAIModelFile async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaUploadAIModelFile", param_str, aiStream);
		}
		return "";
	}
	    std::string StreamMediaApplicationServer::smaUploadOpensetAIModelFile(std::string param_str
        , std::string aiStream, RpcClientCallback rcp_client_callback, std::string from_guid
        , std::string to_take_interface, std::string msg_id, std::string seq)
    {
        if (!_impl->_rpc_client) {
            eap_error("rpc client is null");
            return "";
        }

        if (rcp_client_callback) {
            _impl->_rpc_client->async_call("smaUploadOpensetAIModelFile", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
            (asio_error_codes err_code, string_view data) {
                if (!err_code) {
                    rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
                }
                else {
                    eap_error_printf("smaUploadOpensetAIModelFile async call failed, error description: %s", err_code.message());
                }
            }, param_str);
        }
        else {
            return _impl->_rpc_client->call<std::string>("smaUploadOpensetAIModelFile", param_str, aiStream);
        }
        return "";
    }
	std::string StreamMediaApplicationServer::smaAddAnnotationElements(std::string param_str
		, std::map<std::string, std::string> ar_file, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaAddAnnotationElements", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaAddAnnotationElements async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaAddAnnotationElements", param_str, ar_file);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaDeleteAnnotationElements(std::string param_str
		, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaDeleteAnnotationElements", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaDeleteAnnotationElements async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaDeleteAnnotationElements", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaUploadAIOnnxFile(std::string param_str
		, std::string onnxStream, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaUploadAIOnnxFile", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaUploadAIOnnxFile async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaUploadAIOnnxFile", param_str, onnxStream);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaUploadAITextEncoderFile(std::string param_str, std::string testEncoderStream, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaUploadAITextEncoderFile", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaUploadAITextEncoderFile async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaUploadAITextEncoderFile", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaStartOnnxtoEngine(std::string param_str
		, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaStartOnnxtoEngine", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaStartOnnxtoEngine async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaStartOnnxtoEngine", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaGetOnnxtoEnginePercent(std::string param_str
		, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaGetOnnxtoEnginePercent", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaGetOnnxtoEnginePercent async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaGetOnnxtoEnginePercent", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaRequestAiAssistTrack(std::string param_str
		, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			eap_warning("async_call smaRequestAiAssistTrack~~~~~~~~~~~~~~~~~");
			_impl->_rpc_client->async_call("smaRequestAiAssistTrack", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				eap_warning("async_call smaRequestAiAssistTrack return~~~~~~~~~~");
				if (!err_code) {
					// msg_id = "301";
					std::string msg_id_temp = "301";
					std::string to_take_interface_temp = "ai_assist_track";
					rcp_client_callback(from_guid, to_take_interface_temp, msg_id_temp, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaRequestAiAssistTrack async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaRequestAiAssistTrack", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaSaveSnapShot(std::string param_str
		, RpcClientCallback rcp_client_callback, std::string from_guid
		, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaSaveSnapShot", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaSaveSnapShot async call failed, error description: %s", err_code.message());
				}				
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaSaveSnapShot", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaStartPilotProcess(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call<1000*60*60*30>("smaStartPilotProcess", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					//rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaStartPilotProcess async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaStartPilotProcess", param_str);
		}

		std::string pilotId = "", ret_str = "";
		try {
			Poco::JSON::Parser parser;
			Poco::Dynamic::Var result = parser.parse(param_str);
			Poco::JSON::Object params = *(result.extract<Poco::JSON::Object::Ptr>());
			pilotId = params.has("pilotId") ? params.getValue<std::string>("pilotId") : "";

			Poco::JSON::Object val;
			val.set("code", 200);
			val.set("type", "response");
			val.set("id", pilotId);
			std::stringstream ss;
			val.stringify(ss);
			ret_str = ss.str();
		}
		catch (std::exception& error) {
			eap_error(error.what());
		}

		return ret_str;
	}

	std::string StreamMediaApplicationServer::smaUploadAuxAIModelFile(std::string param_str, std::string aiStream, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaUploadAuxAIModelFile", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaUploadAuxAIModelFile async call failed, error description: %s", err_code.message());
				}
			}, param_str, aiStream);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaUploadAuxAIModelFile", param_str, aiStream);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaSetArLevelDistance(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaSetArLevelDistance", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaSetArLevelDistance async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaSetArLevelDistance", param_str);
		}
		return "";
	}

    std::string StreamMediaApplicationServer::smaUpdateArTowerHeight(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
    {
        if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaUpdateArTowerHeight", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaUpdateArTowerHeight async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaUpdateArTowerHeight", param_str);
		}
		return "";
    }

    std::string StreamMediaApplicationServer::smaUpdateAIPosCor(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
    {
        if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaUpdateAIPosCor", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaUpdateAIPosCor async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaUpdateAIPosCor", param_str);
		}
		return "";
    }

    std::string StreamMediaApplicationServer::smaSetSeekPercent(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
    {
        if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaSetSeekPercent", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaSetSeekPercent async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaSetSeekPercent", param_str);
		}
		return "";
    }

    std::string StreamMediaApplicationServer::smaSetVideoPause(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
    {
        if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaSetVideoPause", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaSetVideoPause async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaSetVideoPause", param_str);
		}
		return "";
    }

    std::string StreamMediaApplicationServer::smaPlaybackMarkRecord(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
    {
        if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaPlaybackMarkRecord", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaPlaybackMarkRecord async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaPlaybackMarkRecord", param_str);
		}
		return "";
    }

	std::string StreamMediaApplicationServer::smaGetHeatmapTotal(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaGetHeatmapTotal", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaGetHeatmapTotal async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaGetHeatmapTotal", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaSnapshot(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaSnapshot", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaSnapshot async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaSnapshot", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaVideoClipRecord(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			eap_error("rpc client is null");
			return "";
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaVideoClipRecord", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaVideoClipRecord async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaVideoClipRecord", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::updatePullUrl(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			std::logic_error err("rpc client is null");
			throw std::exception(err);
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("updatePullUrl", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("updatePullUrl async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("updatePullUrl", param_str);
		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaFireSearchInfo(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			std::logic_error err("rpc client is null");
			throw std::exception(err);
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaFireSearchInfo", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaFireSearchInfo async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		}
		else {
			return _impl->_rpc_client->call<std::string>("smaFireSearchInfo", param_str);
		}
		return "";
	}
	
	std::string StreamMediaApplicationServer::switchHangarStream(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			std::logic_error err("rpc client is null");
			throw std::exception(err);
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("switchHangarStream", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
					if (!err_code) {
						rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
					}
					else {
						eap_error_printf("switchHangarStream async call failed, error description: %s", err_code.message());
					}
				}, param_str);
		}
		else {
			eap_information("exe _rpc_client call  switchHangarStream");

			return _impl->_rpc_client->call<std::string>("switchHangarStream", param_str);
			eap_information("exe _rpc_client call  finiesd  switchHangarStream");

		}
		return "";
	}

	std::string StreamMediaApplicationServer::smaSetAirborne45G(std::string param_str, RpcClientCallback rcp_client_callback, std::string from_guid, std::string to_take_interface, std::string msg_id, std::string seq)
	{
		if (!_impl->_rpc_client) {
			std::logic_error err("rpc client is null");
			throw std::exception(err);
		}

		if (rcp_client_callback) {
			_impl->_rpc_client->async_call("smaSetAirborne45G", [this, rcp_client_callback, from_guid, to_take_interface, msg_id, seq]
			(asio_error_codes err_code, string_view data) {
				if (!err_code) {
					rcp_client_callback(from_guid, to_take_interface, msg_id, seq, rest_rpc::as<std::string>(data));
				}
				else {
					eap_error_printf("smaSetAirborne45G async call failed, error description: %s", err_code.message());
				}
			}, param_str);
		} else {
			return _impl->_rpc_client->call<std::string>("smaSetAirborne45G", param_str);
		}
		return "";
	}

    void StreamMediaApplicationServer::installApi()
	{
		installWebApi();
		installMqttApi();
	}

    std::string StreamMediaApplicationServer::getUserGuid()
    {
		//云平台 FBE242B9-B226-4401-A340-36ABD4D753AD  星图 C5E96E6B-8087-4F06-944F-025A84F27DE4 
		std::string user_id = "FBE242B9-B226-4401-A340-36ABD4D753AD";
		#ifdef ENABLE_GROUND
			user_id = "C5E96E6B-8087-4F06-944F-025A84F27DE4";
		#endif
        return user_id;
    }

    std::string StreamMediaApplicationServer::sendMqttMsg(std::string api, std::string param_str, std::map<std::string, std::string> ar_file, std::string aiStream)
    {
		auto message = stringToJson(param_str);
		auto target = message->has("target") ? message->getValue<std::string>("target"): _station_sn;
		auto pilot_id = message->has("pilot_id") ? message->getValue<std::string>("pilot_id"): "";
		if(_station_sn != target && target != "" && target != "/" && _mqttclient){
			auto topic = "/joeap/sma/" + getUserGuid()+"/" + target + "/cmd";
			Poco::JSON::Object::Ptr send_js{ new Poco::JSON::Object() };
			send_js->set("api", api);
			send_js->set("type", "request");
			send_js->set("target", "/");
			send_js->set("from", "/");
			send_js->set("pilot_id", pilot_id);
			send_js->set("message", message);
			_mqttclient->PublishMessage(jsonToString(send_js), topic, 0, 0, 1);
			// eap_information_printf("topic: %s api: %s, message: %s", topic, api, param_str);
		}else{
			return "";
		}
		Poco::JSON::Object::Ptr reply_js{ new Poco::JSON::Object() };\
		reply_js->set("code", 0);
		reply_js->set("msg", "success");
        return jsonToString(reply_js);
    }

    void StreamMediaApplicationServer::handleMessageArFile(const Poco::JSON::Object &message, std::string &ar_vector_file, std::string &ar_camera_config)
    {
		// 有http下载就去下载，没有就不下载，rpc接口内部会进行判断
		// 这里只进行数据下载
		std::string ar_vector_url{};
		std::string ar_camera_url{};
		auto http_client = HttpClient::createInstance();
		if (message.has("ar_vector_url")) {
			ar_vector_url = message.get("ar_vector_url").toString();
			size_t file_name_pos = ar_vector_url.find_last_of("/") != std::string::npos ? ar_vector_url.find_last_of("/"): 0;
			auto file_name = ar_vector_url.substr(file_name_pos, ar_vector_url.length() - file_name_pos);
			http_client->doHttpRequest(ar_vector_url, [&ar_vector_file, &file_name](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response)
			{
				if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
					Poco::Path current_exe = Poco::Path::self();
					auto current_path = current_exe.parent();
					std::string savePath = current_path.toString() + file_name;

					std::ofstream os(savePath, std::ios::binary);
					Poco::StreamCopier::copyStream(response, os);
					ar_vector_file = savePath;
				}
				else {
					eap_warning("download ar_vector_file fail");
				}

			});
		}
		else {
			eap_warning("mqtt not have ar_vector_url");
		}

		if (message.has("ar_camera_url")) {
			ar_camera_url = message.get("ar_camera_url").toString();
			size_t file_name_pos = ar_camera_url.find_last_of("/") != std::string::npos ? ar_camera_url.find_last_of("/") : 0;
			auto file_name = ar_camera_url.substr(file_name_pos, ar_camera_url.length() - file_name_pos);
			http_client->doHttpRequest(ar_camera_url, [&ar_camera_config, &file_name](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
				if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
					Poco::Path current_exe = Poco::Path::self();
					auto current_path = current_exe.parent();
					std::string savePath = current_path.toString() + file_name;

					std::ofstream os(savePath, std::ios::binary);
					Poco::StreamCopier::copyStream(response, os);
					ar_camera_config = savePath;
				}
				else {
					eap_warning("download ar_camera_config fail");
				}

			});
		}
		else {
			eap_warning("mqtt not have ar_camera_url");
		}
    }

    void StreamMediaApplicationServer::handleMessageAnnotationFile(const Poco::JSON::Object &message, std::string &ar_annotation_elements, std::string &ar_camera_config)
    {
		// 有http下载就去下载，没有就不下载，rpc接口内部会进行判断
		// 这里只进行数据下载
		std::string ar_annotation_elements_url{};
		std::string ar_camera_url{};
		auto http_client = HttpClient::createInstance();

		if (message.has("ar_annotation_elements_url")) {
			ar_annotation_elements_url = message.get("ar_annotation_elements_url").toString();
			size_t file_name_pos = ar_annotation_elements_url.find_last_of("/") != std::string::npos ? ar_annotation_elements_url.find_last_of("/") : 0;
			auto file_name = ar_annotation_elements_url.substr(file_name_pos, ar_annotation_elements_url.length() - file_name_pos);
			http_client->doHttpRequest(ar_annotation_elements_url, [&ar_annotation_elements, &file_name](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response)
			{
				if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
					Poco::Path current_exe = Poco::Path::self();
					auto current_path = current_exe.parent();
					std::string savePath = current_path.toString() + file_name;

					std::ofstream os(savePath, std::ios::binary);
					Poco::StreamCopier::copyStream(response, os);
					ar_annotation_elements = savePath;
				}
				else {
					eap_warning("download ar_annotation_elements fail");
				}

			});
		}
		else {
			eap_warning("mqtt not have ar_vector_url");
		}

		if (message.has("ar_camera_url")) {
			ar_camera_url = message.get("ar_camera_url").toString();
			size_t file_name_pos = ar_camera_url.find_last_of("/") != std::string::npos ? ar_camera_url.find_last_of("/") : 0;
			auto file_name = ar_camera_url.substr(file_name_pos, ar_camera_url.length() - file_name_pos);
			http_client->doHttpRequest(ar_camera_url, [&ar_camera_config, &file_name](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response)
			{
				if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
					Poco::Path current_exe = Poco::Path::self();
					auto current_path = current_exe.parent();
					std::string savePath = current_path.toString() + file_name;

					std::ofstream os(savePath, std::ios::binary);
					Poco::StreamCopier::copyStream(response, os);
					ar_camera_config = savePath;
				}
				else {
					eap_warning("download ar_camera_config fail");
				}

			});
		}
		else {
			eap_warning("mqtt not have ar_camera_url");
		}
    }

    void StreamMediaApplicationServer::handleMessageAiModelFile(const Poco::JSON::Object &message, std::string &ai_model_file)
    {
		// 有http下载就去下载，没有就不下载，rpc接口内部会进行判断
		// 这里只进行数据下载
		std::string ai_model_url{};			
		auto http_client = HttpClient::createInstance();

		if (message.has("ai_model_url")) {
			ai_model_url = message.get("ai_model_url").toString();
			size_t file_name_pos = ai_model_url.find_last_of("/") != std::string::npos ? ai_model_url.find_last_of("/") : 0;
			auto file_name = ai_model_url.substr(file_name_pos, ai_model_url.length() - file_name_pos);
			http_client->doHttpRequest(ai_model_url, [&ai_model_file, &file_name](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response)
			{
				if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
					Poco::Path current_exe = Poco::Path::self();
					auto current_path = current_exe.parent();
					std::string savePath = current_path.toString() + file_name;

					std::ofstream os(savePath, std::ios::binary);
					Poco::StreamCopier::copyStream(response, os);
					ai_model_file = savePath;
				}
				else {
					eap_warning("download ai_model_file fail");
				}

			});
		}
		else {
			eap_warning("mqtt not have ai_model_url");
		}
    }

    void StreamMediaApplicationServer::handleMessageAiOnnxFile(const Poco::JSON::Object &message, std::string &ai_onnx_file)
    {
		// 有http下载就去下载，没有就不下载，rpc接口内部会进行判断
		// 这里只进行数据下载
		std::string ai_onnx_url{};
		auto http_client = HttpClient::createInstance();

		if (message.has("ai_onnx_url")) {
			ai_onnx_url = message.get("ai_onnx_url").toString();
			size_t file_name_pos = ai_onnx_url.find_last_of("/") != std::string::npos ? ai_onnx_url.find_last_of("/") : 0;
			auto file_name = ai_onnx_url.substr(file_name_pos, ai_onnx_url.length() - file_name_pos);

			http_client->doHttpRequest(ai_onnx_url, [&ai_onnx_file, &file_name](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response)
			{
				if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
					Poco::Path current_exe = Poco::Path::self();
					auto current_path = current_exe.parent();
					std::string savePath = current_path.toString() + file_name;

					std::ofstream os(savePath, std::ios::binary);
					Poco::StreamCopier::copyStream(response, os);
					ai_onnx_file = savePath;
				}
				else {
					eap_warning("download ai_onnx_file fail");
				}

			});
		}
		else {
			eap_warning("mqtt not have ai_onnx_url");
		}
    }

    std::string StreamMediaApplicationServer::handleMqttSubMsg(const Poco::JSON::Object& recv_message, const std::string sub_topic)
    {
		std::string type = recv_message.has("type") ? recv_message.get("type") : "";
		auto api = recv_message.has("api")?  recv_message.getValue<std::string>("api"): "";
		auto target = recv_message.has("target")?  recv_message.getValue<std::string>("target"): "";
		auto from = recv_message.has("from")?  recv_message.getValue<std::string>("from"): "";
		auto pilot_id = recv_message.has("pilot_id")?  recv_message.getValue<std::string>("pilot_id"): "";
		auto message = (recv_message.has("message")?  recv_message.getObject("message"): Poco::JSON::Object::Ptr());
		std::string msg = ""; //response msg
		Poco::JSON::Object::Ptr param_temp_json_ptr{ new Poco::JSON::Object() };

		int recv_index = sub_topic.find_last_of("/");
		int index1 = sub_topic.find_last_of("/", recv_index-1);
		std::string recv_station_sn = sub_topic.substr(index1+1, (recv_index-index1-1));
#ifdef ENABLE_CLOUD
		if(recv_station_sn != _station_sn && type == "response"){ //地面边应用平台给的mqtt回复
			//static std::string ground_edge_response_http = configInstance().getString("hook.ground_edge_response");
			static std::string ground_edge_response_http = configInstance().has("hook.ground_edge_response")? configInstance().getString("hook.ground_edge_response"):"";
			if(!ground_edge_response_http.empty()){
				try {
					message->set("from", recv_station_sn);
					message->set("api", api);
					message->set("pilot_id", pilot_id);
					msg = jsonToString(message);
					eap_information_printf("---ground to cloud, msg:%s", msg);

					auto http_client = HttpClient::createInstance();
					http_client->doHttpRequest(ground_edge_response_http, msg, [&msg](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
						if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
							eap_information_printf("ground edge response msg send success! msg:%s", msg);
						}
						else {
							eap_warning("ground edge response msg send failed!");
						}
					});
				} catch(const std::exception& e) {
					eap_error_printf("ground edge response msg http failed, %s ",std::string(e.what()));
				}
			}
			*param_temp_json_ptr = recv_message;
			param_temp_json_ptr->set("from", recv_station_sn);
			return jsonToString(param_temp_json_ptr);
		}
#endif
		
		if (recv_station_sn == _station_sn && type == "request") {
			//判断是否是云上边应用发给地面边应用平台的消息
			message->set("target", target);
			message->set("pilot_id", pilot_id);
			msg = sendMqttMsg(api, jsonToString(message));
			if(!msg.empty())
				return msg;
			std::string ar_vector_file{};
			std::string ar_camera_config{};
			std::map<std::string, std::string> ar_file{};
			JSON::Object::Ptr dataJs = message->has("data") ? message->getObject("data") : JSON::Object::Ptr();
			if(api == "smaStartProcess"){
				handleMessageArFile(*dataJs, ar_vector_file, ar_camera_config);
				if (!ar_vector_file.empty()) {
					ar_file[ar_vector_form_name] = ar_vector_file;
				}
				if (!ar_vector_file.empty()) {
					ar_file[ar_camera_form_name] = ar_camera_config;
				}

				msg = this->smaStartProcess(StreamMediaApplicationServer::jsonToString(dataJs), ar_file);
				
			}else if(api == "smaStartMultiProcess"){
				handleMessageArFile(*dataJs, ar_vector_file, ar_camera_config);
				if (!ar_vector_file.empty()) {
					ar_file[ar_vector_form_name] = ar_vector_file;
				}
				if (!ar_vector_file.empty()) {
					ar_file[ar_camera_form_name] = ar_camera_config;
				}

				msg = this->smaStartMultiProcess(StreamMediaApplicationServer::jsonToString(dataJs), ar_file);
				
			} else if(api == "smaStopProcess"){
				auto send_msg = jsonToString(dataJs);
				msg = this->smaStopProcess(send_msg);
			}else if(api == "smaUpdateFuncMask"){
				handleMessageArFile(*dataJs, ar_vector_file, ar_camera_config);
				if (!ar_vector_file.empty()) {
					ar_file[ar_vector_form_name] = ar_vector_file;
				}
				if (!ar_vector_file.empty()) {
					ar_file[ar_camera_form_name] = ar_camera_config;
				}

				msg = this->smaUpdateFuncMask(StreamMediaApplicationServer::jsonToString(dataJs), ar_file);
			}else if(api == "smaClipSnapShotParam"){
				msg = this->smaClipSnapShotParam(StreamMediaApplicationServer::jsonToString(dataJs));
			}else if(api == "smaGetMediaList"){
				msg = this->smaGetMediaList(jsonToString(dataJs));
			}else if(api == "smaGetMediaInfo"){
				msg = this->smaGetMediaInfo(jsonToString(dataJs));
			}else if(api == "smaGetServerConfig"){
				msg = this->smaGetServerConfig(jsonToString(dataJs));
			}else if(api == "smaGetAIRelated"){
				msg = this->smaGetAIRelated(jsonToString(dataJs));
			}else if(api == "smaSetServerConfig"){
				msg = this->smaSetServerConfig(jsonToString(dataJs));
			}else if(api == "smaUploadAIModelFile"){
				std::string ar_model_file{};
				handleMessageAiModelFile(*dataJs, ar_model_file);
				msg = this->smaUploadAIModelFile(StreamMediaApplicationServer::jsonToString(dataJs), ar_model_file);
			}else if(api == "smaAddAnnotationElements"){
				std::string ar_annotation_elements{};
				std::map<std::string, std::string> ar_annotation_file{};
				handleMessageAnnotationFile(*dataJs, ar_annotation_elements, ar_camera_config);
				if (!ar_annotation_elements.empty()) {
					ar_annotation_file[ar_annotation_elements_form_name] = ar_annotation_elements;
				}
				if (!ar_camera_config.empty()) {
					ar_annotation_file[ar_camera_form_name] = ar_camera_config;
				}

				msg = this->smaAddAnnotationElements(StreamMediaApplicationServer::jsonToString(dataJs), ar_annotation_file);
			}else if(api == "smaDeleteAnnotationElements"){
				msg = this->smaDeleteAnnotationElements(jsonToString(dataJs));
			}else if(api == "smaUploadAIOnnxFile"){
				std::string ar_onnx_file{};
				handleMessageAiOnnxFile(*dataJs, ar_onnx_file);
				msg = this->smaUploadAIOnnxFile(StreamMediaApplicationServer::jsonToString(dataJs), ar_onnx_file);
			}else if(api == "smaGetOnnxtoEnginePercent"){
				msg = this->smaGetOnnxtoEnginePercent(jsonToString(dataJs));
			}else if(api == "smaRequestAiAssistTrack"){
				msg = this->smaRequestAiAssistTrack(jsonToString(dataJs));
			}else if(api == "smaSaveSnapShot"){
				msg = this->smaSaveSnapShot(jsonToString(dataJs));
			}else if(api == "smaUpdateArTowerHeight"){
				msg = this->smaUpdateArTowerHeight(jsonToString(dataJs));
			}else if(api == "smaUpdateAIPosCor"){
				msg = this->smaUpdateAIPosCor(jsonToString(dataJs));
			}else if(api == "smaSetSeekPercent"){
				msg = this->smaSetSeekPercent(jsonToString(dataJs));
			}else if(api == "smaSetVideoPause"){
				msg = this->smaSetVideoPause(jsonToString(dataJs));
			}else if(api == "smaPlaybackMarkRecord"){
				msg = this->smaPlaybackMarkRecord(jsonToString(dataJs));
			}else if(api == "smaGetHeatmapTotal"){
				msg = this->smaGetHeatmapTotal(jsonToString(dataJs));
			} else if (api == "smaSnapshot") {
				msg = this->smaSnapshot(jsonToString(dataJs));
			} else if (api == "smaVideoClipRecord") {
				msg = this->smaVideoClipRecord(jsonToString(dataJs));
			} else if(api == "pushStreamUrl"){
				std::string pilotId = message->has("id") ? message->getValue<std::string>("id") : "";
				std::string pushVideoUrl = dataJs && dataJs->has("streamUrl") ? dataJs->getValue<std::string>("streamUrl") : "";

				param_temp_json_ptr->set("push_url", pushVideoUrl);
				param_temp_json_ptr->set("func_name", "pushStreamUrl");
				param_temp_json_ptr->set("pilotId", pilotId);
				msg = this->smaStartPilotProcess(StreamMediaApplicationServer::jsonToString(param_temp_json_ptr));
			}else if(api == "distributeCfg"){
				std::string pilotId = message->has("id") ? message->getValue<std::string>("id") : "";
				std::string hdVideoUrl = dataJs && dataJs->has("hdVideoUrl") ? dataJs->getValue<std::string>("hdVideoUrl") : "";
				int httpPort = dataJs && dataJs->has("httpPort") ? dataJs->getValue<int>("httpPort") : 8090;
				std::string hdApp = dataJs && dataJs->has("hdApp") ? dataJs->getValue<std::string>("hdApp") : "";
				std::string hdStream = dataJs && dataJs->has("hdStream") ? dataJs->getValue<std::string>("hdStream") : "";
				std::string pushVideoUrl = "webrtc://127.0.0.1:" + std::to_string(httpPort) + "/" + hdApp + "/" + hdStream;

				param_temp_json_ptr->set("push_url", pushVideoUrl);
				param_temp_json_ptr->set("pull_url", hdVideoUrl);
				param_temp_json_ptr->set("httpPort", std::to_string(httpPort));
				param_temp_json_ptr->set("hdApp", hdApp);
				param_temp_json_ptr->set("hdStream", hdStream);
				param_temp_json_ptr->set("pilotId", pilotId);
				param_temp_json_ptr->set("func_name", "distributeCfg");
				msg = this->smaStartPilotProcess(StreamMediaApplicationServer::jsonToString(param_temp_json_ptr));
			} else if (api == "updatePullUrl") {
				msg = this->updatePullUrl(jsonToString(dataJs));
			}
			else if (api == "fireSearchInfo") {
				//森防项目，第一次火点定位出结果的时候通知云端sma，快照图片给云平台
				msg = this->smaFireSearchInfo(jsonToString(dataJs));
			}
			else if ("switchHangarStream") {
			eap_information("exe mqtt switchHangarStream");
				msg = this->switchHangarStream(jsonToString(dataJs));
				eap_information("exe finshed mqtt switchHangarStream");

			}

			Poco::JSON::Object::Ptr response_msg = new Poco::JSON::Object;
			if(!msg.empty()){
				response_msg->set("api", api);
				response_msg->set("type", "response");
				response_msg->set("target", "/");
				response_msg->set("from", _station_sn);
				response_msg->set("pilot_id", pilot_id);
				message->set("code", 200);
				message->set("type", "response");
				auto res_js = stringToJson(msg);
				message->set("data", res_js);
				response_msg->set("message", message);
			}
			return jsonToString(response_msg);
		}
		return msg;
    }

	void StreamMediaApplicationServer::installWebApi()
	{
		// 只有通用参数时，body-json；需要传递文件时，参数和文件用body-formdata

		auto handleFile = [](const std::string& key_name, const std::string& content, std::map<std::string, std::string>& file_map, const std::string& format){
			Poco::Path current_exe = Poco::Path::self();
			auto current_path = current_exe.parent();

			// 获取当前时间戳
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
			std::stringstream ss;
            ss << milliseconds;
            std::string timestamp = ss.str();
			std::string savePath = current_path.toString() + timestamp + format;
			// 打开文件以写入数据
			std::ofstream os(savePath, std::ios::binary);
			os << content;
			os.close();

			file_map[key_name] = savePath;
			eap_information_printf("key_name: %s, savePath: %s", key_name, savePath);
		};
		auto handleRequestFormDataAR = [this, handleFile](Poco::Net::HTTPServerRequest& request, std::string& param_str, 
			std::map<std::string, std::string>& ar_file_str, std::string& msg)
		{
			if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST) {
				MyPartHandler handler(ar_file_str);
				Poco::Net::HTMLForm form(request, request.stream(), handler);
				if (!form.has(param_form_name) && !form.has(ar_vector_form_name) && !form.has(ar_camera_form_name)) {
					std::string err_info = "form-data, no any file";
					eap_error(err_info);
					msg = makeJsonDefault(-1, err_info);
				}
				else {
					if (form.has(param_form_name)) {
						param_str = form.get(param_form_name);
					}
					if(form.has(ar_vector_form_name) && !form.get(ar_vector_form_name).empty()){
						handleFile(ar_vector_form_name, form.get(ar_vector_form_name), ar_file_str, ".kml");
					}
					if(form.has(ar_camera_form_name) && !form.get(ar_camera_form_name).empty()){
						handleFile(ar_camera_form_name, form.get(ar_camera_form_name), ar_file_str, ".config");
					}
				}
			}
			else {
				std::string err_info = "use form-data, only POST method is allowed";
				eap_error(err_info);
				msg = makeJsonDefault(-1, err_info);
			}			
		};

		auto handleRequestFormDataAnnotation = [this](Poco::Net::HTTPServerRequest& request, std::string& param_str,
			std::map<std::string, std::string>& annotation_file_str, std::string& msg)
		{
			if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST) {
				MyPartHandler handler(annotation_file_str);
				Poco::Net::HTMLForm form(request, request.stream(), handler);
				if (!form.has(param_form_name) && !form.has(ar_annotation_elements_form_name) && !form.has(ar_camera_form_name)) {
					std::string err_info = "form-data, no any file";
					eap_error(err_info);
					msg = makeJsonDefault(-1, err_info);
				}
				else {
					if (form.has(param_form_name)) {
						param_str = form.get(param_form_name);
					}
				}
			}
			else {
				std::string err_info = "use form-data, only POST method is allowed";
				eap_error(err_info);
				msg = makeJsonDefault(-1, err_info);
			}
		};

		auto handleRequestBody = [this](Poco::Net::HTTPServerRequest& request, const eap::WebApiBasic::QueryParameters& query_parameters,
			std::string& param_str,  std::string& msg)
		{
			Poco::JSON::Object::Ptr param_json = new Poco::JSON::Object;
			if (query_parameters.size() > 0) {
				for (auto& itempair : query_parameters) {
					param_json->set(itempair.first, itempair.second);
				}
				param_str = jsonToString(param_json);
			}
			else {
				auto& istr = request.stream();
				std::stringstream ss;
				ss << istr.rdbuf();
				param_str = ss.str();
			}
		};

		auto handleRequestFormDataAIModel = [this](Poco::Net::HTTPServerRequest& request, std::string& param_str,
			std::string& ai_model_file_str, std::string& msg)
		{
			if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST) {
				std::map<std::string, std::string> ai_file_map;
				MyPartHandler handler(ai_file_map);
				Poco::Net::HTMLForm form(request, request.stream(), handler);
				if (!form.has(param_form_name) && !form.has(ai_model_file_form_name)) {
					std::string err_info = "form-data, no any file";
					eap_error(err_info);
					msg = makeJsonDefault(-1, err_info);
				}
				else {
					if (form.has(param_form_name)) {
						param_str = form.get(param_form_name);
					}
					if (ai_file_map.size() > 0) {
						ai_model_file_str = ai_file_map.begin()->second;
					}
				}
			}
			else {
				std::string err_info = "use form-data, only POST method is allowed";
				eap_error(err_info);
				msg = makeJsonDefault(-1, err_info);
			}
		};

		auto handleRequestFormDataAIOnnx = [this](Poco::Net::HTTPServerRequest& request, std::string& param_str,
			std::string& ai_onnx_file_str, std::string& msg)
		{
			if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST) {
				std::map<std::string, std::string> ai_file_map;
				MyPartHandler handler(ai_file_map);
				Poco::Net::HTMLForm form(request, request.stream(), handler);
				if (!form.has(param_form_name) && !form.has(ai_onnx_file_form_name)) {
					std::string err_info = "form-data, no any file";
					eap_error(err_info);
					msg = makeJsonDefault(-1, err_info);
				}
				else {
					if (form.has(param_form_name)) {
						param_str = form.get(param_form_name);
					}
					if (ai_file_map.size() > 0) {
						ai_onnx_file_str = ai_file_map.begin()->second;
					}
				}
			}
			else {
				std::string err_info = "use form-data, only POST method is allowed";
				eap_error(err_info);
				msg = makeJsonDefault(-1, err_info);
			}
		};

		_webapi->apiRegist("/index/api/sma/smaReceivePilotData", [this](API_ARGS_DEFAULT) {
			//TODO: 该接口暂时不用对应用层提供，主要是载荷微服务传递数据
		});

		_webapi->apiRegist("/index/api/sma/smaReceivePayloadData", [this](API_ARGS_DEFAULT) {
			//TODO: 该接口暂时不用对应用层提供，主要是飞行控制微服务传递数据
		});

		_webapi->apiRegist("/index/api/sma/smaStartProcess", [handleRequestFormDataAR, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::map<std::string, std::string> ar_file_str{};

			handleRequestFormDataAR(request, param_str, ar_file_str, msg);
			msg = sendMqttMsg("smaStartProcess", param_str, ar_file_str);
			if (msg.empty()) {
				msg = this->smaStartProcess(param_str, ar_file_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaStartMultiProcess", [handleRequestFormDataAR, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::map<std::string, std::string> ar_file_str{};

			handleRequestFormDataAR(request, param_str, ar_file_str, msg);
			msg = sendMqttMsg("smaStartMultiProcess", param_str, ar_file_str);
			if (msg.empty()) {
				msg = this->smaStartMultiProcess(param_str, ar_file_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaUpdateArTowerHeight", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaUpdateArTowerHeight", param_str);
			if (msg.empty()) {
				msg = this->smaUpdateArTowerHeight(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaStopProcess", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaStopProcess", param_str);
			if (msg.empty()) {
				msg = this->smaStopProcess(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaUpdateFuncMask", [handleRequestFormDataAR, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::map<std::string, std::string> ar_file_str{};

			handleRequestFormDataAR(request, param_str, ar_file_str, msg);
			msg = sendMqttMsg("smaUpdateFuncMask", param_str, ar_file_str);
			if (msg.empty()) {
				msg = this->smaUpdateFuncMask(param_str, ar_file_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaClipSnapShotParam", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaClipSnapShotParam", param_str);
			if (msg.empty()) {
				msg = this->smaClipSnapShotParam(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaGetMediaList", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaGetMediaList", param_str);
			if (msg.empty()) {
				msg = this->smaGetMediaList(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaGetMediaInfo", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaGetMediaInfo", param_str);
			if (msg.empty()) {
				msg = this->smaGetMediaInfo(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaGetServerConfig", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaGetServerConfig", param_str);
			if (msg.empty()) {
				msg = this->smaGetServerConfig(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaGetAIRelated", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaGetAIRelated", param_str);
			if (msg.empty()) {
				msg = this->smaGetAIRelated(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaSetServerConfig", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaSetServerConfig", param_str);
			if (msg.empty()) {
				msg = this->smaSetServerConfig(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaSetArLevelDistance", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaSetArLevelDistance", param_str);
			if (msg.empty()) {
				msg = this->smaSetArLevelDistance(param_str);
			}
			response.send() << msg;
		});
		
		_webapi->apiRegist("/index/api/sma/smaUploadAITextEncoderFile", [handleRequestFormDataAIOnnx, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::string ai_text_encoder_file_str{};

			handleRequestFormDataAIOnnx(request, param_str, ai_text_encoder_file_str, msg);
			msg = sendMqttMsg("smaUploadAIOnnxFile", param_str, std::map<std::string, std::string>(), ai_text_encoder_file_str);
			if (msg.empty()) {
				msg = this->smaUploadAITextEncoderFile(param_str, ai_text_encoder_file_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaUploadAIModelFile", [handleRequestFormDataAIModel, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::string ai_model_file_str{};

			handleRequestFormDataAIModel(request, param_str, ai_model_file_str, msg);
			msg = sendMqttMsg("smaUploadAIModelFile", param_str, std::map<std::string, std::string>(), ai_model_file_str);
			if (msg.empty()) {
				msg = this->smaUploadAIModelFile(param_str, ai_model_file_str);
			}
			response.send() << msg;
		});
		        _webapi->apiRegist("/index/api/sma/smaUploadOpensetAIModelFile", [handleRequestFormDataAIModel, this](API_ARGS_DEFAULT) {
            std::string msg{};
            std::string param_str{};
            std::string ai_model_file_str{};

            handleRequestFormDataAIModel(request, param_str, ai_model_file_str, msg);
            msg = sendMqttMsg("smaUploadOpensetAIModelFile", param_str, std::map<std::string, std::string>(), ai_model_file_str);
            if (msg.empty()) {
                msg = this->smaUploadOpensetAIModelFile(param_str, ai_model_file_str);
            }
            response.send() << msg;
        });
		_webapi->apiRegist("/index/api/sma/smaUploadAuxAIModelFile", [handleRequestFormDataAIModel, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::string ai_model_file_str{};

			handleRequestFormDataAIModel(request, param_str, ai_model_file_str, msg);
			msg = sendMqttMsg("smaUploadAuxAIModelFile", param_str, std::map<std::string, std::string>(), ai_model_file_str);
			if (msg.empty()) {
				msg = this->smaUploadAuxAIModelFile(param_str, ai_model_file_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaAddAnnotationElements", [handleRequestFormDataAnnotation, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::map<std::string, std::string> annotation_file_str{};

			handleRequestFormDataAnnotation(request, param_str, annotation_file_str, msg);
			msg = sendMqttMsg("smaAddAnnotationElements", param_str, annotation_file_str);
			if (msg.empty()) {
				msg = this->smaAddAnnotationElements(param_str, annotation_file_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaDeleteAnnotationElements", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaDeleteAnnotationElements", param_str);
			if (msg.empty()) {
				msg = this->smaDeleteAnnotationElements(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaUploadAIOnnxFile", [handleRequestFormDataAIOnnx, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};
			std::string ai_onnx_file_str{};

			handleRequestFormDataAIOnnx(request, param_str, ai_onnx_file_str, msg);
			msg = sendMqttMsg("smaUploadAIOnnxFile", param_str, std::map<std::string, std::string>(), ai_onnx_file_str);
			if (msg.empty()) {
				msg = this->smaUploadAIOnnxFile(param_str, ai_onnx_file_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaStartOnnxtoEngine", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaStartOnnxtoEngine", param_str);
			if (msg.empty()) {
				msg = this->smaStartOnnxtoEngine(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaGetOnnxtoEnginePercent", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaGetOnnxtoEnginePercent", param_str);
			if (msg.empty()) {
				msg = this->smaGetOnnxtoEnginePercent(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaRequestAiAssistTrack", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaRequestAiAssistTrack", param_str);
			if (msg.empty()) {
				msg = this->smaRequestAiAssistTrack(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaSaveSnapShot", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaSaveSnapShot", param_str);
			if (msg.empty()) {
				msg = this->smaSaveSnapShot(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaUpdateArTowerHeight", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaUpdateArTowerHeight", param_str);
			if (msg.empty()) {
				msg = this->smaUpdateArTowerHeight(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaUpdateAIPosCor", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaUpdateAIPosCor", param_str);
			if (msg.empty()) {
				msg = this->smaUpdateAIPosCor(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaSetSeekPercent", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaSetSeekPercent", param_str);
			if (msg.empty()) {
				msg = this->smaSetSeekPercent(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaSetVideoPause", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaSetVideoPause", param_str);
			if (msg.empty()) {
				msg = this->smaSetVideoPause(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaPlaybackMarkRecord", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaPlaybackMarkRecord", param_str);
			if (msg.empty()) {
				msg = this->smaPlaybackMarkRecord(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/pushStreamUrl", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("pushStreamUrl", param_str);
			if (msg.empty()) {
				auto message = stringToJson(param_str);
				JSON::Object::Ptr dataJs = message->has("data") ? message->getObject("data") : JSON::Object::Ptr();
				std::string pilotId = message->has("id") ? message->getValue<std::string>("id") : "";
				std::string pushVideoUrl = dataJs && dataJs->has("streamUrl") ? dataJs->getValue<std::string>("streamUrl") : "";

				Poco::JSON::Object::Ptr param_temp_json_ptr{ new Poco::JSON::Object() };
				param_temp_json_ptr->set("push_url", pushVideoUrl);
				param_temp_json_ptr->set("func_name", "pushStreamUrl");
				param_temp_json_ptr->set("pilotId", pilotId);
				msg = this->smaStartPilotProcess(StreamMediaApplicationServer::jsonToString(param_temp_json_ptr));
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/distributeCfg", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("distributeCfg", param_str);
			if (msg.empty()) {
				auto message = stringToJson(param_str);
				JSON::Object::Ptr dataJs = message->has("data") ? message->getObject("data") : JSON::Object::Ptr();
				std::string pilotId = message->has("id") ? message->getValue<std::string>("id") : "";
				std::string hdVideoUrl = dataJs && dataJs->has("hdVideoUrl") ? dataJs->getValue<std::string>("hdVideoUrl") : "";
				int httpPort = dataJs && dataJs->has("httpPort") ? dataJs->getValue<int>("httpPort") : 8090;
				std::string hdApp = dataJs && dataJs->has("hdApp") ? dataJs->getValue<std::string>("hdApp") : "";
				std::string hdStream = dataJs && dataJs->has("hdStream") ? dataJs->getValue<std::string>("hdStream") : "";
				std::string pushVideoUrl = "webrtc://127.0.0.1:" + std::to_string(httpPort) + "/" + hdApp + "/" + hdStream;
				Poco::JSON::Object::Ptr param_temp_json_ptr{ new Poco::JSON::Object() };
				param_temp_json_ptr->set("push_url", pushVideoUrl);
				param_temp_json_ptr->set("pull_url", hdVideoUrl);
				param_temp_json_ptr->set("httpPort", std::to_string(httpPort));
				param_temp_json_ptr->set("hdApp", hdApp);
				param_temp_json_ptr->set("hdStream", hdStream);
				param_temp_json_ptr->set("pilotId", pilotId);
				param_temp_json_ptr->set("func_name", "distributeCfg");
				msg = this->smaStartPilotProcess(StreamMediaApplicationServer::jsonToString(param_temp_json_ptr));
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaGetHeatmapTotal", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaGetHeatmapTotal", param_str);
			if (msg.empty()) {
				msg = this->smaGetHeatmapTotal(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaSnapshot", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaSnapshot", param_str);
			if (msg.empty()) {
				msg = this->smaSnapshot(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/smaVideoClipRecord", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaVideoClipRecord", param_str);
			if (msg.empty()) {
				msg = this->smaVideoClipRecord(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/updatePullUrl", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("updatePullUrl", param_str);
			if (msg.empty()) {
				msg = this->updatePullUrl(param_str);
			}
			response.send() << msg;
		});
		_webapi->apiRegist("/index/api/sma/smaSetAirborne45G", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("smaSetAirborne45G", param_str);
			if (msg.empty()) {
				msg = this->smaSetAirborne45G(param_str);
			}
			response.send() << msg;
		});

		_webapi->apiRegist("/index/api/sma/switchHangarStream", [handleRequestBody, this](API_ARGS_DEFAULT) {
			std::string msg{};
			std::string param_str{};

			handleRequestBody(request, query_parameters, param_str, msg);
			msg = sendMqttMsg("switchHangarStream", param_str);
			if (msg.empty()) {
				msg = this->switchHangarStream(param_str);
			}
			response.send() << msg;
			});
	}

	void StreamMediaApplicationServer::installMqttApi()
	{
		static auto qos = 0;
		if(_mqttclient)
			_station_sn = _mqttclient->getstation();
		else
			return;
		
		std::string user_id = getUserGuid();
		std::string sub_topic = "/joeap/sma/" + user_id + "/" + _station_sn + "/cmd";

		// 在原webapi中需要传文件的，mqtt的参数里面需要增加参数：文件的下载地址。
		// 在插件中利用http下载解析文件，然后传入rpc接口
		//云平台 FBE242B9-B226-4401-A340-36ABD4D753AD  星图 C5E96E6B-8087-4F06-944F-025A84F27DE4 

		//云上边应用还需处理地面等边应用传上来的回复消息，发给云平台
#ifdef ENABLE_CLOUD
		std::string sub_all_cloud_topic = "/joeap/sma/" + user_id + "/#";
		_mqttclient->Subscribe([this, user_id](const Poco::JSON::Object& recv_message, std::string topic) {
			std::string pub_topic = "/joeap/sma/" + user_id + "/"+_station_sn + "/cmd";
			auto msg = handleMqttSubMsg(recv_message, topic);
			if(!msg.empty()){
				_mqttclient->PublishMessage(msg, pub_topic, qos, 0, 1);
			}
		}, sub_all_cloud_topic, qos);
#else
		_mqttclient->Subscribe([this, user_id](const Poco::JSON::Object& recv_message, std::string topic) {
			std::string pub_topic = "/joeap/sma/" + user_id + "/"+_station_sn + "/cmd";
			auto msg = handleMqttSubMsg(recv_message, topic);
			if(!msg.empty()){
				_mqttclient->PublishMessage(msg, pub_topic, qos, 0, 1);
			}
		}, sub_topic, qos);

		std::string cloud_user_id = "FBE242B9-B226-4401-A340-36ABD4D753AD";
		sub_topic = "/joeap/sma/" + cloud_user_id + "/" + _station_sn + "/cmd";
		_mqttclient->Subscribe([this, cloud_user_id](const Poco::JSON::Object& recv_message, std::string topic) {
			std::string pub_topic = "/joeap/sma/" + cloud_user_id + "/"+_station_sn + "/cmd";
			auto msg = handleMqttSubMsg(recv_message, topic);
			if(!msg.empty()){
				_mqttclient->PublishMessage(msg, pub_topic, qos, 0, 1);
			}
		}, sub_topic, qos);
#endif
	}

	std::string StreamMediaApplicationServer::jsonToString(Poco::JSON::Object::Ptr data_json)
	{
		try {
			if (data_json) {
				std::stringstream ss;
				data_json->stringify(ss);
				return ss.str();
			}
		}
		catch (std::exception& e) {
			eap_error_printf("JsonToString exception: %s", e.what());
		}
		return std::string();
	}

	Poco::JSON::Object::Ptr StreamMediaApplicationServer::stringToJson(const std::string& input)
	{
		Poco::JSON::Parser parser;
		Poco::JSON::Object::Ptr params{};

		try {
			Dynamic::Var result = parser.parse(input);
			params = result.extract<JSON::Object::Ptr>();
		}
		catch (const std::exception& e) {
			eap_error(e.what());
		}
		return params;
	}

	static PluginBase::Ptr createStreamMediaServerInstance()
	{
		return StreamMediaApplicationServer::createInstance();
	}	
}

PLUGIN_EXPORT_API void PluginLoad()
{
	eap::registerPlugin(eap::createStreamMediaServerInstance, eap::s_plugin_guid.c_str());
}

PLUGIN_EXPORT_API void PluginUnLoad()
{
	eap::unregisterPlugin(eap::s_plugin_guid.c_str());
}

PLUGIN_EXPORT_API MSType PluginType()
{
	return MSType::Sma;
}

PLUGIN_EXPORT_API const char* PluginName()
{
	return "JoEAPPluginSma";
}

PLUGIN_EXPORT_API const char* PluginGuid()
{
	return eap::s_plugin_guid.c_str();
}
