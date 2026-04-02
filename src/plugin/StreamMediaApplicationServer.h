#ifndef STREAM_MEDIA_APPLICATION_SERVER_H
#define STREAM_MEDIA_APPLICATION_SERVER_H

#if defined(_MSC_VER) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  define DECL_EXPORT __declspec(dllexport)
#  define DECL_IMPORT __declspec(dllimport)
#else
#  define DECL_EXPORT     __attribute__((visibility("default")))
#  define DECL_IMPORT     __attribute__((visibility("default")))
#endif

#if defined(PLUGIN_EXPORT)
#  define PLUGIN_EXPORT_API DECL_EXPORT
#else
#  define PLUGIN_EXPORT_API DECL_IMPORT
#endif

#include "PluginBase.h"
#include "PluginManager.h"
#include "Poco/TaskManager.h"

#include <memory>
#include <map>

namespace eap
{
	class StreamMediaApplicationServerImpl;
	class PLUGIN_EXPORT_API StreamMediaApplicationServer : public PluginBase
	{
	public:
		using RpcClientCallback = std::function<void(std::string from_guid, std::string to_take_interface,
			std::string msg_id, std::string seq, std::string ret)>;

		using Ptr = std::shared_ptr<StreamMediaApplicationServer>;
		static Ptr createInstance();

	public:
		~StreamMediaApplicationServer();

		virtual void start(WebApiBasic::Ptr web_api, MqttClient::Ptr mqtt,
			std::string ip, std::uint16_t port) override;
		virtual void stop() override;
		
		virtual void pushMessage(std::string message) override;

		std::string smaReceivePilotData(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaReceivePayloadData(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStartProcess(std::string param_str, std::map<std::string, std::string> ar_file,
			RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStartMultiProcess(std::string param_str, std::map<std::string, std::string> ar_file
			, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStopProcess(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStopProcess_dji(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStopAllProcess_dji(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUpdateFuncMask(std::string param_str, std::map<std::string, std::string> ar_file
			, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUpdateAllFuncMask(std::string param_str, std::map<std::string, std::string> ar_file
			, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStartProcess_dji(std::string param_str, std::map<std::string, std::string> ar_file,
			RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUpdateFuncMask_dji(std::string param_str, std::map<std::string, std::string> ar_file
			, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUpdateAllFuncMask_dji(std::string param_str, std::map<std::string, std::string> ar_file
			, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaClipSnapShotParam(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaGetMediaList(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaGetMediaInfo(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaGetServerConfig(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaGetAIRelated(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaSetServerConfig(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUploadAIModelFile(std::string param_str, std::string aiStream, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUploadOpensetAIModelFile(std::string param_str, std::string aiStream, RpcClientCallback rcp_client_callback = nullptr
            , std::string from_guid = std::string(), std::string to_take_interface = std::string()
            , std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaAddAnnotationElements(std::string param_str, std::map<std::string, std::string> ar_file
			, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaDeleteAnnotationElements(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUploadAIOnnxFile(std::string param_str, std::string onnxStream, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUploadAITextEncoderFile(std::string param_str, std::string testEncoderStream, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStartOnnxtoEngine(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaGetOnnxtoEnginePercent(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaRequestAiAssistTrack(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaSaveSnapShot(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaStartPilotProcess(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUploadAuxAIModelFile(std::string param_str, std::string aiStream, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaSetArLevelDistance(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUpdateArTowerHeight(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaUpdateAIPosCor(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
		, std::string from_guid = std::string(), std::string to_take_interface = std::string()
		, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaSetSeekPercent(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
		, std::string from_guid = std::string(), std::string to_take_interface = std::string()
		, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaSetVideoPause(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
		, std::string from_guid = std::string(), std::string to_take_interface = std::string()
		, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaPlaybackMarkRecord(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
		, std::string from_guid = std::string(), std::string to_take_interface = std::string()
		, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaGetHeatmapTotal(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaSnapshot(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaVideoClipRecord(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string updatePullUrl(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaFireSearchInfo(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());
		std::string switchHangarStream(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
				, std::string from_guid = std::string(), std::string to_take_interface = std::string()
				, std::string msg_id = std::string(), std::string seq = std::string());
		std::string smaSetAirborne45G(std::string param_str, RpcClientCallback rcp_client_callback = nullptr
			, std::string from_guid = std::string(), std::string to_take_interface = std::string()
			, std::string msg_id = std::string(), std::string seq = std::string());

		void installApi();
		std::string getUserGuid();
		//http接口 选控上云，需要发mqtt消息实现
		std::string sendMqttMsg(std::string api, std::string param_str, std::map<std::string, std::string> ar_file = std::map<std::string, std::string>(), std::string aiStream = std::string());
		void handleMessageArFile(const Poco::JSON::Object& message, std::string& ar_vector_file, std::string& ar_camera_config);
		void handleMessageAnnotationFile(const Poco::JSON::Object& message, std::string& ar_annotation_elements, std::string& ar_camera_config);
		void handleMessageAiModelFile(const Poco::JSON::Object& message, std::string& ai_model_file);
		void handleMessageAiOnnxFile(const Poco::JSON::Object& message, std::string& ai_onnx_file);
		std::string handleMqttSubMsg(const Poco::JSON::Object& recv_message, const std::string sub_topic);

	private:
		void installWebApi();
		void installMqttApi();
		static std::string jsonToString(Poco::JSON::Object::Ptr data_json);
		static Poco::JSON::Object::Ptr stringToJson(const std::string& input);

	private:
		std::shared_ptr<StreamMediaApplicationServerImpl> _impl{};
		std::string _station_sn{};

	private:
		StreamMediaApplicationServer();
		StreamMediaApplicationServer(StreamMediaApplicationServer& other) = delete;
		StreamMediaApplicationServer(StreamMediaApplicationServer&& other) = delete;
		StreamMediaApplicationServer& operator=(StreamMediaApplicationServer& other) = delete;
		StreamMediaApplicationServer&& operator=(StreamMediaApplicationServer&& other) = delete;
	};
}

extern "C"
{
    PLUGIN_EXPORT_API void PluginLoad();
	PLUGIN_EXPORT_API void PluginUnLoad();
	PLUGIN_EXPORT_API MSType PluginType();
	PLUGIN_EXPORT_API const char* PluginName();
	PLUGIN_EXPORT_API const char* PluginGuid();
}

#endif // STREAM_MEDIA_APPLICATION_SERVER_H