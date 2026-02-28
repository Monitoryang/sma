#include"EapsImageEnhancer.h"

#include<thread>
#include<mutex>

#include<cuda_runtime_api.h>
#include<driver_types.h>

#include<opencv2/core/cuda.hpp>
#include<opencv2/cudafilters.hpp>
#include<opencv2/imgcodecs.hpp>
#include<opencv2/cudaimgproc.hpp>
#include<opencv2/core/cuda_stream_accessor.hpp>

#define PrintInfo(msg) std::cout<<msg<<std::endl 

namespace eap {
	namespace sma {
		namespace Enhancer {

			namespace {
				extern "C"
				{
					bool getDarkchannel_8U(void* src, void* ptrGpuMat, int width, int height, size_t pintch, cudaStream_t stream);
					bool SolveEnvirmentValue(void* dst, void* pBlurredDark, void* pDarkChannel, const float rou, int width, int height, size_t pitch);
					bool getDefoggedResult(void* dst, void* src, void* Ex, float A, int width, int height, size_t pitch);
				}

				class DefogEnhancer :public EnhancerBase
				{
					struct defogParameter {
						// environment optics estimation parameter
						// Higher value, stronger defog effect
						const float rou = 1.25;
						//global optics value
						const unsigned char Amax = 220;
						// Filter Size
						const int filterSize = 13;
					};
					defogParameter* parameter;

				public:
					DefogEnhancer(int width, int height);
					~DefogEnhancer();
					virtual bool Enhance(void* IOImagePtr)override;

				private:
					virtual bool Run() override;
					virtual bool GetEnhancedImage(void* const output) override;

					bool GetDarkChannelImage();
					bool BlurDarkChannel();
					bool SolveEnvironmentValue();
					bool GetDefoggedResult();
				private:
					void* pCacheCudaMemory;
					void* pEnhancedImage;
					void* pDarkChannel, * pBlurredChannelImage, * pEnvironment;    //intermidiate memory
					cudaStream_t stream = 0;
					std::mutex mtx;
					size_t _pitch;

					//img processing related
					cv::Ptr<cv::cuda::Filter> filterPtr;

				};

				DefogEnhancer::DefogEnhancer(int width, int height) {
					_width = width, _height = height/*,_pitch=width*4*/;
					parameter = new defogParameter;

					cv::Mat kernel(parameter->filterSize, 1, CV_32FC1);
					kernel.setTo(1.0 / (double(parameter->filterSize)));
					this->filterPtr = cv::cuda::createSeparableLinearFilter(CV_8UC1, CV_8UC1, kernel, kernel);

					//cuda memory allocate
					cudaMallocPitch(&pCacheCudaMemory, &_pitch, _width * 4, _height);
					cudaMalloc(&pDarkChannel, _pitch * _height / 4);
					cudaMalloc(&pBlurredChannelImage, _pitch * _height / 4);
					cudaMalloc(&pEnvironment, _pitch * _height / 4);
					cudaMalloc(&pEnhancedImage, _pitch * _height);
				}

				DefogEnhancer::~DefogEnhancer() {
					if (pCacheCudaMemory)
					{
						cudaFree(pCacheCudaMemory);
						pCacheCudaMemory = nullptr;
					}
					if (pDarkChannel)
					{
						cudaFree(pDarkChannel);
						pDarkChannel = nullptr;
					}
					if (pBlurredChannelImage)
					{
						cudaFree(pBlurredChannelImage);
						pBlurredChannelImage = nullptr;
					}
					if (pEnvironment)
					{
						cudaFree(pEnvironment);
						pEnvironment = nullptr;
					}
					if (pEnhancedImage)
					{
						cudaFree(pEnhancedImage);
						pEnhancedImage = nullptr;
					}
					if (pEnhancedImage)
					{
						cudaFree(pEnhancedImage);
						pEnhancedImage = nullptr;
					}
					if (parameter)
					{
						delete parameter;
						parameter = nullptr;
					}
				}

				bool DefogEnhancer::Enhance(void* IOImagePtr)
				{
					try {

						//cudaMemoryCpy IOImagePtr to pCacheCudaMemory
						cudaMemcpy2D(pCacheCudaMemory, _pitch, IOImagePtr, _pitch, _width * 4, _height, cudaMemcpyDeviceToDevice);
						cudaDeviceSynchronize();

						//TODO: if processed time is longer than 5ms,using thread to run this function 
						this->Run();

						this->GetEnhancedImage(IOImagePtr);

						return true;
					}
					catch (std::exception& e)
					{
						PrintInfo("DefogEnhancer::Enhancer()::exception " << e.what());
						return false;
					}
				}

				bool DefogEnhancer::Run()
				{//\ main defog pipeline,use pCacheCudaMemory as input Image

					//\step 1 get dark channel
					if (!getDarkchannel_8U(pCacheCudaMemory, pDarkChannel, _width, _height, _pitch, stream))
					{
						PrintInfo("DefogEnhancer::Run::error occur in Getting DarkChannle Image");
						return false;
					}

					//\step 2 get blurred image
					if (!this->BlurDarkChannel())
						PrintInfo("DefogEnhancer::Run::error occur in Getting blurred image Image");
					//end1 = clock();
					//std::cout << "calculation time:end2 " << end1 - start << "ms" << std::endl;

					//\step 3 estimate environment 
					if (!this->SolveEnvironmentValue())
						PrintInfo("DefogEnhancer::Run::error occur in Solving environment Image");

					//\step 4 get enhancedImage
					if (!this->GetDefoggedResult())
						PrintInfo("DefogEnhancer::Run::error occur in GetDefoggedResult Image");

					return true;
				}

				bool DefogEnhancer::GetEnhancedImage(void* const output)
				{
					//\ copy result to output cuda memory
					cudaMemcpy2D(output, _pitch, pEnhancedImage, _pitch, _width * 4, _height, cudaMemcpyDeviceToDevice);
					cudaDeviceSynchronize();
					return true;
				}

				bool DefogEnhancer::GetDarkChannelImage()
				{
					if (!getDarkchannel_8U(pCacheCudaMemory, pDarkChannel, _width, _height, _pitch, stream))
						return false;
					return true;
				}

				bool DefogEnhancer::BlurDarkChannel()
				{
					try {
						cv::cuda::GpuMat src = cv::cuda::GpuMat(_height, _width, CV_8UC1, pDarkChannel);
						cv::cuda::GpuMat dst = cv::cuda::GpuMat(_height, _width, CV_8UC1, pBlurredChannelImage);
						filterPtr->apply(src, dst);
					}
					catch (std::exception& e) {
						PrintInfo("error in blurdarkchannel:" << e.what());
					}

					return true;
				}

				bool DefogEnhancer::SolveEnvironmentValue()
				{
					if (!SolveEnvirmentValue(pEnvironment, pBlurredChannelImage, pDarkChannel, parameter->rou, _width, _height, _pitch))
					{
						PrintInfo("ERROR in DefogEnhancer::SolveEnviromentValue");
						return false;
					}
					return true;
				}

				bool DefogEnhancer::GetDefoggedResult()
				{
					if (!getDefoggedResult(pEnhancedImage, pCacheCudaMemory, pEnvironment, parameter->Amax, _width, _height, _pitch))
					{
						PrintInfo("ERROR in DefogEnhancer::GetDefoggedResult");
						return false;
					}
					return true;
				}

			}

			EnhancerPtr CreateDefogEnhancer(int width, int height)
			{
				return std::make_shared<DefogEnhancer>(width, height);
			}

			namespace {
				extern "C"
				{
					unsigned char GetLowPartPixelValue(unsigned char* data, int width, int height);
					bool ImageDeLow(unsigned char* data, int width, int height, unsigned char t, cudaStream_t s);
				}

				class DelowEnhancer : public EnhancerBase {
				public:
					DelowEnhancer(int width, int height);
					~DelowEnhancer();
					virtual bool Enhance(void* const IOimage) override;

				private:
					virtual bool Run() override;
					virtual bool GetEnhancedImage(void* const output) override;

				private:
					cv::cuda::GpuMat gray;
					cv::cuda::GpuMat processed;
					std::vector<unsigned char> vMeanLowPartPixel;
					unsigned char frameCount = 0;
					cv::cuda::Stream cvStreamWraper;
				};

				DelowEnhancer::DelowEnhancer(int width, int height)
				{
					vMeanLowPartPixel = std::vector<unsigned char>(16, 52);
					_width = width, _height = height;
					cudaStream_t nonBlockStream;
					cudaStreamCreateWithFlags(&nonBlockStream, cudaStreamNonBlocking);
					cv::cuda::StreamAccessor::wrapStream(nonBlockStream);
					this->gray = cv::cuda::GpuMat(_height, _width, CV_8UC1);
				}

				DelowEnhancer::~DelowEnhancer()
				{
					if (!gray.empty()) gray.release();
					cudaStream_t s = cv::cuda::StreamAccessor::getStream(this->cvStreamWraper);
					cudaStreamDestroy(s);
				}

				bool DelowEnhancer::Enhance(void* const IOimage)
				{
					this->processed = cv::cuda::GpuMat(_height, _width, CV_8UC4, IOimage);
					// copy to
					this->Run();

					//// get result
					//this->GetEnhancedImage(IOimage);
					return true;
				}

				bool DelowEnhancer::Run()
				{
					// color covert to gray
					cv::cuda::cvtColor(this->processed, this->gray, cv::COLOR_RGBA2GRAY, 0, cvStreamWraper);
					cvStreamWraper.waitForCompletion();

					//\pixel sort and 10% low part pixel reduce to sum
					//\using thrust
					auto A = GetLowPartPixelValue(this->gray.data, _width, _height);
					if (frameCount == 16)
					{
						frameCount = 0;
					}

					vMeanLowPartPixel[frameCount++] = A;
					// Enhance image
					cudaStream_t s = cv::cuda::StreamAccessor::getStream(this->cvStreamWraper);
					float t = 0;
					for (auto it = vMeanLowPartPixel.begin(); it != vMeanLowPartPixel.end(); it++)
					{
						t += *it * 1 / 16;
					}
					if (!ImageDeLow(this->processed.data, _width, _height, t, s))
						return false;

					cvStreamWraper.waitForCompletion();

					return true;
				}

				bool DelowEnhancer::GetEnhancedImage(void* const output)
				{
					return false;
				}
			}

			EnhancerPtr CreateDelowEnhancer(int width, int height)
			{
				return std::make_shared<DelowEnhancer>(width, height);
			}
		}
	}
}

