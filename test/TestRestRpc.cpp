#include <rest_rpc.hpp>
#include "EapsUtils.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include "Poco/JSON/JSON.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Object.h"
using namespace rest_rpc;
using namespace rest_rpc::rpc_service;

bool g_stopFlag = false;
void signalHandler(int signal) {
	if (signal == SIGINT) {
		std::cout << "Ctrl+C signal received." << std::endl;
		g_stopFlag = true;
	}
}

int main(int argc, char** argv)
{
	

	// 设置信号处理程序
	std::signal(SIGINT, signalHandler);

	// 执行主循环
	//while (!g_stopFlag) {
	//	// 在这里执行你的程序逻辑
		try {
			rpc_client client("127.0.0.1", 9000);
			bool r = client.connect();
			if (!r) {
				std::cout << "rest_rpc connect timeout" << std::endl;
				return 0;
			}

			std::map<std::string, std::string> ar_file;
			ar_file["ar_vector"] = "C:\\Users\\liuxuedan\\Desktop\\泰州演示\\泰州一条杆塔和线路.kml";
			ar_file["ar_camera"] = "C:\\Users\\liuxuedan\\AppData\\Local\\Jouav\\JPlayer\\1.0.0.0\\camera.config";


			std::string paramsStr = R"(
									{
										"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee",
										"pull_url":"udp://224.12.34.56:55066",
										"push_url":"webrtc://127.0.0.1/live/test_lxd",
										"func_mask": 2
									})";
			std::string result = "";
			std::string id = "0c081e6aed6fae46e95290b1a2fe91b0";
			result = client.call<std::string>("SmastartProcess", paramsStr);
			std::cout << " startProcess result " <<result << std::endl;
			//Poco::JSON::Parser parser;
			//Poco::Dynamic::Var resultVar = parser.parse(result);
			//Poco::JSON::Object params = *(resultVar.extract<Poco::JSON::Object::Ptr>());
			//id = params.has("id")? params.get("id").toString(): "";
			//std::this_thread::sleep_for(std::chrono::milliseconds(30 * 1000 * 1));

			//{
			//	paramsStr = R"({"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", "id":"visual"})";
			//	result = client.call<std::string>("SmagetMediaList", paramsStr);
			//	std::cout << " getMediaInfo result " << result << std::endl;
			//}

			//std::this_thread::sleep_for(std::chrono::milliseconds(1*1000));
			//
			//{
			//	Poco::JSON::Object params;
			//	params.set("secret", "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
			//	params.set("id", id);
			//	paramsStr = eap::sma::jsonToString(params);

			//	result = client.call<std::string>("SmagetMediaInfo", paramsStr);
			//	std::cout << " SmagetMediaInfo result " << result << std::endl;
			//	std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

			//	result = client.call<std::string>("SmastopProcess", paramsStr);
			//	std::cout << " stopProcess result " << result << std::endl;
			//}
			//std::this_thread::sleep_for(std::chrono::milliseconds(60 * 1000));
			
		}
		catch (const std::exception &e) {
			std::cout<< "exception -- " << e.what() << std::endl;
		}
	//}

	return 0;
}