#ifndef EAPST_FFMPEG_WRAP_H
#define EAPST_FFMPEG_WRAP_H

//#include "EapsMetaDataStructure.h"
#include "jo_meta_data_structure.h"
#include "EapsCommon.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#include <libavformat/version.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/log.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#include <opencv2/core.hpp>
#endif
#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <queue>

namespace eap {
	namespace sma {
		class Packet
		{
		public:
			inline Packet()
			{
				_meta_data_basic = new JoFmvMetaDataBasic();
#ifdef ENABLE_AIRBORNE
				_meta_data_qianjue = new FRAME_POS_Qianjue();
#endif
			}

			inline Packet(AVPacket* p)
			{
				_meta_data_basic = new JoFmvMetaDataBasic();
#ifdef ENABLE_AIRBORNE
				_meta_data_qianjue = new FRAME_POS_Qianjue();
#endif
				_ptr = std::shared_ptr<AVPacket>(p, [](AVPacket* packet)
				{
					if (packet) {
						av_packet_free(&packet);
						packet = nullptr;
					}
				});
				_native_ptr = p;
			}

			inline Packet(const Packet& other)
			{
				_meta_data_basic = new JoFmvMetaDataBasic();

				_ptr = other._ptr;
				_native_ptr = other._ptr.get();

				_meta_data_valid = other._meta_data_valid;
				if(other._meta_data_basic){
					*_meta_data_basic = *other._meta_data_basic;
				}
#ifdef ENABLE_AIRBORNE
				_meta_data_qianjue = new FRAME_POS_Qianjue();
				if(other._meta_data_qianjue){
					*_meta_data_qianjue = *other._meta_data_qianjue;
				}
#endif
				_sei_buf = other._sei_buf; // 偶现崩溃
				_ar_infos = other._ar_infos;
				_ai_heatmap_infos = other._ai_heatmap_infos;

				_ar_vector_file = other._ar_vector_file;
				_ar_valid_point_index = other._ar_valid_point_index;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				_ar_pixel_points = other._ar_pixel_points;
				_ar_pixel_lines = other._ar_pixel_lines;
				_ar_pixel_warningL1s = other._ar_pixel_warningL1s;
				_ar_pixel_warningL2s = other._ar_pixel_warningL2s;
#endif
				_current_time = other._current_time;
				_original_pts = other._original_pts;
				_bit_rate = other._bit_rate;
				_frame_rate = other._frame_rate;
				_meta_data_json = other._meta_data_json;
			}

			inline Packet(Packet&& other)
			{
				_ptr = other._ptr;
				other._ptr.reset();

				_native_ptr = _ptr.get();

				_meta_data_valid = other._meta_data_valid;
				if(other._meta_data_basic){
					_meta_data_basic = other._meta_data_basic;
					other._meta_data_basic = nullptr;
				}
#ifdef ENABLE_AIRBORNE
				if(other._meta_data_qianjue){
					_meta_data_qianjue = other._meta_data_qianjue;
					other._meta_data_qianjue = nullptr;
				}
#endif
				_ar_infos = other._ar_infos;
				_ai_heatmap_infos = other._ai_heatmap_infos;

				_sei_buf = std::move(other._sei_buf);

				_ar_vector_file = std::move(other._ar_vector_file);
				_ar_valid_point_index = std::move(other._ar_valid_point_index);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				_ar_pixel_points = std::move(other._ar_pixel_points);
				_ar_pixel_lines = std::move(other._ar_pixel_lines);
				_ar_pixel_warningL1s = std::move(other._ar_pixel_warningL1s);
				_ar_pixel_warningL2s = std::move(other._ar_pixel_warningL2s);
#endif
				_current_time = other._current_time;
				_original_pts = other._original_pts;
				_bit_rate = other._bit_rate;
				_frame_rate = other._frame_rate;
				_meta_data_json = std::move(other._meta_data_json);
			}

			inline ~Packet() 
			{
				if(_meta_data_basic)
				{
					delete _meta_data_basic;
					_meta_data_basic = nullptr;
				}
#ifdef ENABLE_AIRBORNE
				if(_meta_data_qianjue)
				{
					delete _meta_data_qianjue;
					_meta_data_qianjue = nullptr;
				}
#endif
				_native_ptr = nullptr; 
			}

			inline Packet& operator=(AVPacket* p)
			{
				auto pt = std::shared_ptr<AVPacket>(p, [this](AVPacket* packet)
				{
					if (packet) {
						av_packet_free(&packet);
						packet = nullptr;
					}
				});
				std::swap(pt, _ptr);

				return *this;
			}

			inline Packet& operator=(const Packet& other)
			{
				_ptr = other._ptr;
				_native_ptr = _ptr.get();

				_meta_data_valid = other._meta_data_valid;
				if(nullptr != other._meta_data_basic){
					*_meta_data_basic = *other._meta_data_basic;
				}
#ifdef ENABLE_AIRBORNE
				if(nullptr != other._meta_data_qianjue){
					*_meta_data_qianjue = *other._meta_data_qianjue;
				}
#endif				
				_ar_infos = other._ar_infos;
				_ai_heatmap_infos = other._ai_heatmap_infos;

				_sei_buf = other._sei_buf;

				_ar_vector_file = other._ar_vector_file;
				_ar_valid_point_index = other._ar_valid_point_index;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				_ar_pixel_points = other._ar_pixel_points;
				_ar_pixel_lines = other._ar_pixel_lines;
				_ar_pixel_warningL1s = other._ar_pixel_warningL1s;
				_ar_pixel_warningL2s = other._ar_pixel_warningL2s;
#endif
				_current_time = other._current_time;
				_original_pts = other._original_pts;
				_bit_rate = other._bit_rate;
				_frame_rate = other._frame_rate;
				_meta_data_json = other._meta_data_json;

				return *this;
			}

			inline Packet& operator=(Packet&& other)
			{
				if (this->_ptr != other._ptr) {
					std::swap(other._ptr, _ptr);
					other._ptr.reset();
					_native_ptr = _ptr.get();

					if(_meta_data_basic)
					{
						delete _meta_data_basic;
						_meta_data_basic = nullptr;
					}

					_meta_data_valid = other._meta_data_valid;
					if(other._meta_data_basic){
						_meta_data_basic = other._meta_data_basic;
						other._meta_data_basic = nullptr;
					}
#ifdef ENABLE_AIRBORNE
					if (_meta_data_qianjue)
					{
						delete _meta_data_qianjue;
						_meta_data_qianjue = nullptr;
					}
					if(other._meta_data_qianjue){
						_meta_data_qianjue = other._meta_data_qianjue;
						other._meta_data_qianjue = nullptr;
					}
#endif
					_ar_infos = other._ar_infos;
					_ai_heatmap_infos = other._ai_heatmap_infos;

					_sei_buf = std::move(other._sei_buf);

					_ar_vector_file = std::move(other._ar_vector_file);
					_ar_valid_point_index = std::move(other._ar_valid_point_index);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
					_ar_pixel_points = std::move(other._ar_pixel_points);
					_ar_pixel_lines = std::move(other._ar_pixel_lines);
					_ar_pixel_warningL1s = std::move(other._ar_pixel_warningL1s);
					_ar_pixel_warningL2s = std::move(other._ar_pixel_warningL2s);
#endif
					_current_time = other._current_time;
					_original_pts = other._original_pts;
					_bit_rate = other._bit_rate;
					_frame_rate = other._frame_rate;
					_meta_data_json = std::move(other._meta_data_json);
				}

				return *this;
			}

			inline void copyTo(AVPacket** out)
			{
				if (out) {
					*out = av_packet_clone(_ptr.get());
				}
			}

			inline void copyTo(Packet& out)
			{
				out = av_packet_clone(_ptr.get());

				out._meta_data_valid = _meta_data_valid;
				if(_meta_data_basic){
					*out._meta_data_basic = *_meta_data_basic;
				}
#ifdef ENABLE_AIRBORNE
				if(_meta_data_qianjue){
					*out._meta_data_qianjue = *_meta_data_qianjue;
				}
#endif
				out._ar_infos = _ar_infos;
				out._sei_buf = _sei_buf;
				out._ar_vector_file = _ar_vector_file;
				out._ar_valid_point_index = _ar_valid_point_index;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				out._ar_pixel_points = _ar_pixel_points;
				out._ar_pixel_lines = _ar_pixel_lines;
				out._ar_pixel_warningL1s = _ar_pixel_warningL1s;
				out._ar_pixel_warningL2s = _ar_pixel_warningL2s;
#endif
				out._current_time = _current_time;
				out._original_pts = _original_pts;
				out._bit_rate = _bit_rate;
				out._frame_rate = _frame_rate;
				out._meta_data_json = _meta_data_json;
			}

			inline AVPacket** assign() { return &_native_ptr; }

			inline void set(AVPacket* p)
			{
				auto pt = std::shared_ptr<AVPacket>(p, [this](AVPacket* packet)
				{
					if (packet) {
						av_packet_free(&packet);
						packet = nullptr;
					}
				});

				std::swap(pt, _ptr);
				_native_ptr = _ptr.get();
			}

			inline AVPacket* get() const { return _ptr.get(); }

			inline AVPacket** operator&() { return assign(); }

			inline    operator AVPacket* () const { return _ptr.get(); }
			inline AVPacket* operator->() const { return _ptr.get(); }

			inline bool operator==(AVPacket* p) const { return _ptr.get() == p; }
			inline bool operator!=(AVPacket* p) const { return _ptr.get() != p; }

			inline bool operator!() const { return !_ptr; }

			inline void setSeiBuf(const std::vector<uint8_t>& sei_buf)
			{
				this->_sei_buf = sei_buf;
			}

			inline const std::vector<uint8_t>& getSeiBuf()
			{
				return _sei_buf;
			}

			inline bool& metaDataValid() { return _meta_data_valid; }

			inline void setMetaDataBasic(const JoFmvMetaDataBasic* meta_data)
			{
				if(meta_data)
					*_meta_data_basic = *meta_data;
			}

			inline JoFmvMetaDataBasic& getMetaDataBasic()
			{
				return *_meta_data_basic;
			}

#ifdef ENABLE_AIRBORNE
			inline void setMetaDataQianjue(const FRAME_POS_Qianjue* meta_data)
			{
				if(meta_data)
					*_meta_data_qianjue = *meta_data;
			}

			inline FRAME_POS_Qianjue& getMetaDataQianjue()
			{
				return *_meta_data_qianjue;
			}
#endif

			inline void setMetaDataJson(std::string json)
			{
				_meta_data_json = json;
			}

			inline const std::string getMetaDataJson()
			{
				return _meta_data_json;
			}
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			inline void setArPixelPoints(const std::vector<cv::Point>& pixel_points)
			{
				_ar_pixel_points = pixel_points;
			}

			inline void setArPixelLines(const std::vector<std::vector<cv::Point>>& pixel_lines) {
				_ar_pixel_lines = pixel_lines;
			}

			inline void setArPixelWarningL1s(const std::vector<std::vector<cv::Point>>& ar_pixel_warningL1s) {
				_ar_pixel_warningL1s = ar_pixel_warningL1s;
			}

			inline void setArPixelWarningL2s(const std::vector<std::vector<cv::Point>>& ar_pixel_warningL2s) {
				_ar_pixel_warningL2s = ar_pixel_warningL2s;
			}
#endif
			inline void setCurrentTime(int64_t current_time)
			{
				_current_time = current_time;
			}

			inline void setOriginalPts(int64_t original_pts)
			{
				_original_pts = original_pts;
			}

			inline void setVideoParams(int frame_rate, int bit_rate)
			{
				_bit_rate = bit_rate;
				_frame_rate = frame_rate;
			}
			inline void getVideoParams(int& frame_rate, int& bit_rate)
			{
				bit_rate = _bit_rate;
				frame_rate = _frame_rate;
			}

			inline void setArValidPointIndex(const std::queue<int>& ar_valid_point_index)
			{
				_ar_valid_point_index = ar_valid_point_index;
			}

			inline void setArVectorFile(const std::string& ar_vector_file)
			{
				_ar_vector_file = ar_vector_file;
			}

			inline void setArMarkInfos(const ArInfosInternal& ar_infos)
			{
				_ar_infos = ar_infos;
			}

			inline void setAiHeatmapInfos(const AiHeatmapInfo& ai_heatmap_infos)
			{
				_ai_heatmap_infos = ai_heatmap_infos;
			}

			inline const AiHeatmapInfo& getAiHeatmapInfos()
			{
				return _ai_heatmap_infos;
			}

			inline const ArInfosInternal& getArInfos()
			{
				return _ar_infos;
			}
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			inline const std::vector<cv::Point>& getArPixelPoints()
			{
				return _ar_pixel_points;
			}

			inline const std::vector<std::vector<cv::Point>>& getArPixelLines()
			{
				return _ar_pixel_lines;
			}
			inline const std::vector<std::vector<cv::Point>>& getArPixelWarningL1s() {
				return _ar_pixel_warningL1s;
			}

			inline const std::vector<std::vector<cv::Point>>& getArPixelWarningL2s() {
				return _ar_pixel_warningL2s;
			}
#endif

			inline const int64_t& getCurrentTime()
			{
				return _current_time;
			}

			inline const int64_t& getOriginalPts()
			{
				return _original_pts;
			}

			inline const std::queue<int>& getArValidPointIndex()
			{
				return  _ar_valid_point_index;
			}

			inline const std::string& getArVectorFile()
			{
				return  _ar_vector_file;
			}
		private:
			std::shared_ptr<AVPacket> _ptr{};
			AVPacket* _native_ptr{};
			std::vector<uint8_t> _sei_buf{};
			bool _meta_data_valid{};
			JoFmvMetaDataBasic* _meta_data_basic{};
#ifdef ENABLE_AIRBORNE
			FRAME_POS_Qianjue* _meta_data_qianjue{};
#endif
			std::string _meta_data_json{};

			ArInfosInternal _ar_infos{};
			AiHeatmapInfo _ai_heatmap_infos{};

			std::string _ar_vector_file{};
			std::queue<int> _ar_valid_point_index{};
			int _frame_rate{};
			int _bit_rate{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> _ar_pixel_points{};
			std::vector<std::vector<cv::Point>> _ar_pixel_lines{};
			std::vector<std::vector<cv::Point>> _ar_pixel_warningL1s{};
			std::vector<std::vector<cv::Point>> _ar_pixel_warningL2s{};
#endif
			int64_t _current_time{};
			int64_t _original_pts{};
		};

		class Frame
		{
		public:
			inline Frame() {}

			inline Frame(AVFrame* p)
			{
				_ptr = std::shared_ptr<AVFrame>(p, [](AVFrame* packet)
				{
					if (packet) {
						av_frame_free(&packet);
					}
				});

				_native_ptr = p;
			}

			inline Frame(const Frame& other)
			{
				_ptr = other._ptr;
				_native_ptr = other._ptr.get();

				_meta_data_valid = other._meta_data_valid;
				_meta_data_basic = other._meta_data_basic;
#ifdef ENABLE_AIRBORNE
				_meta_data_qianjue = other._meta_data_qianjue;
#endif
				_sei_buf = other._sei_buf;

				_ar_vector_file = other._ar_vector_file;
				_ar_valid_point_index = other._ar_valid_point_index;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				_ar_pixel_points = other._ar_pixel_points;
				_ar_pixel_lines = other._ar_pixel_lines;
				_ar_pixel_warningL1s = other._ar_pixel_warningL1s;
				_ar_pixel_warningL2s = other._ar_pixel_warningL2s;
#endif

				_meta_data_json = other._meta_data_json;
				_current_time = other._current_time;
				_ar_infos = other._ar_infos;
				_ai_heatmap_infos = other._ai_heatmap_infos;
			}

			inline Frame(Frame&& other)
			{
				_ptr = other._ptr;
				_native_ptr = _ptr.get();

				other._ptr.reset();

				_meta_data_valid = other._meta_data_valid;
				_meta_data_basic = other._meta_data_basic;
#ifdef ENABLE_AIRBORNE
				_meta_data_qianjue = other._meta_data_qianjue;
#endif
				_sei_buf = std::move(other._sei_buf);

				_ar_vector_file = std::move(other._ar_vector_file);
				_ar_valid_point_index = std::move(other._ar_valid_point_index);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				_ar_pixel_points = std::move(other._ar_pixel_points);
				_ar_pixel_lines = std::move(other._ar_pixel_lines);
				_ar_pixel_warningL1s = std::move(other._ar_pixel_warningL1s);
				_ar_pixel_warningL2s = std::move(other._ar_pixel_warningL2s);
#endif

				_meta_data_json = std::move(other._meta_data_json);
				_current_time = std::move(other._current_time);
				_ar_infos = std::move(other._ar_infos);
				_ai_heatmap_infos = std::move(other._ai_heatmap_infos);
			}

			inline ~Frame() { }

			inline void clear()
			{
				if (_ptr) {
					_ptr.reset();
				}
			}

			inline Frame& operator=(AVFrame* p)
			{
				auto pt = std::shared_ptr<AVFrame>(p, [this](AVFrame* packet)
				{
					if (packet) {
						av_frame_free(&packet);
						packet = nullptr;
					}
				});

				std::swap(pt, _ptr);

				return *this;
			}

			inline Frame& operator=(const Frame& other)
			{
				_ptr = other._ptr;
				_native_ptr = _ptr.get();

				_meta_data_valid = other._meta_data_valid;
				_meta_data_basic = other._meta_data_basic;
#ifdef ENABLE_AIRBORNE
				_meta_data_qianjue = other._meta_data_qianjue;
#endif
				_sei_buf = other._sei_buf;

				_ar_vector_file = other._ar_vector_file;
				_ar_valid_point_index = other._ar_valid_point_index;
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
				_ar_pixel_points = other._ar_pixel_points;
				_ar_pixel_lines = other._ar_pixel_lines;
				_ar_pixel_warningL1s = other._ar_pixel_warningL1s;
				_ar_pixel_warningL2s = other._ar_pixel_warningL2s;
#endif
				_meta_data_json = other._meta_data_json;
				_current_time = other._current_time;
				_ar_infos = other._ar_infos;
				_ai_heatmap_infos = other._ai_heatmap_infos;
				return *this;
			}

			inline Frame& operator=(Frame&& other)
			{
				if (this->_ptr != other._ptr) {
					std::swap(other._ptr, _ptr);
					other._ptr.reset();
					_native_ptr = _ptr.get();

					_meta_data_valid = other._meta_data_valid;
					_meta_data_basic = other._meta_data_basic;
#ifdef ENABLE_AIRBORNE
					_meta_data_qianjue = other._meta_data_qianjue;
#endif
					_sei_buf = std::move(other._sei_buf);

					_ar_vector_file = std::move(other._ar_vector_file);
					_ar_valid_point_index = std::move(other._ar_valid_point_index);
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
					_ar_pixel_points = std::move(other._ar_pixel_points);
					_ar_pixel_lines = std::move(other._ar_pixel_lines);
					_ar_pixel_warningL1s = std::move(other._ar_pixel_warningL1s);
					_ar_pixel_warningL2s = std::move(other._ar_pixel_warningL2s);
#endif
					_ar_infos = std::move(other._ar_infos);
					_meta_data_json = std::move(other._meta_data_json);
					_current_time = std::move(other._current_time);
					_ai_heatmap_infos = std::move(other._ai_heatmap_infos);
				}

				return *this;
			}

			inline void copyTo(AVFrame** out)
			{
				if (out) {
					*out = av_frame_clone(_ptr.get());
				}
			}

			inline AVFrame** assign() { return &_native_ptr; }
			inline void set(AVFrame* p)
			{
				auto pt = std::shared_ptr<AVFrame>(p, [this](AVFrame* packet)
				{
					if (packet) {
						av_frame_free(&packet);
						packet = nullptr;
					}
				});

				std::swap(pt, _ptr);

				_native_ptr = _ptr.get();
			}

			inline AVFrame* get() const { return _ptr.get(); }

			inline AVFrame** operator&() { return assign(); }

			inline    operator AVFrame* () const { return _ptr.get(); }
			inline AVFrame* operator->() const { return _ptr.get(); }

			inline bool operator==(AVFrame* p) const { return _ptr.get() == p; }
			inline bool operator!=(AVFrame* p) const { return _ptr.get() != p; }

			inline bool operator!() const { return !_ptr; }

			inline void setSeiBuf(const std::vector<uint8_t>& sei_buf)
			{
				this->_sei_buf = sei_buf;
			}

			inline const std::vector<uint8_t>& getSeiBuf()
			{
				return _sei_buf;
			}

			inline void setMetaDataBasic(const JoFmvMetaDataBasic& meta_data_basic)
			{
				_meta_data_basic = meta_data_basic;
			}

			inline const JoFmvMetaDataBasic& getMetaDataBasic()
			{
				return _meta_data_basic;
			}
#ifdef ENABLE_AIRBORNE
			inline void setMetaDataQianjue(const FRAME_POS_Qianjue& meta_data)
			{
				_meta_data_qianjue = meta_data;
			}

			inline const FRAME_POS_Qianjue& getMetaDataQianjue()
			{
				return _meta_data_qianjue;
			}
#endif
			inline void setMetaDataJson(std::string json)
			{
				_meta_data_json = json;
			}

			inline const std::string getMetaDataJson()
			{
				return _meta_data_json;
			}

			inline void setMetaDataValid(const bool meta_data_valid)
			{
				_meta_data_valid = meta_data_valid;
			}

			inline void setArValidPointIndex(const std::queue<int>& ar_valid_point_index)
			{
				_ar_valid_point_index = ar_valid_point_index;
			}

			inline void setArVectorFile(const std::string& ar_vector_file)
			{
				_ar_vector_file = ar_vector_file;
			}

			inline bool getMetaDataValid()
			{
				return _meta_data_valid;
			}
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			inline void setArPixelPoints(const std::vector<cv::Point>& pixel_points)
			{
				_ar_pixel_points = pixel_points;
			}

			inline void setArPixelLines(const std::vector<std::vector<cv::Point>>& pixel_lines)
			{
				_ar_pixel_lines = pixel_lines;
			}

			inline void setArPixelWarningL1s(const std::vector<std::vector<cv::Point>>& ar_pixel_warningL1s) {
				_ar_pixel_warningL1s = ar_pixel_warningL1s;
			}

			inline void setArPixelWarningL2s(const std::vector<std::vector<cv::Point>>& ar_pixel_warningL2s) {
				_ar_pixel_warningL2s = ar_pixel_warningL2s;
			}
#endif
			inline void setArMarkInfos(const ArInfosInternal& ar_infos)
			{
				_ar_infos = ar_infos;
			}

			inline void setAiHeatmapInfos(const AiHeatmapInfo& ai_heatmap_infos)
			{
				_ai_heatmap_infos = ai_heatmap_infos;
			}

			inline const AiHeatmapInfo& getAiHeatmapInfos()
			{
				return _ai_heatmap_infos;
			}
			inline const std::queue<int>& getArValidPointIndex()
			{
				return  _ar_valid_point_index;
			}
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			inline const std::vector<cv::Point>& getArPixelPoints()
			{
				return _ar_pixel_points;
			}

			inline const std::vector<std::vector<cv::Point>>& getArPixelLines()
			{
				return _ar_pixel_lines;
			}
			
			inline const std::vector<std::vector<cv::Point>>& getArPixelWarningL1s() {
				return _ar_pixel_warningL1s;
			}
			inline const std::vector<std::vector<cv::Point>>& getArPixelWarningL2s() {
				return _ar_pixel_warningL2s;
			}
#endif
			inline const ArInfosInternal& getArInfos()
			{
				return _ar_infos;
			}

			inline const std::string& getArVectorFile()
			{
				return  _ar_vector_file;
			}

			inline void setCurrentTime(int64_t current_time)
			{
				_current_time = current_time;
			}

			inline const int64_t& getCurrentTime()
			{
				return _current_time;
			}

			inline void setOriginalPts(int64_t original_pts) 
			{
				_original_pts = original_pts;
			}

			inline const int64_t& getOriginalPts()
			{
				return _original_pts;
			}

		private:
			std::shared_ptr<AVFrame> _ptr{};
			AVFrame* _native_ptr{};
			std::vector<uint8_t> _sei_buf{};

			JoFmvMetaDataBasic _meta_data_basic{};
#ifdef ENABLE_AIRBORNE
			FRAME_POS_Qianjue _meta_data_qianjue{};
#endif
			bool _meta_data_valid{};
			ArInfosInternal _ar_infos{};
			AiHeatmapInfo _ai_heatmap_infos{};
			std::string _meta_data_json{};
			std::string _ar_vector_file{};
			std::queue<int> _ar_valid_point_index{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> _ar_pixel_points{};
			std::vector<std::vector<cv::Point>> _ar_pixel_lines{};
			std::vector<std::vector<cv::Point>> _ar_pixel_warningL1s{};
			std::vector<std::vector<cv::Point>> _ar_pixel_warningL2s{};
#endif
			int64_t _current_time{};
			int64_t _original_pts{};
		};

		class Rational
		{
		public:
			Rational() {}
			Rational(AVRational av_rational)
			{
				num = av_rational.num;
				den = av_rational.den;
			}

			Rational(int num, int den)
			{
				this->num = num;
				this->den = den;
			}

			AVRational ToAVRational()
			{
				AVRational av_rational;
				av_rational.num = num;
				av_rational.den = den;
				return av_rational;
			}

			int num{};
			int den{};
		};

		Packet MakePacket(AVPacket* packet);
		Frame MakeFrame(AVFrame* frame);

		std::string AVError2String(int error_num);

		/*
		读取并拷贝sps/pps数据
		codec_extradata是codecpar的扩展数据，sps/pps数据就在这个扩展数据里面
		codec_extradata_size是扩展数据大小
		out_extradata是输出sps/pps数据的AVPacket包
		padding: 就是宏AV_INPUT_BUFFER_PADDING_SIZE的值(64)，是用于解码的输入流的末尾必要的额外字节个数，
				 需要它主要是因为一些优化的流读取器一次读取32或者64比特，可能会读取超过size大小内存的末尾。
		*/
		int h264_extradata_to_annexb(const uint8_t* codec_extradata, const int codec_extradata_size, AVPacket* out_extradata, int padding);

		/*
		为包数据添加起始码、SPS/PPS等信息后写入文件。
		AVPacket数据包可能包含一帧或几帧数据，对于视频来说只有1帧，对音频来说就包含几帧
		in为要处理的数据包
		file为输出文件的指针
		*/
		int h264_mp4toannexb(AVFormatContext* fmt_ctx, AVPacket* in, AVPacket** out);
	}
}

#endif // !EAPST_FFMPEG_WRAP_H
