#pragma once

#include "EapsMetaDataAssemblyInterface.h"
#include "Poco/JSON/Object.h"
#include <queue>

namespace eap {
	namespace sma {
		class MetaDataAssemblyGeoJsonx;
		using MetaDataAssemblyGeoJsonxPtr = std::shared_ptr<MetaDataAssemblyGeoJsonx>;

		class MetaDataAssemblyGeoJsonx : public MetaDataAssembly
		{
		public:
			static MetaDataAssemblyGeoJsonxPtr createInstance();
			void updateArData(const std::queue<int> ar_valid_point_index, const std::string ar_vector_file);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			void updateArData(const std::vector<std::vector<cv::Point>>& pixel_warning_l1_regions, const std::vector<std::vector<cv::Point>>& pixel_warning_l2_regions);
#endif
		public:
			virtual void updateMetaDataStructure(JoFmvMetaDataBasic meta_data_basic) override;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			virtual void updateArData(const std::vector<cv::Point>& pixel_points, const std::vector<std::vector<cv::Point>>& pixel_lines
				, ArInfosInternal ar_infos) override;
#endif
			virtual void updateAiHeapmapData(AiHeatmapInfo ai_heatmap_infos) override;
			virtual void updateFrameCurrentTime(int64_t current_time) override;
			virtual void updateVideoParams(int64_t video_duration, int bit_rate = 50000, int frame_rate = 30) override;

			virtual std::string getAssemblyString() override;

		private:
			virtual std::string assembly() override;
			void convertMultiLayerKMLToGeoJSON(const std::string& ar_vector_file, const std::string& geojson_output_directory);
			void deleteGeoJsonFiles(const std::string& geojson_output_directory);
			Poco::JSON::Array getGeoJsonxPointsArray();
		private:
			JoFmvMetaDataBasic _meta_data_basic{};
			std::string _assembly_str{};
			bool _have_basic{};

			std::string _geojson_output_directory{};
			std::string _ar_vector_file{};
			int _vector_layer_num{ 0 };
			std::queue<int> _ar_valid_point_index{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> _ar_pixel_points{};
			std::vector<std::vector<cv::Point>> _ar_pixel_lines{};
			std::vector<std::vector<cv::Point>> _ar_pixel_warning_l1_regions{};
			std::vector<std::vector<cv::Point>> _ar_pixel_warning_l2_regions{};
#endif
			ArInfosInternal _ar_infos{};
			AiHeatmapInfo _ai_heatmap_infos{};

			int64_t _frame_current_time{};
			int64_t _video_duration{};
			int _frame_rate{};
			int _bit_rate{};
		private:
			MetaDataAssemblyGeoJsonx() {}
			MetaDataAssemblyGeoJsonx(MetaDataAssemblyGeoJsonx& other) = delete;
			MetaDataAssemblyGeoJsonx(MetaDataAssemblyGeoJsonx&& other) = delete;
			MetaDataAssemblyGeoJsonx& operator=(MetaDataAssemblyGeoJsonx& other) = delete;
			MetaDataAssemblyGeoJsonx& operator=(MetaDataAssemblyGeoJsonx&& other) = delete;
		};
	}
}
