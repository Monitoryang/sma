#include "EapsImageStabilizer.h"

#include <deque>
#include <iostream>
#include <thread>

#include <driver_types.h>
#include <cuda_runtime_api.h>
#include <cuda_device_runtime_api.h>

#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/core/cuda_stream_accessor.hpp>

using namespace cv;
using namespace cv::cuda;
using namespace std;

#define PrintInfo(x)  //std::cout<<x<<std::endl

namespace eap {
	namespace sma {
		namespace VideoStabilizer {

			bool StabilizerBase::postProcessFrame(void* input)
			{
				//trim frame

				//int dx = static_cast<int>(floor(trimRatio_ * frame.cols));
				//int dy = static_cast<int>(floor(trimRatio_ * frame.rows));

				return true;
			}

			// method 1 in TX1_Stabilizer
			namespace {
#define MOTION_FILTER_LENGTH 5   //TX1 default 17 when fps is 30
#define OUTPUTFRAMEINDEX MOTION_FILTER_LENGTH/2

#define MAX_CORNERS 20
#define QUALITY_LEVEL 0.03
#define MIN_DISTANCE 80
#define USE_HARRIS_FLAG false
#define BLOCK_SIZE      3
#define HARRIS_K        0.04
#define BORDER_DENOMINATOR (8)

				typedef struct Corners
				{
					vector<Point2f> corners;
					int corner_count;
				} Corners;

				struct Motion {
					Motion operator +(const Motion another) {
						return Motion{ scale + another.scale, angle + another.angle, tx + another.tx, ty + another.ty };
					};

					Motion operator -(const Motion another) {
						return Motion{ scale - another.scale, angle - another.angle, tx - another.tx, ty - another.ty };
					};

					Motion operator /(const float d) {
						return Motion{ scale / d, angle / d, tx / d, ty / d };
					};

					void operator ()(float s_, float angle_, float tx_, float ty_) {
						scale = s_, angle = angle_, tx = tx_, ty = ty_;
					};

					float scale;
					float angle;
					float tx;
					float ty;
				};

				struct FrameInfo
				{
					FrameInfo(cv::cuda::GpuMat& in, int64_t& in_pts, int width, int height) {
						//\!cautions: After constructing GpuMat with a pre-allocate cuda memory,(such as cudaMalloc)
						//            using GpuMat.release() does not free the cuda memory,
						//            You must get this cuda memory ptr and use cudaFree to free this memory
						//_input_image = cv::cuda::GpuMat(height, width, CV_8UC4, image->bgr32_image.data);
						_input_image = in;

						_pts = in_pts;

						motion_est(0, 0, 0, 0);
						motion_comp(0, 0, 0, 0);
						motion_integrated(0, 0, 0, 0);
					}

					~FrameInfo()
					{
						if (!_input_image.empty()) {
							//void *data = _input_image.data;
							_input_image.release();
							//cudaFree(data);
						}
						if (!_stabilized_image.empty()) {
							void* data = _input_image.data;
							_stabilized_image.release();
							cudaFree(data);
						}
						cudaDeviceSynchronize();
					}

					CodecImagePtr _codec_image{};

					size_t pitch;

					cv::cuda::GpuMat _input_image;
					cv::cuda::GpuMat _stabilized_image;

					cv::Mat _M;

					int64_t _pts{};

					Motion motion_est;    // motion between i-1th and ith so the first is 0,0,0,0
					Motion motion_comp;
					Motion motion_integrated;  // Integrated motion among windows
				};

				class TX1_Stabilizer : public StabilizerBase {
				public:
					TX1_Stabilizer(int width, int height);
					~TX1_Stabilizer();

				public:
					void setRadius(const int val) { _radius = val; }
					int getRadius() const { return _radius; }

					void setTrimRatio(float val) { _trim_ratio = val; }
					float getTrimRatio() const { return _trim_ratio; }

					virtual bool run(cv::cuda::GpuMat& inout, int64_t& inout_pts, cv::Mat& M) override;
					virtual bool getStabilizedFrame(cv::cuda::GpuMat& inout, int64_t& inout_pts, cv::Mat& M) override;
					virtual bool reset() override;

				private:
					virtual void stabilize(cv::cuda::GpuMat in, int64_t in_pts) override;
					virtual bool postProcessFrame(void* input) override;

					bool featureReuseJudge(int w, int h);
					void print_corners(vector<Point2f>& harris);
					void print_track(vector<Point2f>& harris, vector<uchar>& status);
					void drawCorners(Mat& img, vector<Point2f>& harris, vector<uchar>& status);
					void adjustCorners(vector<Point2f>& Corners, int w, int h);
					void goodFeaturesToTrack(GpuMat& image, OutputArray corners);
					void calcOpticalFlowPyrLK(GpuMat& prevImg, GpuMat& nextImg, InputArray prevPts, OutputArray nextPts, OutputArray status, OutputArray err);
					void recalcSimilarity(int match_count, double* a, double* b, double* tx, double* ty, int w, int h);
					void recalcTranslation(int match_count, double* tx, double* ty);
					int  motionProcess();
					void ransac(FrameInfo* frameInfo, int w, int h);
					int  flowTrack(GpuMat& Gray, GpuMat& PreGray, vector<uchar>& status, vector<float>& err);

				private:
					Ptr<cuda::CornersDetector> _detector;
					Ptr<cuda::SparsePyrLKOpticalFlow> _optical_flow_estimator;
					cv::cuda::GpuMat _pre_pts;
					cv::cuda::Stream _cv_stream_wraper;

					int _init_success_flag;
					int _idx;
					Corners _corners[2];
					int _rematch_array[MAX_CORNERS];

					Motion _motion_sigma;
					GpuMat _gray_mat;
					GpuMat _pre_gray_mat;
					GpuMat _gray_input;
					GpuMat _bgr_input;

				private:
					std::deque<std::shared_ptr<FrameInfo>> _deque_stabilizing;
					std::shared_ptr<FrameInfo> _new_frame;
					std::mutex _mtx;

					WorkerThread _stablized_thread{};

					int _radius = OUTPUTFRAMEINDEX;
					float _trim_ratio = 0.02;
					int _width{}, _height{};

				public://testing
					//\write images InTheDeque
					void writeAllImageinDeque();
					unsigned int _frame_count = 0;
					//Just for function testing
					GpuMat getStabilizedMat();
					GpuMat getGrayGpuMat() { return _gray_mat; }
				};

				TX1_Stabilizer::TX1_Stabilizer(int width, int height)
				{
					_width = width, _height = height;
					_init_success_flag = 0;
					_idx = 0;
					_corners[0].corner_count = 0;
					_corners[1].corner_count = 0;

					memset(_rematch_array, -1, MAX_CORNERS);

					_motion_sigma(0, 0, 0, 0);
					_detector = createGoodFeaturesToTrackDetector(CV_8UC1, MAX_CORNERS, QUALITY_LEVEL, MIN_DISTANCE, BLOCK_SIZE, USE_HARRIS_FLAG, HARRIS_K);
					_optical_flow_estimator = _optical_flow_estimator->create(Size(80, 80), 3, 20, false);

					_gray_input = cv::cuda::GpuMat(_height, _width, CV_8UC1);
					//_gray_mat=cv::cuda::GpuMat(_height,_width,CV_8UC1);
					_pre_gray_mat = cv::cuda::GpuMat(_height, _width, CV_8UC1);

					cudaStream_t streamNonBlocking;
					cudaStreamCreateWithFlags(&streamNonBlocking, cudaStreamNonBlocking);
					_cv_stream_wraper = cv::cuda::StreamAccessor::wrapStream(streamNonBlocking);
				}

				TX1_Stabilizer::~TX1_Stabilizer()
				{
					_gray_mat.release();
					_bgr_input.release();
					_gray_input.release();
					_pre_gray_mat.release();
					_deque_stabilizing.clear();
					_new_frame.reset();
					_cv_stream_wraper.~Stream();
				}

				void TX1_Stabilizer::stabilize(cv::cuda::GpuMat in, int64_t in_pts)
				{
					try {
						std::unique_lock<std::mutex> lock(_mtx);

						_new_frame = std::make_shared<FrameInfo>(in, in_pts, _width, _height);

						_bgr_input = _new_frame->_input_image;

						_deque_stabilizing.push_back(_new_frame);

						_gray_input.copyTo(_pre_gray_mat, _cv_stream_wraper);

						cv::cuda::cvtColor(_bgr_input, _gray_input, COLOR_BGRA2GRAY, 0, _cv_stream_wraper);

						_cv_stream_wraper.waitForCompletion();

						vector<uchar> status;
						vector<float> err;
						vector<Point2f> harris;
						Mat mask;
						int w = _bgr_input.cols;
						int h = _bgr_input.rows;
						GpuMat in_gpu_mat;

						if (_init_success_flag != 0) {
							flowTrack(_gray_input, _pre_gray_mat, status, err);

							if (!featureReuseJudge(w, h)) {
								in_gpu_mat = _gray_input(Rect(w / BORDER_DENOMINATOR, h / BORDER_DENOMINATOR,
									w * (BORDER_DENOMINATOR - 2) / BORDER_DENOMINATOR, h * (BORDER_DENOMINATOR - 2) / BORDER_DENOMINATOR));

								goodFeaturesToTrack(in_gpu_mat, harris);

								if (!harris.empty()) {
									adjustCorners(harris, w, h);
								}
								else {
									harris = _corners[_idx].corners;
								}
							}
							else {
								harris = _corners[_idx].corners;
							}
						}
						else {
							in_gpu_mat = _gray_input(Rect(w / BORDER_DENOMINATOR, h / BORDER_DENOMINATOR,
								w * (BORDER_DENOMINATOR - 2) / BORDER_DENOMINATOR, h * (BORDER_DENOMINATOR - 2) / BORDER_DENOMINATOR));

							goodFeaturesToTrack(in_gpu_mat, harris);

							if (MAX_CORNERS <= harris.size()) {
								adjustCorners(harris, w, h);
								_init_success_flag = 1;
							}

							motionProcess();

							_idx = 0;
						}

						_corners[_idx].corner_count = harris.size();
						_corners[_idx].corners = harris;
						_idx += 1;
						_idx &= 1;

						in_gpu_mat.release();
					}
					catch (const std::exception& e) {
						auto err = e.what();
						std::cout << e.what() << std::endl;
					}
				}

				bool TX1_Stabilizer::postProcessFrame(void* frame)
				{
					return StabilizerBase::postProcessFrame(frame);
				}

				bool TX1_Stabilizer::getStabilizedFrame(cv::cuda::GpuMat& inout, int64_t& inout_pts, cv::Mat& M)
				{
					std::unique_lock<std::mutex> lock(_mtx);
					try {
						//testing
						_frame_count++;
						if (_frame_count % 300 == 25) {
							//this->writeAllImageinDeque();
						}

						if (_deque_stabilizing.empty()) {
							//PrintInfo("GetStabilizedFrameSource()::  ");
							//dst=nullptr;
							return false;
						}

						if (_deque_stabilizing.size() <= MOTION_FILTER_LENGTH) {
							//PrintInfo("GetStabilizedFrameSource()::  ");
							//dst=_deque_stabilizing.back()->_input_image.data;
							return false;
						}
						else if (_deque_stabilizing.size() == (MOTION_FILTER_LENGTH + 1)) {
							//pop front first element in the deque
							_deque_stabilizing.pop_front();

							auto& frame_info = _deque_stabilizing[_radius - 1];
							auto& output = frame_info->_stabilized_image;
							// copy temp to dst
							//cudaMemcpy2D(out->bgr32_image.data, output.cols * 4, output.data, output.cols * 4, output.cols * 4, output.rows, cudaMemcpyDeviceToDevice);
							//cudaDeviceSynchronize();
							//output.release();
							//cudaDeviceSynchronize();

							output.copyTo(inout, _cv_stream_wraper);

							M = frame_info->_M;
							inout_pts = frame_info->_pts;

							_cv_stream_wraper.waitForCompletion();

							return true;
						}
						else if (_deque_stabilizing.size() >= (MOTION_FILTER_LENGTH + 1)) { // Process some wrong situation 
							PrintInfo("TX1_Stabilizer::GetStabilizedFrameSource()::deque Size is not right " << _deque_stabilizing.size() + 1);

							while (_deque_stabilizing.size() >= (MOTION_FILTER_LENGTH + 1))
								_deque_stabilizing.pop_front();

							auto& frame_info = _deque_stabilizing[_radius - 1];
							auto& output = frame_info->_stabilized_image;
							//copy temp to dst
							//cudaMemcpy2D(out->bgr32_image.data, output.cols * 4, output.data, output.cols * 4, output.cols * 4, output.rows, cudaMemcpyDeviceToDevice);
							//cudaDeviceSynchronize();
							//output.release();
							//cudaDeviceSynchronize();

							output.copyTo(inout, _cv_stream_wraper);

							M = frame_info->_M;
							inout_pts = frame_info->_pts;

							_cv_stream_wraper.waitForCompletion();

							return true;
						}
					}
					catch (std::exception& e) {
						PrintInfo(e.what());

						return false;
					}

					return false;
				}

				bool TX1_Stabilizer::reset() {
					try {
						if (_init_success_flag == 1) {
							_init_success_flag = 0;

							_idx = 0;

							_corners[0].corner_count = 0;
							_corners[1].corner_count = 0;

							memset(_rematch_array, -1, MAX_CORNERS);

							_motion_sigma(0, 0, 0, 0);

							std::unique_lock<std::mutex> lock(_mtx);
							_deque_stabilizing.clear();

							if (!_gray_mat.empty()) _gray_mat.release();
							if (!_pre_gray_mat.empty()) _pre_gray_mat.release();

							PrintInfo("Stabilzer::reset successfully");
						}
					}
					catch (std::exception& e) {
						PrintInfo("TX1_Stabilizer::reset()::" << e.what());
						return false;
					}

					return true;
				}

				GpuMat TX1_Stabilizer::getStabilizedMat()
				{
					std::unique_lock<std::mutex> lock(_mtx);
					if (_deque_stabilizing.size() < MOTION_FILTER_LENGTH) {
						if (_deque_stabilizing.empty()) {
							return GpuMat();
						}
						return _deque_stabilizing.back()->_input_image;
					}
					else if (_deque_stabilizing[OUTPUTFRAMEINDEX]->_stabilized_image.empty()) {
						return _deque_stabilizing.back()->_input_image;
					}
					else {
						return _deque_stabilizing[OUTPUTFRAMEINDEX]->_stabilized_image;
					}
				}

				bool TX1_Stabilizer::run(cv::cuda::GpuMat& inout, int64_t& inout_pts, cv::Mat& M)
				{
					try {
						//void* pCudaMemory;
						//size_t pitch;
						//cudaMallocPitch(&pCudaMemory, &pitch, _width * sizeof(unsigned char) * 4, _height);
						//cudaMemoryCpy IOImagePtr to pCacheCudaMemory
						//cudaMemcpy2D(pCudaMemory, pitch, inout->bgr32_image.data, pitch, _width * 4, _height, cudaMemcpyDeviceToDevice);
						//cudaDeviceSynchronize();

						cv::cuda::GpuMat in{};

						inout.copyTo(in, _cv_stream_wraper);

						/*in->meta_data_basic = inout->meta_data_basic;
						in->meta_data_raw_binary = inout->meta_data_raw_binary;
						in->meta_data_valid = inout->meta_data_valid;
						in->pixel_lines = inout->pixel_lines;
						in->pixel_points = inout->pixel_points;
						in->pts = inout->pts;*/

						//\ wait for last task finish
						_stablized_thread.wait();

						_cv_stream_wraper.waitForCompletion();

						int64_t in_pts = inout_pts;
						int64_t out_pts{};

						//\TODO: USE STREAM Sychronize here
						auto is_geted = this->getStabilizedFrame(inout, out_pts, M);

						inout_pts = out_pts;

						//\ syc version
						//this->Stabilize(pCudaMemory);

						//\ Asyc version
						std::function<void()> stabilizedFunction = std::bind(&TX1_Stabilizer::stabilize, this, in, in_pts);
						_stablized_thread.addJob(stabilizedFunction);

						return is_geted;
					}
					catch (std::exception& ex) {
						std::string exc = ex.what();
						PrintInfo("std::exception TX1_Stabilizer::Run: " << ex.what());

						return false;
					}
				}

				bool TX1_Stabilizer::featureReuseJudge(int w, int h)
				{
					Corners* curr_corners = &_corners[_idx];
					int len = curr_corners->corner_count;
					int i, j;
					int upx = w / (BORDER_DENOMINATOR << 1);
					int upy = h / (BORDER_DENOMINATOR << 1);
					int downx = w * ((BORDER_DENOMINATOR << 1) - 1) / (BORDER_DENOMINATOR << 1);
					int downy = h * ((BORDER_DENOMINATOR << 1) - 1) / (BORDER_DENOMINATOR << 1);

					for (i = 0, j = 0; i < len; i++) {
						if ((_rematch_array[i] != -1)
							&& (curr_corners->corners[i].x >= upx)
							&& (curr_corners->corners[i].x < downx)
							&& (curr_corners->corners[i].y >= upy)
							&& (curr_corners->corners[i].y < downy)) {
							curr_corners->corners[j] = curr_corners->corners[i];
							j++;
						}
					}

					if (j < MAX_CORNERS / 2) {
						return false;
					}

					for (i = j; i < len; i++) {
						curr_corners->corners[i].x = -1;
						curr_corners->corners[i].y = -1;
					}
					curr_corners->corner_count = j;

					return true;
				}

				void TX1_Stabilizer::print_corners(vector<Point2f>& harris)
				{
#if 0
					int len = harris.size();

					for (int i = 0; i < len; i++) {
						printf("%2d: x:%f  y:%f\n", i, harris[i].x, harris[i].y);
					}
#endif
				}

				void TX1_Stabilizer::print_track(vector<Point2f>& harris, vector<uchar>& status)
				{
#if 0
					int len = harris.size();

					for (int i = 0; i < len; i++) {
						printf("**%2d: x:%f  y:%f\n", i, harris[i].x, harris[i].y);
						if (status[i] != 1) {
							printf("idx:%d error\n", i);
						}
					}
#endif
				}

				void TX1_Stabilizer::drawCorners(Mat& img, vector<Point2f>& harris, vector<uchar>& status)
				{
					int i;
					cv::Point2f center;
					char str[10];

					int len = harris.size();
					for (i = 0; i < len; i++) {
						center.x = harris[i].x;
						center.y = harris[i].y;
						circle(img, center, 2, ((status[i] != -1) ? CV_RGB(255, 255, 255) : CV_RGB(0, 0, 0)), 2);
						//sprintf(str, "%d", i);
						putText(img, str, center, cv::FONT_HERSHEY_COMPLEX, 1, CV_RGB(255, 255, 255));
					}
				}

				void TX1_Stabilizer::adjustCorners(vector<Point2f>& Corners, int w, int h)
				{
					int i;
					int len = Corners.size();
					float deltax = w / BORDER_DENOMINATOR;
					float deltay = h / BORDER_DENOMINATOR;

					for (i = 0; i < len; i++) {
						Corners[i].x += deltax;
						Corners[i].y += deltay;
					}
				}

				void TX1_Stabilizer::goodFeaturesToTrack(GpuMat& image, OutputArray corners)
				{
					GpuMat gpu_dst;
					_detector->detect(image, gpu_dst, noArray(), _cv_stream_wraper);

					Mat pts;
					if (gpu_dst.empty()) {
						PrintInfo("TX1_Stabilizer:: goodFeaturesToTrack error");
						cv::Mat im;
						image.download(im);
						cv::imwrite("wrong_image.png ", im);
						//throw runtime_error("image is wrong please check the image file");

						//TODO: solve this problem
					}
					gpu_dst.download(pts, _cv_stream_wraper);
					//USE STREAM Sychronize here
					_cv_stream_wraper.waitForCompletion();

					if (NULL != pts.data) {
						pts.copyTo(corners);
					}
				}

				void TX1_Stabilizer::calcOpticalFlowPyrLK(GpuMat& prevImg, GpuMat& nextImg, InputArray prevPts, OutputArray nextPts, OutputArray status, OutputArray err)
				{
					Mat in_pts = prevPts.getMat();

					_pre_pts.upload(in_pts, _cv_stream_wraper);

					GpuMat gpu_next_pts;
					GpuMat gpu_status;

					_optical_flow_estimator->calc(prevImg, nextImg, _pre_pts, gpu_next_pts, gpu_status, noArray(), _cv_stream_wraper);

					Mat pts, stat;
					gpu_next_pts.download(pts, _cv_stream_wraper);
					gpu_status.download(stat, _cv_stream_wraper);

					_cv_stream_wraper.waitForCompletion();

					pts.copyTo(nextPts);
				}

				void TX1_Stabilizer::recalcSimilarity(int match_count, double* a, double* b, double* tx, double* ty, int w, int h)
				{
					int i;
					int count;
					Corners* curr_corners = &_corners[_idx];
					Corners* prev_corners = &_corners[(_idx + 1) & 1];

					count = 0;
					for (i = 0; i < match_count; i++) {
						if (_rematch_array[i] != -1) {
							count++;
						}
					}

					Mat A = Mat::zeros(count * 2, 4, CV_32FC1);
					Mat B(count * 2, 1, CV_32FC1);

					count = 0;
					for (i = 0; i < match_count; i++) {
						if (_rematch_array[i] != -1) {
							A.at<float>(count * 2, 0) = prev_corners->corners[i].x - w / 2;
							A.at<float>(count * 2, 1) = -prev_corners->corners[i].y + h / 2;
							A.at<float>(count * 2, 2) = 1;
							A.at<float>(count * 2 + 1, 0) = prev_corners->corners[i].y - h / 2;
							A.at<float>(count * 2 + 1, 1) = prev_corners->corners[i].x - w / 2;
							A.at<float>(count * 2 + 1, 3) = 1;
							B.at<float>(count * 2, 0) = curr_corners->corners[i].x - w / 2;
							B.at<float>(count * 2 + 1, 0) = curr_corners->corners[i].y - h / 2;
							count++;
						}
					}

					//add by rui

					if (A.empty() || B.empty()) {
						*a = 0;
						*b = 0;
						*tx = 0;
						*ty = 0;
						return;
					}

					Mat C = A.t() * A;
					Mat P = A.t() * B;
					Mat Q = C.inv() * P;

					*a = Q.at<float>(0, 0);
					*b = Q.at<float>(1, 0);
					*tx = Q.at<float>(2, 0);
					*ty = Q.at<float>(3, 0);
				}

				void TX1_Stabilizer::recalcTranslation(int match_count, double* tx, double* ty)
				{
					int i;
					int count;

					Corners* curr_corners = &_corners[_idx];
					Corners* prev_corners = &_corners[(_idx + 1) & 1];

					count = 0;
					*tx = 0;
					*ty = 0;
					for (i = 0; i < match_count; i++) {
						if (_rematch_array[i] != -1) {
							*tx = (*tx) + curr_corners->corners[i].x - prev_corners->corners[i].x;
							*ty = (*ty) + curr_corners->corners[i].y - prev_corners->corners[i].y;
							count++;
						}
					}
					*tx = (*tx) / count;
					*ty = (*ty) / count;
				}

				int TX1_Stabilizer::motionProcess()
				{
					auto pFrameInfo = _deque_stabilizing.back();
					_motion_sigma = _motion_sigma + pFrameInfo->motion_est;
					pFrameInfo->motion_integrated = _motion_sigma;

					if (_deque_stabilizing.size() == (MOTION_FILTER_LENGTH + 1)) {
						////-----it should not be done here,
						////----- we must make that all InterOperation between GL and CUDA  Work in the same Thread where d3d11 render operation is executed
						////pop_front deque first elem
						//_deque_stabilizing.pop_front();

						Motion motion_Intergrated_Mean = { 0,0,0,0 };
						bool flag = false;
						for (auto& d1 : _deque_stabilizing) {
							if (!flag) {
								flag = true;
								continue;
							}
							motion_Intergrated_Mean = motion_Intergrated_Mean + d1->motion_integrated;  //average every frame's Integrated Motion
						}

						motion_Intergrated_Mean = motion_Intergrated_Mean / (double)(MOTION_FILTER_LENGTH);

						pFrameInfo = _deque_stabilizing[OUTPUTFRAMEINDEX];
						pFrameInfo->motion_comp = pFrameInfo->motion_integrated - motion_Intergrated_Mean;

						//restrict value of comp parameters
						auto trimRatio = getTrimRatio();
						auto maxX = 0.8f * trimRatio * _width;
						auto maxY = 0.8f * trimRatio * _height;
						auto absMaxX = abs(pFrameInfo->motion_comp.tx);
						auto absMaxY = abs(pFrameInfo->motion_comp.ty);
						bool isXOutofRange = absMaxX > maxX;
						bool isYOutofRange = absMaxY > maxY;

						if (isXOutofRange && (absMaxX > absMaxY)) {
							PrintInfo("tx: " << pFrameInfo->motion_comp.tx << " ty: " << pFrameInfo->motion_comp.ty);
							pFrameInfo->motion_comp.ty = pFrameInfo->motion_comp.ty * maxX / absMaxX;
							pFrameInfo->motion_comp.tx = maxX * pFrameInfo->motion_comp.tx / absMaxX;
							PrintInfo("XoutofRange");
						}

						if (isYOutofRange && (absMaxX < absMaxY)) {
							PrintInfo("tx: " << pFrameInfo->motion_comp.tx << " ty: " << pFrameInfo->motion_comp.ty);
							pFrameInfo->motion_comp.tx = pFrameInfo->motion_comp.tx * maxY / absMaxY;
							pFrameInfo->motion_comp.ty = maxY * pFrameInfo->motion_comp.ty / absMaxY;
							PrintInfo("YoutofRange");
						}

						{
							//std::cout << "test_IMP Motion Process: motion_comp tx: " << pFrameInfo->motion_comp.tx << " ty:"
							//          << pFrameInfo->motion_comp.ty << std::endl;
							//std::cout.flush();
						}

						//perspective transform
						Mat similar(3, 3, CV_32FC1);
						float a = 1;
						float b = 0;
						similar.at<float>(0, 0) = a;
						similar.at<float>(0, 1) = -b;
						similar.at<float>(0, 2) = -pFrameInfo->motion_comp.tx + (1 - a) * _width / 2 + b * _height / 2;
						similar.at<float>(1, 0) = b;
						similar.at<float>(1, 1) = a;
						similar.at<float>(1, 2) = -pFrameInfo->motion_comp.ty - b * _width / 2 + (1 - a) * _height / 2;
						similar.at<float>(2, 0) = 0;
						similar.at<float>(2, 1) = 0;
						similar.at<float>(2, 2) = 1;

						cv::cuda::warpPerspective(pFrameInfo->_input_image, pFrameInfo->_stabilized_image, similar, pFrameInfo->_input_image.size(),
							INTER_LINEAR, cv::BorderTypes::BORDER_REPLICATE, Scalar(), _cv_stream_wraper);

						pFrameInfo->_M = similar;

						//\USE STREAM Sychronize here
						_cv_stream_wraper.waitForCompletion();
					}

					//PrintInfo("motionProcess() successfully deque size"<<_deque_stabilizing.size());
					return 0;
				}

				void TX1_Stabilizer::ransac(FrameInfo* frameInfo, int w, int h)
				{
					Corners* curr_corners = &_corners[_idx];
					Corners* prev_corners = &_corners[(_idx + 1) & 1];

					Mat mask;
					float dst_point[MAX_CORNERS * 2];
					float src_point[MAX_CORNERS * 2];
					double tx, ty, a, b;
					int i;

					if (prev_corners->corner_count >= 4) {
						for (i = 0; i < prev_corners->corner_count; i++) {
							dst_point[2 * i] = curr_corners->corners[i].x - w / 2;
							dst_point[2 * i + 1] = curr_corners->corners[i].y - h / 2;
							src_point[2 * i] = prev_corners->corners[i].x - w / 2;
							src_point[2 * i + 1] = prev_corners->corners[i].y - h / 2;
						}
						Mat src(prev_corners->corner_count, 2, CV_32FC1, src_point);
						Mat dst(curr_corners->corner_count, 2, CV_32FC1, dst_point);

						Mat H = findHomography(src, dst, mask, RANSAC, 5);
						memset(_rematch_array, -1, MAX_CORNERS);
						for (i = 0; i < prev_corners->corner_count; i++) {
							if (mask.at<uchar>(i, 0) != 0) {
								_rematch_array[i] = i;
							}
						}

						recalcSimilarity(prev_corners->corner_count, &a, &b, &tx, &ty, w, h);

						frameInfo->motion_est.scale = sqrt(a * a + b * b);
						frameInfo->motion_est.angle = atan(b / a) / 2;
						frameInfo->motion_est.tx = tx;
						frameInfo->motion_est.ty = ty;

					}
					else {
						frameInfo->motion_est.scale = 0;
						frameInfo->motion_est.angle = 0;
						frameInfo->motion_est.tx = 0;
						frameInfo->motion_est.ty = 0;

						for (i = 0; i < prev_corners->corner_count; i++) {
							_rematch_array[i] = -1;
						}

						printf("ransac failed\n");
					}
				}

				int TX1_Stabilizer::flowTrack(GpuMat& Gray, GpuMat& PreGray, vector<uchar>& status, vector<float>& err)
				{
					vector<Point2f> flow_track;

					Corners* curr_corners = &_corners[_idx];
					Corners* prev_corners = &_corners[(_idx + 1) & 1];

					/*1. find matched feature points*/
					calcOpticalFlowPyrLK(PreGray, Gray, prev_corners->corners, flow_track, status, err);
					curr_corners->corner_count = flow_track.size();
					curr_corners->corners = flow_track;

					/*2. ransac process*/
					auto f = _deque_stabilizing.back().get();
					ransac(f, Gray.cols, Gray.rows);

					/*3. motion vector filter, compensation*/
					int ret = motionProcess();

					return ret;
				}

				//testing
				void TX1_Stabilizer::writeAllImageinDeque()
				{
					for (int i = 0; i < OUTPUTFRAMEINDEX; ++i) {
						cv::Mat m1;
						_deque_stabilizing[i]->_stabilized_image.download(m1);
						cv::resize(m1, m1, Size(720, 405));
						std::string  s1 = "output/" + to_string(_frame_count) + "_" + to_string(i) + ".png";
						cv::imwrite(s1, m1);
					}
				}
			}

			StabilizerPtr createTX1Stabilizer(int width, int height)
			{
				return std::make_shared<TX1_Stabilizer>(width, height);
			}
		}   //namespace VideoStabilizer
	}
}


