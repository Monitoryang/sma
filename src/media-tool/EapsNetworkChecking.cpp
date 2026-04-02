#include "EapsNetworkChecking.h"
#include "Poco/Net/NetworkInterface.h"
#include "Logger.h"
#include "Config.h"
#include "EapsConfig.h"
#include <stdio.h>
#include <stdlib.h>

namespace eap {
	namespace common {
#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

		std::mutex NetWorkChecking::_InstanceMutex{};
		NetWorkChecking* NetWorkChecking::_Instance{};

		NetWorkChecking::NetWorkChecking()
		{
			_AdapterStatusCheck();

			_CheckTimer.start(2000, std::bind([this]()
			{
				_AdapterStatusCheck();
			}));
		}

		NetWorkChecking* NetWorkChecking::Instance()
		{
			std::lock_guard<std::mutex> lock(_InstanceMutex);
			if (!_Instance) {
				_Instance = new NetWorkChecking();
			}
			return _Instance;
		}

		NetWorkChecking::~NetWorkChecking()
		{
			Stop();
		}

		bool NetWorkChecking::HaveLanAdapter()
		{
			std::lock_guard<std::mutex> lock(_GlobalMutex);
			return _HaveLanAdapter;
		}

		std::vector<NetWorkChecking::AdapterInfo> NetWorkChecking::GetLanAdapterArray()
		{
			std::lock_guard<std::mutex> lock(_GlobalMutex);
			return _LanAdapterInfos;
		}

		bool NetWorkChecking::HaveWLanAdapter()
		{
			std::lock_guard<std::mutex> lock(_GlobalMutex);
			return _HaveWLanAdapter;
		}

		std::vector<NetWorkChecking::AdapterInfo> NetWorkChecking::GetWLanAdapterArray()
		{
			std::lock_guard<std::mutex> lock(_GlobalMutex);
			return _WLanAdapterInfos;
		}

		std::string NetWorkChecking::getEth0Ip()
		{
			std::string network_name = "eth0";
			if (!eap::configInstance().has(eap::sma::Media::kNetworkName))
			{
				eap::configInstance().setString(eap::sma::Media::kNetworkName, "eth0");
				eap::saveConfig();
			}
			else {
				network_name = eap::configInstance().getString(eap::sma::Media::kNetworkName);
			}
			auto eth0 = Poco::Net::NetworkInterface::forName(network_name);
			return eth0.firstAddress(Poco::Net::IPAddress::IPv4).toString();
		}

		void NetWorkChecking::Stop()
		{
			/*_AdapterStatusChangedCallbacksMutex.lock();
			_AdapterStatusChangedCallbacks.clear();
			_AdapterStatusChangedCallbacksMutex.unlock();*/

			_CheckTimer.stop();
		}

		void NetWorkChecking::_AdapterStatusCheck()
		{
			std::lock_guard<std::mutex> lock(_GlobalMutex);

			decltype(_LanAdapterInfos) lan_adapter_infos_temp{};
			decltype(_WLanAdapterInfos) wlan_adapter_infos_temp{};

			auto adapter_list = Poco::Net::NetworkInterface::list();
			
			for (auto& adapter : adapter_list) {
				try
				{
					/*InfoL << "type: " << (int)adapter.type();
					InfoL << "Name: " << adapter.name();
					InfoL << "displayName: " << adapter.displayName();
					InfoL << "adapterName: " << adapter.adapterName();
					InfoL << "firstAddress: " << adapter.firstAddress(Poco::Net::IPAddress::IPv4);
					InfoL << "macAddress: " << adapter.macAddress();*/
					std::string name = adapter.name();
					if (name.find("br-") != std::string::npos ||
						name.find("docker") != std::string::npos) {
						continue;
					}
					auto address = adapter.firstAddress(Poco::Net::IPAddress::IPv4);
					auto type = adapter.type();

					AdapterInfo adapter_info{};
					adapter_info.name = adapter.adapterName();
					adapter_info.address = adapter.firstAddress(Poco::Net::IPAddress::IPv4).toString();

					if (adapter.isRunning() && adapter.isUp()) {
						adapter_info.status = NetworkAdapterStatus::Up;
					}
					else {
						adapter_info.status = NetworkAdapterStatus::Down;
					}
					
					if (type == Poco::Net::NetworkInterface::NI_TYPE_ETHERNET_CSMACD) {
						adapter_info.type = NetworkAdapterType::Lan;

						lan_adapter_infos_temp.push_back(adapter_info);

						_HaveLanAdapter = true;
					}
					else if (type == Poco::Net::NetworkInterface::NI_TYPE_IEEE80211 ||
						type == Poco::Net::NetworkInterface::NI_TYPE_IEEE1394) {
						adapter_info.type = NetworkAdapterType::WLan;

						wlan_adapter_infos_temp.push_back(adapter_info);

						_HaveWLanAdapter = true;
					}
					else {
						continue;
					}
				}
				catch (const std::exception& e)
				{
					//eap_error(std::string(e.what()));
				}
			}

			std::swap(_LanAdapterInfos, lan_adapter_infos_temp);
			std::swap(_WLanAdapterInfos, wlan_adapter_infos_temp);
		}
	}
}
