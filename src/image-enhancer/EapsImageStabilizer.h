#ifndef STABILIZER_H
#define STABILIZER_H
#include "EapsImageWorkerThread.h"
#include "EapsCommon.h"

#include <iostream>
#include <memory>

namespace eap {
	namespace sma {
		namespace VideoStabilizer {
			class StabilizerBase
			{
			public:
				virtual ~StabilizerBase() {};
				virtual bool run(cv::cuda::GpuMat& inout, int64_t& inout_pts, cv::Mat& M) = 0;
				virtual bool getStabilizedFrame(cv::cuda::GpuMat& inout, int64_t& inout_pts, cv::Mat& M) = 0;
				virtual bool reset() = 0;

			protected:
				virtual bool postProcessFrame(void* const input) = 0;
				virtual void stabilize(cv::cuda::GpuMat in, int64_t in_pts) = 0;
			};

			using StabilizerPtr = std::shared_ptr<StabilizerBase>;

			//Stabilizer Contructor for TX1 Method
			StabilizerPtr createTX1Stabilizer(int width, int height);
		}   //namespace VideoStabilizer
	}
}
#endif // STABILIZER_H
