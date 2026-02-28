#include <rest_rpc.hpp>
#include "EapsUtils.h"
#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>
#include <chrono>

#include "Poco/JSON/JSON.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Object.h"
using namespace rest_rpc;
using namespace rest_rpc::rpc_service;

constexpr size_t CHUNK_SIZE = 9 * 1024 * 1024; // 9MB

bool g_stopFlag = false;
void signalHandler(int signal) {
	if (signal == SIGINT) {
		std::cout << "Ctrl+C signal received." << std::endl;
		g_stopFlag = true;
	}
}

std::string readFileToJsonString(const std::string& filePath)
{
	std::ifstream file(filePath);
	if(!file.is_open())
	{
		std::cerr << "Unable to open file: " << filePath << std::endl;
		return "";
	}

	std::string jsonString((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	return jsonString;
}


int main(int argc, char** argv)
{
	

	// 设置信号处理程序
	std::signal(SIGINT, signalHandler);

	// 执行主循环
	//while (!g_stopFlag) {
	//	// 在这里执行你的程序逻辑
		try {
			rpc_client client("127.0.0.1", 8000);
			bool r = client.connect();
			if (!r) {
				std::cout << "rest_rpc connect timeout" << std::endl;
				return 0;
			}

			std::map<std::string, std::string> ar_file;
			ar_file["ar_vector"] = "C:\\Users\\liuxuedan\\Desktop\\泰州演示\\泰州一条杆塔和线路.kml";
			ar_file["ar_camera"] = "C:\\Users\\liuxuedan\\Desktop\\泰州演示\\camera.config";

			std::string jsonString = readFileToJsonString("C:\\Users\\liuxuedan\\Desktop\\mark_test.json");
			std::cout << "mark json:" << jsonString << std::endl;
			ar_file["video_mark_data"] = jsonString;
			std::string paramsStr = R"(
									{
										"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee",
										"pull_url":"udp://224.12.34.56:55067",
										"push_url":"webrtc://127.0.0.1/jouav/test1",
										"func_mask": 2
									})";
			std::string result = "";
			std::string id = "e79a448454111f2a2892cac506a61b22";
			result = client.call<std::string>("smaStartProcess", paramsStr, ar_file);
			std::cout << " startProcess result " <<result << std::endl;
			Poco::JSON::Parser parser;
			Poco::Dynamic::Var resultVar = parser.parse(result);
			Poco::JSON::Object resultparams = *(resultVar.extract<Poco::JSON::Object::Ptr>());
			id = resultparams.has("id") ? resultparams.get("id").toString() : "";

		std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
			//{
			//	Poco::JSON::Object params;
			//	params.set("secret", "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
			//	params.set("level_one_distance", 10);
			//	params.set("level_two_distance", 20);
			//	params.set("id", id);
			//	paramsStr = eap::sma::jsonToString(params);

			//	result = client.call<std::string>("SmasetArLevelDistance", paramsStr);
			//	std::cout << " SmasetArLevelDistance result " << result << std::endl;

			//}


			{
				Poco::JSON::Object params;
				params.set("secret", "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
				params.set("id", id);
				paramsStr = eap::sma::jsonToString(params);

				result = client.call<std::string>("SmaaddAnnotationElements", paramsStr, ar_file);
				std::cout << " SmaaddAnnotationElements result " << result << std::endl;

			}
			//{
			//	std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
			//	Poco::JSON::Object params;
			//	params.set("secret", "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
			//	params.set("id", id);
			//	paramsStr = eap::sma::jsonToString(params);

			//	result = client.call<std::string>("SmaaddAnnotationElements", paramsStr, ar_file);
			//	std::cout << " SmaaddAnnotationElements result " << result << std::endl;

			//}
			{ 
				std::this_thread::sleep_for(std::chrono::milliseconds(10 * 1000));
				Poco::JSON::Object params1;
				params1.set("secret", "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
				params1.set("id", id);
				params1.set("elements_guid", "1");
				paramsStr = eap::sma::jsonToString(params1);
				result = client.call<std::string>("smaDeleteAnnotationElements", paramsStr);
				std::cout << " smaDeleteAnnotationElements result " << result << std::endl;
			}
			std::this_thread::sleep_for(std::chrono::minutes(5 ));
			/*Poco::JSON::Parser parser;
			Poco::Dynamic::Var resultVar = parser.parse(result);
			Poco::JSON::Object resultparams = *(resultVar.extract<Poco::JSON::Object::Ptr>());
			id = resultparams.has("id")? resultparams.get("id").toString(): "";
			

			Poco::JSON::Object params;
			params.set("secret", "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
			params.set("id", id);
			paramsStr = eap::sma::jsonToString(params);*/

			//result = client.call<std::string>("smaGetServerConfig", paramsStr);
			//std::cout << " smaGetServerConfig result " << result << std::endl;

			//result = client.call<std::string>("smaGetAIRelated", paramsStr);
			//std::cout << " smaGetAIRelated result " << result << std::endl;

			//{
			//	result = client.call<std::string>("smaStopProcess", paramsStr);
			//	std::cout << " smaStopProcess result " << result << std::endl;
			//}

			/*{
				paramsStr = R"({"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", "id":"visual"})";
				result = client.call<std::string>("smaGetMediaList", paramsStr);
				std::cout << " getMediaInfo result " << result << std::endl;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1*1000));
			
			{
				Poco::JSON::Object params;
				params.set("secret", "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
				params.set("id", id);
				paramsStr = eap::sma::jsonToString(params);

				result = client.call<std::string>("smaGetMediaInfo", paramsStr);
				std::cout << " smaGetMediaInfo result " << result << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

				result = client.call<std::string>("smaStopProcess", paramsStr);
				std::cout << " stopProcess result " << result << std::endl;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(60 * 1000));*/
			
			/*{
				std::ifstream file("/usr/src/tensorrt/bin/v8l_640_384_51_V2.0.onnx", std::ios::binary);
				if (!file) {
					std::cerr << "Unable to open file!" << std::endl;
					return 1;
				}

				std::string paramsStr;
				std::string chunk;
				size_t totalFileSize = 0;
				size_t chunkIndex = 0;

				std::vector<char> buffer(CHUNK_SIZE);
				while (!file.eof()) {
					//读取 CHUNK_SIZE 大小的数据块
					std::cout << "File upload START." << std::endl;
					file.read(buffer.data(), CHUNK_SIZE);
					size_t bytesRead = file.gcount();

					// 将数据块转换为字符串，并添加到 chunk
					chunk.assign(buffer.data(), bytesRead);

					// 构建参数字符串
					paramsStr = R"({
									"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", 
									"ai_onnx_file": "v8l_640_384_51_V2.0.onnx",
									"total_file_size": )" + std::to_string(totalFileSize + bytesRead) + R"(,
									"chunk_index": )" + std::to_string(chunkIndex) + R"(,
									"is_first_chunk": )" + (chunkIndex == 0 ? "true" : "false") + R"(,
									"is_last_chunk": )" + (file.eof() ? "true" : "false") + R"(})";

					// 发送该数据块到服务器
					auto result = client.call<std::string>("smaUploadAIOnnxFile", paramsStr, chunk);

					// 更新片段索引和总文件大小
					++chunkIndex;
					totalFileSize += bytesRead;
				}

				std::cout << "File upload completed." << std::endl;
			}*/
		
			/*{
				std::ifstream file("/usr/src/tensorrt/bin/v8l_640_384_51_V2.0.engine", std::ios::binary);
				if (!file) {
					std::cerr << "Unable to open file!" << std::endl;
					return 1;
				}

				std::string paramsStr;
				std::string chunk;
				size_t totalFileSize = 0;
				size_t chunkIndex = 0;

				std::vector<char> buffer(CHUNK_SIZE);
				while (!file.eof()) {
					//读取 CHUNK_SIZE 大小的数据块
					std::cout << "File upload START." << std::endl;
					file.read(buffer.data(), CHUNK_SIZE);
					size_t bytesRead = file.gcount();

					// 将数据块转换为字符串，并添加到 chunk
					chunk.assign(buffer.data(), bytesRead);

					// 构建参数字符串
					paramsStr = R"({
									"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", 
									"model_width": "640",
									"model_heitht": "384",
									"class_number": "51",
									"conf_thresh": "0.45",
									"nms_thresh": "0.25",
									"yolo_version": "Yolov8",
									"track_switch": "0",
									"track_buff_len": "30",
									"ai_model_file": "windows_onnxtoengine_v8l_640_384_51_V2.0.engine",
									"total_file_size": )" + std::to_string(totalFileSize + bytesRead) + R"(,
									"chunk_index": )" + std::to_string(chunkIndex) + R"(,
									"is_first_chunk": )" + (chunkIndex == 0 ? "true" : "false") + R"(,
									"is_last_chunk": )" + (file.eof() ? "true" : "false") + R"(})";

					// 发送该数据块到服务器
					auto result = client.call<std::string>("smaUploadAIModelFile", paramsStr, chunk);

					// 更新片段索引和总文件大小
					++chunkIndex;
					totalFileSize += bytesRead;
				}

				std::cout << "File upload completed." << std::endl;
			}*/
	
			/*{
				std::string paramsStr = R"(
											{
												"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", 
												"find_engine":"1",
												"find_onnx":"1"
											})";
				auto result = client.call<std::string>("smaGetAIRelated", paramsStr);
				std::cout << " smaGetAIRelated result " << result << std::endl;
			}*/

		/*	{
				std::string paramsStr = R"(
											{
												"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", 
												"is_fp16":"1"
											})";
				auto result = client.call<std::string>("smaStartOnnxtoEngine", paramsStr);
				std::cout << " smaStartOnnxtoEngine result " << result << std::endl;
			}*/

			/*{
				std::string paramsStr = R"({"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee"})";
				auto result = client.call<std::string>("smaGetOnnxtoEnginePercent", paramsStr);
				std::cout << " smaGetOnnxtoEnginePercent result " << result << std::endl;
			}*/

			/*{
				std::string paramsStr = R"({"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee",
											"id":"9691dc3c5fd224735d2bb575416fa837",
											"func_mask": 1})";
				auto result = client.call<std::string>("smaUpdateFuncMask", paramsStr);
				std::cout << " smaUpdateFuncMask result " << result << std::endl;
			}*/

			//{
			//	std::string paramsStr = R"(
			//									{
			//										"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", 
			//										"id":"7e699beb5cebc41693c2a5c1b27c7b8e",
			//										"track_cmd":"1",
			//										"track_pixelpos_x":"540",
			//										"track_pixelpos_y":"540"
			//									})";
			//	auto result = client.call<std::string>("smaRequestAiAssistTrack", paramsStr);
			//	std::cout << " smaRequestAiAssistTrack result " << result << std::endl;
			//}

			/*{
				std::string paramsStr = R"({"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee"})";
				auto result = client.call<std::string>("SmagetServerConfig", paramsStr);
				std::cout << " SmagetServerConfig result " << result << std::endl;
			}

			{
				std::string paramsStr = R"({"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", 
													"id":"5bca77e8f4bb41d926ed50ecf9f3bc81",
				auto result = client.call<std::string>("smaSetServerConfig", paramsStr);
				std::cout << " SmasetServerConfig result " << result << std::endl;
			}*/

			// {
			// 	std::string paramsStr = R"({"secret": "035c73f7-bb6b-4889-a715-d9eb2d1925ee", 
			// 										"id":"18b7e7a29cbb81022f27fb1e65e7044e",
			// 										"path":"//mnt//sdcard//why//JoEAPSma//release//linux//Debug//snap//",
			// 										"numbers":"2"})";
			// 	auto result = client.call<std::string>("smaSaveSnapShot", paramsStr);
			// 	std::cout << " smaSaveSnapShot result " << result << std::endl;
			// }
		}
		catch (const std::exception &e) {
			std::cout<< "exception -- " << e.what() << std::endl;
		}
	//}
	#ifdef _WIN32
		system("pause");
	#else
		std::cout << "Press Enter to exit..." << std::endl;
		std::cin.get();
	#endif
	return 0;
}