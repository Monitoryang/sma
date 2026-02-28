#if USE_OLD_SEI_REPLACE
#include "EapsMetaDataProcessingOld.h"
#include "EapsMetaDataBasic.h"
#include "EapsCompressor.h"
#include "EapsCrc32.h"

#include <stdexcept>
#include <system_error>

namespace eap {
	namespace sma {
#define CRC_MAGIC_NUMBER_EDGE 15

		MetaDataProcessingPtr MetaDataProcessing::createInstance()
		{
			return MetaDataProcessingPtr(new MetaDataProcessing());
		}

		MetaDataProcessing::MetaDataProcessing()
		{
			_meta_data_assembly_json = MetaDataAssemblyJson::createInstance();
		}

		JoFmvMetaDataBasic& MetaDataProcessing::getMetaDataStructure()
		{
			return _meta_data_structure;
		}

		const std::string& MetaDataProcessing::assemblyToJson()
		{
			_meta_data_assembly_json->updateMetaDataStructure(_meta_data_structure);

			_meta_data_json = _meta_data_assembly_json->getAssemblyString();

			return _meta_data_json;
		}

		const std::vector<std::uint8_t>& MetaDataProcessing::assemblyToH264Sei()
		{
			auto raw_data = std::vector<uint8_t>(_meta_data_json.length());
			memcpy(raw_data.data(), _meta_data_json.data(), _meta_data_json.length());

			auto compressed_data = zlibCompress(raw_data);

			if (compressed_data.empty()) {
				throw std::invalid_argument("compress data failed");
			}

			auto crc = GetCRC32(CRC_MAGIC_NUMBER_EDGE, compressed_data.data(), (int)compressed_data.size());

			std::vector<uint8_t> message(compressed_data.size() + 8);

			// 消息组装（字段：消息长度+消息负载+CRC）
			std::uint32_t message_size = std::uint32_t(compressed_data.size());

			size_t offset = 0;
			memcpy(message.data() + offset, &message_size, sizeof(message_size)); // 消息长度字段
			offset += sizeof(message_size);
			memcpy(message.data() + offset,
				compressed_data.data(), compressed_data.size()); // 实际消息字段

			offset += compressed_data.size();
			memcpy(message.data() + offset, &crc, sizeof(crc)); // CRC字段

			int out_sei_data_size{ 0 };
			uint8_t* out_sei_data{};

			bool return_value = VideoSeiDataAssemblyH264((uint8_t*)message.data(),
				(int)message.size(), &out_sei_data, &out_sei_data_size);
			if (return_value && out_sei_data && out_sei_data_size > 0) {
				_meta_data_sei = std::vector<uint8_t>(out_sei_data_size);
				memcpy(_meta_data_sei.data(), out_sei_data, out_sei_data_size);

				delete[] out_sei_data;
			}

			return _meta_data_sei;
		}

		Packet MetaDataProcessing::assemblyToPacket(Packet packet)
		{
			// TODO: 如果里边本身有SEI，就应先清除掉

			if (!_meta_data_sei.empty()) {
				AVPacket* pkt_new = av_packet_alloc();

				int new_packet_size = packet->size + (int)_meta_data_sei.size();

				if (pkt_new && av_new_packet(pkt_new, new_packet_size) == 0) {
					size_t pos = 0;
					memcpy(pkt_new->data, _meta_data_sei.data(), _meta_data_sei.size());
					pos += _meta_data_sei.size();
					memcpy(pkt_new->data + pos, packet->data, packet->size);
					pkt_new->pts = packet->pts;
					pkt_new->dts = packet->dts;
					pkt_new->duration = packet->duration;
					pkt_new->flags = packet->flags;

					Packet packet_export(pkt_new);
					//packet_export.setSeiBuf(meta_data_raw);
					packet_export.setMetaDataBasic(packet.getMetaDataBasic());
					packet_export.metaDataValid() = packet.metaDataValid();
					packet_export.setArPixelPoints(packet.getArPixelPoints());
					packet_export.setArPixelLines(packet.getArPixelLines());
					packet_export.setArPixelLines(packet.getArPixelLines());
					packet_export.setCurrentTime(packet.getCurrentTime());

					return packet_export;
				}
				else {
					return packet;
				}
			}
			else {
				return packet;
			}
		}
	}
}
#endif // USE_OLD_SEI_REPLACE