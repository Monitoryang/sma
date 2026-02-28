#ifndef REST_RPC_API_H
#define REST_RPC_API_H

#include "WebApiBasic.h"
#include "mutex"

namespace eap {
	namespace sma {
		extern std::string g_ini_file;

		template<typename Args, typename First>
		bool checkSmaArgs(Args &args, const First &first) {
			return args.has(first);
		}

		template<typename Args, typename First, typename ...KeyTypes>
		bool checkSmaArgs(Args &args, const First &first, const KeyTypes &...keys) {
			return checkSmaArgs(args, first) && checkSmaArgs(args, keys...);
		}

		//检查http url中或body中或http header参数是否为空的宏
		#define CHECK_SMA_ARGS(...)  \
		if(!checkSmaArgs(params, ##__VA_ARGS__)){ \
			std::string jsonString = ("{\"code\": -300, \"msg\": \"Required parameter missing\"}");  \
			return jsonString; \
		}

		//检查http参数中是否附带secret密钥的宏，127.0.0.1的ip不检查密钥
		#define CHECK_SMA_SECRET() \
		CHECK_SMA_ARGS("secret"); \
		if (params.has( "secret" )&& api_secret != params.get("secret").toString()) { \
			std::string jsonString =  ("{\"code\": -300, \"msg\": \"Missing secret parameter\"}");  \
			return jsonString; \
		} 

		void installRestrpc(unsigned int port);

		void unInstanceRestRpc();

		//初始化配置文件默认视频任务，和tasks文件夹里的任务
		void initTasks();
	}
}

#endif // !REST_RPC_API_H