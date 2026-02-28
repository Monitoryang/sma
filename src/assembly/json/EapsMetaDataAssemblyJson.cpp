#include "EapsMetaDataAssemblyJson.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "EapsUtils.h"

namespace eap {
	namespace sma {
		MetaDataAssemblyJsonPtr MetaDataAssemblyJson::createInstance()
		{
			return MetaDataAssemblyJsonPtr(new MetaDataAssemblyJson());
		}

		void MetaDataAssemblyJson::updateMetaDataStructure(JoFmvMetaDataBasic meta_data_basic)
		{
			_meta_data_basic = meta_data_basic;
			_have_basic = true;
		}
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
		void MetaDataAssemblyJson::updateArData(const std::vector<cv::Point>& pixel_points,
			const std::vector<std::vector<cv::Point>>& pixel_lines)
		{
			_ar_pixel_points = pixel_points;
			_ar_pixel_lines = pixel_lines;
		}
#endif

		void MetaDataAssemblyJson::updateFrameCurrentTime(int64_t current_time)
		{
			_frame_current_time = current_time;
		}

		std::string MetaDataAssemblyJson::getAssemblyString()
		{
			auto ass_str = assembly();
			return ass_str;
		}

		std::string MetaDataAssemblyJson::assembly()
		{
			if (_have_basic) {
				Poco::JSON::Object root;
				Poco::JSON::Object uav_pos_info;
				Poco::JSON::Object uav_status_info;
				Poco::JSON::Object gimbal_payload_info;
				Poco::JSON::Object gimbal_pos_info;
				Poco::JSON::Object gimbal_status_info;
				Poco::JSON::Object image_process_board_info;

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

				gimbal_status_info.set("GMPower", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.GMPower);
				gimbal_status_info.set("ServoCmd", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd);
				gimbal_status_info.set("ServoCmd0", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd0);
				gimbal_status_info.set("ServoCmd1", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd1);
				gimbal_status_info.set("PixelElement", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.PixelElement);
				gimbal_status_info.set("GimbalDeployCmd", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.GimbalDeployCmd);
				gimbal_status_info.set("W_or_B", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.W_or_B);
				gimbal_status_info.set("FovLock", _meta_data_basic.GimbalPayloadInfos_p.GimbalStatusInfo_p.FovLock);

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
				for (auto& coord : _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood) {
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

				uint32_t ai_data_detc_size = _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize;
				if (ai_data_detc_size > 0) {
					Poco::JSON::Object ai_infos;
					Poco::JSON::Array ai_data_detc_info_array;
					for (size_t i = 0; i < ai_data_detc_size; ++i) {
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

						ai_data_detc_info_array.add(ai_obj);
					}
					ai_infos.set("AiStatus", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiStatus);
					ai_infos.set("AIDataDetcInfoArray", ai_data_detc_info_array);

					image_process_board_info.set("AiInfos_p", ai_infos);
				}

				Poco::JSON::Object ar_infos_json;
				Poco::JSON::Array ar_elements_array_json;				

				ar_infos_json.set("ArStatus", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArStatus);
				ar_infos_json.set("ArTroubleCode", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArTroubleCode);

				uint32_t ar_elements_num = _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArElementsNum;
				if (ar_elements_num > 0) {
					for (uint32_t i = 0; i < ar_elements_num; i++) {
						Poco::JSON::Object ar_obj;
						ar_obj.set("Type", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].Type);
						ar_obj.set("DotQuantity", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].DotQuantity);
						ar_obj.set("X", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].X);
						ar_obj.set("Y", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].Y);
						ar_obj.set("lon", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].lon);
						ar_obj.set("lat", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].lat);
						ar_obj.set("HMSL", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].HMSL);
						ar_obj.set("Category", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].Category);
						ar_obj.set("CurIndex", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].CurIndex);
						ar_obj.set("NextIndex", _meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[i].NextIndex);
						ar_elements_array_json.add(ar_obj);
					}
				}

				/********************************************************************************************************************/
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				if (!_ar_pixel_points.empty()) {
					for (auto& point : _ar_pixel_points) {
						Poco::JSON::Object ar_obj;
						ar_obj.set("Type", 0);
						ar_obj.set("DotQuantity", 1);
						ar_obj.set("X", point.x);
						ar_obj.set("Y", point.y);
						ar_obj.set("lon", 0);
						ar_obj.set("lat", 0);
						ar_obj.set("HMSL", 0);
						ar_obj.set("Category", 0);
						ar_obj.set("CurIndex", 0);
						ar_obj.set("NextIndex", 0);
						ar_elements_array_json.add(ar_obj);
					}
					ar_infos_json.set("ArStatus", 0);
					ar_infos_json.set("ArTroubleCode", 0);
					ar_elements_num += _ar_pixel_points.size();
				}

				if (!_ar_pixel_lines.empty()) {
					for (auto& line : _ar_pixel_lines) {
						if (!line.empty()) {
							uint32_t index{};
							for (auto& point : line) {
								Poco::JSON::Object ar_obj;
								ar_obj.set("Type", 1);
								ar_obj.set("DotQuantity", line.size());
								ar_obj.set("X", point.x);
								ar_obj.set("Y", point.y);
								ar_obj.set("lon", 0);
								ar_obj.set("lat", 0);
								ar_obj.set("HMSL", 0);
								ar_obj.set("Category", 0);//AR�����Ŀǰ�޷�ȷ����Ԫ������
								ar_obj.set("CurIndex", index);
								ar_obj.set("NextIndex", index + 1);
								++index;
								ar_elements_array_json.add(ar_obj);
								++ar_elements_num;
							}
						}
					}
					ar_infos_json.set("ArStatus", 0);
					ar_infos_json.set("ArTroubleCode", 0);
				}
#endif
				ar_infos_json.set("ArElementsNum", ar_elements_num);
				ar_infos_json.set("ArElementsArray", ar_elements_array_json);
				image_process_board_info.set("ArInfo_p", ar_infos_json);

				//Poco::JSON::Object ar_infos;
				//if (!_ar_pixel_points.empty()) {
				//	Poco::JSON::Object ar_points;

				//	for (auto& point : _ar_pixel_points) {
				//		Poco::JSON::Object ar_point;
				//		ar_point["x", point.x;
				//		ar_point["y", point.y;

				//		ar_points.append(ar_point);
				//	}

				//	ar_infos["ArPoints_p", ar_points;
				//}
				//if (!_ar_pixel_lines.empty()) {
				//	Poco::JSON::Object ar_lines;

				//	for (auto& line : _ar_pixel_lines) {
				//		if (!line.empty()) {
				//			Poco::JSON::Object ar_line;
				//			for (auto& point : line) {
				//				Poco::JSON::Object ar_point;
				//				ar_point["x", point.x;
				//				ar_point["y", point.y;

				//				ar_line.append(ar_point);
				//			}
				//			ar_lines.append(ar_line);
				//		}
				//	}

				//	ar_infos["ArLines_p", ar_lines;
				//}

				//image_process_board_info.set("ArInfo_p", ar_infos;

				/********************************************************************************************************************/

				gimbal_payload_info.set("GimbalPosInfo_p", gimbal_pos_info);
				gimbal_payload_info.set("GimbalStatusInfo_p", gimbal_status_info);
				gimbal_payload_info.set("ImageProcessingBoardInfo_p", image_process_board_info);

				root.set("CarrierVehiclePosInfo_p", uav_pos_info);
				root.set("CarrierVehicleStatusInfo_p", uav_status_info);
				root.set("GimbalPayloadInfos_p", gimbal_payload_info);
				root.set("PayloadOptions", _meta_data_basic.PayloadOptions);

				root.set("FrameCurrentSeconds", _frame_current_time);//��λ��ms

				return eap::sma::jsonToString(root);
			}

			return std::string();
		}
	}
}