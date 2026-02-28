#include "EapsPlaybackAnnotation.h"
#include "EapsFFmpegWrap.h"
#include "EapsConfig.h"
#include "Logger.h"
#include "OnceToken.h"
#include "EapsNoticeCenter.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/ThreadPool.h"
#include "Poco/StreamCopier.h"
#include <chrono>
#include <future>
#include <vector>
#include <algorithm>

namespace eap {
	namespace sma {
		PlaybackAnnotation::PlaybackAnnotation(InitParamPtr init_param)
		{		
			_playbackAddress = init_param->playback_address;
			_videoOutUrl = init_param->video_out_url;
			_metadataDirectory = init_param->metadata_file_directory;
			_taskId = init_param->task_id;

			if (_playbackAddress.empty() || _videoOutUrl.empty() || _metadataDirectory.empty()) {
				std::string desc = "playback annotation param is null";
				throw std::runtime_error(desc);
			}

			_videoMarkMetadataDirectory = exeDir() + _metadataDirectory + _taskId + "/";
			_meta_data_processing = MetaDataProcessing::createInstance();
		}

		PlaybackAnnotation::~PlaybackAnnotation()
		{
			stop();
		}

		void PlaybackAnnotation::start()
		{		
			auto demuxer_packet_callback = [this](Packet packet) {

				if (_meta_data_processing) {
					std::promise<void> meta_data_pre_process_promise;
					auto meta_data_pre_process_future = meta_data_pre_process_promise.get_future();

					std::vector<uint8_t> raw_data{};
					ThreadPool::defaultPool().start([this, &raw_data, &packet, &meta_data_pre_process_promise]()
					{					
						auto meta_data = _meta_data_processing->metaDataParseBasic(
							packet->data, packet->size, _codec_parameter.codec_id, raw_data);
						std::shared_ptr<JoFmvMetaDataBasic> meta_data_basic{};
						if (meta_data.first) {
							packet.metaDataValid() = true;
							packet.setMetaDataBasic(meta_data.first);
						}
						if (!raw_data.empty()) {
							packet.setSeiBuf(raw_data);
							_meta_data_processing->setRawSeiData(raw_data.data(), raw_data.size());
						}

						meta_data_pre_process_promise.set_value();
					});

					std::string original_pts = std::to_string(packet.getOriginalPts());				
					std::string video_mark_meta_data_url = _videoMarkMetadataDirectory + original_pts + ".json";
					std::vector<std::string> mark_meta_data_url_v{};
					std::list<std::string> meta_data_paths{};
                    listFilesRecursively(_videoMarkMetadataDirectory, meta_data_paths, "", ".json");
					for(auto path : meta_data_paths) {
						if (path == video_mark_meta_data_url) {
							continue;
						}

						if (!mark_meta_data_url_v.empty()) {
							if (std::find(mark_meta_data_url_v.begin(), mark_meta_data_url_v.end(), path)
								!= mark_meta_data_url_v.end()) {
								video_mark_meta_data_url = "";
								continue;
							}
						}
						mark_meta_data_url_v.push_back(path);
					};

					ArInfos ar_infos{};
					if (!video_mark_meta_data_url.empty()) {
						std::string mark_meta_data_json_str =readFileContents(video_mark_meta_data_url);
						Poco::JSON::Object json_all;
						Poco::JSON::Parser parser;
						try {
							Poco::Dynamic::Var result = parser.parse(mark_meta_data_json_str);
							json_all = *result.extract<Poco::JSON::Object::Ptr>();
						}
						catch (const std::exception& e) {
							eap_error(e.what());
						}
						auto ar_elements_array = *(json_all.has("ElementsArray") ? json_all.getArray("ElementsArray") : Poco::JSON::Array::Ptr());
						ar_infos.ArStatus = json_all.has("AR_Status") ? json_all.getValue<int>("AR_Status") : 0;
						ar_infos.ArTroubleCode = json_all.has("ArTroubleCode") ? json_all.getValue<int>("ArTroubleCode") : 1;
						ar_infos.ArElementsNum = json_all.has("ElementsNum") ? json_all.getValue<int>("ElementsNum") : 0;

						if (ar_elements_array.size() > 0 && ar_infos.ArStatus != 0 
							&& ar_infos.ArTroubleCode == 0 && ar_infos.ArElementsNum > 0) {
							for (int i = 0; i < ar_infos.ArElementsNum; i++) {
								auto ar_element = *ar_elements_array.getObject(i);
								ar_infos.ElementsArray[i].X = ar_element.getValue<int>("X");
								ar_infos.ElementsArray[i].Y = ar_element.getValue<int>("Y");
								ar_infos.ElementsArray[i].Type = ar_element.getValue<int>("Type");
								ar_infos.ElementsArray[i].DotQuantity = ar_element.getValue<int>("DotQuantity");
								ar_infos.ElementsArray[i].lon = ar_element.getValue<int>("lon");
								ar_infos.ElementsArray[i].lat = ar_element.getValue<int>("lat");
								ar_infos.ElementsArray[i].HMSL = ar_element.getValue<int>("HMSL");
								ar_infos.ElementsArray[i].Category = ar_element.getValue<int>("Category");
								ar_infos.ElementsArray[i].CurIndex = ar_element.getValue<int>("CurIndex");
								ar_infos.ElementsArray[i].NextIndex = ar_element.getValue<int>("NextIndex");
							}
						}
						else {
							eap_error( "play back annotation ar_infos has error value.");
						}			
					}
					else {
						eap_error( "play back annotation  video_mark_meta_data_url is empty.");
					}
					
					meta_data_pre_process_future.get();
					_meta_data_processing->updateArData(&ar_infos);
					auto new_serialized_bytes = _meta_data_processing->getSerializedBytes();
					if (!new_serialized_bytes.empty()) {
						packet.metaDataValid() = true;
						packet.setMetaDataBasic(_meta_data_processing->getMetaDataBasic());

						auto sei_buffer = MetaDataProcessing::seiDataAssemblyH264(
							new_serialized_bytes.data(), new_serialized_bytes.size());
						auto sei_buffer_old = MetaDataProcessing::seiDataAssemblyH264(
							raw_data.data(), raw_data.size());

						if (!sei_buffer.empty()) {
							AVPacket* pkt_new = av_packet_alloc();
							int new_packet_size = packet->size - sei_buffer_old.size() + sei_buffer.size();
							if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
								int pos = 0;
								memcpy(pkt_new->data, sei_buffer.data(), sei_buffer.size());
								pos += sei_buffer.size();
								memcpy(pkt_new->data + pos, packet->data + sei_buffer_old.size(), packet->size - sei_buffer_old.size());
								pkt_new->pts = packet->pts;
								pkt_new->dts = packet->dts;
								pkt_new->duration = packet->duration;
								pkt_new->flags = packet->flags;

								Packet packet_export(pkt_new);
								if (_pusherTradition) {
									_pusherTradition->pushPacket(packet_export);
								}							
								return;
							}
							else {}
						}
						else {}
					}
					else {}
				}
				else {}
				
				if (_pusherTradition) {
					_pusherTradition->pushPacket(packet);
				}			
			};

			auto demuxer_stop_callback = [this](int exit_code) {
				ThreadPool::defaultPool().start([this, exit_code]()
				{				
					std::string desc = "playback mark video record demuxer stoped, exit code: " + std::to_string(exit_code);
					eap_error( desc);
				});
			};

			_demuxer_tradition = DemuxerTradition::createInstance();
			_demuxer_tradition->setPacketCallback(demuxer_packet_callback);
			_demuxer_tradition->setStopCallback(demuxer_stop_callback);
			_demuxer_tradition->open(_playbackAddress, std::chrono::milliseconds(3000));
			_codec_parameter = _demuxer_tradition->videoCodecParameters();

			auto pusher_stop_callback = [this](int ret, std::string err_str) {
				ThreadPool::defaultPool().start([this, ret, err_str]()
				{
					std::string desc = "playback mark video record pusher stoped, exit code: " + std::to_string(ret) + ", description: " + err_str;
					eap_error( desc);
					NoticeCenter::Instance()->getCenter().postNotification(
							new TaskStopedNotice(std::string(_taskId), desc, ""));
				});
			};

			_pusherTradition = PusherTradition::createInstance();
			_pusherTradition->setStopCallback(pusher_stop_callback);
			_pusherTradition->open("", _videoOutUrl, _demuxer_tradition->videoStreamTimebase(), 
				_demuxer_tradition->videoFrameRate(), _codec_parameter, std::chrono::milliseconds(3000));
		}

		void PlaybackAnnotation::stop()
		{
			if (_demuxer_tradition) {
				_demuxer_tradition->close();
				_demuxer_tradition.reset();
			}

			if (_pusherTradition) {
				_pusherTradition->close();
				_pusherTradition.reset();
			}

			if (_meta_data_processing) {
				_meta_data_processing.reset();
			}		
		}

		PlaybackAnnotation::InitParamPtr PlaybackAnnotation::makeInitParam()
		{
			return InitParamPtr(new InitParam());
		}

		PlaybackAnnotationPtr PlaybackAnnotation::createInstance(InitParamPtr init_param)
		{
			return PlaybackAnnotationPtr(new PlaybackAnnotation(init_param));
		}

	}
}


