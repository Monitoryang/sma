#ifndef EAPSNETWORKCHECKING_H
#define EAPSNETWORKCHECKING_H
#pragma once
#include "EapsTimer.h"
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

namespace eap {
	namespace common {
		class NetWorkChecking
		{
		public:
			enum class NetworkAdapterType { Lan, WLan };
			enum class NetworkAdapterStatus { Up, Down };

			struct AdapterInfo
			{
				std::string name{};
				NetworkAdapterType type{};
				NetworkAdapterStatus status{};
				std::string address{};
			};

			using AdapterStatusChangedCallback =
				std::function<void(NetworkAdapterType type, NetworkAdapterStatus new_status,
					std::string address, std::string name, int index)>;

			static NetWorkChecking* Instance();

		public:
			~NetWorkChecking();

			bool HaveLanAdapter();
			std::vector<AdapterInfo> GetLanAdapterArray();
			bool HaveWLanAdapter();
			std::vector<AdapterInfo> GetWLanAdapterArray();
			std::string getEth0Ip();
			void Stop();
		private:
			void _AdapterStatusCheck();

			static std::mutex _InstanceMutex;
			static NetWorkChecking* _Instance;

		private:
			Timer _CheckTimer{};
			std::mutex _GlobalMutex{};
			bool _Inited{};
			std::mutex _LanAdapterInfosMutex{};
			std::vector<AdapterInfo> _LanAdapterInfos{};
			std::mutex _WLanAdapterInfosMutex{};
			std::vector<AdapterInfo> _WLanAdapterInfos{};

			bool _HaveLanAdapter{};
			bool _HaveWLanAdapter{};

		private:
			NetWorkChecking();
			NetWorkChecking(NetWorkChecking& other) = delete;
			NetWorkChecking(NetWorkChecking&& other) = delete;
			NetWorkChecking& operator=(NetWorkChecking& other) = delete;
			NetWorkChecking& operator=(NetWorkChecking&& other) = delete;
		};
	}
}
#endif EAPSNETWORKCHECKING_H
