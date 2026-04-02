#include "EapsMetaDataBasic.h"
#include "EapsCrc32.h"

#include <cstring>



namespace eap {
	namespace sma {
#if USE_OLD_SEI_REPLACE
		static const int UUID_SIZE = 16;

		namespace AssemblyH264 {
			//FFMPEG uuid
			//static unsigned char uuid[] = { 0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9, 0x48, 0xb7, 0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xef };
			//self UUID
			static unsigned char uuid[] = { 0x54, 0x80, 0x83, 0x97, 0xf0, 0x23, 0x47, 0x4b, 0xb7, 0xf7, 0x4f, 0x32, 0xb5, 0x4e, 0x06, 0xac };
			// 4E 01 05 FF FF FF 60 2C A2 DE 09 B5 17 47 DB BB 55 A4 FE 7F C2 FC 4E
			//开始码
			static unsigned char start_code[] = { 0x00,0x00,0x00,0x01 };

			static uint32_t reversebytes(uint32_t value)
			{
				return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
					(value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
			}

			static uint32_t GetSEINaluSize(uint32_t content)
			{
				//SEI payload size
				uint32_t sei_payload_size = content + UUID_SIZE;
				//NALU + payload类型 + 数据长度 + 数据
				uint32_t sei_size = 1 + 1 + (sei_payload_size / 0xFF + (sei_payload_size % 0xFF != 0 ? 1 : 0)) + sei_payload_size;
				//截止码
				uint32_t tail_size = 2;
				if (sei_size % 2 == 1) {
					tail_size -= 1;
				}
				sei_size += tail_size;

				return sei_size;
			}

			static uint32_t GetSEIPacketSize(uint32_t size)
			{
				return GetSEINaluSize(size) + 4;
			}
		}

		namespace ParseSEI {
			static int getSeiBuffer(unsigned char * data, uint32_t size,
				unsigned char * buffer, int *count, int *nType)
			{
				unsigned char * sei = data;
				int sei_type = 0;
				unsigned sei_size = 0;
				// payload type
				do {
					sei_type += *sei;
					*nType = sei_type;
				} while (*sei++ == 255);
				// data size
				do {
					sei_size += *sei;
				} while (*sei++ == 255);

				// check UUID
				static unsigned char uuid[] =
				{ 0x73, 0x74, 0x64, 0x74, 0x73, 0x74, 0x64, 0x74, 0x73, 0x74, 0x64, 0x74, 0x73, 0x74, 0x64, 0x74 };

				if (sei_size >= UUID_SIZE && sei_size <= (uint32_t)(data + size - sei) &&
					sei_type == 5 /*&& memcmp(sei, uuid, UUID_SIZE) == 0*/) {
					sei += UUID_SIZE;
					sei_size -= UUID_SIZE;

					if (buffer != NULL && count != NULL) {
						if (*count > (int)sei_size) {
							memcpy(buffer, sei, sei_size);
						}
					}

					*count = sei_size;

					return sei_size;
				}
				return -1;
			}
		}

		bool VideoSeiDataAssemblyH264(uint8_t* content, int content_size, uint8_t** out_data, int* out_size, bool isAnnexb)
		{
			if (!content || content_size <= 0) {
				return false;
			}
			if (!out_data || !out_size) {
				return false;
			}
			unsigned int nalu_size = (unsigned int)AssemblyH264::GetSEINaluSize(content_size);
			int actual_size = AssemblyH264::GetSEIPacketSize(content_size);

			uint8_t* actual_data = new uint8_t[actual_size];
			uint8_t* data = actual_data;

			uint32_t sei_size = nalu_size;
			//大端转小端
			nalu_size = AssemblyH264::reversebytes(nalu_size);

			//NALU开始码
			unsigned int * size_ptr = &nalu_size;
			if (isAnnexb) {
				memcpy(data, AssemblyH264::start_code, sizeof(unsigned int));
			}
			else {
				memcpy(data, size_ptr, sizeof(unsigned int));
			}
			data += sizeof(unsigned int);

			unsigned char * sei = data;
			//NAL header
			*data++ = 6; //SEI
						 //sei payload type
			*data++ = 5; //unregister
			size_t sei_payload_size = content_size + UUID_SIZE;
			//数据长度
			while (true) {
				*data++ = (sei_payload_size >= 0xFF ? 0xFF : (char)sei_payload_size);
				if (sei_payload_size < 0xFF) break;
				sei_payload_size -= 0xFF;
			}

			//UUID
			memcpy(data, AssemblyH264::uuid, UUID_SIZE);
			data += UUID_SIZE;
			//数据
			memcpy(data, content, content_size);
			data += content_size;

			//tail 截止对齐码
			if (sei + sei_size - data == 1) {
				*data = 0x80;
			}
			else if (sei + sei_size - data == 2) {
				*data++ = 0x00;
				*data++ = 0x80;
			}

			*out_data = actual_data;
			*out_size = actual_size;

			return true;
		}

		bool VideoSeiDataParse(uint8_t* in_data, int in_size, AVCodecID codec_id, uint8_t** out_data, int* out_size)
		{
			static uint8_t h3_0[3] = { 0x00, 0x00, 0x00 };
			static uint8_t h3_1[3] = { 0x00, 0x00, 0x01 };

			uint8_t* data = in_data;
			int32_t  size = in_size;

			//ByteArray_t out_sei_buf;

			if (!data || size <= 0) {
				return false;
			}
			if (!out_data || !out_size) {
				return false;
			}

			bool finded{};

			int index = 0;
			for (auto i = 0; i < size - 4; i++) {
				finded = false;
				if (memcmp(&data[i], h3_0, 3) == 0) {
					i += 4;
					finded = true;
				}
				else if (memcmp(&data[i], h3_1, 3) == 0) {
					i += 3;
					finded = 1;
				}
				if (finded) {
					if (codec_id == AV_CODEC_ID_H264) {
						if ((data[i] & 0x1F) == 0x06) {
							index = i;
							break;
						}
						else if ((data[++i] & 0x1F) == 0x06) {
							index = i;
							break;
						}
						else {
							finded = false;
						}
					}
					else {
						uint16_t t = (*((uint16_t*)&(data[i])));
						if (((t & 0x7E) >> 1) == 39 || ((t & 0x7E) >> 1) == 40) {
							index = i;
							break;
						}
						else {
							finded = false;
						}
					}
				}
			}

			if (!finded) {
				return false;
			}

			unsigned char actul_sei[4096] = { 0 };
			int count = size;
			unsigned char* sei = NULL;
			if (codec_id == AV_CODEC_ID_H264) {
				sei = (&data[index] + 1);
			}
			else if (codec_id == AV_CODEC_ID_H265) {
				sei = (&data[index] + 2);
			}
			int type = 0;
			int nsize = ParseSEI::getSeiBuffer(sei, (data + size - sei), actul_sei, &count, &type);
			if (type == 5 && nsize > 0) {
				*out_data = new uint8_t[nsize];
				memcpy(*out_data, actul_sei, nsize);
				*out_size = nsize;
				return true;
			}
			else {
				return false;
			}
		}

		bool VideoSeiDataChecking(uint8_t* sei_data, int sei_size, uint8_t** real_data, int* real_size, int* v)
		{
#define CRC_MAGIC_NUMBER 11
#define CRC_MAGIC_NUMBER_NEW 13

			if (!sei_data || !sei_size || !real_data || !sei_size || !v) {
				return false;
			}

			uint8_t* data{};
			size_t size{};

			if (sei_data) {
				data = sei_data;
				size = sei_size;
			}

			uint32_t message_size = 0;
			uint32_t data_crc = 0;
			uint32_t current_crc = 0;

			if (data && size > 8) {
				message_size = (*(uint32_t*)data);
				if (message_size + 8 == size) {
					data_crc = (*(uint32_t*)(data + 4 + message_size));
					auto real_message = new uint8_t[message_size];
					memcpy(real_message, data + 4, message_size);

					current_crc = GetCRC32(CRC_MAGIC_NUMBER, real_message, message_size);

					if (data_crc == current_crc) {
						*v = 0;

						//uint8_t* raw_data{};
						//int raw_size{};
						//if (ZLibDecompress(real_message, message_size, &raw_data, &raw_size)) {
						//	if (raw_data) {
						//		*real_data = raw_data;
						//		*real_size = raw_size;
						//		return true;
						//	} else {
						//		return false;
						//	}
						//} else {
						//	//printf("[meta data checking]: Decompress failed.");
						//}
					}
					else {
						current_crc = GetCRC32(CRC_MAGIC_NUMBER_NEW, real_message, message_size);
						if (data_crc == current_crc) {
							*v = 0;
							*real_data = real_message;
							*real_size = message_size;
							return true;
						}
						else {
							//printf("[meta data checking]: CRC Check failed.\n");
						}
					}
				}
			}

			return false;
		}

#else	
		static unsigned char start_code[] = { 0x00,0x00,0x00,0x01 };
		static unsigned char uuid[] = { 0x54, 0x80, 0x83, 0x97, 0xf0, 0x23, 0x47, 0x4b, 0xb7, 0xf7, 0x4f, 0x32, 0xb5, 0x4e, 0x06, 0xac };
		#define UUID_SIZE 16
		uint32_t GetSEINaluSize(uint32_t content)
		{
			//SEI payload size
			uint32_t sei_payload_size = content + UUID_SIZE;
			//NALU + payload���� + ���ݳ��� + ����
			uint32_t sei_size = 1 + 1 + (sei_payload_size / 0xFF + (sei_payload_size % 0xFF != 0 ? 1 : 0)) + sei_payload_size;
			//��ֹ��
			uint32_t tail_size = 2;
			if (sei_size % 2 == 1)
			{
				tail_size -= 1;
			}
			sei_size += tail_size;

			return sei_size;
		}

		uint32_t GetSEIPacketSize(uint32_t size)
		{
			return GetSEINaluSize(size) + 4;
		}

		int getSeiBuffer(unsigned char* data, uint32_t size,
			unsigned char* buffer, int* count, int* nType)
		{
			unsigned char* sei = data;
			int sei_type = 0;
			unsigned sei_size = 0;
			// payload type  
			do {
				sei_type += *sei;
				*nType = sei_type;
			} while (*sei++ == 255);
			// data size
			do {
				sei_size += *sei;
			} while (*sei++ == 255);

			// check UUID  
			static unsigned char uuid[] =
			{ 0x73, 0x74, 0x64, 0x74, 0x73, 0x74, 0x64, 0x74, 0x73, 0x74, 0x64, 0x74, 0x73, 0x74, 0x64, 0x74 };
			//static unsigned int  UUID_SIZE = 16;

			if (sei_size >= UUID_SIZE && sei_size <= (uint32_t)(data + size - sei) &&
				sei_type == 5 /*&& memcmp(sei, uuid, UUID_SIZE) == 0*/) {
				sei += UUID_SIZE;
				sei_size -= UUID_SIZE;

				if (buffer != NULL && count != NULL) {
					if (*count > (int)sei_size) {
						memcpy(buffer, sei, sei_size);
					}
				}

				*count = sei_size;

				return sei_size;
			}
			return -1;
		}

		uint32_t reversebytes(uint32_t value)
		{
			return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
				(value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
		}

		ByteArray_t VideoExtraDataAssemblyH264(ByteArray_t content, bool isAnnexb)
		{
			unsigned int nalu_size = (unsigned int)GetSEINaluSize(content->size());
			int actual_size = GetSEIPacketSize(content->size());

			ByteArray_t actual_data = MakeByteArray(actual_size);
			std::uint8_t* data = actual_data->data();

			uint32_t sei_size = nalu_size;
			//���תС��
			nalu_size = reversebytes(nalu_size);

			//NALU��ʼ��
			unsigned int* size_ptr = &nalu_size;
			if (isAnnexb)
			{
				memcpy(data, start_code, sizeof(unsigned int));
			}
			else
			{
				memcpy(data, size_ptr, sizeof(unsigned int));
			}
			data += sizeof(unsigned int);

			unsigned char* sei = data;
			//NAL header
			*data++ = 6; //SEI
						 //sei payload type
			*data++ = 5; //unregister
			size_t sei_payload_size = content->size() + UUID_SIZE;
			//���ݳ���
			while (true)
			{
				*data++ = (sei_payload_size >= 0xFF ? 0xFF : (char)sei_payload_size);
				if (sei_payload_size < 0xFF) break;
				sei_payload_size -= 0xFF;
			}

			//UUID
			memcpy(data, uuid, UUID_SIZE);
			data += UUID_SIZE;
			//����
			memcpy(data, content->data(), content->size());
			data += content->size();

			//tail ��ֹ������
			if (sei + sei_size - data == 1)
			{
				*data = 0x80;
			}
			else if (sei + sei_size - data == 2)
			{
				*data++ = 0x00;
				*data++ = 0x80;
			}

			return actual_data;
		}

		std::shared_ptr<std::vector<uint8_t>> VideoExtraDataParseInternal(AVPacket* packet, AVCodecID codec_id, int& sei_start_pos, int& sei_length)
		{
			static uint8_t h3[3] = { 0x00, 0x00, 0x01 };
			static uint8_t h4[4] = { 0x00, 0x00, 0x00, 0x01 };

			uint8_t* data = packet->data;
			int32_t  size = packet->size;

			std::shared_ptr<std::vector<std::uint8_t>> out_sei_buf;

			if (!data || size <= 0)
				return out_sei_buf;

			int finded = 0;

			int index = 0;
			for (auto i = 0; i < size - 4; i++) {
				finded = 0;
				if (memcmp(&data[i], h3, 3) == 0) {
					sei_start_pos = i;
					i += 3;
					finded = 1;
				}
				else if (memcmp(&data[i], h4, 4) == 0) {
					sei_start_pos = i;
					i += 4;
					finded = 1;
				}
				if (finded) {
					if (codec_id == AV_CODEC_ID_H264) {
						if ((data[i] & 0x1F) == 0x06) {
							index = i;
							break;
						}
					}
					else {
						uint16_t t = (*((uint16_t*)&(data[i])));
						if (((t & 0x7E) >> 1) == 39 || ((t & 0x7E) >> 1) == 40) {
							index = i;
							break;
						}
					}
				}
			}
			unsigned char actul_sei[32768] = { 0 };
			int count = size;
			unsigned char* sei = NULL;
			if (codec_id == AV_CODEC_ID_H264) {
				sei = (&data[index] + 1);
			}
			else if (codec_id == AV_CODEC_ID_H265) {
				sei = (&data[index] + 2);
			}
			int type = 0;
			int nsize = getSeiBuffer(sei, (data + size - sei), actul_sei, &count, &type);
			if (type == 5 && nsize > 0) {
				out_sei_buf = std::make_shared<std::vector<std::uint8_t>>(nsize);
				memcpy(out_sei_buf->data(), actul_sei, nsize);

				sei_length = GetSEIPacketSize(out_sei_buf->size());
			}
			return out_sei_buf;
		}

		AVPacket* VideoExtraDataReplace(AVPacket* packet, AVCodecID codec_id, ByteArray_t content, bool isAnnexb)
		{
			int sei_start_pos = 0;
			int sei_length = 0;
			auto old_sei_buffer = VideoExtraDataParseInternal(packet, codec_id, sei_start_pos, sei_length);
			if (old_sei_buffer && old_sei_buffer->size() > 0) {
				auto new_sei_buffer = VideoExtraDataAssemblyH264(content, isAnnexb);
				auto new_data_size = packet->size - sei_length + new_sei_buffer->size();
				AVPacket* new_packet = av_packet_alloc();
				if (0 == av_new_packet(new_packet, new_data_size)) {
					memcpy(new_packet->data, new_sei_buffer->data(), new_sei_buffer->size());
					if (sei_start_pos > 0) {
						memcpy(new_packet->data + new_sei_buffer->size(), packet->data, sei_start_pos);
						memcpy(new_packet->data + new_sei_buffer->size() + sei_start_pos,
							packet->data + sei_start_pos + sei_length, packet->size - sei_start_pos - sei_length);
					}
					else {
						memcpy(new_packet->data + new_sei_buffer->size(), packet->data + sei_length, packet->size - sei_length);
						//new_packet->size = new_sei_buffer->size() + (packet->size - sei_length);
					}
					new_packet->pos = packet->pos;
					new_packet->pts = packet->pts;
					new_packet->dts = packet->dts;
					new_packet->duration = packet->duration;
					new_packet->stream_index = packet->stream_index;
					new_packet->flags = packet->flags;

					return new_packet;
				}
			}
			else {
				auto new_sei_buffer = VideoExtraDataAssemblyH264(content, isAnnexb);
				auto new_data_size = packet->size + new_sei_buffer->size();
				AVPacket* new_packet = av_packet_alloc();
				if (0 == av_new_packet(new_packet, new_data_size)) {
					memcpy(new_packet->data, new_sei_buffer->data(), new_sei_buffer->size());
					memcpy(new_packet->data + new_sei_buffer->size(), packet->data, packet->size);

					new_packet->pos = packet->pos;
					new_packet->pts = packet->pts;
					new_packet->dts = packet->dts;
					new_packet->duration = packet->duration;
					new_packet->stream_index = packet->stream_index;
					new_packet->flags = packet->flags;

					return new_packet;
				}
			}
			return nullptr;
		}
#endif // USE_OLD_SEI_REPLACE		
	}
}