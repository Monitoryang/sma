#ifndef JO_VAS_IMAGE_CVT_COLOR_H
#define JO_VAS_IMAGE_CVT_COLOR_H
#ifdef ENABLE_GPU
#include <opencv2/core/cuda_stream_accessor.hpp>
#include <opencv2/opencv.hpp>
#ifndef _WIN32
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudaimgproc.hpp>
#else
#include "opencv2/cudaarithm.hpp"
#include "opencv2/cudaimgproc.hpp"
#endif

#include "EapsFFmpegWrap.h"

#include <mutex>
#include <memory>

namespace eap {
	namespace sma {
		class ImageCvtColor;
		using ImageCvtColorPtr = std::shared_ptr<ImageCvtColor>;

		class ImageCvtColor
		{
		public:
			static ImageCvtColorPtr Instance();

		public:
			void* createCudaStream();
			void nv12ToBgr24(AVFrame* src, cv::cuda::GpuMat& dst, void* pStream = 0);
			void bgr24ToBgr32(cv::cuda::GpuMat& src, cv::cuda::GpuMat& dst, void* pStream = 0);
			void bgr32ToNV12(cv::cuda::GpuMat& src, AVFrame* dst);
			void bgr32MatCopy(cv::cuda::GpuMat& src, cv::cuda::GpuMat& dst, void* pStream = 0);
			void bgr32MatToFrameCopy(cv::cuda::GpuMat& src, AVFrame* dst, void* pStream = 0);

		private:
			static std::mutex s_inst_mutex;
			static ImageCvtColorPtr s_instance;

		private:
			ImageCvtColor() {}
			ImageCvtColor(ImageCvtColor& other) = delete;
			ImageCvtColor(ImageCvtColor&& other) = delete;
			ImageCvtColor& operator=(ImageCvtColor& other) = delete;
			ImageCvtColor& operator=(ImageCvtColor&& other) = delete;
		};
	}
}
#endif
#endif // JO_VAS_IMAGE_CVT_COLOR_H

