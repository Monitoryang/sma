#include "img_conv.h"
#ifdef ENABLE_AIRBORNE
#include <stdio.h>
#include <sys/time.h>
#include <driver_types.h>
#include <cuda_runtime_api.h>
#include <cuda_device_runtime_api.h>
#include <device_launch_parameters.h>
#include <cuda_runtime.h>
#include <assert.h>

namespace eap{
	namespace sma{
		void AllocDeviceMem(char** pBuf, int size)
		{
			cudaMalloc((void**)pBuf, size);
		}

		void FreeDeviceMem(char** pBuf)
		{
			cudaFree(*pBuf);
			*pBuf = NULL; 
		}

		inline  __device__ unsigned char saturate_clip(float a)
		{
			if (a <= 255.0)
			{
				if( a>=0 )
				{
					return (unsigned char)a;
				}
				else
				{
					return 0;
				}
			}
			else
			{
				return 255; 
			}
		}

		__global__ void kernel_i4202bgr(char *pIn420Dev, char *pBgrDev, int src_w, int src_h, int offset_x, int offset_y, int dst_w, int dst_h)
		{
			#define clip(a) ((a<=255)?((a>=0)?a:0):255)

			int dst_x = (threadIdx.x + blockIdx.x * blockDim.x) * 2;
			int dst_y = (threadIdx.y + blockIdx.y * blockDim.y) * 2;
			int src_x = dst_x + offset_x;
			int src_y = dst_y + offset_y;

			if((dst_x >= dst_w) || (dst_y >= dst_h))
			{
				return; 
			}
				
			unsigned char l1,l2,l3,l4,u,v;
			
			int src_pos = src_y * src_w + src_x;    
			unsigned char* pY = (unsigned char *)(pIn420Dev + src_pos);
			unsigned char* pU = (unsigned char *)(pIn420Dev + src_w * src_h + (src_y*src_w>>2) + (src_x>>1));
			unsigned char* pV = (unsigned char *)(pIn420Dev + ((src_w * src_h * 5 + src_y * src_w)>>2) + (src_x>>1));

			int dst_pos = dst_y * dst_w + dst_x;
			unsigned char* b = (unsigned char *)(pBgrDev + dst_pos * 3);
			unsigned char* g = b + 1;
			unsigned char* r = b + 2;	

			l1 = (unsigned char)pY[0];
			u = (unsigned char)(*pU);
			v = (unsigned char)(*pV);

			*b = saturate_clip((1.164*(l1 - 16) + 2.017 * (u - 128)));
			*g = saturate_clip((1.164*(l1 - 16) - 0.813 * (v - 128)-0.392 * (u - 128)));
			*r = saturate_clip((1.164*(l1 - 16) + 1.596 * (v -128)));

			l2 = *(pY + 1);  
			*(b + 3) = saturate_clip((1.164*(l2 - 16) + 2.017 * (u - 128)));
			*(g + 3) = saturate_clip((1.164*(l2 - 16) - 0.813 * (v - 128)-0.392 * (u - 128)));
			*(r + 3) = saturate_clip((1.164*(l2 - 16) + 1.596 * (v -128)));

			l3 = *(pY + src_w);	
			*(b + dst_w * 3) = saturate_clip((1.164*(l3 - 16) + 2.017 * (u - 128)));
			*(g + dst_w * 3) = saturate_clip((1.164*(l3 - 16) - 0.813 * (v - 128)-0.392 * (u - 128)));
			*(r + dst_w * 3) = saturate_clip((1.164*(l3 - 16) + 1.596 * (v -128)));

			l4 = *(pY + src_w + 1);
			*(b + dst_w * 3 + 3) = saturate_clip((1.164*(l4 - 16) + 2.017 * (u - 128)));
			*(g + dst_w * 3 + 3) = saturate_clip((1.164*(l4 - 16) - 0.813 * (v - 128)-0.392 * (u - 128)));
			*(r + dst_w * 3 + 3) = saturate_clip((1.164*(l4 - 16) + 1.596 * (v -128)));   
		}

		void I4202Bgr(char* pInput, char* pBgr, char* pSrcDev, char* pDstDev, int src_w, int src_h, int offset_x, int offset_y, int dst_w, int dst_h)
		{    
			assert(dst_w + offset_x <= src_w);
			assert(dst_h + offset_y <= src_h);

			dim3 blocks((dst_w/64)/2, (dst_h/2)/2);
			dim3 threads(64, 2);

			cudaError_t ret = cudaMemcpy(pSrcDev, pInput, src_w * src_h * 3 / 2, cudaMemcpyHostToDevice);
			//printf("%d reason__:%s\n",ret,cudaGetErrorString(ret));
			kernel_i4202bgr<<<blocks, threads>>>(pSrcDev, pDstDev, src_w, src_h, offset_x, offset_y, dst_w, dst_h);
			cudaMemcpy(pBgr, pDstDev, dst_w * dst_h * 3, cudaMemcpyDeviceToHost);
		}
	}
}

#endif // ENABLE_AIRBORNE