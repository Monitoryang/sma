#include "EapsImageCvtColorCuda.h"
#include "Logger.h"
#ifdef ENABLE_GPU
#include <nppcore.h>
#include <cuda_runtime.h>
#include <npp.h>
#include <nppi.h>

namespace eap {
	namespace sma {
		std::mutex ImageCvtColor::s_inst_mutex{};
		ImageCvtColorPtr ImageCvtColor::s_instance{};

		ImageCvtColorPtr ImageCvtColor::Instance()
		{
			std::lock_guard<std::mutex> lock(s_inst_mutex);
			if (!s_instance) {
				s_instance = ImageCvtColorPtr(new ImageCvtColor());
			}

			return s_instance;
		}

		void* ImageCvtColor::createCudaStream()
		{
			cudaStream_t _pS;
			auto ce = cudaStreamCreateWithFlags(&_pS, cudaStreamNonBlocking);  //create nonblocking stream
			if (ce != cudaSuccess) {
				eap_information("ImageColorConversion::cudaStream crea1te unsuccessfully");
				return 0;
			}
			return static_cast<void*>(_pS);
		}

		void ImageCvtColor::nv12ToBgr24(AVFrame* src, cv::cuda::GpuMat& dst, void* pStream)
		{
			cudaError_t cuda_error;
			NppStatus status;

			if (!pStream) {
				NppiSize oSize{ src->width, src->height };

				// status = nppiNV12ToRGB_8u_P2C3R(src->data, src->linesize[0],
				// 	dst.data, dst.step, oSize);
				status = nppiNV12ToBGR_8u_P2C3R(src->data, src->linesize[0],
					dst.data, dst.step, oSize);
				if (status != NPP_NO_ERROR) {
					eap_information("NV12ToBGR24: nppiNV12ToBGR_8u_P2C3R failed");
				}
			}
			else {
				NppiSize oSize{ src->width, src->height };

				cudaStream_t stream = static_cast<cudaStream_t>(pStream);
				nppSetStream(stream);

				// status = nppiNV12ToRGB_8u_P2C3R(src->data, src->linesize[0],
				// 	dst.data, dst.step, oSize);
				status = nppiNV12ToBGR_8u_P2C3R(src->data, src->linesize[0],
					dst.data, dst.step, oSize);
				if (status != NPP_NO_ERROR) {
					eap_information("NV12ToBGR24: nppiNV12ToBGR_8u_P2C3R failed");
				}
				//sync
				cudaStreamSynchronize(stream);
			}
		}

		void ImageCvtColor::bgr24ToBgr32(cv::cuda::GpuMat& src, cv::cuda::GpuMat& dst, void* pStream)
		{
			if (!pStream) {
				// cv::cuda::cvtColor(src, dst, cv::COLOR_RGB2RGBA);
				cv::cuda::cvtColor(src, dst, cv::COLOR_BGR2RGBA);
			}
			else {
				cudaStream_t stream = static_cast<cudaStream_t>(pStream);
				cv::cuda::Stream opencvStream = cv::cuda::StreamAccessor::wrapStream(stream);
				// cv::cuda::cvtColor(src, dst, cv::COLOR_RGB2RGBA, 0, opencvStream);
				cv::cuda::cvtColor(src, dst, cv::COLOR_BGR2RGBA, 0, opencvStream);
				//sync
				opencvStream.waitForCompletion();
			}
		}

		void ImageCvtColor::bgr32ToNV12(cv::cuda::GpuMat& src, AVFrame* dst)
		{

		}

		void ImageCvtColor::bgr32MatCopy(cv::cuda::GpuMat& src, cv::cuda::GpuMat& dst, void* pStream)
		{
			if (!pStream) {
				// printf("deviceSynchronized version");
				cudaMemcpy2D(dst.data, dst.step, src.data, src.step,
					src.cols * sizeof(unsigned char) * 4, src.rows, cudaMemcpyDeviceToDevice);
			}
			else {
				cudaStream_t stream = static_cast<cudaStream_t>(pStream);
				cudaMemcpy2DAsync(dst.data, dst.step, src.data, src.step,
					src.cols * sizeof(unsigned char) * 4, src.rows, cudaMemcpyDeviceToDevice, stream);
				//sync
				cudaStreamSynchronize(stream);
			}
		}

		void ImageCvtColor::bgr32MatToFrameCopy(cv::cuda::GpuMat& src, AVFrame* dst, void* pStream)
		{
			if (!pStream) {
				// printf("deviceSynchronized version");
				cudaMemcpy2D(dst->data[0], dst->linesize[0], src.data, src.step,
					src.cols * sizeof(unsigned char) * 4, src.rows, cudaMemcpyDeviceToDevice);
			}
			else {
				cudaStream_t stream = static_cast<cudaStream_t>(pStream);
				cudaMemcpy2DAsync(dst->data[0], dst->linesize[0], src.data, src.step,
					src.cols * sizeof(unsigned char) * 4, src.rows, cudaMemcpyDeviceToDevice, stream);
				//sync
				cudaStreamSynchronize(stream);
			}
		}
	}
}
#endif