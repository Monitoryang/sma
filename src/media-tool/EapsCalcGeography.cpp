#include "EapsCalcGeography.h"
#include "EapsJacTypeDefs.h"

#include<string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
namespace eap {
	namespace sma {
#if 0
		void CalcGeography::InitWgs84()
		{
			pWgs84.SemimajorAxis = 6378137;
			pWgs84.InverseFlattening = 298.257223563;
			pWgs84.Eccentricity = 0.0818191908426215;
		}
#endif

		/*
		输入
			|   视场角 fov_h
			|   图像分辨率frame_cols，frame_rows
			|	a 0 b     a=frame_cols / 2.0 / tan(fov_h / 2.0)    b=frame_cols / 2.0
			|	0 c d	  c=frame_cols / 2.0 / tan(fov_h / 2.0)    d=frame_rows / 2.0
			|	0 0 0
		*/
		cv::Mat CalcGeography::CalcCameraIntrinsicsbaseonFOV(double fov_h, double frame_cols, double frame_rows)
		{
			cv::Mat K_Intrinsic = cv::Mat::eye(3, 3, CV_32F);;
			K_Intrinsic.at<float>(0, 0) = frame_cols / 2.0 / tan(fov_h / 2.0);
			K_Intrinsic.at<float>(0, 2) = frame_cols / 2.0;
			K_Intrinsic.at<float>(1, 1) = frame_cols / 2.0 / tan(fov_h / 2.0);
			K_Intrinsic.at<float>(1, 2) = frame_rows / 2.0;

			return K_Intrinsic;
		}

		GimbalFrameAngle CalcGeography::Matrix2Euler(cv::Mat R)
		{
			GimbalFrameAngle gimbalframeangle;
			gimbalframeangle.FrameYaw = atan2(R.at<float>(1, 0), R.at<float>(0, 0));
			gimbalframeangle.FrameRoll = atan2(R.at<float>(2, 1), R.at<float>(2, 2));
			gimbalframeangle.FramePitch = atan2(-R.at<float>(2, 0), sqrt(R.at<float>(2, 1)*R.at<float>(2, 1) + R.at<float>(2, 2)*R.at<float>(2, 2)));

			return gimbalframeangle;
		}

		cv::Mat CalcGeography::AngletoMat(double pitch, double yaw, double roll)
		{
			double pitch_r = PI * pitch / 180.0;
			double yaw_r = PI * yaw / 180.0;
			double roll_r = PI * roll / 180.0;

			cv::Mat AngleMatrix(3, 3, CV_32F);

			AngleMatrix.at<float>(0, 0) = cos(pitch_r) * cos(yaw_r);
			AngleMatrix.at<float>(0, 1) = cos(pitch_r) * sin(yaw_r);
			AngleMatrix.at<float>(0, 2) = -sin(pitch_r);

			AngleMatrix.at<float>(1, 0) = sin(roll_r) * sin(pitch_r) * cos(yaw_r) - cos(roll_r) * sin(yaw_r);
			AngleMatrix.at<float>(1, 1) = sin(roll_r) * sin(pitch_r) * sin(yaw_r) + cos(roll_r) * cos(yaw_r);
			AngleMatrix.at<float>(1, 2) = sin(roll_r) * cos(pitch_r);

			AngleMatrix.at<float>(2, 0) = cos(roll_r) * sin(pitch_r) * cos(yaw_r) + sin(roll_r) * sin(yaw_r);
			AngleMatrix.at<float>(2, 1) = cos(roll_r) * sin(pitch_r) * sin(yaw_r) - sin(roll_r) * cos(yaw_r);
			AngleMatrix.at<float>(2, 2) = cos(roll_r) * cos(pitch_r);

			return AngleMatrix;
		}

		cv::Mat CalcGeography::GetCameraToAircraftBodyRotation(double psi, double theta, double delta)
		{
			cv::Mat Rotation = AngletoMat(theta, psi, delta);
			return Rotation.t();
		}

		cv::Mat CalcGeography::GetAircraftBodyToLocalNEDRotation(double heading, double pitch, double roll)
		{
			cv::Mat Rotation = AngletoMat(pitch, heading, roll);
			return Rotation.t();
		}

		cv::Mat CalcGeography::GetCameraToLocalNEDRotation(double heading, double pitch, double roll)
		{
			cv::Mat Rotation = AngletoMat(pitch, heading, roll);
			return Rotation.t();
		}

		cv::Mat CalcGeography::CalcRotationMatrix(double heading, double pitch, double roll, double psi, double theta, double delta)
		{
			cv::Mat _cRi(3, 3, CV_32F);
			_cRi.at<float>(0, 0) = 0;
			_cRi.at<float>(0, 1) = 0;
			_cRi.at<float>(0, 2) = 1;

			_cRi.at<float>(1, 0) = 1;
			_cRi.at<float>(1, 1) = 0;
			_cRi.at<float>(1, 2) = 0;

			_cRi.at<float>(2, 0) = 0;
			_cRi.at<float>(2, 1) = 1;
			_cRi.at<float>(2, 2) = 0;

			cv::Mat _bRc(3, 3, CV_32F);
			_bRc = GetCameraToAircraftBodyRotation(psi, theta, delta);

			cv::Mat _nRb(3, 3, CV_32F);
			_nRb = GetAircraftBodyToLocalNEDRotation(heading, pitch, roll);

			cv::Mat _pRn(3, 3, CV_32F);

			_pRn.at<float>(0, 0) = 0;
			_pRn.at<float>(0, 1) = 1;
			_pRn.at<float>(0, 2) = 0;

			_pRn.at<float>(1, 0) = 1;
			_pRn.at<float>(1, 1) = 0;
			_pRn.at<float>(1, 2) = 0;

			_pRn.at<float>(2, 0) = 0;
			_pRn.at<float>(2, 1) = 0;
			_pRn.at<float>(2, 2) = -1;


			//Mat _pRi = pRn * _nRb * _bRc* _cRi;
			cv::Mat _nRi(3, 3, CV_32F);
#if 1
			_nRi = _nRb * _bRc * _cRi;
			//cout << "************" << endl;
			//cout << _nRb << endl;
			//cout << _bRc << endl;
			//cout << _cRi << endl;
			//cout << _nRi << endl;
			return _nRi;
			//return _nRb;
#else
			cv::Mat tmp = cv::Mat::zeros(3, 3, CV_32F);
			tmp.at<float>(0, 1) = 1;
			tmp.at<float>(1, 0) = 1;
			tmp.at<float>(2, 2) = 1;
			_nRi = tmp * _nRb;
			return _nRi;
#endif
		}

		cv::Point3d CalcGeography::Pixeltomm(cv::Point3d pixelPoints, cv::Mat K)
		{
			cv::Point3d mmPoints;

			cv::Mat pixelVector(3, 1, CV_32F);

			pixelVector.at<float>(0, 0) = pixelPoints.x;
			pixelVector.at<float>(1, 0) = pixelPoints.y;
			pixelVector.at<float>(2, 0) = pixelPoints.z;

			cv::Mat mmVector = K.inv() * pixelVector;

			mmPoints.x = mmVector.at<float>(0, 0);
			mmPoints.y = mmVector.at<float>(1, 0);
			mmPoints.z = mmVector.at<float>(2, 0);

			return mmPoints;
		}

		/// <summary>
		/// 将像点坐标从像空间坐标系转换到局部导航坐标系;
		/// </summary>;
		/// <param name="iPoints">像点在像空间的坐标</param>
		///<param name="nRi"></param>像空间坐标系到局部导航坐标系的旋转矩阵;
		/// <returns>返回像点在局部导航坐标系的坐标</returns>;
		cv::Point3d CalcGeography::RotateImageToLocalNED(cv::Point3d iPoints, cv::Mat nRi)
		{
			cv::Point3d nPoints;

			cv::Mat iVector(3, 1, CV_32F);

			iVector.at<float>(0, 0) = iPoints.x;
			iVector.at<float>(1, 0) = iPoints.y;
			iVector.at<float>(2, 0) = iPoints.z;

			cv::Mat nVector = nRi * iVector;

			nPoints.x = nVector.at<float>(0, 0);
			nPoints.y = nVector.at<float>(1, 0);
			nPoints.z = nVector.at<float>(2, 0);

			return nPoints;
		}

		/// <summary>
		/// 将像点从像空间坐标系投影到指定高度的局部导航坐标系得到物点;
		/// </summary>
		/// <param name="pHeight">指定椭球高的投影面[m]</param>;
		/// <param name="iPoints">像点在像空间的坐标</param>;
		///<param name="nRi"></param>像空间坐标系到局部导航坐标系的旋转矩阵;
		/// <returns>返回物点在指定高度局部导航坐标系的坐标</returns>;
		cv::Point3d CalcGeography::ProjectAllImageToLocalNED(cv::Point3d Origin, double refHeight, double pHeight, cv::Point3d iPoints, cv::Mat nRi)
		{

			double dHeight = Origin.z + refHeight - pHeight;
			cv::Point3d nPoints = RotateImageToLocalNED(iPoints, nRi);

			cv::Point3d nVector = cv::Point3d(nPoints.x, nPoints.y, nPoints.z);
			cv::Point3d pVector = cv::Point3d(dHeight * nVector.x / nVector.z + Origin.x, dHeight * nVector.y / nVector.z + Origin.y, dHeight);

			return pVector;
		}

		cv::Point3d CalcGeography::ProjectAllPixelToLocalNED(cv::Point3d Origin, double refHeight, double pHeight, cv::Mat nRi, cv::Point3d pixelPoints, cv::Mat K)
		{
			cv::Point3d iPoints = Pixeltomm(pixelPoints, K);

			cv::Point3d pVector = ProjectAllImageToLocalNED(Origin, refHeight, pHeight, iPoints, nRi);
			return pVector;
		}

		/// <summary>
		/// 将像点从像空间坐标系投影到指定高度的局部导航坐标系得到物点;
		/// </summary>
		/// <param name="pHeight">指定椭球高的投影面[m]</param>
		/// <param name="iPoints">像点在像空间的坐标</param>;
		/// <returns>返回物点在指定高度局部导航坐标系的坐标</returns>;
		cv::Point3d CalcGeography::ProjectImageToLocalNED(GeographicLatLng Origin, double pHeight, cv::Point3d iPoints, cv::Mat nRi)
		{
			double dHeight = Origin.Altitude - pHeight;
			cv::Point3d nPoints = RotateImageToLocalNED(iPoints, nRi);
			cv::Point3d pPoints;

			if (nPoints.z < 0) {
				pPoints = cv::Point3d(-9999, -9999, -9999);
			}
			else {
				cv::Point3d nVector = cv::Point3d(nPoints.x, nPoints.y, nPoints.z);
				cv::Point3d pVector = cv::Point3d(dHeight * nVector.x / nVector.z, dHeight * nVector.y / nVector.z, dHeight);
				pPoints = cv::Point3d(pVector.x, pVector.y, pVector.z);
			}

			return pPoints;
		}

		/// <summary>
		/// 将像点从BLUH像空间坐标系投影到指定高度的得到物点在大地测量坐标系的经纬高
		/// </summary>
		/// <param name="pHeight">指定投影的椭球高</param>
		/// <param name="iPoints">像点在BLUH像空间的坐标</param>
		/// <returns>返回物点在大地测量坐标系的经纬高</returns>
		GeographicLatLng CalcGeography::ProjectImageToGeodetic(GeographicLatLng Origin, double pHeight, cv::Mat nRi, cv::Point3d pixelPoints, cv::Mat K)
		{
			cv::Point3d iPoints = Pixeltomm(pixelPoints, K);

			cv::Point3d nPoints = ProjectImageToLocalNED(Origin, pHeight, iPoints, nRi);

			GeographicLatLng gPoints = TransformLocalNEDToGeodetic(Origin, nPoints);

			return gPoints;
		}

		/// <summary>
		/// 将点p从局部导航坐标系转换到大地测量坐标系;
		/// </summary> 
		/// <param name="origin">局部导航坐标系原点o</param>
		/// <param name="nPoints">点p的局部导航坐标</param>
		/// <returns>返回该点p在大地测量坐标系的经纬高</returns>;
		//大地坐标系：经纬高；
		GeographicLatLng CalcGeography::TransformLocalNEDToGeodetic(GeographicLatLng origin, cv::Point3d nPoints)
		{
			double lat0 = origin.Latitude, lon0 = origin.Longitude, h0 = origin.Altitude;
			double latRad0 = lat0 / RAD2DEG, lonRad0 = lon0 / RAD2DEG;

			double e2 = EC * EC;
			double slamd2 = pow(sin(latRad0), 2.0);
			double den = pow(1.0 - e2 * slamd2, 1.5);
			double RLAT = SE * (1.0 - e2) / den + h0;
			double den1 = sqrt(1.0 - e2 * slamd2);
			double RLONG = (SE / den1 + h0) * cos(latRad0);

			double x = nPoints.x, y = nPoints.y, z = nPoints.z;
			double temp1 = x / RLAT;
			double temp2 = y / RLONG;
			double lat = (latRad0 + temp1) * RAD2DEG;
			double lon = (lonRad0 + temp2) * RAD2DEG;
			double h = h0 - z;

			//gPoints.Longitude = lon;
			//gPoints.Latitude = lat;
			//gPoints.Altitude = h;
			GeographicLatLng  gPoints = { lat,lon,h };

			return gPoints;
		}

		/// <summary>
		/// 将某一点从大地测量坐标系(WGS84)转换到局部导航坐标系(北东地坐标系);  
		/// </summary> ;
		/// <param name="origin">局部导航坐标系原点o</param>;
		/// <param name="gPoints">点p的大地测量坐标</param>;
		/// <returns>返回该点在局部导航坐标系中的的坐标</returns>;
		/*
			lat/180*pi
		*/
		cv::Point3d CalcGeography::TransformGeodeticToLocalNED(GeographicLatLng origin, GeographicLatLng gPoints)
		{
			cv::Point3d  nPoints;
			//step 1 : 从LLA坐标系转到ECEF坐标系
			double lat0 = origin.Latitude, lon0 = origin.Longitude, h0 = origin.Altitude;
			double latRad0 = lat0 / RAD2DEG, lonRad0 = lon0 / RAD2DEG; //经纬度转弧度

			double e2 = EC * EC;  //第一偏心率
			double slamd2 = pow(sin(latRad0), 2.0);   // sin(纬度)的平方
			double den = pow(1.0 - e2 * slamd2, 1.5); // ((1-偏心率^2) * sin(纬度)的平方)^1.5
			double RLAT = SE * (1.0 - e2) / den + h0;   //se为地球长半轴, 长半轴 * （1-偏心率^2） / ((1-偏心率^2) * sin(纬度)的平方)^1.5 + 原点高度
			double den1 = sqrt(1.0 - e2 * slamd2);
			double RLONG = (SE / den1 + h0) * cos(latRad0);
			//step 2 : 已知P点的大地测量坐标系
			double lat = gPoints.Latitude, lon = gPoints.Longitude, h = gPoints.Altitude;
			double latRad = lat / RAD2DEG, lonRad = lon / RAD2DEG;
			//step 3 : 已知原点和P点的LLA坐标系距离关系，乘以对应的ECEF坐标可以得到航向，俯仰、偏航高度三个方向的距离
			double x = (latRad - latRad0) * RLAT;   //半径*弧度  
			double y = (lonRad - lonRad0) * RLONG;	//半径*弧度	 
			double z = h0 - h;

			nPoints.x = x;
			nPoints.y = y;
			nPoints.z = z;

			return nPoints;
		}

		//已知两个坐标点，以orgin 为原点，计算两个点之间在北东地坐标系下的偏移矢量
		DeltaDistAndAngle CalcGeography::CalcYawBetweenTwoGeographicLatLngpoints(GeographicLatLng origin, GeographicLatLng gPoints)
		{
			cv::Point3d nPoints = TransformGeodeticToLocalNED(origin, gPoints);
			float l = std::sqrt(nPoints.y*nPoints.y + nPoints.x*nPoints.x);
			DeltaDistAndAngle vals = { 0,0,0,0,0,0 };
			vals.dist_H = std::sqrt(nPoints.x * nPoints.x + nPoints.y * nPoints.y);
			vals.yaw = atan2(nPoints.y, nPoints.x);
			vals.tilt = -(atan2(abs(nPoints.z), l));
			vals.dist_S = std::sqrt(nPoints.x * nPoints.x + nPoints.y * nPoints.y + nPoints.z * nPoints.z);
			vals.delta_x = nPoints.x;
			vals.delta_y = nPoints.y;
			vals.delta_z = nPoints.z;

			return vals;
		}

		GeographicLatLng CalcGeography::CalcGeographicLatLngBaseonSlantR(GeographicLatLng origin, double TgtHeight, double SlanR, double yaw)
		{
			double deltaheight = origin.Altitude - TgtHeight;
			double GroundDist = sqrt(SlanR * SlanR - deltaheight * deltaheight);
			double GroundDist_x = GroundDist * cos(yaw);
			double GroundDist_y = GroundDist * sin(yaw);

			cv::Point3d nPoints;
			nPoints.x = GroundDist_x;
			nPoints.y = GroundDist_y;
			nPoints.z = deltaheight;

			GeographicLatLng  gPoints = TransformLocalNEDToGeodetic(origin, nPoints);

			return gPoints;
		}

		/*
		坐标模式
		launchpoint：发射点坐标；
		prepoint：预定点坐标；
		realpoint：实际点坐标；
		*/
		DeltaDistAndAngle CalcGeography::CalcCoordinationMode(GeographicLatLng launchpoint, GeographicLatLng  prepoint, GeographicLatLng  realpoint)
		{
			//计算发射点和预定点之间的角度等信息//
			DeltaDistAndAngle pre2launch = CalcYawBetweenTwoGeographicLatLngpoints(launchpoint, prepoint);
			double pre2launchyaw = 0;//将角度转换为0~2PI//

			if ((int)(pre2launch.yaw * 1e4) < 0) {
				pre2launchyaw = ((int)(pre2launch.yaw * 1e4) + 62832) / 1e4;
			}
			else {
				pre2launchyaw = pre2launch.yaw;
			}
			//计算预定点和弹着点之间的角度等信息//
			DeltaDistAndAngle real2launch = CalcYawBetweenTwoGeographicLatLngpoints(prepoint, realpoint);
			double real2launchyaw = 0;//将角度转换为0~2PI//

			if ((int)(real2launch.yaw * 1e4) < 0) {
				real2launchyaw = ((int)(real2launch.yaw * 1e4) + 62832) / 1e4;
			}
			else {
				real2launchyaw = real2launch.yaw;
			}

			//以发射点和预定点方向为X轴0方向，计算弹着点在此坐标系的角度值---偏差角---取值方位-PI~PI//
			double real2pre = real2launchyaw - pre2launchyaw;//

			if (((int)(real2pre * 1e7)) > ((int)(PI * 1E7))) {
				real2pre = (((int)(real2pre * 1e7)) - ((int)(2 * PI * 1E7))) / 1e7;
			}

			if (((int)(real2pre * 1e7)) < ((int)(-PI * 1E7))) {
				real2pre = -(((int)(real2pre * 1e7)) + ((int)(2 * PI * 1E7))) / 1e7;
			}

			//根据弹着点和预定点之间的斜距计算在X轴、y轴方向的投影//
			double delta_x = real2launch.dist_H * std::cos(real2pre);

			//根据弹着点和预定点之间的斜距计算在X轴、y轴方向的投影//
			double delta_y = -real2launch.dist_H * std::sin(real2pre);

			DeltaDistAndAngle finalval = { 0,0,0,0,0,0 };

			finalval.delta_x = delta_y;
			finalval.delta_y = delta_x;
			finalval.delta_z = real2launch.delta_z;
			finalval.dist_H = real2launch.dist_H;
			finalval.dist_S = real2launch.dist_S;
			finalval.yaw = real2pre;

			return finalval;
		}

		/*
		弹道角模式：
		trajagl:射向方向，单位弧度，范围0~2PI;
		prepoint:预定点坐标；
		realpoint：实际点坐标；
		*/
		DeltaDistAndAngle CalcGeography::CalcTrajectoryAngleMode(double trajagl, GeographicLatLng  prepoint, GeographicLatLng  realpoint)
		{
			//计算预定点和弹着点之间的角度等信息//
			DeltaDistAndAngle real2launch = CalcYawBetweenTwoGeographicLatLngpoints(prepoint, realpoint);

			double real2launchyaw = 0;//将角度转换为0~2PI//

			if ((int)(real2launch.yaw * 1e4) < 0) {
				real2launchyaw = ((int)(real2launch.yaw * 1e4) + 62832) / 1e4;
			}
			else {
				real2launchyaw = real2launch.yaw;
			}

			//以发射点和预定点方向为X轴0方向，计算弹着点在此坐标系的角度值---偏差角//
			double real2pre = real2launchyaw - trajagl;//

			if (((int)(real2pre * 1e7)) > ((int)(PI * 1E7))) {
				real2pre = (((int)(real2pre * 1e7)) - ((int)(2 * PI * 1E7))) / 1e7;
			}

			if (((int)(real2pre * 1e7)) < ((int)(-PI * 1E7))) {
				real2pre = -(((int)(real2pre * 1e7)) + ((int)(2 * PI * 1E7))) / 1e7;
			}

			//根据弹着点和预定点之间的斜距计算在X轴、y轴方向的投影//
			double delta_x = real2launch.dist_H * std::cos(real2pre);

			//根据弹着点和预定点之间的斜距计算在X轴、y轴方向的投影//
			double delta_y = -real2launch.dist_H * std::sin(real2pre);

			DeltaDistAndAngle finalval = { 0,0,0,0,0,0 };

			finalval.delta_x = delta_y;
			finalval.delta_y = delta_x;
			finalval.delta_z = real2launch.delta_z;
			finalval.dist_H = real2launch.dist_H;
			finalval.dist_S = real2launch.dist_S;
			finalval.yaw = real2pre;

			return finalval;
		}

		/*
		通过计算地理定位，计算吊舱框架角；
		*/
		GimbalFrameAngle CalcGeography::CalcGimbalAngleBasedonGeography(UAV_POS PosImgData)
		{
			GimbalFrameAngle gimbalframeangle;

#define TEST 0	

			cv::Mat K_Intrinsic = CalcCameraIntrinsicsbaseonFOV(PosImgData.fov_h, PosImgData.framecols, PosImgData.framerows);
			cv::Mat R_LocalNED2Img = CalcRotationMatrix(PosImgData.yaw * 180.0 / PI, PosImgData.tilt * 180.0 / PI, PosImgData.roll * 180.0 / PI, PosImgData.frame_pan * 180.0 / PI, PosImgData.frame_pitch * 180.0 / PI, PosImgData.frame_roll * 180.0 / PI);
#if TEST
			R_LocalNED2Img = CalcRotationMatrix(168.548, 2.693, 0.189, 45.256, -59.581, 0.0);
#endif

			//保持纵横比不变,步长为100个像素//
			double xymax = std::max(PosImgData.pix_x, PosImgData.pix_y);
			double xyratio = std::abs(PosImgData.pix_x) / std::abs(PosImgData.pix_y);

			double stepwidth = 100.0;

			double ctrl_x = 0.0;
			if (std::abs(PosImgData.pix_x) > stepwidth) {
				if (PosImgData.pix_x > 0.0) {
					ctrl_x = PosImgData.framecols / 2.0 + stepwidth;
				}
				else {
					ctrl_x = PosImgData.framecols / 2.0 - stepwidth;
				}
			}
			else {
				ctrl_x = PosImgData.framecols / 2.0 + PosImgData.pix_x;
			}

			double ctrl_y = 0.0;

			if (std::abs(PosImgData.pix_y) > stepwidth) {
				if (PosImgData.pix_y > 0.0) {
					ctrl_y = PosImgData.framerows / 2.0 - stepwidth;
				}
				else {
					ctrl_y = PosImgData.framerows / 2.0 + stepwidth;
				}
			}
			else {
				ctrl_y = PosImgData.framerows / 2.0 - PosImgData.pix_y;
			}

#if TEST
			K_Intrinsic = cv::Mat::eye(3, 3, CV_32F);
			K_Intrinsic.at<float>(0, 0) = 2342.1273;
			K_Intrinsic.at<float>(0, 2) = 953.0816;
			K_Intrinsic.at<float>(1, 1) = 2342.1273;
			K_Intrinsic.at<float>(1, 2) = 550.8064;
#endif

			cv::Point3d pixelPoints = cv::Point3d(ctrl_x, ctrl_y, 1.0);
#if TEST
			pixelPoints = cv::Point3d(0, 0, 1.0);
#endif

			cv::Point3d iPoints = Pixeltomm(pixelPoints, K_Intrinsic);
			GeographicLatLng Origin;
			Origin.Latitude = PosImgData.lat;
			Origin.Longitude = PosImgData.lon;
			Origin.Altitude = PosImgData.alt;

#if TEST
			Origin.Latitude = 31.5671223;//获取飞机纬度
			Origin.Longitude = 104.4502309;//获取飞机经度
			Origin.Altitude = 1098;//获取飞机高度
			PosImgData.tgtalt = 620;//获取目标高度
#endif

			cv::Point3d nPoints = ProjectImageToLocalNED(Origin, PosImgData.tgtalt, iPoints, R_LocalNED2Img);
			//printf("nPoints.x = %.3f,nPoints.y = %.3f,nPoints.z = %.3f\n",nPoints.x, nPoints.y, nPoints.z);

			//GeographicLatLng gPoints = ProjectImageToGeodetic(Origin, PosImgData.tgtalt, R_LocalNED2Img, pixelPoints, K_Intrinsic);
			//printf("gPoints.Latitude = %.7f,gPoints.Longitude = %.7f,gPoints.Altitude = %.3f\n", gPoints.Latitude, gPoints.Longitude, gPoints.Altitude);
			float yaw = atan2(nPoints.y, nPoints.x) * 180.0 / PI;
			float tilt = (atan2(sqrt(nPoints.x*nPoints.x + nPoints.y * nPoints.y), abs(nPoints.z)) - PI / 2.0) * 180.0 / PI;
			float roll = 0;

			//计算从相机到导航的变换矩阵//
			cv::Mat nRi = GetCameraToAircraftBodyRotation(yaw, tilt, roll);
			//printf("yaw = %.3f,tilt = %.3f,roll = %.3f\n", yaw, tilt, roll);
			//printf("PosImgData.yaw = %.3f,PosImgData.tilt = %.3f,PosImgData.roll = %.3f\n", PosImgData.yaw * 57.3, PosImgData.tilt * 57.3, PosImgData.roll * 57.3);
			//计算从机体到导航的变换矩阵//
			cv::Mat nRb = GetCameraToAircraftBodyRotation(PosImgData.yaw * 180.0 / PI, PosImgData.tilt * 180.0 / PI, PosImgData.roll * 180.0 / PI);

			//计算从相机到机体的变换矩阵//
			cv::Mat bRi = nRb.inv() * nRi;

			gimbalframeangle = Matrix2Euler(bRi);
			//printf("gimbalframeangle.yaw = %.3f,gimbalframeangle.tilt = %.3f,gimbalframeangle.roll = %.3f\n", gimbalframeangle.FrameYaw * 57.3, gimbalframeangle.FramePitch * 57.3, gimbalframeangle.FrameRoll * 57.3);

			return gimbalframeangle;
		}

		/*
		double heading, double pitch, double roll:飞机欧拉角
		double psi, double theta, double delta：导航姿态角
		单位度；
		*/
		cv::Mat CalcGeography::CalcRotationMatrixCameraToAircraftBody(double heading, double pitch, double roll, double psi, double theta, double delta)
		{

			cv::Mat _nRc(3, 3, CV_32F);
			_nRc = GetCameraToLocalNEDRotation(psi, theta, delta);

			cv::Mat _nRb(3, 3, CV_32F);
			_nRb = GetAircraftBodyToLocalNEDRotation(heading, pitch, roll);

			cv::Mat _bRc(3, 3, CV_32F);

			_bRc = _nRb.inv() * _nRc;

			return _bRc;
		}

		GimbalFrameAngle CalcGeography::ClacGimbalFrameAngle(double heading, double pitch, double roll, double psi, double theta, double delta)
		{
			cv::Mat _bRc = CalcRotationMatrixCameraToAircraftBody(heading, pitch, roll, psi, theta, delta);

			GimbalFrameAngle gimbalframeangle = Matrix2Euler(_bRc);
			//printf("gimbalframeangle.yaw = %.3f,gimbalframeangle.tilt = %.3f,gimbalframeangle.roll = %.3f\n", gimbalframeangle.FrameYaw * 57.3, gimbalframeangle.FramePitch * 57.3, gimbalframeangle.FrameRoll * 57.3);

			return gimbalframeangle;
		}
	}
}
#endif