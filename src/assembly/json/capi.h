#pragma once

#ifndef DGR_CAPI_H
#define DGR_CAPI_H

#include <core.h>

DGR_C_START

/**
 * @brief  创建直接地理定位对象
 *
 * @param  load 载荷姿态角（偏航/俯仰/滚转，单位为弧度）
 * @param  body 机体姿态角（偏航/俯仰/滚转，单位为弧度）
 *
 * @return 直接地理定位对象
 */
DirectGeoreferenceH DGR_DLL DGR_STDCALL DirectGeoreferenceCreate(EulerAngles load, EulerAngles body);

/**
 * @brief  计算图像坐标转地理坐标
 *
 * @param  hDirectGeoreference 直接地理定位对象
 * @param  uavLoc 飞机经纬高坐标（单位为度）
 * @param  targetAlt 目标高度（单位为米）
 * @param  cInfo 相机参数
 * @param  imageLoc 像素坐标（单位为像素）
 *
 * @return 像素坐标对应的地理坐标
 */
Coordinate DGR_DLL DGR_STDCALL DirectGeoreferenceImageToGeo(DirectGeoreferenceH hDirectGeoreference,
                                                            Coordinate uavLoc,
                                                            double targetAlt,
                                                            CameraInfo cInfo,
                                                            Coordinate imageLoc);

/**
 * @brief  计算地理坐标转图像坐标
 *
 * @param  hDirectGeoreference 直接地理定位对象
 * @param  uavLoc 飞机经纬高坐标（单位为度）
 * @param  targetAlt 目标高度（单位为米）
 * @param  cInfo 相机参数
 * @param  geoLoc 地理坐标（单位为度）
 *
 * @return 地理坐标对应的像素坐标
 */
Coordinate DGR_DLL DGR_STDCALL DirectGeoreferenceGeoToImage(
    DirectGeoreferenceH hDirectGeoreference, Coordinate uavLoc, double targetAlt, CameraInfo cInfo, Coordinate geoLoc);

/**
 * @brief  销毁直接地理定位对象
 *
 * @param  hDirectGeoreference 直接地理定位对象
 */
void DGR_DLL DGR_STDCALL DirectGeoreferenceDestroy(DirectGeoreferenceH hDirectGeoreference);

DGR_C_END

#endif // !DGR_CAPI_H