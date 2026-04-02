#ifndef EAPS_META_DATA_BASIC_H
#define EAPS_META_DATA_BASIC_H

#include "EapsFFmpegWrap.h"
#include "EapsUtils.h"

#define USE_OLD_SEI_REPLACE 0

namespace eap {
	namespace sma {

#if USE_OLD_SEI_REPLACE
		bool VideoSeiDataAssemblyH264(uint8_t* content, int content_size, uint8_t** out_data, int* out_size, bool isAnnexb = true);

		bool VideoSeiDataParse(uint8_t* in_data, int in_size, AVCodecID codec_id, uint8_t** out_data, int* out_size);

		bool VideoSeiDataChecking(uint8_t* sei_data, int sei_size, uint8_t** real_data, int* real_size, int* v);
#else
		AVPacket* VideoExtraDataReplace(AVPacket* packet, AVCodecID codec_id, ByteArray_t content, bool isAnnexb = true);
#endif // #if USE_OLD_SEI_REPLACE		

	}
}

#endif // !EAPS_META_DATA_BASIC_H