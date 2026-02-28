#ifndef EAPS_PLAYBACK_ANNOTATION
#define EAPS_PLAYBACK_ANNOTATION
#pragma once
#include "EapsMacros.h"
#include "EapsDemuxerTradition.h"
#include "EapsPusherTradition.h"
#include "EapsMetaDataProcessing.h"
#include "EapsMuxer.h"
#include <memory>
#include <string>
#include <condition_variable>

namespace eap {
	namespace sma {
		class PlaybackAnnotation;
		using PlaybackAnnotationPtr = std::shared_ptr<PlaybackAnnotation>;

		class PlaybackAnnotation
		{
			public:
				struct InitParam
				{
					std::string playback_address{};
					std::string video_out_url{};
					std::string metadata_file_directory{};
					std::string task_id{};
				};

				using InitParamPtr = std::shared_ptr<InitParam>;

				static InitParamPtr makeInitParam();

				static PlaybackAnnotationPtr createInstance(InitParamPtr init_param);

			public:
				~PlaybackAnnotation();
				void start();
				void stop();

			private:
				DemuxerPtr _demuxer_tradition{};
				PusherPtr _pusherTradition{};

			private:
				std::string _playbackAddress{};
				std::string _videoOutUrl{};
				std::string _metadataDirectory{};
				std::string _taskId{};
				std::string _videoMarkMetadataDirectory{};
				std::condition_variable _decoded_images_cv{};
				MetaDataProcessingPtr _meta_data_processing{};
				AVCodecParameters _codec_parameter{};

			private:
				PlaybackAnnotation(InitParamPtr init_param);
				PlaybackAnnotation(PlaybackAnnotation& other) = delete;
				PlaybackAnnotation(PlaybackAnnotation&& other) = delete;
				PlaybackAnnotation& operator=(PlaybackAnnotation& other) = delete;
				PlaybackAnnotation& operator=(PlaybackAnnotation&& other) = delete;
		};
	}
}
#endif // !EAPS_PLAYBACK_ANNOTATION