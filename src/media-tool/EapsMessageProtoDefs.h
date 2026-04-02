#ifndef EAPS_MESSAGE_PROTO_DEFS_H
#define EAPS_MESSAGE_PROTO_DEFS_H

#include "EapsJacTypeDefs.h"

#include <iostream>
#include <cstdio>
#include <iomanip>

#pragma pack(1)

#define JACX_SYNHEAD0 0xb5
#define JACX_SYNHEAD1 0x62
#define JACX_MSG_ACK_REQ_NONE 0x00
#define JACX_MSG_ACK_REQ_RECVED 0x01
#define JACX_MSG_ACK_REQ_RESULT 0x02
#define JACX_MSG_ACK_RET_RECVED 0x05
#define JACX_MSG_ACK_RET_FAILED 0x06
#define JACX_MSG_ACK_RET_SUCCEED 0x07

#define CMD_RET_FAILED -1
#define CMD_RET_NO_ACK 0
#define CMD_RET_SUCCEED 1
#define CMD_RET_SUCCEED_CONT 2
#define CMD_RET_CONT_FINISHED 3

#define MAX_PACKET_LENGTH 256
#define MAX_DETECT_NUM 1024
#define MAX_TEMP_NUM 100
#define TX_MESG_FREQUNCY 20
#define MAX_VECTORBUFSIZE 2048

#define PI_VAL 3.1415926

#define GIMBAL_OURSELF 0

enum MPFeature
{
	NONE = 0,
	OSD = 1,
	SLAM = 2,
	AR = 4,
	ORTHO = 8
};

/************************************************************************/
/*define destination and source ID                                      */
/************************************************************************/
enum enm_MSG_SOURCEID
{
	JACX_AUTOPILOT_ID = 0,
	JACX_GCS_ID,
	JACX_OI,
	NUM_MSG_SOURCEID
};

/************************************************************************/
/*define Message ID                                                     */
/************************************************************************/
//
enum enm_MSG_ID
{
	/*basic*/
	JACX_OPERATOR_CMD_PULSE = 3,
	JACX_MANUAL_ASSIST_MODE = 5,
	JACX_AP_MODE = 6,

	/*fight plan*/
	JACX_WAYPOINT = 8,
	JACX_WAYPOINT_LIST,
	JACX_WAYPOINT_QUICK,
	JACX_LANDING_PLAN,

	/*safe*/
	JACX_MISSION_LIMITS = 12,
	//add 20120913
	JACX_MOVING_LANDING_PLAN = 14,
	JACX_MOVING_PLATFORM_CFG = 15,

	/*guide and control*/
	JACX_TRACK = 16,
	JACX_AUTOPILOT_LOOP,
	JACX_HEADING_CONTROL,
	JACX_HEIGHT_CONTROL,
	//add 20130628
	JACX_SEMIAUTO_DATA,

	//add 20130709
	JACX_TAKEOFF_PLAN,

	JACX_CONTROLLER_DATA = 25,
	JACX_CONTROLLER_DATA_REQUEST,
	JACX_CONTROLLER_DATA_DEFAULT,
	JACX_ALTCONTROL_SOURCE,

	/*sensor*/
	JACX_SENSOR_ORIENTATION = 31,
	JACX_SENSOR_ERROR,
	JACX_ALTIMETER_SETTING,
	JACX_AIR_DATA_ZERO,

	JACX_MAG_DECLINATION,
	JACX_MAG_CALIBRATION,
	JACX_MAG_CALIBRATION_DATA,
	JACX_VIBRATE_ANALYSIS,

	/*surface*/
	JACX_SURFACE_TABLE_SIMPLE = 41,
	JACX_SURFACE_TABLE,
	JACX_SURFACE_TEST,
	JACX_SURFACE_PRE_TEST,
	JACX_RC_PWM_CAL,

	/*others*/
	JACX_SET_FUEL_LEVEL = 51,
	JACX_SYSTEM_VERSION,
	JACX_SYSTEM_RESET,
	JACX_TIMER_COUNT_DOWN,
	JACX_LOCK_PARA,
	JACX_HIL_SIM,
	//add 20130529
	JACX_EULER_BIAS,

	/*config*/
	JACX_CONFIG_FLSAH_DATA = 60,
	JACX_SAVE_CONFIG_FLSAH,
	JACX_DEFAULT_CONFIG_FLSAH,
	JACX_RO_CONFIG_FLSAH_DATA,
	JACX_SAVE_RO_CONFIG_FLSAH,
	JACX_DEFAULT_RO_CONFIG_FLSAH,

	/*doublet*/
	JACX_DOUBLET_CMD = 67,
	JACX_DOUBLET_AP_DATA,
	JACX_DOUBLET_SENSOR_DATA,

	/*telemetry*/
	JACX_TELEMETRY_CONFIG = 71,
	JACX_TELEMETRY_HI_RES,
	JACX_TELEMETRY_LO_RES,
	JACX_TELEMETRY_SYSSTATUS,
	JACX_TELEMETRY_PAYLOAD_DATA,
	JACX_TELEMETRY_DBG_CONT,
	JACX_TELEMETRY_RADIO,

	/*gcs*/
	JACX_GCS_RADIO_CONFIG = 81,
	JACX_GCS_GPS_READING,
	JACX_GCS_SYSTEM_STATUS,
	JACX_GCS_PWM_ENABLE,

	/*flight command*/
	JACX_PRELAUNCH = 86,
	JACX_LAUNCH_NOW,
	JACX_LAND_NOW,
	JACX_ABORT,

	/*record*/
	JACX_FLY_RECORD = 91,
	JACX_RECORD_FREQMODE,

	/*payload*/
	JACX_ENGINE_KILL = 95,
	JACX_PARACHUTE,
	JACX_DROP,
	JACX_AIRBAG,
	JACX_TURNON_LIGHTS,
	JACX_GIMAL_CMD,
	JACX_GIMBAL_STARE,
	JACX_RC_SWITCH,
	JACX_AIRBAG_DATA,
	JACX_PAYLODD_USER,

	/*airphoto*/
	JACX_AIRPHOTO_CFG_DATA = 110,
	JACX_AIRPHOTO_ACT,
	JACX_AIRPHOTO_NUM,
	JACX_AIRPHOTO_DOWNLOAD,
	JACX_EXT_PHOTO,
	JACX_PHOTO_PARA,

	/*radio config*/
	JACX_RADIO_CONFIG = 120,

	/*navigation*/
	JACX_DGPS_OEMV = 150,
	JACX_OP_GUIDE,
	JACX_ORIGIN_POINT,
	JACX_KALMAN_GAINS,
	JACX_KALMAN_GAINS_REQUEST,

	/*helicoptor(reserve)*/
	JACX_READ_HELI_TRIM = 160,
	JACX_THROT_CURVE,
	JACX_RC_CHANNEL_RANG,
	JACX_RC_SWASH_CENTER,
	JACX_SINGLE_CONTROL_DATA,
	JACX_SWASH_AFR,
	JACX_HELI_SURFACE_TEST,
	JACX_AUTHENTICATION_DATA,

	JACX_SUPER_AUTHENTICATION,
	JACX_SUPER_AUTHENTICATION_REQUEST,

	JACX_CAMERA_EXT = 171,

	JACX_AIRCRAFT_NO = 179,
	JACX_MOVING_LANDING_PARA = 180,
	JACX_HELI_GET_SET_TRIM = 190,
	JACX_HELI_GET_SET_INNER_LOOP_B,
	JACX_HELI_SET_TELE_RCPWM,

	JACX_USER_DEF = 0xf0, /*!< 0xfe */

	NUM_MSG_ID
};

/* Message's length*/

//FMS command

#define JACX_WAYPOINT_LEN 20
#define JACX_WAYPOINT_LIST_LEN 16
#define JACX_TRACK_LEN 4
#define JACX_MISSION_LIMITS_LEN 32

//Autopilot command
#define JACX_OPERATOR_CMD_ANGLE_LEN 10
#define JACX_OPERATOR_CMD_PULSE_LEN 12
#define JACX_MANUAL_ASSIST_MODE_LEN 2
#define JACX_ENGINE_KILL_LEN 2
#define JACX_CONTROLLER_DATA_LEN 4
#define JACX_CONTROLLER_DATA_REQUEST_LEN 6
#define JACX_CONTROLLER_DATA_DEFAULT_LEN 6
#define JACX_AUTOPILOT_LOOP_LEN 8

#define JACX_INNER_CONTROL_LEN 0
#define JACX_HEADING_CONTROL_LEN 2
#define JACX_HEIGHT_CONTROL_LEN 2

//configuration command
#define JACX_SENSOR_ORIENTATION_LEN 24

#define JACX_MAG_DECLINATION_LEN 8
#define JACX_MAG_CALIBRATION_LEN 2
#define JACX_MAG_CALIBRATION_DATA_LEN 48

#define JACX_SURFACE_TABLE_LEN 44
#define JACX_SURFACE_TABLE_SIMPLE_LEN 8

#define JACX_SYSTEM_VERSION_LEN 36

#define JACX_TELEMETRY_CONFIG_LEN 4
#define JACX_TELEMETRY_SYSSTATUS_LEN 72

#define JACX_DOUBLET_CMD_LEN 8
#define JACX_DOUBLET_AP_DATA_LEN 106
#define JACX_DOUBLET_SENSOR_DATA_LEN 86

#define JACX_HEIGHT_PDI_DATA_LEN 12
#define JACX_SWASH_AFR_SENSE_DATA_LEN 16
#define JACX_THROT_CURVE_LEN 10
#define JACX_RC_CHANNEL_RANG_LEN 8

#define JACX_AIRPHOTO_DATA_LEN 20

#define JACX_AIRPHOTO_DOWNLOAD_LEN 36
#define JACX_AIRPHOTO_NUM_LEN 4

#define JACX_RC_PWM_LEN 16

#define JACX_SINGLE_DATA_LEN 8


/*! Autopilot mode states */
enum APHCStates
{
	APV_GROUND_TEST_STATE,   //0
	APV_PRE_LAUNCH_STATE,		//!< Waiting for launch
	APV_ATT_ASSIST_STATE,    //2
	APV_HOVER_ASSIST_STATE,  //3
	APV_LIFT_OFF_STATE,
	APV_CLIMB_OUT_STATE,    //5
	APV_ACCELERATE_STATE,
	APV_V2L_STATE,
	APV_FLYING_STATE,      //8
	APV_LANDING_STATE,     //9
	APV_DECELERATE_STATE,   //10
	APV_L2V_STATE,          //11
	APV_FINAL_HOVER_STATE,  //12
	APV_FINAL_DESCENT_STATE, //13
	APV_FORCED_LANDING_STATE,  //14
	APV_AHRS_FORCED_LANDING_STATE,  //15   
	APV_SEMI_AUTO_ASSIST_STATE,  //16
	APV_SEMI_AUTO_V2L_STATE,     //17   
	APV_SEMI_AUTO_L2V_STATE,     //18  
	APV_AUTO_HOVER_STATE,        //19  
	APV_SELF_DESTRUCTION_STATE,  //20     
	APV_AIR_MAG_CAL_STATE,      //21 航磁校准         
	APV_BOMB_ATTACK_STATE,      //22 攻击模式   
	APV_OBSTACLE_MANEUVER_STATE,  //23 避障机动模式   
	APV_JOUAV_SDK_STATE,     //24 SDK控制模式
	NUM_APV_STATES              //!< Number of mode states
};

/************************************************************************/
/* Packet structure definition                                          */
/************************************************************************/
#pragma pack(1)
typedef struct
{
	UINT8 sync0;
	UINT8 sync1;
	UINT8 dest;
	UINT8 source;
	UINT8 msgID;
	UINT8 SeqNum;
	UINT8 ACK_NAK;
	UINT8 len;
} JACX_MSG_HEAD;

typedef struct
{
	UINT8 sync0;
	UINT8 sync1;
	UINT8 sync2;
	UINT8 len;
	UINT8 msgID;
} JACX_MSG_HEAD_HF;

typedef struct
{
	JACX_MSG_HEAD msgHead;
	UINT8 Data[MAX_PACKET_LENGTH]; //data exclude 2 crc
} JACX_MSG_PACKET;

typedef struct
{
	UINT8 sync0;
	UINT8 sync1;
	UINT8 cls;
	UINT8 id;
	UINT8 len;
	UINT8 res;
	UINT8 mag[6];
	UINT8 CK_A;
	UINT8 CK_B;
} JACX_ExtMag_PACKET;

/////////////////////////////////////////////////////////////////////////
//*AUTOPILOT Message
/////////////////////////////////////////////////////////////////////////
typedef struct
{
	UINT8 ControllerID;
	UINT8 ControllerVer;
	UINT8 Category;
	UINT8 DataID;
	UINT8 NumData;
	UINT8 Res;
} CONTROLLER_DATA_DEF;

typedef struct
{
	UINT8 Loop;
	UINT8 Control;
	UINT16 Res;
	FLOAT32 Value;
} CONTROLLER_LOOP_CMD;

/************************************************************************/
/* Telemetry Message                                                    */
/************************************************************************/
#define TELE_MSG_DATA_FLAG_BIT_GPS 0x1
#define TELE_MSG_DATA_FLAG_BIT_SENSOR 0x2
#define TELE_MSG_DATA_FLAG_BIT_RAW 0x4
#define TELE_MSG_DATA_FLAG_BIT_MAG 0x8
#define TELE_MSG_DATA_FLAG_BIT_AGL 0x10
#define TELE_MSG_DATA_FLAG_BIT_FUEL 0x20
#define TELE_MSG_DATA_FLAG_BIT_ACT 0x40
#define TELE_MSG_DATA_FLAG_BIT_OTHER 0x80

/************************************************************************/
/* go around reason                                                     */
/************************************************************************/

#define GOAROUND_YTRACK_BIT 0x1
#define GOAROUND_ALT_BIT 0x2
#define GOAROUND_SPEED_BIT 0x4
#define GOAROUND_LOW_ALT_BIT 0x8
#define GOAROUND_HIGH_ALT_BIT 0x10

/************************************************************************/
/* authority reason                                                     */
/************************************************************************/
#define FULL_PERMIT_BIT 0x00
#define DATE_AUTH_PERMIT_BIT 0x01
#define WP_AUTH_PERMIT_BIT 0x02
#define LOCATION_AUTH_PERMIT_BIT 0x04

#define DATE_AUTH_PERMIT_EXPIRED_BIT 0x08 //许可到期
#define LOCATION_AUTH_BREAK_BIT 0x10      //地理越界

typedef struct
{
	UINT8 dataFlags;
	UINT8 NumAct;
	UINT16 Limits;
	UINT32 sysTime;
} MSG_TELEMETRY_HEAD;

typedef struct
{
	INT32 Lat;    //1e7 rad
	INT32 Lon;    //1e7 rad
	INT32 Height; //cm
	INT16 Vnorth; //cm/s
	INT16 Veast;  //cm/s
	INT16 Vdown;  //cm/s
	UINT8 gpsFix;
	UINT8 flags;
	UINT8 pDOP; //5X position DOP
	UINT8 numSV;
	UINT16 GPSWeek;
	UINT32 GPSTow;
} MSG_TELEMETRY_GPS_SUB;

typedef struct
{
	INT16 ROLL;       //1e4 rad
	INT16 PITCH;      //1e4 rad
	UINT16 YAW;       //1e4 rad
	INT16 AltvGPSAlt; //altitude above gps altitude in cm (wangchen modified) change to decimeter
	INT16 TAS;        //cm/s
	INT16 WSouth;     //cm/s
	INT16 WWest;      // cm/s
	UINT16 LeftRPM;
	UINT16 RightRPM;
	UINT8 DensityRatio; //1/200, 1.225 is 200
	char OAT;           //deg
} MSG_TELEMETRY_SENSOR_SUB;

typedef struct
{
	INT16 Xaccel;       //500 m/s/s
	INT16 Yaccel;       //500 m/s/s
	INT16 Zaccel;       //500 m/s/s
	INT16 RollRate;     //5e3 rad/s
	INT16 PitchRate;    //5e3 rad/s
	INT16 YawRate;      // 5e3 rad/s
	UINT16 StaticP;     //0.5 pa
	UINT16 DynPressure; //10 Pa
} MSG_TELEMETRY_RAW_SUB;

typedef struct
{
	INT16 XMagFiled;
	INT16 YMagFiled;
	INT16 ZMagFiled;
	UINT16 Compass;
} MSG_TELEMETRY_MAG_SUB;

typedef struct
{
	INT16 FuelFlow;
	INT16 Fuel;
} MSG_TELEMETRY_FUEL_SUB;

/************************************************************************/
/* Low res telemetry packet structure                                   */
/************************************************************************/

typedef struct
{
	INT8 ROLL;       //deg
	INT8 PITCH;      //deg
	UINT8 YAW;       //deg
	INT8 AltvGPSAlt; //0.5m altitude above gps altitude in cm
	INT8 TAS;        //m/s
	INT8 WSouth;     //m/s
	INT8 WWest;      // m/s
	UINT8 LeftRPM;
	UINT8 RightRPM;
	UINT8 DensityRatio; //1/200, 1.225 is 200
	INT8 OAT;           //deg
} MSG_TELEMETRY_LO_SENSOR_SUB;

typedef struct
{
	INT8 Xaccel;        //500 m/s/s
	INT8 Yaccel;        //500 m/s/s
	INT8 Zaccel;        //500 m/s/s
	INT8 RollRate;      //5e3 rad/s
	INT8 PitchRate;     //5e3 rad/s
	INT8 YawRate;       // 5e3 rad/s
	UINT16 StaticP;     //0.5 pa
	UINT16 DynPressure; //10 Pa
} MSG_TELEMETRY_LO_RAW_SUB;

typedef struct
{
	INT8 XMagFiled; //0.01 gauss
	INT8 YMagFiled; //0.01 gauss
	INT8 ZMagFiled; //0.01 gauss
	UINT8 Compass;  //deg
} MSG_TELEMETRY_LO_MAG_SUB;

/************************************************************************/
/* System_status message                                                */
/************************************************************************/
typedef struct
{
	UINT8 MPowerALo;
	UINT8 MPowerAHi : 4;
	UINT8 MPowerVLo : 4;
	UINT8 MPowerVHi;
	UINT8 SPowerALo;
	UINT8 SPowerAHi : 4;
	UINT8 SPowerVLo : 4;
	UINT8 SPowerVHi;
	UINT8 InternalV;
	INT8 BoardT;
	INT8 RSSI;
	UINT8 VSWR;

	//	UINT8 errSensor;
	UINT8 dsExIMU : 1;
	UINT8 dsExGPS : 1;
	UINT8 dsExAir : 1;
	UINT8 dsExMag : 1;
	UINT8 dsRes : 4;
	/*
		UINT8 errGrosX : 1;	//!< Set if grosX is bad
		UINT8 errGrosY : 1;	//!< Set if grosY is bad
		UINT8 errGrosZ : 1;	//!< Set if grosZ is bad
		UINT8 errAcclX : 1;	//!< Set if acclX is bad
		UINT8 errAcclY : 1;	//!< Set if acclY is bad
		UINT8 errAcclZ : 1;	//!< Set if acclZ is bad
		UINT8 errStaticP:1;    //!< Set if StaticP is bad
		UINT8 errDynamicP:1;
	*/
	UINT8 Res1;
	UINT16 NavHealth;
	INT16 HorizStdDev;
	UINT16 VertStdDev;
	INT8 RollBias;
	INT8 PitchBias;
	INT8 YawBias;
	INT8 XaccBias;
	INT8 YaccBias;
	INT8 ZaccBias;
#if 0
	INT8 XmagBias;
	INT8 YmagBias;
	INT8 ZmagBias;
#else
	INT16 PhotoNum;
	INT8 altControlSource;
#endif
	INT8 Res2;
	UINT8 GlobalStatus;
	UINT8 Failure;
	UINT16 Action;
	UINT16 Tracker;
	UINT16 TrackerStatus;
	UINT8 OrbitRadius;
	UINT8 NumLoops;
	UINT16 LoopStatus;
	FLOAT32 lpTarget[8];
	UINT16 P_Sol_Status;
	UINT16 Pos_Type;
	UINT16 P_X_Sigma;
	UINT16 P_Y_Sigma;
	UINT16 P_Z_Sigma;
	UINT16 V_Z_Sigma;
	//	UINT   Ytracker;
	UINT8 Ext_Sol_Stat;
	UINT8 Sig_Mask;
	UINT16 Flag;
} MSG_SYSTEM_STATUS;

/************************************************************************/
/* NAVIGATOR KALMAN FILTER DEBUG MSG                                    */
/************************************************************************/
typedef struct
{
	UINT32 msTime;  //in ms
	INT32 stPosX;   //cm
	INT32 stPosY;   //cm
	INT32 stPosZ;   //cm
	INT16 stVNorth; //cm/s
	INT16 stVEast;  //cm/s
	INT16 stVDown;  //cm/s
	INT16 stQ0;     //1e4
	INT16 stQ1;     //1e4
	INT16 stQ2;     //1e4
	INT16 stQ3;     //1e4

	INT16 stAxBias; //1e4
	INT16 stAyBias; //1e4
	INT16 stAzBias; //1e4
	INT16 stPBias;  //1e4
	INT16 stQBias;  //1e4
	INT16 stRBias;  //1e4

	INT16 stAltBias; //dm
	INT16 stWn;      //cm/s
	INT16 stWe;      //cm/s

	INT16 ssXaccel;    //5e2 m/s/s
	INT16 ssYaccel;    //5e2 m/s/s
	INT16 ssZaccel;    //5e2 m/s/s
	INT16 ssRollRate;  //5e3 rad/s
	INT16 ssPitchRate; //5e3 rad/s
	INT16 ssYawRate;   // 5e3 rad/s

	INT32 smGPSX;   //cm
	INT32 smGPSY;   //cm
	UINT16 smGPSZ;  //dm
	INT16 smVNorth; //cm/s
	INT16 smVEast;  //cm/s
	INT16 smVDown;  //cm/s

	UINT16 smAlt; //dm
	INT16 smTAS;  //cm/s
} MSG_DEBUG_NAVKALMAN;

typedef struct
{
	UINT32 msTime; //in ms
	double stQ;
	double stCm0;
	double stCma;
	double stCmq;
	double stCmde;
	double stP;
	double stClp;
	double stCln;
	double stClda;
	double stCldr;
	double stR;
	double stCnp;
	double stCnr;
	double stCnda;
	double stCndr;
} MSG_DEBUG_ESTIMATOR;

/************************************************************************/
/* CONTROLLER DEBUG MSG                                                 */
/************************************************************************/
typedef struct
{
	UINT32 msTime;
	UINT16 stLoop;

	UINT16 cmdTrack;
	INT16 cmdHeading; //0~2PI 1e4 rad/s
	INT16 cmdBank;    //1e4 rad/s
	UINT16 cmdAlt;    //dm
	INT16 cmdVrate;   //cm/s
	INT16 cmdTAS;     //cm/s
	INT16 cmdFlap;    //1e4 rad

	INT16 actCmdAileron; //1e4 rad
	INT16 actCmdElv;     //1e4 rad
	INT16 actCmdThrot;   //1e4
	INT16 actCmdRudder;  //1e4 rad
	INT16 actCmdFlap;    //1e4 rad

	INT16 fsTAS;       //cm/s
	INT16 fsTASdot;    //cm/s/s
	UINT16 fsAlt;      //dm
	INT16 fsVrate;     //cm/s
	INT16 fsVertAcc;   //cm/s/s
	INT16 fsRoll;      //1e4 rad
	INT16 fsPitch;     //1e4 rad
	UINT16 fsYaw;      //1e4 rad
	INT16 fsRollRate;  //5e3 rad
	INT16 fsPitchRate; //5e3 rad
	INT16 fsYawRate;   //5e3 rad
} MSG_DEBUG_CONTROLL;

typedef struct
{
	INT16 cmdTarget;
	INT16 target;
} CONTROL_TARGET;

typedef struct
{
	UINT16 type;
	CONTROL_TARGET debug[20];
} MSG_DEBUG_CONTROLL_2;

typedef struct
{
	FLOAT32 MagX;
	FLOAT32 MagY;
	FLOAT32 MagZ;
	FLOAT32 XAccel;
	FLOAT32 YAccel;
	FLOAT32 ZAccel;
} MSG_MAG_SENSORS_CALIB_DATA;

typedef struct
{
	INT16 RollRate;  //1e4 rad
	INT16 PitchRate; //1e4 rad
	INT16 YawRate;   //1e4 rad
	INT16 XAccel;    //2e2
	INT16 YAccel;    //2e2
	INT16 ZAccel;    //2e2
	INT16 DynamicP;
	UINT16 StaticP; //2
} DOUBLET_SENSOR_DATA;

typedef struct
{
	UINT16 SampleIndex;
	UINT16 SampleRate;
	UINT16 TotalSamples;
	DOUBLET_SENSOR_DATA SensorData[5];
} MSG_DOUBLET_SENSOR_DATA;

typedef struct
{
	UINT16 LeftRPM;
	UINT16 RightRPM;
	INT16 ROLL;     //1e4 rad
	INT16 PITCH;    //1e4 rad
	UINT16 YAW;     //1e4 rad
	INT16 Aileron;  //1e4 rad
	INT16 Elevator; //1e4 rad
	UINT16 Throtle; //1e4
	INT16 Rudder;   //1e4 rad
	INT16 Flap;     //1e4 rad
} DOUBLET_AP_DATA;

typedef struct
{
	UINT16 SampleIndex;
	UINT16 SampleRate;
	UINT16 TotalSamples;
	DOUBLET_AP_DATA ApData[5];
} MSG_DOUBLET_AP_DATA;

typedef struct
{
	UINT8 Flags;
	UINT8 Duration;
	UINT16 Pulse;
	INT16 Center;
	INT16 Delta;
} MSG_DOUBLET_CMD;

/************************************************************************/
/* GCS command msg                                                      */
/* add by Ren Bin  2008/12/16                                           */
/************************************************************************/
typedef struct
{
	UINT8 power;
	UINT8 RSSI;
} MSG_RADIO_CONFIG;

typedef struct
{
	UINT16 Aileron;  // chanel 0副翼
	UINT16 Elevator; //升降舵 chanel 1
	UINT16 Throttle; //遥控器油门/风门滑条 chanel 2
	UINT16 Rudder;   // chanel 3
	UINT8  Manual;   //=1 manual; =0 auto; = 3 面板优先	chanel 4
	UINT8  IPB;      //
	UINT16 Flap;     // chanel 5
	UINT8  IPBbak;
	UINT8  InstructionL;
	UINT8  InstructionC;
	UINT8  InstructionR;
} OPERATOR_CMD;

#define APMODE_NO_POSITION (0) //! < 无位置信息
#define APMODE_TAKEOFF (1)     //! < 起飞
#define APMODE_TRACK (2)       //! < 位置信息
#define APMODE_LAND (3)        //! < 降落

typedef struct
{
	UINT32 ApMode;
	INT32 lon;       //!< 1e7 deg
	INT32 lat;       //!< 1e7  deg
	INT32 hight;     //!< 1e3  m
	INT16 yaw;       //!< 1e1   deg,0~360
	INT16 pitch;     //!< 1e1  deg
	INT16 roll;      //!< 1e1   deg
	INT32 Vground;   //!< 1e2  m/s
	UINT32 loc_time; //!< 1000*(H*3600+M*60+S)+MS
} AP_RT_TRACK_MSG;

typedef struct
{
	UINT32 ApMode;
	INT32 lon;       //!< 1e7 deg
	INT32 lat;       //!< 1e7  deg
	INT32 hight;     //!< 1e3  m
	INT32 Vground;   //!< 1e2  m/s
	UINT32 loc_time; //!< 1000*(H*3600+M*60+S)+MS
	INT16 yaw;       //!< 1e1   deg,0~360
	INT16 pitch;     //!< 1e1  deg
	INT16 roll;      //!< 1e1   deg
} MSG_AP_RT_TRACK_MSG;

typedef struct
{
	FLOAT32 panCmd;   //!< 航向指令
	FLOAT32 TiltCmd;  //!< 俯仰指令
	INT32 Zoom;       //!< 20为放大，-20为缩小，0为不变
	FLOAT32 Euler[3]; //!< 基座三轴欧拉角
	UINT8 Mode;       //!< 0 航向锁定模式，1姿态角模式
	UINT8 Rev[3];
} GIMBAL_GP_CMD_t;

/* 转给吊舱 */
typedef struct
{
	INT16    Euler[3];		//!< 1E4，基座三轴欧拉角，单位弧度
	UINT8    SeroveCmd;		//!< 0 ，伺服机构指令模式，0角速率模式，1姿态角模式
	UINT8    OptoSensorCmd;	//!< 光学传感器指令
	UINT16   CamaraPara;	//!< 光学机构指令参数
	INT16    ServoCmd0;		//!< 1E4， 航向指令，单位rad,rad/s
	INT16    ServoCmd1;		//!< 1E4，俯仰指令，单位rad,rad/s
} GIMBAL_GP_CMD_100_t;

typedef struct
{
	FLOAT32 pan;
	FLOAT32 Tilt;
} GIMBAL_MG_CMD_Back_t;

/* 返回给飞控的 */
typedef struct
{
	INT16   pan;		//!<航向欧拉角，1e4,单位弧度
	INT16   Tilt;		//!<俯仰欧拉角，1e4,单位弧度
	UINT16  ViewAngle;	//!<视场角,1e4,单位弧度
	INT16   Framepan;	//!<航向欧拉角，1e4,单位弧度
	INT16   FrameTilt;	//!<俯仰欧拉角，1e4,单位弧度
	UINT8   TrackStatus;//!<跟踪状态，0x00：锁定 ；0x01:脱锁，进入速率半自主；0x02:跟踪状态不佳，但仍是跟踪态，仅关闭RTT;
	UINT8   DetRltStatus;//0x00:未发现目标；0x01:发现目标。简单逻辑：若飞机当前处于RTT模式，无论有没有发现目标，均发送0x00;若飞机当前未处于RTT模式，当检测到目标时，发送0x01,未检测到目标时，发送0x00;//
	INT32	DemOutLat;  //高程查找结果
	INT32	DemOutLon;
	INT32	DemOutAlt;  //单位cm
	INT16	Subx;		//X像素差 *10000(mm)
	INT16	Suby;		//Y像素差 *10000(mm)
	UINT16	Focus;		//焦距 *100（mm）
	INT16	GimbalSelfCheck;
} GIMBAL_MG_CMD_100_Back_12_t;

/* 吊舱返回 */
typedef struct
{
	INT16   pan;		//!<航向欧拉角，1e4,单位弧度
	INT16   Tilt;		//!<俯仰欧拉角，1e4,单位弧度
	UINT16  ViewAngle;	//!<视场指令值
	INT16   Framepan;	//!<航向欧拉角，1e4,单位弧度
	INT16   FrameTilt;	//!<俯仰欧拉角，1e4,单位弧度
	INT16   DeltaPan;	//!<航向增量，1e4,单位弧度
	INT16   DeltaTilt;	//!<俯仰增量，1e4,单位弧度
	UINT16  GMPower;	//吊舱电源 V, *50
	UINT32  DeltaTime; //!<视场指令值2
} GIMBAL_MG_CMD_100_Back_t;

//外部指令数据包
typedef struct
{
	UINT32 UserID; //用户ID，表明控制请求用户身份，ID取值需大于100。用户可申请占用某个ID。
	UINT32 Time;   //消息时戳，表示指令包数据来源的产生时刻点
	UINT8  Task; 	//任务模式(状态值)：0.无任务；1.任务1；2.任务2 以此类推
	UINT8  Sensorcmd;//吊舱传感器控制指令;吊舱传感器控制指令和参数说明见表一
	UINT16  SensorPara1;//吊舱传感器控制指令参数1;吊舱传感器控制指令和参数说明见表一
	UINT16  SensorPara2;//吊舱传感器控制指令参数2;吊舱传感器控制指令和参数说明见表一
	UINT8 AiCmd;//机载AI控制指令；AI控制指令说明见表二
	UINT8 ServoCmd;//吊舱伺服控制指令；伺服控制指令和参数说明见表三
	INT32  ServoCmdPara1;//吊舱伺服控制指令参数1；伺服控制指令和参数说明见表三
	INT32  ServoCmdPara2;//吊舱伺服控制指令参数2；伺服控制指令和参数说明见表三
	INT32  ServoCmdPara3;//吊舱伺服控制指令参数3；伺服控制指令和参数说明见表三
	UINT8 Res;//保留字节（用于四字节对齐）
	UINT8 FlyCmd;//无人机飞行引导指令；无人机飞行引导指令和参数说明见表四
	UINT16 FlyCmdPara1;//无人机飞行引导指令参数1；无人机飞行引导指令和参数说明见表四
	UINT32 FlyCmdPara2;//无人机飞行引导指令参数2；无人机飞行引导指令和参数说明见表四
	UINT32 FlyCmdPara3;//无人机飞行引导指令参数3；无人机飞行引导指令和参数说明见表四
	UINT8 VideoCmd;//流媒体控制指令；指令和参数说明见表5
	UINT8 VideoCmdParaNum;//流媒体控制参数数量，指令和参数说明见表5
	//UINT8 VideoCmdPara[30];//参数为30个字节的ASCII码，用于存储视频推流地址和推流格式等
	UINT16 VideoCmdPara;
} PodExtraCtrol;

//遥测数据组播
typedef struct
{
	UINT8 GpsYear;  //GPS时间：年
	UINT8 GpsMonth;//GPS时间：月
	UINT8 Gpsday;//GPS时间：日
	UINT8 GpsHour;//GPS时间：时
	UINT8 GpsMinute;//GPS时间：分
	UINT8 GpsSecond;//GPS时间：秒
	UINT8 UTC;//时区

	UINT32 UserID;   	  //发布任务的用户ID
	UINT8  TaskPlan; 	  //任务规划(状态值)：0.无任务；1.任务1；2.任务2 以此类推
	UINT8  CorrentTask;	  //当前任务：0.无任务；1.任务1；2.任务2； 以此类推
	UINT16 Result;	  	  //任务执行结果：0.无结果；1.任务成功 ; 2.任务失败。
} UsvTask;

typedef struct
{
	UINT32 UsvID;   	 //飞机编号。
	UINT32 UsvPosTime;  //飞机位置时戳
	INT32 UsvPosLon;   //飞机位置: 经度  单位 1e7 * 度 (精确到小数点后7位)
	INT32 UsvPosLat;   //飞机位置：纬度  单位 1e7 * 度 (精确到小数点后7位)
	INT32 UsvPosAlt;   //飞机位置：海拔高度 单位 100 * 米(精确到厘米)
	INT32 UsvAttHead;  //飞机姿态: 航向角 单位 1e4 * 度
	INT32 UsvAttPitch; //飞机姿态：俯仰角 单位 1e4 * 度
	INT32 UsvAttRoll;  //飞机姿态：滚转角 单位 1e4 * 度
	UINT8 WaypointsID; //飞机飞行引导航点编号
	UINT8 WaypointsType;//飞机飞行引导航点类型：0 .无引导航点  1.预规划航线任务区航点。 2.快速飞行计划：临时航点。
	INT16 WaypointsAlt;//飞机飞行引导航点位置：高度 单位 100 * 米 (精确到厘米)
	INT32 WaypointsLon;//飞机飞行引导航点位置：经度 单位 1e7 * 度 (精确到小数点后7位)
	INT32 WaypointsLat;//飞机飞行引导航点位置：纬度 单位 1e7 * 度 (精确到小数点后7位)
	UINT32 FlyReqUserID;//非预设航点飞行引导请求用户ID
	UINT32 FlyReqTime;//非预设航点飞行引导请求时戳
	UINT32 FlyReqBackTime;//非预设航点飞行引导请求状态反馈时戳
	UINT8 FlyReqResult;//非预设航点飞行引导请求状态 0.无引导  1.请求成功  2.请求失败
	UINT8 Res1;	//保留字节1
	UINT16 Res2;  //保留字节2
} UsvSituation;

#if 0
typedef struct
{
	INT16    Euler[3];
	UINT8    SeroveCmd;
	UINT8    OptoSensorCmd;
	UINT16   CamaraPara;
	INT16    ServoCmd0;
	INT16    ServoCmd1;
	UINT8    year;
	UINT8    month;
	UINT8    day;
	UINT8    hour;
	UINT8    minute;
	UINT8    second;
	INT32    lat;
	INT32    lon;
	INT16    HMSL;
	UINT16   VGnd;
	UINT16  Tas;
	UINT8   pdop;
	UINT8   numSV;
}GIMBAL_GP_CMD_ext_t;

#endif

typedef struct
{
	INT16    ServoCmd0;
	INT16    ServoCmd1;

	UINT16   ViewAngle;	//!<视场指令值//
	UINT16   FrameGap;

	INT16	Subx;		//X像素差
	INT16	Suby;		//Y像素差

	INT16   KpPan;		//角度、速率控制比例增益缩放  <1E3
	INT16   KpTilt;		//<1E3

	INT16   KdPan;		//角度控制微分增益缩放    <1E3
	INT16   KdTilt;		//<1E3

	INT16   KiPan;		//角度控制积分增益缩放    <1E3
	INT16   KiTilt;		//<1E3

	INT16   delta_kppan; //<1E3
	INT16   delta_kptilt;//<1E3

	INT16   delta_kdpan;//<1E3
	INT16   delta_kdtilt;//<1E3

	INT16   delta_kipan;//<1E3
	INT16   delta_kitilt;//<1E3

	INT16   delta_PanCom;//<1E3
	INT16   delta_TiltCom;//<1E3

	//UINT8   framenum;
	//UINT8   PID_ctrl;

	UINT8   res[16];

} GIMBAL_PID;

// 来自飞控的消息（102号协议）
typedef struct
{
	INT16    Euler[3];		//!< 1E4，基座三轴欧拉角，单位弧度
	UINT8    SeroveCmd;		//!< 0 ，伺服机构指令模式，0角速率模式，1姿态角模式
	UINT8    OptoSensorCmd;	//!<  光学传感器指令

	UINT16   CamaraPara;	//!<光学机构指令参数
	INT16    ServoCmd0;		//!< 1E4， 航向指令，单位rad,rad/s

	INT16    ServoCmd1;		//!< 1E4，俯仰指令，单位rad,rad/s
	UINT8    year;			//!< Integer year value 0代表2000年.
	UINT8    month;			//!< Integer month of year 1-12.

	UINT8    day;			//!< Integer day of month 1-31.
	UINT8    hour;			// 时  utc时间
	UINT8    minute;		//分
	UINT8    second;		//秒    

	INT32    lat;			//飞机纬度   度  1e7
	INT32    lon;			//飞机经度   度， 1e7

	INT16    HMSL;			//飞机高度   cm  
	UINT16   VGnd;			//地速       m/s 1e1

	UINT16  Tas;			//空速        m/s 1e1

	UINT8   pdop;			//卫星精度  1e1
	UINT8   numSV;			//卫星颗  

	INT32    TGTlat;		//目标纬度   度  1e7
	INT32    TGTlon;		//目标经度   度， 1e7

	INT16    TGTHMSL;		//目标高度   cm

	INT16    ModeState;		//吊舱工作模式

	UINT16  SlantR;			//飞机目标估距
	UINT16  RPM;			//发动机转速  /60

	INT16   Vnorth;			//地速（北） m/s，1e2
	INT16   Veast;			//地速（东） m/s，1e2

	INT16   Vdown;			//地速（地） m/s，1e2
	UINT8   ThrottleCmd;	//油门指令， 1e2
	UINT8   DefectFlag;		//自毁标志
	/*
	if（DefectFlag == 0x0D
	{
		PreDefectFlag = 1;
		//图像显示预自毁
	}
	else if ((DefectFlag == 0x5D) && (preDefectFlag))
	{
		//图像显示自毁后自毁
		PreDefectFlag = 0;
	}
	else
	{
		PreDefectFlag = 0；
	}
	*/

	UINT16  MPowerA;		//动力电源 V, *50
	UINT16  MpowerV;		//主电源 V, *100

	INT32	DemSearchLat;   //高程查找数据 *1e7
	INT32	DemSearchLon;
	INT32	DemSearchAlt;

	UINT32  DisFromHome;	//离家距离
	INT16	HeadFromHome;	//离家方位角 * 100 （rad）
	UINT16  TargetVelocity; //目标速度 * 100


	INT16	TargetHeading;	//目标速度方位角 * 100 (rad)
	UINT8	ScoutTask;		//搜索任务，具体解析见后面表格
	UINT8	apmodestates;	//具体解析见文档

	UINT16	TasCmd;			//空速指令 *10
	INT16	HeightCmd;		//高度指令 （m）

	INT16	WypNum;
	INT8	VeclType;		//0：电机 1：油机
	INT8	GimbalDeployCmd;//0:收起 1:放下

	INT32   HMSLCM;//飞机飞行高度，单位cm//
} GIMBAL_GP_CMD_102_t;

#if 0
typedef struct
{
	INT16    Euler[3];
	UINT8    SeroveCmd;
	UINT8    OptoSensorCmd;
	UINT16   CamaraPara;
	INT16    ServoCmd0;
	INT16    ServoCmd1;

	UINT8   RateControl;
	UINT8   FilterStatus;

	INT16   KpPan;
	INT16   KpTilt;
	INT16   KdPan;
	INT16   KdTilt;
	INT16   KiPan;
	INT16   KiTilt;

	UINT16   ThresValueScale;
	UINT16   AngleThreshold;
	UINT8   FrameGap;
	UINT16   GimbalPool;
}GIMBAL_GP_CMD_103_t;

#endif

typedef struct
{
	UINT8   RateControl;		//速率控制(1:速率控制 0；角度控制)
	UINT8   FilterStatus;		//滤波(1:滤波 0:取消滤波)

	INT16   KpPan;				//角度、速率控制比例增益缩放  <1E3
	INT16   KpTilt;				//<1E3
	INT16   KdPan;				//角度控制微分增益缩放    <1E3
	INT16   KdTilt;				//<1E3
	INT16   KiPan;				//角度控制积分增益缩放    <1E3
	INT16   KiTilt;				//<1E3

	UINT16   ThresValueScale;	//速率阈值增益     <1E3
	UINT16   AngleThreshold;	//角度控制阈值 (Rad)  <1E3
	UINT16   GimbalPool;		//吊舱控制阈值 (Rad)  <1E3   
	UINT8   FrameGap;			//帧间隔
	UINT8   Res;                //预留
	UINT16  Res1;				//预留1
} GIMBAL_GP_CMD_103_t;

typedef struct
{
	std::uint16_t   DetLefttopX;	// !<跟踪框左上角X坐标++++++++++
	std::uint16_t   DetLefttopY;	// !< 跟踪框左上角Y坐标+++++++++++
	std::uint16_t   DetWidth;		// !<跟踪框宽度++++++++++
	std::uint16_t   DetHeight;  	// !< 跟踪框高度++++++++++

	std::int16_t    DetTGTclass;    // !<目标类别 ，0-person(人) 1-auto（车）2-boat（船）3-fire（火）
	std::int16_t   TgtConfidence;  // !目标类别置性度 ，1e2

	std::int32_t  TrackId;		 //目标序列号

	std::uint32_t IFF;           //目标敌我属性：0-未知 1-敌 2-我

	std::int32_t    DetTGTlat;			// !<目标纬度   度  1e7
	std::int32_t    DetTGTlon;			// !<目标经度   度， 1e7
	std::int16_t    DetTGTHMSL;		// !<目标高度    m
	std::uint16_t  TgtVelocity; // !<目标速度，m/s，1e2
} DETCDATA;

typedef struct
{
	INT16 IR_thermometry_ID;       //目标编号
	UINT8 IR_thermometry_width;   //目标像素高度
	UINT8 IR_thermometry_height;  //目标像素高度

	INT16 IR_thermometry_pixelX;  //目标位置中心点坐标X
	INT16 IR_thermometry_pixelY;  //目标位置中心点坐标Y


	INT16 IR_thermometry_maxT;    //区域最高温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
	INT16 IR_thermometry_minT;    //区域最低温度（单位摄氏度，精度0.1摄氏度，全F表示无效）

	INT16 IR_thermometry_centreT; //区域中心点温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
	INT16 IR_thermometry_meanT;  //区域平均温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
} Targettemp;

typedef struct
{
	std::uint8_t LaserStatus;  /*激光测距机的状态;0:表示无效；1：单次测距；2：连续测距；3：关闭*/
	std::uint8_t LaserMeasStatus;  /*激光测距异常态监测：bit0---FPGA系统状态，bit1-激光出光状态，bit2-主波监测状态，bit3-回波检测状态，bit4-偏压开关状态，bit5-偏压输出状态，bit6-温度状态，bit7-出光关断状态，上述状态位中1表示正常，0表示异常*/
	std::uint16_t  LaserMeasVal;   /*激光测距值,当测距无异常时，此值为激光测距值,单位米，否则显示超量程*/
	std::int32_t Laserlat;/*激光测距点纬度值,单位度，1E7*/
	std::int32_t Laserlon;/*激光测距点经度值，单位度，1E7*/
	std::int32_t LaserHMSL;/*激光测距点高度值，单位米*/
	std::uint8_t LaserRefStatus;/*激光测距靶点状态；0：无效；1：表示估距，2：表示激光测距*/
	std::uint8_t LaserCurStatus;/*激光测距弹着点状态；0：无效；1：表示估距，2：表示激光测距*/
	std::uint8_t LaserModel;/*测距结果模式，0x00:测距结果为单目标，0x01:测距结果为前目标；0x02:测距结果为后目标；0x03:测距结果为前目标和后目标；0x04:测距结果为超距*/
	std::uint8_t LaserFreq;/*若测距模式为连续测距，则此值为连续测距频率；若测距模式为单次测距，则此值为单次测距个数*/
	std::int32_t LaserRefLat;/*激光测距靶点纬度值,单位度，1E7*/
	std::int32_t LaserRefLon;/*激光测距靶点经度值,单位度，1E7*/
	std::int32_t LaserRefHMSL;/*激光测距靶点高度值，单位米*/
	std::int32_t LaserCurLat;/*激光测距弹着点纬度值,单位度，1E7*/
	std::int32_t LaserCurLon;/*激光测距弹着点经度值,单位度，1E7*/
	std::int32_t LaserCurHMSL;/*激光测距弹着点高度值，单位米*/
	std::uint16_t LaserDist;/*激光测距靶点与弹着点的距离，单位米*/
	std::uint16_t LaserAngle;/*激光测距靶点与弹着点的角度，单位弧度，1E4*/
	std::uint16_t res1;/*保留1 */
	std::int16_t res2;/*保留2 */
} LASER_STATUS_t;

// 发到地面的消息协议
typedef struct
{
	//添加吊舱跟踪控制调试信息//
	std::int32_t deltaFrame_pan; //!<偏航方向偏差量，1e4,单位弧度；
	std::int32_t KP_pan; //!<偏航方向KP参数，1e2；
	std::int32_t KI_pan; //!<偏航方向KI参数，1e2；
	std::int32_t KD_pan; //!<偏航方向KD参数，1e2；
	std::int32_t Compensate_pan; //!<偏航方向补偿量，1e4,单位弧度；
	std::int32_t KP_pan_Rate; //!<偏航KP分量，1e4,单位弧度；
	std::int32_t KI_pan_Rate; //!<偏航KI分量，1e4,单位弧度；
	std::int32_t KD_pan_Rate; //!<偏航KD分量，1e4,单位弧度；
	std::int32_t PID_pan_Rate; //!<偏航PID分量总和，1e4,单位弧度；
	std::int32_t pan_Rate; //!<偏航分量控制速率总和，1e4,单位弧度；

	std::int32_t deltaFrame_tilt; //!<俯仰方向偏差量，1e4,单位弧度；
	std::int32_t KP_tilt; //!<俯仰方向KP参数，1e2；
	std::int32_t KI_tilt; //!<俯仰方向KI参数，1e2；
	std::int32_t KD_tilt; //!<俯仰方向KD参数，1e2；
	std::int32_t Compensate_tilt; //!<俯仰方向补偿量，1e4,单位弧度；
	std::int32_t KP_tilt_Rate; //!<俯仰KP分量，1e4,单位弧度；
	std::int32_t KI_tilt_Rate; //!<俯仰KI分量，1e4,单位弧度；
	std::int32_t KD_tilt_Rate; //!<俯仰KD分量，1e4,单位弧度；
	std::int32_t PID_tilt_Rate; //!<俯仰PID分量总和，1e4,单位弧度；
	std::int32_t tilt_Rate; //!<俯仰分量控制速率总和，1e4,单位弧度；
	std::int32_t delta_x;// !<偏航方向，偏离中心点的坐标；
	std::int32_t delta_y;// !<俯仰方向，偏离中心点的坐标；
} GIMBAL_ADJUST_t;

// 发到地面的消息协议
typedef struct
{
	//飞控消息//
	std::int16_t    Euler[3];//!< 1E4，基座三轴欧拉角，单位弧度
	std::uint8_t    SeroveCmd; //!< 0 ，伺服机构指令模式，0角速率模式，1姿态角模式
	std::uint8_t    OptoSensorCmd;//!<  光学传感器指令

	std::uint16_t    CamaraPara;//!<光学机构指令参数
	std::int16_t    ServoCmd0; //!< 1E4， 航向指令，单位rad,rad/s

	std::int16_t    ServoCmd1;//!< 1E4，俯仰指令，单位rad,rad/s
	std::uint8_t    year; //!< Integer year value 0代表2000年.
	std::uint8_t    month; //!< Integer month of year 1-12.

	std::uint8_t    day;   //!< Integer day of month 1-31.
	std::uint8_t    hour; // 时  utc时间
	std::uint8_t    minute; //分
	std::uint8_t    second;   //秒 

	std::int32_t    lat;    //飞机纬度   度  1e7
	std::int32_t    lon;    //飞机经度   度， 1e7

	std::int16_t    HMSL;   //飞机高度   m
	std::uint16_t    VGnd;  //地速       m/s 1e1

	std::uint16_t   Tas;           //空速        m/s 1e1
	std::uint8_t   pdop;          //卫星精度  1e1
	std::uint8_t   numSV;         //卫星颗   

	std::int32_t    TGTlat;      //目标纬度   度  1e7
	std::int32_t    TGTlon;      //目标经度   度， 1e7

	std::int16_t    TGTHMSL;     //目标高度   m 
	std::int16_t    ModeState;  //吊舱工作模式，详见后续解析

	std::uint16_t   SlantR;      //飞机目标估距
	std::uint16_t   RPM;         //发动机转速  /60

	std::int16_t   Vnorth;     //地速（北） m/s，1e2
	std::int16_t   Veast;      //地速（东） m/s，1e2

	std::int16_t   Vdown;      //地速（地） m/s，1e2
	std::uint8_t   ThrottleCmd; //油门指令， 1e2
	std::uint8_t   DefectFlag;  //自毁标志

	std::uint16_t   MPowerA;  //动力电源 V, *50
	std::uint16_t   MpowerV;  //主电源 V, *100

	std::int32_t DemSearchLat;    //高程查找数据 *1e7
	std::int32_t  DemSearchLon;
	std::int32_t  DemSearchAlt;

	std::uint32_t  DisFromHome;  //离家距离
	std::int16_t  HeadFromHome; //离家方位角 * 100 （rad）
	std::uint16_t   TargetVelocity; //目标速度 * 100

	std::int16_t TargetHeading; //目标速度方位角 * 100 (rad)
	std::uint8_t ScoutTask;    //搜索任务，具体解析见后面表格
	std::uint8_t apmodestates; //具体解析见后文

	std::uint16_t  TasCmd;  //空速指令 *10
	std::int16_t HeightCmd;  //高度指令 （m）

	std::int16_t WypNum; //当前跟踪航路点
	std::int8_t	 VeclType;	 //0：cw007 1:cw10 2：cw15 3:cw20 4:cw25 5：cw30 6:CW100
	std::int8_t	 GimbalDeployCmd;	////0:收起 1:放下

	std::int32_t HMSLCM;//飞机飞行高度，单位cm//

	std::int32_t    Wyplat;    //航路点纬度   度  1e7
	std::int32_t    Wyplon;    //航路点经度   度， 1e7

	std::int16_t    WypHMSL;   //航路点高度   m
	std::int16_t HMSLDEM; //飞机所在位置查询的高度  m

	//吊舱消息//
	std::int16_t   pan;		//!<航向欧拉角，1e4,单位弧度
	std::int16_t   Tilt;		//!<俯仰欧拉角，1e4,单位弧度

	std::uint16_t   ViewAngle;	//!<视场角,1e4,单位弧度
	std::int16_t   Framepan;	//!<航向框架角，1e4,单位弧度

	std::int16_t   FrameTilt;	//!<俯仰框架角，1e4,单位弧度
	std::int16_t   DeltaPan;	//!<航向增量，1e4,单位弧度

	std::int16_t   DeltaTilt;	//!<俯仰增量，1e4,单位弧度
	std::uint16_t   GMPower;	//吊舱电源 V, *50

	std::uint32_t  DeltaTime; //!<视场指令值2

	std::int16_t   roll;		//!<滚转框架角，1e4,单位弧度
	std::int16_t   FrameRoll;	//!<滚转框架角，1e4,单位弧度

	//图像处理板消息//

	std::uint16_t   VersionNum;		// !<图像处理板应用程序版本号

	//!<吊舱工作模式
	/*
	0:速率模式
	1:姿态模式
	2:关伺服（休眠使用）
	3:归零位锁定
	4:零位调整
	5:航向扫描
	6:框架角模式
	*/
	std::uint8_t	SeroveCmd_tx;

	/*
	0:初始化未完成 (OFF)
	1:初始化完成(ON)
	*/
	std::uint8_t	SeroveInit;

	std::uint16_t  VisualFOVH;	//!<可见光水平视场角，1e4,单位弧度
	std::uint16_t  VisualFOVV;	//!<可见光垂直视场角，1e4，单位弧度
	std::uint16_t  InfaredFOVH;   //!<红外水平视场角，1e4，单位弧度
	std::uint16_t  InfaredFOVV;	//!<红外垂直视场角，1e4，单位弧度

	std::uint16_t   SearchWidth;    // !<搜索框宽度
	std::uint16_t   SearchHeight;   // !< 搜索框高度

	std::uint16_t   ServoCrossX;	// !<随动X方向坐标
	std::uint16_t   ServoCrossY;	// !<随动Y方向坐标
	std::uint16_t   ServoCrossWidth;// !<随动十字宽度
	std::uint16_t   ServoCrossheight;// !<随动十字高度

	std::uint16_t   TrackLefttopX;	  // !<跟踪框左上角X坐标++++++++++
	std::uint16_t   TrackLefttopY;	  // !< 跟踪框左上角Y坐标+++++++++++
	std::uint16_t   TrackWidth;		// !<跟踪框宽度++++++++++
	std::uint16_t   TrackHeight;	// !< 跟踪框高度++++++++++

	std::uint16_t   SDMemory;		// !<单位G，*10++++++++++++
	std::int8_t		TimeZoneNum;	//!<时区数
	std::uint8_t	SDGainVal;		//!红外相机增益值，*10

	std::uint16_t   SnapNum;		// !<快照数
	std::int16_t	GimbalRoll;		//!<滚转角，1e4,单位弧度

	/*
	OSD状态
	0:完全关闭
	1:完全开启
	2:部分开启：仅开启跟踪框/搜索框，导航点更新标识，检测框，快照标识，所有数据信息均不显示
	*/
	std::uint8_t	OSDFlag;


	/*
	跟踪状态
	0:红外跟踪
	1:可见光跟踪
	2:非跟踪态
	*/
	std::uint8_t	TrackFlag;

	/*
	稳像状态
	0:稳像关闭 (OFF)
	1:稳像开启(ON)
	*/
	std::uint8_t	StableFlag;

	/*
	旋偏纠正
	0:关闭
	1:仅校正选偏角；
	2:校正旋偏与指北
	*/
	std::uint8_t	ImageAdjust;

	/*
	SD卡状态
	0:SD卡正常；
	1:无mmcblk1设备；
	2:mmcblk1设备挂载成功，但删除文件失败，SD卡内存空间小于200M;
	3:mmcblk1设备挂载成功，但删除文件失败，SD卡内存空间大于200M，小于8G;
	4:有mmcblk1设备，但是未挂载成功；
	*/
	std::uint8_t	SDFlag;

	/*
	录制状态
	0:未录制；
	1:录制；
	2:未录制；
	*/
	std::uint8_t	RecordFlag;

	/*
	飞行状态
	0:降落
	1:起飞
	*/
	std::uint8_t	FlyFlag;

	/*
	运动检测
	0:关闭
	1:开启
	*/
	std::uint8_t	MTI;


	/*
	车辆跟踪
	0:关闭
	1:开启
	*/
	std::uint8_t	AI_R; //是否开启车辆跟踪，关闭后进入跟踪时，不检测车辆

	/*
	融合
	0:关闭
	1:开启
	*/
	std::uint8_t	IM;

	/*
	黑热或白热
	0:黑热
	1:白热
	*/
	std::uint8_t	W_or_B;

	/*
	伪彩
	0:黑白
	1:伪彩
	2:无画中画
	*/
	std::uint8_t	IR;

	/*
	高清或标清
	0:标清作为主输出，无高清画中画；
	1:高清作为主输出，标清作为画中画；
	2:标清作为主输出，高清作为画中画；
	*/
	std::uint8_t	HDvsSD;

	/*
	车辆跟踪状态
	1:车辆跟踪
	0:非车辆跟踪
	*/
	std::uint8_t	CarTrack;

	/*
	跟踪类别
	0:Car ;
	1:Truck;
	2:Bus;
	3:not
	*/
	std::uint8_t	TrackClass;

	/*
	视场角是否锁定
	0:未锁定
	 1：锁定
	*/
	std::uint8_t FovLock;

	std::uint32_t   FlySeconds;//飞机飞行seconds//

	DETCDATA DetRect[MAX_DETECT_NUM]; //检测的目标框//

	std::uint32_t Encryption;  //!<加密flag,详见后面表格

	std::uint32_t  DetectionFlag; //检测flag,详见后面表格

	std::uint16_t DetectionNum; //目标检测到的数量 //
	std::uint8_t   TrackStatus; //!<跟踪状态//
	std::uint8_t   DetRltStatus; //0x00:未发现目标；0x01:发现目标//
	std::uint32_t visualfovhmul; //可见光机芯倍率
	std::uint32_t infaredfovhmul; //红外机芯倍率
	LASER_STATUS_t LaserData;//激光测距状态及数据；//

	GIMBAL_ADJUST_t GimbalAdjData;

	int IrRangeT_num;//红外测温目标数量
	Targettemp TargetRect[MAX_DETECT_NUM];//红外测温

	INT8 LaserWorkStatus; //激光器工作状态
	UINT32 GR; //地理分辨率（每个像素代表的地理尺寸，单位：cm）
} FRAME_POS_t;

typedef struct
{
	UINT16  Ver; //协议版本号//
	UINT16  MSGID;//消息ID//
	UINT8  SEQ;//消息序号//
	UINT8  ACK;//消息回复//
	UINT16  TargetX; //跟踪图像坐标X//
	UINT16  TargetY;//跟踪图像坐标Y//
	UINT16  TargetRectWidth;//跟踪图像范围//
	UINT16  TargetRectHeight;//跟踪图像范围//
	INT16  TrackFlag;//0x00:无效；0x01:进跟踪；0x02:退跟踪//
} TrackingTarget_t;//手动框选跟踪框//

typedef struct
{
	UINT16  Ver; //协议版本号//
	UINT16  MSGID;//消息ID//
	UINT8  SEQ;//消息序号//
	UINT8  ACK;//消息回复//
	UINT16  FormatFlag; //SD卡格式化标志，0x01表示格式化，0x02表示清空；//
	UINT16  Restart;//重启图像处理板，0x01表示重启；//
	UINT8    FrmCtl;//!< 框架角锁定指令//
	UINT8    Res0;//!< 保留//
	INT16   PanPara;//!<框架角锁定偏航角值//
	INT16   TiltPara;//!<框架角锁定俯仰角值//
} FormatSD_t;//图像处理板格式化，重启，框架角锁定//

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT16  Switch; //巡检模式总开关，0x00表示无效，0x01表示开启，0x02表示关闭；
	UINT16  FOVMode;//FOV模式设置，0x00表示无效，0x01表示预设模式，0x02表示手动模式,0x03表示预设值更新模式（单次触发）； 
	UINT8    FOVValueChange;//!<FOV预设调整,0x00表示无效，0x01表示巡航FOV预设调整，0x02表示识别FOV预设调整（仅在FOVMode为03有效）
	UINT8    FOVModeChange;//!<FOV预设调整,0x00表示无效，0x01表示FOV模式切换为巡航，0x02表示FOV模式切换为识别。
	INT16   rev1;//!<
	INT16   rev2;//!<
} Targetonoff_t;//

typedef struct
{
	UINT16  Ver; //Э��汾��
	UINT16  MSGID;//��ϢID=0x09
	UINT8  SEQ;//��Ϣ���
	UINT8  ACK;//��Ϣ�ظ�
	UINT16  EO_Fov_Value; //�ɼ����ӳ����趨ֵ����λ1e-2
	UINT16  Value1;// ����1 
	UINT16  Value2;// ����  
	INT16   Value3;//!< ����
	INT16   Value4;//!< ����
} GimbalExEOParam_t;//

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID---0x0a
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT16  TargetX; //跟踪图像坐标X
	UINT16  TargetY;//跟踪图像坐标Y
	UINT16  TargetRectWidth;//跟踪图像范围
	UINT16  TargetRectHeight;//跟踪图像范围
	INT16   TrackFlag;//0x00:无效；0x01:进跟踪；0x02:退跟踪//;
	UINT16  FOV;	//指定视场角度, 10倍
	UINT16  TYPE; //跟踪目标类别
	INT32  INDEX; //目标编号
} JACX_GIMBAL_TRACK_ZOOM;

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID，详见表2 MSGID 说明
	UINT8  SEQ;//消息序号，
	UINT8  ACK;//消息回复
	UINT8   EncodeHDSend; //!< 0x00:表示无效；0x01：表示H264;0x02:表示H265;
	UINT8  HDSendprofile; //!< H264编码时：0->baseline,1->mainfile,2->hp,3->svc_t；H265编码时：0-> mainfile；注：profile需与EncodeFormat变量配对使用，否则可能导致吊舱编码板应用程序无法正常使用。
	UINT8    BitRateHDSend; //!< 0x00:表示无效；取值范围为1~6，表示可见光发送编码参数为BitRateHDSend *1024;
	UINT8 Res1; //!<  保留
	UINT16   Res2; //!<  保留
} GIMBAL_ENCODEHDSEND_t;

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT8   EncodeHDWrite; //!< 0x00:表示无效；0x01：表示H264;0x02:表示H265;
	UINT8  HDWriteprofile; //!< H264编码时：0->baseline,1->mainfile,2->hp,3->svc_t；H265编码时：0-> mainfile；注：profile需与EncodeFormat变量配对使用，否则可能导致吊舱编码板应用程序无法正常使用。
	UINT8    BitRateHDWrite; //!< 0x00:表示无效；取值范围为1~20，表示可见光存储编码参数为BitRateHDWrite *1024;
	UINT8 Res1; //!<  保留
	UINT16   Res2; //!<  保留
} GIMBAL_ENCODEHDWRITE_t;

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT8   EncodeSD; //!< 0x00:表示无效；0x01：表示H264;0x02:表示H265;
	UINT8  SDprofile; //!< H264编码时：0->baseline,1->mainfile,2->hp,3->svc_t；H265编码时：0-> mainfile；注：profile需与EncodeFormat变量配对使用，否则可能导致吊舱编码板应用程序无法正常使用。
	UINT8    BitRateSD;// !< 0x00:表示无效；取值范围为1~4，表示红外编码参数为BitRateSD *1024;
	UINT8 Res1; //!<  保留
	UINT16   Res2; //!<  保留

} GIMBAL_ENCODESD_t;

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT8    EncodeReboot; //!//!< 0x00:表示无效；0x01:表示编码板重启；
	UINT8  Res1;//保留1
} GIMBAL_ENCODESYS_t;

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT8  Res1;//保留1
	UINT8    OptoPara; //!< 对比度、亮度、色调、饱和度等参数调试，详见表1；
	UINT16   OptoParaVal;//!< 对比度、亮度、色调、饱和度等参数值；
	UINT16   Res2; //!<  保留2
} GIMBAL_HDHUE_t;

#ifdef PodCtlProto103
typedef struct
{
	//----M包------
	INT16    Euler_roll;//!< 1bit = 360/65536°
	INT16    Euler_pitch;//!< 1bit = 360/65536°
	INT16    Euler_yaw;//!< 1bit = 360/65536°
	UINT8    year; //!< Integer year value 0代表2000年.
	UINT8    month; //!< Integer month of year 1-12.
	UINT8    day;   //!< Integer day of month 1-31.   
	UINT8    hour; // 时  utc时间
	UINT8    minute; //分
	UINT8    second;   //秒    
	INT32    lat;    //飞机纬度   度  1e7
	INT32    lon;    //飞机经度   度， 1e7
	INT16    HMSL;   //飞机高度   m 
	INT16    TGTHMSL; //目标高度   m 
   // ----A1包------
	UINT8    ServStatus;   //伺服状态，详细定义参见表1；
	UINT8   AdapTemplate;// 0x00:无动作；0x01:自适应模板开启；0x02:自适应模板关闭；
	INT16  ServParam1; //参数1~参数7，详细定义参见表2；
	INT16  ServParam2;
	INT16  ServParam3;
	INT16  ServParam4;
	INT32 ServParam5;
	INT32 ServParam6;
	INT32 ServParam7;
	//----A2包------
	UINT8  ServAdjStatus;// 伺服状态调整，详细定义参见表3；
	INT8 ServAdjVal;//调整量；详细定义参见表4；
	//----C1包与C2包------
	UINT8  SensorStatus;//0x00:无动作；0x01:可见光；0x02:红外；也是用来切换主输出，且谁为主输出，指令对谁有效；在切换主输出时，若处于跟踪态，则会自动退出跟踪。与SensorCmd和SensorCmdVal配套使用？
	UINT8  SensorCmd;//传感器指令控制，详见定义参见表5；
	UINT8 SensorCmdVal;//操作指令值，说明详见表5备注；
	INT8 SensorCmdVal1;//补充SensorCmd命令的操作指令值
	UINT8  LaserCmd;//激光指令，详见表6
	//----E1包与E2------
	UINT8 TrackCtrl;//跟踪器指令，详细定义参见表7； 
	UINT8 OSDFlag;// 0x00:无动作；0x01:OSD完全开启（字符显示开）;0x02:OSD完全关闭（字符显示关）;0x03:OSB半开（OSD精简）
	UINT8 BoxFlag; // 0x00:无动作；0x01:波门开启;0x02: 波门关闭; 
	UINT8 CrossFlag; // 0x00:无动作；0x01:十字线开启;0x02: 十字线关闭;
	INT16 TrackPan;// 跟踪点方位指令位置，有符号整型，1bit=1pixel
	INT16 TrackTilt;// 跟踪点俯仰指令位置，有符号整型，1bit=1pixel说明：视频正中心为坐标原点，向右为方位正方向，向上为俯仰正方向；本指令用于直接转移跟踪点到指令位置。
	INT8 IR_thermometry_Status; //红外测温状态 0.关闭测温 1.开启测温
	INT16 TgtDistance;//吊舱与目标之间的直线距离(斜距)
	INT16 IR_thermometry_ID;//红外测温目标编号
	UINT8 IR_thermometry_width;//红外测温目标像素宽度
	UINT8 IR_thermometry_height;//红外测温目标像素高度
	INT16 IR_thermometry_pixelX;//红外测温目标位置中心点坐标X
	INT16 IR_thermometry_pixelY;//红外测温目标位置中心点坐标Y
#if 0
	INT16 RES1;
	INT16 RES2;
	INT16 RES3;
	INT16 RES4;
#endif
	INT16 RES5;
} GIMBAL_HF_Ctrl;

typedef struct
{
	//----B1包与B2包------
	UINT8   ServStatus;   //伺服状态反馈，详细定义参见表1；
	UINT8   ServAdjStatus;// 伺服状态调反馈，详细定义参见表3；
	INT16 FramePan;//框架偏航角， 1bit=360/65536°，有符号整型
	INT16 FrameTilt;//框架俯仰角， 1bit=360/65536°，有符号整型
	INT16 FrameRoll;//框架滚转角， 1bit=360/65536°，有符号整型
	INT16 Pan;//姿态偏航角， 1bit=360/65536°，有符号整型
	INT16 Tilt;// 姿态俯仰角，1bit=360/65536°，有符号整型
	INT16 Roll;// 姿态滚转角，1bit=360/65536°，有符号整型
	INT16 PanRateInput;// 方位速度环输入, 1bit=0.01°/S，有符号整型
	INT16 TiltRateInput;// 俯仰速度环输入2字节, 1bit=0.01°/S，有符号整型
	INT16 PanAglVel;// 方位角速度, 1bit=0.01°/S，有符号整型
	INT16 TiltAglVel;// 俯仰角速度, 1bit=0.01°/S，有符号整型
	UINT8   ComFailures;// 0xff表示无故障，其他故障详见表8；
	UINT8 StabStatus; //图像稳定状态，0：图像不稳定，1：图像稳定；
	//----D1包与D2包------
	UINT8 SensorStatus;//传感器状态，用来显示谁为主输出
	UINT8 WhitevsBlack;//对于红外而言，0：白热；1：表示黑热；
	UINT8 LaserEchoStatus;//激光回波状态，0：回波数据无效，1：回波数据有效
	UINT8 LaserStatus;//激光周期测距状态，0：表示激光周期测距关闭；1：激光周期测距开启；
	UINT16 FOVRateVal;//可见光相机机芯反馈的倍数原始指令值；若电子变焦完全开启，则指令值为0x7AC0;
	//----F1包------
	UINT8 TrackStatus;//跟踪器状态，0x00:停止；0x01无效；0x02:跟踪；0x03:搜索；注：如何知道当前是在红外上跟踪还是可见光上跟踪；
	UINT8 RES1;
	INT16 PanPixelsub;//方位目标像素差值（被跟踪目标pan方向偏离像素中心的像素差值）,
	INT16 TiltPixelsub;//俯仰目标像素差值（被跟踪目标tilt方向偏离像素中心的像素差值），说明：视频正中心为坐标原点，向右为方位正方向，向上为俯仰正方向；本指令用于直接转移跟踪点到指令位置。
	UINT16 TrackWidth;//跟踪框的宽度；//跟踪框的坐标均按照1920*1080画幅给；
	UINT16 TrackHeight;//跟踪框的高度；
	INT16 TrackX;//跟踪框中心点坐标X；
	INT16 TrackY;// 跟踪框中心点坐标Y；
	INT32 TargetLat;//目标纬度；1bit=10^-7°, WGS-84
	INT32 TargetLon;//目标经度；1bit=10^-7°, WGS-84
	INT16 TargetHMSL;//目标高度；1bit=1m
	UINT16 Temp;//吊舱温度度; 1bit=1度
	UINT16   SearchWidth;    // !<搜索框宽度（模板大小）
	UINT16   SearchHeight;   // !< 搜索框高度（模板大小）
	UINT16 Rangval; //激光测距值，最近一次有效的测距返回值：1bit表示1m，全零代表无效，无符号整型
	UINT16 InfaredFOV; //红外相机视场角，1E4；单位弧度；
	UINT16 Infaredlum; //红外相机亮度值；
	UINT16 InfaredContrast; //红外相机对比度值；
	UINT8 Defogflag;//0:可见光去雾关闭；1：可见光去雾关闭；
	INT8 LaserWorkStatus; //激光器工作状态，请浩孚标注状态值含义
	UINT16 VisualFOV; //可见光相机视场角，1E4；单位弧度；
	INT16 IR_thermometry_ID;       //目标编号
	UINT8 IR_thermometry_width;   //目标像素高度
	UINT8 IR_thermometry_height;  //目标像素高度

	INT16 IR_thermometry_pixelX;  //目标位置中心点坐标X
	INT16 IR_thermometry_pixelY;  //目标位置中心点坐标Y

	INT16 IR_thermometry_maxT;    //区域最高温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
	INT16 IR_thermometry_minT;    //区域最低温度（单位摄氏度，精度0.1摄氏度，全F表示无效）

	INT16 IR_thermometry_centreT; //区域中心点温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
	INT16 IR_thermometry_meanT;  //区域平均温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
	UINT32 GR; //地理分辨率（每个像素代表的地理尺寸，单位：cm）

} GIMBAL_HF_CTRL_FEEDBACK_;

#else
typedef struct
{
	//----M包------
	INT16    Euler_roll;//!< 1bit = 360/65536°
	INT16    Euler_pitch;//!< 1bit = 360/65536°

	INT16    Euler_yaw;//!< 1bit = 360/65536°
	UINT8    year; //!< Integer year value 0代表2000年.
	UINT8    month; //!< Integer month of year 1-12.

	UINT8    day;   //!< Integer day of month 1-31.   
	UINT8    hour; // 时  utc时间
	UINT8    minute; //分
	UINT8    second;   //秒   

	INT32    lat;    //飞机纬度   度  1e7
	INT32    lon;    //飞机经度   度， 1e7
	INT16    HMSL;   //飞机高度   m 
	INT16    TGTHMSL; //目标高度   m 
   // ----A1包------
	UINT8    ServStatus;   //伺服状态，详细定义参见表1；
	UINT8   AdapTemplate;// 0x00:无动作；0x01:自适应模板开启；0x02:自适应模板关闭；
	INT16  ServParam1; //参数1~参数7，详细定义参见表2；

	INT16  ServParam2;
	INT16  ServParam3;
	INT16  ServParam4;
	INT32 ServParam5;
	INT32 ServParam6;
	INT32 ServParam7;
	//----A2包------
	UINT8  ServAdjStatus;// 伺服状态调整，详细定义参见表3；
	INT8 ServAdjVal;//调整量；详细定义参见表4；
	//----C1包与C2包------
	UINT8  SensorStatus;//0x00:无动作；0x01:可见光；0x02:红外；也是用来切换主输出，且谁为主输出，指令对谁有效；在切换主输出时，若处于跟踪态，则会自动退出跟踪。与SensorCmd和SensorCmdVal配套使用？
	UINT8  SensorCmd;//传感器指令控制，详见定义参见表5；
	UINT8 SensorCmdVal;//操作指令值，说明详见表5备注；
	UINT8  LaserCmd;//激光指令，详见表6
	//----E1包与E2------
	UINT8 TrackCtrl;//跟踪器指令，详细定义参见表7； 
	UINT8 OSDFlag;// 0x00:无动作；0x01:OSD完全开启（字符显示开）;0x02:OSD完全关闭（字符显示关）;0x03:OSB半开（OSD精简）
	UINT8 BoxFlag; // 0x00:无动作；0x01:波门开启;0x02: 波门关闭; 
	UINT8 CrossFlag; // 0x00:无动作；0x01:十字线开启;0x02: 十字线关闭;
	INT16 TrackPan;// 跟踪点方位指令位置，有符号整型，1bit=1pixel
	INT16 TrackTilt;// 跟踪点俯仰指令位置，有符号整型，1bit=1pixel说明：视频正中心为坐标原点，向右为方位正方向，向上为俯仰正方向；本指令用于直接转移跟踪点到指令位置。
	INT16 RES1;
	INT16 RES2;
	INT16 RES3;
	INT16 RES4;
	INT16 RES5;

} GIMBAL_HF_Ctrl;

typedef struct
{
	//----B1包与B2包------
	UINT8   ServStatus;   //伺服状态反馈，详细定义参见表1；
	UINT8   ServAdjStatus;// 伺服状态调反馈，详细定义参见表3；
	INT16 FramePan;//框架偏航角， 1bit=360/65536°，有符号整型
	INT16 FrameTilt;//框架俯仰角， 1bit=360/65536°，有符号整型
	INT16 FrameRoll;//框架滚转角， 1bit=360/65536°，有符号整型
	INT16 Pan;//姿态偏航角， 1bit=360/65536°，有符号整型
	INT16 Tilt;// 姿态俯仰角，1bit=360/65536°，有符号整型
	INT16 Roll;// 姿态滚转角，1bit=360/65536°，有符号整型
	INT16 PanRateInput;// 方位速度环输入, 1bit=0.01°/S，有符号整型
	INT16 TiltRateInput;// 俯仰速度环输入2字节, 1bit=0.01°/S，有符号整型
	INT16 PanAglVel;// 方位角速度, 1bit=0.01°/S，有符号整型
	INT16 TiltAglVel;// 俯仰角速度, 1bit=0.01°/S，有符号整型
	UINT8   ComFailures;// 0xff表示无故障，其他故障详见表8；
	UINT8 StabStatus; //图像稳定状态，0：图像不稳定，1：图像稳定；
	//----D1包与D2包------
	UINT8 SensorStatus;//传感器状态，用来显示谁为主输出
	UINT8 WhitevsBlack;//对于红外而言，0：白热；1：表示黑热；
	UINT8 LaserEchoStatus;//激光回波状态，0：回波数据无效，1：回波数据有效
	UINT8 LaserStatus;//激光周期测距状态，0：表示激光周期测距关闭；1：激光周期测距开启；
	UINT16 FOVRateVal;//可见光相机机芯反馈的倍数原始指令值；若电子变焦完全开启，则指令值为0x7AC0;
	//----F1包------
	UINT8 TrackStatus;//跟踪器状态，0x00:停止；0x01无效；0x02:跟踪；0x03:搜索；注：如何知道当前是在红外上跟踪还是可见光上跟踪；
	UINT8 RES1;
	INT16 PanPixelsub;//方位目标像素差值（被跟踪目标pan方向偏离像素中心的像素差值）,
	INT16 TiltPixelsub;//俯仰目标像素差值（被跟踪目标tilt方向偏离像素中心的像素差值），说明：视频正中心为坐标原点，向右为方位正方向，向上为俯仰正方向；本指令用于直接转移跟踪点到指令位置。
	UINT16 TrackWidth;//跟踪框的宽度；//跟踪框的坐标均按照1920*1080画幅给；
	UINT16 TrackHeight;//跟踪框的高度；
	INT16 TrackX;//跟踪框中心点坐标X；
	INT16 TrackY;// 跟踪框中心点坐标Y；
	INT32 TargetLat;//目标纬度；1bit=10^-7°, WGS-84
	INT32 TargetLon;//目标经度；1bit=10^-7°, WGS-84
	INT16 TargetHMSL;//目标高度；1bit=1m
	UINT16 Temp;//吊舱温度度; 1bit=1度
	UINT16   SearchWidth;    // !<搜索框宽度（模板大小）
	UINT16   SearchHeight;   // !< 搜索框高度（模板大小）
	UINT16 Rangval; //激光测距值，最近一次有效的测距返回值：1bit表示1m，全零代表无效，无符号整型
	UINT16 InfaredFOV; //红外相机视场角，1E4；单位弧度；
	UINT16 Infaredlum; //红外相机亮度值；
	UINT16 InfaredContrast; //红外相机对比度值；
	UINT8 Defogflag;//0:可见光去雾关闭；1：可见光去雾关闭；
	INT8 RES3;
	INT16 RES4;

} GIMBAL_HF_CTRL_FEEDBACK_;
#endif

typedef struct
{
	UINT8 ServStatus; //伺服状态反馈，0-速率模式  1-姿态锁定模式  2-框架锁定模式  3-目标跟踪模式
	UINT8 TrackerStatus;//目标跟踪状态：0-未跟踪 1-搜索态 2-跟踪态 
	INT16 TrackX;	 //跟踪框中心点坐标X(列坐标)
	INT16 TrackY;	 //跟踪框中心点坐标Y(行坐标)
	UINT16 TrackWidth;//跟踪框的宽度
	UINT16 TrackHeight;//跟踪框的高度
	INT16 FramePan; //框架偏航角，单位 1e4 * 弧度 
	INT16 FrameTilt;//框架俯仰角，单位 1e4 * 弧度
	INT16 FrameRoll;//框架滚转角，单位 1e4 * 弧度
	INT16 AttPan;   //姿态偏航角，单位 1e4 * 弧度
	INT16 AttTilt;  //姿态俯仰角，单位 1e4 * 弧度
	INT16 AttRoll;     //姿态滚转角，单位 1e4 * 弧度
	UINT16 VisualFOV;  //可见光相机视场角，单位：10 * 度 ，精度0.1度
	UINT8 VisualDzoomStatus;//可见光相机电子放大功能状态：0-关闭 1-开启
	UINT8 VisualDzoomX;//可见光相机电子放大倍率：0-8 倍   精度0.1倍
	UINT8 VisualSpotAE;//可见光相机电子区域点测光自动曝光模式开关：0-关闭  1-开启
	UINT8 VisualDefogStatus;//可见光机芯去雾增强状态，0-可见光去雾关闭；1-可见光去雾开启；
	UINT8 VisualIcrStatus;//可见光机芯近红外模式功能状态，0-近红外已经关闭 1-近红外已经开启
	UINT8 LaserStatus;//激光周期测距状态，0-表示激光周期测距关闭；1-激光周期测距开启；
	UINT16 LaserDis;//激光测距值 单位：米
	UINT16 IrFOV; //红外相机视场角，  单位：10 * 度 ，精度0.1度
	UINT8 IrDzoomStatus;//红外相机电子放大功能状态：0-关闭 1-开启
	UINT8 IrDisplayMode;//红外显示模式：0-白热 1-黑热 2-伪彩模式1   注：取值【3,7】为其他几种伪彩模式
	UINT8 VideoStatus;//视频状态，用来显示谁为主输出 0-可见光为主输出 1-红外为主输出
	UINT8 PipStatus;//画中画状态，0-画中画关闭 1-画中画开启
	UINT8 StabStatus;//主输出电子稳像状态，0-电子稳像关闭；1-电子稳像开启
	UINT8 PodTemp;//吊舱温度,单位 ：℃
} Gimbal_Ctrl;

// FPV视频推流控制,获取来自地面的推流地址:即value1.value2.value3.value4
typedef struct
{
	JACX_MSG_HEAD msgHead;
	uint16_t deviceId;
	uint16_t msgId;
	uint8_t functionId;
	uint8_t value1;
	uint8_t value2;
	uint8_t value3;
	uint8_t value4;
	uint8_t cka;
	uint8_t ckb;
} JACX_PUSHVIDEO_SWITCH;

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID=0x08
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT16  Rev1; //保留:
	UINT32  Index;// 序号 
	INT32  Lat;// 纬度 1e7
	INT32  Lon;//!< 经度1e7
	INT32  Alt;//!< 指向点高度，收到的值为厘米单位，除1e2转化为米
} GimbalExTarget_t;


typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID=0x07
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT16  CommandType; //指令类型:
	UINT16  Value1;// 保留1 
	UINT16  Value2;// 保留  
	INT16   Value3;//!< 保留
	INT16   Value4;//!< 保留
} GimbalExCommand_t;

typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID=0x06
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT16  ParamsType; //参数类型；
	INT16  Value1;// 保留1 
	UINT16  Value2;// 保留  
	INT16   Value3;//!< 保留
	INT16   Value4;//!< 保留
} GimbalExParam_t;

//指定目标点经纬度，控制吊舱观察该点
//指令CMD：0 退出指点跟踪 1.以姿态模式进入指定跟踪、2、以目标跟踪模式进入指点跟踪。
typedef struct
{
	UINT16  Ver; //协议版本号
	UINT16  MSGID;//消息ID=0x0b
	UINT8  SEQ;//消息序号
	UINT8  ACK;//消息回复
	UINT8 CMD;// 指定跟踪状态指令 
	UINT8 rev1;
	INT32  Lat;//   纬度 1e7     目标指向点维度
	INT32  Lon;//!< 经度1e7	     目标指向点经度
	INT32  Alt;//!< 目标指向点高度，1e2 单位cm
} GimbalTgtTrack_t;

typedef struct
{
	UINT16 Ver; //协议版本号 
	UINT16 MSGID;//消息 ID，详见表 2 MSGID 说明 消息ID=0xA9
	UINT8 SEQ;//消息序号， 
	UINT8 ACK;//消息回复 
	UINT8 VisualCameraPowerSwitch; //!< 0x00:表示无效；0x01：表示打开可见光机芯电源;0x02: 表示关闭可见光机芯电源; 
	UINT8 InfaredCameraPowerSwitch; //!< 0x00:表示无效；0x01：表示打开红外机芯电源;0x02: 表示关闭红外机芯电源; 
	UINT8 LaserPowerSwitch; //!<0x00:表示无效；0x01：表示打开激光器电源;0x02: 表示关闭激光器电源; 
	UINT8 GimbalPowerSwitch; //!< !<0x00:表示无效；0x01：表示打开吊舱任务设备电源;0x02: 表示关闭激光器吊舱任务设备电源; 
	UINT16 Res2; //!< 保留 2
} GIMBAL_POWERCTRL_t;        //特别注明，任务设备指可见光机芯、红外机芯、激光器

//目标态势数据包
typedef struct
{
	UINT32 Time;//消息产生时戳
	UINT16 ID; //目标编号
	UINT8 IFF;//目标敌我属性：0-未知 1-敌 2-我
	UINT8 Class;//目标类别,类别值与目标对应关系待定
	UINT16 confidence;//目标类别置性度：精度 0.01，如confidence=9999标识置性度为99.99%
	INT16 PixelX;//目标像素坐标：列坐标
	INT16 PixelY;//目标像素坐标：行坐标
	INT16 PixelW;//目标像素大小：宽度
	INT16 PixelH;//目标像素大小：高度
	INT16 Alt;//目标地理坐标：海拔高度
	INT32 Lon;//目标地理坐标：经度
	INT32 Lat;//目标地理坐标：纬度
	//UINT8 Logo[32];//目标标识：以ASCII码(字符串)形式标识，结尾以“/r”的ASCII码结束
} DetectTar;

typedef struct
{
	INT32 ViewpointTime;//视点中心态势产生时刻消息时戳
	INT32 ViewpointLon;//视点中心目标态势：经度（单位:度*1e7）
	INT32 ViewpointLat;//视点中心目标态势：纬度（单位:度*1e7）
	INT16 ViewpointAlt;//视点中心目标态势：高度（单位:米）
	UINT16 RangeingMode;//定位用测距模式，0，使用估距进行定位，1.使用激光测距进行定位
	INT32 TrackerTarTime;//跟踪器目标态势产生时刻消息时戳
	INT32 TrackerTarLon;//跟踪器目标态势：经度（单位:度*1e7）
	INT32 TrackerTarLat;//跟踪器目标态势：纬度（单位:度*1e7）
	INT16 TrackerTarAlt;//跟踪器目标态势：高度（单位:米）
	UINT8 res;//保留字
	UINT8 DetectTarNum;//AI检测目标数量
	DetectTar  DetectTarPos[15];//AI检测的目标位置
} TarSituation;

typedef struct
{
	double longitude;
	double latitude;
	double altitude;
} RoadCtrlStr;

typedef struct
{
	double longitude;
	double latitude;
	double altitude;
	float  yaw;
	float  tilt;
	float  roll;
} PILOTStr;

typedef struct
{
	float speed_yaw;  //偏航
	float speed_pitch;//俯仰
} PodSpeedCtl;        //吊舱速率控制

#pragma pack()

typedef JACX_MSG_PACKET COMMPACKET_t;

#define JACX_HEAD_SIZE sizeof(JACX_MSG_HEAD)
#define HEAD_LEN_EX_SYNC (sizeof(JACX_MSG_HEAD) - 2)

#endif // !EAPS_MESSAGE_PROTO_DEFS_H