#include "HttpServer.h"
#include "RestRpcApi.h"
#include "WebHook.h"
#include "Config.h"
#include "Logger.h"
#include "Utils.h"
#include "WebHookBasic.h"
#include "MqttClient++.h"
#include "EapsDispatchCenter.h"
#include "StreamMediaApplicationServer.h"
#include "EapsConfig.h"
#include "EapsUtils.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "ServerApplication.h"
#include "ReadAIconfig.hpp"

#include <rest_rpc.hpp>
using namespace rest_rpc;
using namespace rest_rpc::rpc_service;
using namespace eap;
using namespace eap::sma;

static int findAvailablePort(int start_port, int end_port)
{
	for ( int port = start_port; port < end_port; ++port ){
		try
		{
			Poco::Net::ServerSocket server_socket{};
			server_socket.bind(Poco::Net::SocketAddress("127.0.0.1", port), true);
			server_socket.close();
			return port;
		}
		catch (const Poco::Exception& e)
		{
			eap_error_printf("the port %d is unavailable, exception: %s. try the next port", port, e.what());
		}
	}
}

static void httpResponseCallback(Poco::Net::HTTPResponse::HTTPStatus status, Poco::JSON::Object& body)
{
	if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
		eap_information("the microservice reported info to primary service success");
	}
	else {
		eap_error_printf("the microservice reported info to primary service faile, status is %d", int(status));
	}
}

class DaemonMainServer : public eap::ServerApplication
	/// The main application class.
	///
	/// This class handles command-line arguments and
	/// configuration files.
	/// Start the JoHoneyAtlas executable with the help
	/// option (/help on Windows, --help on Unix) for
	/// the available command line options.
{
public:
	DaemonMainServer() : _helpRequested(false)
	{
		registCrashHandler();
		Poco::Net::initializeSSL();
	}

	~DaemonMainServer()
	{
		_is_stop = true;
		Poco::Net::uninitializeSSL();
	}

protected:
	void initialize(Application& self)
	{
		loadConfiguration(); // load default configuration files, if present
		ServerApplication::initialize(self);
	}

	void uninitialize()
	{
		ServerApplication::uninitialize();
	}

	void defineOptions(Poco::Util::OptionSet& options)
	{
		ServerApplication::defineOptions(options);

		options.addOption(
			Poco::Util::Option("help", "h", "display help information on command line arguments")
			.required(false)
			.repeatable(false));
	}

	void handleOption(const std::string& name, const std::string& value)
	{
		ServerApplication::handleOption(name, value);

		if (name == "help")
			_helpRequested = true;
	}

	void displayHelp()
	{
		Poco::Util::HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("OPTIONS");
		helpFormatter.setHeader("A web server that serves the all microservice.");
		helpFormatter.format(std::cout);
	}

	int main(const std::vector<std::string>& args)
	{
		if (_helpRequested) {
			displayHelp();
		}
		else {
			eap::initRpcOption();
			eap::loadLogging();
			eap::sma::loadIniConfig();
			eap::loadConfig();
			eap::sma::loadIniConfig();
			// eap::sma::loadHangarComConfig(); // 加载主控通用配置，如果主控有相关字段，以主控的为主

			// 获取rpc服务可绑定的端口

			std::string start_port{};
			std::string end_port{};
			try {
				GET_CONFIG(std::string, getString, my_start_port, SmaRpc::kStartPort);
				GET_CONFIG(std::string, getString, my_end_port, SmaRpc::kEndPort);
				start_port = my_start_port;
				end_port = my_end_port;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kStartPort or kEndPort throw exception: %s", e.what());
			}

			int rpc_port{};
			rpc_port = findAvailablePort(std::stoi(start_port), std::stoi(end_port));
			if (0 != rpc_port) {
				eap_information_printf("rpc port is %d", rpc_port);
			}
			else {
				eap_error("can't find available rcp port to bind");
				// TODO: 没有找到可用端口，应该直接退出，并报告主服务
			}
			eap::sma::DispatchCenter::Instance()->start();

			// 给主服务上报信息 http	 TODO:
			std::string local_ip = "127.0.0.1";
			std::string rpc_port_str = std::to_string(rpc_port);
			std::string ms_type = "0";

			std::string sma_guid{};
			std::string sma_version{};
			try {
				GET_CONFIG(std::string, getString, my_sma_guid, General::kGuid);
				GET_CONFIG(std::string, getString, my_sma_version, General::kServerVersion);
				sma_guid = my_sma_guid;
				sma_version = my_sma_version;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kGuid or kServerVersion throw exception: %s", e.what());
			}
			auto http_port = args.size() ? args[0] : "80";
			std::string url = "http://" + local_ip + ":" + http_port + "/index/api/all/report?ip=" + local_ip
				+ "&port=" + rpc_port_str + "&type=" + ms_type + "&guid=" + sma_guid + "&version=" + sma_version;
			//eap::ThreadPool::defaultPool().start([url]() {
				eap::WebHookBasic::defaultWebHook()->doHttpRequest(url, httpResponseCallback);
				//TODO: 上报信息返回失败要不要继续启动？
			//});

#ifdef ENABLE_AIRBORNE
			// 机载添加组播路由
			int result = std::system("ip route add 224.0.0.0/4 dev eth0 2>/dev/null");
			if (result != 0) {
				// 处理错误（非0返回值通常意味着失败），但因为我们用了2>/dev/null，且路由已存在时也会返回非0，所以这里通常只是日志记录
				eap_error("Note: Could not add multicast route (it might already exist).");
			}

#endif

			installRestrpc(rpc_port);
			eap_information_printf("rpc server has been started, port: %d", rpc_port);
			initTasks();

			// 读取joedge_version.json文件内容（使用程序当前目录）
			try {
				// 获取程序所在目录
				std::string exeDir = eap::sma::exeDir();
				// 拼接JSON文件路径
				Poco::Path jsonFilePath(exeDir);
				jsonFilePath.append("joedge_version.json");
				
				std::string jsonContent = eap::sma::readFileContents(jsonFilePath.toString());
				eap_information_printf("Successfully read joedge_version.json content, size: %zu bytes", jsonContent.size());
				// 这里可以添加对jsonContent的后续处理逻辑
				eap::sma::DispatchCenter::Instance()->receiveVersinData(jsonContent);
			} catch (const std::exception& e) {
				eap_error_printf("Failed to read joedge_version.json: %s", e.what());
			}

			_timeout_check_timer.start(1000*600*2, [this]()
			{
				if (eap::sma::DispatchCenter::Instance()->getTaskCount() == 0) {
					Poco::Path current_exe = Poco::Path::self();
					auto process_name = current_exe.getFileName();
					eap::cleanProcessByName(process_name);
				}
			});

#ifdef APPLICATION_ENABLE
			std::string mqtt_url = "127.0.0.1";
			std::string user_name = "jouav";
			std::string passwd = "123456";
			std::string station_sn = "jouav-GCS1009";

			MqttClient::Ptr mqtt_client{};
			//  MqttClient::Ptr mqtt_client = MqttClient::createInstance(mqtt_url, station_sn, user_name, passwd);
			//  mqtt_client->Connect([](int error_code) -> bool {
			//  	if (error_code != 0) {
			//  		return true;
			//  	}

			//  	return false;
			//  	}, [](std::string cause) -> bool {
			//  		eap_error_printf("MQTT client connect lost, description: %s", cause);
			//  		fflush(stdout);
			//  		return true;
			//  	});

			Poco::Net::ServerSocket svs(8080);
			eap::WebApiBasic::instance()->start(svs);

			eap::StreamMediaApplicationServer::Ptr sma = eap::StreamMediaApplicationServer::createInstance();
			sma->start(eap::WebApiBasic::instance(), mqtt_client, "127.0.0.1", rpc_port);
#endif

			// wait for CTRL-C or kill
			waitForTerminationRequest();
			eap_information(" tasks stop start! ");
			eap::sma::DispatchCenter::Instance()->stop();
			eap_information(" tasks stop finish! ");
			_is_stop = true;
			_timeout_check_timer.stop();
			unInstanceRestRpc();
		}

		eap_information("The program was exited");

		return Application::EXIT_OK;
	}

private:
	bool _helpRequested;
	bool _is_stop{};
	common::Timer _timeout_check_timer{};
};

int main(int argc, char** argv)
{
	DaemonMainServer app;
	return app.run(argc, argv);
}