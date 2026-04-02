#include "EapsDispatchTaskImpl.h"
#include "EapsMacros.h"
#include "EapsConfig.h"
#ifdef ENABLE_AI
#include "jo_ai_object_detect.h"
#include "Track.h"
#endif

#ifdef ENABLE_AR
#include "jo_ar_engine_interface.h"
#endif

#ifdef ENABLE_AIRBORNE
#include "img_conv.h"
#else
#ifdef ENABLE_GPU
#include "EapsImageCvtColorCuda.h"
#endif
#endif // ENABLE_AIRBORNE

#include "Logger.h"
#include "OnceToken.h"
#include "EapsNoticeCenter.h"
#include "EapsMetaDataBasic.h"
#include "HttpClient.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/ThreadPool.h"
#include "Poco/StreamCopier.h"
#include <random>
#include <string>
#include <future>
#include <algorithm>
#include <chrono>
#include <cmath>

#include <Poco/SharedMemory.h>

#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
//#include <opencv2/opencv.hpp>
#include "opencv2/opencv.hpp"
#endif
namespace eap {
	namespace sma {
		struct Engines
		{
#ifdef ENABLE_AI
			joai::ObjectDetectPtr _ai_object_detector{};

#ifdef ENABLE_OPENSET_DETECTION
			joai::ObjectDetectPtr _openset_ai_object_detector{};
			std::mutex _openset_ai_engine_mutex{};
#endif 
			std::vector<joai::Result> latest_detect_result{};

			joai::ObjectDetectPtr _aux_ai_object_detector{};
			joai::MotTrackPtr _aux_ai_mot_tracker{};

			joai::MotTrackPtr _ai_mot_tracker{};
			std::mutex _ai_engine_mutex{};
#endif
		
#ifdef ENABLE_AR
			joar::ArEnginePtr _ar_engine{};
			jomarker::ArMarkerEnginePtr _ar_mark_engine{};
#endif
		};

		JoEdgeVersion DispatchTaskImpl::_joedge_version{};

		DispatchTaskImplPtr DispatchTaskImpl::createInstance(
			InitParameter init_parameter)
		{
			return DispatchTaskImplPtr(new DispatchTaskImpl(
				init_parameter));
		}

		DispatchTaskImpl::DispatchTaskImpl(InitParameter init_parameter)
			: DispatchTask(init_parameter)
		{
			_engines = EnginesPtr(new Engines());
			_meta_data_processor = MetaDataProcessing::createInstance();

			if (init_parameter.pull_url.find("webrtc://") == 0 || 
				init_parameter.pull_url.find("webrtcs://") == 0) {
				_is_pull_rtc = true;
			}
			if (init_parameter.pull_url.find("udp") == 0) {
				try {
					// 创建 IPAddress 对象
					auto first_index = init_parameter.pull_url.rfind("/")+1;
					auto end_index = init_parameter.pull_url.rfind(":");
					std::string pull_url = init_parameter.pull_url.substr(first_index, end_index- first_index);
					Poco::Net::IPAddress ip(pull_url);
					// 检查是否为多播地址
					_is_pull_udp = ip.isMulticast();
				}
				catch (const std::exception& e) {
					std::cerr << "Invalid IP address: " << std::string(e.what()) << std::endl;
				}
			}

			_record_file_prefix = init_parameter.record_file_prefix;
			_record_time_str = init_parameter.record_time_str;
			_ar_vector_file = init_parameter.ar_vector_file;
			_ar_camera_config = init_parameter.ar_camera_config;
			_task_id = init_parameter.id;
			_pull_url = init_parameter.pull_url;
			_push_url = init_parameter.push_url;
			_push_urls = init_parameter._push_urls;
			if (_push_urls.size() == 0) {
				_push_urls.push_back(_push_url);
			}
			_func_mask = init_parameter.func_mask;
			_is_pilotcontrol_task = init_parameter.is_pilotcontrol_task;
			eap_information_printf("----funcmask: %d-----", _func_mask.load());
			updateFuncmaskL();

			//_all_meta_data_ptr = std::make_shared<AllMetaData>();
			//_communication_reactor = CommunicationReactor::createInstance(immediately_msg_callback, _all_meta_data_ptr);
			//_communication_reactor->start();
			_airborne_45G = eap::configInstance().has(Media::kAirborne45G)? eap::configInstance().getInt(Media::kAirborne45G): 0;
#ifdef ENABLE_AIRBORNE
			// SD卡挂载
			// SdcartMount(device_name, mount_point);
			// 检查video文件系统状态并挂载
			// _file_manager = FileManager::createInstance();
			// _file_manager->setParams(device_path, device_name, mount_point);
			// _file_manager->fileSystemCheck();
			// _mFileManageThread = std::thread(&FileManager::monitorSpace, _file_manager);

			if (_init_parameter.need_decode) {
				std::string record_path{};
				try {
					_linux_storage_dev_manage = std::make_shared<LinuxStorageDevManage>(device_name, mount_point);
					GET_CONFIG(std::string, getString, my_record_path, Media::kRecordPath);
					record_path = my_record_path;
					_linux_storage_dev_manage->setautoClearfileflag(true, record_path, ".ts");
				}
				catch (const std::exception& e) {
					eap_error_printf("get config kRecordPath throw exception: %s", e.what());
				}
				
				AllocDeviceMem(&pIn420Dev, 2048 * 1080 * 3 / 2);
				AllocDeviceMem(&pOutBgrDev, 1920 * 1080 * 3);
			}

			//机载端是否重新编码
			try {
				GET_CONFIG(bool, getBool, my_enable_encode, Media::kEnableEncode);
				_enable_encode=my_enable_encode;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kEnableEncode throw exception: %s", e.what());
			}

			#ifdef ENABLE_PIO
				if(!_pio_pubilsh){
					_pio_pubilsh=std::make_shared<PioPublish>();
					#ifdef ENABLE_FIRE_DETECTION
					_pio_pubilsh->SubscribeIrAiMsgPio();
					#endif
				}

			#endif
#endif // ENABLE_AIRBORNE

			_meta_data_processing_pret = MetaDataProcessing::createInstance();
			_meta_data_processing_after = MetaDataProcessing::createInstance();
		}

		DispatchTaskImpl::~DispatchTaskImpl() 
		{
			stop();
		}

		void DispatchTaskImpl::start()
		{
#ifdef ENABLE_AIRBORNE
			if (_init_parameter.need_decode) {
				// 使用线程启动 io_context.run
				_tcpserver_io_run_thread = std::thread([this]() {
					// 创建TCP SERVER,暂时就多路流的第一路高清和单路流，才解码给连上TCP server的客户端发图片
					auto tcp_server_para_ptr = JoTcpServer::MakeInitParameter();
					auto port = 8444;
					if (eap::configInstance().has(General::kTcpServerPort)) {
						GET_CONFIG(int, getInt, tcp_port, General::kTcpServerPort);
						port = tcp_port;
					}
					else {
						eap::configInstance().setInt(General::kTcpServerPort, port);
						eap::saveConfig();
					}
					tcp_server_para_ptr->port = port;
					tcp_server_para_ptr->max_connections = 10;
					tcp_server_para_ptr->timeout = 100;
					_tcp_server = std::make_shared<JoTcpServer>(tcp_server_para_ptr);
					if (_tcp_server) {
						_tcp_server->run();
					}
				});
			}
#endif

			auto encoder_packet_callback = [this](Packet packet) {
				if (_is_manual_stoped)
					return;

				std::lock_guard<std::mutex> lock(_pushers_mutex);
				auto push_proc = [this](Packet& packet) {
					if (_pusher && _pusher->_pusher) {
						auto& md = packet.getMetaDataBasic();
						uint32_t detc_size = md.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize;
						uint32_t ai_status = md.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiStatus;
						// if (ai_status == 1 && detc_size > 0) {
						// 	eap_information_printf("[OPENSET-STEP2] push_proc -> pushPacket, metaDataValid: %d, AiStatus: %d, DetcSize: %d",
						// 		(int)packet.metaDataValid(), (int)ai_status, (int)detc_size);
						// }
						_pusher->_pusher->pushPacket(packet);
					}
				};
				
				auto meta_data_raw = packet.getSeiBuf();
				if (!meta_data_raw.empty()) {
					JoFmvMetaDataBasic metadata = packet.getMetaDataBasic();
					if (metadata.CarrierVehiclePosInfo_p.CarrierVehicleLat == 0 && metadata.CarrierVehiclePosInfo_p.CarrierVehicleLon == 0) {
						metadata = _last_metadata;
					} 
					int meta_data_sei_buffer_size{};
					meta_data_raw = _meta_data_processing_after->getSerializedBytesBySetMetaDataBasic(&metadata, &meta_data_sei_buffer_size);

					auto sei_buffer = MetaDataProcessing::seiDataAssemblyH264(
						meta_data_raw.data(), meta_data_raw.size());
					if (!sei_buffer.empty()) {
						AVPacket* pkt_new = av_packet_alloc();
						int new_packet_size = packet->size + sei_buffer.size();
						if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
							int pos = 0;
							memcpy(pkt_new->data, sei_buffer.data(), sei_buffer.size());
							pos += sei_buffer.size();
							memcpy(pkt_new->data + pos, packet->data, packet->size);
							pkt_new->pts = packet->pts;
							pkt_new->dts = packet->dts;
							pkt_new->duration = packet->duration;
							pkt_new->flags = packet->flags;
							
							Packet packet_export(pkt_new);
							packet_export.setSeiBuf(meta_data_raw);
							packet_export.setMetaDataBasic(&packet.getMetaDataBasic());
							packet_export.metaDataValid() = packet.metaDataValid();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
							packet_export.setArPixelPoints(packet.getArPixelPoints());
							packet_export.setArPixelLines(packet.getArPixelLines());
							packet_export.setArPixelWarningL1s(packet.getArPixelWarningL1s());
							packet_export.setArPixelWarningL2s(packet.getArPixelWarningL2s());
#endif
							packet_export.setCurrentTime(packet.getCurrentTime());
							packet_export.setArMarkInfos(packet.getArInfos());
							packet_export.setAiHeatmapInfos(packet.getAiHeatmapInfos());
							packet_export.setArValidPointIndex(packet.getArValidPointIndex());
							packet_export.setArVectorFile(packet.getArVectorFile());
							packet_export.setVideoParams(_framerate.num, _bit_rate);
							// 录像
#ifdef ENABLE_AIRBORNE
				if(_enable_encode)
				{
						if (_is_video_record_on) {
							std::lock_guard<std::mutex> lock(_muxer_mutex);
							if (!_muxer) {						
								eap_information("create muxer muxer muxer muxer ");
								createMuxer();
							}
							if (_muxer) {
								Packet new_packet;
								packet_export.copyTo(new_packet);
								_muxer->pushPacket(new_packet);
							}
							
						}
						else {
							destroyMuxer();
						}

						// 还缺少机载端编码后的片段录像功能
				}

#endif

							push_proc(packet_export);
						}
						else {
							push_proc(packet);
						}
					}
					else {
						push_proc(packet);
					}
				}else {
					push_proc(packet);
				}
			};

			// 创建demuxer
			eap_information_printf("create demuxer, pull url: %s", _pull_url);
			auto demuxer_packet_callback = [encoder_packet_callback, this](Packet packet) {
				if (_is_manual_stoped)
					return;
				auto start_t = std::chrono::system_clock::now();

#ifdef ENABLE_AIRBORNE
				if (_init_parameter.need_decode) {
					//同步飞行控制和吊舱的元数据（目前不考虑吊舱出来的视频里面就有元数据的情况），将元数据嵌入视频
					PayloadData close_payload_data{};
					PilotData close_pilot_data{};
					{
						//std::lock_guard<std::mutex> receive_payload_data_lock(_receive_payload_data_mutex);
						close_payload_data = _payload_data;
						//if(!_payload_data_map.empty()){
						//	// 获取当前时间戳
						//	auto now = std::chrono::system_clock::now();
						//	auto duration = now.time_since_epoch();
						//	auto ms_time_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
						//	close_payload_data = findClosePayload(_payload_data_map, ms_time_stamp);
						//}
					}
					{
						//std::lock_guard<std::mutex> receive_pilot_data_lock(_receive_pilot_data_mutex);
						close_pilot_data = _pilot_data;
						/*if(!_pilot_data_map.empty()){
							auto now = std::chrono::system_clock::now();
							auto duration = now.time_since_epoch();
							auto ms_time_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
							close_pilot_data = findClosePilot(_pilot_data_map, ms_time_stamp);
						}*/
					}

					auto meta_data_qianjue = _meta_data_processing_pret->metaDataParseQianjue(packet->data, packet->size, _codec_parameter.codec_id);
					if(meta_data_qianjue){
						packet.setMetaDataQianjue(meta_data_qianjue);
					}

					// 匹配元数据中时间戳与当前时间戳最近的取出, 赋值到 meta_data_basic 中
					std::shared_ptr<JoFmvMetaDataBasic> meta_data_basic = std::make_shared<JoFmvMetaDataBasic>();

#ifdef ENABLE_PIO	
#ifdef ENABLE_FIRE_DETECTION
					IrAiMsg ir_ai_msg{};
					if(_pio_pubilsh->GetIrAiMsgPio(ir_ai_msg)){
						convertToMetaDataBasic(meta_data_basic, ir_ai_msg);
					}
#endif
#endif
					convertToMetaDataBasic(meta_data_basic, close_payload_data, close_pilot_data);
					// 使用元数据SDK，获取序列换化的meta_data sei buffer，然后调接口直接替换
					int meta_data_basic_raw_size{};
					auto meta_data_raw = _meta_data_processing_pret->getSerializedBytesBySetMetaDataBasic(meta_data_basic.get(), &meta_data_basic_raw_size);
					//解析元数据，将元数据放入封装的packet
					packet.metaDataValid() = true;
					packet.setMetaDataBasic(meta_data_basic.get());
					packet.setSeiBuf(meta_data_raw);
				}
#else // not ENABLE_AIRBORNE

				// 解析元数据，将元数据放入封装的packet
				if (_meta_data_processing_pret) {
					std::vector<uint8_t> raw_data{};
					auto meta_data = _meta_data_processing_pret->metaDataParseBasic(
						packet->data, packet->size, _codec_parameter.codec_id, raw_data);

					if (meta_data.first) {
						packet.metaDataValid() = true;
						packet.setMetaDataBasic(meta_data.first);
						_last_metadata = *meta_data.first;
                        if (configInstance().getBool(Vehicle::KARSeat)) {
                            // 解析元数据
                            parseMetaData(meta_data.first);
						}
					}
					if (!raw_data.empty()) {
						packet.setSeiBuf(raw_data);
					}
				}
				
				
				if(_record || _is_recording){
					std::lock_guard<std::mutex> lock(_record_queue_mutex);
					_record_packets.push(packet);
					_record_queue_cv.notify_all();
				}else{
					std::unique_lock<std::mutex> lock(_record_queue_mutex);
					while(!_record_packets.empty()){
						_record_packets.pop();
					}
				}
#endif // ENABLE_AIRBORNE			
				// 设置当前时间点
				AVRational  dst_time_base = { 1, AV_TIME_BASE };
				int64_t current_time = (packet->pts /*- _start_time_stamp*/) * av_q2d(_timebase) * 1000.f;
				packet.setCurrentTime(current_time);
#ifndef ENABLE_AIRBORNE
				try {
					// 非机载录像
					if (_is_video_record_on) {
						std::lock_guard<std::mutex> lock(_muxer_mutex);
						if (!_muxer) {
							eap_information("create muxer muxer muxer muxer ");
							createMuxer();
						}
						if (_muxer) {
							Packet new_packet;
							packet.copyTo(new_packet);
							_muxer->pushPacket(new_packet);
						}
					}
					else {
						destroyMuxer();
					}
				}
				catch (const std::exception &exp) {
					eap_warning_printf("video_record exception: %s", std::string(exp.what()));
				}
#endif
#ifdef ENABLE_GPU
				// 解码 - 默认一直解码
				{
					if (_decoder){
						_decoder->pushPacket(packet);
					}
				}
#else
#ifdef ENABLE_AR
				std::promise<void> ar_promise{};
				auto ar_future = ar_promise.get_future();
				createAREngine();
				if (_is_ar_on || _is_enhanced_ar_on) {
						std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
						ThreadPool::defaultPool().start([this, &packet, &ar_promise, this_weak_ptr]()
						{
								auto this_shared_ptr = this_weak_ptr.lock();
								if (!this_shared_ptr) {
									ar_promise.set_value();
									return;
								}

								executeARProcess(packet);
								ar_promise.set_value();
						});
				}
				else {
						ar_promise.set_value();
				}				

				{
						//TODO: 先把标注拿出来改成同步,相当于也是异步，在主线程中和子线程异步
						//camera.config 有更新，就更新AR标注引擎
						if(_is_update_ar_mark_engine){
								destroyARMarkEngine();
								createARMarkEngine();
								_is_update_ar_mark_engine.store(false);
						}
						executeARMarkProcess(packet);
				}
#endif
				if(_is_update_func){
					if(!_update_func_err_desc.empty()){
						_update_func_err_desc += std::string("update failed");
					}

					std::string id_temp = _id;
					NoticeCenter::Instance()->getCenter().postNotification(new FunctionUpdatedNotice(
					std::string(id_temp), _update_func_result, _update_func_err_desc, _func_mask));

					_is_update_func = false;
					_update_func_result = true;
					_update_func_err_desc.clear();
				}

				//设置当前时间点			
				int64_t _start_time_stamp = av_rescale_q(_start_time, dst_time_base, _timebase);
				current_time = (packet->pts - _start_time_stamp) * av_q2d(_timebase) * 1000.f;
				packet.setCurrentTime(current_time);

				encoder_packet_callback(packet);
#endif //ENABLE_GPU

#ifdef ENABLE_AIRBORNE
				if(!_enable_encode && _init_parameter.need_decode)
				{
					std::lock_guard<std::mutex> lock_wait_meta_data(_wait_meta_data_packet_q_mutex);
					_wait_meta_data_packet_q.push(packet);
					_wait_meta_data_packet_q_cv.notify_all();
				}
#endif // ENABLE_AIRBORNE
			};
			auto demuxer_stop_callback = [this](int exit_code) {
				_is_demuxer_closed.store(true);
				auto weak_this = weak_from_this();
				eap::ThreadPool::defaultPool().start([this, weak_this, exit_code]() {
					if (_is_manual_stoped)
						return;
					//不是udp组播视频，demuxer stop就算拉流失败
					if (!_is_pull_udp)
						_is_demuxer_opened = false;
					bool read_frame_tiomeout = _demuxer? _demuxer->isReadFrameTimeout(): false;
					std::string desc = "demuxer stoped, exit code: "
						+ std::to_string(exit_code) + ", error msg:" + AVError2String(exit_code)
						+ ", id:"+ _id + ", read_frame_tiomeout " + std::to_string((int)read_frame_tiomeout);
					eap_error(desc);
					std::string pilot_sn{};
					std::string task_id{_id};
					try {
						std::size_t index = _push_url.rfind("/");
						std::size_t second_index = _push_url.rfind("/", index - 1);
						pilot_sn = _push_url.substr(second_index + 1, index - second_index - 1);
					} catch (const std::exception& e) {
						eap_error(std::string(e.what()));
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					if (_is_start_finished) {
						if(_is_pilotcontrol_task || read_frame_tiomeout){
							std::string error_msg{};
							if (!openDemuxer(error_msg)) {
								eap_information_printf("reopen demuxer failed, id: %s", task_id);
								NoticeCenter::Instance()->getCenter().postNotification(
									new TaskStopedNotice(std::string(task_id), desc, pilot_sn));
							}
						}else {
							eap_information_printf("demuxer stoped, start remove task id: %s", task_id);
							NoticeCenter::Instance()->getCenter().postNotification(
							new TaskStopedNotice(std::string(task_id), desc, pilot_sn));
						}
					}
				});
			};
			if (_is_pull_udp) {
				//初始化
				if (_lan_demuxer_reactors.size() > 0) {
					for (auto iter : _lan_demuxer_reactors) {
						if (iter) {
							iter.reset();
						}
					}
					_lan_demuxer_reactors.resize(0);
				}
				if (_wlan_demuxer_reactors.size() > 0) {
					for (auto iter : _wlan_demuxer_reactors) {
						if (iter) {
							iter.reset();
						}
					}
					_wlan_demuxer_reactors.resize(0);
				}
				_reactor_thread_looptimes = 0;
				_current_network_index = -1;
				_adapter_num = 0;	
				_is_stop_reactor_loop.store(false);
				_is_demuxer_closed.store(false);
				_is_demuxer_opened = true;
				_demuxer_stop_callback = demuxer_stop_callback;
				_demuxer_packet_callback = demuxer_packet_callback;
				_open_timeout = std::chrono::milliseconds(1000*10);

				//如果_multiNetWorkCardFiltering，查询到有网卡，但是查找到时，所有网卡的状态都是down，那么就没有open过，那么就会一直卡死在上一个步骤
				//只有_multiNetWorkCardFiltering，查询到有网卡，存在网卡的状态时up，然后调用了_OpenDmuexer，待demuxer的open返回结果（不论打开成功还是失败）
				//才会执行_reactorLoopThread
				multiNetWorkCardFiltering(_init_parameter.pull_url, _open_timeout, 30, demuxer_stop_callback, demuxer_packet_callback);

				_reactor_thread_ptr = std::make_shared<std::thread>(&DispatchTaskImpl::reactorLoopThread, this);

				auto start_t = std::chrono::system_clock::now();
				do
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));//0.1s

					std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
					if (_current_network_index >= 0) {//这样获取参数，如果中途断开，然后接上的是不同的参数的视频流，会存在问题
						if (_current_network_adapter_type == common::NetWorkChecking::NetworkAdapterType::Lan) {
							_codec_parameter = _lan_demuxer_reactors[_current_network_index]->getAVCodecParameters();
							_framerate = _lan_demuxer_reactors[_current_network_index]->getFrameRate();
							_bit_rate = _lan_demuxer_reactors[_current_network_index]->getBitRate();
							_timebase = _lan_demuxer_reactors[_current_network_index]->getTimeBase();
						}
						else {
							_codec_parameter = _wlan_demuxer_reactors[_current_network_index]->getAVCodecParameters();
							_framerate = _wlan_demuxer_reactors[_current_network_index]->getFrameRate();
							_bit_rate = _wlan_demuxer_reactors[_current_network_index]->getBitRate();
							_timebase = _wlan_demuxer_reactors[_current_network_index]->getTimeBase();
						}
					}

					if(!_is_pilotcontrol_task){
						//超时抛异常退出
						auto now_t = std::chrono::system_clock::now();
						if (std::chrono::duration_cast<std::chrono::milliseconds>(now_t - start_t).count()
							>= (_open_timeout.count() * 3))
						{
							_is_stop_reactor_loop.store(true);
							if (_reactor_thread_ptr && _reactor_thread_ptr->joinable()) {
								_reactor_thread_ptr->join();
							}
							_is_demuxer_opened = false;
							std::string error_description = "udp cannot get video codec param, url = " + _init_parameter.pull_url;
							eap_error(error_description);
							throw std::system_error(-1, std::system_category(), error_description);
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					}
				} while (_framerate.num <= 0 || _framerate.den <= 0 || _timebase.num <= 0 || _timebase.den <= 0
					|| _codec_parameter.height <= 0 || _codec_parameter.width <= 0);
			}
			else {
				if (_is_pull_rtc) {
					_demuxer = DemuxerRtc::createInstance();
				}
				else {
					_demuxer = DemuxerTradition::createInstance();
				}
				_demuxer->setPacketCallback(demuxer_packet_callback);
				_demuxer->setStopCallback(demuxer_stop_callback);
				std::string error_msg{};
				auto ret = openDemuxer(error_msg);
				if(!_is_pilotcontrol_task){
					if(ret)
						_is_demuxer_opened = true;
					else if(!_is_manual_stoped)
						throw std::system_error(-1, std::system_category(), error_msg);
				}
				if (_is_manual_stoped)
					return;
				_codec_parameter = _demuxer->videoCodecParameters();
				_timebase = _demuxer->videoStreamTimebase();
				_framerate = _demuxer->videoFrameRate();
				_bit_rate = _demuxer->bitRate();
				_start_time = _demuxer->videoStartTime();
			}

			// 创建推流器
			eap_information_printf("create pusher, push url: %s", _push_url);
			createPusher(_init_parameter.push_url);

#ifdef ENABLE_GPU
			// 创建解码器
			auto decoder_frame_callback = [this](Frame frame) {
				{// AI、AR、处理不过来时丢image，丢帧时可能会导致元数据跟帧率对不上
					std::lock_guard<std::mutex> lock(_decoded_images_mutex);
					if (_decoded_images.size() > 5) {
						frame.clear();
						eap_warning("decoded images queue is full, drop frame");
						return;
					}
				}

				// 转换
				CodecImagePtr decode_image = CodecImagePtr(new CodecImage());
				decode_image->height = frame->height;
				decode_image->width = frame->width;
#ifdef ENABLE_AIRBORNE
				if (_init_parameter.need_decode) {// tcp server send queue
					auto frame_interval = 1;
					if (eap::configInstance().has(General::kFrameInterval)) {
						GET_CONFIG(int, getInt, _frame_interval, General::kFrameInterval);
						frame_interval = _frame_interval;
					}
					else {
						eap::configInstance().setInt(General::kFrameInterval, frame_interval);
						eap::saveConfig();
					}
					if (frame_interval == _send_frame_count) {//默认2抽1
						_send_frame_count = 0;
						std::lock_guard<std::mutex> lock(_tcp_send_frame_mutex);
						if (_tcp_send_frame_q.size() >= 5) {
							auto frame = _tcp_send_frame_q.front();
							av_frame_free(&frame);
							_tcp_send_frame_q.pop();
							_tcp_send_sei_q.pop();
						}
						AVFrame* new_frame;
						frame.copyTo(&new_frame);
						_tcp_send_frame_q.push(new_frame);
						_tcp_send_sei_q.push(frame.getMetaDataQianjue());
					}
					else {
						_send_frame_count++;
					}
				}

				try {// TODO: 增加判断条件，根据是否需要做ai相关来决定是否进行转换
					//cv::Mat bgr240_image(frame->height, frame->width, CV_8UC3);
					cv::Mat mat;
					cv::Mat yuv_data(frame->height * 3 / 2, frame->width, CV_8UC1);
					int image_size = frame->height * frame->width;
					switch (frame->format)
					{
					case (int)AV_PIX_FMT_RGB24:
						// 直接映射 RGB24 到 cv::Mat
						mat = cv::Mat(frame->height, frame->width, CV_8UC3, frame->data[0], frame->linesize[0]);
						decode_image->bgr24_image = mat;
						break;

					case (int)AV_PIX_FMT_BGR24:
						mat = cv::Mat(frame->height, frame->width, CV_8UC3, frame->data[0], frame->linesize[0]);
						cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB); // 转RGB
						decode_image->bgr24_image = mat;
						break;
					default:
						memcpy(yuv_data.data, frame->data[0], image_size);
						memcpy(yuv_data.data + image_size, frame->data[1], image_size / 4);
						memcpy(yuv_data.data + image_size * 5 / 4, frame->data[2], image_size / 4);
						// I4202Bgr((char*)yuv_data.data, (char*)bgr240_image.data, pIn420Dev, pOutBgrDev, yuv_data.cols, yuv_data.rows * 2 / 3, 0, 0, frame->width, frame->height);
						if (!yuv_data.empty()) {
							decode_image->bgr24_image = yuv_data;
						}
						break;
					}
					decode_image->format = frame->format;
				}
				catch (const std::exception& e) {
					eap_error_printf("frame 2 cpu_mat faile, error description: %s", e.what());
					return;
				}
#else
				#ifdef ENABLE_GPU
								cv::cuda::GpuMat bgr24_image_gpu;
								cv::cuda::GpuMat bgr32_image_gpu;
								try {
									bgr24_image_gpu = cv::cuda::GpuMat(frame->height, frame->width, CV_8UC3);
									bgr32_image_gpu = cv::cuda::GpuMat(frame->height, frame->width, CV_8UC4);

									ImageCvtColor::Instance()->nv12ToBgr24(frame, bgr24_image_gpu);
									#ifdef ENABLE_STABLIZE
									if (_is_stable_on) {
										createStablizer();
										if (_stablizer) {
											cv::Rect rc; // 可根据需要设置 ROI
											// auto start = std::chrono::high_resolution_clock::now();
											_stablizer->stablize(bgr24_image_gpu, rc);
											// auto end = std::chrono::high_resolution_clock::now();
											// auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
											// std::cout << "[TIME] : " << duration << "ms" << std::endl;
										}
									}
									#endif
									ImageCvtColor::Instance()->bgr24ToBgr32(bgr24_image_gpu, bgr32_image_gpu);
								} catch (const std::exception& e) {
									eap_information(e.what());
									return;
								}
								decode_image->bgr24_image = bgr24_image_gpu;
								decode_image->bgr32_image = bgr32_image_gpu;
								// bgr24_image_gpu.release();
								// bgr32_image_gpu.release();
				#endif
#endif // ENABLE_AIRBORNE

				decode_image->meta_data.pts = frame->pts;			
				decode_image->meta_data.current_time = frame.getCurrentTime();
				auto sei_buffer = frame.getSeiBuf();
				if (!sei_buffer.empty()) {
					decode_image->meta_data.meta_data_raw_binary = sei_buffer;
				}
				decode_image->meta_data.meta_data_basic = frame.getMetaDataBasic();
				decode_image->meta_data.meta_data_valid = frame.getMetaDataValid();
				decode_image->meta_data.original_pts = frame.getOriginalPts();

				// 封装一个快照类，提供pushimage接口
				// 往快照类送image
				// 效率不够，会导致内存堆积
				{
					std::lock_guard<std::mutex> lock(_decoded_images_mutex);
					_decoded_images.push(decode_image);
					_decoded_images_cv.notify_all();
				}
				
				{
					int snapshot_numbers{};
					try {
						GET_CONFIG(int, getInt, my_snapshot_numbers, Media::kSnapShotNumbers);
						snapshot_numbers = my_snapshot_numbers;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config kSnapShotNumbers throw exception: %s", e.what());
					}

					std::lock_guard<std::mutex> snapshot_lock(_decoded_images_snapshot_mutex);
					_decoded_images_snapshot.push(decode_image);
					while (_decoded_images_snapshot.size() > snapshot_numbers) {
						_decoded_images_snapshot.pop();
					}
				}

				if(_is_ai_assist_track_on){
					std::lock_guard<std::mutex> assist_lock(_decoded_images_assist_track_mutex);
					_decoded_images_assist_track.push(decode_image);
					while (_decoded_images_assist_track.size() > 2) {
						_decoded_images_assist_track.pop();
					}
				}
			};
	
#ifndef ENABLE_AIRBORNE
			if (!_is_demuxer_closed){
				eap_information_printf("create decoder, task id: %s", _id);
				createDecoder(decoder_frame_callback);// 默认创建decoder
			}
#else
			if (_init_parameter.need_decode) {
				eap_information_printf("create decoder, task id: %s", _id);
				createDecoder(decoder_frame_callback);// 默认创建decoder
			}
#endif
#endif
#ifdef ENABLE_GPU
//非机载端直接创建解码器
#ifndef ENABLE_AIRBORNE
			if (!_is_demuxer_closed){
				eap_information("not airbornd,create encoder");
				createEncoder(encoder_packet_callback);
			}
#else
	//机载端，如果配置文件里面写了要创建解码器，才会去创建解码器
	if(_enable_encode)
	{
		eap_information("airbornd,create encoder encoder encoder encoder ");
		createEncoder(encoder_packet_callback);
	}

#endif // ENABLE_AIRBORNE
			// 创建 ai engine
#ifdef ENABLE_AI
			createAIEngine();
			createOpensetAIEngine();
			_is_ai_first_create = false;
			_is_openset_ai_first_create = false;
#endif
#endif // ENABLE_GPU
			// 创建 ar engine
#ifdef ENABLE_AR
			createAREngine();
			_is_ar_first_create = false;
#endif
			if (_is_demuxer_closed) {
				//start函数还没有结束，但是已经触发了demuxer stop callback，还是抛异常出去删任务
				std::string err_msg = "task demuxer stoped before start finished! id:" + _id;
				eap_information(err_msg);
				throw std::system_error(-1, std::system_category(), err_msg);
				return;
			}
			recordLoopThread();
#ifdef ENABLE_GPU
			dangerLoopThread();
			_loop_thread_new = std::thread([encoder_packet_callback, this]() {
				// 做AI、AR还要增加一个抽帧率 - TODO: 目前默认2抽1，后续写到配置文件中
#ifndef ENABLE_AIRBORNE
					_loop_run_new = true;
#else
					_loop_run_new = _init_parameter.need_decode;
#endif

					bool should_process{};
					
					std::string ai_heatmap_file_path{};
					bool track_switch{};
					try {
						GET_CONFIG(std::string, getString, my_ai_heatmap_file_path, AI::kAiHeatmapFilePath);
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
						ai_heatmap_file_path = my_ai_heatmap_file_path;
						track_switch = my_track_switch;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					for (; _loop_run_new;) {
					std::unique_lock<std::mutex> lock(_decoded_images_mutex);
					if (_decoded_images.empty()) {
						// _decoded_images_cv.wait(lock, [this] {
						// 	return !_loop_run_new;
						// });
						_decoded_images_cv.wait_for(lock, std::chrono::milliseconds(100));
					}
					if (!_loop_run_new) {
						break;
					}

					if (_decoded_images.empty()) {
						lock.unlock();
						continue;
					}

					auto image = _decoded_images.front();				
					_decoded_images.pop();
					lock.unlock();

#ifndef ENABLE_AIRBORNE
					if (_snapshot) { //快照
						std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
						_snapshot_image = image->bgr24_image.clone();
						ThreadPool::defaultPool().start([this, this_weak_ptr]() {
							auto this_shared_ptr = this_weak_ptr.lock();
							if (!this_shared_ptr) {
								return;
							}
							//上传图片
							std::string base64_encoded{};
							cv::Mat bgr24_image_cpu;// 是否需要初始化
							try {
								bgr24_image_cpu.create(_snapshot_image.size(), _snapshot_image.type()); // 初始化 cpuImage
								_snapshot_image.download(bgr24_image_cpu);
								std::vector<uchar> buffer;
								cv::imencode(".jpg", bgr24_image_cpu, buffer, { cv::IMWRITE_JPEG_QUALITY, 80 });
								base64_encoded = "data:image/jpg;base64," + encodeBase64({ buffer.begin(), buffer.end() });
								bgr24_image_cpu.release();
								_snapshot_image.release();
							}
							catch (const std::exception& e) {
								bgr24_image_cpu.release();
								_snapshot_image.release();
								eap_error_printf("snapshot throw exception: %s", std::string(e.what()));
								return;
							}
							Poco::JSON::Object json;
							std::size_t index = _push_url.rfind("/");
							std::size_t second_index = _push_url.rfind("/", index - 1);
							std::string pilot_sn = _push_url.substr(second_index + 1, index - second_index - 1);
							json.set("file", base64_encoded);
							json.set("autopilotSn", pilot_sn);
							json.set("code", 0);
							GET_CONFIG(std::string, getString, uploadSnapshot, General::kUploadSnapshot);
							std::string url = uploadSnapshot + "/flightmonitor/custom/v1/file/addPhoto";
							if (!_pilot_id.empty()) {
								//森防项目，第一次火点定位出结果的时候通知云端sma，快照图片给云平台
								json.set("targetLatitude", _target_lat);
								json.set("targetLongitude", _target_lon);
								json.set("targetAltitude", _target_alt);
								uploadSnapshot = eap::configInstance().getString(AI::KDangerPhotoServerUrl);
								url = uploadSnapshot + "/flightmonitor/index/joVideo/hook/lockFireNotify";
								_pilot_id = "";
							} else {
								json.set("is_hd", true);
								json.set("record_no", _recordNo);
							}
							std::string json_string = jsonToString(json);
							eap_information_printf("hook snapshot, url: %s, pilot id: %s, target_lat:%f, target_lon: %f, target_alt: %f", url, pilot_sn, _target_lat, _target_lon, _target_alt);
							auto http_client = HttpClient::createInstance();
							http_client->doHttpRequest(url, json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									try {
										Poco::JSON::Parser parser;
										auto dval = parser.parse(response);

										auto obj = dval.extract<Poco::JSON::Object::Ptr>();
										int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
										if (code != 0 && code != 200) {
											std::string msg = obj && obj->has("msg") ? obj->getValue<std::string>("msg"): "";
											eap_error_printf("addPhoto post failed, http status=%d, return code=%d, msg=%s",status, code, msg);
										} else {
											eap_information("addPhoto post succeed");
										}
									} catch (...) {
										eap_warning("addPhoto post failed");
									}
								} else {
									eap_warning("addPhoto post failed");
								}
							});
						});
						_snapshot = false;
					}
#endif
					// AI AR DEFOG等在这里调度
	#ifdef ENABLE_AI
					std::vector<joai::Result> ai_detect_ret{};

					std::promise<void> ai_promise;
					auto ai_future = ai_promise.get_future();
#ifdef ENABLE_OPENSET_DETECTION
					std::vector<joai::Result> openset_detect_ret{};

					std::promise<void> openset_ai_promise;
					auto openset_ai_future = openset_ai_promise.get_future();
					
                if (_is_ai_on && !_is_openset_ai_on) {
                    destroyOpensetAIEngine();
                }
                if (_is_openset_ai_on && !_is_ai_on)
				{
					destroyAIEngine();
				}
#endif

					if (_is_ai_on) {
						createAIEngine();
						std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
						ThreadPool::defaultPool().start([this, &image, &ai_promise, &ai_detect_ret, track_switch, this_weak_ptr]()
						{
							auto this_shared_ptr = this_weak_ptr.lock();
							if (!this_shared_ptr) {
								ai_promise.set_value();
								return;
							}

							if (!_engines->_ai_object_detector) {
								ai_promise.set_value();
								return;
							}
							auto start_t_pio_ai = std::chrono::system_clock::now();
#ifndef ENABLE_AIRBORNE
							auto detect_objects = _engines->_ai_object_detector->detect(image->bgr24_image);

#else
							std::vector<joai::Result> detect_objects{};

							//ai compute： skip 1 frame
							#ifdef ENABLE_FIRE_DETECTION
							if(_is_process_ai.load()){
								_sengfang_detect_objects = _engines->_ai_object_detector->detect(image->bgr24_image, true);	
								_is_process_ai.store(false);
							}else{
								_is_process_ai.store(true);
							}
							detect_objects=_sengfang_detect_objects;
							// eap_information_printf("-----------------before pio detect_objects size: %d",(int)detect_objects.size());
							#else
							detect_objects=_engines->_ai_object_detector->detect(image->bgr24_image, true);	
							#endif
							auto now_t_ai = std::chrono::system_clock::now();
							auto ai_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_t_ai - start_t_pio_ai).count();
							if((int)ai_duration_ms>(33*2)){ //frame_rate=30
								eap_information_printf("ai_duration_ms: %d",(int)ai_duration_ms);
							}


							
#ifdef ENABLE_PIO
							//[sengfang project]
							#ifdef ENABLE_FIRE_DETECTION
							if(_pio_pubilsh){
								AiInfosPio ai_infos{};
								if(track_switch){
									auto track_rets = _engines->_ai_mot_tracker->Update(detect_objects);
									std::vector<joai::Result> ai_infos_tra{};
									for (auto object : track_rets) {
										joai::Result ai_detect_temp{};
										ai_detect_temp.Frame_num = object.Frame_num;
										ai_detect_temp.frame_id = object.frame_id;
										ai_detect_temp.cls = object.cls;
										ai_detect_temp.confidence = object.confidence;
										ai_detect_temp.Bounding_box = object.Bounding_box;
										ai_infos_tra.push_back(ai_detect_temp);
									}
									ai_infos=_pio_pubilsh->AiInfosPreprocess(ai_infos_tra, image->bgr24_image.cols, image->bgr24_image.rows, _fire_conf_thresh, _smoke_conf_thresh);
								}else{
									ai_infos=_pio_pubilsh->AiInfosPreprocess(detect_objects, image->bgr24_image.cols, image->bgr24_image.rows, _fire_conf_thresh, _smoke_conf_thresh);
								}
								
								_pio_pubilsh->PioAiInfosPublish(ai_infos);
								auto now_t_ai_pio = std::chrono::system_clock::now();
								auto pio_ai_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_t_ai_pio - start_t_pio_ai).count();
								if((int)ai_duration_ms>(33*2)){//frame_rate=30
									eap_information_printf("pio_ai_duration_ms: %d",(int)pio_ai_duration_ms);
								}
							}
							#endif

#endif					
#endif
							// image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
							// 	ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = detect_objects.size();
							image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
								ImageProcessingBoardInfo_p.AiInfos_p.AiStatus = 1;

							// int frame_counter = 0;
							ai_detect_ret.clear();
							if(track_switch){
								auto track_ret = _engines->_ai_mot_tracker->Update(detect_objects);
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
									ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = track_ret.size();
								int i = 0;

								static std::unordered_set<int> prev_ids_class0;
								static std::unordered_set<int> prev_ids_class1;
								std::unordered_set<int> current_ids_class0;
								std::unordered_set<int> current_ids_class1;
								
								joai::Result ai_detect_temp{};
								for (auto object : track_ret) {
									ai_detect_temp.Frame_num = object.Frame_num;
									ai_detect_temp.frame_id = object.frame_id;
									ai_detect_temp.cls = object.cls;
									ai_detect_temp.confidence = object.confidence;
									ai_detect_temp.Bounding_box = object.Bounding_box;
									ai_detect_ret.push_back(ai_detect_temp);

									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX =
										object.Bounding_box.x;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY =
										object.Bounding_box.y;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetWidth =
										object.Bounding_box.width;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetHeight =
										object.Bounding_box.height;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtSN =
										object.track_id;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass =
										object.cls;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtConfidence =
										object.confidence * 100;

									if(object.cls == 0) {
										current_ids_class0.insert(object.track_id);
									} else if(object.cls == 1) {
										current_ids_class1.insert(object.track_id);
									}
									++i;
								}
								int new_count_class0 = 0;
								for(auto id : current_ids_class0) {
									if(prev_ids_class0.find(id) == prev_ids_class0.end()){
										new_count_class0++;
									}
								}
								int new_count_class1 = 0;
								for(auto id : current_ids_class1) {
									if(prev_ids_class1.find(id) == prev_ids_class1.end()){
										new_count_class1++;
									}
								}
								image->meta_data.ai_heatmap_info.new_count_class0 = new_count_class0;
								image->meta_data.ai_heatmap_info.new_count_class1 = new_count_class1;
								prev_ids_class0 = current_ids_class0;
								prev_ids_class1 = current_ids_class1;

								// if (frame_counter % 60 == 0 && new_count_class0 > 0 || new_count_class1 > 0) {
								// 	HeatmapData data;
								// 	data.class0_count = new_count_class0;
								// 	data.class1_count = new_count_class1;
								// 	data.ImgCood.Lat = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[4].Lat;
								// 	data.ImgCood.Lon = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[4].Lon;
								// 	// data.ImgCood.HMSL = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[4].HMSL;
								// 	heatmap_cache.push_back(data);
								// }
								// if (heatmap_cache.size() >= cache_limit) {
								// 	std::string file_path = ai_heatmap_file_path + "/heatmap_" + _task_id + ".txt";
								// 	FILE* file = fopen(file_path.c_str(), "a");
								// 	if (!file) {
								// 		eap_error("Failed to open heatmap file for writing");
								// 	} else {
								// 		for (const auto& data : heatmap_cache) {
								// 			fprintf(file, "%ld,%ld,%d,%d\n",
								// 				 data.ImgCood.Lat, data.ImgCood.Lon, data.class0_count, data.class1_count);
								// 		}
								// 	}
								// }
								// frame_counter++;
							}else{
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
								ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = detect_objects.size();
								// eap_information_printf("-----------------before pio detect_objects size: %d",(int)detect_objects.size());
								int i = 0;
								for (auto object : detect_objects) {
#ifdef ENABLE_AIRBORNE
#ifdef ENABLE_FIRE_DETECTION
									bool fire_u_condition = (object.cls == 0) && (object.confidence <= _fire_conf_thresh);
                                    bool smoke_u_condition = (object.cls == 1) && (object.confidence <= _smoke_conf_thresh);
									if(fire_u_condition || smoke_u_condition){
										#if 0
										const auto cls_str = std::to_string(object.cls);
                    					const auto conf_str = std::to_string(object.confidence);
										eap_information_printf("unpassed in task! pobj.cls: %s,obj.confidence: %s",cls_str, conf_str);
										#endif
										continue;
									}
#endif
#endif
									ai_detect_ret.push_back(object);

									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX =
										object.Bounding_box.x;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY =
										object.Bounding_box.y;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetWidth =
										object.Bounding_box.width;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetHeight =
										object.Bounding_box.height;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass =
										object.cls;
									image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
										ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtConfidence =
										object.confidence * 100;
									++i;
								}
							}
							
							ai_promise.set_value();
						});
					}
					else {
#ifndef ENABLE_FIRE_DETECTION
						destroyAIEngine();
#endif
						ai_promise.set_value();
					}

// ... existing code ...
#ifdef ENABLE_OPENSET_DETECTION
                if (_is_openset_ai_on) {
                    if (!_engines->_openset_ai_object_detector && (_is_openset_ai_first_create || _is_update_func)) {
                        if (!_openset_ai_creating.load()) {
                            _openset_ai_creating.store(true);
                            _is_openset_ai_first_create = true;
                            std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
                            ThreadPool::defaultPool().start([this, this_weak_ptr]() {
                                auto this_shared_ptr = this_weak_ptr.lock();
                                if (!this_shared_ptr) {
                                    _openset_ai_creating.store(false);
                                    return;
                                }
                                {
                                    std::lock_guard<std::mutex> lock(_engines->_openset_ai_engine_mutex);
                                    createOpensetAIEngine();
                                }
                                _is_openset_ai_first_create = false;
                                _openset_ai_creating.store(false);
                            });
                        }
                        openset_ai_promise.set_value();
                    } else if (_engines->_openset_ai_object_detector && !_openset_ai_creating.load()) {
                        std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
                        ThreadPool::defaultPool().start([this, &image, &openset_ai_promise, &openset_detect_ret, this_weak_ptr]()
                        {
                            auto this_shared_ptr = this_weak_ptr.lock();
                            if (!this_shared_ptr) {
                                openset_ai_promise.set_value();
                                return;
                            }

                            std::lock_guard<std::mutex> lock(_engines->_openset_ai_engine_mutex);
                            if (!_engines->_openset_ai_object_detector) {
                                openset_ai_promise.set_value();
                                return;
                            }
                            GET_CONFIG(std::string, getString, vl_text, AI::kVLtext);
                            if (prompt.empty() || prompt != vl_text){
                                prompt = vl_text;
                                bool encode_text_flag= _engines->_openset_ai_object_detector->encode_text(prompt);
                            }
                            auto detect_objects = _engines->_openset_ai_object_detector->detect(image->bgr24_image);

                            image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                                ImageProcessingBoardInfo_p.AiInfos_p.AiStatus = 1;

                            openset_detect_ret.clear();
                            for (auto& object : detect_objects) {
                                openset_detect_ret.push_back(object);
                            }

                            openset_ai_promise.set_value();
                        });
                    } else {
                        openset_ai_promise.set_value();
                    }
                }
                else {
                    destroyOpensetAIEngine();
                    openset_ai_promise.set_value();
                }
#endif // ENABLE_OPENSET_DETECTION
// ... existing code ...
#endif
#ifdef ENABLE_AR
					std::promise<void> ar_promise{};
					auto ar_future = ar_promise.get_future();
					createAREngine();
					if (_is_ar_on || _is_enhanced_ar_on) {
						if(_is_enhanced_ar_on){
							createAuxiliaryAIEngine();
						}
						std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
						ThreadPool::defaultPool().start([this, &image, &ar_promise, this_weak_ptr]()
						{
							auto this_shared_ptr = this_weak_ptr.lock();
							if (!this_shared_ptr) {
								ar_promise.set_value();
								return;
							}

							executeARProcess(image);
							ar_promise.set_value();
						});
					}
					else {
						if(_engines->_aux_ai_object_detector){
							destroyAuxiliaryAIEngine();
						}
						ar_promise.set_value();
					}				
					
					{
						//TODO: 先把标注拿出来改成同步,相当于也是异步，在主线程中和子线程异步

						//camera.config 有更新，就更新AR标注引擎
						if(_is_update_ar_mark_engine){
							destroyARMarkEngine();
							createARMarkEngine();
							_is_update_ar_mark_engine.store(false);
						}
						executeARMarkProcess(image);
					}
#endif
					cv::Mat M;
					bool is_stable_geted{};
					int64_t pts_temp{};
#ifndef ENABLE_AIRBORNE
					if (_is_image_enhancer_on) {
						do {
							if (_is_image_stable_on && !is_stable_geted) {
								break;
							}
							createImageEnhancer();

							if (_enhancer) {
								_enhancer->Enhance(image->bgr32_image.data);
							}
						} while (false);
					}
					else {
						destroyImageEnhancer();
					}
#endif

	#ifdef ENABLE_AI
					ai_future.get();
#ifdef ENABLE_OPENSET_DETECTION
                openset_ai_future.get();
                {
                    int base_idx = (int)ai_detect_ret.size();
                    for (auto& obj : openset_detect_ret) {
                        ai_detect_ret.push_back(obj);

                        image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                            ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[base_idx].DetLefttopX =
                            obj.Bounding_box.x;
                        image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                            ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[base_idx].DetLefttopY =
                            obj.Bounding_box.y;
                        image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                            ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[base_idx].DetWidth =
                            obj.Bounding_box.width;
                        image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                            ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[base_idx].DetHeight =
                            obj.Bounding_box.height;
                        image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                            ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[base_idx].DetTGTclass =
                            obj.cls;
                        image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                            ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[base_idx].TgtConfidence =
                            obj.confidence * 100;
                        ++base_idx;
                    }
                    if (!openset_detect_ret.empty()) {
                        image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
                            ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = (int)ai_detect_ret.size();
                    }
                }
#endif // ENABLE_OPENSET_DETECTION
	#endif
	#ifdef ENABLE_AI
					// 直接路径：AI检测结果绕过元数据链路直接写入assembly
					{
						std::lock_guard<std::mutex> lock(_pushers_mutex);
						if (_pusher && _pusher->_pusher) {
							_pusher->_pusher->updateAiDetectInfo(
								image->meta_data.meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p);
						}
					}
	#endif
	#ifdef ENABLE_AR
					ar_future.get();
	#endif
					if(!_loop_run_new)
						return;
#ifndef ENABLE_AIRBORNE
					if (_is_image_stable_on) {
						pts_temp= image->meta_data.pts;
						_meta_data_cache[pts_temp] = image->meta_data;
						if (!is_stable_geted) {
							continue;
						}
					}

					if (_is_image_stable_on && !M.empty()) {
						auto meta_data_it = _meta_data_cache.find(image->meta_data.pts);
						if (meta_data_it != _meta_data_cache.end()) {
							image->meta_data = meta_data_it->second;
							_meta_data_cache.erase(meta_data_it->first);
						}
						else {
							image->meta_data.meta_data_valid = false;
						}

						std::promise<void> tracking_transform_promise{};
						std::promise<void> ar_transform_promise{};
						std::promise<void> ai_transform_promise{};

						auto tracking_transform_future = tracking_transform_promise.get_future();
						auto ar_transform_future = ar_transform_promise.get_future();
						auto ai_transform_future = ai_transform_promise.get_future();

						ThreadPool::defaultPool().start(
							[this, &image, &M, &tracking_transform_promise]() {
							trackingCoordTransform(image, M);
							tracking_transform_promise.set_value();
						});

						if (_is_ar_on || _is_enhanced_ar_on) {
							ThreadPool::defaultPool().start(
								[this, &image, &M, &ar_transform_promise]() {
								arCoordTransform(image, M);
								ar_transform_promise.set_value();
							});
						}
						else {
							ar_transform_promise.set_value();
						}
						
						if (_is_ai_on) {
							ThreadPool::defaultPool().start(
								[this, &image, &M, &ai_transform_promise]() {
								aiCoordTransform(image, M);
								ai_transform_promise.set_value();
							});
						}
						else {
							ai_transform_promise.set_value();
						}

						tracking_transform_future.get();
						ar_transform_future.get();
						ai_transform_future.get();
						if(!_loop_run_new)
							return;
					}
#endif
	#ifdef ENABLE_AI				
					auto now = std::chrono::system_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _upload_image_time_point).count();
					
					bool track_switch{};
					int frequency{};
					std::string danger_photo_server_url{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
						GET_CONFIG(int, getInt, my_frequency, General::kUploadImageTimeDuration);
						GET_CONFIG(std::string, getString, my_danger_photo_server_url, AI::KDangerPhotoServerUrl);
						track_switch = my_track_switch;
						frequency = my_frequency;
						danger_photo_server_url = my_danger_photo_server_url;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					#ifdef ENABLE_AR
					if(danger_photo_server_url.empty()){
						eap_warning("danger_photo_server_url.empty");
					}
#ifndef ENABLE_DJI_OBJ_RETURN
					if(!_engines->_ar_engine){
						eap_warning("_engines->_ar_engine is null");
					}
#endif

#ifdef ENABLE_DJI_OBJ_RETURN
					if (!ai_detect_ret.empty() && !danger_photo_server_url.empty() && duration > frequency * 1000) {
#else
					if (!ai_detect_ret.empty() && !danger_photo_server_url.empty() && _engines->_ar_engine && duration > frequency * 1000) {
#endif
						_upload_image_time_point = std::chrono::system_clock::now();
						std::lock_guard<std::mutex> lock(_danger_queue_mutex);
						if (_danger_images.size() >= 6){// 隐患回传效率跟不上,就抛掉前面的，缓存最新的
							eap_warning("danger photo push queue size > 5, need to pop");
							auto danger_image = _danger_images.front();
							danger_image->bgr24_image.release();
							_danger_images.pop();
							_danger_ai_ret.pop();
						}

						// std::string base64_encoded{};
						// cv::Mat bgr24_image_cpu;// 是否需要初始化
						// try {
						// 	image->width = image->bgr24_image.cols;
						// 	image->height = image->bgr24_image.rows;
						// 	bgr24_image_cpu.create(image->bgr24_image.size(), image->bgr24_image.type()); // 初始化 cpuImage
						// 	image->bgr24_image.download(bgr24_image_cpu);
						// 	std::vector<uchar> buffer;
						// 	cv::imencode(".jpg", bgr24_image_cpu, buffer, { cv::IMWRITE_JPEG_QUALITY, 80 });
						// 	base64_encoded = "data:image/jpg;base64," + encodeBase64({ buffer.begin(), buffer.end() });
						// 	bgr24_image_cpu.release();
						// }
						// catch (const std::exception& e) {
						// 	bgr24_image_cpu.release();
						// 	eap_error(std::string(e.what()));
						// 	//continue;
						// }

						CodecImagePtr danger_image = CodecImagePtr(new CodecImage());
						danger_image->bgr24_image = image->bgr24_image.clone(); // 深拷贝
						danger_image->meta_data = image->meta_data;
						_danger_images.push(danger_image);
						_danger_ai_ret.push(ai_detect_ret);
						_danger_queue_cv.notify_all();

						// image->base64_encoded = base64_encoded;
						// _danger_images.push(image);
						// _danger_ai_ret.push(ai_detect_ret);
						// _danger_queue_cv.notify_all();
					}
					#endif
	#endif // !_WIN32

	#ifndef ENABLE_AIRBORNE
					std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
					ThreadPool::defaultPool().start([this, image, this_weak_ptr]()
					{
						auto this_shared_ptr = this_weak_ptr.lock();
						if (!this_shared_ptr) {
							return;
						}

						std::lock_guard<std::mutex> lock_encoder(_encoder_mutex);
						if (_encoder) {
							_last_metadata = image->meta_data.meta_data_basic;
							_encoder->updateFrame(*image);//编码太快，现在加了AR计算耗时太多？导致GPU显存上涨
						}
					});
	#else

					if(_enable_encode)
					{
							std::weak_ptr<DispatchTaskImpl> this_weak_ptr = weak_from_this();
							ThreadPool::defaultPool().start([this, image, this_weak_ptr]()
							{
								auto this_shared_ptr = this_weak_ptr.lock();
								if (!this_shared_ptr) {
									return;
								}

								std::lock_guard<std::mutex> lock_encoder(_encoder_mutex);
								if (_encoder) {
									_last_metadata = image->meta_data.meta_data_basic;
									_encoder->updateFrame(*image);//编码太快，现在加了AR计算耗时太多？导致GPU显存上涨
								}
							});
					}
					else{
							Packet packet{};
							packet.setArPixelLines(image->meta_data.pixel_lines);
							packet.setArPixelPoints(image->meta_data.pixel_points);
							packet.setMetaDataBasic(&image->meta_data.meta_data_basic);
							packet.metaDataValid() = true;
							packet.setArValidPointIndex(image->meta_data.ar_valid_point_index);
							packet.setArVectorFile(image->meta_data.ar_vector_file);
							packet.setArMarkInfos(image->meta_data.ar_mark_info);
							packet.setAiHeatmapInfos(image->meta_data.ai_heatmap_info);
							packet.setCurrentTime(image->meta_data.current_time);
					#ifdef ENABLE_AR
							packet.setArPixelWarningL1s(image->meta_data.getArPixelWarningL1s());
							packet.setArPixelWarningL2s(image->meta_data.getArPixelWarningL2s());
					#endif
							//国产化板卡上不主动释放有内存泄漏，nx上不会，这儿释放了快照那儿没法用
							/*image->bgr24_image.release();
							image->bgr32_image.release();*/
							std::lock_guard<std::mutex> _bearer_meta_data_packet_lock(_bearer_meta_data_packet_q_mutex);
							_bearer_meta_data_packet_q.push(packet);
					}

	#endif // ENABLE_AIRBORNE
					
					if(_is_update_func){
						if(!_update_func_err_desc.empty()){
							_update_func_err_desc += std::string("update failed");
						}

						std::string id_temp = _id;
						NoticeCenter::Instance()->getCenter().postNotification(new FunctionUpdatedNotice(
						std::string(id_temp), _update_func_result, _update_func_err_desc, _func_mask));

						_is_update_func = false;
						_update_func_result = true;
						_update_func_err_desc.clear();
					}
				}

			});
#endif // ENABLE_GPU

#ifdef ENABLE_AIRBORNE
			// 此线程用来获取nx的一些状态值，比如：CPU温度、sdcard状态
			_loop_thread_get_cpu_temp = std::thread([this]() {
				_loop_thread_get_cpu_temp_run.store(_init_parameter.need_decode);
				for(;_loop_thread_get_cpu_temp_run;) {
					// CPU温度
					int cpu_temp = getCpuTemperature();
					if(cpu_temp > 82 && _ai_status){// 暂时设置为超过70°就关闭AI相关；目前森防项目仅提示温度情况
						eap_warning_printf("cpu_temp = %d, >82°", cpu_temp);
						// eap_warning_printf("cpu_temp = %d , stop ai", cpu_temp);
						// _is_ai_on.store(false);
						// _is_ai_assist_track_on.store(false);
					}else if(cpu_temp <= 82 && _ai_status){
						eap_warning_printf("cpu_temp = %d , <=82°", cpu_temp);
						// eap_warning_printf("cpu_temp = %d , start ai", cpu_temp);
						// _is_ai_on.store(true);
						// _is_ai_assist_track_on.store(true);
						// _is_update_func.store(true);
					}
					// sdcard 挂载状态
					std::string dev = "/dev/" + device_name;
					bool sdcard_status = _linux_storage_dev_manage->checkMountStatus(dev, mount_point);
					if(sdcard_status){
						_sd_flag = 0;
					}
					else{
						_sd_flag = 1;
					}
					// sdcard内存
					int used_size{};
					int free_size{};
					int total_size{};
					_linux_storage_dev_manage->getMemoryCapacityStatus(mount_point, used_size, free_size, total_size);
					_sd_memory = free_size / 1024  * 10; // 地面端播放器拿到会除以10					

					Poco::JSON::Array to_guid_arr;
					to_guid_arr.add("{DCC1BA68-6C63-46EB-ADEF-7B05F95F2F30}");
					Poco::JSON::Object res_json, replay_recode_json, recode_json, data_json;
					data_json.set("cpu_temp", cpu_temp);
					data_json.set("mount_point", mount_point);
					data_json.set("sd_flag", _sd_flag.load());
					data_json.set("used_size", used_size);
					data_json.set("free_size", free_size);
					data_json.set("total_size", total_size);
					data_json.set("sd_memory", _sd_memory.load());
					recode_json.set("data", data_json);
					recode_json.set("type", "request");
					recode_json.set("code", 200);
					replay_recode_json.set("record", recode_json);
					replay_recode_json.set("topic", "/index/api/sma/smaDeviceStatus");
					replay_recode_json.set("plan_id", _pilot_id);
					replay_recode_json.set("type", "mediaservice");
					auto now = std::chrono::system_clock::now();
					auto duration = now.time_since_epoch();
					auto ms_time_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
					replay_recode_json.set("time_stamp", ms_time_stamp);
					res_json.set("data", replay_recode_json);
					res_json.set("guid", pluginGuid());
					res_json.set("seq", ++_seq);
					res_json.set("to", to_guid_arr);
					res_json.set("api", "sendGimbalControlCmd247");
					res_json.set("msg_id", (int)SmaMsgId::DeviceMsgID);
					if(_device_infos_callback)
						_device_infos_callback(jsonToString(res_json));
					std::this_thread::sleep_for(std::chrono::milliseconds(60000));
				}
			});

			if(!_enable_encode)
			{
				_loop_thread_main = std::thread([this]() {
					// 用来做AI、AR时，避免效率不够造成送packet堵塞
					_loop_thread_main_run.store(_init_parameter.need_decode);
					for(; _loop_thread_main_run;)
					{
						std::unique_lock<std::mutex> lock(_wait_meta_data_packet_q_mutex);
						if(_wait_meta_data_packet_q.empty()){
							_wait_meta_data_packet_q_cv.wait(lock, [this](){
								return !_wait_meta_data_packet_q.empty() || !_loop_thread_main_run;
							});
							if(!_loop_thread_main_run){
								break;
							}
						}
						if(_wait_meta_data_packet_q.empty()){
							continue;
						}
						
						auto to_push_packet = _wait_meta_data_packet_q.front();
						_wait_meta_data_packet_q.pop();
						lock.unlock();

						AiInfos aiInfos_p{};
						_bearer_meta_data_packet_q_mutex.lock();
						if (!_bearer_meta_data_packet_q.empty()) {
							auto meta_data_packet = _bearer_meta_data_packet_q.front();// 这里会偶现 packet中 _seibuf = out.seibuf 崩溃
							_bearer_meta_data_packet_q.pop();
							_bearer_meta_data_packet_q_mutex.unlock();
							aiInfos_p = meta_data_packet.getMetaDataBasic().GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p;
							/*int64_t meta_data_packet_current_time = meta_data_packet.getCurrentTime();
							int64_t to_push_packet_current_time = to_push_packet.getCurrentTime();
							int64_t diff_value = std::abs(to_push_packet_current_time - meta_data_packet_current_time);
							to_push_packet.setArPixelLines(meta_data_packet.getArPixelLines());
							to_push_packet.setArPixelPoints(meta_data_packet.getArPixelPoints());
							to_push_packet.setMetaDataBasic(&meta_data_packet.getMetaDataBasic());
							to_push_packet.metaDataValid() = true;
							to_push_packet.setArValidPointIndex(meta_data_packet.getArValidPointIndex());
							to_push_packet.setArVectorFile(meta_data_packet.getArVectorFile());
							to_push_packet.setArMarkInfos(meta_data_packet.getArInfos());
							to_push_packet.setAiHeatmapInfos(meta_data_packet.getAiHeatmapInfos());*/
						}
						else {
							_bearer_meta_data_packet_q_mutex.unlock();
						}
						//元数据封装SEI嵌入视频
						if (to_push_packet.metaDataValid()) {
							JoFmvMetaDataBasic meta_data = to_push_packet.getMetaDataBasic();
							meta_data.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p = aiInfos_p;
							to_push_packet.setMetaDataBasic(&meta_data);
							int meta_data_sei_buffer_size{};
							auto meta_data_sei_buffer = _meta_data_processing_after->getSerializedBytesBySetMetaDataBasic(&meta_data, &meta_data_sei_buffer_size);
							auto sei_h264 = MetaDataProcessing::seiDataAssemblyH264(meta_data_sei_buffer.data(), meta_data_sei_buffer.size());
							if (!sei_h264.empty()) {
								ByteArray_t meta_data_sei_buffer_ptr = MakeByteArray(meta_data_sei_buffer_size, meta_data_sei_buffer.data());
								auto new_pkt = VideoExtraDataReplace(to_push_packet, _codec_parameter.codec_id, meta_data_sei_buffer_ptr);
								to_push_packet.set(new_pkt);
								//std::cout << "---+++++++-----sma push packet visual mul: " << meta_data.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.VisualFOVHMul << std::endl;
							}
						}

						// 录像
						if (_is_video_record_on) {
							std::lock_guard<std::mutex> lock(_muxer_mutex);
							if (!_muxer) {						
								eap_information("create muxer muxer muxer muxer ");
								createMuxer();
							}
							if (_muxer) {
								Packet new_packet;
								to_push_packet.copyTo(new_packet);
								_muxer->pushPacket(new_packet);
							}
						}
						else {
							destroyMuxer();
						}

						// 航点动作新增的片段录像，机载端如果需要重新编码，那么应该编码后再录
						if (_is_clip_video_record_on) {
							std::lock_guard<std::mutex> lock(_clip_muxer_mutex);
							if (!_clip_muxer) {						
								eap_information("create clip_muxer clip_muxer clip_muxer clip_muxer ");
								createClipMuxer();
							}
							if (_clip_muxer) {
								Packet new_packet;
								to_push_packet.copyTo(new_packet);
								_clip_muxer->pushPacket(new_packet);
							}
						}
						else {
							destroyClipMuxer();
						}

						pushPacket(to_push_packet);
					}
				});
			}
			// 创建TCP server的线程
			if (_init_parameter.need_decode) {
				_tcpserver_listen_thread_run.store(true);
				_tcpserver_listen_thread = std::thread(&DispatchTaskImpl::tcpServerListen, this);
			}

#endif // ENABLE_AIRBORNE

			// AR 座椅线程
            if (configInstance().getBool(Vehicle::KARSeat)) {
                _udp_socket = UdpSocket::createInstance("192.168.1.8", 7408);

                if (!_ar_seat_thread_run && _udp_socket) {
					_ar_seat_thread = std::make_shared<std::thread>(&DispatchTaskImpl::sendDataToUdp, this);
                    eap_information("create ar seat thread succeed");
                }
            }

			if (_is_demuxer_closed) {
				//start函数还没有结束，但是已经触发了demuxer stop callback，还是抛异常出去删任务
				std::string err_msg = "task demuxer stoped before start finished! id:" + _id;
				eap_information(err_msg);
				throw std::system_error(-1, std::system_category(), err_msg);
				return;
			}
			_is_start_finished = true;
		}

		bool DispatchTaskImpl::openDemuxer(std::string& error_msg) {
			bool ret{ false };
			std::string  eth0Ip = "";
#ifdef ENABLE_AIRBORNE
			try {
				eth0Ip = common::NetWorkChecking::Instance()->getEth0Ip();
			}
			catch (const std::exception& exp) {
				eap_information_printf("getEth0Ip failed, ip: %s, err: %s", eth0Ip, std::string(exp.what()));
			}
#endif
			while (!_is_manual_stoped) {
				try {
					if (_demuxer) {
						if(_init_parameter.pull_url.find("udp") == std::string::npos)
							_demuxer->close(true);
						else
							_demuxer->close();

						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						if (_is_manual_stoped)
							return ret;
						_demuxer->open(_init_parameter.pull_url, std::chrono::milliseconds(3000), eth0Ip);
						eap_information_printf("demuxer open successed, url: %s", _init_parameter.pull_url);
						_is_demuxer_opened = true;
						ret = true;
						_is_demuxer_closed = false;
						break;
					}
				} catch(const std::exception& exp) {
					_is_demuxer_opened = false;
					std::string err_msg = std::string(exp.what());
					error_msg = err_msg;
					NoticeCenter::Instance()->getCenter().postNotification(
						new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::DemuxerOpenFailed, err_msg));
					eap_information_printf("demuxer open failed, msg: %s, url: %s", err_msg, _init_parameter.pull_url);
					if(!_is_pilotcontrol_task || _is_manual_stoped)
						return ret;

					std::this_thread::sleep_for(std::chrono::milliseconds(1500));
					continue;
				}
			}
			eap_information_printf("open demuxer finished! url: %s", _init_parameter.pull_url);
			return ret;
		}

		bool DispatchTaskImpl::openPusher()
        {
			bool ret{ false };
			std::string err_msg{};
			std::string  eth0Ip = "";
#ifdef ENABLE_AIRBORNE
			try {
				eth0Ip = common::NetWorkChecking::Instance()->getEth0Ip();
			}
			catch (const std::exception& exp) {
				eap_information_printf("getEth0Ip failed, ip: %s, err: %s", eth0Ip, std::string(exp.what()));
			}
#endif
			while (!_is_manual_stoped) {
				//机载4、5G情况下，不发udp组播视频流
				if (_airborne_45G) {
					if (_pusher && _pusher->_pusher)
						_pusher->_pusher->close();
					eap_information_printf("enable_airborne_45G, close udp pusher, task id: %s, push url: %s", _id, _push_url);
					break;
				}
				if (_is_demuxer_closed && !_is_start_finished && !_is_pilotcontrol_task) {
					//在start函数结束之前已经收到了demuxer stop callback的回调，停止推流
					eap_information_printf("demuxer closed before start finished, pusher url: %s", _init_parameter.push_url);
					break;
				}
				if (!_is_demuxer_opened) {
					std::this_thread::sleep_for(std::chrono::milliseconds(800));
					eap_information_printf("demuxer not open, so dont open pusher, pusher url: %s", _init_parameter.push_url);
					continue;
				}
				try {
					std::unique_lock<std::mutex> lock(_pushers_mutex);
					if (_pusher && _pusher->_pusher) {
						_pusher->_pusher->close();

						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						if (_is_manual_stoped)
							return ret;
						_pusher->_pusher->open(_id, _init_parameter.push_url, _timebase, _framerate,
							_codec_parameter, std::chrono::milliseconds(5000), eth0Ip);
						eap_information_printf("----pusher open successed, url: %s", _init_parameter.push_url);
						_reopen_pusher_cnt = 0;
						ret = true;
						break;
					}
					else {
						break;
					}
				}
				catch (const std::exception& e) {
					err_msg = std::string(e.what());
					NoticeCenter::Instance()->getCenter().postNotification(
						new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::PusherOpenFailed, err_msg));
					eap_information_printf("----pusher open failed, msg: %s, url: %s", err_msg, _init_parameter.push_url);
					if (!_is_pilotcontrol_task || _is_manual_stoped)
						break;
					std::this_thread::sleep_for(std::chrono::milliseconds(1500));
					continue;
				}
			}
			if (!_is_pilotcontrol_task && !err_msg.empty() && !_is_manual_stoped)
				throw std::system_error(-1, std::system_category(), err_msg);
			eap_information_printf("open pusher finished! url: %s", _init_parameter.push_url);
			return ret;
        }

		void DispatchTaskImpl::openAirPusher()
		{
			while (!_is_manual_stoped) {
				try {
					std::unique_lock<std::mutex> lock(_pushers_air_mutex);
					if (_pusher && _pusher->_air_pusher) {
						_pusher->_air_pusher->close();
						//机载图传情况下，不发流媒体视频流
						if (!_airborne_45G) {
							eap_information_printf("disable_airborne_45G, close pusher, task id: %s, push url: %s", _id, _pusher->_air_url);
							break;
						}
						if (_is_manual_stoped)
							return;
						_pusher->_air_pusher->open(_id, _pusher->_air_url, _timebase, _framerate,
							_codec_parameter, std::chrono::milliseconds(5000));
						eap_information_printf("----air pusher open successed, url: %s", _pusher->_air_url);
						break;
					}
					else {
						break;
					}
				}
				catch (const std::exception& exp) {
					std::string err_msg = std::string(exp.what());
					NoticeCenter::Instance()->getCenter().postNotification(
						new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::PusherOpenFailed, err_msg));
					eap_information_printf("----_air_pusher open failed, msg: %s", err_msg);
					if (!_is_pilotcontrol_task || _is_manual_stoped)
						break;
					std::this_thread::sleep_for(std::chrono::milliseconds(3000));
					continue;
				}
			}
			eap_information("open _air_pusher finished!");
		}

		void DispatchTaskImpl::updateTaskFuncmask(bool is_update_task_file)
		{
			if(is_update_task_file)
				DispatchCenter::Instance()->updateTaskInfo(_id, _func_mask);

			_init_parameter.func_mask = _func_mask;
			std::string visual_guid{};
			std::string infrared_guid{};
			try {
				GET_CONFIG(std::string, getString, my_visual_guid, Media::kVisualGuid);
				GET_CONFIG(std::string, getString, my_infrared_guid, Media::kInfraredGuid);
				visual_guid = my_visual_guid;
				infrared_guid = my_infrared_guid;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kVisualGuid throw exception: %s", e.what());
			}
			if (_id == visual_guid) {
				eap::configInstance().setInt(Media::kVisualFuncmask, _func_mask);
				eap::saveConfig();
			}
			else if (_id == infrared_guid) {
				eap::configInstance().setInt(Media::kInfraredFuncmask, _func_mask);
				eap::saveConfig();
			}
		}

        void DispatchTaskImpl::stop()
		{
			eap_warning_printf("task impl manal close, id: %s", _id);
			_is_stop_reactor_loop.store(true);
			_is_manual_stoped = true;
			_loop_run_new = false;
			_danger_photo_loop_thread_run.store(false);
			_loop_thread_main_run.store(false);
			_ar_seat_thread_run = false;
			_record_loop_thread_run.store(false);
			_open_timeout = std::chrono::milliseconds(0);
			if (_reactor_thread_ptr && _reactor_thread_ptr->joinable()) {
				_reactor_thread_ptr->join();
				eap_information_printf("reactor_thread join successed, id: %s", _id);
			}
			_reactor_thread_looptimes = 0;
			if (_danger_photo_loop_thread.joinable()) {
				_danger_queue_cv.notify_all();
				_danger_photo_loop_thread.join();
				eap_information_printf("danger_photo_thread join successed, id: %s", _id);
			}
			
			if (_record_loop_thread.joinable()) {
				_record_queue_cv.notify_all();
				_record_loop_thread.join();
				eap_information_printf("_record_loop_thread join successed, id: %s", _id);
			}
			
			if(_loop_thread_new.joinable()) {
				eap_information_printf("loop_thread join start, id: %s", _id);
				_decoded_images_cv.notify_all();
				_loop_thread_new.join();
				eap_information_printf("loop_thread join successed, id: %s", _id);
			}

			if (_loop_thread_main.joinable()) {
				_wait_meta_data_packet_q_cv.notify_all();
				_loop_thread_main.join();
			}

            if (_ar_seat_thread && _ar_seat_thread->joinable()) {
				_ar_seat_thread->join();
				_ar_seat_thread.reset();
			}
			eap_information_printf("all thread join successed, id: %s", _id);
#ifdef ENABLE_AIRBORNE
			if(_loop_thread_get_cpu_temp.joinable()){
				_loop_thread_get_cpu_temp_run.store(false);
				_loop_thread_get_cpu_temp.join();
			}
			if (_init_parameter.need_decode) {
				_tcpserver_listen_thread_run.store(false);
				if (_tcpserver_listen_thread.joinable()) {
					_tcpserver_listen_thread.join();
					eap_information_printf("_tcpserver_listen_thread succesd, id: %s", _id);
				}
				if (_tcp_server) {
					_tcp_server->stop();
					if (_tcpserver_io_run_thread.joinable()) {
						_tcpserver_io_run_thread.join();
					}
					_tcp_server.reset();
				}
				{
					std::lock_guard<std::mutex> lock(_tcp_send_frame_mutex);
					if (_tcp_send_frame_q.size() > 0) {
						while (!_tcp_send_frame_q.empty()) {
							auto frame = _tcp_send_frame_q.front();
							av_frame_free(&frame);
							_tcp_send_frame_q.pop();

						}
						while (!_tcp_send_sei_q.empty()) {
							_tcp_send_sei_q.pop();
						}
					}
				}
			}

#endif // ENABLE_AIRBORNE

			destroyDemuxer();
			
			destroyDecoder();		

			destroyMuxer();

			destroyPusher();

#ifdef ENABLE_AIRBORNE
#ifdef ENABLE_GPU
//机载端，打开了编码才创建编码器
	if(_enable_encode)
	{
		destroyEncoder();
	}
#endif
#else
//非机载端，直接创建编码器
#ifdef ENABLE_GPU
			destroyEncoder();
#endif
#endif
			if (_meta_data_processing_pret) {
				_meta_data_processing_pret.reset();
			}
			if (_meta_data_processing_after) {
				_meta_data_processing_after.reset();
			}
#ifdef ENABLE_AI
			while (!_danger_images.empty()) {
				auto danger_image = _danger_images.front();
				danger_image->bgr24_image.release();
				_danger_images.pop();
			}
			destroyAIEngine();
#endif

#ifdef ENABLE_AR
			destroyAREngine();
#endif

#ifdef ENABLE_AIRBORNE
			if (_init_parameter.need_decode) {
				_linux_storage_dev_manage.reset();
#ifdef ENABLE_PIO
				if(_pio_pubilsh){
					_pio_pubilsh.reset();
				}
#endif	
				FreeDeviceMem(&pIn420Dev);
				FreeDeviceMem(&pOutBgrDev);
			}
#endif			
			eap_warning_printf("dispatch task stoped, id: %s, pull url: %s, push url: %s", _task_id,
			 	_init_parameter.pull_url, _init_parameter.push_url);
		}

		void DispatchTaskImpl::updateFuncMask(int func_mask, std::string ar_camera_config, std::string ar_vector_file)
		{
			if (!ar_camera_config.empty() && !ar_vector_file.empty()) {
				std::lock_guard<std::mutex> update_func_lock(_ar_related_mutex);
#ifdef ENABLE_AR
				//arengine config和kml文件有区别时才删除引擎
				if(_engines->_ar_engine && (_ar_camera_config != ar_camera_config || _ar_vector_file != ar_vector_file))
					destroyAREngine();
#endif
				_ar_camera_config = ar_camera_config;
				_ar_vector_file = ar_vector_file;
			}
			if (!ar_camera_config.empty()) {
				_ar_camera_config = ar_camera_config;
			}
			_init_parameter.ar_vector_file = _ar_vector_file;
			_init_parameter.ar_camera_config = _ar_camera_config;
			_init_parameter.func_mask = func_mask;
			_func_mask = func_mask;
			updateFuncmaskL();
			_is_update_func.store(true);
			updateTaskFuncmask();
		}

		void DispatchTaskImpl::clipSnapShotParam(int time_count)
		{
			_clip_video_record_time_count = time_count;
		}

#ifdef ENABLE_AIRBORNE
		void DispatchTaskImpl::receivePilotData(std::string param_str)
		{
			Poco::JSON::Parser parser;
			Poco::Dynamic::Var result = parser.parse(param_str);
			Poco::JSON::Object all_params = *(result.extract<Poco::JSON::Object::Ptr>());
			Poco::JSON::Object params = all_params.has("PilotData")? *(all_params.getObject("PilotData")): Poco::JSON::Object();

			PilotData pilot_data{};

			pilot_data.UnixTimeStamp = params.has("UnixTimeStamp") ? params.getValue<int64_t>("UnixTimeStamp") : 0;
			pilot_data.Euler0 = params.has("Euler0") ? params.getValue<int64_t>("Euler0") : 0;
			pilot_data.Euler1 = params.has("Euler1") ? params.getValue<int64_t>("Euler1") : 0;
			pilot_data.Euler2 = params.has("Euler2") ? params.getValue<int64_t>("Euler2") : 0;
			pilot_data.SeroveCmd = params.has("SeroveCmd") ? params.getValue<int64_t>("SeroveCmd") : 0;
			pilot_data.OptoSensorCmd = params.has("OptoSensorCmd") ? params.getValue<int64_t>("OptoSensorCmd") : 0;

			_opt_sensor_cmd = pilot_data.OptoSensorCmd;
			// if(_opt_sensor_cmd != 0){
			// 	eap_warning_printf("_opt_sensor_cmd is %d", (int)_opt_sensor_cmd);
			// }
			switch (_opt_sensor_cmd)
			{
			case 0x12: // 快照
				eap_warning("snap shot on ~~~~~~~");
				_is_snap_shot_on.store(true);
				_snap_num++;
				saveSnapShot();
				break;
			case 0x28: // 开始录像
				eap_warning("uav take off, video record on ~~~~~~~");
				_is_video_record_on.store(true);
				break;
			case 0x29: // 停止录像
				eap_warning("uav land, video record off ~~~~~~~");
				_is_video_record_on.store(false);
				break;
			case 0x0b: // 录像开关，乒乓操作
				if(_is_video_record_on){
					eap_warning("video record off ~~~~~~~");
					_is_video_record_on.store(false);
				}else{
					eap_warning("video record on ~~~~~~~");
					_is_video_record_on.store(true);
				}
				break;
			case 0x0C: // 稳像开关，乒乓操作
				if(_is_stable_on){
					eap_warning("stable off ~~~~~~~");
					_is_stable_on.store(false);
					// _is_image_stable_on = false;
				}else{
					eap_warning("stable on ~~~~~~~");
					_is_stable_on.store(true);
					// _is_image_stable_on.store(true);
					_is_update_func.store(true);
				}
				break;
			case 0x4d: // AI开关，乒乓操作
				if(_is_ai_on){
					eap_warning("pilot:ai detect off ~~~~~~~");
					_is_ai_on.store(false);
					_ai_status.store(false);
				}else{
					eap_warning("pilot:ai detect on ~~~~~~~");
					_is_ai_on.store(true);
					_ai_status.store(true);
					_is_update_func.store(true);
				}
				break;
			case 0x1d: // AI辅助跟踪(在没有开AI的情况下需要先打开AI，关闭时如果是需要AI检测的就不能关AI)，乒乓操作
				if(_is_ai_assist_track_on){
					eap_warning("ai assit track off ~~~~~~~");
					_is_ai_assist_track_on.store(false);
				}else{
					eap_warning("ai assit track on ~~~~~~~");
					_is_ai_assist_track_on.store(true);
				}
				break;
			default:
				break;
			}

			pilot_data.CamaraPara = params.has("CamaraPara") ? params.getValue<int64_t>("CamaraPara") : 0;
			pilot_data.ServoCmd0 = params.has("ServoCmd0") ? params.getValue<int64_t>("ServoCmd0") : 0;
			pilot_data.ServoCmd1 = params.has("ServoCmd1") ? params.getValue<int64_t>("ServoCmd1") : 0;
			pilot_data.year = params.has("year") ? params.getValue<int64_t>("year") : 0;
			pilot_data.month = params.has("month") ? params.getValue<int64_t>("month") : 0;
			pilot_data.day = params.has("day") ? params.getValue<int64_t>("day") : 0;
			pilot_data.hour = params.has("hour") ? params.getValue<int64_t>("hour") : 0;
			pilot_data.minute = params.has("minute") ? params.getValue<int64_t>("minute") : 0;
			pilot_data.second = params.has("second") ? params.getValue<int64_t>("second") : 0;
			// eap_warning_printf("pilot_data.year: %d, pilot_data.month: %d, pilot_data.day: %d , pilot_data.hour: %d, pilot_data.minute: %d, pilot_data.second: %d"
			// , (int)pilot_data.year, (int)pilot_data.month, (int)pilot_data.day, (int)pilot_data.hour, (int)pilot_data.minute, (int)pilot_data.second);
			pilot_data.lat = params.has("lat") ? params.getValue<int64_t>("lat") : 0;
			pilot_data.lon = params.has("lon") ? params.getValue<int64_t>("lon") : 0;
			pilot_data.HMSL = params.has("HMSL") ? params.getValue<int64_t>("HMSL") : 0;
			pilot_data.VGnd = params.has("VGnd") ? params.getValue<int64_t>("VGnd") : 0;
			pilot_data.Tas = params.has("Tas") ? params.getValue<int64_t>("Tas") : 0;
			pilot_data.pdop = params.has("pdop") ? params.getValue<int64_t>("pdop") : 0;
			pilot_data.numSV = params.has("numSV") ? params.getValue<int64_t>("numSV") : 0;
			pilot_data.TGTlat = params.has("TGTlat") ? params.getValue<int64_t>("TGTlat") : 0;
			pilot_data.TGTlon = params.has("TGTlon") ? params.getValue<int64_t>("TGTlon") : 0;
			pilot_data.TGTHMSL = params.has("TGTHMSL") ? params.getValue<int64_t>("TGTHMSL") : 0;

			pilot_data.ModeState = params.has("ModeState") ? params.getValue<int64_t>("ModeState") : 0;
			pilot_data.SlantR = params.has("SlantR") ? params.getValue<int64_t>("SlantR") : 0;
			pilot_data.RPM = params.has("RPM") ? params.getValue<int64_t>("RPM") : 0;
			pilot_data.Vnorth = params.has("Vnorth") ? params.getValue<int64_t>("Vnorth") : 0;
			pilot_data.Veast = params.has("Veast") ? params.getValue<int64_t>("Veast") : 0;
			{
				int64_t temp_vdown = params.has("Vdown") ? params.getValue<int64_t>("Vdown") : 0;
				pilot_data.Vdown = temp_vdown * -1;
			}
			pilot_data.ThrottleCmd = params.has("ThrottleCmd") ? params.getValue<int64_t>("ThrottleCmd") : 0;
			pilot_data.DefectFlag = params.has("DefectFlag") ? params.getValue<int64_t>("DefectFlag") : 0;
			pilot_data.MPowerA = params.has("MPowerA") ? params.getValue<int64_t>("MPowerA") : 0;
			pilot_data.MpowerV = params.has("MpowerV") ? params.getValue<int64_t>("MpowerV") : 0;
			pilot_data.DemSearchLat = params.has("DemSearchLat") ? params.getValue<int64_t>("DemSearchLat") : 0;
			pilot_data.DemSearchLon = params.has("DemSearchLon") ? params.getValue<int64_t>("DemSearchLon") : 0;
			pilot_data.DemSearchAlt = params.has("DemSearchAlt") ? params.getValue<int64_t>("DemSearchAlt") : 0;
			pilot_data.DisFromHome = params.has("DisFromHome") ? params.getValue<int64_t>("DisFromHome") : 0;
			pilot_data.HeadFromHome = params.has("HeadFromHome") ? params.getValue<int64_t>("HeadFromHome") : 0;
			pilot_data.TargetVelocity = params.has("TargetVelocity") ? params.getValue<int64_t>("TargetVelocity") : 0;
			pilot_data.TargetHeading = params.has("TargetHeading") ? params.getValue<int64_t>("TargetHeading") : 0;
			pilot_data.ScoutTask = params.has("ScoutTask") ? params.getValue<int64_t>("ScoutTask") : 0;
			pilot_data.apmodestates = params.has("apmodestates") ? params.getValue<int64_t>("apmodestates") : 0;
			pilot_data.TasCmd = params.has("TasCmd") ? params.getValue<int64_t>("TasCmd") : 0;
			pilot_data.HeightCmd = params.has("HeightCmd") ? params.getValue<int64_t>("HeightCmd") : 0;
			pilot_data.WypNum = params.has("WypNum") ? params.getValue<int64_t>("WypNum") : 0;
			pilot_data.VeclType = params.has("VeclType") ? params.getValue<int64_t>("VeclType") : 0;
			pilot_data.GimbalDeployCmd = params.has("GimbalDeployCmd") ? params.getValue<int64_t>("GimbalDeployCmd") : 0;
			pilot_data.HMSLCM = params.has("HMSLCM") ? params.getValue<int64_t>("HMSLCM") : 0;
			pilot_data.Wyplat = params.has("Wyplat") ? params.getValue<int64_t>("Wyplat") : 0;
			pilot_data.Wyplon = params.has("Wyplon") ? params.getValue<int64_t>("Wyplon") : 0;
			pilot_data.WypHMSL = params.has("WypHMSL") ? params.getValue<int64_t>("WypHMSL") : 0;
			pilot_data.HMSLDEM = params.has("HMSLDEM") ? params.getValue<int64_t>("HMSLDEM") : 0;
			_pilot_id = params.has("PilotId") ? params.getValue<std::string>("PilotId") : "";

			std::lock_guard<std::mutex> receive_pilot_data_lock(_receive_pilot_data_mutex);
			_pilot_data = pilot_data;
			//while (_pilot_data_map.size() > 5) {
			//	_pilot_data_map.erase(_pilot_data_map.begin());
			//}
			//_pilot_data_map[pilot_data.UnixTimeStamp] = pilot_data;//map中默认按键值从小到大排序
		}
#endif

#ifdef ENABLE_AIRBORNE
		void DispatchTaskImpl::receivePayloadData(std::string param_str)
		{
			Poco::JSON::Parser parser;
			Poco::Dynamic::Var result = parser.parse(param_str);
			Poco::JSON::Object all_params = *(result.extract<Poco::JSON::Object::Ptr>());
			Poco::JSON::Object params = all_params.has("PayloadData") ? *(all_params.getObject("PayloadData")) : Poco::JSON::Object();

			GimbalData gimbal_data{};
			LaerDataProcMsg laer_data_procmsg{};
			IrThermometerBack ir_thermometer_back{};
			PayloadData payload_data{};
			
			gimbal_data.pan = params.has("pan") ? (int32_t)(params.getValue<double>("pan") * 1e4): 0;
			gimbal_data.Tilt = params.has("Tilt") ? (int32_t)(params.getValue<double>("Tilt") * 1e4): 0;
			gimbal_data.ViewAngle = params.has("ViewAngle") ? params.getValue<uint32_t>("ViewAngle"): 0; //未 * 1e4，结构体里面没有这个字段
			gimbal_data.Framepan = params.has("Framepan") ? (int32_t)(params.getValue<double>("Framepan") * 1e4): 0;
			gimbal_data.FrameTilt = params.has("FrameTilt") ? (int32_t)(params.getValue<double>("FrameTilt") * 1e4): 0;
			gimbal_data.DeltaPan = params.has("DeltaPan") ? params.getValue<int32_t>("DeltaPan"): 0; //未 * 1e4，结构体里面没有这个字段
			gimbal_data.DeltaTilt = params.has("DeltaTilt") ? params.getValue<int32_t>("DeltaTilt"): 0; //未 * 1e4，结构体里面没有这个字段
			gimbal_data.GMPower = params.has("GMPower") ? params.getValue<uint32_t>("GMPower") : 0;//未 * 50，结构体里面没有这个字段
			gimbal_data.DeltaTime = params.has("DeltaTime") ? params.getValue<uint32_t>("DeltaTime") : 0;
			gimbal_data.roll = params.has("roll") ? (int32_t)(params.getValue<double>("roll") * 1e4): 0;
			gimbal_data.FrameRoll = params.has("FrameRoll") ? (int32_t)(params.getValue<double>("FrameRoll") * 1e4): 0;
			gimbal_data.visual_GR = params.has("visual_GR") ? (int32_t)(params.getValue<double>("visual_GR") * 1e2): 0;
			gimbal_data.visual_ZoomAD = params.has("visual_ZoomAD") ? params.getValue<uint32_t>("visual_ZoomAD") : 0;
			gimbal_data.visual_FocusAD = params.has("visual_FocusAD") ? params.getValue<uint32_t>("visual_FocusAD") : 0;
			gimbal_data.IR_GR = params.has("IR_GR") ? (int32_t)(params.getValue<double>("IR_GR") * 1e2): 0;
			gimbal_data.IR_ZoomAD = params.has("IR_ZoomAD") ? params.getValue<uint32_t>("IR_ZoomAD") : 0;
			gimbal_data.IR_FocusAD = params.has("IR_FocusAD") ? params.getValue<uint32_t>("IR_FocusAD") : 0;

			payload_data.TxData_p.VersionNum = params.has("VersionNum") ? params.getValue<uint32_t>("VersionNum") : 0;
			payload_data.TxData_p.SeroveCmd = params.has("SeroveCmd") ? params.getValue<uint32_t>("SeroveCmd") : 0;
			payload_data.TxData_p.SeroveInit = params.has("SeroveInit") ? params.getValue<uint32_t>("SeroveInit") : 0;
			payload_data.TxData_p.VisualFOVH = params.has("VisualFOVH") ? (uint32_t)(params.getValue<double>("VisualFOVH") * 1e4) : 0;
			payload_data.TxData_p.VisualFOVV = params.has("VisualFOVV") ? params.getValue<uint32_t>("VisualFOVV"): 0;//未 * 1e4，结构体里面没有这个字段
			payload_data.TxData_p.InfaredFOVH = params.has("InfaredFOVH") ? (uint32_t)(params.getValue<double>("InfaredFOVH") * 1e4): 0;
			payload_data.TxData_p.InfaredFOVV = params.has("InfaredFOVV") ? params.getValue<uint32_t>("InfaredFOVV"): 0;//未 * 1e4，结构体里面没有这个字段
			payload_data.TxData_p.SearchWidth = params.has("SearchWidth") ? params.getValue<uint32_t>("SearchWidth") : 0;
			payload_data.TxData_p.SearchHeight = params.has("SearchHeight") ? params.getValue<uint32_t>("SearchHeight") : 0;
			{
				int64_t servo_cross_x = params.has("ServoCrossX") ? params.getValue<int64_t>("ServoCrossX") : 0;
				if(servo_cross_x > 1920){
					payload_data.TxData_p.ServoCrossX = 1920;
				}else if(servo_cross_x < 0){					
					payload_data.TxData_p.ServoCrossX = 0;
				}
				else{
					payload_data.TxData_p.ServoCrossX = servo_cross_x;
				}

				int64_t servo_cross_y = params.has("ServoCrossY") ? params.getValue<int64_t>("ServoCrossY") : 0;
				if(servo_cross_y > 1080){
					payload_data.TxData_p.ServoCrossY = 1080;
				}else if(servo_cross_y < 0){					
					payload_data.TxData_p.ServoCrossY = 0;
				}
				else{
					payload_data.TxData_p.ServoCrossY = servo_cross_y;
				}
			}
			payload_data.TxData_p.ServoCrossWidth = params.has("ServoCrossWidth") ? params.getValue<uint32_t>("ServoCrossWidth") : 0;
			payload_data.TxData_p.ServoCrossheight = params.has("ServoCrossheight") ? params.getValue<uint32_t>("ServoCrossheight") : 0;
			payload_data.TxData_p.TrackLefttopX = params.has("TrackLefttopX") ? params.getValue<uint32_t>("TrackLefttopX") : 0;
			payload_data.TxData_p.TrackLefttopY = params.has("TrackLefttopY") ? params.getValue<uint32_t>("TrackLefttopY") : 0;
			payload_data.TxData_p.TrackWidth = params.has("TrackWidth") ? params.getValue<uint32_t>("TrackWidth") : 0;
			payload_data.TxData_p.TrackHeight = params.has("TrackHeight") ? params.getValue<uint32_t>("TrackHeight") : 0;
						
			payload_data.TxData_p.SDMemory = _sd_memory;

			// payload_data.TxData_p.TimeZoneNum = params.has("TimeZoneNum") ? params.getValue<int32_t>("TimeZoneNum") : 0;
			payload_data.TxData_p.SDGainVal = params.has("SDGainVal") ? params.getValue<uint32_t>("SDGainVal") : 0;//未 * 10，结构体里面没有这个字段

			// payload_data.TxData_p.SnapNum = params.has("SnapNum") ? params.getValue<uint32_t>("SnapNum") : 0;
			payload_data.TxData_p.SnapNum = _snap_num;


			payload_data.TxData_p.OSDFlag = params.has("OSDFlag") ? params.getValue<uint32_t>("OSDFlag") : 0;
			payload_data.TxData_p.TrackFlag = params.has("TrackFlag") ? params.getValue<uint32_t>("TrackFlag") : 0;
			payload_data.TxData_p.StableFlag = params.has("StableFlag") ? params.getValue<uint32_t>("StableFlag") : 0;
			payload_data.TxData_p.ImageAdjust = params.has("ImageAdjust") ? params.getValue<uint32_t>("ImageAdjust") : 0;
			payload_data.TxData_p.SDFlag = _sd_flag;
			// payload_data.TxData_p.RecordFlag = params.has("RecordFlag") ? params.getValue<uint32_t>("RecordFlag") : 0;
			if(_is_video_record_on){
				payload_data.TxData_p.RecordFlag = 1;
			}
			else{
				payload_data.TxData_p.RecordFlag = 0;
			}			 

			payload_data.TxData_p.FlyFlag = params.has("FlyFlag") ? params.getValue<uint32_t>("FlyFlag") : 0;
			payload_data.TxData_p.MTI = params.has("MTI") ? params.getValue<uint32_t>("MTI") : 0;

			// payload_data.TxData_p.AI_R = params.has("AI_R") ? params.getValue<uint32_t>("AI_R") : 0;
			if(_is_ai_assist_track_on){
				payload_data.TxData_p.AI_R = 1;
			}
			else{
				payload_data.TxData_p.AI_R = 0;
			}

			payload_data.TxData_p.IM = params.has("IM") ? params.getValue<uint32_t>("IM") : 0;
			payload_data.TxData_p.W_or_B = params.has("W_or_B") ? params.getValue<uint32_t>("W_or_B") : 0;
			payload_data.TxData_p.IR = params.has("IR") ? params.getValue<uint32_t>("IR") : 0;
			payload_data.TxData_p.HDvsSD = params.has("HDvsSD") ? params.getValue<uint32_t>("HDvsSD") : 0;
			payload_data.TxData_p.CarTrack = params.has("CarTrack") ? params.getValue<uint32_t>("CarTrack") : 0;
			payload_data.TxData_p.TrackClass = params.has("TrackClass") ? params.getValue<uint32_t>("TrackClass") : 0;
			payload_data.TxData_p.FlySeconds = params.has("FlySeconds") ? params.getValue<uint32_t>("FlySeconds") : 0;
			payload_data.TxData_p.Encryption = params.has("Encryption") ? params.getValue<uint32_t>("Encryption") : 0;
			payload_data.TxData_p.DetectionFlag = params.has("DetectionFlag") ? params.getValue<uint32_t>("DetectionFlag") : 0;
			payload_data.TxData_p.DetNum = params.has("DetNum") ? params.getValue<uint32_t>("DetNum") : 0;
			payload_data.TxData_p.FovLock = params.has("FovLock") ? params.getValue<uint32_t>("FovLock") : 0;
			payload_data.TxData_p.TrackStatus = params.has("TrackStatus") ? params.getValue<uint32_t>("TrackStatus") : 0;
			payload_data.TxData_p.DetRltStatus = params.has("DetRltStatus") ? params.getValue<uint32_t>("DetRltStatus") : 0;
			payload_data.TxData_p.VisualFOVHMul = params.has("VisualFOVHMul") ? (uint32_t)(params.getValue<double>("VisualFOVHMul") * 1e2): 0;
			//std::cout << "---########-----sma receive visual mul: " << payload_data.TxData_p.VisualFOVHMul << std::endl;
			payload_data.TxData_p.InfaredFOVHMul = params.has("InfaredFOVHMul") ? (uint32_t)(params.getValue<double>("InfaredFOVHMul") * 1e2): 0;
			payload_data.TxData_p.AiStatus = params.has("AiStatus") ? params.getValue<uint32_t>("AiStatus") : 0;
			payload_data.TxData_p.PIP = params.has("PIP") ? params.getValue<uint32_t>("PIP") : 0;
			payload_data.TxData_p.ImgStabilize = params.has("ImgStabilize") ? params.getValue<uint32_t>("ImgStabilize") : 0;
			payload_data.TxData_p.ImgDefog = params.has("ImgDefog") ? params.getValue<uint32_t>("ImgDefog") : 0;
			payload_data.TxData_p.Pintu = params.has("Pintu") ? params.getValue<uint32_t>("Pintu") : 0;
			payload_data.TxData_p.DzoomWho = params.has("DzoomWho") ? params.getValue<uint32_t>("DzoomWho") : 0;
			//DzoomWho:[sengfang project] as a switch of AI function, 1:open ai; 0: close ai
#ifdef ENABLE_FIRE_DETECTION
			bool switch_ai=payload_data.TxData_p.DzoomWho;
			// bool switch_ai=1;
			if((!switch_ai)&&_is_ai_on){
				eap_warning("payload:ai detect off ~~~~~~~");
				_is_ai_on.store(false);
				_ai_status.store(false);
			}
			else if(switch_ai&&(!_is_ai_on)){
				eap_warning("payload:ai detect on ~~~~~~~");
				_is_ai_on.store(true);
				_ai_status.store(true);
				_is_update_func.store(true);
			}
#endif
			laer_data_procmsg.LaserStatus = params.has("LaserStatus") ? params.getValue<uint32_t>("LaserStatus") : 0;
			laer_data_procmsg.LaserMeasVal = params.has("LaserMeasVal") ? params.getValue<uint32_t>("LaserMeasVal") : 0;
			laer_data_procmsg.LaserMeasStatus = params.has("LaserMeasStatus") ? params.getValue<uint32_t>("LaserMeasStatus") : 0;
			laer_data_procmsg.Laserlat = params.has("Laserlat") ? (int32_t)(params.getValue<double>("Laserlat") * 1e7): 0;
			laer_data_procmsg.Laserlon = params.has("Laserlon") ? (int32_t)(params.getValue<double>("Laserlon") * 1e7): 0;
			laer_data_procmsg.LaserHMSL = params.has("LaserHMSL") ? params.getValue<int32_t>("LaserHMSL") : 0;
			laer_data_procmsg.LaserRefStatus = params.has("LaserRefStatus") ? params.getValue<uint32_t>("LaserRefStatus") : 0;
			laer_data_procmsg.LaserRefLat = params.has("LaserRefLat") ? (int32_t)(params.getValue<double>("LaserRefLat") * 1e7): 0;
			laer_data_procmsg.LaserRefLon = params.has("LaserRefLon") ? (int32_t)(params.getValue<double>("LaserRefLon") * 1e7): 0;
			laer_data_procmsg.LaserRefHMSL = params.has("LaserRefHMSL") ? params.getValue<int32_t>("LaserRefHMSL") : 0;
			laer_data_procmsg.LaserCurStatus = params.has("LaserCurStatus") ? params.getValue<uint32_t>("LaserCurStatus") : 0;
			laer_data_procmsg.LaserCurLat = params.has("LaserCurLat") ? (int32_t)(params.getValue<double>("LaserCurLat") * 1e7): 0;
			laer_data_procmsg.LaserCurLon = params.has("LaserCurLon") ? (int32_t)(params.getValue<double>("LaserCurLon") * 1e7): 0;
			laer_data_procmsg.LaserCurHMSL = params.has("LaserCurHMSL") ? params.getValue<int32_t>("LaserCurHMSL") : 0;
			laer_data_procmsg.LaserDist = params.has("LaserDist") ? params.getValue<int32_t>("LaserDist") : 0;
			laer_data_procmsg.LaserAngle = params.has("LaserAngle") ? (int32_t)(params.getValue<double>("LaserAngle") * 1e4): 0;
			laer_data_procmsg.LaserModel = params.has("LaserModel") ? params.getValue<uint32_t>("LaserModel") : 0;
			laer_data_procmsg.LaserFreq = params.has("LaserFreq") ? params.getValue<uint32_t>("LaserFreq") : 0;
			laer_data_procmsg.res1 = params.has("res1") ? params.getValue<uint32_t>("res1") : 0;
			laer_data_procmsg.res2 = params.has("res2") ? params.getValue<uint32_t>("res2") : 0;
			
			TargetTemp target_temp{};
			target_temp.centreT = params.has("centreT") ? params.getValue<uint32_t>("centreT") : 0;
			target_temp.height = params.has("height") ? params.getValue<uint32_t>("height") : 0;
			target_temp.id = params.has("id") ? params.getValue<uint32_t>("id") : 0;
			target_temp.irAreaTemp = params.has("irAreaTemp") ? params.getValue<uint32_t>("irAreaTemp") : 0;
			target_temp.irPointTemp = params.has("irPointTemp") ? params.getValue<uint32_t>("irPointTemp") : 0;
			target_temp.maxT = params.has("maxT") ? params.getValue<uint32_t>("maxT") : 0;
			target_temp.meanT = params.has("meanT") ? params.getValue<uint32_t>("meanT") : 0;
			target_temp.minT = params.has("minT") ? params.getValue<uint32_t>("minT") : 0;
			target_temp.pixelX = params.has("pixelX") ? params.getValue<uint32_t>("minT") : 0;
			target_temp.pixelY = params.has("pixelY") ? params.getValue<uint32_t>("pixelY") : 0;
			target_temp.width = params.has("width") ? params.getValue<uint32_t>("width") : 0;
			std::swap(target_temp, ir_thermometer_back.TargetTemp_p);
			ir_thermometer_back.msgid = params.has("msgid") ? params.getValue<uint32_t>("msgid") : 0;
			ir_thermometer_back.Time = params.has("Time") ? params.getValue<uint32_t>("Time") : 0;
			ir_thermometer_back.num = params.has("num") ? params.getValue<uint32_t>("num") : 0;

			payload_data.UnixTimeStamp = params.has("UnixTimeStamp") ? (params.getValue<int64_t>("UnixTimeStamp") / 1000) : 0;

			std::swap(gimbal_data, payload_data.GimbalData_p);
			std::swap(ir_thermometer_back, payload_data.IrThermometerBack_p);
			std::swap(laer_data_procmsg, payload_data.LaerDataProcMsg_p);
			//std::swap(payload_data.TxData_p, payload_data.TxData_p);

			{
				std::lock_guard<std::mutex> receive_payload_data_lock(_receive_payload_data_mutex);
				_payload_data = payload_data;
				//while (_payload_data_map.size() > 3) {
				//	_payload_data_map.erase(_payload_data_map.begin());
				//}

				//_payload_data_map[payload_data.UnixTimeStamp] = payload_data;//map中默认按键值从小到大排序
			}
			
		}
		void DispatchTaskImpl::snapshot(std::string recordNo, int interval, int total_time)
		{
			eap_warning("snap shot on ~~~~~~~");
			_is_snap_shot_on.store(true);
			//如果定时快照 - 这样所有的快照，都存的同一个目录，不管是航点动作触发的，还是其它方式触发的
			if(interval > 0 && total_time > 0){
				eap_information("timeing snap shot on ~~~");
				auto start_time = std::chrono::system_clock::now();
				ThreadPool::defaultPool().start([this, interval, total_time, start_time]() {
					int elapsed_time = 0;
					while (elapsed_time < total_time) { 
						// 这样存在可能的问题：
						// （1）每次按照固定间隔时间拍照，但每张快照存下来的间隔还包括了照片写磁盘的时间，并不是真正的飞行间隔时间；
						// （2）如果写磁盘的时间过久，还会导致每张照片存下来的时间间隔过大
						// （3）而且现在这种写法，快照的存下来的间隔，最小都得是照片写磁盘的耗时+设置的间隔
						_is_snap_shot_on.store(true);
						_snap_num++;
						saveSnapShot();
						std::this_thread::sleep_for(std::chrono::milliseconds(interval));//需要改成interval减去saveSnapShot的耗时
						elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::system_clock::now() - start_time).count();
					}
				});
			}else{
				_snap_num++;
				saveSnapShot();
			}
		}
		void DispatchTaskImpl::tcpServerListen()
		{
			_frame_header.sync0 = 44;
			_frame_header.sync1 = 45;
			_frame_header.dest = 1;
			_frame_header.source = 0;
			_frame_header.msgId = 1;
			_frame_header.length = 777600 + 68;

			// Poco::SharedMemory shmem(
            //     SHARED_MEM_NAME,
            //     SHARED_MEM_SIZE,
            //     Poco::SharedMemory::AM_WRITE,
            //     nullptr,
            //     true
            // );
            // LockFreeSharedData* shared_data = reinterpret_cast<LockFreeSharedData*>(shmem.begin());
            // shared_data->version.store(0, std::memory_order_relaxed); // 初始版本号0（偶数）

			// 连接上来的TCP client都给发
			for (; _tcpserver_listen_thread_run;) {

				if (nullptr == _tcp_server) {
					_tcpserver_listen_thread_run.store(false);
					break;
				}

				{
					std::lock_guard<std::mutex> lock(_tcp_send_frame_mutex);
					auto queue_size = _tcp_send_frame_q.size();
				}
				if (_tcp_send_frame_q.size() <= 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}

				// 如果有tcp client 连接上的
				if (_tcp_server->connectionCount() > 0/*1*/) {
					AVFrame* frame{};
					FRAME_POS_Qianjue frame_pos_qianjue{};
					{
						std::lock_guard<std::mutex> lock(_tcp_send_frame_mutex);
						auto m_frame = _tcp_send_frame_q.front();
						_tcp_send_frame_q.pop();
						frame = av_frame_clone(m_frame);
						av_frame_free(&m_frame);
						frame_pos_qianjue = _tcp_send_sei_q.front();
						_tcp_send_sei_q.pop();
					}

					frameSeiMsg sei_msg;
					sei_msg.SyncTimeEO = frame_pos_qianjue.SyncTimeEO;
					sei_msg.SyncTimeUAV = frame_pos_qianjue.SyncTimeUAV;
					sei_msg.PodFramePan = frame_pos_qianjue.PodFramePan;
					sei_msg.PodFrameTilt = frame_pos_qianjue.PodFrameTilt;
					sei_msg.PodFrameRoll = frame_pos_qianjue.PodFrameRoll;
					sei_msg.PodPan = frame_pos_qianjue.PodPan;
					sei_msg.PodTilt = frame_pos_qianjue.PodTilt;
					sei_msg.PodRoll = frame_pos_qianjue.PodRoll;
					sei_msg.UAVPan = frame_pos_qianjue.UAVPan;
					sei_msg.UAVTilt = frame_pos_qianjue.UAVTilt;
					sei_msg.UAVRoll = frame_pos_qianjue.UAVRoll;
					sei_msg.UAVLat = frame_pos_qianjue.UAVLat;
					sei_msg.UAVLon = frame_pos_qianjue.UAVLon;
					sei_msg.UAVHeight = frame_pos_qianjue.UAVHeight;
					sei_msg.TgtHeight = frame_pos_qianjue.TgtHeight;

					std::string send_data{};
					send_data.append(reinterpret_cast<const char*>(&_frame_header), sizeof(frame_header));

					// resize
					AVFrame* dst_frame{};
					fastYuv420pResize(frame, dst_frame);
					av_frame_free(&frame);

					// 从frame中拷贝数据，此处认为我们视频解码后的图像是YUV420的
					auto y_size = dst_frame->height * dst_frame->linesize[0];
					auto u_size = (dst_frame->height / 2) * dst_frame->linesize[1];
					auto v_size = (dst_frame->height / 2) * dst_frame->linesize[2];
					auto frame_data_size = y_size + u_size + v_size;

					for (int i = 0; i < dst_frame->height; ++i) {
						send_data.append(reinterpret_cast<const char*>(dst_frame->data[0] + i * dst_frame->linesize[0]), dst_frame->linesize[0]);
					}
					for (int i = 0; i < dst_frame->height / 2; ++i) {
						send_data.append(reinterpret_cast<const char*>(dst_frame->data[1] + i * dst_frame->linesize[1]), dst_frame->linesize[1]);
					}
					for (int i = 0; i < dst_frame->height / 2; ++i) {
						send_data.append(reinterpret_cast<const char*>(dst_frame->data[2] + i * dst_frame->linesize[2]), dst_frame->linesize[2]);
					}
					av_frame_free(&dst_frame);

					static int send_data_count = 0;
					send_data_count++;
					send_data.append(reinterpret_cast<const char*>(&sei_msg), sizeof(sei_msg));
					// eap_warning_printf("send_data_count ===================: %d ", (int)send_data.length());
					_tcp_server->broadcast(send_data);
					// sendPictureData(shared_data, send_data);
				} else {
					//TODO 没有TCP client在
					std::this_thread::sleep_for(std::chrono::milliseconds(15));//避免队列中还有数据，但是TCP客户端没有，导致CPU占用升高
				}
			}

		}

		void DispatchTaskImpl::sendPictureData(LockFreeSharedData* shared_data, const std::string &send_data)
		{
            // 写
            // 版本号+1（标记开始写入，变为奇数）
            uint32_t curr_version = shared_data->version.load(std::memory_order_relaxed);
            shared_data->version.store(curr_version + 1, std::memory_order_release);
			std::strcpy(shared_data->data, send_data.c_str());
			// 版本号+1（标记写入完成，变为偶数）
			shared_data->version.store(curr_version + 2, std::memory_order_release);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			eap_information_printf(">>>> write share mem ver: %d", curr_version);
		}

		int DispatchTaskImpl::fastYuv420pResize(AVFrame* src_frame, AVFrame*& dst_frame)
		{
			int src_width = src_frame->width;
			int src_height = src_frame->height;

			int dst_width = 960;
			int dst_height = 540;

			if (nullptr == _SwsCtx) {// frame 暂时不需要判空
				_SwsCtx = sws_getContext(src_width, src_height,
					(AVPixelFormat)src_frame->format,
					dst_width, dst_height,
					(AVPixelFormat)src_frame->format, SWS_BILINEAR,
					NULL, NULL, NULL);
			}

			if (!_SwsCtx) {
				eap_warning(" can't create sws context ");
				return -1;
			}

			if (!dst_frame || dst_frame->width != dst_width || dst_frame->height != dst_height || dst_frame->format != src_frame->format) {
				av_frame_free(&dst_frame);
				dst_frame = av_frame_alloc();
				if (!dst_frame) {
					eap_warning(" can't alloc dst_frame ");
					sws_freeContext(_SwsCtx);
					return -1;
				}
				dst_frame->format = src_frame->format;
				dst_frame->width = dst_width;
				dst_frame->height = dst_height;
				// 分配缓冲区
				if (av_frame_get_buffer(dst_frame, 1) < 0) { // 1字节对齐
					eap_warning(" can't get dst_frame buffer ");
					av_frame_free(&dst_frame);
					sws_freeContext(_SwsCtx);
					return -1;
				}
			}

			int result = sws_scale(_SwsCtx,
				src_frame->data,
				src_frame->linesize,
				0,
				src_height,
				dst_frame->data,
				dst_frame->linesize);
			if (result < 0) {
				eap_warning(" swscale faile ");
				return -1;
			}
			else {
				return 1;
			}
		}
#endif
		
		void DispatchTaskImpl::receiveVersinData(std::string version_data)
		{
			try
			{
				Poco::JSON::Parser parser;
				Poco::Dynamic::Var result = parser.parse(version_data);
				Poco::JSON::Object all_params = *(result.extract<Poco::JSON::Object::Ptr>());

				Poco::JSON::Object params_databack = all_params.has("dataBack") ? *(all_params.getObject("dataBack")) : Poco::JSON::Object();
				_joedge_version.DataBackMajor = params_databack.has("majorVersion") ? params_databack.getValue<uint32_t>("majorVersion") : 0;
				_joedge_version.DataBackMinor = params_databack.has("minorVersion") ? params_databack.getValue<uint32_t>("minorVersion") : 0;
				_joedge_version.DataBackPatch = params_databack.has("patch") ? params_databack.getValue<uint32_t>("patch") : 0;
				_joedge_version.DataBackBuild = params_databack.has("buildVersion") ? params_databack.getValue<uint32_t>("buildVersion") : 0;

				Poco::JSON::Object params_payload = all_params.has("payloadControl") ? *(all_params.getObject("payloadControl")) : Poco::JSON::Object();
				_joedge_version.PayloadMajor = params_payload.has("majorVersion") ? params_payload.getValue<uint32_t>("majorVersion") : 0;
				_joedge_version.PayloadMinor = params_payload.has("minorVersion") ? params_payload.getValue<uint32_t>("minorVersion") : 0;
				_joedge_version.PayloadPatch = params_payload.has("patch") ? params_payload.getValue<uint32_t>("patch") : 0;
				_joedge_version.PayloadBuild = params_payload.has("buildVersion") ? params_payload.getValue<uint32_t>("buildVersion") : 0;

				Poco::JSON::Object params_pilot = all_params.has("pilotControl") ? *(all_params.getObject("pilotControl")) : Poco::JSON::Object();
				_joedge_version.PilotMajor = params_pilot.has("majorVersion") ? params_pilot.getValue<uint32_t>("majorVersion") : 0;
				_joedge_version.PilotMinor = params_pilot.has("minorVersion") ? params_pilot.getValue<uint32_t>("minorVersion") : 0;
				_joedge_version.PilotPatch = params_pilot.has("patch") ? params_pilot.getValue<uint32_t>("patch") : 0;
				_joedge_version.PilotBuild = params_pilot.has("buildVersion") ? params_pilot.getValue<uint32_t>("buildVersion") : 0;

				Poco::JSON::Object params_sma = all_params.has("sma") ? *(all_params.getObject("sma")) : Poco::JSON::Object();
				_joedge_version.SmaMajor = params_sma.has("majorVersion") ? params_sma.getValue<uint32_t>("majorVersion") : 0;
				_joedge_version.SmaMinor = params_sma.has("minorVersion") ? params_sma.getValue<uint32_t>("minorVersion") : 0;
				_joedge_version.SmaPatch = params_sma.has("patch") ? params_sma.getValue<uint32_t>("patch") : 0;
				_joedge_version.SmaBuild = params_sma.has("buildVersion") ? params_sma.getValue<uint32_t>("buildVersion") : 0;

			}
			catch(const std::exception& e)
			{
				eap_error_printf("receiveVersinData parse json throw exception: %s", e.what());
			}
			catch(...)
			{
				eap_error("receiveVersinData parse json throw exception: unknown error");
			}
		}

		void DispatchTaskImpl::createPusher(std::string url)
		{
			_pusher = PusherObjectPtr(new PusherObject());
			for (int i = 0; i < _push_urls.size(); i++) {
				PusherPtr pusher{};
				url = _push_urls[i];
				if (url.find("webrtc://") == 0 ||
					url.find("webrtcs://") == 0) {
					pusher = PusherRtc::createInstance();
					_pusher->_is_rtc = true;
				}
				else {
					pusher = PusherTradition::createInstance();
					_pusher->_is_rtc = false;
				}
				pusher->setStopCallback(std::bind(&DispatchTaskImpl::pusherStopCallback, this,
					url, std::placeholders::_1, std::placeholders::_2));
				{
					std::lock_guard<std::mutex> lock(_pushers_mutex);
					if (i == 0) {
						_pusher->_url = url;
						_pusher->_pusher = pusher;
					}
					else {
						_pusher->_air_url = url;
						_pusher->_air_pusher = pusher;
					}
				}
			}
			if (!_airborne_45G) {
				openPusher();
			}else if (!_pusher->_air_url.empty()) {
				eap::ThreadPool::defaultPool().start([this]() {
					openAirPusher();
				});
			}
		}

		void DispatchTaskImpl::destroyPusher()
		{
			std::unique_lock<std::mutex> pusher_lock(_pushers_mutex);
			if (_pusher && _pusher->_pusher) {
				_pusher->_pusher->close();
				_pusher->_pusher.reset();
			}
			if (_pusher && _pusher->_air_pusher) {
				_pusher->_air_pusher->close();
				_pusher->_air_pusher.reset();
			}
			pusher_lock.unlock();
		}

#ifdef ENABLE_AI
		void DispatchTaskImpl::createAIEngine()
		{
			if (_is_ai_on || _is_ai_assist_track_on) {
				if (!_engines->_ai_object_detector && (_is_ai_first_create || _is_update_func)) {
					std::string engine_file_full_name{};
					int width{};
					int height{};
					int class_num{};
					double conf_thresh{};
					double nums_thresh{};
					float fire_conf_thresh{};
					float smoke_conf_thresh{};
					std::string yolo_version{};
					std::string text_encoder_feature{};
					try {
#ifdef ENABLE_FIRE_DETECTION
						//[sengfang project]ENABLE_PIO on ：Independent AI configuration
						//build deb file path:doc/AiDetectConfig/deb_made.sh, installing  deb file to generate Independent AI configuration and engine file
						DetectAiCfg aiInfo=getDetectAiCfg();
						aiInfo.print();
						_fire_conf_thresh = std::stof(aiInfo.FireConfThresh);
						_smoke_conf_thresh = std::stof(aiInfo.SmokeConfThresh);

						engine_file_full_name = aiInfo.EnginePath;
						width = std::stoi(aiInfo.InputWidth);
						height = std::stoi(aiInfo.InputHeight);
						class_num = std::stoi(aiInfo.ClassNumber);
						conf_thresh = std::stod(aiInfo.ConfThresh);
						nums_thresh = std::stod(aiInfo.NmsThresh);
						yolo_version = aiInfo.YoloVersion;
#else
						GET_CONFIG(std::string, getString, my_engine_file_full_name, AI::kEngineFileFullName);
						GET_CONFIG(int, getInt, my_width, AI::kModelWidth);
						GET_CONFIG(int, getInt, my_height, AI::kModelHeight);
						GET_CONFIG(int, getInt, my_class_num, AI::kClassNumber);
						GET_CONFIG(double, getDouble, my_conf_thresh, AI::kConfThresh);
						GET_CONFIG(double, getDouble, my_nums_thresh, AI::kNmsThresh);
						GET_CONFIG(std::string, getString, my_yolo_version, AI::kYoloVersion);
						//GET_CONFIG(std::string, getString,my_text_encoder_feature, AI::kTextEncoderFeature);
						engine_file_full_name = my_engine_file_full_name;
						width = my_width;
						height = my_height;
						class_num = my_class_num;
						conf_thresh = my_conf_thresh;
						nums_thresh = my_nums_thresh;
						yolo_version = my_yolo_version;
						//text_encoder_feature = my_text_encoder_feature;
#endif

					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					_engines->_ai_object_detector = joai::ObjectDetectPtr(new joai::ObjectDetect());

					auto status = _engines->_ai_object_detector->initNetwork(engine_file_full_name,
					width, height, class_num, conf_thresh, nums_thresh, yolo_version);
					if (status != joai::ENGINE_SUCCESS) {
						if(!_is_update_func){
							std::string desc = "AI engine init failed, status code: " + std::to_string(status);						
							eap_error(desc);
							//AI engine 初始化失败,上报异常
							NoticeCenter::Instance()->getCenter().postNotification(
								new VideoMsgNotice(_id, ((int)status + 7), desc));
						}

						eap_error_printf("AI function update failed: %d", (int)status);
						_update_func_err_desc += std::string("AI; ");
						_update_func_result = false;
						// _func_mask -= FUNCTION_MASK_AI;//如果是关闭功能，关闭失败的话应该+，但是目前认为不会关闭失败
						_engines->_ai_object_detector.reset();
					} else {
						eap_information_printf("AI Engine created and inited success:%d", (int)status);
						if ((_func_mask & FUNCTION_MASK_AI) != FUNCTION_MASK_AI) {
							_func_mask += FUNCTION_MASK_AI;
							updateTaskFuncmask();
						}
					}
					
				}
				
				if (!_engines->_ai_mot_tracker && (_is_ai_first_create || _is_update_func)) {
					bool track_switch{};
					int track_buff_len{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
						GET_CONFIG(int, getInt, my_track_buff_len, AI::kTrackBufferLength);
						track_switch = my_track_switch;
						track_buff_len = my_track_buff_len;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					if (track_switch) {
						_engines->_ai_mot_tracker = joai::MotTrackPtr(new joai::MotTrack(30, track_buff_len, true));
						if (!_engines->_ai_mot_tracker) {
							if (!_is_update_func) {
								std::string desc = "AI track create failed";
								throw std::runtime_error(desc);
							}
							eap_error_printf("AI track func update failed", " ");
							_update_func_err_desc += std::string("AI_Track; ");
							_update_func_result = false;
							_engines->_ai_mot_tracker.reset();
						}else{
							eap_information("AI track func update success");
						}
					}
				}
			}
		}
#ifdef ENABLE_OPENSET_DETECTION
		void DispatchTaskImpl::createOpensetAIEngine()
		{
			if (_is_openset_ai_on) {
				if (!_engines->_openset_ai_object_detector && (_is_openset_ai_first_create || _is_update_func)) {
					std::string opset_engine_file_full_name{};
					int width{};
					int height{};
					int class_num{};
					double openset_conf_thresh{};
					double openset_nms_thresh{};
					std::string OwlVersion{};
					std::string text_encoder_feature{};
					std::string text_encoder_onnx{};
					std::string vocab{};
					std::string merges{};
					std::string added_tokens{};
					std::string special_tokens_map{};
					std::string tokenizer_config{};
					try{
						GET_CONFIG(std::string, getString, my_opset_engine_file_full_name, AI::kOpsetEngineFileFullName);
						GET_CONFIG(int, getInt, my_width, AI::kModelWidth);
						GET_CONFIG(int, getInt, my_height, AI::kModelHeight);
						GET_CONFIG(int, getInt, my_class_num, AI::kClassNumber);
						GET_CONFIG(double, getDouble, my_openset_conf_thresh, AI::kOpensetConfThresh);
						GET_CONFIG(double, getDouble, my_openset_nms_thresh, AI::kOpensetNmsThresh);
						GET_CONFIG(std::string, getString, my_OwlVersion, AI::kOwlVersion);
						GET_CONFIG(std::string, getString, my_text_encoder_feature, AI::kTextEncoderFeature);
						GET_CONFIG(std::string, getString, my_text_encoder_onnx, AI::kTextEncoderOnnx);
						GET_CONFIG(std::string, getString, my_vocab, AI::kVocab);
						GET_CONFIG(std::string, getString, my_merges, AI::kMerges);
						GET_CONFIG(std::string, getString, my_added_tokens, AI::kAddedTokens);
						GET_CONFIG(std::string, getString, my_special_tokens_map, AI::kSpecialTokensMap);
						GET_CONFIG(std::string, getString, my_tokenizer_config, AI::kTokenizerConfig);
						opset_engine_file_full_name = my_opset_engine_file_full_name;
						width = my_width;
						height = my_height;
						class_num = my_class_num;
						openset_conf_thresh = my_openset_conf_thresh;
						openset_nms_thresh = my_openset_nms_thresh;
						OwlVersion = my_OwlVersion;
						text_encoder_feature = my_text_encoder_feature;
						text_encoder_onnx = my_text_encoder_onnx;
						vocab = my_vocab;
						merges = my_merges;
						added_tokens = my_added_tokens;
						special_tokens_map = my_special_tokens_map;
						tokenizer_config = my_tokenizer_config;
					//GET_CONFIG(std::string, getString,my_text_encoder_feature, AI::kTextEncoderFeature);
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}
					_engines->_openset_ai_object_detector = joai::ObjectDetectPtr(new joai::ObjectDetect());
					auto status = _engines->_openset_ai_object_detector->initNetwork(opset_engine_file_full_name,
					width, height, class_num, openset_conf_thresh, openset_nms_thresh, OwlVersion, 
					text_encoder_onnx,vocab,merges,
					added_tokens,special_tokens_map,tokenizer_config,text_encoder_feature);
					//std::cout << "Openset AI Engine create params: engine_file_full_name: " << opset_engine_file_full_name << ", width: " << width << ", height: " << height << ", class_num: " << class_num << ", conf_thresh: " << openset_conf_thresh << ", nms_thresh: " << openset_nms_thresh << ", OwlVersion: " << OwlVersion << ", text_encoder_feature: " << text_encoder_feature << ", text_encoder_onnx: " << text_encoder_onnx << ", vocab: " << vocab << ", merges: " << merges << ", added_tokens: " << added_tokens << ", special_tokens_map: " << special_tokens_map << ", tokenizer_config: " << tokenizer_config << std::endl;
					eap_information_printf("Openset AI Engine create params: engine_file_full_name: %s, width: %d, height: %d, class_num: %d, conf_thresh: %f, nms_thresh: %f, OwlVersion: %s, text_encoder_feature: %s, text_encoder_onnx: %s, vocab: %s, merges: %s, added_tokens: %s, special_tokens_map: %s, tokenizer_config: %s",
						opset_engine_file_full_name, width, height, class_num, openset_conf_thresh, openset_nms_thresh,
						OwlVersion, text_encoder_feature, text_encoder_onnx, vocab, merges,
						added_tokens, special_tokens_map, tokenizer_config);
					if (status != joai::ENGINE_SUCCESS) {
						std::string desc = "Openset AI engine init failed, status code: " + std::to_string(status);						
						eap_error(desc);
						//Openset AI engine 初始化失败,上报异常
						NoticeCenter::Instance()->getCenter().postNotification(
							new VideoMsgNotice(_id, ((int)status + 7), desc));
						eap_error_printf("Openset AI function update failed: %d", (int)status);
						_update_func_err_desc += std::string("Openset AI; ");
						_update_func_result = false;
						// _func_mask -= FUNCTION_MASK_OPENSET_AI;//如果是关闭功能，关闭失败的话应该+，但是目前认为不会关闭失败
						_engines->_openset_ai_object_detector.reset();
					} else {
						eap_information_printf("Openset AI Engine created and inited success:%d", (int)status);
						if ((_func_mask & FUNCTION_MASK_OPENSET_AI) != FUNCTION_MASK_OPENSET_AI) {
							_func_mask += FUNCTION_MASK_OPENSET_AI;
							updateTaskFuncmask();
						}
					}
				}
			}
		}
#endif 
#endif
	
#ifdef ENABLE_AR
		void DispatchTaskImpl::createAREngine()
		{
			if (!_engines->_ar_engine && (_is_ar_first_create || _is_update_func) && !_ar_camera_config.empty()) {
				try
				{
					std::lock_guard<std::mutex> lock(_ar_related_mutex);
					_engines->_ar_engine = joar::create2dArEngine(
						_ar_vector_file, _ar_camera_config);
				}
				catch (const std::exception& e)
				{
					std::string exception_desc = std::string("ar engine create failed, exception description: ") + e.what();
					eap_error(exception_desc);
					NoticeCenter::Instance()->getCenter().postNotification(
						new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::AREngineCreateFailed, exception_desc));
					NoticeCenter::Instance()->getCenter().postNotification(new AREngineResultNotice(
						std::string(_id), false, exception_desc));
				}

				if ((_is_ar_on || _is_enhanced_ar_on) && _engines->_ar_engine) {
					if(_engines->_ar_engine && !_engines->_ar_engine->start(true)){
						if(!_is_update_func)
						{
							std::string desc = "AR engine init failed";
							eap_error(desc);
							// 上报失败结果
							NoticeCenter::Instance()->getCenter().postNotification(
								new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::AREngineInitFailed, desc));
						}
						eap_error( "AR function update failed");
						_update_func_err_desc += std::string("AR; ");
						_update_func_result = false;
						_func_mask -= FUNCTION_MASK_AR;
						_engines->_ar_engine.reset();
					} else {
						eap_information("--------AR Engine start!");
					}
				}
				
				if (_engines->_ar_engine) {
					_engines->_ar_engine->setWarningArea(_ar_level_one_distance, _ar_level_two_distance);
					_engines->_ar_engine->configTowerAR(_ar_is_tower, _ar_tower_height, _ar_buffer_sync_height);
					NoticeCenter::Instance()->getCenter().postNotification(new AREngineResultNotice(
						std::string(_id), true, ""));
				}
				eap_information_printf( "AR Engine created and inited success, _ar_camera_config: %s, _ar_vector_file: %s", _ar_camera_config, _ar_vector_file);
			}
		}
		void DispatchTaskImpl::createARMarkEngine()
		{
			try {
				if(!_engines->_ar_mark_engine) {
					_ar_related_mutex.lock();
					_engines->_ar_mark_engine = jomarker::createMarkerEngine(_ar_camera_config);
					_ar_related_mutex.unlock();
					eap_information("AR Mark created and inited success!");
				}
			}
			catch(const std::exception& e) {
				//标注引擎初始化失败，不能影响其它工作
				std::string exception_desc = std::string(" ar mark engine create failed, exception description: ") + std::string(e.what());
				eap_error(exception_desc);
				NoticeCenter::Instance()->getCenter().postNotification(
					new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::ARMarkEngineCreateFailed, exception_desc));
			}
		}
		void DispatchTaskImpl::createAuxiliaryAIEngine()
		{
#ifdef ENABLE_AI
			if (_is_enhanced_ar_on) {
				if (!_engines->_aux_ai_object_detector && (_is_aux_ai_first_create || _is_update_func)) {

					std::string engine_file_full_name{};
					int width{};
					int height{};
					int class_num{};
					float conf_thresh{};
					float nums_thresh{};
					std::string yolo_version{};
					std::string text_encoder_feature{};
					try {
						GET_CONFIG(std::string, getString, my_engine_file_full_name, AI::kEngineFileFullNameAux);
						GET_CONFIG(int, getInt, my_width, AI::kModelWidthAux);
						GET_CONFIG(int, getInt, my_height, AI::kModelHeightAux);
						GET_CONFIG(int, getDouble, my_class_num, AI::kClassNumberAux);
						GET_CONFIG(float, getDouble, my_conf_thresh, AI::kConfThreshAux);
						GET_CONFIG(float, getDouble, my_nums_thresh, AI::kNmsThreshAux);
						GET_CONFIG(std::string, getString, my_yolo_version, AI::kYoloVersionAux);
						GET_CONFIG(std::string, getString,my_text_encoder_feature, AI::kTextEncoderFeatureAux);
						engine_file_full_name = my_engine_file_full_name;
						width = my_width;
						height = my_height;
						class_num = my_class_num;
						conf_thresh = my_conf_thresh;
						nums_thresh = my_nums_thresh;
						yolo_version = my_yolo_version;
						text_encoder_feature = my_text_encoder_feature;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					_engines->_aux_ai_object_detector = joai::ObjectDetectPtr(new joai::ObjectDetect());
					//engine_file_full_name = "H:/code/JoEAPSma/release/windows/Debug/Debug/ai_file/2024_06_06_17_25_01_windows_onnxtoengine_v8l_640_384_51_V2.0.engine";
					//yolo_version = "Yolov8";
					//class_num = 51;

					
					auto status = _engines->_aux_ai_object_detector->initNetwork(engine_file_full_name,
						width, height, class_num, conf_thresh, nums_thresh, yolo_version);
					
					if (status != joai::ENGINE_SUCCESS) {
						if (!_is_update_func) {
							std::string desc = "Auxiliary AI engine init failed, status code: " + std::to_string(status);
							eap_error(desc);
						}
						eap_error("Auxiliary AI func update failed");
						_update_func_err_desc += std::string("Auxiliary AI; ");
						_update_func_result = false;
						_func_mask -= FUNCTION_MASK_ENHANCED_AR;//如果是关闭功能，关闭失败的话应该+，但是目前认为不会关闭失败
						_is_enhanced_ar_on = false;
						_engines->_aux_ai_object_detector.reset();
					}
					eap_information("Auxiliary AI func update success!");
				}

				if (!_engines->_aux_ai_mot_tracker && (_is_aux_ai_first_create || _is_update_func)) {
					
					bool track_switch{};
					int track_buff_len{};
					try {
						GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitchAux);
						GET_CONFIG(int, getInt, my_track_buff_len, AI::kTrackBufferLengthAux);
						track_switch = my_track_switch;
						track_buff_len = my_track_buff_len;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}

					if (track_switch) {
						_engines->_aux_ai_mot_tracker = joai::MotTrackPtr(new joai::MotTrack(30, track_buff_len, true));
						if (!_engines->_aux_ai_mot_tracker) {
							if (!_is_update_func) {
								std::string desc = "Auxiliary AI track create failed";
								eap_error(desc);
							}

							eap_error("Auxiliary AI track func update failed");
							_update_func_err_desc += std::string("Auxiliary AI_Track; ");
							_update_func_result = false;
							_engines->_aux_ai_mot_tracker.reset();
						}
						else
						{
							eap_information_printf("AI track func update success", " ");
						}
					}
				}
			}
#endif
		}
		void DispatchTaskImpl::calculatGeodeticToImage(std::list<ArElementsInternal> ar_elements_array, ArInfosInternal& ar_infos, std::string guid, const int64_t& timestamp, const int& img_width, const int& img_height, const jo::JoARMetaDataBasic& meta_data, const std::vector<jo::GeographicPosition>& points, const std::vector<std::vector<jo::GeographicPosition>>& lines, const std::vector<std::vector<jo::GeographicPosition>>& regions)
		{
#ifdef ENABLE_AR
			//物点转像点，等到回调结果再继续
			if(_engines->_ar_mark_engine->reflectGeodeticToImage(timestamp, img_width, img_height, meta_data, points, lines, regions, _pixel_points, _pixel_lines, _pixel_regions))
			{
				if(!_pixel_points.empty())
				{
					for(std::size_t i = 0; i < _pixel_points.size(); ++i)
					{

						ArElementsInternal ar_elements_internal{};
						ar_elements_internal.X = _pixel_points[i].x;
						ar_elements_internal.Y = _pixel_points[i].y;
						ar_elements_internal.lat = points[i].latitude;
						ar_elements_internal.lon = points[i].longitude;
						ar_elements_internal.HMSL = points[i].altitude;
						ar_elements_internal.Guid = guid;

						//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
						//计算完出来后，也要跟送进去的点、线、面顺序一致
						if(!ar_elements_array.empty())
						{
							auto ar_elements = ar_elements_array.front();
							ar_elements_internal.Type = ar_elements.Type;
							ar_elements_internal.Category = ar_elements.Category;
							ar_elements_internal.CurIndex = ar_elements.CurIndex;
							ar_elements_internal.NextIndex = ar_elements.NextIndex;
							ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
							ar_elements_array.pop_front();
						}

						ar_infos.ArElementsArray.push_back(ar_elements_internal);
						ar_infos.ArElementsNum++;
					}
					_pixel_points.clear();
				}
				if(!_pixel_lines.empty())
				{
					for(std::size_t i = 0; i < _pixel_lines.size(); ++i)
					{
						for(std::size_t j = 0; j < _pixel_lines[i].size(); ++j)
						{
							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = _pixel_lines[i][j].x;
							ar_elements_internal.Y = _pixel_lines[i][j].y;
							ar_elements_internal.lat = lines[i][j].latitude;
							ar_elements_internal.lon = lines[i][j].longitude;
							ar_elements_internal.HMSL = lines[i][j].altitude;
							ar_elements_internal.Guid = guid;

							//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
							//计算完出来后，也要跟送进去的点、线、面顺序一致
							if(!ar_elements_array.empty())
							{
								auto ar_elements = ar_elements_array.front();
								ar_elements_internal.Type = ar_elements.Type;
								ar_elements_internal.Category = ar_elements.Category;
								ar_elements_internal.CurIndex = ar_elements.CurIndex;
								ar_elements_internal.NextIndex = ar_elements.NextIndex;
								ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
								ar_elements_array.pop_front();
							}

							ar_infos.ArElementsArray.push_back(ar_elements_internal);
							ar_infos.ArElementsNum++;
						}
					}
					_pixel_lines.clear();
				}
				if(!_pixel_regions.empty())
				{
					for(std::size_t i = 0; i < _pixel_regions.size(); ++i)
					{
						for(std::size_t j = 0; j < _pixel_regions[i].size(); ++j)
						{
							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = _pixel_regions[i][j].x;
							ar_elements_internal.Y = _pixel_regions[i][j].y;
							ar_elements_internal.lat = regions[i][j].latitude;//_pixel_regions中有两个vector，regions中只有一个vector，导致崩溃
							ar_elements_internal.lon = regions[i][j].longitude;
							ar_elements_internal.HMSL = regions[i][j].altitude;
							ar_elements_internal.Guid = guid;

							//这样赋值，必须保证收到的协议数据中，是先排的点，然后是线，最后是面
							//计算完出来后，也要跟送进去的点、线、面顺序一致
							if(!ar_elements_array.empty())
							{
								auto ar_elements = ar_elements_array.front();
								ar_elements_internal.Type = ar_elements.Type;
								ar_elements_internal.Category = ar_elements.Category;
								ar_elements_internal.CurIndex = ar_elements.CurIndex;
								ar_elements_internal.NextIndex = ar_elements.NextIndex;
								ar_elements_internal.DotQuantity = ar_elements.DotQuantity;
								ar_elements_array.pop_front();
							}

							ar_infos.ArElementsArray.push_back(ar_elements_internal);
							ar_infos.ArElementsNum++;
						}
					}
					_pixel_regions.clear();
				}
			}
#endif
		}

		void DispatchTaskImpl::videoMarkMetaDataWrite(ArInfosInternal& ar_infos, int64_t packet_pts)
		{
			//若由于时间戳混乱，导致可能有时间戳相同的帧，相同的帧目前默认只给一帧数据，其它没有标注数据
			_video_mark_frame_count++;
			std::string video_mark_metadata_file_name = std::to_string(packet_pts) + ".json";

			Poco::JSON::Object json_all{};
			Poco::JSON::Array json_elements_array{};

			//送进来的没有存储该值
			// json_all["AR_Status"] = ar_infos.ArStatus;
			// json_all["ArTroubleCode"] = ar_infos.ArTroubleCode;
			json_all.set("ElementsNum", ar_infos.ArElementsNum);

			for(auto iter : ar_infos.ArElementsArray)
			{
				Poco::JSON::Object json_element_array{};
				json_element_array.set("Type", iter.Type);
				json_element_array.set("DotQuantity", iter.DotQuantity);
				json_element_array.set("X", iter.X);
				json_element_array.set("Y", iter.Y);
				json_element_array.set("lon", iter.lon);
				json_element_array.set("lat", iter.lat);
				json_element_array.set("HMSL", iter.HMSL);
				json_element_array.set("Category", iter.Category);
				json_element_array.set("CurIndex", iter.CurIndex);
				json_element_array.set("NextIndex", iter.NextIndex);
				json_elements_array.add(json_element_array);
			}
			json_all.set("ElementsArray", json_elements_array);
			std::string json_all_str = eap::sma::jsonToString(json_all);
			//不写文件，直接http发送出去?
		}
		
#endif

		void DispatchTaskImpl::createStablizer()
		{
		#ifdef ENABLE_STABLIZE
			if (_is_stable_on) {
				if (!_stablizer && (_is_stable_first_create || _is_update_func)) {
					_stablizer = std::make_shared<stablizer>(true, 16);
					if (!_stablizer) {
						if (!_is_update_func) {
							std::string desc = "Stabilizer run failed";
							throw std::runtime_error(desc);
						}
						eap_error("stabilize func update failed");
						_update_func_err_desc += std::string("Stabilizer; ");
						_update_func_result = false;
						_func_mask -= FUNCTION_MASK_STABLE;
					}
					eap_information("Stabilize create success");
				}
			}
			_is_stable_first_create = false;
		#endif
		}
	
#ifdef ENABLE_AI
		void DispatchTaskImpl::executeAIProcess(CodecImagePtr image, bool is_process)
		{
			std::vector<joai::Result> detect_objects{};
			std::vector<joai::TrackResult>  track_ret{}; 
			if (image->bgr24_image.empty() || !is_process) {
				// detect_objects = _engines->latest_detect_result;
				detect_objects = std::move(_engines->latest_detect_result);
			} else {
				detect_objects = _engines->_ai_object_detector->detect(image->bgr24_image);
				_engines->latest_detect_result = detect_objects;				
			}
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AiStatus = 1;
			
			bool track_switch{};
			try {
				GET_CONFIG(bool, getBool, my_track_switch, AI::kTrackSwitch);
				track_switch = my_track_switch;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config throw exception: %s", e.what());
			}

			if(track_switch ){
				track_ret = _engines->_ai_mot_tracker->Update(detect_objects);
				image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = track_ret.size();
				int i = 0;
				for (auto object : track_ret) {
					/*cv::rectangle(image->bgr24_image, 
					 				cv::Rect(object.Bounding_box.x, object.Bounding_box.y, object.Bounding_box.width, object.Bounding_box.height), 
									cv::Scalar(0, 255, 0), 2);
					cv::putText(image->bgr24_image, "ID: " + std::to_string(object.track_id), cv::Point(object.Bounding_box.x, object.Bounding_box.y - 1), cv::FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(0, 0, 255), 2);*/
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX =
						object.Bounding_box.x;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY =
						object.Bounding_box.y;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetWidth =
						object.Bounding_box.width;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetHeight =
						object.Bounding_box.height;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtSN =
						object.track_id;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass =
						object.cls;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtConfidence =
						object.confidence * 100;
					++i;
				}
			}else{
				image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = detect_objects.size();
				int i = 0;
				for (auto object : detect_objects) {
				/*	cv::rectangle(image->bgr24_image, 
					 				cv::Rect(object.Bounding_box.x, object.Bounding_box.y, object.Bounding_box.width, object.Bounding_box.height), 
									cv::Scalar(0, 255, 0), 2);*/
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX =
						object.Bounding_box.x;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY =
						object.Bounding_box.y;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetWidth =
						object.Bounding_box.width;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetHeight =
						object.Bounding_box.height;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass =
						object.cls;
					image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
						ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].TgtConfidence =
						object.confidence * 100;
					++i;

					// eap_warning_printf("ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX = %d"
					// , object.Bounding_box.x);
					// eap_warning_printf("ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY = %d"
					// , object.Bounding_box.y);
					// eap_warning_printf("ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetWidth = %d"
					// , object.Bounding_box.width);
					// eap_warning_printf("ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetHeight = %d"
					// , object.Bounding_box.height);
					// eap_warning_printf("ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetTGTclass = %d"
					// , object.cls);
				}
			}	
			//// 保存图片到results文件夹
	/*		std::string detectfilename = "H:/JoEAPSma/release/windows/Debug/Debug/results/detect_" + std::to_string(std::rand()) + ".jpg";
			cv::imwrite(detectfilename, image->bgr24_image);*/
	
			// 直接路径：AI检测结果绕过元数据链路直接写入assembly
			{
				std::lock_guard<std::mutex> lock(_pushers_mutex);
				if (_pusher && _pusher->_pusher) {
					_pusher->_pusher->updateAiDetectInfo(
						image->meta_data.meta_data_basic.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p);
				}
			}
		}
#endif
	
#ifdef ENABLE_AR
		void DispatchTaskImpl::executeARProcess(CodecImagePtr image)
		{
			if (_engines->_ar_engine && image->meta_data.meta_data_valid) {
				if (_ar_image_compute_state) {
						_ar_image_compute_state = false;
						std::vector<cv::Rect> cv_rect{};
		#ifdef ENABLE_AI
						if(_is_enhanced_ar_on && _engines->_aux_ai_object_detector &&_engines->_aux_ai_mot_tracker){
								auto detect_objects = _engines->_aux_ai_object_detector->detect(image->bgr24_image);
								if(!detect_objects.empty()){
										std::vector<joai::TrackResult>  track_ret = _engines->_aux_ai_mot_tracker->Update(detect_objects);
										for (const auto &ret : track_ret) {
												cv_rect.push_back(ret.Bounding_box);
										}
								}
						}
		#endif

						auto pts = image->meta_data.pts;
						auto width = image->bgr24_image.cols;
						auto height = image->bgr24_image.rows;
						cv::Mat mat;
						JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;

						jo::JoARMetaDataBasic ar_meta_data{};
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

						std::vector<cv::Point> pixel_points{};
						std::vector<std::vector<cv::Point>> pixel_lines{};

						std::vector<cv::Point> tmp_pixel_points{};

						std::vector<std::vector<cv::Point>> pixel_warning_l1_regions{};
						std::vector<std::vector<cv::Point>> pixel_warning_l2_regions{};
						if(_is_enhanced_ar_on)
						{
								_engines->_ar_engine->withAuxiliaryAI(true);
								_engines->_ar_engine->configAuxiliaryAI(_ai_pos_cor);
								_engines->_ar_engine->inputAuxiliaryAIData(cv_rect);
						}
						std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();
						_engines->_ar_engine->frameProcess(true, pts, mat, width, height, ar_meta_data, pixel_points, pixel_lines
								, pixel_warning_l1_regions, pixel_warning_l2_regions);
						std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();
						auto time=std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1);

						// 筛选去掉屏幕外多余的点
						int i = 0;
						std::queue<int> ar_valid_point_index{}; // 筛选去掉屏幕外多余的点后有效点的索引
						for(const auto &iter : pixel_points){
								if(iter.x <= width  && iter.x >= 0 && iter.y <= height && iter.y >= 0){
										tmp_pixel_points.push_back(iter);
										ar_valid_point_index.push(i);
								}
								++i;
						}

						std::swap(_ar_valid_point_index, ar_valid_point_index);
						std::swap(_ar_image_compute_pixel_points, tmp_pixel_points);
						std::swap(_ar_image_compute_pixel_lines, pixel_lines);
						std::swap(_ar_image_compute_pixel_warning_l1_regions, pixel_warning_l1_regions);
						std::swap(_ar_image_compute_pixel_warning_l2_regions, pixel_warning_l2_regions);
				}
				else{
						_ar_image_compute_state = true;
				}

				image->meta_data.pixel_points = _ar_image_compute_pixel_points;
				image->meta_data.pixel_lines = _ar_image_compute_pixel_lines;
				image->meta_data.pixel_warning_l1_regions = _ar_image_compute_pixel_warning_l1_regions;
				image->meta_data.pixel_warning_l2_regions = _ar_image_compute_pixel_warning_l2_regions;
				image->meta_data.ar_valid_point_index = _ar_valid_point_index;
				if(image->meta_data.ar_vector_file!=_ar_vector_file){
						image->meta_data.ar_vector_file = _ar_vector_file;
				}
			}
		}

		void DispatchTaskImpl::executeARMarkProcess(CodecImagePtr image)
		{
			if(_engines->_ar_mark_engine && image->meta_data.meta_data_valid){
				Poco::JSON::Object root;	
				JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
				uint64_t timeStamp = meta_temp.CarrierVehiclePosInfo_p.TimeStamp;

				jo::JoARMetaDataBasic ar_meta_data{};
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

				int width = _codec_parameter.width;
				int height = _codec_parameter.height;										

				//先判断是否需要删除
				auto _ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.find(_mark_guid);
				while (_ar_mark_pixel_and_geographic != _ar_mark_pixel_and_geographic_map.end() 
					&& _ar_mark_pixel_and_geographic->first == _mark_guid)
				{
					_ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.erase(_ar_mark_pixel_and_geographic);
				}
				
				_mark_guid = "";

				ArInfosInternal ar_infos_internal_to_push_rtc{};//存储以前的计算结果和当前新的标注要素，用来push rtc
				ArInfosInternal ar_infos_internal_to_store{};//这次如果有新增的标注元素，用这个先存下来，再放在map中

				//前面存的像素转地理，地理坐标每帧都要重新反算新的像素坐标;前面存的地理转像素，地理坐标每帧都要重新反算新的像素坐标
				for(auto iter_map = _ar_mark_pixel_and_geographic_map.begin(); 
					iter_map != _ar_mark_pixel_and_geographic_map.end(); iter_map++)
				{
					//地理坐标转图像坐标
					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					ar_point points{};
					ar_line_or_region lines{};
					ar_line_or_region regions{};
					
					// ar_infos_internal_to_push_rtc.ArElementsNum += iter_map->second.ArElementsNum;
					
					for(auto iter_list : iter_map->second.ArElementsArray)
					{
						tmpPoint.longitude = iter_list.lon;
						tmpPoint.latitude = iter_list.lat;
						tmpPoint.altitude = iter_list.HMSL;

						if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) 
						{
							switch (iter_list.Type) {
							case 0: {								
								points.push_back(tmpPoint);
								break;
							}
							case 1: {
								//添加每条线的各个物点
								if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {									
									tmpPointVct.push_back(tmpPoint);
								}
								//当前线已结束，添加整条线
								if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {									
									lines.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							case 2: {
								//添加每个面的各个物点
								if (iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
									tmpPointVct.push_back(tmpPoint);
								}
								//当前面已结束，添加整个面
								if (iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
									regions.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							default:
								break;
							}
						}
						if (!points.empty() || !lines.empty() || !regions.empty()) 
						{
							calculatGeodeticToImage(iter_map->second.ArElementsArray, ar_infos_internal_to_push_rtc
								, iter_map->first, timeStamp, width, height, ar_meta_data, points, lines, regions);
						}
					}
				}

				while (!_video_mark_data.empty()) {
					std::string mark_data = _video_mark_data.front();
					_video_mark_data.pop();
					Poco::JSON::Parser parser;
					try {
						Poco::Dynamic::Var result = parser.parse(mark_data);
						root = *result.extract<Poco::JSON::Object::Ptr>();
					}
					catch (const std::exception& e) {
						eap_error(e.what());
					}
					auto elementsArray = root.has("ArElementsArray") ? root.getArray("ArElementsArray"): Poco::JSON::Array::Ptr();
					int elementsArray_size = elementsArray->size();
					if (elementsArray_size <= 0) { continue; }

					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					ar_point points{};
					ar_line_or_region lines{};
					ar_line_or_region regions{};
					
					//透传回云端
					_ar_mark_elements_guid = root.has("Guid") ? root.getValue<std::string>("Guid") : "";					
					std::list<ArElementsInternal> ar_elements_array;
					for (int i = 0; i < (std::min)(elementsArray_size, 1024); ++i) {

						Poco::JSON::Object elementJs = *elementsArray->getObject(i);

						//图像坐标转地理坐标
						if (elementJs.has("X") && elementJs.has("Y")) {
							cv::Point pixel_point;
							jo::GeographicPosition geoPos{};

							int pixelX = elementJs.getValue<int>("X");
							int pixelY = elementJs.getValue<int>("Y");

							pixel_point.x = pixelX;
							pixel_point.y = pixelY;
														
							if(_engines->_ar_mark_engine){
								_engines->_ar_mark_engine->projectImageToGeodetic(timeStamp, width, height, ar_meta_data, pixel_point, geoPos);
							}else{
								eap_warning("_ar_mark_engine->projectImageToGeodetic but _ar_mark_engine is nullptr!----");
							}

							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.X = pixel_point.x;
							ar_elements_internal.Y = pixel_point.y;
							ar_elements_internal.lat = geoPos.latitude;
							ar_elements_internal.lon = geoPos.longitude;
							ar_elements_internal.HMSL = geoPos.altitude;
							ar_elements_internal.Type = elementJs.getValue<int>("Type");
							ar_elements_internal.DotQuantity = elementJs.getValue<int>("DotQuantity");
							ar_elements_internal.Category = elementJs.getValue<int>("Category");
							ar_elements_internal.CurIndex = elementJs.getValue<int>("CurIndex");
							ar_elements_internal.NextIndex = elementJs.getValue<int>("NextIndex");
							ar_elements_internal.Guid = _ar_mark_elements_guid;

							ar_infos_internal_to_store.ArElementsArray.push_back(ar_elements_internal);
							ar_infos_internal_to_store.ArElementsNum++;					
						}
												
						//地理坐标转图像坐标
						if (elementJs.has("lon") && elementJs.has("lat") && elementJs.has("HMSL")) {
							
							ArElementsInternal ar_elements_internal{};
							ar_elements_internal.Category = elementJs.getValue<int>("Category");
							ar_elements_internal.Type = elementJs.getValue<int>("Type");
							ar_elements_internal.DotQuantity = elementJs.getValue<int>("DotQuantity");
							ar_elements_internal.CurIndex = elementJs.getValue<int>("CurIndex");
							ar_elements_internal.NextIndex = elementJs.getValue<int>("NextIndex");
							ar_elements_internal.Guid = _ar_mark_elements_guid;
							ar_elements_array.push_back(ar_elements_internal);

							tmpPoint.longitude = elementJs.getValue<double>("lon");
							tmpPoint.latitude = elementJs.getValue<double>("lat");
							tmpPoint.altitude = elementJs.getValue<double>("HMSL");
							if (tmpPoint.longitude != 0 && tmpPoint.latitude != 0) {
								switch (elementJs.getValue<int>("Type")) {
								case 0: {
									points.push_back(tmpPoint);
									break;
								}
								case 1: {
									//添加每条线的各个物点
									if (elementJs.getValue<int>("CurIndex") != 0 || (elementJs.getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
										tmpPointVct.push_back(tmpPoint);
									}
									//当前线已结束，添加整条线
									if (elementJs.getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
										lines.push_back(tmpPointVct);
										tmpPointVct.clear();
									}
									break;
								}
								case 2: {
									//添加每个面的各个物点
									if (elementJs.getValue<int>("CurIndex") != 0 || (elementJs.getValue<int>("CurIndex") == 0 && tmpPointVct.empty())) {
										tmpPointVct.push_back(tmpPoint);
									}
									//当前面已结束，添加整个面
									if (elementJs.getValue<int>("NextIndex") == 0 && !tmpPointVct.empty()) {
										regions.push_back(tmpPointVct);
										tmpPointVct.clear();
									}
									break;
								}
								default:
									break;
								}
							}
						}
					}

					//地理坐标转图像坐标(地理坐标存完后一次计算)
					if (!points.empty() || !lines.empty() || !regions.empty()) {
						calculatGeodeticToImage(ar_elements_array, ar_infos_internal_to_store, _ar_mark_elements_guid, 
							timeStamp, width, height, ar_meta_data, points, lines, regions);
					}
					
					//存储到需要push rt的容器中去
					ar_infos_internal_to_push_rtc.ArElementsNum += ar_infos_internal_to_store.ArElementsNum;
					for(auto iter : ar_infos_internal_to_store.ArElementsArray){
						ar_infos_internal_to_push_rtc.ArElementsArray.push_back(iter);
					}

					//存储当前有新增的标注要素
					_ar_mark_pixel_and_geographic_map.insert(std::make_pair(_ar_mark_elements_guid, ar_infos_internal_to_store));
				}
				image->meta_data.ar_mark_info = ar_infos_internal_to_push_rtc;
			}
		}

		void DispatchTaskImpl::executeARProcess(Packet& packet)
		{
			if(_engines->_ar_engine && packet.metaDataValid())
			{
				if(_ar_image_compute_state)
				{
					_ar_image_compute_state = false;

					auto pts = packet->pts;//重写后的时间戳
					auto width = _codec_parameter.width;
					auto height = _codec_parameter.height;
					cv::Mat mat;
					JoFmvMetaDataBasic meta_temp = packet.getMetaDataBasic();

					jo::JoARMetaDataBasic ar_meta_data{};
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
					ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
					ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

					std::vector<cv::Point> pixel_points{};
					std::vector<std::vector<cv::Point>> pixel_lines{};

					std::vector<cv::Point> tmp_pixel_points{};
					std::vector<std::vector<cv::Point>> tmp_pixel_lines{};

					std::vector<std::vector<cv::Point>> pixel_warning_l1_regions{};
					std::vector<std::vector<cv::Point>> pixel_warning_l2_regions{};

					auto start_t_geodetic_to_image_first = std::chrono::system_clock::now();
					_engines->_ar_engine->frameProcess(true, pts, mat, width, height, ar_meta_data, pixel_points, pixel_lines
						, pixel_warning_l1_regions, pixel_warning_l2_regions);
					auto end_t_geodetic_to_image_first = std::chrono::system_clock::now();
					int64_t duration_geodetic_to_image_first =
						std::chrono::duration_cast<std::chrono::milliseconds>(end_t_geodetic_to_image_first - start_t_geodetic_to_image_first).count();

					for(const auto& iter : pixel_warning_l1_regions)
					{
						pixel_lines.push_back(iter);
					}
					for(const auto& iter : pixel_warning_l2_regions)
					{
						pixel_lines.push_back(iter);
					}
					printf("pixel_points size:%d \n", (int)pixel_points.size());
					printf("pixel_lines size:%d \n", (int)pixel_lines.size());
					printf("pixel_warning_l1_regions size:%d \n", (int)pixel_warning_l1_regions.size());
					printf("pixel_warning_l2_regions size:%d \n", (int)pixel_warning_l2_regions.size());


					// 筛选去掉屏幕外多余的点
					int i = 0;
					std::queue<int> ar_valid_point_index{}; // 筛选去掉屏幕外多余的点后有效点的索引
					for(const auto& iter : pixel_points)
					{
						if(iter.x <= width && iter.x >= 0 && iter.y <= height && iter.y >= 0)
						{
							tmp_pixel_points.push_back(iter);
							ar_valid_point_index.push(i);
						}
						++i;
					}
					for(std::size_t index = 0; index < pixel_lines.size(); ++index)
					{
						auto& iter_line = pixel_lines[index];

						std::vector<cv::Point> tmp_points{};

						bool is_in_screen{};

						for(std::size_t i = 0; i < iter_line.size(); ++i)
						{
							auto& iter = iter_line[i];
							if(iter.x <= width && iter.x >= 0 && iter.y <= height && iter.y >= 0)
							{
								is_in_screen = true;
							}

							if(!is_in_screen)
							{
								continue;
							}
							is_in_screen = false;

							if(i)
							{
								auto& front_iter = iter_line[i - 1];
								if(front_iter.x > width || front_iter.x < 0 || front_iter.y > height || front_iter.y < 0)
								{
									tmp_points.push_back(front_iter);
								}
							}

							tmp_points.push_back(iter);

							if(i < iter_line.size() - 1)
							{
								auto& back_iter = iter_line[i + 1];
								//警戒线是个环，不能break
								if(back_iter.x > width || back_iter.x < 0 || back_iter.y > height || back_iter.y < 0)
								{
									tmp_points.push_back(back_iter);
								}
							}
						}

						if(tmp_points.empty() && _ar_image_compute_pixel_lines.size() == pixel_lines.size())
						{
							auto& tmp_line = _ar_image_compute_pixel_lines[index];
							for(auto& iter : tmp_line)
							{
								tmp_points.push_back(iter);
							}

						}

						tmp_pixel_lines.push_back(tmp_points);
					}

					std::swap(_ar_valid_point_index, ar_valid_point_index);
					std::swap(_ar_image_compute_pixel_points, tmp_pixel_points);
					std::swap(_ar_image_compute_pixel_lines, tmp_pixel_lines);
				}
				else {
					_ar_image_compute_state = true;
				}

				JoFmvMetaDataBasic& meta_temp = packet.getMetaDataBasic();
				//将AR计算数据塞入元数据中
				meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArStatus = 1;
				meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArTroubleCode = 0;
				uint32_t element_index = meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArElementsNum;
				for(auto point : _ar_image_compute_pixel_points)
				{
					meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].Type = 0;
					meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].DotQuantity = 1;
					meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].X = point.x;
					meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].Y = point.y;
					++element_index;
				}
				uint32_t line_point_size{ 0 };
				for(auto line : _ar_image_compute_pixel_lines)
				{
					line_point_size += line.size();
					uint32_t element_line_point_index{ 0 };
					for(auto line_point : line)
					{
						meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].Type = 1;
						meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].DotQuantity = line.size();
						meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].X = line_point.x;
						meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].Y = line_point.y;
						meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].CurIndex = element_line_point_index;
						meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray[element_index].NextIndex = ++element_line_point_index;
						++element_index;
					}
				}

				meta_temp.GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArElementsNum += _ar_image_compute_pixel_points.size() + line_point_size;

				packet.setArPixelLines(_ar_image_compute_pixel_lines);
				packet.setArPixelPoints(_ar_image_compute_pixel_points);
				packet.setArValidPointIndex(_ar_valid_point_index);
				packet.setArVectorFile(_ar_vector_file);

				// TODO: 放入image的meta_data_basic，在json封装那里就不再封装 pixel_points pixel_lines ar_valid_point_index
				// 现在这种写法会导致AR计算结果无法嵌入视频SEI

				printf("_ar_image_compute_pixel_lines size:%d \n", (int)_ar_image_compute_pixel_lines.size());
				printf("_ar_image_compute_pixel_points size:%d \n", (int)_ar_image_compute_pixel_points.size());
				printf("_ar_valid_point_index size:%d \n", (int)_ar_valid_point_index.size());

			}
		}

		void DispatchTaskImpl::executeARMarkProcess(Packet& packet)
		{
			if(_engines->_ar_mark_engine && packet.metaDataValid()) {
				Poco::JSON::Object root;
				Poco::JSON::Parser reader;

				JoFmvMetaDataBasic& meta_temp = packet.getMetaDataBasic();
				uint64_t timeStamp = meta_temp.CarrierVehiclePosInfo_p.TimeStamp;

				jo::JoARMetaDataBasic ar_meta_data{};
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
				ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
				ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

				int width = _codec_parameter.width;
				int height = _codec_parameter.height;

				//先判断是否需要删除
				auto _ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.find(_mark_guid);
				while(_ar_mark_pixel_and_geographic != _ar_mark_pixel_and_geographic_map.end()
					&& _ar_mark_pixel_and_geographic->first == _mark_guid) {
					_ar_mark_pixel_and_geographic = _ar_mark_pixel_and_geographic_map.erase(_ar_mark_pixel_and_geographic);
				}

				if(_ar_mark_pixel_and_geographic_map.empty()) {
					destroyARMarkEngine();
				}

				_mark_guid = "";

				ArInfosInternal ar_infos_internal_to_push_rtc{};//存储以前的计算结果和当前新的标注要素，用来push rtc
				ArInfosInternal ar_infos_internal_to_store{};//这次如果有新增的标注元素，用这个先存下来，再放在map中

				//前面存的像素转地理，地理坐标每帧都要重新反算新的像素坐标;前面存的地理转像素，地理坐标每帧都要重新反算新的像素坐标
				for(auto iter_map = _ar_mark_pixel_and_geographic_map.begin();
					iter_map != _ar_mark_pixel_and_geographic_map.end(); iter_map++) {
					//地理坐标转图像坐标
					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					std::vector<jo::GeographicPosition> points{};
					std::vector<std::vector<jo::GeographicPosition>> lines{};
					std::vector<std::vector<jo::GeographicPosition>> regions{};

					// ar_infos_internal_to_push_rtc.ArElementsNum += iter_map->second.ArElementsNum;

					for(auto iter_list : iter_map->second.ArElementsArray) {
						tmpPoint.longitude = iter_list.lon;
						tmpPoint.latitude = iter_list.lat;
						tmpPoint.altitude = iter_list.HMSL;

						if(tmpPoint.longitude != 0 && tmpPoint.latitude != 0) {
							switch(iter_list.Type) {
							case 0:
							{
								points.push_back(tmpPoint);
								break;
							}
							case 1:
							{
								//添加每条线的各个物点
								if(iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
									tmpPointVct.push_back(tmpPoint);
								}
								//当前线已结束，添加整条线
								if(iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
									lines.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							case 2:
							{
								//添加每个面的各个物点
								if(iter_list.CurIndex != 0 || (iter_list.CurIndex == 0 && tmpPointVct.empty())) {
									tmpPointVct.push_back(tmpPoint);
								}
								//当前面已结束，添加整个面
								if(iter_list.NextIndex == 0 && !tmpPointVct.empty()) {
									regions.push_back(tmpPointVct);
									tmpPointVct.clear();
								}
								break;
							}
							default:
								break;
							}
						}
						if(!points.empty() || !lines.empty() || !regions.empty()) {
							calculatGeodeticToImage(iter_map->second.ArElementsArray, ar_infos_internal_to_push_rtc
								, iter_map->first, timeStamp, width, height, ar_meta_data, points, lines, regions);
						}
					}
				}

				while(!_video_mark_data.empty())
				{
					std::string mark_data = _video_mark_data.front();
					_video_mark_data.pop();

					Poco::Dynamic::Var result = reader.parse(mark_data);
					root = *(result.extract<Poco::JSON::Object::Ptr>());
					Poco::JSON::Array elementsArray = root.has("ArElementsArray") ? *(root.getArray("ArElementsArray")) : Poco::JSON::Array();
					int elementsArray_size = elementsArray.size();
					if(elementsArray_size <= 0) {
						continue;
					}

					jo::GeographicPosition tmpPoint{};
					std::vector<jo::GeographicPosition> tmpPointVct{};
					std::vector<jo::GeographicPosition> points{};
					std::vector<std::vector<jo::GeographicPosition>> lines{};
					std::vector<std::vector<jo::GeographicPosition>> regions{};

					//透传回云端
					_ar_mark_elements_guid = root.has("Guid") ? root.getValue<std::string>("Guid") : "";
					std::list<ArElementsInternal> ar_elements_array;
					for(int i = 0; i < (std::min)(elementsArray_size, 1024); ++i) {
						Poco::JSON::Object::Ptr elementJs = elementsArray.getObject(i);
						if(elementJs) {
							//图像坐标转地理坐标
							if(elementJs->has("X") && elementJs->has("Y")) {
								cv::Point pixel_point;
								jo::GeographicPosition geoPos{};

								int pixelX = elementJs->getValue<int32_t>("X");
								int pixelY = elementJs->getValue<int32_t>("Y");

								pixel_point.x = pixelX;
								pixel_point.y = pixelY;

								if(_engines->_ar_mark_engine) {
									_engines->_ar_mark_engine->projectImageToGeodetic(timeStamp, width, height, ar_meta_data, pixel_point, geoPos);
								}
								else {
									eap_error("_ar_mark_engine->projectImageToGeodetic but _ar_mark_engine is nullptr!----");
								}

								ArElementsInternal ar_elements_internal{};
								ar_elements_internal.X = pixel_point.x;
								ar_elements_internal.Y = pixel_point.y;
								ar_elements_internal.lat = geoPos.latitude;
								ar_elements_internal.lon = geoPos.longitude;
								ar_elements_internal.HMSL = geoPos.altitude;
								ar_elements_internal.Type = elementJs->getValue<uint32_t>("Type");
								ar_elements_internal.DotQuantity = elementJs->getValue<uint32_t>("DotQuantity");
								ar_elements_internal.Category = elementJs->getValue<uint32_t>("Category");
								ar_elements_internal.CurIndex = elementJs->getValue<uint32_t>("CurIndex");
								ar_elements_internal.NextIndex = elementJs->getValue<uint32_t>("NextIndex");
								ar_elements_internal.Guid = _ar_mark_elements_guid;

								ar_infos_internal_to_store.ArElementsArray.push_back(ar_elements_internal);
								ar_infos_internal_to_store.ArElementsNum++;
							}

							//地理坐标转图像坐标
							if(elementJs->has("lon") && elementJs->has("lat") && elementJs->has("HMSL")) {

								ArElementsInternal ar_elements_internal{};
								ar_elements_internal.Category = elementJs->getValue<uint32_t>("Category");
								ar_elements_internal.Type = elementJs->getValue<uint32_t>("Type");
								ar_elements_internal.DotQuantity = elementJs->getValue<uint32_t>("DotQuantity");
								ar_elements_internal.CurIndex = elementJs->getValue<uint32_t>("CurIndex");
								ar_elements_internal.NextIndex = elementJs->getValue<uint32_t>("NextIndex");
								ar_elements_internal.Guid = _ar_mark_elements_guid;
								ar_elements_array.push_back(ar_elements_internal);

								tmpPoint.longitude = elementJs->getValue<double>("lon");
								tmpPoint.latitude = elementJs->getValue<double>("lat");
								tmpPoint.altitude = elementJs->getValue<double>("HMSL");
								if(tmpPoint.longitude != 0 && tmpPoint.latitude != 0) {
									switch(elementJs->getValue<uint32_t>("Type")) {
									case 0:
									{
										points.push_back(tmpPoint);
										break;
									}
									case 1:
									{
										//添加每条线的各个物点
										if(elementJs->getValue<uint32_t>("CurIndex") != 0 || (elementJs->getValue<uint32_t>("CurIndex") == 0 && tmpPointVct.empty())) {
											tmpPointVct.push_back(tmpPoint);
										}
										//当前线已结束，添加整条线
										if(elementJs->getValue<uint32_t>("NextIndex") == 0 && !tmpPointVct.empty()) {
											lines.push_back(tmpPointVct);
											tmpPointVct.clear();
										}
										break;
									}
									case 2:
									{
										//添加每个面的各个物点
										if(elementJs->getValue<uint32_t>("CurIndex") != 0 || (elementJs->getValue<uint32_t>("CurIndex") == 0 && tmpPointVct.empty()))
										{
											tmpPointVct.push_back(tmpPoint);
										}
										//当前面已结束，添加整个面
										if(elementJs->getValue<uint32_t>("NextIndex") == 0 && !tmpPointVct.empty())
										{
											regions.push_back(tmpPointVct);
											tmpPointVct.clear();
										}
										break;
									}
									default:
										break;
									}
								}
							}
						}

					}

					//地理坐标转图像坐标(地理坐标存完后一次计算)
					if(!points.empty() || !lines.empty() || !regions.empty()) {
						calculatGeodeticToImage(ar_elements_array, ar_infos_internal_to_store, _ar_mark_elements_guid,
							timeStamp, width, height, ar_meta_data, points, lines, regions);
					}

					//存储到需要push rt的容器中去
					ar_infos_internal_to_push_rtc.ArElementsNum += ar_infos_internal_to_store.ArElementsNum;
					for(auto iter : ar_infos_internal_to_store.ArElementsArray)
					{
						ar_infos_internal_to_push_rtc.ArElementsArray.push_back(iter);
					}

					//存储当前有新增的标注要素
					_ar_mark_pixel_and_geographic_map.insert(std::make_pair(_ar_mark_elements_guid, ar_infos_internal_to_store));

					//if (!_is_video_mark) { _is_video_mark.store(true); }
					videoMarkMetaDataWrite(ar_infos_internal_to_store, packet.getOriginalPts());
					// break;//???
				}

				packet.setArMarkInfos(ar_infos_internal_to_push_rtc);
			}
		}
		
#endif
	
#ifdef ENABLE_AI
		void DispatchTaskImpl::destroyAIEngine()
		{
			if(_engines){
				std::lock_guard<std::mutex> ai_lock(_engines->_ai_engine_mutex);
				if (_engines->_ai_object_detector) {
					_engines->_ai_object_detector.reset();
				}

				if (_engines->_ai_mot_tracker) {
					_engines->_ai_mot_tracker.reset();
				}
				if ((_func_mask & FUNCTION_MASK_AI) == FUNCTION_MASK_AI) {
					_func_mask -= FUNCTION_MASK_AI;
					updateTaskFuncmask();
				}
			}
		}

#ifdef ENABLE_OPENSET_DETECTION
		void DispatchTaskImpl::destroyOpensetAIEngine()
				{
					if (_engines) {
						std::lock_guard<std::mutex> ai_lock(_engines->_openset_ai_engine_mutex);
						if (_engines->_openset_ai_object_detector) {
							_engines->_openset_ai_object_detector.reset();
						}
						prompt.clear();
						if ((_func_mask & FUNCTION_MASK_OPENSET_AI) == FUNCTION_MASK_OPENSET_AI) {
							_func_mask -= FUNCTION_MASK_OPENSET_AI;
							updateTaskFuncmask();
						}
					}
				}
#endif


        void DispatchTaskImpl::destroyAuxiliaryAIEngine()
        {
			if (_engines->_aux_ai_object_detector) {
				_engines->_aux_ai_object_detector.reset();
			}

			if (_engines->_aux_ai_mot_tracker) {
				_engines->_aux_ai_mot_tracker.reset();
			}
        }
#endif

#ifdef ENABLE_AR
		void DispatchTaskImpl::destroyAREngine()
		{
			if (_engines->_ar_engine) {
				_engines->_ar_engine->shutDown();
				_engines->_ar_engine.reset();
				eap_information("destroy arEngine successed!");
			}
		}
		void DispatchTaskImpl::destroyARMarkEngine()
		{
			if(_engines->_ar_mark_engine) {
				//_engines->_ar_mark_engine->shutdownReflect();
				_ar_related_mutex.lock();
				_engines->_ar_mark_engine.reset();
				_ar_related_mutex.unlock();

			}
		}
#endif

		void DispatchTaskImpl::destroyStablizer()
		{
		#ifdef ENABLE_STABLIZE
			if (_stablizer) {
				_stablizer.reset();
			}
		#endif
		}

		void DispatchTaskImpl::addAnnotationElements(std::string ar_camera_path, std::string annotation_elements_json, bool is_hd)
		{
#ifdef ENABLE_AR
			//每次有新的camera.config过来，都进行更新
			//TODO: 根据使用情况，可不给_ar_camera_config加锁
			if(_ar_camera_config != ar_camera_path) {
				_ar_camera_config = ar_camera_path;
				_is_update_ar_mark_engine.store(true);
			}
			if(!_engines->_ar_mark_engine) {
				_is_update_ar_mark_engine.store(true);
			}
			_video_mark_data.push(annotation_elements_json);
#endif
		}

		void DispatchTaskImpl::deleteAnnotationElements(std::string mark_guid, bool is_hd)
		{
#ifdef ENABLE_AR
			_mark_guid = mark_guid;
#endif
		}

		void DispatchTaskImpl::pushPacket(Packet packet)
		{
			//元数据封装SEI嵌入视频
			// if (packet.metaDataValid()) {
			// 	JoFmvMetaDataBasic meta_data = packet.getMetaDataBasic();
			// 	int meta_data_sei_buffer_size{};
			// 	auto meta_data_sei_buffer = _meta_data_processing_after->getSerializedBytesBySetMetaDataBasic(&meta_data, &meta_data_sei_buffer_size);
			// 	auto sei_h264 = MetaDataProcessing::seiDataAssemblyH264(meta_data_sei_buffer.data(), meta_data_sei_buffer.size());
			// 	if (!sei_h264.empty()) {
			// 		ByteArray_t meta_data_sei_buffer_ptr = MakeByteArray(meta_data_sei_buffer_size, meta_data_sei_buffer.data());
			// 		auto new_pkt = VideoExtraDataReplace(packet, _codec_parameter.codec_id, meta_data_sei_buffer_ptr);
			// 		packet.set(new_pkt);
			// 	}
			// }

			std::unique_lock<std::mutex> lock(_pushers_mutex);
			if (_pusher && _pusher->_air_pusher) {
				Packet new_packet;
				packet.copyTo(new_packet);
				_pusher->_air_pusher->pushPacket(new_packet);
			}
			if (_pusher && _pusher->_pusher) {
				_pusher->_pusher->pushPacket(packet);
			}
			lock.unlock();
		}

		void DispatchTaskImpl::pusherStopCallback(std::string url, int ret, std::string err_str)
		{
			auto weak_this = weak_from_this();
			eap::ThreadPool::defaultPool().start(
				[this, weak_this, url, ret, err_str] () {
				std::string desc = "pusher stoped, exit code: " + 
					std::to_string(ret) + ", description: " + err_str + " , url: "+ url+" , id: "+ _id;
				eap_error(desc);

				auto _this = weak_this.lock();
				if (!_this) {
					return;
				}
				if (_is_manual_stoped) {
					return;
				}
				std::string pilot_sn{};
				try {
					std::size_t index = _push_url.rfind("/");
					std::size_t second_index = _push_url.rfind("/", index - 1);
					pilot_sn = _push_url.substr(second_index + 1, index - second_index - 1);
				} catch (const std::exception& e) {
					eap_error(std::string(e.what()));
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				
				_reopen_pusher_cnt++;
				if (url == _pusher->_url) {
					int retryCnt{ 10 };
					while (retryCnt--) {
						if (_is_manual_stoped) {
							return;
						}
						try {
							openPusher();
							break;
						}
						catch (const std::exception& e) {
							eap_information_printf("reopen pusher failed, id: %s", _id);
							std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						}
					}
					if (retryCnt <= 0) {
						NoticeCenter::Instance()->getCenter().postNotification(
							new TaskStopedNotice(std::string(_id), desc, pilot_sn));
					}
				}
				else
					openAirPusher();

				if (!_is_pilotcontrol_task && _reopen_pusher_cnt > 200) {
					eap_information_printf("reopen pusher 200 times failed, id: %s", _id);
					NoticeCenter::Instance()->getCenter().postNotification(
						new TaskStopedNotice(std::string(_id), desc, pilot_sn));
				}
			});
		}

		void DispatchTaskImpl::SdcartMount(std::string device_name, std::string MountPoint)
		{
			// 定义文件指针
			FILE* fp = NULL;
			// 列出已挂载的文件系统 按照设备名 文件系统类型 以及挂载目录显示
			std::string op_f = std::string("mount | grep ") + device_name;
			// 以读入方式 执行op_f命令
			fp = popen(op_f.c_str(), "r");

			if (!fp)
			{
				perror("popen");
				std::cout << "abc" << std::endl;
				// return false;
			}
			else
			{
				eap_information("cba");
			}
			pclose(fp);
		}

		void DispatchTaskImpl::quittrackingSetParams()
		{
			//_communication_reactor->setTrackingStatus(false); // 跟踪状态//
			//_communication_reactor->setTrackStatusPilot(1);   // 告知飞控跟踪已脱锁//
			// _communication_reactor->setTrackHDOrSD(2);
		}

		void DispatchTaskImpl::updateFuncmaskL()
		{
			if ((_func_mask & FUNCTION_MASK_AI) == FUNCTION_MASK_AI) {
				_is_ai_on.store(true);
				_ai_status.store(true);
			}
			else {
				_is_ai_on.store(false);
				_ai_status.store(false);
			}
			if ((_func_mask & FUNCTION_MASK_OPENSET_AI) == FUNCTION_MASK_OPENSET_AI) {
				_is_openset_ai_on.store(true);
			}
			else {
				_is_openset_ai_on.store(false);
			}

			if ((_func_mask & FUNCTION_MASK_AR) == FUNCTION_MASK_AR) {
				_is_ar_on.store(true);
			}
			else {
				_is_ar_on.store(false);
			}

			if ((_func_mask & FUNCTION_MASK_DEFOG) == FUNCTION_MASK_DEFOG) {
				_is_defog_on.store(true);
				_is_image_enhancer_on = true;
			}
			else {
				_is_defog_on.store(false);
				_is_image_enhancer_on = false;
			}

			if ((_func_mask & FUNCTION_MASK_STABLE) == FUNCTION_MASK_STABLE) {
				_is_stable_on.store(true);
				// _is_image_stable_on = true;
			}
			else {
				_is_stable_on.store(false);
				// _is_image_stable_on = false;
			}

			if ((_func_mask & FUNCTION_MASK_VIDEO_RECORD) == FUNCTION_MASK_VIDEO_RECORD) {
				_is_video_record_on.store(true);
			}
			else {
				_is_video_record_on.store(false);
			}

			if ((_func_mask & FUNCTION_MASK_CLIP_SNAP_SHOT) == FUNCTION_MASK_CLIP_SNAP_SHOT) {
				_is_clip_video_record_on.store(true);
				clipMuxerRecordTimer();// 片段录像计时开始
			}
			else {
				_is_clip_video_record_on.store(false);
			}

			if ((_func_mask & FUNCTION_MASK_SNAP_SHOT) == FUNCTION_MASK_SNAP_SHOT) {
				_is_snap_shot_on.store(true);
			}
			else {
				_is_snap_shot_on.store(false);
			}

			if ((_func_mask & FUNCTION_MASK_AI_ASSIST_TRACK) == FUNCTION_MASK_AI_ASSIST_TRACK) {
				_is_ai_assist_track_on.store(true);
			}
			else {
				_is_ai_assist_track_on.store(false);
			}

			if ((_func_mask & FUNCTION_MASK_ENHANCED_AR) == FUNCTION_MASK_ENHANCED_AR) {
				_is_enhanced_ar_on.store(true);
			}
			else {
				_is_enhanced_ar_on.store(false);
			}
		}

#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)		
		void DispatchTaskImpl::trackingCoordTransform(CodecImagePtr image, const cv::Mat& M)
		{
			if (!image) {
				return;
			}

			cv::Mat p(3, 1, CV_32FC1);

			p.at<float>(0, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossX;
			p.at<float>(1, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossY;
			p.at<float>(2, 0) = 1;

			cv::Mat result = M * p;
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.ServoCrossX = result.at<float>(0, 0) / result.at<float>(2, 0);
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.ServoCrossY = result.at<float>(1, 0) / result.at<float>(2, 0);

			/////////////////////////////////////////////////////////////////////////////////////////////

			p.at<float>(0, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopX;
			p.at<float>(1, 0) = image->meta_data.meta_data_basic.
				GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopY;
			p.at<float>(2, 0) = 1;

			result = M * p;
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.TrackLeftTopX = result.at<float>(0, 0) / result.at<float>(2, 0);
			image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.TrackLeftTopY = result.at<float>(1, 0) / result.at<float>(2, 0);
		}

		void DispatchTaskImpl::arCoordTransform(CodecImagePtr image, const cv::Mat& M)
		{
			if (!image) {
				return;
			}

			for (auto& point : image->meta_data.pixel_points) {
				cv::Mat p(3, 1, CV_32FC1);

				p.at<float>(0, 0) = point.x;
				p.at<float>(1, 0) = point.y;
				p.at<float>(2, 0) = 1;

				cv::Mat result = M * p;
				point.x = result.at<float>(0, 0) / result.at<float>(2, 0);
				point.y = result.at<float>(1, 0) / result.at<float>(2, 0);
			}

			for (auto& line : image->meta_data.pixel_lines) {
				for (auto& point : line) {
					cv::Mat p(3, 1, CV_32FC1);

					p.at<float>(0, 0) = point.x;
					p.at<float>(1, 0) = point.y;
					p.at<float>(2, 0) = 1;

					cv::Mat result = M * p;
					point.x = result.at<float>(0, 0) / result.at<float>(2, 0);
					point.y = result.at<float>(1, 0) / result.at<float>(2, 0);
				}
			}
		}

		void DispatchTaskImpl::aiCoordTransform(CodecImagePtr image, const cv::Mat& M)
		{
			if (!image) {
				return;
			}

			for (int i = 0; i < image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
				ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize; ++i) {
				cv::Point point;
				point.x = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX;
				point.y = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopY;

				cv::Mat p(3, 1, CV_32FC1);

				p.at<float>(0, 0) = point.x;
				p.at<float>(1, 0) = point.y;
				p.at<float>(2, 0) = 1;

				cv::Mat result = M * p;

				point.x = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX = 
					result.at<float>(0, 0) / result.at<float>(2, 0);
				point.y = image->meta_data.meta_data_basic.GimbalPayloadInfos_p.
					ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray[i].DetLefttopX = 
					result.at<float>(1, 0) / result.at<float>(2, 0);
			}

		}
#endif
		PayloadData DispatchTaskImpl::findClosePayload(std::map<int64_t, PayloadData> payload_data_map, int64_t now_time_stamp)
		{
			//查找键值大于等于now_time_stamp的第一个元素，如果没有，返回end
			auto lower = payload_data_map.lower_bound(now_time_stamp);

			PayloadData payload_data{};

			if (lower == payload_data_map.begin()) {
				payload_data = lower->second;
				payload_data_map.erase(payload_data_map.begin());
				return payload_data;
			}

			if (lower == payload_data_map.end()) {
				auto last_iter = std::prev(payload_data_map.end());
				payload_data = last_iter->second;
				payload_data_map.erase(last_iter);
				return payload_data;
			}

			auto prev = std::prev(lower);
			int64_t prev_first_64 = (int64_t)prev->first;
			int64_t lower_first_64 = (int64_t)lower->first;

			if (std::abs((int64_t)now_time_stamp - prev_first_64) 
				<= std::abs((int64_t)now_time_stamp - lower_first_64)) {				
				payload_data = prev->second;
				payload_data_map.erase(prev->first);
				return payload_data;
			}
			else {
				payload_data = lower->second;
				payload_data_map.erase(lower->first);
				return payload_data;
			}
			
			return payload_data;
		}

		PilotData DispatchTaskImpl::findClosePilot(std::map<int64_t, PilotData> pilot_data_map, int64_t now_time_stamp)
		{
			//查找键值大于等于now_time_stamp的第一个元素，如果没有，返回end
			auto lower = pilot_data_map.lower_bound(now_time_stamp);

			PilotData pilot_data{};

			if (lower == pilot_data_map.begin()) {				
				pilot_data = lower->second;
				pilot_data_map.erase(pilot_data_map.begin());
				return pilot_data;
			}

			if (lower == pilot_data_map.end()) {
				auto last_iter = std::prev(pilot_data_map.end());				
				pilot_data = last_iter->second;
				pilot_data_map.erase(last_iter);
				return pilot_data;
			}

			auto prev = std::prev(lower);
			int64_t prev_first_64 = (int64_t)prev->first;
			int64_t lower_first_64 = (int64_t)lower->first;

			if (std::abs((int64_t)now_time_stamp - prev_first_64) 
				<= std::abs((int64_t)now_time_stamp - lower_first_64)) {				
				pilot_data = prev->second;
				pilot_data_map.erase(prev->first);
				return pilot_data;
			}
			else {
				pilot_data = lower->second;
				pilot_data_map.erase(lower->first);
				return pilot_data;
			}
			
			return pilot_data;
		}

		int DispatchTaskImpl::getCpuTemperature()
		{
			FILE *fd;
			int temp;
			char buff[256];

			fd = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
			fgets(buff, sizeof(buff), fd);
			sscanf(buff, "%d", &temp);

			fclose(fd);

			return temp / 1000;
		}

        void DispatchTaskImpl::sendDataToUdp()
        {
            bool is_land{};
            uint8_t byte{};
            int32_t value{};
            uint64_t count{};
            int freq_hz{ 100 };
			int grade{ configInstance().getInt(Vehicle::KARSeatGrade) };

            float takeoff_step{};
            float abs_pitch_step{};
            float abs_roll_step{};

            float about{};
            float around{};
            float up_down{};    //-0.4~0.21
            float pitch{};      //-0.6~0.6
            float roll{};       //-2.3~2.3
            float yaw{};
            std::vector<uint8_t> buffer;

			_ar_seat_thread_run = true;
            while (_ar_seat_thread_run) {
                if (_apmode <= 4) {
                    // 非飞行阶段都不发送数据
                    up_down = 0;
                    pitch = 0;
                    roll = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }

                buffer.clear();
                buffer.reserve(56);
                buffer.push_back(0x55);
                buffer.push_back(0xAA);
                buffer.push_back(0xBB);
                buffer.push_back(0x06);

                if (_is_forecast) {
                    if (!_is_first_get_value) {
                        // 在小的那个值的基础上依次增加步长或者在大的那个值的基础上依次减小步长
                        if ((_drone_pitch_latest >= 0 && _drone_pitch >= 0) || (_drone_pitch_latest < 0 && _drone_pitch < 0)) {
                            abs_pitch_step = std::abs(_drone_pitch - _drone_pitch_latest) / (freq_hz / 10);
                            if (_drone_pitch < _drone_pitch_latest) {
                                pitch += abs_pitch_step;
                            }
                            else {
                                pitch -= abs_pitch_step;
                            }
                        }
                        else if (_drone_pitch_latest < _drone_pitch) {
                            abs_pitch_step = (_drone_pitch - _drone_pitch_latest) / (freq_hz / 10);
                            pitch -= abs_pitch_step;
                        }
                        else if (_drone_pitch_latest > _drone_pitch) {
                            abs_pitch_step = (_drone_pitch_latest - _drone_pitch) / (freq_hz / 10);
                            pitch += abs_pitch_step;
                        }

                        if ((_drone_roll_latest >= 0 && _drone_roll >= 0) || (_drone_roll_latest < 0 && _drone_roll < 0)) {
                            abs_roll_step = std::abs(_drone_roll - _drone_roll_latest) / (freq_hz / 10);
                            if (_drone_roll < _drone_roll_latest) {
                                roll += abs_roll_step;
                            }
                            else {
                                roll -= abs_roll_step;
                            }
                        }
                        else if (_drone_roll_latest < _drone_roll) {
                            abs_roll_step = (_drone_roll - _drone_roll_latest) / (freq_hz / 10);
                            roll -= abs_roll_step;
                        }
                        else if (_drone_roll_latest > _drone_roll) {
                            abs_roll_step = (_drone_roll_latest - _drone_roll) / (freq_hz / 10);
                            roll += abs_roll_step;
                        }
                    }
                }
                else {
                    _is_forecast = true;

                    pitch = _drone_pitch;
                    roll = _drone_roll;
                }

                if (grade == 1) {
                    pitch = pitch > 0.3 ? 0.3 : pitch;
                    pitch = pitch < -0.3 ? -0.3 : pitch;
                    roll = roll > 1.2 ? 1.2 : roll;
                    roll = roll < -1.2 ? -1.2 : roll;
                }
                else if (grade == 2) {
                    pitch = pitch > 0.45 ? 0.45 : pitch;
                    pitch = pitch < -0.45 ? -0.45 : pitch;
                    roll = roll > 1.8 ? 1.8 : roll;
                    roll = roll < -1.8 ? -1.8 : roll;
                }
                else {
                    pitch = pitch > 0.6 ? 0.6 : pitch;
                    pitch = pitch < -0.6 ? -0.6 : pitch;
                    roll = roll > 2.3 ? 2.3 : roll;
                    roll = roll < -2.3 ? -2.3 : roll;
                }

                //if (_apmode >= 4 && _apmode <= 7) {
                //    // 起飞
                //    pitch = 0;
                //    roll = 0;
                //    yaw = 0;
                //    if (!is_land) {
                //        is_land = true;
                //        takeoff_step = (0.21 - up_down) / freq_hz / 5;
                //    }
                //    if (up_down < 0.21) {
                //        up_down += takeoff_step;
                //    }
                //}
                //else if (_apmode >= 10 && _apmode <= 13) {
                //    // 降落
                //    pitch = 0;
                //    roll = 0;
                //    yaw = 0;
                //    if (is_land) {
                //        is_land = false;
                //        takeoff_step = (0.4 + up_down) / freq_hz / 8;
                //    }
                //    up_down -= takeoff_step;
                //}

                //std::cout << count << " *******************roll: " << roll << std::endl;
                //std::cout << count << " *******************pitch: " << pitch << std::endl;
                //std::cout << std::endl;
                //++count;

                // 左右位移
                value = *reinterpret_cast<int32_t*>(&about);
                for (int i{}; i < 4; ++i) {
                    // 小端
                    byte = (value >> (i * 8)) & 0xFF;
                    // 大端
                    //byte = (value >> (24 - i * 8)) & 0xFF;
                    buffer.push_back(byte);
                }

                // 前后位移
                value = *reinterpret_cast<int32_t*>(&around);
                for (int i{}; i < 4; ++i) {
                    // 小端
                    byte = (value >> (i * 8)) & 0xFF;
                    // 大端
                    //byte = (value >> (24 - i * 8)) & 0xFF;
                    buffer.push_back(byte);
                }

                // 上下位移
                value = *reinterpret_cast<int32_t*>(&up_down);
                for (int i{}; i < 4; ++i) {
                    // 小端
                    byte = (value >> (i * 8)) & 0xFF;
                    // 大端
                    //byte = (value >> (24 - i * 8)) & 0xFF;
                    buffer.push_back(byte);
                }

                // 俯仰，单位弧度，范围:正负13度
                value = *reinterpret_cast<int32_t*>(&pitch);
                for (int i{}; i < 4; ++i) {
                    // 小端
                    byte = (value >> (i * 8)) & 0xFF;
                    // 大端
                    //byte = (value >> (24 - i * 8)) & 0xFF;
                    buffer.push_back(byte);
                }

                // 滚转，单位弧度，范围:正负13度
                value = *reinterpret_cast<int32_t*>(&roll);
                for (int i{}; i < 4; ++i) {
                    // 小端
                    byte = (value >> (i * 8)) & 0xFF;
                    // 大端
                    //byte = (value >> (24 - i * 8)) & 0xFF;
                    buffer.push_back(byte);
                }

                // 偏航，单位弧度，范围:正负13度
                value = *reinterpret_cast<int32_t*>(&yaw);
                for (int i{}; i < 4; ++i) {
                    // 小端
                    byte = (value >> (i * 8)) & 0xFF;
                    // 大端
                    //byte = (value >> (24 - i * 8)) & 0xFF;
                    buffer.push_back(byte);
                }

                for (int i{}; i < 24; ++i) {
                    buffer.push_back(0x00);
                }

                // time：固定为100
                buffer.push_back(0x64);
                for (int i{}; i < 3; ++i) {
                    buffer.push_back(0x00);
                }

                // 发送数据到六轴AR座椅
                _udp_socket->sendData(buffer.data(), buffer.size());

                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / freq_hz));
            }

			_apmode = 1;
        }

        void DispatchTaskImpl::parseMetaData(const JoFmvMetaDataBasic* meta_data)
        {
			if (meta_data) {
                _apmode = meta_data->CarrierVehicleStatusInfo_p.APModeStates;
                std::cout << " *******************apmode: " << _apmode << std::endl;

                float roll = meta_data->CarrierVehiclePosInfo_p.CarrierVehicleRollAngle / 10000.0 * 57.2957795131;
                float pitch = meta_data->CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle / 10000.0 * 57.2957795131;
                std::cout << " *******************roll: " << roll << std::endl;
                std::cout << " *******************pitch: " << pitch << std::endl;
                std::cout << std::endl;

                // 飞机滚转正负30度、俯仰正负15度
                roll = (roll + 30) / 60;
                pitch = (pitch + 15) / 30;

				if (configInstance().getInt(Vehicle::KARSeatGrade) == 1) {
					roll = roll * 2.4 - 1.2;
					pitch = pitch * 0.6 - 0.3;
				}
                else if (configInstance().getInt(Vehicle::KARSeatGrade) == 2) {
                    roll = roll * 3.6 - 1.8;
                    pitch = pitch * 0.9 - 0.45;
                }
                else {
					// 归一化到座椅正负10度
                    roll = roll * 4.6 - 2.3;
                    pitch = pitch * 1.2 - 0.6;
				}

                if (!_is_first_get_value) {
                    _drone_roll = _drone_roll_latest;
                    _drone_pitch = _drone_pitch_latest;
                }
                _drone_roll_latest = roll;
                _drone_pitch_latest = pitch;

                if (_is_first_get_value) {
                    _is_first_get_value = false;
                }
                _is_forecast = false;
			}
        }

		void DispatchTaskImpl::multiNetWorkCardFiltering(std::string url, std::chrono::milliseconds time_out, int frame_rate, Demuxer::StopCallback stop_callback, Demuxer::PacketCallback packet_callback)
		{
			//根据网卡创建多个demuxer，不管网卡的status是up还是down，当是up时，都去打开demuxer
			auto start_t = std::chrono::system_clock::now();
			while (!_is_manual_stoped) {
				//lan
				if (common::NetWorkChecking::Instance()->HaveLanAdapter()) {
					auto lan_adapter_infos = common::NetWorkChecking::Instance()->GetLanAdapterArray();
					_adapter_num += lan_adapter_infos.size();
					int i = 0;
					for (auto& lan_adapter_info : lan_adapter_infos) {
						eap_information_printf("find lan_adapter_info.address:%s-------", lan_adapter_info.address);

						auto demuxer_reactor = creatDemuxerReactor(url, time_out, frame_rate, stop_callback, packet_callback, lan_adapter_info, i);
						_lan_demuxer_reactors.push_back(std::move(demuxer_reactor));
						++i;
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
					}
				}

				//wlan
				if (common::NetWorkChecking::Instance()->HaveWLanAdapter()) {
					auto wlan_adapter_infos = common::NetWorkChecking::Instance()->GetWLanAdapterArray();
					_adapter_num += wlan_adapter_infos.size();
					int i = 0;
					for (auto& wlan_adapter_info : wlan_adapter_infos) {

						eap_information_printf("find wlan_adapter_info.address:%s-----", wlan_adapter_info.address);
						auto demuxer_reactor = creatDemuxerReactor(url, time_out, frame_rate, stop_callback, packet_callback, wlan_adapter_info, i);
						_wlan_demuxer_reactors.push_back(std::move(demuxer_reactor));
						++i;
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
					}
				}
				eap_information("create demuxer reactor start!");
				if (_wlan_demuxer_reactors.size() != 0 || _lan_demuxer_reactors.size() != 0) {
					eap_information("create demuxer reactor successed!");
					break;
				} else {
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}
			}
			bool have_demuxer_opened{ false };
			// 先根据用户输入的ip进行查找网卡
			int tmpIndex{ 0 };
			for (auto& lan_demuxer_reactor : _lan_demuxer_reactors) {
				if (lan_demuxer_reactor->IsOpened() && lan_demuxer_reactor->GetNetworkAdapterAddress() == _adapter_ip) {
					eap_information_printf("lan network adapter opened, adapter name:%s , adapter address: %s"
						, lan_demuxer_reactor->GetNetworkAdapterName(), _adapter_ip);

					_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::Lan;
					_current_network_index = tmpIndex;
					have_demuxer_opened = true;
					break;
				}
				++tmpIndex;
			}
			if (!have_demuxer_opened) {
				tmpIndex = 0;
				for (auto wlan_demuxer_reactor : _wlan_demuxer_reactors) {
					if (wlan_demuxer_reactor->IsOpened() && wlan_demuxer_reactor->GetNetworkAdapterAddress() == _adapter_ip) {
						eap_information_printf( "wlan network adapter opened, adapter name: %s, adapter address: %s"
							, wlan_demuxer_reactor->GetNetworkAdapterName(), _adapter_ip);

						_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::WLan;
						_current_network_index = tmpIndex;
						have_demuxer_opened = true;
						break;
					}
					++tmpIndex;
				}
			}
			//如果前面就一个成功打开的都没有，就会陷入死循环（如果刚开始所有网卡都没有流就会陷入这种情况）
			//暂时改成：全部轮询一遍，都没有打开的，就直接退出。按照 网卡数量*设置的超时时间+1个超时时间 来退出，因为open是异步的
			for (; !_is_stop_reactor_loop && !have_demuxer_opened;) {
				bool is_finded{};
				int index{};
				for (auto& lan_demuxer_reactor : _lan_demuxer_reactors) {
					if (lan_demuxer_reactor->IsOpened()) {
						is_finded = true;
						eap_information_printf("lan network adapter opened, adapter name: %s, adapter address: %s"
							, lan_demuxer_reactor->GetNetworkAdapterName(), lan_demuxer_reactor->GetNetworkAdapterAddress());
						break;
					}
					++index;
					std::this_thread::sleep_for(std::chrono::milliseconds(30));
				}

				if (is_finded) {
					//std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
					_current_network_adapter_type = _lan_demuxer_reactors[index]->GetNetworkAdapterType();
					_current_network_index = index;
					have_demuxer_opened = true;
					break;
				}

				index = 0;
				for (auto wlan_demuxer_reactor : _wlan_demuxer_reactors) {
					if (wlan_demuxer_reactor->IsOpened()) {
						is_finded = true;

						// eap_information_printf("wlan network adapter opened, adapter name: %s, adapter address: %s"
						// 	, wlan_demuxer_reactor->GetNetworkAdapterName(), wlan_demuxer_reactor->GetNetworkAdapterAddress());

						break;
					}
					++index;
					std::this_thread::sleep_for(std::chrono::milliseconds(30));
				}

				if (is_finded) {
					//std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
					_current_network_adapter_type = _wlan_demuxer_reactors[index]->GetNetworkAdapterType();
					_current_network_index = index;
					have_demuxer_opened = true;
					break;
				}

				//当所有网卡都没有流，退出
				auto now_t = std::chrono::system_clock::now();
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now_t - start_t).count()
					>= (time_out.count() * (_adapter_num + 1)))
				{
					// std::string error_description = "all adapter can't open, no video stream" + url;
					// eap_information(error_description);
					// throw std::system_error(-1, std::system_category(), error_description);
					return;
				}
			}

			if (_is_stop_reactor_loop) {
				return;
			}

			if (have_demuxer_opened) {
				//std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
				if (_current_network_adapter_type == common::NetWorkChecking::NetworkAdapterType::Lan) {
					_lan_demuxer_reactors[_current_network_index]->StartCache();
				}
				else if (_current_network_adapter_type == common::NetWorkChecking::NetworkAdapterType::WLan) {
					_wlan_demuxer_reactors[_current_network_index]->StartCache();
				}
			}
		}

		DemuxerReactorPtr DispatchTaskImpl::creatDemuxerReactor(std::string url, std::chrono::milliseconds time_out, int frame_rate, Demuxer::StopCallback stop_callback, Demuxer::PacketCallback packet_callback, common::NetWorkChecking::AdapterInfo adapter_info, int i)
		{
			DemuxerReactorPtr demuxer_reactor = std::make_shared<DemuxerReactor>
			(_id, adapter_info.name, adapter_info.type, adapter_info.status,
				adapter_info.address, url, time_out, frame_rate,
				std::bind([this, stop_callback](int exit_code, int index)
					{
						if (index == _current_network_index) {
							if (stop_callback) {
								stop_callback(exit_code);
							}
						}
					}, std::placeholders::_1, i),
				std::bind([this](Packet packet, int index, Demuxer::PacketCallback packet_callback)
					{
						if (index == _current_network_index) {
							if (packet_callback) {
								//若中间断开，切换后接上的视频流与原来不是同一条，且编解码参数不一样，那么会导致后续的解码，编码出现问题
								packet_callback(packet);
							}
						}
					}, std::placeholders::_1, i, packet_callback),
				std::bind([this](int index)
					{
						std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
						if (index == _current_network_index) {
							_lan_demuxer_reactors[_current_network_index]->StartCache();
						}
						if (_stream_timeout_recover_callback) {
							_stream_timeout_recover_callback(1);
						}
					}, i),
				std::bind([this]()
					{
						if (_stream_timeout_recover_callback) {
							_stream_timeout_recover_callback(0);
						}
					})
			);
			return demuxer_reactor;
		}

		void DispatchTaskImpl::reactorLoopThread()
		{
			for (; !_is_stop_reactor_loop;) {
				if (_is_manual_stoped)
					break;

				if (_is_pull_udp) {
					if (_is_manual_stoped)
						break;
					auto start_t = std::chrono::system_clock::now();
					if(_reactor_thread_looptimes % 600 == 0)
						eap_information_printf( "reactor loop thread loop times: %d", (int)_reactor_thread_looptimes);
					_reactor_thread_looptimes++;
					if(!_is_demuxer_opened)
						_is_demuxer_opened = true;
					//检查是否有网卡状态变成up的，如果就，就去打开demuxer
					checkNetworkAdapterStatusChange([this](_networkCheckResult check_result)
					{

					});

					if (_current_network_adapter_type == common::NetWorkChecking::NetworkAdapterType::Lan) {
						if (_current_network_index < _lan_demuxer_reactors.size() && _current_network_index >= 0 && _lan_demuxer_reactors[_current_network_index]->IsTimeout()){
							_lan_demuxer_reactors[_current_network_index]->StopCache();

							bool is_success_switch = false;
							for (std::size_t i = 0; i < _lan_demuxer_reactors.size(); ++i) {
								if (i != _current_network_index &&
									_lan_demuxer_reactors[i]->IsOpened() &&
									!_lan_demuxer_reactors[i]->IsTimeout()) {

									std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
									_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::Lan;
									_current_network_index = i;
									_lan_demuxer_reactors[i]->StartCache();
									 eap_information_printf( "switch to lan adapter %s, address %s"
									 	, _lan_demuxer_reactors[i]->GetNetworkAdapterName(), _lan_demuxer_reactors[i]->GetNetworkAdapterAddress());

									is_success_switch = true;
									break;
								}
							}

							if (!is_success_switch) {
								for (std::size_t i = 0; i < _wlan_demuxer_reactors.size(); ++i) {
									if (_wlan_demuxer_reactors[i]->IsOpened() &&
										!_wlan_demuxer_reactors[i]->IsTimeout()) {

										std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
										_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::WLan;
										_current_network_index = i;

										_wlan_demuxer_reactors[i]->StartCache();

										 eap_information_printf( "switch to wlan adapter %s, address %s"
										 	, _wlan_demuxer_reactors[i]->GetNetworkAdapterName(), _wlan_demuxer_reactors[i]->GetNetworkAdapterAddress());

										is_success_switch = true;
										break;
									}
								}
							}
						}
					}
					else if (_current_network_adapter_type == common::NetWorkChecking::NetworkAdapterType::WLan) {
						bool is_success_switch = false;
						for (std::size_t i = 0; i < _lan_demuxer_reactors.size(); ++i) {
							if (_lan_demuxer_reactors[i]->IsOpened() &&
								!_lan_demuxer_reactors[i]->IsTimeout()) {
								_lan_demuxer_reactors[_current_network_index]->StopCache();

								std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
								_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::Lan;
								_current_network_index = i;

								_lan_demuxer_reactors[i]->StartCache();

								 eap_information_printf("switch to lan adapter %s, address %s"
								 	, _lan_demuxer_reactors[i]->GetNetworkAdapterName(), _lan_demuxer_reactors[i]->GetNetworkAdapterAddress());

								is_success_switch = true;
								break;
							}
						}

						if (!is_success_switch) {
							for (std::size_t i = 0; i < _wlan_demuxer_reactors.size(); ++i) {
								if (i != _current_network_index &&
									_wlan_demuxer_reactors[i]->IsOpened() &&
									!_wlan_demuxer_reactors[i]->IsTimeout()) {
									_wlan_demuxer_reactors[_current_network_index]->StopCache();

									std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
									_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::WLan;
									_current_network_index = i;

									_wlan_demuxer_reactors[i]->StartCache();

									 eap_information_printf( "switch to wlan adapter %s, address %s"
									 	, _wlan_demuxer_reactors[i]->GetNetworkAdapterName(), _wlan_demuxer_reactors[i]->GetNetworkAdapterAddress());

									is_success_switch = true;
									break;
								}
							}
						}
					}

					auto end_t = std::chrono::system_clock::now();
					auto stime = std::chrono::milliseconds(2000);
					auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_t - start_t);
					if (elapsed >= stime) {
						std::this_thread::sleep_for(std::chrono::milliseconds(200));
						continue;
					}
					else {
						auto sleep_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stime - elapsed).count();
						for (std::int64_t ela = 0; ela < sleep_elapsed && !_is_stop_reactor_loop; ela += 200) {
							std::this_thread::sleep_for(std::chrono::milliseconds(200));
						}
					}


				}
				else {
					std::this_thread::sleep_for(std::chrono::milliseconds(2000));
					continue;
				}
			}
		}

		void DispatchTaskImpl::checkNetworkAdapterStatusChange(std::function<void(_networkCheckResult result)> callback)
		{
			bool is_demuxer_open{};
			if (_current_network_index == -1) {
				for (std::size_t j = 0; j < _lan_demuxer_reactors.size(); ++j) {
					if (_lan_demuxer_reactors[j]->IsOpened()) {
						is_demuxer_open = true;
						_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::Lan;
						_current_network_index = j;
						break;
					}
				}
			}
			if (_current_network_index == -1) {
				for (std::size_t j = 0; j < _wlan_demuxer_reactors.size(); ++j) {
					if (_wlan_demuxer_reactors[j]->IsOpened()) {
						is_demuxer_open = true;
						_current_network_adapter_type = common::NetWorkChecking::NetworkAdapterType::WLan;
						_current_network_index = j;
						break;
					}
				}
			}
			if (common::NetWorkChecking::Instance()->HaveLanAdapter()) {
				auto adapter_infos = common::NetWorkChecking::Instance()->GetLanAdapterArray();
				//识别到新增网卡
				if (adapter_infos.size() > _lan_demuxer_reactors.size()) {
					for (std::size_t i = 0; i < adapter_infos.size(); ++i) {
						if (_lan_demuxer_reactors.size()) {
							for (std::size_t j = 0; j < _lan_demuxer_reactors.size(); ++j) {
								if (_lan_demuxer_reactors[j]->GetNetworkAdapterName() == adapter_infos[i].name) {
									break;
								}
								else if (j == (_lan_demuxer_reactors.size() - 1)) {
									auto demuxer_reactor = creatDemuxerReactor(_init_parameter.pull_url, _open_timeout, 30,
										_demuxer_stop_callback, _demuxer_packet_callback, adapter_infos[i], _lan_demuxer_reactors.size());
									_lan_demuxer_reactors.push_back(std::move(demuxer_reactor));
									eap_information_printf("new_adapter_info ip: %s", adapter_infos[i].address);
								}
							}
						}
						else {
							auto demuxer_reactor = creatDemuxerReactor(_init_parameter.pull_url, _open_timeout, 30,
								_demuxer_stop_callback, _demuxer_packet_callback, adapter_infos[i], _lan_demuxer_reactors.size());
							_lan_demuxer_reactors.push_back(std::move(demuxer_reactor));
							eap_information_printf("new_adapter_info ip: %s", adapter_infos[i].address);
						}
					}
				}
				for (std::size_t i = 0; i < adapter_infos.size(); ++i) {
					//eap_information_printf("loop reactor find lan_adapter_info.address: %s, i = %d", adapter_infos[i].address, (int)i);
					for (std::size_t j = 0; j < _lan_demuxer_reactors.size(); ++j) {
						if (_lan_demuxer_reactors[j]->GetNetworkAdapterName() == adapter_infos[i].name && !adapter_infos[i].address.empty() &&
							((_lan_demuxer_reactors[j]->GetNetworkAdapterStatus() !=
							adapter_infos[i].status) || !is_demuxer_open)) {
							_networkCheckResult result{};
							result.address = adapter_infos[i].address;
							result.name = adapter_infos[i].name;
							result.new_status = adapter_infos[i].status;
							result.type = adapter_infos[i].type;
							result.index = j;

							std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
							_lan_demuxer_reactors[j]->SetStatusAddress(result.new_status, result.address);

							if (callback) {
								callback(result);
							}
						}
					}
				}
			}
			if (common::NetWorkChecking::Instance()->HaveWLanAdapter()) {
				auto adapter_infos = common::NetWorkChecking::Instance()->GetWLanAdapterArray();
				//识别到新增网卡
				if (adapter_infos.size() > _wlan_demuxer_reactors.size()) {
					for (std::size_t i = 0; i < adapter_infos.size(); ++i) {
						if (_wlan_demuxer_reactors.size()) {
							for (std::size_t j = 0; j < _wlan_demuxer_reactors.size(); ++j) {
								if (_wlan_demuxer_reactors[j]->GetNetworkAdapterName() == adapter_infos[i].name) {
									break;
								}
								else if (j == (_wlan_demuxer_reactors.size() - 1)) {
									auto demuxer_reactor = creatDemuxerReactor(_init_parameter.pull_url, _open_timeout, 30, _demuxer_stop_callback, _demuxer_packet_callback, adapter_infos[i], _wlan_demuxer_reactors.size());
									_wlan_demuxer_reactors.push_back(std::move(demuxer_reactor));
									eap_information_printf("new_adapter_info ip: %s", adapter_infos[i].address);
								}
							}
						}
						else {
							auto demuxer_reactor = creatDemuxerReactor(_init_parameter.pull_url, _open_timeout, 30, _demuxer_stop_callback, _demuxer_packet_callback, adapter_infos[i], _wlan_demuxer_reactors.size());
							_wlan_demuxer_reactors.push_back(std::move(demuxer_reactor));
							eap_information_printf("new_adapter_info ip: %s", adapter_infos[i].address);
						}
					}
				}
				for (std::size_t i = 0; i < adapter_infos.size(); ++i) {
					//eap_information_printf("loop reactor find wlan_adapter_info.address: %s, i = %d", adapter_infos[i].address, (int)i);
					for (std::size_t j = 0; j < _wlan_demuxer_reactors.size(); ++j) {
						if (_wlan_demuxer_reactors[j]->GetNetworkAdapterName() == adapter_infos[i].name &&
							((_wlan_demuxer_reactors[j]->GetNetworkAdapterStatus() !=
							adapter_infos[i].status) || !is_demuxer_open)) {
							_networkCheckResult result{};
							result.address = adapter_infos[i].address;
							result.name = adapter_infos[i].name;
							result.new_status = adapter_infos[i].status;
							result.type = adapter_infos[i].type;
							result.index = j;

							std::lock_guard<std::mutex> lock(_network_adapter_switch_mutex);
							_wlan_demuxer_reactors[j]->SetStatusAddress(result.new_status, result.address);

							if (callback) {
								callback(result);
							}
						}
					}
				}
			}

		}

		void DispatchTaskImpl::dangerLoopThread()
		{
#if defined(ENABLE_AI) && defined(ENABLE_AR)
			_danger_photo_loop_thread = std::thread([this]() {
				if (_is_manual_stoped)
					return;
				_danger_photo_loop_thread_run.store(true);
				for (; _danger_photo_loop_thread_run;) {
					std::unique_lock<std::mutex> lock(_danger_queue_mutex);
					if (_danger_images.empty()) { // 两个队列都是同时往里放数据，判断其中任一即可
						_danger_queue_cv.wait_for(lock, std::chrono::milliseconds(500));
					}

					if (!_danger_photo_loop_thread_run) {
						break;
					}

					if (_danger_images.empty()) {
						continue;
					}

					auto image = _danger_images.front();
					_danger_images.pop();
					auto ai_detect_ret = _danger_ai_ret.front();
					_danger_ai_ret.pop();
					lock.unlock();

					std::string danger_photo_server_url{};
					try {
						GET_CONFIG(std::string, getString, my_danger_photo_server_url, AI::KDangerPhotoServerUrl);
						danger_photo_server_url = my_danger_photo_server_url;
					}
					catch (const std::exception& e) {
						eap_error_printf("get config throw exception: %s", e.what());
					}
					
					auto http_client = HttpClient::createInstance();
					//上传图片
					std::string base64_encoded{};
					cv::Mat bgr24_image_cpu;// 是否需要初始化
					try {
						image->width = image->bgr24_image.cols;
						image->height = image->bgr24_image.rows;
						bgr24_image_cpu.create(image->bgr24_image.size(), image->bgr24_image.type()); // 初始化 cpuImage
						image->bgr24_image.download(bgr24_image_cpu);
						std::vector<uchar> buffer;
						cv::imencode(".jpg", bgr24_image_cpu, buffer, { cv::IMWRITE_JPEG_QUALITY, 80 });
						base64_encoded = "data:image/jpg;base64," + encodeBase64({ buffer.begin(), buffer.end() });
						bgr24_image_cpu.release();
						image->bgr24_image.release();
					}
					catch (const std::exception& e) {
						bgr24_image_cpu.release();
						image->bgr24_image.release();
						eap_error(std::string(e.what()));
						//continue;
					}
					
					std::vector<cv::Rect> cv_rect{};
					std::vector<jo::WarningInfo> warning_info{};

#ifdef ENABLE_DJI_OBJ_RETURN
					// DJI OBJ RETURN 模式：根据是否有元数据选择地理坐标计算方式
					{
						if (base64_encoded.empty()) {
							eap_warning("danger base64_encoded is empty");
						}

						// 有元数据：用 AR 引擎计算各检测框的精确地理坐标
						if (image->meta_data.meta_data_valid && _engines->_ar_engine) {
							for (const auto& ret : ai_detect_ret) {
								cv_rect.push_back(ret.Bounding_box);
							}
							jo::JoARMetaDataBasic ar_meta_data{};
							JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
							ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
							ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
							ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
							ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
							ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
							ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
							ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;
							int width = image->width;
							int height = image->height;
							warning_info = _engines->_ar_engine->getWarningInfo(cv_rect, width, height, ar_meta_data);
						}

						Poco::JSON::Array json_warning_info_array;
						for (std::size_t i = 0; i < ai_detect_ret.size(); ++i) {
							Poco::JSON::Object elementJs;
							Poco::JSON::Array pixelPositionArr;
							auto object = ai_detect_ret[i];
							elementJs.set("identifyType", object.cls);
							elementJs.set("reliability", object.confidence);
							pixelPositionArr.add(object.Bounding_box.x);
							pixelPositionArr.add(object.Bounding_box.y);
							pixelPositionArr.add(object.Bounding_box.width);
							pixelPositionArr.add(object.Bounding_box.height);
							elementJs.set("pixelPosition", pixelPositionArr);
							// wargetPosition：有元数据且 AR 引擎有效时用 AR 引擎结果，否则用载体位置作为 fallback
							Poco::JSON::Array wargetPositionArr;
							if (image->meta_data.meta_data_valid
								&& _engines->_ar_engine
								&& i < warning_info.size()
								&& !isnanf(warning_info[i].target_position.longitude)
								&& !isnanf(warning_info[i].target_position.latitude)
								&& warning_info[i].target_position.longitude != 0
								&& warning_info[i].target_position.latitude != 0) {
								wargetPositionArr.add(warning_info[i].target_position.longitude);
								wargetPositionArr.add(warning_info[i].target_position.latitude);
								wargetPositionArr.add(warning_info[i].target_position.altitude);
								eap_information_printf("[DJI-GEO] AR engine wargetPosition: lon=%.8f lat=%.8f alt=%.2f",
									warning_info[i].target_position.longitude,
									warning_info[i].target_position.latitude,
									warning_info[i].target_position.altitude);
							} else {
								// 无元数据：使用载体位置作为 fallback
								JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
								double fallback_lon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon * 1e-7;
								double fallback_lat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat * 1e-7;
								double fallback_alt = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL * 1e-2;
								wargetPositionArr.add(fallback_lon);
								wargetPositionArr.add(fallback_lat);
								wargetPositionArr.add(fallback_alt);
								eap_warning_printf("[DJI-GEO] no valid AR geo, fallback to vehicle pos: lon=%.8f lat=%.8f alt=%.2f",
									fallback_lon, fallback_lat, fallback_alt);
							}
							elementJs.set("wargetPosition", wargetPositionArr);
							json_warning_info_array.add(elementJs);
						}

						std::size_t index = _push_url.rfind("/");
						std::size_t second_index = _push_url.rfind("/", index - 1);
						std::string pilot_sn = _push_url.substr(second_index + 1, index - second_index - 1);

						Poco::JSON::Object json;
						json.set("file", base64_encoded);
						json.set("autopilotSn", pilot_sn);
						json.set("warningInfo", json_warning_info_array);
						{
							std::ostringstream warning_oss;
							json_warning_info_array.stringify(warning_oss);
							eap_information_printf("[addDangerPhoto] POST %s",
								(danger_photo_server_url + "/api/order/v1/aiEventFile/addDangerPhoto").c_str());
							eap_information_printf("[addDangerPhoto] autopilotSn=%s, file_size=%zu, warningInfo=%s",
								pilot_sn.c_str(), base64_encoded.size(), warning_oss.str().c_str());
						}
						http_client->doHttpRequest(danger_photo_server_url + "/api/order/v1/aiEventFile/addDangerPhoto", jsonToString(json), [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
							if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
								try {
									Poco::JSON::Parser parser;
									auto dval = parser.parse(response);
									auto obj = dval.extract<Poco::JSON::Object::Ptr>();
									int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
									if (code != 200) {
										std::string msg = obj->getValue<std::string>("msg");
										eap_error_printf("addDangerPhoto post failed, http status=%d, return code=%d, msg=%s", (int)status, code, msg);
									}
									else {
										eap_information("addDangerPhoto post succeed");
									}
								}
								catch (...) {
									eap_warning("addDangerPhoto post failed");
								}
							}
							else {
								eap_warning("addDangerPhoto post failed");
							}
						});
					}
#else
					if (image->meta_data.meta_data_valid) {
						for (const auto& ret : ai_detect_ret) {
							cv_rect.push_back(ret.Bounding_box);
						}

						jo::JoARMetaDataBasic ar_meta_data{};
						JoFmvMetaDataBasic meta_temp = image->meta_data.meta_data_basic;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLon = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleLat = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat;
						ar_meta_data.CarrierVehiclePosInfo_p.CarrierVehicleHMSL = meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL;

						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical;
						ar_meta_data.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR;

						int width = image->width;
						int height = image->height;
						warning_info = _engines->_ar_engine->getWarningInfo(cv_rect, width, height, ar_meta_data);
						Poco::JSON::Array json_warning_info_array;
						for (int i = 0; i < std::min(warning_info.size(), ai_detect_ret.size()); ++i) {
							jo::WarningInfo warning_info_elem = warning_info.at(i);
							if (isnanf(warning_info_elem.target_position.longitude) || isnanf(warning_info_elem.target_position.latitude)) {

								eap_warning("isnanf(warning_info_elem.target_position.longitude) || isnanf(warning_info_elem.target_position.latitude)");
								continue;
							}
							if (warning_info_elem.target_position.longitude == 0 || warning_info_elem.target_position.latitude == 0) {

								eap_warning("warning_info_elem.target_position.longitude == 0 or warning_info_elem.target_position.latitude == 0");
								continue;
							}

							Poco::JSON::Object elementJs;
							elementJs.set("warningLevel", warning_info_elem.warning_level);
							Poco::JSON::Array wargetPositionArr, pixelPositionArr;
							wargetPositionArr.add((warning_info_elem.target_position.longitude));
							wargetPositionArr.add((warning_info_elem.target_position.latitude));
							wargetPositionArr.add((warning_info_elem.target_position.altitude));
							elementJs.set("wargetPosition", wargetPositionArr);

							auto object = ai_detect_ret[i];
							elementJs.set("identifyType", object.cls);
							elementJs.set("reliability", object.confidence);
							pixelPositionArr.add(object.Bounding_box.x);
							pixelPositionArr.add(object.Bounding_box.y);
							pixelPositionArr.add(object.Bounding_box.width);
							pixelPositionArr.add(object.Bounding_box.height);
							elementJs.set("pixelPosition", pixelPositionArr);
							json_warning_info_array.add(elementJs);
						}

						if (json_warning_info_array.empty()) {
							eap_warning_printf("image->width = %d", width);
							eap_warning_printf("image->height = %d", height);
							eap_warning_printf("cv_rect.size = %d", (int)cv_rect.size());
							eap_warning_printf("warning_info.size = %d", (int)warning_info.size());
							eap_warning_printf("ai_detect_ret.size = %d", (int)ai_detect_ret.size());
							eap_warning("json_warning_info_array is empty");
							continue;
						}
						if(base64_encoded.empty()){
							eap_warning("danger base64_encoded is empty");
						}

						std::size_t index = _push_url.rfind("/");
						std::size_t second_index = _push_url.rfind("/", index - 1);
						std::string pilot_sn = _push_url.substr(second_index + 1, index - second_index - 1);

						Poco::JSON::Object json;
						json.set("file", base64_encoded);
						json.set("autopilotSn", pilot_sn);
						json.set("warningInfo", json_warning_info_array);
						json.set("gimbal_pan", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan * 1e-4);
						json.set("gimbal_tilt", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt * 1e-4);
						json.set("gimbal_roll", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll * 1e-4);
						json.set("vehicle_lat", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLat * 1e-7);
						json.set("vehicle_lon", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleLon * 1e-7);
						json.set("vehicle_hmsl", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHMSL * 1e-2);
						json.set("vehicle_heading", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle * 1e-4);
						json.set("vehicle_pitch", meta_temp.CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle * 1e-4);
						json.set("vehicle_roll", meta_temp.CarrierVehiclePosInfo_p.CarrierVehicleRollAngle * 1e-4);
						json.set("frame_roll", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll * 1e-4);
						json.set("frame_pan", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan * 1e-4);
						json.set("frame_tilt", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt * 1e-4);
						json.set("visual_horitontal", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal * 1e-4 * 180.0 / M_PI);
						json.set("tgt_hmsl", meta_temp.GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL);

						http_client->doHttpRequest(danger_photo_server_url + "/api/order/v1/aiEventFile/addDangerPhoto", jsonToString(json), [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
							if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
								try {
									Poco::JSON::Parser parser;
									auto dval = parser.parse(response);

									auto obj = dval.extract<Poco::JSON::Object::Ptr>();
									int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
									if (code != 200) {
										std::string msg = obj->getValue<std::string>("msg");
										eap_error_printf("addDangerPhoto post failed, http status=%d, return code=%d, msg=%s", (int)status, code, msg);
									}
									else {
										eap_information("addDangerPhoto post succeed");
									}
								}
								catch (...) {
									eap_warning("addDangerPhoto post failed");
								}
							}
							else {
								eap_warning("addDangerPhoto post failed");
							}
						});
					}
				}
#endif // ENABLE_DJI_OBJ_RETURN
				}
			});
#endif //  ENABLE_AI
		}

        void DispatchTaskImpl::recordLoopThread()
        {
			_record_loop_thread = std::thread([this]() {
				if (_is_manual_stoped)
					return;
				_record_loop_thread_run.store(true);
				for (; _record_loop_thread_run;) {
					Packet record_pkt;
					{
						std::unique_lock<std::mutex> lock(_record_queue_mutex);
						if (_record_packets.empty()) { // 两个队列都是同时往里放数据，判断其中任一即可
							_record_queue_cv.wait_for(lock, std::chrono::milliseconds(500));
						}

						if (!_record_loop_thread_run || _is_manual_stoped)
							break;

						if (_record_packets.empty())
							continue;

						record_pkt = _record_packets.front();
						_record_packets.pop();
					}
					if (_record && !_is_recording) {//片段视频录制
						std::lock_guard<std::mutex> lock(_pusher_recode_mutex);
						try {
							if (!_pusher_tradition_recode)
								_pusher_tradition_recode = PusherTradition::createInstance();

							GET_CONFIG(std::string, getString, media_server_ip, General::kMediaServerIp);
							_recode_start_time_point = std::chrono::system_clock::now();
							auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(_recode_start_time_point.time_since_epoch()).count();
							_recode_start_timestamp_str = get_current_time_string_second_compact();
							auto url = "rtsp://" + media_server_ip + "/" + _id + "/" + _recode_start_timestamp_str;
							_pusher_tradition_recode->open(_id, url, _timebase, _framerate,
								_codec_parameter, std::chrono::milliseconds(3000));
							eap_information_printf("hd recode push url: %s", url);
							_is_recording = true;
							_record = false;
							_is_start_recording = false;
						}
						catch (const std::exception& e) {
							auto err_msg = std::string(e.what());
							eap_information_printf("--record pusher open failed, msg: %s", err_msg);
							if (_pusher_tradition_recode)
								_pusher_tradition_recode.reset();
						}
					}

					if (_is_recording) {
						std::lock_guard<std::mutex> lock(_pusher_recode_mutex);
						GET_CONFIG(std::string, getString, media_server_url, General::kMediaServerUrl);
						GET_CONFIG(std::string, getString, media_server_secret, General::kMediaServerSecret);
						auto cli = HttpClient::createInstance();
						Poco::JSON::Object json;
						json.set("secret", media_server_secret);
						json.set("type", 1); //1 mp4
						json.set("app", _id);
						json.set("vhost", "__defaultVhost__");
						json.set("stream", _recode_start_timestamp_str);

						auto tt = std::chrono::system_clock::to_time_t
						(_recode_start_time_point);
						struct tm* ptm = localtime(&tt);
						char date[60] = { 0 };
						sprintf(date, "%d-%02d-%02d",
							(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday);
						json.set("period", std::string(date));
						auto json_string = jsonToString(json);
						if (!_is_start_recording) {
							cli->doHttpRequest(media_server_url + "/index/api/isRecording", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
								if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
									try {
										Poco::JSON::Parser parser;
										auto dval = parser.parse(response);
										auto obj = dval.extract<Poco::JSON::Object::Ptr>();
										int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
										auto status = obj && obj->has("status") ? obj->getValue<bool>("status") : false;
										if (code == 0 && status) {
											_is_start_recording = true;
										}
										else {
											if(!_is_start_recording){
												cli->doHttpRequest(media_server_url + "/index/api/startRecord", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
													if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
														Poco::JSON::Parser parser;
														auto dval = parser.parse(response);
														auto obj = dval.extract<Poco::JSON::Object::Ptr>();
														int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
														auto result = obj && obj->has("result") ? obj->getValue<bool>("result") : false;
														if (code == 0 && result) {
															_recode_start_time_point = std::chrono::system_clock::now();
															_is_start_recording = true;
															eap_information_printf("startRecord post successed!, url: %s", _init_parameter.push_url);
														}
													}
												});
											}
										}
									}
									catch (...) {
										eap_warning("isRecording post failed");
									}
								}
								else {
									eap_warning("isRecording post failed");
								}
							});
						}

						auto push_proc = [this, &cli, json_string](Packet& packet) {
							if (_pusher_tradition_recode) {
								_pusher_tradition_recode->pushPacket(packet);
							}
							auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _recode_start_time_point).count();
							if (_is_start_recording && duration >= _record_duration) {
								cli->doHttpRequest(media_server_url + "/index/api/stopRecord", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
									if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
										eap_information_printf("stopRecord post successed!, url: %s", _init_parameter.push_url);
									}
								});
								{
									std::unique_lock<std::mutex> lock(_record_queue_mutex);
									while(!_record_packets.empty()){
										_record_packets.pop();
									}
								}
								if (_pusher_tradition_recode)
									_pusher_tradition_recode.reset();

								cli->doHttpRequest(media_server_url + "/index/api/getMp4RecordFile", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
									if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
										try {
											Poco::JSON::Parser parser;
											auto dval = parser.parse(response);
											auto obj = dval.extract<Poco::JSON::Object::Ptr>();
											int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
											if (code == 0) {
												GET_CONFIG(std::string, getString, media_server_url, General::kMediaServerUrl);
												auto recode_url = media_server_url;
												auto data = obj && obj->has("data") ? obj->getObject("data") : Poco::JSON::Object::Ptr();
												auto rootPath = data && data->has("rootPath") ? data->getValue<std::string>("rootPath") : "";
												auto path = data && data->has("paths") ? data->getArray("paths")->getElement<std::string>(0) : "";
												recode_url += rootPath.substr(5, rootPath.length() - 5) + path;
												eap_information_printf("recode http url : %s", recode_url);
												

												Poco::JSON::Object json;
												json.set("code", 0);
												json.set("record_url", recode_url);
												json.set("is_hd", true);
												std::size_t index = _push_url.rfind("/");
												std::size_t second_index = _push_url.rfind("/", index - 1);
												std::string pilot_sn = _push_url.substr(second_index + 1, index - second_index - 1);
												json.set("autopilotSn", pilot_sn);
												json.set("record_no", _recordNo);
												GET_CONFIG(std::string, getString, video_record_url, General::kVideoClipRecordUrl);
												auto cli = HttpClient::createInstance();
												auto json_string = jsonToString(json);
												cli->doHttpRequest(video_record_url + "/flightmonitor/custom/v1/file/addVideo", json_string, [&](Poco::Net::HTTPResponse::HTTPStatus status, std::istream& response) {
													if (Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK == status) {
														try {
															Poco::JSON::Parser parser;
															auto dval = parser.parse(response);

															auto obj = dval.extract<Poco::JSON::Object::Ptr>();
															int code = obj && obj->has("code") ? obj->getValue<int>("code") : 0;
															if (code != 0) {
																std::string msg = obj && obj->has("code") ? obj->getValue<std::string>("msg") : "";
																eap_error_printf("record post failed, http status=%d, return code=%d, msg=%s", (int)status, code, msg);
															}
															else {
																eap_information("record post succeed");
															}
														}
														catch (...) {
															eap_warning("addVideo post failed");
														}
													}
													else {
														eap_warning("addVideo post failed");
													}
												});
											}
										}
										catch (...) {
											eap_warning("getMp4RecordFile post failed");
										}
									}
									else {
										eap_warning("getMp4RecordFile post failed");
									}
								});
								eap_information("hd recode end!");
								_is_recording = false;
								_is_start_recording = false;
							}
						};

						auto meta_data_raw = record_pkt.getSeiBuf();
						if (!meta_data_raw.empty() && _meta_data_processing_pret) {
							JoFmvMetaDataBasic metadata = record_pkt.getMetaDataBasic();
							int meta_data_sei_buffer_size{};
							meta_data_raw = _meta_data_processing_pret->getSerializedBytesBySetMetaDataBasic(&metadata, &meta_data_sei_buffer_size);
							auto sei_buffer = MetaDataProcessing::seiDataAssemblyH264(
								meta_data_raw.data(), meta_data_raw.size());
							if (!sei_buffer.empty()) {
								AVPacket* pkt_new = av_packet_alloc();
								int new_packet_size = record_pkt->size + sei_buffer.size();
								if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
									int pos = 0;
									memcpy(pkt_new->data, sei_buffer.data(), sei_buffer.size());
									pos += sei_buffer.size();
									memcpy(pkt_new->data + pos, record_pkt->data, record_pkt->size);
									pkt_new->pts = record_pkt->pts;
									pkt_new->dts = record_pkt->dts;
									pkt_new->duration = record_pkt->duration;
									pkt_new->flags = record_pkt->flags;

									Packet packet_export(pkt_new);
									packet_export.setSeiBuf(meta_data_raw);
									packet_export.setMetaDataBasic(&record_pkt.getMetaDataBasic());
									packet_export.metaDataValid() = record_pkt.metaDataValid();
	#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
									packet_export.setArPixelPoints(record_pkt.getArPixelPoints());
									packet_export.setArPixelLines(record_pkt.getArPixelLines());
									packet_export.setArPixelWarningL1s(record_pkt.getArPixelWarningL1s());
									packet_export.setArPixelWarningL2s(record_pkt.getArPixelWarningL2s());
	#endif
									packet_export.setCurrentTime(record_pkt.getCurrentTime());
									packet_export.setArMarkInfos(record_pkt.getArInfos());
									packet_export.setArValidPointIndex(record_pkt.getArValidPointIndex());
									packet_export.setArVectorFile(record_pkt.getArVectorFile());
									push_proc(packet_export);
								}
								else {
									push_proc(record_pkt);
								}
							}
							else {
								push_proc(record_pkt);
							}
						}
						else {
							push_proc(record_pkt);
						}
					}

				}
			});
        }
#ifdef ENABLE_PIO
		void DispatchTaskImpl::convertToMetaDataBasic(std::shared_ptr<JoFmvMetaDataBasic> meta_data, IrAiMsg ir_ai_msg){
			#if 0
			eap_information("AIStatus: " + std::to_string(int(ir_ai_msg.AIStatus)));
			eap_information("AIDetcInfoSize: " + std::to_string(int(ir_ai_msg.AIDetcInfoSize)));
			for(int i=0;i<int(ir_ai_msg.AIDetcInfoSize);++i){
				auto ir_ai_pio=ir_ai_msg.AIDetcInfoArray[i];
				eap_information("i: " + std::to_string(i));
				eap_information("DetLefttopX: " + std::to_string(int(ir_ai_pio.DetLefttopX)));
				eap_information("DetLefttopY: " + std::to_string(int(ir_ai_pio.DetLefttopY)));
				eap_information("DetWidth: " + std::to_string(int(ir_ai_pio.DetWidth)));
				eap_information("DetHeight: " + std::to_string(int(ir_ai_pio.DetHeight)));
				eap_information("DetTGTclass: " + std::to_string(int(ir_ai_pio.DetTGTclass)));
				eap_information("TgtConfidence: " + std::to_string(int(ir_ai_pio.TgtConfidence)));
				eap_information("FireArea: " + std::to_string(int(ir_ai_pio.FireArea)));
				eap_information("FireTemperature: " + std::to_string(int(ir_ai_pio.FireTemperature)));
				eap_information("--------------------------------------------");
				
			}
			#endif
			meta_data->IRAiInfos_p.IRAIStatus=ir_ai_msg.AIStatus;
			meta_data->IRAiInfos_p.IRAIDetcInfoSize=ir_ai_msg.AIDetcInfoSize;
			for(int i=0;i<meta_data->IRAiInfos_p.IRAIDetcInfoSize;++i){
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].DetLefttopX=ir_ai_msg.AIDetcInfoArray[i].DetLefttopX;
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].DetLefttopY=ir_ai_msg.AIDetcInfoArray[i].DetLefttopY;
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].DetWidth=ir_ai_msg.AIDetcInfoArray[i].DetWidth;
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].DetHeight=ir_ai_msg.AIDetcInfoArray[i].DetHeight;
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].DetTGTclass=ir_ai_msg.AIDetcInfoArray[i].DetTGTclass;
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].TgtConfidence=ir_ai_msg.AIDetcInfoArray[i].TgtConfidence;
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].FireArea=ir_ai_msg.AIDetcInfoArray[i].FireArea;
				meta_data->IRAiInfos_p.IRAIDetcInfoArray[i].FireTemperature=ir_ai_msg.AIDetcInfoArray[i].FireTemperature;
			}
		}
#endif
        void DispatchTaskImpl::convertToMetaDataBasic(std::shared_ptr<JoFmvMetaDataBasic> meta_data
			, PayloadData close_payload_data, PilotData close_pilot_data)
		{
			// 飞机pos数据
			// 下面计算的时间戳时格林威治事件 1970年01月01日00时00分00秒起至当前的秒数 GMT=UTC+0
			uint8_t hour_t = close_pilot_data.hour /*+ close_pilot_data.TimeZoneNum*/;
			uint8_t hour = hour_t >= 24 ? hour_t - 24 : hour_t;
			uint8_t day = hour_t >= 24 ? close_pilot_data.day + 1 : close_pilot_data.day;
			tm tm1;
			tm tm2;
			memset(&tm1, 0, sizeof(tm));
			memset(&tm2, 0, sizeof(tm));
			tm1.tm_sec = close_pilot_data.second;
			tm1.tm_min = close_pilot_data.minute;
			tm1.tm_hour = hour;
			tm1.tm_mday = day;
			tm1.tm_mon = close_pilot_data.month - 1;
			tm1.tm_year = close_pilot_data.year + 2000 - 1900;
			auto cost = mktime(&tm1);
			int64_t currentSeconds = cost;

			meta_data->CarrierVehiclePosInfo_p.TimeStamp = currentSeconds * 1000;

			std::string time_zone_num{ "8" };
			try {
				GET_CONFIG(std::string, getString, my_time_zone_num, General::KTimeZoneNum);
				time_zone_num = my_time_zone_num;
				meta_data->CarrierVehiclePosInfo_p.TimeZone = /*close_payload_data.TxData_p.TimeZoneNum*/std::stoi(time_zone_num);
			}
			catch (const std::exception& e) {
				eap_error_printf("get config throw exception: %s", e.what());
			}

			meta_data->CarrierVehiclePosInfo_p.CarrierVehicleHeadingAngle = close_pilot_data.Euler2;
			meta_data->CarrierVehiclePosInfo_p.CarrierVehiclePitchAngle = close_pilot_data.Euler1;
			meta_data->CarrierVehiclePosInfo_p.CarrierVehicleRollAngle = close_pilot_data.Euler0;
			meta_data->CarrierVehiclePosInfo_p.CarrierVehicleLat = close_pilot_data.lat;
			meta_data->CarrierVehiclePosInfo_p.CarrierVehicleLon = close_pilot_data.lon;
			meta_data->CarrierVehiclePosInfo_p.CarrierVehicleHMSL = close_pilot_data.HMSL * 100;
			meta_data->CarrierVehiclePosInfo_p.HMSL=close_pilot_data.HMSL;
			meta_data->CarrierVehiclePosInfo_p.DisFromHome = close_pilot_data.DisFromHome;
			meta_data->CarrierVehiclePosInfo_p.HeadingFromHome = close_pilot_data.HeadFromHome;
			meta_data->CarrierVehiclePosInfo_p.VGnd = close_pilot_data.VGnd;
			meta_data->CarrierVehiclePosInfo_p.Tas = close_pilot_data.Tas;
			meta_data->CarrierVehiclePosInfo_p.VNorth = close_pilot_data.Vnorth;
			meta_data->CarrierVehiclePosInfo_p.VEast = close_pilot_data.Veast;
			meta_data->CarrierVehiclePosInfo_p.VDown = close_pilot_data.Vdown;
			meta_data->CarrierVehiclePosInfo_p.FlySeconds = close_payload_data.TxData_p.FlySeconds;

			// 飞机状态数据
			memcpy(meta_data->CarrierVehicleStatusInfo_p.CarrierVehicleSN, "00000000000000000000000000000000", 32);
			memset(meta_data->CarrierVehicleStatusInfo_p.CarrierVehicleID, 0, sizeof(meta_data->CarrierVehicleStatusInfo_p.CarrierVehicleID));
			memcpy(meta_data->CarrierVehicleStatusInfo_p.CarrierVehicleID, "00000000000000000000000000000000", 32);
			meta_data->CarrierVehicleStatusInfo_p.CarrierVehicleFirmwareVersion = 0;
			meta_data->CarrierVehicleStatusInfo_p.VeclType = close_pilot_data.VeclType;
			meta_data->CarrierVehicleStatusInfo_p.Pdop = close_pilot_data.pdop;
			meta_data->CarrierVehicleStatusInfo_p.NumSV = close_pilot_data.numSV;
			meta_data->CarrierVehicleStatusInfo_p.Orienteering = close_pilot_data.pdop <= 1 ? 0 : 1;
			meta_data->CarrierVehicleStatusInfo_p.RPM = close_pilot_data.RPM;
			meta_data->CarrierVehicleStatusInfo_p.ThrottleCmd = close_pilot_data.ThrottleCmd;
			meta_data->CarrierVehicleStatusInfo_p.MPowerV = close_pilot_data.MpowerV;
			meta_data->CarrierVehicleStatusInfo_p.MPowerA = close_pilot_data.MPowerA;
			meta_data->CarrierVehicleStatusInfo_p.ElecricQuantity = (50.2 - close_pilot_data.MPowerA * 50.f) / 8.2 * 100.f;
			meta_data->CarrierVehicleStatusInfo_p.ScoutTask = close_pilot_data.ScoutTask;
			meta_data->CarrierVehicleStatusInfo_p.LineInspecMode = close_pilot_data.ModeState & 0x07;
			meta_data->CarrierVehicleStatusInfo_p.AutoPlan = close_pilot_data.ModeState >> 6 & 0x03;
			meta_data->CarrierVehicleStatusInfo_p.RTTMaunalActing = close_pilot_data.ModeState >> 8 & 0x01;
			meta_data->CarrierVehicleStatusInfo_p.APModeStates = close_pilot_data.apmodestates;
			meta_data->CarrierVehicleStatusInfo_p.TasCmd = close_pilot_data.TasCmd;
			meta_data->CarrierVehicleStatusInfo_p.HeightCmd = close_pilot_data.HeightCmd;
			meta_data->CarrierVehicleStatusInfo_p.WypNum = close_pilot_data.WypNum;

			// 吊舱pos数据
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleHorizontal = close_payload_data.TxData_p.VisualFOVH;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical = close_payload_data.TxData_p.VisualFOVV;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.InfaredViewAngleHorizontal = close_payload_data.TxData_p.InfaredFOVH;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.InfaredViewAngleVertical = close_payload_data.TxData_p.InfaredFOVV;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.FocalDistance = 0;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalPan = close_payload_data.GimbalData_p.pan;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalTilt = close_payload_data.GimbalData_p.Tilt;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.GimbalRoll = close_payload_data.TxData_p.GimbalRoll;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.FramePan = close_payload_data.GimbalData_p.Framepan;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.FrameTilt = close_payload_data.GimbalData_p.FrameTilt;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.FrameRoll = 0;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLat = close_pilot_data.TGTlat;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.TGTLon = close_pilot_data.TGTlon;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHMSL = close_pilot_data.TGTHMSL;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.TGTVelocity = close_pilot_data.TargetVelocity;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.TGTHeading = close_pilot_data.TargetHeading;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.SlantR = close_pilot_data.SlantR;
			if (meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical == 0) {
				float ViewAngleHorizonal = close_payload_data.TxData_p.VisualFOVH / 10000.f;
				meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.VisualViewAngleVertical =
					atan(0.5625 * (tan(ViewAngleHorizonal / 2.0))) * 2.0 * 10000;
			}
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.Elevation = close_pilot_data.ModeState >> 5 & 0x01;
			//红外测温数据
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.IrThermometerBackData_P.num = close_payload_data.IrThermometerBack_p.num;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.IrThermometerBackData_P.msgid = close_payload_data.IrThermometerBack_p.msgid;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.IrThermometerBackData_P.Time = close_payload_data.IrThermometerBack_p.Time;
			//meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.IrThermometerBackData_P.TargetTempsList[0] = close_payload_data.IrThermometerBack_p.TargetTemp_p;//TODO: 这个应该是数组数据

			// 吊舱状态数据						
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.GMPower = close_payload_data.GimbalData_p.GMPower;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd = close_payload_data.TxData_p.SeroveCmd;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.SeroveInit = close_payload_data.TxData_p.SeroveInit;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd0 = close_pilot_data.ServoCmd0;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.ServoCmd1 = close_pilot_data.ServoCmd1;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.PixelElement = 0;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.GimbalDeployCmd = close_pilot_data.GimbalDeployCmd;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.W_or_B = close_payload_data.TxData_p.W_or_B;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.FovLock = close_payload_data.TxData_p.FovLock;
			meta_data->GimbalPayloadInfos_p.GimbalStatusInfo_p.GimbalCalibrate = close_pilot_data.ModeState >> 3 & 0x01;

			// 图像处理板数据
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.Version = close_payload_data.TxData_p.VersionNum;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SearchWidth = close_payload_data.TxData_p.SearchWidth;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SearchHeight = close_payload_data.TxData_p.SearchHeight;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossX = close_payload_data.TxData_p.ServoCrossX;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossY = close_payload_data.TxData_p.ServoCrossY;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossWidth = close_payload_data.TxData_p.ServoCrossWidth;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ServoCrossHeight = close_payload_data.TxData_p.ServoCrossheight;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopX = close_payload_data.TxData_p.TrackLefttopX;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackLeftTopY = close_payload_data.TxData_p.TrackLefttopY;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackWidth = close_payload_data.TxData_p.TrackWidth;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackHeight = close_payload_data.TxData_p.TrackHeight;

			// TODO: 没有计算的接口，暂时赋值为0
			//{
			//	JO::Motion::PoseData pose =
			//	{

			//		_frame_pos_type->Euler[2] * 1E-4,  // Yaw Angle   (Rads)
			//		_frame_pos_type->Euler[1] * 1E-4,  // Pitch Angle (Rads)
			//		_frame_pos_type->Euler[0] * 1E-4,  // Roll Angle  (Rads)

			//		_frame_pos_type->lat * 1E-7,       // Latitude    (Degs)
			//		_frame_pos_type->lon * 1E-7,       // Longitude   (Degs)
			//		_frame_pos_type->HMSLCM * 1E-2,    // Altitude    (m)


			//		_frame_pos_type->Framepan * 1E-4,
			//		_frame_pos_type->FrameTilt * 1E-4,
			//		0,                           // Spin Angle  (Rads)

			//		_frame_pos_type->ViewAngleHorizonal * 1E-4,// View Angle  (Rads)
			//		_frame_pos_type->TGTHMSL,          // Elevation   (m)
			//	};

			//	oCamera.SetPoseData(pose);
			//	JO::Spatial::Coordinates fcps = oCamera.GetImageControls();
			//	Matrix3d affine_matrix = MakeAffine(1920, 1080, fcps);

			//	JO::Spatial::Coordinates pxs(3, 5);//3*4
			//	pxs <<
			//		JO::Spatial::Coordinate(1, 0, 0),  // 左上
			//		JO::Spatial::Coordinate(1, 1920, 0),   // 右上
			//		JO::Spatial::Coordinate(1, 1920, 1080),   // 右下
			//		JO::Spatial::Coordinate(1, 0, 1080),// 左下
			//		JO::Spatial::Coordinate(1, 960, 540);  // 中点
			//	JO::Spatial::Coordinates ips = affine_matrix * pxs;
			//	auto f = oCamera.GetFocalLength();
			//	JO::Spatial::Coordinate offset;
			//	offset << 0, 0, -1 - f;
			//	ips.colwise() += offset;
			//	JO::Spatial::GeoCoordinates gps = oCamera.ProjectImageToGEO(ips);
			//	for (int i = 0; i < 5; i++)
			//	{
			//		meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[i].Lat = gps.lat(i) * 1e7;
			//		meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[i].Lon = gps.lon(i) * 1e7;
			//		meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[i].HMSL = gps.alt(i);
			for(int i = 0; i < 5; i++){
				meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[i].Lat = 0;
				meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[i].Lon = 0;
				meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgCood[i].HMSL = 0;
			}
			//	}
			//}

			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SDMemory = close_payload_data.TxData_p.SDMemory;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SnapNum = close_payload_data.TxData_p.SnapNum;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.SDFlag = close_payload_data.TxData_p.SDFlag;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.RecordFlag = close_payload_data.TxData_p.RecordFlag;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackFlag = close_payload_data.TxData_p.TrackFlag;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AI_R = close_payload_data.TxData_p.AI_R;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.CarTrack = close_payload_data.TxData_p.CarTrack;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackClass = close_payload_data.TxData_p.TrackClass;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.TrackStatus = close_payload_data.TxData_p.TrackStatus;

			// AI相关数据, 其它先设为空，做了AI之后再重新嵌入
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiStatus = close_payload_data.TxData_p.AiStatus;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcSize = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiTargetInfo.referNameID = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiTargetInfo.DetLefttopX = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiTargetInfo.DetLefttopY = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiTargetInfo.DetWidth = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiTargetInfo.DetHeight = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiTargetInfo.TgtConfidence = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AiTargetInfo.TrackID = 0;
			//meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.AiInfos_p.AIDataDetcInfoArray

			// AR与AI同理
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArStatus = 0;// 待做了AR后手动赋值
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArTroubleCode = 0;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ArElementsNum = 0;
			//meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ArInfos_p.ElementsArray

			// 画中画、去雾、稳像
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.PIP = close_payload_data.TxData_p.PIP;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgStabilize = close_payload_data.TxData_p.ImgStabilize;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.ImgDefog = close_payload_data.TxData_p.ImgDefog;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.Pintu = close_payload_data.TxData_p.Pintu;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.DzoomWho = close_payload_data.TxData_p.DzoomWho;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.OsdFlag = close_payload_data.TxData_p.OSDFlag;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.HDvsSD = close_payload_data.TxData_p.HDvsSD;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.VisualFOVHMul = close_payload_data.TxData_p.VisualFOVHMul;
			meta_data->GimbalPayloadInfos_p.ImageProcessingBoardInfo_p.InfaredFOVHMul = close_payload_data.TxData_p.InfaredFOVHMul;

			// 激光测距相关信息
			meta_data->LaserDataInfos_p.LaserStatus = close_payload_data.LaerDataProcMsg_p.LaserStatus;
			meta_data->LaserDataInfos_p.LaserMeasVal = close_payload_data.LaerDataProcMsg_p.LaserMeasVal;
			meta_data->LaserDataInfos_p.LaserMeasStatus = close_payload_data.LaerDataProcMsg_p.LaserMeasStatus;
			meta_data->LaserDataInfos_p.Laserlat = close_payload_data.LaerDataProcMsg_p.Laserlat;
			meta_data->LaserDataInfos_p.Laserlon = close_payload_data.LaerDataProcMsg_p.Laserlon;
			meta_data->LaserDataInfos_p.LaserHMSL = close_payload_data.LaerDataProcMsg_p.LaserHMSL;
			meta_data->LaserDataInfos_p.LaserRefStatus = close_payload_data.LaerDataProcMsg_p.LaserRefStatus;
			meta_data->LaserDataInfos_p.LaserRefLat = close_payload_data.LaerDataProcMsg_p.LaserRefLat;
			meta_data->LaserDataInfos_p.LaserRefLon = close_payload_data.LaerDataProcMsg_p.LaserRefLon;
			meta_data->LaserDataInfos_p.LaserRefHMSL = close_payload_data.LaerDataProcMsg_p.LaserRefHMSL;
			meta_data->LaserDataInfos_p.LaserCurStatus = close_payload_data.LaerDataProcMsg_p.LaserCurStatus;
			meta_data->LaserDataInfos_p.LaserCurLat = close_payload_data.LaerDataProcMsg_p.LaserCurLat;
			meta_data->LaserDataInfos_p.LaserCurLon = close_payload_data.LaerDataProcMsg_p.LaserCurLon;
			meta_data->LaserDataInfos_p.LaserCurHMSL = close_payload_data.LaerDataProcMsg_p.LaserCurHMSL;
			meta_data->LaserDataInfos_p.LaserDist = close_payload_data.LaerDataProcMsg_p.LaserDist;
			meta_data->LaserDataInfos_p.LaserAngle = close_payload_data.LaerDataProcMsg_p.LaserAngle;
			meta_data->LaserDataInfos_p.LaserModel = close_payload_data.LaerDataProcMsg_p.LaserModel;
			meta_data->LaserDataInfos_p.LaserFreq = close_payload_data.LaerDataProcMsg_p.LaserFreq;

			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_major = _joedge_version.DataBackMajor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_minor = _joedge_version.DataBackMinor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_patch = _joedge_version.DataBackPatch;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.data_back_build = _joedge_version.DataBackBuild;

			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.payload_major = _joedge_version.PayloadMajor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.payload_minor = _joedge_version.PayloadMinor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.payload_patch = _joedge_version.PayloadPatch;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.payload_build = _joedge_version.PayloadBuild;

			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_major = _joedge_version.PilotMajor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_minor = _joedge_version.PilotMinor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_patch = _joedge_version.PilotPatch;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.pilot_build = _joedge_version.PilotBuild;

			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.sma_major = _joedge_version.SmaMajor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.sma_minor = _joedge_version.SmaMinor;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.sma_patch =  _joedge_version.SmaPatch;
			meta_data->GimbalPayloadInfos_p.GimbalPosInfo_p.sma_build = _joedge_version.SmaBuild;
		}

		void DispatchTaskImpl::createMuxer()
		{
			if (_record_file_prefix.empty()) {
				eap_error("_record_file_prefix is null, record fail");
			}

			std::string record_path{};
			try {
				GET_CONFIG(std::string, getString, my_record_path, Media::kRecordPath);
				record_path = my_record_path;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kRecordPath throw exception: %s", e.what());
			}

			try {
				createPath(record_path);
				_record_time_str = get_current_time_string_second_compact();
				
				_mux_file_name = record_path + "/";
				_mux_file_name += _record_file_prefix + _record_time_str + ".ts";
				eap_information_printf("Directory created successfully. _mux_file_name: %s", _mux_file_name);
				_muxer = Muxer::createInstance();
				_muxer->open(_mux_file_name, _timebase, _codec_parameter);
				if ((_func_mask & FUNCTION_MASK_VIDEO_RECORD) != FUNCTION_MASK_VIDEO_RECORD) {
					_func_mask += FUNCTION_MASK_VIDEO_RECORD;
					updateTaskFuncmask();
				}
			} catch (const std::exception& exp) {
				std::string err_msg = std::string(exp.what());
				NoticeCenter::Instance()->getCenter().postNotification(
					new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::MuxerOpenFailed, err_msg));
			}
			_record_time_str = "";
			if (_is_update_func) {
				// TODO: 目前默认muxer创建成功，即每次打开录像一定会成功
			}
		}

		void DispatchTaskImpl::createClipMuxer()
		{
			if (_record_file_prefix.empty()) {
				eap_error("_record_file_prefix is null, record fail");
			}

			std::string record_path{};
			try {
				if (!eap::configInstance().has(Media::kClipRecordPath)) {
					eap::configInstance().setString(Media::kClipRecordPath, "/mnt/sdcard/FLIGHT_INBOX");
				}
				GET_CONFIG(std::string, getString, my_record_path, Media::kClipRecordPath);
				record_path = my_record_path;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config kRecordPath throw exception: %s", e.what());
			}

			try {
				createPath(record_path);
				_record_time_str = get_current_time_string_second_compact();
				
				_mux_file_name = record_path + "/";
				_mux_file_name += _record_file_prefix + _record_time_str + ".ts";
				eap_information_printf("Directory created successfully. _mux_file_name: %s", _mux_file_name);
				_clip_muxer = Muxer::createInstance();
				_clip_muxer->open(_mux_file_name, _timebase, _codec_parameter);
				if ((_func_mask & FUNCTION_MASK_CLIP_SNAP_SHOT) != FUNCTION_MASK_CLIP_SNAP_SHOT) {
					_func_mask += FUNCTION_MASK_CLIP_SNAP_SHOT;
					updateTaskFuncmask();
				}
			} catch (const std::exception& exp) {
				std::string err_msg = std::string(exp.what());
				NoticeCenter::Instance()->getCenter().postNotification(
					new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::MuxerOpenFailed, err_msg));
			}
			_record_time_str = "";
			if (_is_update_func) {
				// TODO: 目前默认muxer创建成功，即每次打开录像一定会成功
			}
		}

		void DispatchTaskImpl::destroyMuxer()
		{
			std::lock_guard<std::mutex> lock(_muxer_mutex);
			if (_muxer) {
				_muxer->close();
				_muxer.reset();
				if ((_func_mask & FUNCTION_MASK_VIDEO_RECORD) == FUNCTION_MASK_VIDEO_RECORD) {
					_func_mask -= FUNCTION_MASK_VIDEO_RECORD;
					updateTaskFuncmask();
				}
				eap_information("destroyMuxer successed!");
			}
		}

		void DispatchTaskImpl::destroyClipMuxer()
		{
			std::lock_guard<std::mutex> lock(_clip_muxer_mutex);
			if (_clip_muxer) {
				_clip_muxer->close();
				_clip_muxer.reset();
				if ((_func_mask & FUNCTION_MASK_CLIP_SNAP_SHOT) == FUNCTION_MASK_CLIP_SNAP_SHOT) {
					_func_mask -= FUNCTION_MASK_CLIP_SNAP_SHOT;
					updateTaskFuncmask();
				}
				eap_information("destroyMuxer successed!");
			}
		}

		void DispatchTaskImpl::clipMuxerRecordTimer()
		{
			auto start_time = std::chrono::system_clock::now();
			ThreadPool::defaultPool().start([this, start_time]() {
				int elapsed_time = 0;
				while (elapsed_time < _clip_video_record_time_count.load()) { 
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
					elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now() - start_time).count();
				}
				_is_clip_video_record_on.store(false);
			});
		}

		void DispatchTaskImpl::createDecoder(Decoder::FrameCallback frame_callback)
		{			
#ifdef ENABLE_GPU
			_is_hardware_decode = true;
#endif
			try {
				if (!_decoder) {
					_decoder = Decoder::createInstance();
					_decoder->setFrameCallback(frame_callback);
					_decoder->open(_codec_parameter, _framerate, _init_parameter.pull_url, _is_hardware_decode);
				}
			} catch (const std::exception& exp) {
				std::string err_msg = std::string(exp.what());
				NoticeCenter::Instance()->getCenter().postNotification(
					new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::DecoderOpenFailed, err_msg));
				throw std::system_error(-1, std::system_category(), err_msg);
			}
		}


#ifdef ENABLE_GPU
// #ifndef ENABLE_AIRBORNE
//非机载端，直接创建编码器
        void DispatchTaskImpl::createEncoder(EncoderNVENC::EncodedPacketCallback encoder_packet_callback)
        {
			EncoderNVENC::InitParameter init_parameter;
			init_parameter.bit_rate = 5000000;
			init_parameter.dst_width = _codec_parameter.width;
			init_parameter.dst_height = _codec_parameter.height;
			init_parameter.time_base = _timebase;
			init_parameter.framerate = _framerate;
			init_parameter.start_time = _start_time;
			
			try {
				if(!_encoder){
					_encoder = EncoderNVENC::createInstance(init_parameter, encoder_packet_callback);
					_encoder->start();
				}
			} catch (const std::exception& exp) {
				std::string err_msg = std::string(exp.what());
				NoticeCenter::Instance()->getCenter().postNotification(
					new VideoMsgNotice(_id, (int)VideoMsgNotice::VideoCodeType::EncoderOpenFailed, err_msg));
				throw std::system_error(-1, std::system_category(), err_msg);
			}
        }
		void DispatchTaskImpl::destroyEncoder()
		{
			if (_encoder) {
				_encoder->stop();
				_encoder.reset();
			}
		}
// #endif
#endif

        void DispatchTaskImpl::destroyDecoder()
		{
			if (_decoder) {
				_decoder->close();
				_decoder.reset();
			}
		}
        void DispatchTaskImpl::destroyDemuxer()
		{
			for (auto iter : _lan_demuxer_reactors) {
				if (iter) {
					iter->Stop();
					iter.reset();
				}
			}
			for (auto iter : _wlan_demuxer_reactors) {
				if (iter) {
					iter->Stop();
					iter.reset();
				}
			}
			eap_information(" udp demuxer_reactors destroy successed!-");

			if (_demuxer) {
				_demuxer->close(true);
				_demuxer.reset();
			}
		}
#ifdef ENABLE_GPU
#ifndef ENABLE_AIRBORNE
        void DispatchTaskImpl::createImageEnhancer()
        {
			if (_is_image_enhancer_on) {
				if (!_enhancer && (_is_image_enhancer_first_create || _is_update_func)) {
					_enhancer = Enhancer::CreateDefogEnhancer(
						_codec_parameter.width, _codec_parameter.height);
					if (!_enhancer) {
						if (!_is_update_func) {
							std::string desc = "Enhancer create failed";
							throw std::runtime_error(desc);
						}
						_update_func_err_desc += std::string("Enhancer; ");
						eap_error("Enhance func update failed");
						_update_func_result = false;
						_func_mask -= FUNCTION_MASK_DEFOG;
					}
				}
			}

        }
		 void DispatchTaskImpl::destroyImageEnhancer()
		{
			if (_enhancer) {
				_enhancer.reset();
			}
		}
#endif
#endif // ENABLE_GPU

        std::string DispatchTaskImpl::aiAssistTrack(int track_cmd, int track_pixelpos_x, int track_pixelpos_y)
		{
			AiassistTrackResults assist_track_obj;
			assist_track_obj.track_cmd = track_cmd;
			// assist_track_obj.track_id = track_id;
			assist_track_obj.track_pixelpos_x = track_pixelpos_x;
			assist_track_obj.track_pixelpos_y = track_pixelpos_y;
			assist_track_obj.track_pixelpos_h = 0;
			assist_track_obj.track_pixelpos_w = 0;
			std::string desc = "Return original coordinates";
		#ifdef ENABLE_AI
			if (!_is_ai_assist_track_on) {
				eap_error_printf("Please check if AI assist track is enabled, AI assist track switch status: %s",
					static_cast<int>(_is_ai_assist_track_on.load()));
				desc +=", AI assist track not enabled";
			}else{
				//AI检测，只处理当前一帧
				std::vector<joai::Result> detect_objects{};
				std::lock_guard<std::mutex> assist_lock(_decoded_images_assist_track_mutex);
				if (!_decoded_images_assist_track.empty()) {
					auto current_image = _decoded_images_assist_track.front();
					// if(track_cmd == 0){
					std::promise<void> ai_promise;
					auto ai_future = ai_promise.get_future();
					if (!current_image->bgr24_image.empty()) {
#ifndef ENABLE_AIRBORNE
						detect_objects = _engines->_ai_object_detector->detect(current_image->bgr24_image);
#else
						detect_objects = _engines->_ai_object_detector->detect(current_image->bgr24_image, true);
#endif
						_engines->latest_detect_result = detect_objects;
					} 
					ai_promise.set_value();
					ai_future.get();
				}else {
					detect_objects = _engines->latest_detect_result;
				}
		
				if(!detect_objects.empty()){
					// 遍历 detect_objects，找到最近的目标
					joai::Result* nearest_result = nullptr;
					double min_distance = std::numeric_limits<double>::max();
					const double threshold = 540.0; // 距离阈值，例如 540 像素

					for (auto& obj : detect_objects) {
						int center_x = obj.Bounding_box.x + obj.Bounding_box.width / 2;
						int center_y = obj.Bounding_box.y + obj.Bounding_box.height / 2;
						double distance = std::pow(center_x - track_pixelpos_x, 2) + std::pow(center_y - track_pixelpos_y, 2); // 比较平方距离
						if (distance < min_distance && distance <= threshold * threshold) {
							min_distance = distance;
							nearest_result = &obj;
						}
					}
					if (nearest_result != nullptr) {
						// 返回最近的目标中心坐标、宽高
						int target_x = nearest_result->Bounding_box.x + nearest_result->Bounding_box.width / 2;
						int target_y = nearest_result->Bounding_box.y + nearest_result->Bounding_box.height / 2;
						assist_track_obj.track_pixelpos_x = target_x;
						assist_track_obj.track_pixelpos_y = target_y;
						assist_track_obj.track_pixelpos_h = nearest_result->Bounding_box.height;
						assist_track_obj.track_pixelpos_w = nearest_result->Bounding_box.width;
						eap_information_printf("AI assist track success,target_x: %s", target_x);
						eap_information_printf("AI assist track success,target_y: %s", target_y);
						desc = "Return AI detection pixel coordinates";

						//// 绘制跟踪框在原图像上
						//auto current_image = _decoded_images_assist_track.front();
						//cv::rectangle(current_image->bgr24_image, cv::Rect(nearest_result->Bounding_box.x, nearest_result->Bounding_box.y, nearest_result->Bounding_box.width, nearest_result->Bounding_box.height), cv::Scalar(0, 255, 0), 2);

						//// 保存图像
						//std::string save_path = "H:/JoEAPSma/release/windows/Debug/Debug/results/tracked_" + std::to_string(std::rand()) + ".jpg";
						//cv::imwrite(save_path, current_image->bgr24_image);
					} 
				}
			}
		#endif
			// NoticeCenter::Instance()->getCenter().postNotification(new AiAssistTrackResultNotice(
			// 		std::string(_id), assist_track_obj, desc));
			
			// 如果没有新的 track_mask_plate ,就把原来接收到的 track_mask_plate 传回去
			// assist_track_obj.track_mask_plate = 2;

			Poco::JSON::Object jsonObject;
			jsonObject.set("track_cmd", assist_track_obj.track_cmd);
			jsonObject.set("track_pixelpos_x", assist_track_obj.track_pixelpos_x);
			jsonObject.set("track_pixelpos_y", assist_track_obj.track_pixelpos_y);
			jsonObject.set("track_pixelpos_w", assist_track_obj.track_pixelpos_w);
			jsonObject.set("track_pixelpos_h", assist_track_obj.track_pixelpos_h);
			// jsonObject.set("track_mask_plate", assist_track_obj.track_mask_plate);

			std::stringstream ss;
			Poco::JSON::Stringifier::stringify(jsonObject, ss);
			return ss.str();
		}

		void DispatchTaskImpl::saveSnapShot()
		{	
			int snapshot_numbers{};
			std::string snapshot_path{};
			try {
				GET_CONFIG(int, getInt, my_snapshot_numbers, Media::kSnapShotNumbers);
				if (!eap::configInstance().has(Media::kSnapShotPath)) {
					eap::configInstance().setString(Media::kSnapShotPath, std::string("/mnt/sdcard/FLIGHT_INBOX/"));
				}
				GET_CONFIG(std::string, getString, my_snapshot_path, Media::kSnapShotPath);
				snapshot_numbers = my_snapshot_numbers;
				snapshot_path = my_snapshot_path;
			}
			catch (const std::exception& e) {
				eap_error_printf("get config throw exception: %s", e.what());
			}

			createPath(snapshot_path);
			std::string desc = "";
			if (!_is_snap_shot_on) {
				eap_error_printf("Please check if save snapshot is enabled, _is_snap_shot_on switch status: %s",
					static_cast<int>(_is_snap_shot_on.load()));
				desc +=", save snapshot not enabled";
			}else{
				std::lock_guard<std::mutex> snapshot_lock(_decoded_images_snapshot_mutex);
				if (!_decoded_images_snapshot.empty()) {
					// auto image_count = std::min(snapshot_numbers, static_cast<int>(_decoded_images_snapshot.size()));
					for (int i = 0; i < snapshot_numbers; ++i) {
						auto image = _decoded_images_snapshot.front();
						_decoded_images_snapshot.pop();
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI)
						if (!image->bgr24_image.empty()) {
							// 获取当前时间
							auto now = std::chrono::system_clock::now();
							auto time_t_now = std::chrono::system_clock::to_time_t(now);
							auto tm_now = *std::localtime(&time_t_now);
							
							// 格式化时间作为文件名
							std::ostringstream oss;
							oss << std::put_time(&tm_now, "%Y%m%d%H%M%S");
							std::string file_name = oss.str() + ".png";
							std::string full_path = snapshot_path +  file_name;
							cv::Mat bgr240_image(image->height, image->width, CV_8UC3);
#ifdef ENABLE_AIRBORNE
							if (image->format == AV_PIX_FMT_RGB24 || image->format == AV_PIX_FMT_BGR24) {
								bgr240_image = image->bgr24_image;
							}
							else {
								//yuv转bgr
								I4202Bgr((char*)image->bgr24_image.data, (char*)bgr240_image.data, pIn420Dev, pOutBgrDev, image->bgr24_image.cols, image->bgr24_image.rows * 2 / 3, 0, 0, image->width, image->height);
							}
#else
							image->bgr24_image.download(bgr240_image);
#endif

							// 保存图片
							cv::imwrite(full_path, bgr240_image);
							_is_snap_shot_on = false;
							eap_information_printf("snap save successed, name: %s", full_path);
						}else {
							desc += ", no bgr24_images to save";
						}
#endif
					}
				} else {
					desc += ", no images to save";
				}
			}
			NoticeCenter::Instance()->getCenter().postNotification(new SaveSnapShotResultNotice(
					std::string(_id), desc));
		}

		void DispatchTaskImpl::updateArLevelDistance(int level_one_distance, int level_two_distance)
		{
#ifdef ENABLE_AR
			_ar_level_one_distance = level_one_distance;
			_ar_level_two_distance = level_two_distance;
			if (_engines->_ar_engine && _ar_level_one_distance > 0 && _ar_level_two_distance > 0) {
				_engines->_ar_engine->setWarningArea(_ar_level_one_distance, _ar_level_two_distance);
			}
#endif
		}

		// 更新tower_height
        void DispatchTaskImpl::updateArTowerHeight(bool is_tower, double tower_height, bool buffer_sync_height)
        {
#ifdef ENABLE_AR
			// 如果ar引擎不存在那么将这个tower_height放入配置文件中
			if (!_engines->_ar_engine) {
				std::string desc = "AR engine is not exist,  update tower_height failed";
				throw std::runtime_error(desc);
			}

			if (_engines->_ar_engine && tower_height >= 0) {
				// 异步写入tower_height到config file
				ThreadPool::defaultPool().start(
					[this, tower_height]() {
						eap::configInstance().setDouble("tower_height", tower_height);
						eap::saveConfig();
						eap_information("ar engine  exist, tower_height update to config file");
					});
				// 2直接传入
				_ar_tower_height = tower_height;
				_ar_is_tower = is_tower;
				_ar_buffer_sync_height = buffer_sync_height;
				_engines->_ar_engine->configTowerAR(is_tower, tower_height, buffer_sync_height);
			}
#endif
        }
		void DispatchTaskImpl::setSeekPercent(float percent)
		{
			if (!_demuxer) {//目前只有http和文件作为视频源时才支持跳转
				std::string desc = "_demuxer is null";
				throw std::runtime_error(desc);			
			}
			_demuxer->seek(percent);
		}

		void DispatchTaskImpl::pause(int paused)
		{
			if (!_demuxer) {//目前只有http和文件作为视频源时才支持暂停
				std::string desc = "_demuxer is null";
				throw std::runtime_error(desc);
			}
			if(!_is_pull_rtc)
				_demuxer->pause(paused);
			/*if(_pusher && _pusher->_pusher){
				_pusher->_pusher->pause(paused);
			}*/
		}

		std::vector<int64_t> DispatchTaskImpl::getVideoDuration()
		{
			std::vector<int64_t> durations;
			if (_demuxer) {
				durations.push_back(_demuxer->videoDuration());
			}
			return durations;
		}

		void DispatchTaskImpl::updateAiCorPos(int ai_pos_cor)
		{
#ifdef ENABLE_AR
			if (ai_pos_cor == 0) {
				_ai_pos_cor = jo::AIPosCor::BOTTOM;
			} else if (ai_pos_cor == 1) {
				_ai_pos_cor = jo::AIPosCor::CENTER;
			} else {
				_ai_pos_cor = jo::AIPosCor::TOP;
			}
#endif
			eap_information("update aicorpos success");
		}

		void DispatchTaskImpl::fireSearchInfo(const std::string id, const double target_lat, const double target_lon, const double target_alt)
		{
			_pilot_id = id;
			_target_lat = target_lat;
			_target_lon = target_lon;
			_target_alt = target_alt;
			_snapshot = true;
			eap_information_printf("start fire snapshot, task id: %s, pilot id: %s, target_lat:%f, target_lon: %f, target_alt: %f", _id, _pilot_id, _target_lat, _target_lon, _target_alt);
		}
		void DispatchTaskImpl::setAirborne45G(const int airborne_45G)
		{
			if(airborne_45G == _airborne_45G){
  				eap_information("airborne_45G is same");
  				return;
			}
			_airborne_45G = airborne_45G;
			eap::configInstance().setInt(Media::kAirborne45G, _airborne_45G);
			eap::saveConfig();
			if (_airborne_45G) {
				//机载走4、5G链路，不走图传，不推udp组播视频流，推流媒体视频流
				eap_information_printf("enable_airborne_45G, close udp pusher, task id: %s, push url: %s", _id, _push_url);
				if (_pusher && _pusher->_pusher)
					_pusher->_pusher->close();
				
				if (!_pusher->_air_url.empty()) {
					eap::ThreadPool::defaultPool().start([this]() {
						openAirPusher();
					});
				}
			}
			else {
				//机载不走4、5G链路，走图传，推udp组播视频流，停止推流媒体视频流
				if (_pusher && _pusher->_air_pusher) {
					eap_information_printf("disable_airborne_45G, close pusher, task id: %s, push url: %s", _id, _pusher->_air_url);
					_pusher->_air_pusher->close();
				}
				eap::ThreadPool::defaultPool().start([this]() {
					openPusher();
				});
			}
		}
	}
}
