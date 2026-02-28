#ifndef META_DATA_ASSEMBLY_JSON
#define META_DATA_ASSEMBLY_JSON

#include "jo_meta_data_structure.h"
#include "EapsCommon.h"
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#include <opencv2/core.hpp>
#endif
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace eap {
	namespace sma {
		class MetaDataAssembly;
		using MetaDataAssemblyPtr = std::shared_ptr<MetaDataAssembly>;

		class MetaDataAssembly
		{
		public:

			virtual void updateMetaDataStructure(JoFmvMetaDataBasic meta_data_basic) = 0;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			virtual void updateArData(const std::vector<cv::Point>& pixel_points, const std::vector<std::vector<cv::Point>>& pixel_lines
				, ArInfosInternal ar_infos) = 0;
#endif
			virtual	void updateAiHeapmapData(AiHeatmapInfo ai_heatmap_infos) = 0;
			virtual void updateFrameCurrentTime(int64_t current_time) = 0;
			virtual void updateVideoParams(int64_t video_duration, int bit_rate=50000, int frame_rate=30) = 0;

			virtual std::string getAssemblyString() = 0;

		protected:
			virtual std::string assembly() = 0;
		};
	}
}
#endif