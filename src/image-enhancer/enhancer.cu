#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include<cuda_runtime.h>
#include<thrust/device_vector.h>
#include<thrust/device_ptr.h>
#include<thrust/reduce.h>
#include<thrust/sort.h>

/////////////////////////////////////////////////////
// These are CUDA Helper functions

inline bool __checkCudaErrors(cudaError err)
{
	if (cudaSuccess != err)
	{
		fprintf(stderr, "%s(%i):CUDA Runtim API error %d: %s.\n",
			__FILE__, __LINE__, (int)err, cudaGetErrorString(err));
		return false;
	}
	return true;
}


namespace JAV {
	namespace IMP {
		namespace Enhancer {

            //\\    DefogEnhancer
			__global__ void kernel_GetDarkchannel_8U(unsigned char* pRgb, unsigned char* pDark, int width, int height, size_t pitch)
			{
				unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
				unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;

				if (x >= width || y >= height) return;

				unsigned char temp = 255;
				unsigned int currentIndex= ((y * width)<<2) + (x<<2);
				unsigned char pixel;
				for (unsigned int i = currentIndex; i < currentIndex+ 3; ++i)
				{
                    pixel = pRgb[i];
					if ( pixel< temp)
						temp = pixel;
				}
				pDark[y*width+x] = temp;
			}

			__global__ void kernel_SolveEnvironment(unsigned char* dst, unsigned char * pBlurredDark, unsigned char* pDarkChannel, const float min, int width, int height, size_t pitch)
			{
				unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
				unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;

				if (x >= width || y >= height) return;

				unsigned int currentIndex = y * width + x;
				unsigned char blurredpixel = pDarkChannel[currentIndex];
				float minPixel = min * blurredpixel;
				if (minPixel > blurredpixel)
					minPixel = blurredpixel;;
				dst[currentIndex] = (unsigned char)minPixel;
			
			}

			__global__ void kernel_GetDefogged(unsigned char* dst, unsigned char* src, unsigned char* Ex, float A, int width, int height, size_t pitch)
			{
				unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
				unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;

				if (x >= width || y >= height) return;

				//for rgb
				unsigned int currentIndexEx = y * width + x;
				unsigned int currentIndexDst = ((y * width) << 2) + (x << 2);
				float result;
				unsigned char ex = Ex[currentIndexEx];
				float temp = (1.0 - (float)ex / A);
				for (int i = 0; i < 3; ++i) {
					result = (src[currentIndexDst+i] - ex) / temp;
					if (result > 255.0)
						result = 255;
					dst[currentIndexDst+i] = (unsigned char)result;
				}

			}

			extern "C"
				bool getDarkchannel_8U(void *src, void * ptrGpuMat, int width, int height, size_t pintch, cudaStream_t stream)
			{

				dim3 Db = dim3(32, 32);   // block dimensions are fixed to be 1024 threads
				dim3 Dg = dim3((width + Db.x - 1) / Db.x, (height + Db.y - 1) / Db.y);

				kernel_GetDarkchannel_8U << <Dg, Db >> > ((unsigned char *)src, (unsigned char *)ptrGpuMat, width, height, pintch);
				if (!stream)
				{
					//synchronize
					if (!__checkCudaErrors(cudaDeviceSynchronize()))
						return false;
				}

				return true;
			}
			extern "C"
				bool SolveEnvirmentValue(void *dst, void* pBlurredDark, void* pDarkChannel, const float rou, int width, int height, size_t pitch)
			{
				//use thrust to get the mean value of Dark Channel 
				thrust::device_ptr<unsigned char> ptrStart((unsigned char*)pDarkChannel);
				thrust::device_ptr<unsigned char> ptrEnd((unsigned char*)pDarkChannel + width * height);

				// calculation time is long in debug mode
			    //      description: this procedure cost too much time (more than 40ms)
				// reason: it is that in the debug mode the excu is slower supremely than in release mode (about 0.1ms)
				thrust::device_vector<unsigned char> vec(ptrStart, ptrEnd);
				int sum = thrust::reduce(vec.begin(), vec.end(),0, thrust::plus<int>());

				float mean = (float)sum / (float)(width*height*255.0);   //make sure mean is in(0,1)
				float min = (rou*mean) < 0.9 ? rou * mean : 0.9;
				//lanuch kenel to estimate the environment component
				dim3 Db = dim3(32, 32);   // block dimensions are fixed to be 1024 threads
				dim3 Dg = dim3((width + Db.x - 1) / Db.x, (height + Db.y - 1) / Db.y);
				kernel_SolveEnvironment << <Dg, Db, 0 >> > ((unsigned char *)dst, (unsigned char *)pBlurredDark, (unsigned char *)pDarkChannel, min, width,  height,width);
				
				//synchronize
				if (!__checkCudaErrors(cudaDeviceSynchronize()))
					return false;

				return true;
			}
			extern "C"
				bool getDefoggedResult(void* dst, void* src,void *Ex, float A, int width, int height, size_t pitch)
			{
				dim3 Db = dim3(32, 32);   // block dimensions are fixed to be 1024 threads
				dim3 Dg = dim3((width + Db.x - 1) / Db.x, (height + Db.y - 1) / Db.y);
				kernel_GetDefogged << <Dg, Db, 0 >> > ((unsigned char *)dst, (unsigned char *)src,(unsigned char *)Ex, A, width, height,pitch);

				//synchronize
				if (!__checkCudaErrors(cudaDeviceSynchronize()))
					return false;

				return true;
			}

            //\\    DefogEnhancer


            //\\    DelowEnhancer

            __global__ void kernel_Delow(unsigned char *data,int width, int height,float gama)
                {
                    unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
                    unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;

                    if (x >= width || y >= height) return;

                    unsigned int currentIndex = ((y * width) << 2) + (x << 2);
                    float normPixel;
                    unsigned char result;
                    for (unsigned int i = currentIndex; i < currentIndex+3; ++i)
                    {
                        normPixel = (float)data[i] / 255.0;
                        result = 255 * powf(normPixel, gama);
                        if (result > 255) result = 255;

                        data[i] = result;
                    }
                }

            extern "C"
                unsigned char GetLowPartPixelValue(unsigned char* data, int width, int height)
                {
                    int length = 2048 * height;
                    thrust::device_ptr<unsigned char> ptrStart((unsigned char*)data);
                    thrust::device_ptr<unsigned char> ptrEnd((unsigned char*)data + length);
                    thrust::device_vector<unsigned char> vec(ptrStart, ptrEnd);
                    thrust::sort(vec.begin(), vec.begin()+length);
                    int end = (2048-1920+192)*1080;
                    int sum = thrust::reduce(vec.begin() , vec.begin() + end, 0, thrust::plus<int>());
                    unsigned char result = (float)sum /(float) (192*1080);
                    return result;
                }

            extern "C"
                bool ImageDeLow(unsigned char* data, int width, int height, unsigned char t, cudaStream_t s)
                {
                    dim3 Db = dim3(32, 32);   // block dimensions are fixed to be 1024 threads
                    dim3 Dg = dim3((width + Db.x - 1) / Db.x, (height + Db.y - 1) / Db.y);
                    //求伽马值
                    float gama = 0.01186 *t + 0.38457;
                    kernel_Delow << <Dg,Db,0,s >> > (data, width, height, gama);
                    cudaError_t e = cudaGetLastError();
                    if (e != cudaSuccess)
                    {
                        fprintf(stderr, "%s(%i):CUDA Runtim API error %d: %s.\n",
                            __FILE__, __LINE__, (int)e, cudaGetErrorString(e));

                    }
                    //printf("%d\n", t);
                    return true;
                }

            //\\    DelowEnhancer



        }
	}
}
