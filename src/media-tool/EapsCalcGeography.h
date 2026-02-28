#ifndef EAPS_CALC_GEOGRAPHY_H
#define EAPS_CALC_GEOGRAPHY_H
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace eap {
	namespace sma {
		typedef struct GeographicLatLng
		{
			double Latitude;   //纬度
			double Longitude;  //经度
			double Altitude;   //高度
		} GeographicLatLng;

		typedef struct DistAndAngle
		{
			double dist_H;//水平距离，单位米
			double dist_S;//斜距，单位米
			double yaw;   //偏航角度，单位弧度

		} DistAndAngle;

		typedef struct
		{
			double dist_H; //水平距离，单位米
			double dist_S; //斜距，单位米
			double yaw;    //偏航角度，单位弧度
			double tilt;   //偏航角度，单位弧度
			double delta_x;//X方向距离，单位米
			double delta_y;//Y方向距离，单位米
			double delta_z;//Z方向距离，单位米

		} DeltaDistAndAngle;

		typedef struct
		{
			double FrameYaw;  //欧拉偏航角,单位弧度;//
			double FramePitch;//欧拉俯仰角,单位弧度;//
			double FrameRoll; //欧拉滚转角，单位弧度;//

		} GimbalFrameAngle;

		typedef struct
		{
			double geo_x;
			double geo_y;
			double geo_z;
			double tgtalt;
			double yaw;
			double tilt;
			double roll;
			double frame_pan;
			double frame_pitch;
			double frame_roll;
			double pix_x;
			double pix_y;
			double fov_h;
			double framecols;
			double framerows;
		} OBS_STR;

		typedef struct
		{
			double lat;
			double lon;
			double alt;
			double tgtalt;
			double yaw;
			double tilt;
			double roll;
			double frame_pan;
			double frame_pitch;
			double frame_roll;
			double pix_x;
			double pix_y;
			double fov_h;
			double framecols;
			double framerows;
		} UAV_POS;

		class CalcGeography
		{
			// #define PI 3.1415926
			// #define RAD2DEG (180.0/3.1415926)

			//WGS 1984 是一个长半轴(a)为6378137，短半轴（b）为6356752.314245179 的椭球体，扁率(f)为298.257223563，f=(a-b)/a 。
			//第一偏心率：e^2= (a^2-b^2)/a^2 第二偏心率：ep^2= (a^2-b^2)/b^2
#define SE  6378137   //WGS84坐标系  地球长半轴
#define INV 298.257223563  //扁率
#define EC 0.0818191908426215 //第一偏心率

		public:
			static cv::Mat CalcCameraIntrinsicsbaseonFOV(double fov_h, double frame_cols, double frame_rows);
			static GimbalFrameAngle Matrix2Euler(cv::Mat R);
			static cv::Mat AngletoMat(double pitch, double yaw, double roll);
			static cv::Mat GetCameraToAircraftBodyRotation(double psi, double theta, double delta);
			static cv::Mat GetAircraftBodyToLocalNEDRotation(double heading, double pitch, double roll);
			static cv::Mat GetCameraToLocalNEDRotation(double heading, double pitch, double roll);
			static cv::Mat CalcRotationMatrix(double heading, double pitch, double roll, double psi, double theta, double delta);
			static cv::Point3d Pixeltomm(cv::Point3d pixelPoints, cv::Mat K);
			static cv::Point3d RotateImageToLocalNED(cv::Point3d iPoints, cv::Mat nRi);
			static cv::Point3d ProjectAllImageToLocalNED(cv::Point3d Origin, double refHeight, double pHeight, cv::Point3d iPoints, cv::Mat nRi);
			static GeographicLatLng TransformLocalNEDToGeodetic(GeographicLatLng origin, cv::Point3d nPoints);
			static cv::Point3d ProjectAllPixelToLocalNED(cv::Point3d Origin, double refHeight, double pHeight, cv::Mat nRi, cv::Point3d pixelPoints, cv::Mat K);
			static cv::Point3d TransformGeodeticToLocalNED(GeographicLatLng origin, GeographicLatLng gPoints);
			static cv::Point3d ProjectImageToLocalNED(GeographicLatLng Origin, double pHeight, cv::Point3d iPoints, cv::Mat nRi);
			static GeographicLatLng ProjectImageToGeodetic(GeographicLatLng Origin, double pHeight, cv::Mat nRi, cv::Point3d pixelPoints, cv::Mat K);
			static DeltaDistAndAngle CalcYawBetweenTwoGeographicLatLngpoints(GeographicLatLng origin, GeographicLatLng gPoints);

			static GeographicLatLng CalcGeographicLatLngBaseonSlantR(GeographicLatLng origin, double TgtHeight, double SlanR, double yaw);
			static DeltaDistAndAngle CalcCoordinationMode(GeographicLatLng launchpoint, GeographicLatLng  prepoint, GeographicLatLng  realpoint);
			static DeltaDistAndAngle CalcTrajectoryAngleMode(double trajagl, GeographicLatLng  prepoint, GeographicLatLng  realpoint);
			static GimbalFrameAngle CalcGimbalAngleBasedonGeography(UAV_POS PosImgData);
			static GimbalFrameAngle ClacGimbalFrameAngle(double heading, double pitch, double roll, double psi, double theta, double delta);
			static cv::Mat CalcRotationMatrixCameraToAircraftBody(double heading, double pitch, double roll, double psi, double theta, double delta);
		};
	}
}
#endif
#endif !EAPS_CALC_GEOGRAPHY_H
