#include "EapsMetaDataProcessing.h"

namespace eap {
	namespace sma {
		MetaDataProcessingPtr MetaDataProcessing::createInstance()
		{
			return MetaDataProcessingPtr(new MetaDataProcessing());
		}

		MetaDataProcessing::~MetaDataProcessing()
		{
			_frame_pos_process_mutex.lock();
			if (_frame_pos_process_pointer) {
				jo_meta_data_processor_destroy(_frame_pos_process_pointer);
				_frame_pos_process_pointer = nullptr;
			}
			_frame_pos_process_mutex.unlock();
		}

		std::pair<JoFmvMetaDataBasic*, FRAME_POS_Type*> MetaDataProcessing::metaDataParseBasic(
			std::uint8_t* data, std::int32_t size, AVCodecID codec_id, std::vector<uint8_t>& raw_sei_data)
		{
			std::lock_guard<std::mutex> lock(_frame_pos_process_mutex);

			if (!_frame_pos_process_pointer) {
				std::pair<JoFmvMetaDataBasic*, FRAME_POS_Type*>();
			}

			std::pair<JoFmvMetaDataBasic*, FRAME_POS_Type*> meta_data_wrap{};

			EncodeAlgo encode_algo;
			if (codec_id == AV_CODEC_ID_H264) {
				encode_algo = ENCODE_ALGO_H264;
			}
			else if (codec_id == AV_CODEC_ID_H265) {
				encode_algo = ENCODE_ALGO_H265;
			}
			else {
				return meta_data_wrap;
			}

			uint8_t* out_sei_data{};
			int out_sei_size{};
			auto result = jo_video_sei_data_parse(data, size, encode_algo, &out_sei_data, &out_sei_size);
			if (!result) {
				return meta_data_wrap;
			}

			if (out_sei_data && out_sei_size > 0) {
				raw_sei_data = std::vector<uint8_t>(out_sei_size);
				memcpy(raw_sei_data.data(), out_sei_data, out_sei_size);
			}

			ProtocalVersion proto_vsersion;
			uint8_t* real_data{};
			int real_size{};
			result = jo_video_sei_data_checking(out_sei_data, out_sei_size, &real_data, &real_size, &proto_vsersion);
			jo_meta_data_processing_buffer_free(out_sei_data);
			if (!result) {
				return meta_data_wrap;
			}

			jo_meta_data_processor_update_data(_frame_pos_process_pointer, real_data, real_size, proto_vsersion);
			jo_meta_data_processing_buffer_free(real_data);

			meta_data_wrap.second = jo_meta_data_processor_get_frame_pos(_frame_pos_process_pointer);
			meta_data_wrap.first = jo_meta_data_processor_get_meta_data_basic(_frame_pos_process_pointer);

			return meta_data_wrap;
		}
#ifdef ENABLE_AIRBORNE
		FRAME_POS_Qianjue* MetaDataProcessing::metaDataParseQianjue(std::uint8_t* data, std::int32_t size, AVCodecID codec_id)
		{
			FRAME_POS_Qianjue* meta_data_wrap{};
			EncodeAlgo encode_algo;
			if (codec_id == AV_CODEC_ID_H264) {
				encode_algo = ENCODE_ALGO_H264;
			}
			else if (codec_id == AV_CODEC_ID_H265) {
				encode_algo = ENCODE_ALGO_H265;
			}
			else {
				return meta_data_wrap;
			}

			uint8_t* out_sei_data{};
			int out_sei_size{};
			auto result = jo_video_sei_data_parse(data, size, encode_algo, &out_sei_data, &out_sei_size);
			if (!result) {
				return meta_data_wrap;
			}

			ProtocalVersion proto_vsersion = ProtocalVersion::ProtocalVersion_Qianjue;
			jo_meta_data_processor_update_data(_frame_pos_process_pointer, out_sei_data, out_sei_size, proto_vsersion);
			meta_data_wrap = jo_meta_data_processor_get_meta_data_qianjue(_frame_pos_process_pointer);
			jo_meta_data_processing_buffer_free(out_sei_data);
			if (!result) {
				return meta_data_wrap;
			}
			return meta_data_wrap;
		}
#endif

		void MetaDataProcessing::setRawSeiData(uint8_t* raw_sei_data, int raw_sei_size)
		{
			ProtocalVersion proto_vsersion;
			uint8_t* real_data{};
			int real_size{};
			auto result = jo_video_sei_data_checking(raw_sei_data, raw_sei_size, &real_data, &real_size, &proto_vsersion);
			if (!result) {
				return;
			}

			jo_meta_data_processor_update_data(_frame_pos_process_pointer, real_data, real_size, proto_vsersion);
			jo_meta_data_processing_buffer_free(real_data);
		}

		void MetaDataProcessing::updateAiData(AiInfos* ai_infos)
		{
			jo_meta_data_processor_update_ai_data(_frame_pos_process_pointer, ai_infos);
		}

		void MetaDataProcessing::updateArData(ArInfos* ar_infos)
		{
			jo_meta_data_processor_update_ar_data(_frame_pos_process_pointer, ar_infos);
		}

		std::vector<uint8_t> MetaDataProcessing::getSerializedBytes()
		{
			int size = 0;
			auto buffer = jo_meta_data_processor_get_serialized_bytes(_frame_pos_process_pointer, &size);

			if (size > 0 && buffer) {
				auto buffer_vec = std::vector<uint8_t>(size);
				memcpy(buffer_vec.data(), buffer, size);
				return buffer_vec;
			}
			return std::vector<uint8_t>();
		}

		std::vector<uint8_t> MetaDataProcessing::getSerializedBytesBySetMetaDataBasic(JoFmvMetaDataBasic* meta_data_basic, int* serialized_bytes_size)
		{
			auto buffer = jo_meta_data_processor_set_meta_data_basic(_frame_pos_process_pointer, meta_data_basic, serialized_bytes_size);
			if ((*serialized_bytes_size > 0) && buffer) {
				auto buffer_vec = std::vector<uint8_t>(*serialized_bytes_size);
				memcpy(buffer_vec.data(), buffer, *serialized_bytes_size);
				return buffer_vec;
			}
			return std::vector<uint8_t>();
		}

		JoFmvMetaDataBasic* MetaDataProcessing::getMetaDataBasic()
		{
			return jo_meta_data_processor_get_meta_data_basic(_frame_pos_process_pointer);
		}

		std::vector<uint8_t> MetaDataProcessing::seiDataAssemblyH264(uint8_t* content, int content_size)
		{
			int out_sei_data_size{ 0 };
			uint8_t* out_sei_data{};
			std::vector<uint8_t> assemblied_data{};

			bool return_value = jo_video_sei_data_assembly_h264(content,
				content_size, &out_sei_data, &out_sei_data_size);
			if (return_value && out_sei_data && out_sei_data_size > 0) {
				assemblied_data = std::vector<uint8_t>(out_sei_data_size);
				memcpy(assemblied_data.data(), out_sei_data, out_sei_data_size);
			}
			jo_meta_data_processing_buffer_free(out_sei_data);

			return assemblied_data;
		}

		MetaDataProcessing::MetaDataProcessing()
		{
			_frame_pos_process_pointer = jo_meta_data_processor_create();
		}
	}
}