#ifndef EAPS_META_DATA_PROCESSING_H
#define EAPS_META_DATA_PROCESSING_H

#if USE_OLD_SEI_REPLACE
//#include "EapsMetaDataStructure.h"
#include "jo_meta_data_structure.h"
#include "EapsFFmpegWrap.h"
#include "EapsMetaDataAssemblyJson.h"

#include <memory>
#include <vector>
#include <string>

namespace eap {
	namespace sma {
		using MetaDataBasicPtr = std::shared_ptr<JoFmvMetaDataBasic>;

		class MetaDataProcessing;
		using MetaDataProcessingPtr = std::shared_ptr<MetaDataProcessing>;

		class MetaDataProcessing
		{
		public:
			static MetaDataProcessingPtr createInstance();

		public:
			// 获取元数据结构体，取值或者赋值
			JoFmvMetaDataBasic& getMetaDataStructure();

			// 将元数据结构体转换成json字符串
			// TODO: 先转换为元数据结构的json，后续将其标准化为GeoJson
			const std::string& assemblyToJson();

			// 压缩json字符串并分装消息，封装SEI NALU
			const std::vector<std::uint8_t>& assemblyToH264Sei();

			// 将封装好的SEI NALU封装到packet，如果有则替换
			Packet assemblyToPacket(Packet packet);

		private:
			JoFmvMetaDataBasic _meta_data_structure{};
			std::string _meta_data_json{};
			std::vector<std::uint8_t> _meta_data_sei{};

			MetaDataAssemblyJsonPtr _meta_data_assembly_json{};

		private:
			MetaDataProcessing();
			MetaDataProcessing(MetaDataProcessing& other) = delete;
			MetaDataProcessing(MetaDataProcessing&& other) = delete;
			MetaDataProcessing& operator=(MetaDataProcessing& other) = delete;
			MetaDataProcessing& operator=(MetaDataProcessing&& other) = delete;
		};
	}
}
#endif //USE_OLD_SEI_REPLACE

#endif // !EAPS_META_DATA_PROCESSING_H