#include "EapsDemuxerReactor.h"
#include "EapsNoticeCenter.h"
namespace eap {
	namespace sma {
		void DemuxerReactor::_OpenDemuxer()
		{
			try {
				if (_VDemuxer) {
					_VDemuxer->close(true);
					_VDemuxer.reset();
				}

				_VDemuxer = DemuxerTradition::createInstance();
				_VDemuxer->setPacketCallback(_PacketCallback);
				_VDemuxer->setStopCallback(_StopCallback);//在udp组播时，stopcallback不往上回调，因为会一直自动去循环打开
				_VDemuxer->setStreamRecoverCallback(_stream_recover_callback);
				_VDemuxer->setTimeoutCallback(_timeout_callback);
				eap_information_printf("---demuxer start open url:%s, localaddr: %s", _Url, _AdapterAddress);
				_VDemuxer->open(_Url, _TimeOut, _AdapterAddress);
			}
			//catch的异常不抛出，会存在网卡活跃，但是没有流的网卡，当avformat_open_input，超过超时时间
			//如果抛出异常，那么有可能会还未搜索到其它后续活跃网卡就退出了
			catch (std::exception& exp) {
				std::string err_msg = std::string(exp.what());
				NoticeCenter::Instance()->getCenter().postNotification(
					new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::DemuxerOpenFailed, err_msg));
				_IsOpened = false;
				_IsOpening = false;
				eap_information_printf("---demuxer open faile, localaddr: %s, error msg: %s", _AdapterAddress, err_msg);
				return;
				//std::rethrow_exception(std::current_exception());
			}

			eap_information_printf("local address: %s opened", _AdapterAddress);

			_IsOpening = false;
			_IsOpened = true;
		}
	}
}