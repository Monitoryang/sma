#ifndef EAPSDEMUXERREATOR_H
#define EAPSDEMUXERREATOR_H
#ifndef ENABLE_AIRBORNE
#define ASIO_STANDALONE
#endif
#include "asio.hpp"
#include "Logger.h"
#include "EapsDemuxerTradition.h"
#include "EapsNetworkChecking.h"
#include <string>
#include <memory>
#include <vector>

namespace eap {
	namespace sma {
		class DemuxerReactor;
		using DemuxerReactorPtr = std::shared_ptr<DemuxerReactor>;
#ifdef ENABLE_AIRBORNE
    using namespace boost;
#endif

		class DemuxerReactor
		{
		public:
			DemuxerReactor(std::string id, std::string adapter_name,
				common::NetWorkChecking::NetworkAdapterType adapter_type,
				common::NetWorkChecking::NetworkAdapterStatus adapter_status,
				std::string adapter_address, std::string url, std::chrono::milliseconds time_out, int frame_rate,
				Demuxer::StopCallback stop_callback, Demuxer::PacketCallback packet_callback,
				Demuxer::StreamRecoverCallback stream_recover_callback,
				Demuxer::TimeoutCallback timeout_callback)
			{
				_id = id;
				_PacketCallback = packet_callback;
				_AdapterName = adapter_name;
				_AdapterType = adapter_type;
				_AdapterStatus = adapter_status;
				_AdapterAddress = adapter_address;
				_IsOpened = false;
				_IsTimeout = false;

				eap_information( "new demuxer reactor++++++++++++++++++++++++++++++++++++");
				eap_information_printf( "current ip is %s",  _AdapterAddress);
				if (_AdapterStatus == common::NetWorkChecking::NetworkAdapterStatus::Up) {
					eap_information( "current status is up");
				} else {
					eap_information( "current status is down");
				}

				_Url = url;
				_TimeOut = time_out;
				_FrameRate = frame_rate;

				_timeout_callback = [this, timeout_callback]()
				{
					_IsTimeout = true;
					if (_VDemuxer) {
						_VDemuxer->stopCache();
					}
					if (timeout_callback) {
						timeout_callback();
					}
				};

				_stream_recover_callback = [this, stream_recover_callback]()
				{
					_IsTimeout = false;
					if (stream_recover_callback) {
						stream_recover_callback();
					}
				};

				_StopCallback = std::bind([this](int exit_code, Demuxer::StopCallback stop_callback)
				{
					if (stop_callback) {
						stop_callback(exit_code);
					}

					if (_VDemuxer) {
						_VDemuxer->stopCache();
					}

					//是否根据 exit_code 判断重新打开，现在是只要停掉就循环打开
					_IsOpened = false;
					_IsTimeout = false;
					_IsOpening = true;
					//调用stop回调时，由于_OpenDemuxer会reset _VDemuxer，而此时也可能同时触发了_timeout_callback，而其中在使用_VDemuxer
					//如此会导致_timeout_callback访问野指针
					if (!_IsStoped) {
						_ASIOIOContext.post(std::bind(&DemuxerReactor::_OpenDemuxer, this));
					}
				}, std::placeholders::_1, stop_callback);

				if (_AdapterStatus == common::NetWorkChecking::NetworkAdapterStatus::Up &&
					!_Url.empty() && _AdapterAddress.find("169.") == std::string::npos) {
					_IsOpening = true;
					_ASIOIOContext.post(std::bind(&DemuxerReactor::_OpenDemuxer, this));
				}

				_ASIOIOContextThread =
					std::thread([this]()
				{
					try {
						_ASIOIOContext.run();
					} catch (std::system_error& e) {
						return;
					}
				});

			}

			~DemuxerReactor()
			{
				Stop();
			}

			void SetStatusAddress(common::NetWorkChecking::NetworkAdapterStatus status,
				std::string address)
			{
				_AdapterStatus = status;
				_AdapterAddress = address;

				// 新的状态是Up
				if (status == common::NetWorkChecking::NetworkAdapterStatus::Up) {
					if (!_IsOpened && !_IsOpening) {
						if (!_Url.empty() && _AdapterAddress.find("169.") == std::string::npos) {
							_IsOpened = false;
							_IsTimeout = false;
							_IsOpening = true;
							_ASIOIOContext.post(std::bind(&DemuxerReactor::_OpenDemuxer, this));
							eap_information_printf("SetStatusAddress _Url: %s , _AdapterAddress: %s", _Url, _AdapterAddress);
						}
					}
				}
				// 新的状态是Down
				else {
					/*_IsOpened = false;
					_IsTimeout = false;
					if (_VDemuxer) {
						_VDemuxer->Stop();
						_VDemuxer.reset();
					}*/
				}
			}

			std::string GetNetworkAdapterName()
			{
				return _AdapterName;
			}

			std::string GetNetworkAdapterAddress()
			{
				return _AdapterAddress;
			}

			common::NetWorkChecking::NetworkAdapterType GetNetworkAdapterType()
			{
				return _AdapterType;
			}

			common::NetWorkChecking::NetworkAdapterStatus GetNetworkAdapterStatus()
			{
				return _AdapterStatus;
			}

			bool IsOpened()
			{
				return _IsOpened;
			}

			bool IsTimeout()
			{
				return _IsTimeout;
			}

			AVCodecParameters getAVCodecParameters()
			{
				return (_IsOpened && _VDemuxer) ? _VDemuxer->videoCodecParameters() : AVCodecParameters();
			}

			AVRational getTimeBase()
			{
				return (_IsOpened && _VDemuxer) ? _VDemuxer->videoStreamTimebase() : AVRational();
			}

			AVRational getFrameRate()
			{
				return (_IsOpened && _VDemuxer) ? _VDemuxer->videoFrameRate() : AVRational();
			}

			int getBitRate() {
				return (_IsOpened && _VDemuxer) ? _VDemuxer->bitRate() : 0;
			}

			void StartCache()
			{
				if (_IsOpened && _VDemuxer) {
					_VDemuxer->startCache();
				}
			}

			void StopCache()
			{
				if (_IsOpened && _VDemuxer) {
					_VDemuxer->stopCache();
				}
			}

			void Stop()
			{
				_IsStoped.store(true);
				if (/*_IsOpened &&*/ _VDemuxer) {
					_VDemuxer->close(true);
				}
				
				if (_ASIOIOContextThread.joinable()) {
					_ASIOExecutorWorkGuard.reset();
					_ASIOIOContext.stop();
					_ASIOIOContextThread.join();
				}
				
				eap_information("demuxer reactor stop end!");
			}

		private:
			void _OpenDemuxer();

		private:
			std::string _AdapterName{};
			common::NetWorkChecking::NetworkAdapterType _AdapterType{};
			common::NetWorkChecking::NetworkAdapterStatus _AdapterStatus{};
			std::string _AdapterAddress{};

			DemuxerPtr _VDemuxer{};

			std::string _Url{};
			std::string _id{};
			Demuxer::PacketCallback _PacketCallback{};
			Demuxer::StopCallback _StopCallback{};
			Demuxer::TimeoutCallback _timeout_callback{};
			Demuxer::StreamRecoverCallback _stream_recover_callback{};

			std::thread _ASIOIOContextThread{};
			asio::io_context _ASIOIOContext{};
			asio::executor_work_guard<asio::io_context::executor_type> _ASIOExecutorWorkGuard =
				asio::make_work_guard(_ASIOIOContext);

			std::atomic_bool _IsOpening{};
			std::atomic_bool _IsOpened{};
			std::atomic_bool _IsTimeout{};
			std::atomic_bool _IsStoped{};

			std::chrono::milliseconds _TimeOut{};
			int _FrameRate{};
		};
	}
}

#endif // EAPSDEMUXERREATOR_H