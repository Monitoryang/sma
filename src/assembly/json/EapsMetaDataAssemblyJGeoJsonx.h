#pragma once

#include "EapsMetaDataAssemblyInterface.h"
#include "Poco/JSON/Object.h"
#include "EapsMqttClient.h"
#include "jo_ai_detect_common.h"
#include <queue>
#include <atomic>
#include <mutex>

namespace eap {
	namespace sma {
		class MetaDataAssemblyGeoJsonx;
		using MetaDataAssemblyGeoJsonxPtr = std::shared_ptr<MetaDataAssemblyGeoJsonx>;

		class MetaDataAssemblyGeoJsonx : public MetaDataAssembly
		{
		public:
			/** 初始化 MQTT 客户端（在任务启动时调用一次） */
    		void initMqttClient();
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
			virtual void updateAiDetectInfo(const AiInfos& ai_infos) override;
			virtual void updateVideoSize(int width, int height) override;
			virtual void updateFrameCurrentTime(int64_t current_time) override;
			virtual void updateVideoParams(int64_t video_duration, int bit_rate=50000, int frame_rate=30) override;
			virtual void updateVideoParams2(int64_t video_duration, int bit_rate = 50000, int frame_rate = 30, int width = 1920, int height = 1080) override;

			virtual std::string getAssemblyString() override;

			/** 实时计算 AI 检测各框的地理坐标（lon, lat, alt），供发送 POST 请求时调用 */
			std::vector<std::tuple<double, double, double>> calcAiGeoLocations(
				const std::vector<joai::Result>& ai_detect_ret, int img_w, int img_h);

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

			AiInfos _direct_ai_infos{};
			std::atomic_bool _has_direct_ai{false};
			std::mutex _direct_ai_mutex{};

			int64_t _frame_current_time{};
			int64_t _video_duration{};
			int _frame_rate{};
			int _bit_rate{};
			int _video_width{};
            int _video_height{};
		private:
			MetaDataAssemblyGeoJsonx() {}
			MetaDataAssemblyGeoJsonx(MetaDataAssemblyGeoJsonx& other) = delete;
			MetaDataAssemblyGeoJsonx(MetaDataAssemblyGeoJsonx&& other) = delete;
			MetaDataAssemblyGeoJsonx& operator=(MetaDataAssemblyGeoJsonx& other) = delete;
			MetaDataAssemblyGeoJsonx& operator=(MetaDataAssemblyGeoJsonx&& other) = delete;
			// MQTT 遥测数据
    		EapsMqttClient::Ptr _mqtt_client;
		};
	}
}
