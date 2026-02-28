#pragma once

#include "EapsFFmpegWrap.h"
#include "jo_meta_data_processing.h"

#include <utility>
#include <memory>
#include <mutex>
#include <vector>

namespace eap {
	namespace sma {
		class MetaDataProcessing;
		using MetaDataProcessingPtr = std::shared_ptr<MetaDataProcessing>;

		class MetaDataProcessing
		{
		public:
			static MetaDataProcessingPtr createInstance();

		public:
			~MetaDataProcessing();

			std::pair<JoFmvMetaDataBasic*, FRAME_POS_Type*> metaDataParseBasic(
				std::uint8_t* data, std::int32_t size, AVCodecID codec_id,
				std::vector<uint8_t>& raw_sei_data);
#ifdef ENABLE_AIRBORNE
			FRAME_POS_Qianjue* metaDataParseQianjue(std::uint8_t* data, std::int32_t size, AVCodecID codec_id);
#endif
			void setRawSeiData(uint8_t* raw_sei_data, int raw_sei_size);
			void updateAiData(AiInfos* ai_infos);
			void updateArData(ArInfos* ar_infos);
			std::vector<uint8_t> getSerializedBytes();
			std::vector<uint8_t> getSerializedBytesBySetMetaDataBasic(JoFmvMetaDataBasic* meta_data_basic, int* serialized_bytes_size);
			JoFmvMetaDataBasic* getMetaDataBasic();

			static std::vector<uint8_t> seiDataAssemblyH264(uint8_t* content, int content_size);
		private:
			std::mutex _frame_pos_process_mutex{};
			MetaDataProcessor* _frame_pos_process_pointer{};

		private:
			MetaDataProcessing();
			MetaDataProcessing(MetaDataProcessing& other) = delete;
			MetaDataProcessing(MetaDataProcessing&& other) = delete;
			MetaDataProcessing& operator=(MetaDataProcessing& other) = delete;
			MetaDataProcessing& operator=(MetaDataProcessing&& other) = delete;
		};
	}
}

