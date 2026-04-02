#include "EapsMetaDataAssemblyJGeoJsonx.h"
#include "EapsConfig.h"
#ifdef ENABLE_AR
#include "ogrsf_frmts.h"
#include "gdal_priv.h"
#endif
#include "Poco/File.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/JSON/Parser.h"
#include "Logger.h"
#include "EapsUtils.h"
#include <fstream>
#include <cmath>
#include <limits>
#include "capi.h"

namespace eap
{
	namespace sma
	{
		// ── 初始化 MQTT 客户端 ──────────────────────────────────────────────────────
		void MetaDataAssemblyGeoJsonx::initMqttClient()
		{
			try 
			{
				GET_CONFIG(bool, getBool, mqtt_enable, eap::sma::MQTT::kEnable);
				if (!mqtt_enable)
					return;

				GET_CONFIG(std::string, getString, broker_host, eap::sma::MQTT::kBrokerHost);
				GET_CONFIG(int, getInt, broker_port, eap::sma::MQTT::kBrokerPort);
				GET_CONFIG(std::string, getString, client_id, eap::sma::MQTT::kClientId);
				GET_CONFIG(std::string, getString, username, eap::sma::MQTT::kUsername);
				GET_CONFIG(std::string, getString, password, eap::sma::MQTT::kPassword);
				GET_CONFIG(std::string, getString, topic, eap::sma::MQTT::kSubscribeTopic);
				GET_CONFIG(int, getInt, keep_alive, eap::sma::MQTT::kKeepAlive);

				_mqtt_client = EapsMqttClient::createInstance();
				_mqtt_client->start(broker_host, broker_port, client_id,
									username, password, topic, keep_alive);
				eap_information_printf("[MQTT] client initialized, broker=%s:%d topic=%s",
									broker_host, broker_port, topic);
			} 
			catch (const std::exception& e) 
			{
				eap_error_printf("[MQTT] initMqttClient failed to read config! Error: %s", std::string(e.what()));
			}
			catch (...)
			{
				eap_error("[MQTT] initMqttClient failed with unknown exception!");
			}
		}

		MetaDataAssemblyGeoJsonxPtr MetaDataAssemblyGeoJsonx::createInstance()
		{
			MetaDataAssemblyGeoJsonxPtr instance = MetaDataAssemblyGeoJsonxPtr(new MetaDataAssemblyGeoJsonx());
			instance->initMqttClient();

			return instance;
		}
		void MetaDataAssemblyGeoJsonx::updateArData(const std::queue<int> ar_valid_point_index, const std::string ar_vector_file)
		{
			_ar_valid_point_index = ar_valid_point_index;
			_geojson_output_directory = exeDir() + "ar_file/";
			if (_ar_vector_file != ar_vector_file && !ar_vector_file.empty())
			{
				eap_warning_printf("_ar_vector_file!=ar_vector_file, new file: %s", ar_vector_file);
				_ar_vector_file = ar_vector_file;
				convertMultiLayerKMLToGeoJSON(_ar_vector_file, _geojson_output_directory);
			}
		}
#if defined(ENABLE_GPU) || defined(ENABLE_AI) || defined(ENABLE_AR)
		void MetaDataAssemblyGeoJsonx::updateArData(const std::vector<std::vector<cv::Point>> &pixel_warning_l1_regions, const std::vector<std::vector<cv::Point>> &pixel_warning_l2_regions)
		{
			_ar_pixel_warning_l1_regions = pixel_warning_l1_regions;
			_ar_pixel_warning_l2_regions = pixel_warning_l2_regions;
		}

		void MetaDataAssemblyGeoJsonx::updateArData(const std::vector<cv::Point> &pixel_points,
													const std::vector<std::vector<cv::Point>> &pixel_lines, ArInfosInternal ar_infos)
		{
			_ar_pixel_points = pixel_points;
			_ar_pixel_lines = pixel_lines;
			_ar_infos = ar_infos;
		}
#endif
		void MetaDataAssemblyGeoJsonx::updateMetaDataStructure(JoFmvMetaDataBasic meta_data_basic)
		{
			_meta_data_basic = meta_data_basic;
			_have_basic = true;
		}

		void MetaDataAssemblyGeoJsonx::updateAiHeapmapData(AiHeatmapInfo ai_heatmap_infos)
		{
			_ai_heatmap_infos = ai_heatmap_infos;
		}

		void MetaDataAssemblyGeoJsonx::updateAiDetectInfo(const AiInfos &ai_infos)
		{
			std::lock_guard<std::mutex> lock(_direct_ai_mutex);
			_direct_ai_infos = ai_infos;
			_has_direct_ai.store(true);
		}

		void MetaDataAssemblyGeoJsonx::updateVideoSize(int width, int height)
		{
			if (width > 0 && height > 0) {
				_video_width = width;
				_video_height = height;
			}
		}

		void MetaDataAssemblyGeoJsonx::updateFrameCurrentTime(int64_t current_time)
		{
			_frame_current_time = current_time;
		}

        void MetaDataAssemblyGeoJsonx::updateVideoParams(int64_t video_duration, int bit_rate, int frame_rate)
        {
			_video_duration = video_duration;
			_bit_rate = bit_rate;
        }

        void MetaDataAssemblyGeoJsonx::updateVideoParams2(int64_t video_duration, int bit_rate, int frame_rate, int width, int heigh)
		{
			_video_duration = video_duration;
			_bit_rate = bit_rate;
			_frame_rate = frame_rate;
			_video_width = width;
			_video_height = heigh;
		}

		std::string MetaDataAssemblyGeoJsonx::getAssemblyString()
		{
			auto ass_str = assembly();
			return ass_str;
		}

		std::string MetaDataAssemblyGeoJsonx::assembly()
		{
			// ── 追加 MQTT 遥测数据 ──────────────────────────────────────────────
			TelemetryData telemetry;
			bool telemetry_valid = _mqtt_client && _mqtt_client->getTelemetryData(telemetry);

			// ── 辅助 lambda：将 AI 检测框中心像素坐标转换为地理坐标 ────────────
			auto aiPixelToGeo = [&](double det_left_top_x, double det_left_top_y,
									double det_width, double det_height,
									double img_width, double img_height) -> std::tuple<double, double, double>
			{
				EulerAngles load{ telemetry.framePan / 180.0 * 3.1415926, telemetry.frameTilt / 180.0 * 3.1415926, telemetry.frameRoll / 180.0 * 3.1415926 };
				// 飞机姿态角，单位弧度
				EulerAngles body{ telemetry.yaw / 180.0 * 3.1415926, telemetry.pitch / 180.0 * 3.1415926, telemetry.roll / 180.0 * 3.1415926 };

				DirectGeoreferenceH hDG = DirectGeoreferenceCreate(load, body);

				// 飞机位置（经/纬/高）
				Coordinate uavLoc{ telemetry.lon, telemetry.lat, telemetry.alt};

				// 目标高度（使用激光测距高度，若无效则退化为载体高度）
				double targetAlt = telemetry.alt - telemetry.rel_alt; // fallback

				// 水平视场角（优先用对角FOV换算，与 test.cpp 保持一致）
				double hfov = abs(telemetry.dfov) > 1e-7 ? 2 * atan(tan(telemetry.dfov * 3.1415926 / 360.0) * img_width / (sqrt(img_width * img_width + img_height * img_height))) * 180.0 / 3.1415926 : telemetry.sensorHorizontalFov;

				// 相机参数（框幅单位 mm，像素数量，水平FOV 度）
				CameraInfo camera{6.4, 4.8, img_width, img_height, hfov};

				// AI 检测框中心像素坐标
				Coordinate imgLoc{
					det_left_top_x + det_width / 2.0,
					det_left_top_y + det_height / 2.0};

				Coordinate geoLoc = DirectGeoreferenceImageToGeo(hDG, uavLoc, targetAlt, camera, imgLoc);
				DirectGeoreferenceDestroy(hDG);

				return {geoLoc.x, geoLoc.y, geoLoc.z};
			};

			if (_have_basic)
			{
				Poco::JSON::Object root;
				Poco::JSON::Object uav_pos_info;
				Poco::JSON::Object uav_status_info;
				Poco::JSON::Object gimbal_payload_info;
				Poco::JSON::Object gimbal_pos_info;
				Poco::JSON::Object gimbal_status_info;
				Poco::JSON::Object image_process_board_info;
				Poco::JSON::Object laser_data_info;
				Poco::JSON::Object pilot_locate_info;

				uav_pos_info.set("TimeStamp", _meta_data_basic.CarrierVehiclePosInfo_p.TimeStamp);
				uav_pos_info.set("TimeZone", _meta_data_basic.CarrierVehiclePosInfo_p.TimeZone);
				uav_pos_info.set("CarrierVehicleHeadingAngle", _meta_data_basic.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle);
				uav_pos_info.set("CarrierVehiclePitchAngle", _meta_data_basic.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle);
				uav_pos_info.set("CarrierVehicleRollAngle", _meta_data_basic.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle);
				uav_pos_info.set("CarrierVehicleLat", _meta_data_basic.CarrierVehiclePosInfo_p.CarrierVehicleLat);
				uav_pos_info.set("CarrierVehicleLon", _meta_data_basic.CarrierVehiclePosInfo_p.CarrierVehicleLon);
				uav_pos_info.set("CarrierVehicleHMSL", _meta_data_basic.CarrierVehiclePosInfo_p.CarrierVehicleHMSL);
				uav_pos_info.set("HMSL", _meta_data_basic.CarrierVehiclePosInfo_p.HMSL);
				uav_pos_info.set("DisFromHome", _meta_data_basic.CarrierVehiclePosInfo_p.DisFromHome);
				uav_pos_info.set("HeadingFromHome", _meta_data_basic.CarrierVehiclePosInfo_p.HeadingFromHome);
				uav_pos_info.set("VGnd", _meta_data_basic.CarrierVehiclePosInfo_p.VGnd);
				uav_pos_info.set("Tas", _meta_data_basic.CarrierVehiclePosInfo_p.Tas);
				uav_pos_info.set("VNorth", _meta_data_basic.CarrierVehiclePosInfo_p.VNorth);
				uav_pos_info.set("VEast", _meta_data_basic.CarrierVehiclePosInfo_p.VEast);
				uav_pos_info.set("VDown", _meta_data_basic.CarrierVehiclePosInfo_p.VDown);
				uav_pos_info.set("FlySeconds", _meta_data_basic.CarrierVehiclePosInfo_p.FlySeconds);

				laser_data_info.set("LaserAngle", _meta_data_basic.LaserDataInfos_p.LaserAngle);
				laser_data_info.set("LaserCurHMSL", _meta_data_basic.LaserDataInfos_p.LaserCurHMSL);
				laser_data_info.set("LaserCurLat", _meta_data_basic.LaserDataInfos_p.LaserCurLat);
				laser_data_info.set("LaserCurLon", _meta_data_basic.LaserDataInfos_p.LaserCurLon);
				laser_data_info.set("LaserCurStatus", _meta_data_basic.LaserDataInfos_p.LaserCurStatus);
				laser_data_info.set("LaserDist", _meta_data_basic.LaserDataInfos_p.LaserDist);
				laser_data_info.set("LaserFreq", _meta_data_basic.LaserDataInfos_p.LaserFreq);
				laser_data_info.set("LaserHMSL", _meta_data_basic.LaserDataInfos_p.LaserHMSL);
				laser_data_info.set("Laserlat", _meta_data_basic.LaserDataInfos_p.Laserlat);
				laser_data_info.set("Laserlon", _meta_data_basic.LaserDataInfos_p.Laserlon);
				laser_data_info.set("LaserMeasStatus", _meta_data_basic.LaserDataInfos_p.LaserMeasStatus);
				laser_data_info.set("LaserMeasVal", _meta_data_basic.LaserDataInfos_p.LaserMeasVal);
				laser_data_info.set("LaserModel", _meta_data_basic.LaserDataInfos_p.LaserModel);
				laser_data_info.set("LaserRefHMSL", _meta_data_basic.LaserDataInfos_p.LaserRefHMSL);
				laser_data_info.set("LaserRefLat", _meta_data_basic.LaserDataInfos_p.LaserRefLat);
				laser_data_info.set("LaserRefLon", _meta_data_basic.LaserDataInfos_p.LaserRefLon);
				laser_data_info.set("LaserRefStatus", _meta_data_basic.LaserDataInfos_p.LaserRefStatus);
				laser_data_info.set("LaserStatus", _meta_data_basic.LaserDataInfos_p.LaserStatus);
				laser_data_info.set("res1", _meta_data_basic.LaserDataInfos_p.res1);
				laser_data_info.set("res2", _meta_data_basic.LaserDataInfos_p.res2);

				uav_status_info.set("CarrierVehicleSN", _meta_data_basic.CarrierVehicleStatusInfo_p.CarrierVehicleSN);
				uav_status_info.set("CarrierVehicleID", _meta_data_basic.CarrierVehicleStatusInfo_p.CarrierVehicleID);
				uav_status_info.set("CarrierVehicleFirmwareVersion", _meta_data_basic.CarrierVehicleStatusInfo_p.CarrierVehicleFirmwareVersion);
				uav_status_info.set("VeclType", _meta_data_basic.CarrierVehicleStatusInfo_p.VeclType);
				uav_status_info.set("Pdop", _meta_data_basic.CarrierVehicleStatusInfo_p.Pdop);
				uav_status_info.set("NumSV", _meta_data_basic.CarrierVehicleStatusInfo_p.NumSV);
				uav_status_info.set("Orienteering", _meta_data_basic.CarrierVehicleStatusInfo_p.Orienteering);
				uav_status_info.set("RPM", _meta_data_basic.CarrierVehicleStatusInfo_p.RPM);
				uav_status_info.set("ThrottleCmd", _meta_data_basic.CarrierVehicleStatusInfo_p.ThrottleCmd);
				uav_status_info.set("MPowerV", _meta_data_basic.CarrierVehicleStatusInfo_p.MPowerV);
				uav_status_info.set("MPowerA", _meta_data_basic.CarrierVehicleStatusInfo_p.MPowerA);
				uav_status_info.set("ElecricQuantity", _meta_data_basic.CarrierVehicleStatusInfo_p.ElecricQuantity);
				uav_status_info.set("ScoutTask", _meta_data_basic.CarrierVehicleStatusInfo_p.ScoutTask);
				uav_status_info.set("APModeStates", _meta_data_basic.CarrierVehicleStatusInfo_p.APModeStates);
				uav_status_info.set("TasCmd", _meta_data_basic.CarrierVehicleStatusInfo_p.TasCmd);
				uav_status_info.set("HeightCmd", _meta_data_basic.CarrierVehicleStatusInfo_p.HeightCmd);
				uav_status_info.set("WypNum", _meta_data_basic.CarrierVehicleStatusInfo_p.WypNum);
				uav_status_info.set("LineInspecMode", _meta_data_basic.CarrierVehicleStatusInfo_p.LineInspecMode);
				uav_status_info.set("AutoPlan", _meta_data_basic.CarrierVehicleStatusInfo_p.AutoPlan);
				uav_status_info.set("RTTMaunalActing", _meta_data_basic.CarrierVehicleStatusInfo_p.RTTMaunalActing);

				gimbal_pos_info.set("VisualViewAngleHorizontal", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal);
				gimbal_pos_info.set("VisualViewAngleVertical", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical);
				gimbal_pos_info.set("InfaredViewAngleHorizontal", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.InfaredViewAngleHorizontal);
				gimbal_pos_info.set("InfaredViewAngleVertical", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.InfaredViewAngleVertical);
				gimbal_pos_info.set("FocalDistance", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance);
				gimbal_pos_info.set("GimbalPan", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan);
				gimbal_pos_info.set("GimbalTilt", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt);
				gimbal_pos_info.set("GimbalRoll", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll);
				gimbal_pos_info.set("FramePan", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan);
				gimbal_pos_info.set("FrameTilt", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt);
				gimbal_pos_info.set("FrameRoll", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll);
				gimbal_pos_info.set("TGTLat", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat);
				gimbal_pos_info.set("TGTLon", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon);
				gimbal_pos_info.set("TGTHMSL", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL);
				gimbal_pos_info.set("TGTVelocity", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTVelocity);
				gimbal_pos_info.set("TGTHeading", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHeading);
				gimbal_pos_info.set("SlantR", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR);
				gimbal_pos_info.set("Elevation", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.Elevation);
				gimbal_pos_info.set("data_back_major", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_major);
				gimbal_pos_info.set("data_back_minor", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_minor);
				gimbal_pos_info.set("data_back_patch", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_patch);
				gimbal_pos_info.set("data_back_build", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_build);
				gimbal_pos_info.set("payload_major", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.payload_major);
				gimbal_pos_info.set("payload_minor", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.payload_minor);
				gimbal_pos_info.set("payload_patch", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.payload_patch);
				gimbal_pos_info.set("payload_build", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.payload_build);
				gimbal_pos_info.set("pilot_major", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_major);
				gimbal_pos_info.set("pilot_minor", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_minor);
				gimbal_pos_info.set("pilot_patch", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_patch);
				gimbal_pos_info.set("pilot_build", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_build);
				gimbal_pos_info.set("sma_major", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.sma_major);
				gimbal_pos_info.set("sma_minor", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.sma_minor);
				gimbal_pos_info.set("sma_patch", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.sma_patch);
				gimbal_pos_info.set("sma_build", _meta_data_basic.GimbalPayloadInfos_p.GimbalPosInfo_p.sma_build);

				gimbal_status_info.set("GMPower", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.GMPower);
				gimbal_status_info.set("ServoCmd", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd);
				gimbal_status_info.set("ServoCmd0", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd0);
				gimbal_status_info.set("ServoCmd1", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd1);
				gimbal_status_info.set("PixelElement", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.PixelElement);
				gimbal_status_info.set("GimbalDeployCmd", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.GimbalDeployCmd);
				gimbal_status_info.set("W_or_B", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.W_or_B);
				gimbal_status_info.set("FovLock", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.FovLock);
				gimbal_status_info.set("GimbalCalibrate", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.GimbalCalibrate);
				gimbal_status_info.set("SeroveInit", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.SeroveInit);

				image_process_board_info.set("Version", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.Version);
				image_process_board_info.set("SearchWidth", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SearchWidth);
				image_process_board_info.set("SearchHeight", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SearchHeight);
				image_process_board_info.set("ServoCrossX", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossX);
				image_process_board_info.set("ServoCrossY", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossY);
				image_process_board_info.set("ServoCrossWidth", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossWidth);
				image_process_board_info.set("ServoCrossHeight", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossHeight);
				image_process_board_info.set("TrackLeftTopX", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopX);
				image_process_board_info.set("TrackLeftTopY", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopY);
				image_process_board_info.set("TrackWidth", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackWidth);
				image_process_board_info.set("TrackHeight", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackHeight);

				Poco::JSON::Array img_coords;
				for (auto &coord : _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood)
				{
					Poco::JSON::Object single_coord;
					single_coord.set("Lat", coord.Lat);
					single_coord.set("Lon", coord.Lon);
					single_coord.set("HMSL", coord.HMSL);

					img_coords.add(single_coord);
				}
				image_process_board_info.set("ImgCoods", img_coords);

				image_process_board_info.set("SDMemory", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SDMemory);
				image_process_board_info.set("SnapNum", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SnapNum);
				image_process_board_info.set("SDFlag", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SDFlag);
				image_process_board_info.set("RecordFlag", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.RecordFlag);
				image_process_board_info.set("TrackFlag", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackFlag);
				image_process_board_info.set("AI_R", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AI_R);
				image_process_board_info.set("CarTrack", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.CarTrack);
				image_process_board_info.set("TrackClass", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackClass);
				image_process_board_info.set("TrackStatus", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackStatus);
				image_process_board_info.set("PIP", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.PIP);
				image_process_board_info.set("ImgStabilize", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgStabilize);
				image_process_board_info.set("ImgDefog", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgDefog);
				image_process_board_info.set("Pintu", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.Pintu);
				image_process_board_info.set("OsdFlag", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.OsdFlag);
				image_process_board_info.set("HDvsSD", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.HDvsSD);
				image_process_board_info.set("VisualFOVHMul", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.VisualFOVHMul);
				image_process_board_info.set("InfaredFOVHMul", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.InfaredFOVHMul);

				Poco::JSON::Object ai_infos;
				uint32_t ai_data_detc_size = _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize;
				uint32_t ai_status = _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiStatus;

				if (ai_status == 1 && ai_data_detc_size > 0)
				{
					/* AI检测结果 */
					static const std::vector<std::string> kClassNames = {
						"浮萍", "水面垃圾", "网笱", "垃圾堆", "建筑", "工业废水",
						"采砂场", "采砂船", "采砂堆", "浮动设施", "渔家乐", "跨河便道",
						"杂物堆积", "藻类", "排污口", "排污管道", "裂缝", "坑槽",
						"拥包", "积水", "标牌异常", "标线淡化", "护栏异常", "伸缩缝异常",
						"绿化侵占", "摅位", "施工设备", "标牌", "伸缩缝", "已修复裂缝",
						"船只", "道路阻断", "滴漏", "火点", "烟雾"
					};
					{
						std::map<int, int> class_count;
						for (size_t i = 0; i < ai_data_detc_size; ++i) {
							int cls = (int)_meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass;
							class_count[cls]++;
						}
						std::string count_str;
						for (const auto& kv : class_count) {
							const std::string& name = (kv.first >= 0 && kv.first < (int)kClassNames.size()) ? kClassNames[kv.first] : std::to_string(kv.first);
							if (!count_str.empty()) count_str += ", ";
							count_str += name + ":" + std::to_string(kv.second);
						}
						eap_information_printf("[OPENSET-STEP6] assembly() generating JSON, AiStatus: 1, DetcSize: %d, classes: [%s]",
							(int)ai_data_detc_size, count_str);
					}
					/* ********* */
					Poco::JSON::Array ai_data_detc_info_array;

					// 取图像尺寸（用于地理定位换算）
					const double img_w = static_cast<double>(
						_meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SearchWidth);
					const double img_h = static_cast<double>(
						_meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SearchHeight);

					for (size_t i = 0; i < ai_data_detc_size; ++i)
					{
						AIDataDetcInfo ai_data_detec_info = _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i];
						Poco::JSON::Object ai_obj;
						ai_obj.set("DetLefttopX", ai_data_detec_info.DetLefttopX);
						ai_obj.set("DetLefttopY", ai_data_detec_info.DetLefttopY);
						ai_obj.set("DetWidth", ai_data_detec_info.DetWidth);
						ai_obj.set("DetHeight", ai_data_detec_info.DetHeight);
						ai_obj.set("DetTGTclass", ai_data_detec_info.DetTGTclass);
						ai_obj.set("TgtConfidence", ai_data_detec_info.TgtConfidence);
						ai_obj.set("TgtSN", ai_data_detec_info.TgtSN);
						ai_obj.set("IFF", ai_data_detec_info.IFF);
						// ── 像素坐标 → 地理坐标 ──────────────────────────────────
						if (img_w > 0 && img_h > 0)
						{
							auto [geo_lon, geo_lat, geo_alt] = aiPixelToGeo(
								static_cast<double>(ai_data_detec_info.DetLefttopX),
								static_cast<double>(ai_data_detec_info.DetLefttopY),
								static_cast<double>(ai_data_detec_info.DetWidth),
								static_cast<double>(ai_data_detec_info.DetHeight),
								img_w, img_h);

							Poco::JSON::Object geo_obj;
							geo_obj.set("lon", geo_lon);
							geo_obj.set("lat", geo_lat);
							geo_obj.set("alt", geo_alt);
							ai_obj.set("GeoLocation", geo_obj);

							// eap_information_printf("[AI-GEO] lon=%.8f lat=%.8f alt=%.2f",geo_lon, geo_lat, geo_alt);
						}
						// ─────────────────────────────────────────────────────────

						ai_data_detc_info_array.add(ai_obj);
					}
					ai_infos.set("AIDataDetcInfoArray", ai_data_detc_info_array);
					ai_infos.set("NewCountClass0", _ai_heatmap_infos.new_count_class0);
					ai_infos.set("NewCountClass1", _ai_heatmap_infos.new_count_class1);
				}
				ai_infos.set("AiStatus", ai_status);
				image_process_board_info.set("AiInfos_p", ai_infos);

				Poco::JSON::Object ar_infos_json;
				Poco::JSON::Array ar_elements_array_json;
				uint32_t ar_elements_num{0};

				ar_infos_json.set("ArStatus", _ar_infos.ArStatus);
				ar_infos_json.set("ArTroubleCode", _ar_infos.ArTroubleCode);
				if (_ar_infos.ArStatus == 0 && _ar_infos.ArElementsNum > 0)
				{
					ar_elements_num += _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArElementsNum;

					for (auto iter : _ar_infos.ArElementsArray)
					{
						Poco::JSON::Object ar_obj;
						ar_obj.set("Type", iter.Type);
						ar_obj.set("DotQuantity", iter.DotQuantity);
						ar_obj.set("X", iter.X);
						ar_obj.set("Y", iter.Y);
						ar_obj.set("lon", iter.lon);
						ar_obj.set("lat", iter.lat);
						ar_obj.set("HMSL", iter.HMSL);
						ar_obj.set("Category", iter.Category);
						ar_obj.set("CurIndex", iter.CurIndex);
						ar_obj.set("NextIndex", iter.NextIndex);
						ar_obj.set("Guid", iter.Guid);
						ar_elements_array_json.add(ar_obj);
					}
				}
#if defined(ENABLE_GPU) || defined(ENABLE_AI) || defined(ENABLE_AR)
				if (!_ar_pixel_lines.empty())
				{
					for (auto &line : _ar_pixel_lines)
					{
						if (!line.empty())
						{
							uint32_t index{};
							for (auto &point : line)
							{
								Poco::JSON::Object ar_obj;
								ar_obj.set("Type", 1);
								ar_obj.set("DotQuantity", line.size());
								ar_obj.set("X", point.x);
								ar_obj.set("Y", point.y);
								ar_obj.set("lon", 0);
								ar_obj.set("lat", 0);
								ar_obj.set("HMSL", 0);
								ar_obj.set("Category", 0); //
								ar_obj.set("CurIndex", index);
								ar_obj.set("NextIndex", index + 1);
								++index;
								// ar_obj["Guid"]
								ar_elements_array_json.add(ar_obj);
								++ar_elements_num;
							}
						}
					}
					ar_infos_json.set("ArStatus", 1);
					ar_infos_json.set("ArTroubleCode", 0);
				}
				if (!_ar_pixel_warning_l1_regions.empty())
				{
					for (auto &line : _ar_pixel_warning_l1_regions)
					{
						if (!line.empty())
						{
							uint32_t index{};
							for (auto &point : line)
							{
								Poco::JSON::Object ar_obj;
								ar_obj.set("Type", 1);
								ar_obj.set("DotQuantity", line.size());
								ar_obj.set("X", point.x);
								ar_obj.set("Y", point.y);
								ar_obj.set("lon", 0);
								ar_obj.set("lat", 0);
								ar_obj.set("HMSL", 0);
								ar_obj.set("Category", 1); // AR
								ar_obj.set("CurIndex", index);
								ar_obj.set("NextIndex", index + 1);
								++index;
								// ar_obj["Guid"]
								ar_elements_array_json.add(ar_obj);
								++ar_elements_num;
							}
						}
					}
				}

				if (!_ar_pixel_warning_l2_regions.empty())
				{
					for (auto &line : _ar_pixel_warning_l2_regions)
					{
						if (!line.empty())
						{
							uint32_t index{};
							for (auto &point : line)
							{
								Poco::JSON::Object ar_obj;
								ar_obj.set("Type", 1);
								ar_obj.set("DotQuantity", line.size());
								ar_obj.set("X", point.x);
								ar_obj.set("Y", point.y);
								ar_obj.set("lon", 0);
								ar_obj.set("lat", 0);
								ar_obj.set("HMSL", 0);
								ar_obj.set("Category", 2);
								ar_obj.set("CurIndex", index);
								ar_obj.set("NextIndex", index + 1);
								++index;
								ar_elements_array_json.add(ar_obj);
								++ar_elements_num;
							}
						}
					}
				}
#endif
				ar_infos_json.set("ArElementsNum", ar_elements_num);
				ar_infos_json.set("ArElementsArray", ar_elements_array_json);
				image_process_board_info.set("ArInfo_p", ar_infos_json);

				/********************************************************************************************************************/

				gimbal_payload_info.set("GimbalPosInfo_p", gimbal_pos_info);
				gimbal_payload_info.set("GimbalStatusInfo_p", gimbal_status_info);
				gimbal_payload_info.set("ImageProcessingBoardInfo_p", image_process_board_info);

				pilot_locate_info.set("iTow", _meta_data_basic.PilotLocateMsg_p.iTow);
				pilot_locate_info.set("SingleLat", _meta_data_basic.PilotLocateMsg_p.SingleLat);
				pilot_locate_info.set("SingleLon", _meta_data_basic.PilotLocateMsg_p.SingleLon);
				pilot_locate_info.set("SingAlt", _meta_data_basic.PilotLocateMsg_p.SingAlt);
				pilot_locate_info.set("ElevationState", _meta_data_basic.PilotLocateMsg_p.ElevationState);
				pilot_locate_info.set("SingleErrorCode", _meta_data_basic.PilotLocateMsg_p.SingleErrorCode);
				pilot_locate_info.set("LaserErrorCode", _meta_data_basic.PilotLocateMsg_p.LaserErrorCode);
				pilot_locate_info.set("LaserLat", _meta_data_basic.PilotLocateMsg_p.LaserLat);
				pilot_locate_info.set("LaserLon", _meta_data_basic.PilotLocateMsg_p.LaserLon);
				pilot_locate_info.set("LaserAlt", _meta_data_basic.PilotLocateMsg_p.LaserAlt);
				pilot_locate_info.set("LaserDis", _meta_data_basic.PilotLocateMsg_p.LaserDis);
				pilot_locate_info.set("LaserDisiTow", _meta_data_basic.PilotLocateMsg_p.LaserDisiTow);
				root.set("CarrierVehiclePosInfo_p", uav_pos_info);
				root.set("CarrierVehicleStatusInfo_p", uav_status_info);
				root.set("GimbalPayloadInfos_p", gimbal_payload_info);
				root.set("PayloadOptions", _meta_data_basic.PayloadOptions);
				root.set("LaserDataInfos_p", laser_data_info);
				root.set("PilotLocateMsg_p", pilot_locate_info);

				root.set("FrameCurrentSeconds", _frame_current_time); // 单位-毫秒
				root.set("VideoDuration", _video_duration);			  // 单位-秒
				root.set("BitRate", _bit_rate);						  // 码率
				root.set("FrameRate", _frame_rate);					  // 帧率

				// convert to GeoJsonx type
				Poco::JSON::Object entity_root;
				entity_root.set("type", "EntityCollection");
				Poco::JSON::Array entity_array = getGeoJsonxPointsArray();
				entity_root.set("entities", entity_array);
				entity_root.set("proprties", root);
				return eap::sma::sanitizeJsonNaN(eap::sma::jsonToString(entity_root));
			}

			// 元数据无法解析时，若有直接路径AI数据则仅输出AI检测结果
			if (_has_direct_ai.load())
			{
				AiInfos direct_ai_copy{};
				{
					std::lock_guard<std::mutex> lock(_direct_ai_mutex);
					direct_ai_copy = _direct_ai_infos;
					_has_direct_ai.store(false);
				}

				if (direct_ai_copy.AiStatus == 1 && direct_ai_copy.AIDataDetcSize > 0)
				{
					/* AI检测结果 */
					static const std::vector<std::string> kClassNames = {
						"浮萍", "水面垃圾", "网笱", "垃圾堆", "建筑", "工业废水",
						"采砂场", "采砂船", "采砂堆", "浮动设施", "渔家乐", "跨河便道",
						"杂物堆积", "藻类", "排污口", "排污管道", "裂缝", "坑槽",
						"拥包", "积水", "标牌异常", "标线淡化", "护栏异常", "伸缩缝异常",
						"绿化侵占", "摅位", "施工设备", "标牌", "伸缩缝", "已修复裂缝",
						"船只", "道路阻断", "滴漏", "火点", "烟雾"
					};
					{
						std::map<int, int> class_count;
						for (size_t i = 0; i < direct_ai_copy.AIDataDetcSize; ++i) {
							int cls = (int)direct_ai_copy.AIDataDetcInfoArray[i].DetTGTclass;
							class_count[cls]++;
						}
						std::string count_str;
						for (const auto& kv : class_count) {
							const std::string& name = (kv.first >= 0 && kv.first < (int)kClassNames.size()) ? kClassNames[kv.first] : std::to_string(kv.first);
							if (!count_str.empty()) count_str += ", ";
							count_str += name + ":" + std::to_string(kv.second);
						}
						eap_information_printf("[AI-ASSEMBLY] direct path (no metadata), AiStatus: 1, DetcSize: %d, classes: [%s]",
							(int)direct_ai_copy.AIDataDetcSize, count_str);
					}
					/****************/
					Poco::JSON::Object root;
					Poco::JSON::Object gimbal_payload_info;
					Poco::JSON::Object image_process_board_info;
					Poco::JSON::Object ai_infos;
					Poco::JSON::Array ai_data_detc_info_array;
					
					for (size_t i = 0; i < direct_ai_copy.AIDataDetcSize; ++i)
					{
						AIDataDetcInfo ai_data_detec_info = direct_ai_copy.AIDataDetcInfoArray[i];
						Poco::JSON::Object ai_obj;
						ai_obj.set("DetLefttopX", ai_data_detec_info.DetLefttopX);
						ai_obj.set("DetLefttopY", ai_data_detec_info.DetLefttopY);
						ai_obj.set("DetWidth", ai_data_detec_info.DetWidth);
						ai_obj.set("DetHeight", ai_data_detec_info.DetHeight);
						ai_obj.set("DetTGTclass", ai_data_detec_info.DetTGTclass);
						ai_obj.set("TgtConfidence", ai_data_detec_info.TgtConfidence);
						ai_obj.set("TgtSN", ai_data_detec_info.TgtSN);
						ai_obj.set("IFF", ai_data_detec_info.IFF);
						// ── 像素坐标 → 地理坐标 ──────────────────────────────────
						auto [geo_lon, geo_lat, geo_alt] = aiPixelToGeo(
							static_cast<double>(ai_data_detec_info.DetLefttopX),
							static_cast<double>(ai_data_detec_info.DetLefttopY),
							static_cast<double>(ai_data_detec_info.DetWidth),
							static_cast<double>(ai_data_detec_info.DetHeight),
							_video_width, _video_height);
					
						Poco::JSON::Object geo_obj;
						geo_obj.set("lon", geo_lon);
						geo_obj.set("lat", geo_lat);
						geo_obj.set("alt", geo_alt);
						ai_obj.set("GeoLocation", geo_obj);
					
						ai_data_detc_info_array.add(ai_obj);
					}

					ai_infos.set("AiStatus", direct_ai_copy.AiStatus);
					ai_infos.set("AIDataDetcInfoArray", ai_data_detc_info_array);
					ai_infos.set("NewCountClass0", _ai_heatmap_infos.new_count_class0);
					ai_infos.set("NewCountClass1", _ai_heatmap_infos.new_count_class1);

					image_process_board_info.set("AiInfos_p", ai_infos);
					gimbal_payload_info.set("ImageProcessingBoardInfo_p", image_process_board_info);
					root.set("GimbalPayloadInfos_p", gimbal_payload_info);

					Poco::JSON::Object entity_root;
					entity_root.set("type", "EntityCollection");
					entity_root.set("proprties", root);

					return eap::sma::sanitizeJsonNaN(eap::sma::jsonToString(entity_root));
				}
			}

			return std::string();
		}

		std::vector<std::tuple<double, double, double>> MetaDataAssemblyGeoJsonx::calcAiGeoLocations(
			const std::vector<joai::Result>& ai_detect_ret, int img_w, int img_h)
		{
			TelemetryData telemetry;
			if (!(_mqtt_client && _mqtt_client->getTelemetryData(telemetry))) {
				eap_warning_printf("%s", "[AI-GEO] calcAiGeoLocations: no telemetry data");
				return {};
			}
			if (telemetry.lon == 0 && telemetry.lat == 0) {
				eap_warning_printf("%s", "[AI-GEO] calcAiGeoLocations: telemetry lon/lat is zero");
				return {};
			}

			EulerAngles load{ telemetry.framePan / 180.0 * 3.1415926, telemetry.frameTilt / 180.0 * 3.1415926, telemetry.frameRoll / 180.0 * 3.1415926 };
			EulerAngles body{ telemetry.yaw / 180.0 * 3.1415926, telemetry.pitch / 180.0 * 3.1415926, telemetry.roll / 180.0 * 3.1415926 };
			DirectGeoreferenceH hDG = DirectGeoreferenceCreate(load, body);
			Coordinate uavLoc{ telemetry.lon, telemetry.lat, telemetry.alt };
			double targetAlt = telemetry.alt - telemetry.rel_alt;
			double hfov = std::abs(telemetry.dfov) > 1e-7
				? 2 * atan(tan(telemetry.dfov * 3.1415926 / 360.0) * img_w / sqrt((double)img_w * img_w + (double)img_h * img_h)) * 180.0 / 3.1415926
				: telemetry.sensorHorizontalFov;
			CameraInfo camera{ 6.4, 4.8, (double)img_w, (double)img_h, hfov };

			std::vector<std::tuple<double, double, double>> result;
			result.reserve(ai_detect_ret.size());
			for (const auto& det : ai_detect_ret) {
				Coordinate imgLoc{
					det.Bounding_box.x + det.Bounding_box.width / 2.0,
					det.Bounding_box.y + det.Bounding_box.height / 2.0 };
				Coordinate geoLoc = DirectGeoreferenceImageToGeo(hDG, uavLoc, targetAlt, camera, imgLoc);
				result.emplace_back(geoLoc.x, geoLoc.y, geoLoc.z);
				eap_information_printf("[AI-GEO] calcAiGeoLocations: lon=%.8f lat=%.8f alt=%.2f", geoLoc.x, geoLoc.y, geoLoc.z);
			}
			DirectGeoreferenceDestroy(hDG);
			return result;
		}



		void MetaDataAssemblyGeoJsonx::convertMultiLayerKMLToGeoJSON(const std::string &ar_vector_file, const std::string &geojson_output_directory)
		{
#ifdef ENABLE_AR
			GDALAllRegister();
			CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
			GDALDataset *poDS = (GDALDataset *)GDALOpenEx(ar_vector_file.c_str(), GDAL_OF_ALL | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);

			if (poDS == NULL)
			{
				eap_information_printf("Could not open KML file: %s", ar_vector_file);
				return;
			}

			deleteGeoJsonFiles(geojson_output_directory);

			int nLayers = poDS->GetLayerCount();

			_vector_layer_num = nLayers;

			eap_warning_printf("_vector_layer_num ==== %d", _vector_layer_num);

			for (int i = 0; i < nLayers; ++i)
			{
				OGRLayer *poLayer = poDS->GetLayer(i);
				if (poLayer == NULL)
				{
					eap_information_printf("Could not get layer %d.", (int)i);
					continue;
				}

				CPLString layerName = std::to_string(i);

				CPLString geoJSONPath = CPLFormFilename(geojson_output_directory.c_str(), layerName.c_str(), "geojson");

				GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
				GDALDataset *poOutDS = poDriver->Create(geoJSONPath, 0, 0, 0, GDT_Unknown, NULL);
				if (poOutDS == NULL)
				{
					eap_information_printf("Could not create GeoJSON file for layer %s.", layerName);
					continue;
				}

				OGRLayer *poOutLayer = poOutDS->CreateLayer(layerName, NULL, wkbUnknown, NULL);
				if (poOutLayer == NULL)
				{
					eap_information_printf("Could not create layer in output dataset for layer %s.", layerName);
					GDALClose(poOutDS);
					continue;
				}

				// 复制字段定义
				for (int j = 0; j < poLayer->GetLayerDefn()->GetFieldCount(); ++j)
				{
					OGRFieldDefn *poFieldDefn = poLayer->GetLayerDefn()->GetFieldDefn(j);
					poOutLayer->CreateField(poFieldDefn);
				}

				// 遍历要素并复制到新的图层
				OGRFeature *poFeature = new OGRFeature(poLayer->GetLayerDefn());
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					OGRFeature *poNewFeature = OGRFeature::CreateFeature(poOutLayer->GetLayerDefn());
					if (poNewFeature->SetFrom(poFeature, 0) != OGRERR_NONE)
					{
						eap_information_printf("Failed to set feature from source for layer %s.", layerName);
						delete poFeature;
						continue;
					}
					if (poOutLayer->CreateFeature(poNewFeature) != OGRERR_NONE)
					{
						eap_information_printf("Failed to create feature in destination for layer %s.", layerName);
					}
					delete poNewFeature;
					delete poFeature;
				}

				GDALClose(poOutDS);
			}

			GDALClose(poDS);
#endif
		}

		void MetaDataAssemblyGeoJsonx::deleteGeoJsonFiles(const std::string &geojson_output_directory)
		{
			Poco::Path directory_path(geojson_output_directory);

			if (!Poco::File(directory_path).exists() || !Poco::File(directory_path).isDirectory())
			{
				eap_warning("geojson_output_directory doesn't exist or isn't direcyory.");
			}

			for (Poco::DirectoryIterator dir_iter(directory_path); dir_iter != Poco::DirectoryIterator(); ++dir_iter)
			{
				Poco::Path filePath(dir_iter->path());
				// 检查文件扩展名是否为.geojson
				if (filePath.getExtension() == "geojson")
				{
					Poco::File file(dir_iter->path());
					if (file.exists() && file.isFile())
					{
						eap_information_printf("delte file: %s", file.path());
						file.remove();
					}
				}
			}
		}

		Poco::JSON::Array MetaDataAssemblyGeoJsonx::getGeoJsonxPointsArray()
		{
			Poco::JSON::Object root;
			Poco::JSON::Array entity_array;
#if defined(ENABLE_GPU) || defined(ENABLE_AI) || defined(ENABLE_AR)
			if (_ar_pixel_points.empty())
			{
				return entity_array;
			}
#endif
			int ar_feature_num_count{0};
			int useful_ar_feature_num_count{0};

			for (int i = 0; i < _vector_layer_num; ++i)
			{
				std::ifstream ifs;
				std::string filepath = _geojson_output_directory;

				char path_sub_buf[BUFSIZ];
				const char *buffer = "%d.geojson";
				snprintf(path_sub_buf, sizeof(path_sub_buf), buffer, i);

				filepath.append(path_sub_buf);

				ifs.open(filepath);
				if (!ifs)
				{
					eap_warning("============================geojson path open fail!");
					break;
				}

				try
				{
					Poco::JSON::Parser parser;
					Poco::Dynamic::Var result = parser.parse(ifs);
					ifs.close();
					root = *(result.extract<Poco::JSON::Object::Ptr>());
				}
				catch (const Poco::Exception &exc)
				{
					eap_warning_printf("error parsing geojson from stream: %s", exc.displayText());
					break;
				}

				Poco::JSON::Array useful_features_array;
				if (root.has("features") && root.isArray("features"))
				{
					auto featuresArray = root.getArray("features");
					for (size_t i = 0; i < featuresArray->size(); ++i)
					{
						Poco::JSON::Object::Ptr feature_ptr = featuresArray->getObject(i);
						// 处理每个feature对象
						if (feature_ptr)
						{
							auto geometry_var = feature_ptr->get("geometry");
							if (geometry_var)
							{
								Poco::JSON::Object::Ptr geometry = geometry_var.extract<Poco::JSON::Object::Ptr>();
								if (geometry->has("type") && (geometry->getValue<std::string>("type") == std::string("Point")))
								{
									if (!_ar_valid_point_index.empty() && (ar_feature_num_count == _ar_valid_point_index.front()))
									{
										auto properties_var = feature_ptr->get("properties");
										if (properties_var)
										{
											Poco::JSON::Object::Ptr properties = properties_var.extract<Poco::JSON::Object::Ptr>();
											Poco::JSON::Array ar_pixel_value;
#if defined(ENABLE_GPU) || defined(ENABLE_AI) || defined(ENABLE_AR)
											ar_pixel_value.add(_ar_pixel_points[useful_ar_feature_num_count].x);
											ar_pixel_value.add(_ar_pixel_points[useful_ar_feature_num_count].y);
#endif
											properties->set("pixel", ar_pixel_value);

											useful_features_array.add(*feature_ptr);

											_ar_valid_point_index.pop();
											++useful_ar_feature_num_count;
										}
									}
									++ar_feature_num_count;
								}
							}
						}
					}
				}

				Poco::JSON::Object useful_root;
				useful_root.set("name", root.getValue<std::string>("name"));
				useful_root.set("type", root.getValue<std::string>("type"));
				useful_root.set("features", useful_features_array);

				Poco::JSON::Object entity_content;
				entity_content.set("graphics", useful_root);
				entity_array.add(entity_content);
			}
			return entity_array;
		}
	}
}