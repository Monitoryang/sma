#ifndef EAPS_CONFIG_H
#define EAPS_CONFIG_H

#include "Config.h"

#include <string>

namespace eap {
	namespace sma {
		bool loadIniConfig();
		// 加载主控通用配置，如果主控有相关字段，以主控的为主
		void loadHangarComConfig();
		extern const std::string kServerName;
		// extern const std::string kServerVersion;
		extern const std::string kServerGitVersion;

		////////////广播名称///////////
		namespace Broadcast {
			extern const std::string kBroadcastAddTask;
			extern const std::string kBroadcastAISwitched;
			extern const std::string kBroadcastARSwitched;
			extern const std::string kBroadcastAddTaskResult;
			extern const std::string kBroadcastRemoveTask;
			extern const std::string kBroadcastTaskStoped;
			extern const std::string kBroadcastOnnxToEngineResult;
			extern const std::string kBroadcastFunctionUpdated;

			// 更新配置文件事件广播,执行loadIniConfig函数加载配置文件成功后会触发该广播
			extern const std::string kBroadcastReloadConfig;
#define BroadcastReloadConfigArgs void

#define ReloadConfigTag ((void *)(0xFF))
#define RELOAD_KEY(funcType, arg, key)                                                                                       \
		do {                                                                                                               \
			decltype(arg) arg##_tmp = eap::configInstance().funcType(key);                                                    \
			if (arg == arg##_tmp) {                                                                                        \
				return;                                                                                                    \
			}                                                                                                              \
			arg = arg##_tmp;                                                                                               \
			ha_information_printf( "reload config:" << key << "=" << arg;                                                                \
		} while (0)

			// 监听某个配置发送变更
#define LISTEN_RELOAD_KEY(arg, key, ...)                                                                           \
		do {                                                                                                               \
			static ::eap::onceToken s_token_listen([]() {                                                              \
				::toolkit::NoticeCenter::Instance().addListener(                                                           \
					ReloadConfigTag, fmm::Broadcast::kBroadcastReloadConfig, [](BroadcastReloadConfigArgs) { __VA_ARGS__; });   \
			});                                                                                                            \
		} while (0)

#define GET_CONFIG(type, funcType, arg, key)                                                                                 \
		static type arg = eap::configInstance().funcType(key);                                                                \
		arg = eap::configInstance().funcType(key);

#define GET_CONFIG_FUNC(type, funcType, arg, key, ...)                                                                       \
		static type arg;                                                                                                   \
		do {                                                                                                               \
			static ::eap::onceToken s_token_set([]() {                                                                 \
				static auto lam = __VA_ARGS__;                                                                             \
				static auto arg##_str = ::toolkit::mINI::Instance()[key];                                                  \
				arg = lam(arg##_str);                                                                                      \
				LISTEN_RELOAD_KEY(arg, key, {                                                                              \
					RELOAD_KEY(funcType, arg##_str, key);                                                                            \
					arg = lam(arg##_str);                                                                                  \
				});                                                                                                        \
			});                                                                                                            \
		} while (0)
		}

		namespace General {
			extern const std::string kEapsServerId;
			extern const std::string kGuid;
			extern const std::string KTimeZoneNum;
			extern const std::string kServerVersion;
			extern const std::string kUploadImageTimeDuration;
			extern const std::string kUploadSnapshot;
			extern const std::string kVideoClipRecordUrl;
			extern const std::string kMediaServerIp;
			extern const std::string kMediaServerSecret;
			extern const std::string kMediaServerUrl;
			extern const std::string kMediaServerRtmpPort;
			extern const std::string kTcpServerPort;
			extern const std::string kFrameInterval;
		}

		namespace SmaRpc{
			extern const std::string kStartPort;
			extern const std::string kEndPort;
		}

		namespace Media {
			extern const std::string kEnableVisualAndInfrared;
			extern const std::string kVisualInputUrl;
			extern const std::string kInfraredInputUrl;
			extern const std::string kVisualMulticastUrl;
			extern const std::string kInfraredMulticastUrl;
			extern const std::string kVisualSmsUrl;
			extern const std::string kInfraredSmsUrl;
			extern const std::string kRecordPath;
			extern const std::string kClipRecordPath;
			extern const std::string kEnableEncode;
			extern const std::string kRecordHDFilePrefix;//机载端默认hd任务的录像后缀
			extern const std::string kRecordSDFilePrefix;//机载端默认sd任务的录像后缀
			extern const std::string kRecordFilePrefix;//普通任务的录像后缀
			extern const std::string kSnapShotPath;		
			extern const std::string kSnapShotNumbers;
			extern const std::string kVisualGuid;
			extern const std::string kInfraredGuid;
			extern const std::string kVisualInputFrameRate;
			extern const std::string kVisualFuncmask; //可见光funcmask
			extern const std::string kInfraredFuncmask;//红外funcmask
			extern const std::string kNetworkName;//机载默认网卡名字
			extern const std::string kAirborne45G;//机载是否走4、5G，默认是0，走的图传
		}

		namespace AI {
			extern const std::string kEnable;
			extern const std::string kOnnxFileFullName;
			extern const std::string kEngineFileFullName;
			extern const std::string kModelWidth;
			extern const std::string kModelHeight;
			extern const std::string kClassNumber;
			extern const std::string kConfThresh;
			extern const std::string kNmsThresh;
			extern const std::string kYoloVersion;
			extern const std::string kTrackBufferLength;
			extern const std::string kTrackSwitch;
			extern const std::string KDangerPhotoServerUrl;
			extern const std::string kAiHeatmapFilePath;
			/*openset*/
			extern const std::string kOpsetEngineFileFullName;
			extern const std::string kTextEncoderFeature;
			extern const std::string kTextEncoderOnnx;
			extern const std::string kVocab;
			extern const std::string kMerges;
			extern const std::string kAddedTokens;
			extern const std::string kSpecialTokensMap;
			extern const std::string kTokenizerConfig;
			extern const std::string kVLtext;
			extern const std::string kOwlVersion;
			extern const std::string kOpensetConfThresh;
			extern const std::string kOpensetNmsThresh;
			/*********/

			extern const std::string kOnnxFileFullNameAux;
			extern const std::string kEngineFileFullNameAux;
			extern const std::string kTextEncoderFeatureAux;
			extern const std::string kModelWidthAux;
			extern const std::string kModelHeightAux;
			extern const std::string kClassNumberAux;
			extern const std::string kConfThreshAux;
			extern const std::string kNmsThreshAux;
			extern const std::string kYoloVersionAux;
			extern const std::string kTrackBufferLengthAux;
			extern const std::string kTrackSwitchAux;
			
			// extern const std::string kFireConfThresh;
			// extern const std::string kSmokeConfThresh;

		}

		namespace MQTT {
            extern const std::string kEnable;
            extern const std::string kBrokerHost;
            extern const std::string kBrokerPort;
            extern const std::string kClientId;
            extern const std::string kUsername;
            extern const std::string kPassword;
            extern const std::string kSubscribeTopic;
            extern const std::string kKeepAlive;
        }

		namespace AR {
			//不需要AR的配置文件，默认启动的流不开启功能，另外开启的流要启动AR功能都要传camera和vector文件；单独针对某个任务开启AR功能时也要传camera和vector文件
			//这样每个任务都可以有自己的camera和vector文件
			//extern const std::string kEnable;
			//extern const std::string kVectorFile;
			//extern const std::string kSetttingsFile;
		}

		namespace Hook {
			extern const std::string kEnable;
			extern const std::string kTimeoutSec;
			extern const std::string kOnAddTask;
			extern const std::string kOnAddTaskResult;
			extern const std::string kOnOnnxToEngineResult;
			extern const std::string kOnFunctionUpdated;
			extern const std::string kOnAiAssistTrackResult;
			extern const std::string kOnSaveSnapShotResult;
			extern const std::string kOnRemoveTask;
			extern const std::string kOnTaskStoped;
			extern const std::string kOnVideoMsg; //反馈当前视频的一些问题

			extern const std::string kOnAISwitched;
			extern const std::string kOnCreateArEngineReslut;
			extern const std::string kGoundEdgeResponse;
			extern const std::string kOnServerKeepalive;
			extern const std::string kOnServerStarted;
			extern const std::string kAdminParams;
			extern const std::string kAliveInterval;
			extern const std::string kRetry;
			extern const std::string kRetryDelay;
			extern const std::string kPlayBackMarkRecord;
		}

		namespace API {
#define API_FIELD "api."
			const std::string kApiDebug = API_FIELD"apiDebug";
			const std::string kSecret = API_FIELD"secret";
		}

		namespace MediaServer {
#define MEDIASERVER_FIELD "mediaserver."
			const std::string KHttpPort = MEDIASERVER_FIELD"httpPort";
			const std::string KRtspPort = MEDIASERVER_FIELD"rtspPort";
			const std::string KRtmpPort = MEDIASERVER_FIELD"rtmpPort";
		}
		
		namespace Vehicle {
			extern const std::string KHdApp;
			extern const std::string KHdStream;
			extern const std::string KHdUrlSrc;
			extern const std::string KSdUrlSrc;
			extern const std::string KRtcPort;
			extern const std::string KHttpPort;
			extern const std::string KHdPushUrl;
			extern const std::string KSdPushUrl;
            extern const std::string KPilotId;
            extern const std::string KARSeat;
            extern const std::string KARSeatGrade;
			extern const std::string hanger_in1_frame_rate;
			extern const std::string KHangerIn1UrlPusher;
			extern const std::string KHangerIn1UrlSrc;
			extern const std::string hanger_in2_frame_rate;
			extern const std::string KHangerIn2UrlPusher;
			extern const std::string KHangerIn2UrlSrc;
			extern const std::string hanger_out_frame_rate;
			extern const std::string KHangerOutUrlPusher;
			extern const std::string KHangerOutUrlSrc;
		}

	}
}
#endif // !EAPS_CONFIG_H