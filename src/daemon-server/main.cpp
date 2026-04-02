#include "Logger.h"
#include "OnceToken.h"
#include "Config.h"
#include "EapsConfig.h"
#include "HttpClient.h"
#include "Utils.h"

#include "Poco/SingletonHolder.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/Exception.h"
#include "Poco/Process.h"
#include "Poco/Util/Option.h"
#include "Poco/Net/NetSSL.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "ServerApplication.h"

#include <signal.h>
#include <iostream>
#include <regex>

#if defined(ENABLE_VERSION)
#include "version.h"
#endif


using namespace std;
using namespace eap;
using namespace eap::sma;

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
		} else {
			eap::loadLogging();
			eap::sma::loadIniConfig();
			eap::loadConfig();

			Poco::Path current_exe = Poco::Path::self();
			std::string jo_video_application_server_path = eap::getCurrentPath() + "/JoEAPSmaSub";
//#if defined(_WIN32)
//			jo_video_application_server_path += ".exe";
//#endif
			std::string api_secret = "035c73f7-bb6b-4889-a715-d9eb2d1925ee";
			Poco::Process::Args args{};
			
			try {
				auto current_path = current_exe.parent().parent().parent().toString();
				// 首次来先启动流媒体应用服务(initialDirectory 库的搜索目录)
				Poco::ProcessHandle handle = Poco::Process::launch(jo_video_application_server_path, args, current_path);
				eap_information_printf( "Daemon server start,edge path: %s, sub path %s", current_path, jo_video_application_server_path);
				// 查询流媒体应用服务的配置文件信息
				std::string http_port{};
				try {
					GET_CONFIG(std::string, getString, my_http_port, http::kPort);
					eap_information("JoEAPSmaSub http port is" + my_http_port);
					http_port = my_http_port;
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kPort throw exception: %s", e.what());
				}

				// JoEAPSmaSubSub 是否正常，不正常就重启（尝试5次，都还启动不成功，就退出当前主进程)
				int reboort_count{};
				do
				{
					int code_check = handle.tryWait();
					if(-1 != code_check){
						eap_error_printf("%s exited, code_check: %d, restart it", jo_video_application_server_path, code_check);
						handle = Poco::Process::launch(jo_video_application_server_path, args, current_path);
						reboort_count++;
						eap_warning_printf("%s restart: %d", jo_video_application_server_path, reboort_count);
					}
					else{
						break;
					}				
				} while (reboort_count > 5);
				
				if(reboort_count > 5){
					reboort_count = 0;
					throw std::runtime_error("start JoEAPSmaSub 5 times fail");
				}

				// 构造http请求查询流媒体应用服务media list的参数
				std::string uri = std::string("http://127.0.0.1:") + http_port 
					+ "/index/api/sma/smaGetMediaList" + std::string("?") + std::string("secret=") + api_secret;
				std::string body{};
				std::string schema_domain_port{};

				std::string paths{};
				std::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
				std::cmatch what;
				if (std::regex_match(uri.c_str(), what, ex)) {
					std::string schema = std::string(what[1].first, what[1].second);
					std::string domain = std::string(what[2].first, what[2].second);
					std::string port = std::string(what[3].first, what[3].second);
					std::string path = std::string(what[4].first, what[4].second);
					std::string query = std::string(what[5].first, what[5].second);

					schema_domain_port = schema + "://" + domain;
					if (!port.empty()) {
						schema_domain_port += ":";
						schema_domain_port += port;
					}

					paths = path + std::string("?") + query;
				}
				else {
					throw std::runtime_error("url invalid");
				}

				auto http_client = HttpClient::createInstance();
				// 循环重启
				while (1)
				{
					// 检查JoEAPSmaSub运行是否正常，不正常，尝试重启5次
					reboort_count = 0;
					do
					{
						int code_check = handle.tryWait();
						if(-1 != code_check){
							eap_error_printf("%s exited, code_check: %d, restart it" ,jo_video_application_server_path, code_check);
							handle = Poco::Process::launch(jo_video_application_server_path, args, current_path);
							reboort_count++;
							eap_warning_printf("%s restart: %d", jo_video_application_server_path, reboort_count);
						}
						else{
							break;
						}				
					} while (reboort_count > 5);
					
					if(reboort_count > 5){
						reboort_count = 0;
						throw std::runtime_error("start JoEAPSmaSub 5 times fail");
					}
				
					// 查询 media list
					http_client->doHttpRequest(schema_domain_port, body, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response)
					{
						if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
							try {
								Poco::JSON::Parser parser;
								auto dval = parser.parse(response);
								auto res_json = dval.extract<Poco::JSON::Object::Ptr>();

								if (res_json) {
									// TODO: 不在意回复的情况
									auto code = res_json->has("code")? res_json->getValue<int>("code"): -1;
									if (code != 0) {
										// 请求返回错误码，不能抛异常（该进程会被结束），也不能去kill流媒体应用服务器重启（可能存在任务）
										eap_error_printf( "post http to JoEAPSmaSub, response code: %d",code);
										return;
									}
									
									// media list 是否为空
									if(!res_json->has("data")){
										// 停止进程并重启
										auto code = handle.tryWait();
										if(-1 == code){
											eap_information( jo_video_application_server_path + " running, ready to kill it");
											Poco::Process::kill(handle);
											handle.wait();
											eap_information( jo_video_application_server_path + " has been manual kill");
											handle = Poco::Process::launch(jo_video_application_server_path, args, current_path);
											eap_information( jo_video_application_server_path + " manual restart");
										}
									}
								}
								else {

								}
							}
							catch (const std::exception&) {
								Poco::JSON::Object obj1;
								eap_warning("post http to JoEAPSmaSub faile");
							}
						}
						else {
							// 请求失败，不能抛异常（该进程会被结束），也不能去kill流媒体应用服务器重启（可能存在任务）
							eap_warning("post http to JoEAPSmaSub faile");
						}

					});

					std::this_thread::sleep_for(std::chrono::seconds(300));
				}
			} catch (std::exception &ex) {
				eap_warning( "The port is occupied or has no permission:" + std::string(ex.what()));
				eap_error( "The program failed to start. Please change the port number in the configuration file and try again!");
				std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 1));
				return -1;
			}

			// wait for CTRL-C or kill
			waitForTerminationRequest();
			// if(handle.id())
			// 	Poco::Process::kill(handle);
		}

		//休眠1秒再退出，防止资源释放顺序错误
		eap_information( "The program is exiting, please wait...");


		std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 1));
		eap_information( "Program exit complete!");
		return Application::EXIT_OK;
	}

private:
	bool _helpRequested;
};

int main(int argc, char** argv)
{
	DaemonMainServer app;
	return app.run(argc, argv);
}
