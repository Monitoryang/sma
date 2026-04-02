#ifndef META_DATA_ASSEMBLY_JSON
#define META_DATA_ASSEMBLY_JSON

//#include "EapsMetaDataStructure.h"
#include "jo_meta_data_structure.h"
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#include <opencv2/core.hpp>
#endif
#include <memory>
#include <string>
#include <vector>

namespace eap {
	namespace sma {
		class MetaDataAssemblyJson;
		using MetaDataAssemblyJsonPtr = std::shared_ptr<MetaDataAssemblyJson>;

		class MetaDataAssemblyJson
		{
		public:
			static MetaDataAssemblyJsonPtr createInstance();

			void updateMetaDataStructure(JoFmvMetaDataBasic meta_data_basic);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			void updateArData(const std::vector<cv::Point> &pixel_points,
				const std::vector<std::vector<cv::Point>> &pixel_lines);
#endif
			void updateFrameCurrentTime(int64_t current_time);

			std::string getAssemblyString();

		private:
			std::string assembly();

		private:
			JoFmvMetaDataBasic _meta_data_basic{};
			std::string _assembly_str{};
			bool _have_basic{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> _ar_pixel_points{};
			std::vector<std::vector<cv::Point>> _ar_pixel_lines{};
#endif

			int64_t _frame_current_time{};
		private:
			MetaDataAssemblyJson() {}
			MetaDataAssemblyJson(MetaDataAssemblyJson& other) = delete;
			MetaDataAssemblyJson(MetaDataAssemblyJson&& other) = delete;
			MetaDataAssemblyJson& operator=(MetaDataAssemblyJson& other) = delete;
			MetaDataAssemblyJson& operator=(MetaDataAssemblyJson&& other) = delete;
		};
	}
}
#endif