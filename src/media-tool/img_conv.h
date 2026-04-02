#ifndef EAPS_IMG_CONV_H
#define EAPS_IMG_CONV_H
#pragma once

#ifdef ENABLE_AIRBORNE
#include "Common.h"
namespace eap{
    namespace sma{
        DECL_EXPORT void I4202Bgr(char* pInput, char* pBgr, char* pSrcDev, char* pDstDev, int src_w, int src_h, int offset_x, int offset_y, int dst_w, int dst_h);
        DECL_EXPORT void AllocDeviceMem(char** pBuf, int size);
        DECL_EXPORT void FreeDeviceMem(char** pBuf);
    }
}

#endif // ENABLE_AIRBORNE
#endif
