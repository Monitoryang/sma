#include "EapsConfig.h"
#include "version.h"
#include "Logger.h"
#include "OnceToken.h"
#include "Config.h"
#include "EapsUtils.h"
#include "Poco/File.h"
#include <Poco/Path.h>
#include <string>

namespace eap {
	namespace sma {
		std::string getCurrentPath() {
			Poco::Path current_exe = Poco::Path::self();
			return current_exe.parent().toString();
		}

		bool loadIniConfig() {
			try {
				static eap::onceToken token([]() {
					eap::configInstance().setString(General::kEapsServerId, eap::sma::makeRandStr(16));
					std::string server_id{};
					try {
						GET_CONFIG(std::string, getString, my_server_id, General::kEapsServerId);
						server_id = my_server_id;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config kEapsServerId throw exception: %s", e.what());
					}

					eap::configInstance().setString(General::kGuid, "{681DBCD6-9F8D-4482-951A-B8A73E0E3865}");
					eap::configInstance().setString(General::KTimeZoneNum, "8");
					eap::configInstance().setString(General::kUploadImageTimeDuration, "5");
					eap::configInstance().setInt(General::kTcpServerPort, 8444);
					eap::configInstance().setInt(General::kFrameInterval, 1);
					eap::configInstance().setString(General::kUploadSnapshot, "http://47.92.100.249:8700");
					eap::configInstance().setString(General::kVideoClipRecordUrl, "http://47.92.100.249:8700");
					eap::configInstance().setString(General::kMediaServerIp, "47.92.100.249");
					eap::configInstance().setString(General::kMediaServerRtmpPort, "554");
					eap::configInstance().setString(General::kMediaServerUrl, "http://47.92.100.249:8300");
					eap::configInstance().setString(General::kMediaServerSecret, "pkF2zfVu83BKL664pPYQEXUzkGJP9VCM");
					eap::configInstance().setString(General::kServerVersion, RELEASE_VERSION);
					eap::configInstance().setString(SmaRpc::kStartPort, "20000");
					eap::configInstance().setString(SmaRpc::kEndPort, "20099");
					eap::configInstance().setString(Vehicle::KHdApp, "live");
					eap::configInstance().setString(Vehicle::KHdStream, "hd");
					eap::configInstance().setString(Vehicle::KHdUrlSrc, "udp://224.12.34.56:55012");
					eap::configInstance().setString(Vehicle::KSdUrlSrc, "udp://224.12.34.56:55013");
					eap::configInstance().setString(Vehicle::KRtcPort, "8000");
					eap::configInstance().setString(Vehicle::KHttpPort, "8090");
					eap::configInstance().setString(Vehicle::KHdPushUrl, "");
					eap::configInstance().setString(Vehicle::KSdPushUrl, "");
					eap::configInstance().setString(Vehicle::KPilotId, "");
					eap::configInstance().setInt(Vehicle::hanger_in1_frame_rate, 30);
					eap::configInstance().setString(Vehicle::KHangerIn1UrlPusher, "");
					eap::configInstance().setString(Vehicle::KHangerIn1UrlSrc, "");
					eap::configInstance().setInt(Vehicle::hanger_in2_frame_rate, 30);
					eap::configInstance().setString(Vehicle::KHangerIn2UrlPusher, "");
					eap::configInstance().setString(Vehicle::KHangerIn2UrlSrc, "");
					eap::configInstance().setInt(Vehicle::hanger_out_frame_rate, 30);
					eap::configInstance().setString(Vehicle::KHangerOutUrlPusher, "");
					eap::configInstance().setString(Vehicle::KHangerOutUrlSrc, "");
                    eap::configInstance().setBool(Vehicle::KARSeat, false);
                    eap::configInstance().setInt(Vehicle::KARSeatGrade, 2);
					eap::configInstance().setString(Media::kEnableVisualAndInfrared, "1");
					eap::configInstance().setString(Media::kVisualInputUrl, "udp://127.0.0.1:8818");
					eap::configInstance().setString(Media::kInfraredInputUrl, "udp://224.12.34.56:44445");
					eap::configInstance().setString(Media::kVisualMulticastUrl, "udp://224.12.34.56:55012");
					eap::configInstance().setString(Media::kInfraredMulticastUrl, "udp://224.12.34.56:55013");
					eap::configInstance().setString(Media::kVisualSmsUrl, "");
					eap::configInstance().setString(Media::kInfraredSmsUrl, "");
					eap::configInstance().setString(Media::kRecordPath, "/mnt/sdcard/VIDEO");
					eap::configInstance().setString(Media::kClipRecordPath, "/mnt/sdcard/FLIGHT_INBOX");
					eap::configInstance().setBool(Media::kEnableEncode, false);
					eap::configInstance().setString(Media::kRecordHDFilePrefix, "vid-hd-f");
					eap::configInstance().setString(Media::kRecordSDFilePrefix, "vid-sd-f");
					eap::configInstance().setString(Media::kRecordFilePrefix, "vid-f");
					eap::configInstance().setString(Media::kVisualGuid, "{6F984231-2E38-4776-9E41-9E9D0E5072B1}");
					eap::configInstance().setString(Media::kInfraredGuid, "{E6481D50-80CE-4C76-A4DD-53466494F5B7}");
					// eap::configInstance().setString(Media::kSnapShotPath, std::string("/mnt/sdcard/VIDEO/") + "snap");
					eap::configInstance().setString(Media::kSnapShotPath, std::string("/mnt/sdcard/FLIGHT_INBOX/"));
					eap::configInstance().setInt(Media::kSnapShotNumbers, 1);
					eap::configInstance().setInt(Media::kVisualInputFrameRate, 30);
					eap::configInstance().setInt(Media::kVisualFuncmask, 0);
					eap::configInstance().setInt(Media::kInfraredFuncmask, 0);
					eap::configInstance().setString(Media::kNetworkName, "eth0");
					eap::configInstance().setInt(Media::kAirborne45G, 0);
					eap::configInstance().setInt(AI::kEnable, 0);
					eap::configInstance().setString(AI::kOnnxFileFullName, getCurrentPath() + std::string("ai_file/onnx/") + "JoOnboardAi_C15_V1.0.0.onnx");
					eap::configInstance().setString(AI::kEngineFileFullName, getCurrentPath() + std::string("ai_file/engine/") + "JoOnboardAi_C15_V1.0.0.engine");
					eap::configInstance().setString(AI::kTextEncoderFeature, getCurrentPath() + std::string("ai_file/text_encoder/") + "smoke_fire.json");
					// eap::configInstance().setString(AI::kEngineFileFullName, "/usr/local/lib/yolov7tiny_c15_640_384.engine");
					eap::configInstance().setInt(AI::kModelWidth, 640);
					eap::configInstance().setInt(AI::kModelHeight, 384);
					eap::configInstance().setDouble(AI::kClassNumber, 4);
					eap::configInstance().setDouble(AI::kConfThresh, 0.4);
					eap::configInstance().setDouble(AI::kNmsThresh, 0.3);
					eap::configInstance().setString(AI::kYoloVersion, "Yolov7");
					eap::configInstance().setInt(AI::kTrackBufferLength, 30);
					eap::configInstance().setBool(AI::kTrackSwitch, false);
					eap::configInstance().setString(AI::KDangerPhotoServerUrl, "http://124.70.20.168:8700");
					eap::configInstance().setString(AI::kOnnxFileFullNameAux, getCurrentPath() + std::string("ai_file/onnx/") + "JoOnboardAi_C15_V1.0.0.onnx");
					eap::configInstance().setString(AI::kEngineFileFullNameAux, getCurrentPath() + std::string("ai_file/engine/") + "JoOnboardAi_C15_V1.0.0.engine");
					eap::configInstance().setString(AI::kTextEncoderFeatureAux, getCurrentPath() + std::string("ai_file/text_encoder/") + "smoke_fire.json");
					eap::configInstance().setInt(AI::kModelWidthAux, 640);
					eap::configInstance().setInt(AI::kModelHeightAux, 384);
					eap::configInstance().setDouble(AI::kClassNumberAux, 4);
					eap::configInstance().setDouble(AI::kConfThreshAux, 0.4);
					eap::configInstance().setDouble(AI::kNmsThreshAux, 0.3);
					eap::configInstance().setString(AI::kYoloVersionAux, "Yolov7");
					eap::configInstance().setInt(AI::kTrackBufferLengthAux, 30);
					eap::configInstance().setBool(AI::kTrackSwitchAux, true);
					eap::configInstance().setString(AI::kAiHeatmapFilePath, getCurrentPath());
					/*openset AI*/
					eap::configInstance().setString(AI::kOpsetEngineFileFullName, getCurrentPath() + std::string("ai_file/engine/") + "00_00_00_00_00_00_owlv2_image_encoder_base_patch16.engine");
					eap::configInstance().setString(AI::kTextEncoderFeature, getCurrentPath() + std::string("ai_file/text_encoder/") + "00_00_00_00_00_00.json");
					eap::configInstance().setString(AI::kTextEncoderOnnx, getCurrentPath() + std::string("ai_file/text_encoder/") + "owlv2_text_encoder_with_projection.torch.onnx");
					eap::configInstance().setString(AI::kVocab, getCurrentPath() + std::string("ai_file/text_encoder/") + "vocab.json");
					eap::configInstance().setString(AI::kMerges, getCurrentPath() + std::string("ai_file/text_encoder/") + "merges.txt");
					eap::configInstance().setString(AI::kAddedTokens, getCurrentPath() + std::string("ai_file/text_encoder/") + "added_tokens.json");
					eap::configInstance().setString(AI::kSpecialTokensMap, getCurrentPath() + std::string("ai_file/text_encoder/") + "special_tokens_map.json");
					eap::configInstance().setString(AI::kTokenizerConfig, getCurrentPath() + std::string("ai_file/text_encoder/") + "tokenizer_config.json");
					eap::configInstance().setString(AI::kVLtext, "[person,car,boat,fire,smoke]");
					eap::configInstance().setString(AI::kOwlVersion, "google/owlv2-base-patch16-ensemble");
					eap::configInstance().setDouble(AI::kOpensetConfThresh, 0.1);
					eap::configInstance().setDouble(AI::kOpensetNmsThresh, 0.3);
					/***********/


					// eap::configInstance().setInt(AI::kFireConfThresh, 0.4);
					// eap::configInstance().setInt(AI::kSmokeConfThresh, 0.4);

					//eap::configInstance().setInt(AR::kEnable, 0);
					//eap::configInstance().setString(AR::kVectorFile, "default.kml");
					//eap::configInstance().setString(AR::kSetttingsFile, "default.config");
					eap::configInstance().setBool(Hook::kEnable, false);
					eap::configInstance().setBool(MQTT::kEnable, false);
					// MQTT 默认配置
					eap::configInstance().setString(MQTT::kBrokerHost, "223.85.99.73");
					eap::configInstance().setInt(MQTT::kBrokerPort, 1883);
					eap::configInstance().setString(MQTT::kClientId, "eaps_sma_client");
					eap::configInstance().setString(MQTT::kUsername, "jouav");
					eap::configInstance().setString(MQTT::kPassword, "MQ2024@jocloud!@#");
					eap::configInstance().setString(MQTT::kSubscribeTopic, "thing/product/1581F8HGX252U00A025H/osd");
					eap::configInstance().setInt(MQTT::kKeepAlive, 60);

					eap::configInstance().setBool(Hook::kOnAddTask, false);
					eap::configInstance().setInt(Hook::kTimeoutSec, 10);
					eap::configInstance().setString(Hook::kAdminParams, "secret=035c73f7-bb6b-4889-a715-d9eb2d1925dd");
					eap::configInstance().setInt(Hook::kAliveInterval, 30);
					eap::configInstance().setInt(Hook::kRetry, 1);
					eap::configInstance().setInt(Hook::kRetryDelay, 3);
					eap::configInstance().setString(Hook::kOnServerStarted, "http://127.0.0.1/index/api/on_server_started");
					eap::configInstance().setString(Hook::kPlayBackMarkRecord, "");
					eap::configInstance().setString(Hook::kGoundEdgeResponse, "");
					eap::configInstance().setString(Hook::kOnServerKeepalive, "http://127.0.0.1/index/api/on_server_keepalive");
					eap::configInstance().setString(Hook::kOnAddTask, "");
					eap::configInstance().setString(Hook::kOnVideoMsg, "");
					eap::configInstance().setString(Hook::kOnAddTaskResult, "");
					eap::configInstance().setString(Hook::kOnRemoveTask, "");
					eap::configInstance().setString(Hook::kOnOnnxToEngineResult, "");
					eap::configInstance().setString(Hook::kOnFunctionUpdated, "");
					eap::configInstance().setString(Hook::kOnAiAssistTrackResult, "");
					eap::configInstance().setString(Hook::kOnSaveSnapShotResult, "");
					eap::configInstance().setString(Hook::kOnTaskStoped, "");
					eap::configInstance().setString(Hook::kOnAISwitched, "");
					eap::configInstance().setString(Hook::kOnCreateArEngineReslut, "");
					eap::configInstance().setString(API::kApiDebug, "1");
					eap::configInstance().setString(API::kSecret, "035c73f7-bb6b-4889-a715-d9eb2d1925ee");
					std::string sms_http_port = sma::getEnv("SMS_HTTP_PORT");
					std::string sms_rtsp_port = sma::getEnv("SMS_RTSP_PORT");
					std::string sms_rtmp_port = sma::getEnv("SMS_RTMP_PORT");
					eap::configInstance().setString(MediaServer::KHttpPort, sms_http_port.empty() ? "80" : sms_http_port);
					eap::configInstance().setString(MediaServer::KRtspPort, sms_rtsp_port.empty() ? "554" : sms_rtsp_port);
					eap::configInstance().setString(MediaServer::KRtmpPort, sms_rtmp_port.empty() ? "1935" : sms_rtmp_port);
				});
				if (!eap::configInstance().has(General::kUploadSnapshot)) {
					eap::configInstance().setString(General::kUploadSnapshot, "http://47.92.100.249:8700");
					eap::configInstance().setString(General::kVideoClipRecordUrl, "http://47.92.100.249:8700");
					eap::configInstance().setString(General::kMediaServerIp, "47.92.100.249");
					eap::configInstance().setString(General::kMediaServerRtmpPort, "554");
					eap::configInstance().setString(General::kMediaServerUrl, "http://47.92.100.249:8300");
					eap::configInstance().setString(General::kMediaServerSecret, "pkF2zfVu83BKL664pPYQEXUzkGJP9VCM");
					eap::saveConfig();
				}
				if(!eap::configInstance().has(Media::kEnableEncode)){
					eap::configInstance().setBool(Media::kEnableEncode, false);
					eap::saveConfig();
				}

				if (!eap::configInstance().has(Media::kClipRecordPath)) {
					eap::configInstance().setString(Media::kClipRecordPath, "/mnt/sdcard/FLIGHT_INBOX");
					eap::configInstance().setString(Media::kSnapShotPath, std::string("/mnt/sdcard/FLIGHT_INBOX/"));
					eap::saveConfig();
				}
				if (!eap::configInstance().has(Media::kAirborne45G)) {
					eap::configInstance().setInt(Media::kAirborne45G, 0);
					eap::saveConfig();
				}
					
				return true;
			}
			catch (std::exception &) {
				eap::saveConfig();
				return false;
			}
		}

		void loadHangarComConfig()
		{
#ifdef ENABLE_GROUND
			try {
				auto src = "/etc/hangarInfo/hangarComConfig.json";
				Poco::Path source_path(src);
				Poco::File source_file(source_path);
				if (!source_file.exists()) {
					eap_error_printf("Source file does not exist: %s", src);
					return;
				}

				int change_cnt{ 0 };
				Poco::Util::JSONConfiguration hangar_config(src);
				std::string pilotCameraUrl = hangar_config.has("pilotCameraUrl") ? hangar_config.getString("pilotCameraUrl") : "";
				if (!pilotCameraUrl.empty() && pilotCameraUrl != configInstance().getString(Vehicle::KHdUrlSrc)) {
					configInstance().setString(Vehicle::KHdUrlSrc, pilotCameraUrl);
					change_cnt++;
				}
				std::string pilotCameraPublishUrl = hangar_config.has("pilotCameraPublishUrl") ? hangar_config.getString("pilotCameraPublishUrl") : "";
				if (!pilotCameraPublishUrl.empty() && pilotCameraPublishUrl != configInstance().getString(Vehicle::KHdPushUrl)) {
					configInstance().setString(Vehicle::KHdPushUrl, pilotCameraPublishUrl);
					change_cnt++;
				}
				std::string cameraIn1Url = hangar_config.has("cameraIn1Url") ? hangar_config.getString("cameraIn1Url") : "";
				if (!cameraIn1Url.empty() && cameraIn1Url != configInstance().getString(Vehicle::KHangerIn1UrlSrc)) {
					configInstance().setString(Vehicle::KHangerIn1UrlSrc, cameraIn1Url);
					change_cnt++;
				}
				std::string cameraIn1PublishUrl = hangar_config.has("cameraIn1PublishUrl") ? hangar_config.getString("cameraIn1PublishUrl") : "";
				if (!cameraIn1PublishUrl.empty() && cameraIn1PublishUrl != configInstance().getString(Vehicle::KHangerIn1UrlPusher)) {
					configInstance().setString(Vehicle::KHangerIn1UrlPusher, cameraIn1PublishUrl);
					change_cnt++;
				}
				std::string cameraIn2Url = hangar_config.has("cameraIn2Url") ? hangar_config.getString("cameraIn2Url") : "";
				if (!cameraIn2Url.empty() && cameraIn2Url != configInstance().getString(Vehicle::KHangerIn2UrlSrc)) {
					configInstance().setString(Vehicle::KHangerIn2UrlSrc, cameraIn2Url);
					change_cnt++;
				}
				std::string cameraIn2PublishUrl = hangar_config.has("cameraIn2PublishUrl") ? hangar_config.getString("cameraIn2PublishUrl") : "";
				if (!cameraIn2PublishUrl.empty() && cameraIn2PublishUrl != configInstance().getString(Vehicle::KHangerIn2UrlPusher)) {
					configInstance().setString(Vehicle::KHangerIn2UrlPusher, cameraIn2PublishUrl);
					change_cnt++;
				}
				std::string cameraOut1Url = hangar_config.has("cameraOut1Url") ? hangar_config.getString("cameraOut1Url") : "";
				if (!cameraOut1Url.empty() && cameraOut1Url != configInstance().getString(Vehicle::KHangerOutUrlSrc)) {
					configInstance().setString(Vehicle::KHangerOutUrlSrc, cameraOut1Url);
					change_cnt++;
				}
				std::string cameraOut1PublishUrl = hangar_config.has("cameraOut1PublishUrl") ? hangar_config.getString("cameraOut1PublishUrl") : "";
				if (!cameraOut1PublishUrl.empty() && cameraOut1PublishUrl != configInstance().getString(Vehicle::KHangerOutUrlPusher)) {
					configInstance().setString(Vehicle::KHangerOutUrlPusher, cameraOut1PublishUrl);
					change_cnt++;
				}
				if (change_cnt)
					eap::saveConfig();
			} catch (const std::exception& exp) {
				eap_error(std::string(exp.what()));
			}
#endif
		}

		const std::string kServerName = "JoEAPSma";
		// const std::string kServerVersion = RELEASE_VERSION;
		const std::string kServerGitVersion = std::string(BRANCH_NAME) + " " + COMMIT_HASH + "/" + COMMIT_TIME + " " + BUILD_TIME;

		////////////广播名称///////////
		namespace Broadcast {
			const std::string kBroadcastAddTask = "kBroadcastAddTask";
			const std::string kBroadcastAISwitched = "kBroadcastAISwitched";
			const std::string kBroadcastARSwitched = "kBroadcastARSwitched";
			const std::string kBroadcastAddTaskResult = "kBroadcastAddTaskResult";
			const std::string kBroadcastRemoveTask = "kBroadcastRemoveTask";
			const std::string kBroadcastTaskStoped = "kBroadcastTaskStoped";
			const std::string kBroadcastReloadConfig = "kBroadcastReloadConfig";
			const std::string kBroadcastOnnxToEngineResult = "kBroadcastOnnxToEngineResult";
			const std::string kBroadcastFunctionUpdated = "kBroadcastFunctionUpdated";
		} // namespace Broadcast

		namespace General {
#define GENERAL_FIELD "general."
			const std::string kEapsServerId = GENERAL_FIELD "EapsServerId";
			const std::string kGuid = GENERAL_FIELD "guid";
			const std::string KTimeZoneNum = GENERAL_FIELD "TimeZoneNum";
			const std::string kServerVersion = GENERAL_FIELD "server_version";
			const std::string kUploadImageTimeDuration = GENERAL_FIELD "UploadImageTimeDuration";
			const std::string kUploadSnapshot = GENERAL_FIELD "UploadSnapshot";
			const std::string kVideoClipRecordUrl = GENERAL_FIELD "VideoClipRecordUrl";
			const std::string kMediaServerIp = GENERAL_FIELD "MediaServerIp";
			const std::string kMediaServerSecret = GENERAL_FIELD "MediaServerSecret";
			const std::string kMediaServerUrl = GENERAL_FIELD "MediaServerUrl";
			const std::string kMediaServerRtmpPort = GENERAL_FIELD "MediaServerRtmpPort";
			const std::string kTcpServerPort = GENERAL_FIELD "tcp_server_port";
			const std::string kFrameInterval = GENERAL_FIELD "frame_interval";
		}

		namespace SmaRpc {
#define RPC_INFO "sma_rpc."
			const std::string kStartPort = RPC_INFO "start_port";
			const std::string kEndPort = RPC_INFO "end_port";
		}

		namespace Vehicle {
#define VEHICLE_FIELD "vehicle."
			const std::string KHdApp = VEHICLE_FIELD "hd_app";
			const std::string KHdStream = VEHICLE_FIELD "hd_stream";
			const std::string KHdUrlSrc = VEHICLE_FIELD "hd_url_src";
			const std::string KSdUrlSrc = VEHICLE_FIELD "sd_url_src";
			const std::string KRtcPort = VEHICLE_FIELD "rtc_port";
			const std::string KHttpPort = VEHICLE_FIELD "http_port";
			const std::string KHdPushUrl = VEHICLE_FIELD "hd_url_pusher";
			const std::string KSdPushUrl = VEHICLE_FIELD "sd_url_pusher";
            const std::string KPilotId = VEHICLE_FIELD "pilot_id";
            const std::string KARSeat = VEHICLE_FIELD"six_axis_ar_seat";
			const std::string KARSeatGrade = VEHICLE_FIELD"six_axis_ar_seat_grade";
			const std::string hanger_in1_frame_rate = VEHICLE_FIELD"hanger_in1_frame_rate";
			const std::string KHangerIn1UrlPusher = VEHICLE_FIELD"hanger_in1_url_pusher";
			const std::string KHangerIn1UrlSrc = VEHICLE_FIELD"hanger_in1_url_src";
			const std::string hanger_in2_frame_rate = VEHICLE_FIELD"hanger_in2_frame_rate";
			const std::string KHangerIn2UrlPusher = VEHICLE_FIELD"hanger_in2_url_pusher";
			const std::string KHangerIn2UrlSrc = VEHICLE_FIELD"hanger_in2_url_src";
			const std::string hanger_out_frame_rate = VEHICLE_FIELD"hanger_out_frame_rate";
			const std::string KHangerOutUrlPusher = VEHICLE_FIELD"hanger_out_url_pusher";
			const std::string KHangerOutUrlSrc = VEHICLE_FIELD"hanger_out_url_src";
		}

		namespace Media {
#define MEDIA_FIELD "media."
			const std::string kEnableVisualAndInfrared = MEDIA_FIELD "enable_visual_and_infrared";
			const std::string kVisualInputUrl = MEDIA_FIELD "visual_input_url";
			const std::string kInfraredInputUrl = MEDIA_FIELD "infrared_input_url";
			const std::string kVisualMulticastUrl = MEDIA_FIELD "visual_multicast_url";
			const std::string kInfraredMulticastUrl = MEDIA_FIELD "infrared_multicast_url";
			const std::string kVisualSmsUrl = MEDIA_FIELD "visual_sms_url";
			const std::string kInfraredSmsUrl = MEDIA_FIELD "infrared_sms_url";
			//const std::string kEnableTranscode = MEDIA_FIELD "enable_transcode";
			//const std::string kVisualOutFramerate = MEDIA_FIELD "visual_out_framerate";
			//const std::string kInfraredOutFramerate = MEDIA_FIELD "infrared_out_framerate";
			//const std::string kVisualOutBitrate = MEDIA_FIELD "visual_out_bitrate";
			//const std::string kInfraredOutBitrate = MEDIA_FIELD "infrared_out_bitrate";
			const std::string kRecordPath = MEDIA_FIELD "record_path";
			const std::string kClipRecordPath = MEDIA_FIELD "clip_record_path";
			const std::string kEnableEncode=MEDIA_FIELD "enable_encode";
			const std::string kRecordHDFilePrefix = MEDIA_FIELD "record_hd_file_prefix";
			const std::string kRecordSDFilePrefix = MEDIA_FIELD "record_sd_file_prefix";
			const std::string kRecordFilePrefix = MEDIA_FIELD "record_file_prefix";
			const std::string kSnapShotPath = MEDIA_FIELD "snap_shot_path";
			const std::string kSnapShotNumbers = MEDIA_FIELD "snap_shot_numbers";
			const std::string kVisualInputFrameRate = MEDIA_FIELD "visual_input_frame_rate";
			const std::string kVisualGuid = MEDIA_FIELD "visual_guid";
			const std::string kInfraredGuid = MEDIA_FIELD "infrared_guid";
			const std::string kVisualFuncmask = MEDIA_FIELD "visual_funcmask";
			const std::string kInfraredFuncmask = MEDIA_FIELD "infrared_funcmask";
			const std::string kNetworkName = MEDIA_FIELD "network_name"; //机载网卡名字，默认eth0
			const std::string kAirborne45G = MEDIA_FIELD "airborne_45G";//机载是否走4、5G，默认是0，走的图传
		}

		namespace AI {
#define AI_FIELD "ai."
			const std::string kEnable = AI_FIELD "enable";
			const std::string kOnnxFileFullName = AI_FIELD "onnx_file_full_name";
			const std::string kEngineFileFullName = AI_FIELD "engine_file_full_name";
			const std::string kModelWidth = AI_FIELD "width";
			const std::string kModelHeight = AI_FIELD "height";
			const std::string kClassNumber = AI_FIELD "class_num";
			const std::string kConfThresh = AI_FIELD "conf_thresh";
			const std::string kNmsThresh = AI_FIELD "nms_thresh";
			const std::string kYoloVersion = AI_FIELD "YoloVersion";
			const std::string kTrackBufferLength = AI_FIELD "track_buff_len";
			const std::string kTrackSwitch = AI_FIELD "track_switch";
			const std::string KDangerPhotoServerUrl = AI_FIELD "danger_photo_server_url";
			const std::string kAiHeatmapFilePath = AI_FIELD "ai_heatmap_file_path";

			/*openset*/
			const std::string kOpsetEngineFileFullName = AI_FIELD "opset_engine_file_full_name";
			const std::string kTextEncoderFeature = AI_FIELD "text_encoder_feature";
			const std::string kTextEncoderOnnx = AI_FIELD "text_encoder_onnx";
			const std::string kVocab = AI_FIELD "vocab";
			const std::string kMerges = AI_FIELD "merges";
			const std::string kAddedTokens = AI_FIELD "added_tokens";
			const std::string kSpecialTokensMap = AI_FIELD "special_tokens_map";
			const std::string kTokenizerConfig = AI_FIELD "tokenizer_config";
			const std::string kVLtext = AI_FIELD "vl_text";
			const std::string kOwlVersion = AI_FIELD "OwlVersion";
			const std::string kOpensetConfThresh = AI_FIELD "openset_conf_thresh";
			const std::string kOpensetNmsThresh = AI_FIELD "openset_nms_thresh";
			/*********/

			extern const std::string kOnnxFileFullNameAux = AI_FIELD "aux_onnx_file_full_name";
			extern const std::string kEngineFileFullNameAux = AI_FIELD "aux_engine_file_full_name";
			extern const std::string kModelWidthAux = AI_FIELD "aux_width";
			extern const std::string kModelHeightAux = AI_FIELD "aux_height";
			extern const std::string kClassNumberAux = AI_FIELD "aux_class_num";
			extern const std::string kConfThreshAux = AI_FIELD "aux_conf_thresh";
			extern const std::string kNmsThreshAux = AI_FIELD "aux_nms_thresh";
			extern const std::string kYoloVersionAux = AI_FIELD "aux_YoloVersion";
			extern const std::string kTextEncoderFeatureAux = AI_FIELD "aux_text_encoder_feature";
			extern const std::string kTrackBufferLengthAux = AI_FIELD "aux_track_buff_len";
			extern const std::string kTrackSwitchAux = AI_FIELD "aux_track_switch";

			// extern const std::string kFireConfThresh = AI_FIELD "fire_conf_thresh";
			// extern const std::string kSmokeConfThresh = AI_FIELD "smoke_conf_thresh";
		}

		 namespace MQTT {
#define MQTT_FIELD "mqtt."
            const std::string kEnable        = MQTT_FIELD "enable";
            const std::string kBrokerHost    = MQTT_FIELD "broker_host";
            const std::string kBrokerPort    = MQTT_FIELD "broker_port";
            const std::string kClientId      = MQTT_FIELD "client_id";
            const std::string kUsername      = MQTT_FIELD "username";
            const std::string kPassword      = MQTT_FIELD "password";
            const std::string kSubscribeTopic = MQTT_FIELD "subscribe_topic";
            const std::string kKeepAlive     = MQTT_FIELD "keep_alive";
        }

		namespace AR {
//#define AR_FIELD "ar."
//			const std::string kEnable = AR_FIELD "enable";
//			const std::string kVectorFile = AR_FIELD "vector_file";
//			const std::string kSetttingsFile = AR_FIELD "settints_file";

		}

		namespace Hook {
#define HOOK_FIELD "hook."

			const std::string kEnable = HOOK_FIELD "enable";
			const std::string kTimeoutSec = HOOK_FIELD "timeoutSec";
			const std::string kOnAddTask = HOOK_FIELD "on_add_task";
			const std::string kOnAddTaskResult = HOOK_FIELD "on_add_task_result";
			const std::string kOnRemoveTask = HOOK_FIELD "on_remove_task";
			const std::string kOnTaskStoped = HOOK_FIELD "on_task_stoped";
			const std::string kOnOnnxToEngineResult = HOOK_FIELD "onnx_to_engine_result";
			const std::string kOnFunctionUpdated = HOOK_FIELD "on_function_updated";
			const std::string kOnAiAssistTrackResult = HOOK_FIELD "ai_assist_track_result";
			const std::string kOnSaveSnapShotResult = HOOK_FIELD "on_save_snapshot_result";
			const std::string kOnAISwitched = HOOK_FIELD "on_ai_switched";
			const std::string kOnCreateArEngineReslut = HOOK_FIELD "on_create_ar_engine_result";
			const std::string kOnServerKeepalive = HOOK_FIELD "on_server_keepalive";
			const std::string kOnServerStarted = HOOK_FIELD "on_server_started";
			const std::string kAdminParams = HOOK_FIELD "admin_params";
			const std::string kAliveInterval = HOOK_FIELD "alive_interval";
			const std::string kRetry = HOOK_FIELD "retry";
			const std::string kRetryDelay = HOOK_FIELD "retry_delay";
			const std::string kPlayBackMarkRecord = HOOK_FIELD "playback_mark_record";
			const std::string kGoundEdgeResponse = HOOK_FIELD "ground_edge_response";
			const std::string kOnVideoMsg = HOOK_FIELD "on_video_msg";
		} // namespace Hook
	}
}
