//#ifndef EAPS_META_DATA_STRUCTURE_H
//#define EAPS_META_DATA_STRUCTURE_H
//
///**
// * @file
// * @ingroup jav_meta_data
// * JOUAV meta data struct header
// */
//
//#include <stdint.h>
//#include <vector>
//#include <string>
//
//#pragma pack(1)
//
// /**
// * @brief 视频图像顶点及中心点坐标
// */
//struct ImageVertexCoordinate
//{
//	int32_t Lat;   ///< 纬度 1E7
//	int32_t Lon;   ///< 经度 1E7
//	int32_t HMSL;  ///< 高度，单位m
//};
//
///**
//* @brief 无人机POS数据
//*/
//struct CarrierVehiclePosInfo
//{
//	uint64_t TimeStamp;     ///< 从1970-01-01:00:00:00开始的毫秒数
//	uint8_t TimeZone;       ///< 时区
//
//	int16_t CarrierVehicleHeadingAngle;///< 飞机偏航角，1E4，单位弧度
//	int16_t CarrierVehiclePitchAngle;  ///< 飞机俯仰角，1E4，单位弧度
//	int16_t CarrierVehicleRollAngle;   ///< 飞机滚转角，1E4，单位弧度
//
//	int32_t CarrierVehicleLat;         ///< 飞机纬度，1E7
//	int32_t CarrierVehicleLon;         ///< 飞机经度，1E7
//	int32_t CarrierVehicleHMSL;        ///< 飞机海拔高度，1E2，单位m
//
//	uint32_t DisFromHome;   ///< 离家距离，单位m
//	int16_t HeadingFromHome;///< 离家方位角，*100，单位弧度
//
//	uint16_t VGnd;          ///< 地速，单位m/s
//	uint16_t Tas;           ///< 空速，单位m/s
//
//	int16_t VNorth;         ///< 地速(北)，1E2，单位m/s
//	int16_t VEast;          ///< 地速(东)，1E2，单位m/s
//	int16_t VDown;          ///< 地速(地)，1E2，单位m/s
//
//	uint32_t FlySeconds;    ///< 飞机飞行时间，单位s
//};
//
///**
//* @brief 无人机状态数据
//*/
//struct CarrierVehicleStatusInfo
//{
//	std::string CarrierVehicleSN;          ///< 无人机飞控序列号，手动设置
//	uint16_t CarrierVehicleID;             ///< 无人机唯一ID
//	uint32_t CarrierVehicleFirmwareVersion;///< 无人机固件版本，手动设置
//	uint8_t VeclType;           ///< 飞机型号，0:cw007 1:cw10 2:cw15 3:cw20 4:cw25 5:cw30 6:CW100
//
//	uint8_t Pdop;               ///< 卫星精度
//	uint8_t NumSV;              ///< 卫星颗数
//
//	uint8_t Orienteering;       ///< RTK状态，RTK状态，0: RTK 1:SINGLE
//
//	uint16_t RPM;               ///< 发动机转速，r/m
//	uint8_t ThrottleCmd;        ///< 油门指令，1E2
//	uint16_t MPowerV;           ///< 主电源，单位V
//	uint16_t MPowerA;           ///< 动力电源，单位V
//	uint8_t ElecricQuantity;    ///< 电池电量百分比
//	uint32_t EnduranceMileage;  ///< 剩余续航里程，单位m
//
//	/**
//	* @brief 搜索任务
//	* @details \n
//	*	0: NO_TASK,         无任务		OFF \n
//	*	1: SEARCH_TASK,     搜寻任务		SCH \n
//	*	2: TP_OBSERVE_TASK, 临时观察任务	TPO \n
//	*	3: CT_OBSERVE_TASK, 持续观察任务	CTO \n
//	*	4: TRACK_TASK,      跟踪任务		TRK \n
//	*	5: IDLE_TASK,       闲置任务		IDL \n
//	*/
//	uint8_t ScoutTask;
//
//	//!<飞控状态模式，具体解析见文档
//	/**
//	* @brief 巡线相关模式
//	* @details \n
//	* 0:自由模式（Rate）
//	* 1:巡线模式（Line）
//	* 2:定点模式(GPS)
//	* 3:位置半自主模式(P_Semi)
//	* 4:框架锁定模式(F_Lock)
//	* 5:速率半自主模式(A2G)
//	*/
//	uint8_t LineInspecMode;
//
//	/**
//	* @brief 自主航线规划
//	* @details \n
//	* 0:  RTT:OFF
//	* 1:  RTT:Auto
//	* 2:  RTT:Manual
//	*/
//	uint8_t	AutoPlan;
//
//	/**
//	* @brief 自驾状态
//	* @details \n
//	*	GROUND_TEST,                  //0  地面测试  GTST \n
//	*	APV_PRE_LAUNCH_STATE,	      //1  飞前检查 PLCH \n
//	*	APV_ATT_ASSIST_STATE,         //2  姿态辅助 ATT \n
//	*	APV_HOVER_ASSIST_STATE,       //3  悬停辅助 HOR \n
//	*	APV_LIFT_OFF_STATE,           //4  离地 LIFT \n
//	*	APV_CLIMB_OUT_STATE,          //5  爬升 CLMB \n
//	*	APV_ACCELERATE_STATE,         //6  加速 ACCL \n
//	*	APV_V2L_STATE,                //7  垂转平 V2L \n
//	*	APV_FLYING_STATE,             //8  飞行  FLY \n
//	*	APV_LANDING_STATE,            //9  降落 LAND \n
//	*	APV_DECELERATE_STATE,         //10 减速 DECL \n
//	*	APV_L2V_STATE,                //11 平转垂 L2V \n
//	*	APV_FINAL_HOVER_STATE,        //12 末端悬停 FNHR \n
//	*	APV_FINAL_DESCENT_STATE,      //13 末端下降 FNDC \n
//	*	APV_FORCED_LANDING_STATE,     //14 迫降 FRLD \n
//	*	APV_AHRS_FORCED_LANDING_STATE,//15 AHRS迫降  AFRLD \n
//	*	APV_SEMI_AUTO_ASSIST_STATE,   //16 半自动辅助 SAST \n
//	*	APV_SEMI_AUTO_V2L_STATE,      //17 半自动垂转平 SV2L \n
//	*	APV_SEMI_AUTO_L2V_STATE,      //18 半自动平转垂 SL2V \n
//	*	APV_AUTO_HOVER_STATE,         //19 即时悬停 AHOR \n
//	*	APV_SELF_DESTRUCTION_STATE,   //20 自毁 DSRC
//	*/
//	uint32_t APModeStates;
//
//	uint16_t TasCmd;            ///< 空速指令，*10
//	int16_t HeightCmd;          ///< 高度指令，单位m
//
//	int16_t WypNum;             ///< 当前跟踪航路点
//};
//
///**
//* @brief 吊舱POS数据
//*/
//struct GimbalPosInfo
//{
//	uint16_t VisualViewAngleHorizontal; ///< 可见光水平视场角，单位弧度，1E4
//	uint16_t VisualViewAngleVertical;   ///< 可见光垂直视场角，单位弧度，1E4
//	uint16_t InfaredViewAngleHorizontal;///< 红外水平视场角，单位弧度，1E4
//	uint16_t InfaredViewAngleVertical;  ///< 红外垂直视场角，单位弧度，1E4
//
//	uint16_t FocalDistance;             ///< 焦距，单位mm
//
//	int16_t GimbalPan;                  ///< 吊舱航向欧拉角，单位弧度，1E4
//	int16_t GimbalTilt;                 ///< 吊舱俯仰欧拉角，单位弧度，1E4
//	int16_t GimbalRoll;                 ///< 吊舱滚转欧拉角，单位弧度，1E4
//
//	int16_t FramePan;                   ///< 航向框架角，单位弧度，1E4
//	int16_t FrameTilt;                  ///< 俯仰框架角，单位弧度，1E4
//	int16_t FrameRoll;                  ///< 滚转框架角，单位弧度，1E4
//
//	int32_t TGTLat;                     ///< 目标纬度，单位度
//	int32_t TGTLon;                     ///< 目标经度，单位度
//	int16_t TGTHMSL;                    ///< 目标高度，单位m
//	uint16_t TGTVelocity;               ///< 目标速度，*100，单位m/s
//	int16_t TGTHeading;                 ///< 目标速度方位角，*100，单位弧度
//	uint16_t SlantR;                    ///< 目标估距
//};
//
///**
//* @brief 吊舱状态数据
//*/
//struct GimbalStatusInfo
//{
//	std::int16_t    Euler[3];//!< 1E4，基座三轴欧拉角，单位弧度
//	
//	uint16_t GMPower;                   ///< 吊舱电源，*50
//
//	/**
//	* @brief 吊舱工作模式
//	* @details \n
//	*	0x00 速率模式
//		ServoCmd0为航向指令，1e4，rad/s \n
//		ServoCmd1为俯仰指令，1e4，rad/s \n
//		 \n
//		0x01 姿态模式，手柄不支持 \n
//		 \n
//		0x02 关伺服（休眠使用） \n
//		 \n
//		0x03 归零位锁定 \n
//		 \n
//		0x04 零位调整，1e4,rad/s \n
//		ServoCmd0为航向零位调整量， \n
//		ServoCmd1为俯仰零位调整量， \n
//		 \n
//		0x05 航向扫描 \n
//		ServoCmd0为航向扫描速率 1e4，rad/s \n
//		 \n
//		0x06 框架角模式 \n
//		ServoCmd0为航向指令，1e4，rad \n
//		ServoCmd1为俯仰指令，1e4，rad \n
//		 \n
//		0x08 低能量控制 \n
//		 \n
//		0x09 球机速率补偿注入(每按下一次发送0.01度/S的补偿速率值) \n
//	*/
//	uint8_t ServoCmd;
//
//	/**
//	* @brief
//	* @details \n
//	* 0:初始化未完成 (OFF)
//	* 1:初始化完成(ON)
//	*/
//	uint8_t SeroveInit;					///< 
//
//	int16_t ServoCmd0;                  ///< 吊舱航向指令，1E4，单位rad，rad/s
//	int16_t ServoCmd1;                  ///< 吊舱俯仰指令，1E4，单位rad，rad/s
//
//	std::uint16_t   ServoCrossX;	// !<随动X方向坐标
//	std::uint16_t   ServoCrossY;	// !<随动Y方向坐标
//	std::uint16_t   ServoCrossWidth;// !<随动十字宽度
//	std::uint16_t   ServoCrossheight;// !<随动十字高度
//
//	uint16_t PixelElement;              ///< 像元大小，单位um
//
//	int8_t GimbalDeployCmd;             ///< 吊舱收放 0:收起 1:放下
//
//
//
//	/**
//	* @brief 黑热或白热
//	* @details \n
//	*	0:白热 \n
//	*	1:黑热 \n
//	*	2:伪彩1 \n
//	*	3:伪彩2 \n
//	*	4:伪彩3 \n
//	*	5:伪彩4 \n
//	*	6:伪彩5 \n
//	*/
//	uint8_t W_or_B;
//
//	/*
//	伪彩
//	0:黑白
//	1:伪彩
//	2:无画中画
//	*/
//	std::uint8_t	IR;
//
//	std::uint32_t visualfovhmul; //可见光机芯倍率
//
//	uint8_t FovLock;                    ///< 视场角是否锁定
//};
//
///**
//* @brief 检测对象参数
//*/
//struct AIDataDetcInfo
//{
//	uint32_t DetLefttopX{};				//< 跟踪框左上角X坐标
//	uint32_t DetLefttopY{};				//< 跟踪框左上角Y坐标
//	uint32_t DetWidth{};				//< 跟踪框宽度
//	uint32_t DetHeight{};				//< 跟踪框高度
//
//	//const char* DetTGTclass{};		//< 目标类别 ，目标类别暂未定
//	int32_t DetTGTclass{};
//	uint32_t TgtConfidence{};			//< 目标置信度 ，1e2
//	uint32_t TgtSN{};					//< 目标序列号
//	uint32_t IFF{};						//< 目标敌我属性：0-未知 1-敌 2-我
//};
//
///**
//* @brief AI相关信息
//*/
//struct AiInfos
//{
//	uint32_t AiStatus;//AI状态 0关闭 1开启
//	//uint32_t AIDataDetcSize;//AI检测数据 数量
//	std::vector<AIDataDetcInfo> AIDataDetcInfoArray{};//AI检测数据
//};
//
///**
//* @brief AR参数
//*/
//
//struct ARElement
//{
//	uint32_t Type;    // 元素类型 0.点元素；1.线元素；2.面元素
//	uint32_t DotQuantity;    //元素点数量,同一个元素的点数量
//	int32_t X;    //像素坐标-X轴
//	int32_t Y;    //像素坐标-Y轴
//	int32_t lon;   //WGS84地理坐标-纬度
//	int32_t lat;   //WGS84地理坐标-经度
//	int32_t HMSL;  // WGS84地理坐标-高度
//	uint32_t Category; //线元素类型点目标：0.房屋；1.杆塔;  线目标：0.目标线路；1.A级警戒线；2.B级警戒线;  面目标：0.正常区域；1.警戒区域
//	uint32_t CurIndex;  //当前节点索引号,线/面元素时有效，每个元素的索引号都从0开始
//	uint32_t NextIndex; //下个节点索引号, 线/面元素时有效
//};
//
///**
//* @brief AR相关信息
//*/
//// 统一命名
//struct ArInfos
//{
//	uint32_t ArStatus;        // AR状态 0关闭 1开启
//	uint32_t ArTroubleCode;   // AR故障码  0.正常  1.异常
//	//uint32_t ArElementsNum;     // AR元素数量
//	std::vector<ARElement> ElementsArray{}; // AR元素数据集
//};
//
///**
//* @brief 图像处理板数据
//*/
//struct ImageProcessingBoardInfo
//{
//	uint16_t SearchWidth;               ///< 搜索框宽度
//	uint16_t SearchHeight;              ///< 搜索框高度
//
//	uint16_t ServoCrossX;               ///< 随动中心X坐标
//	uint16_t ServoCrossY;               ///< 随动中心Y坐标
//	uint16_t ServoCrossWidth;           ///< 随动十字宽度
//	uint16_t ServoCrossHeight;          ///< 随动十字高度
//
//	uint16_t TrackLeftTopX;             ///< 跟踪框左上角X坐标
//	uint16_t TrackLeftTopY;             ///< 跟踪框左上角Y坐标
//	uint16_t TrackWidth;                ///< 跟踪框宽度
//	uint16_t TrackHeight;               ///< 跟踪框高度
//
//	ImageVertexCoordinate ImgCood[5];   ///< 视频图像5个点的坐标
//
//	uint16_t SDMemory;                  ///< SD卡剩余内存，*10，单位G
//	uint16_t SnapNum;                   ///< 快照数
//
//	/**
//	* @brief SD卡状态
//	* @details \n
//	*	0:SD卡正常； \n
//	*	1:无mmcblk1设备； \n
//	*	2:mmcblk1设备挂载成功，但删除文件失败，SD卡内存空间小于200M; \n
//	*	3:mmcblk1设备挂载成功，但删除文件失败，SD卡内存空间大于200M，小于8G; \n
//	*	4:有mmcblk1设备，但是未挂载成功
//	*/
//	uint8_t SDFlag;
//
//	/**
//	* @brief 录制状态
//	* @details \n
//	*	0:未录制； \n
//	*	1:录制； \n
//	*	2:未录制；
//	*/
//	uint8_t RecordFlag;
//
//	/**
//	* @brief 跟踪类型
//	* @details \n
//	*	0:红外跟踪 \n
//	*	1:可见光跟踪 \n
//	*	2:非跟踪态
//	*/
//	uint8_t TrackFlag;
//
//	/**
//	* @brief 车辆跟踪 \n
//	* @details \n
//	*	0:关闭 \n
//	*	1:开启
//	*/
//	uint8_t AI_R;
//
//	/**
//	* @brief 车辆跟踪状态
//	* @details \n
//	*	1:车辆跟踪 \n
//	*	0:非车辆跟踪
//	*/
//	uint8_t CarTrack;
//
//	/**
//	* @brief 跟踪类别
//	* @details \n
//		0:Car ; \n
//		1:Truck; \n
//		2:Bus; \n
//		3:not
//	*/
//	uint8_t TrackClass;
//
//	/**
//	* @brief 跟踪状态
//	* @details \n
//		0x00:非车辆锁定  \n
//		0x01:脱锁，进入速率半自主 \n
//		0x02:跟踪状态不佳，但仍是跟踪态，仅关闭RTT \n
//		0x03:车辆锁定
//	*/
//	uint8_t TrackStatus;
//
//	uint32_t Version;                   ///< 图像处理程序版本号
//
//	AiInfos AiInfos_p;					///<AI相关信息，包括AI开关状态
//
//	ArInfos ArInfos_p;                   /// AR相关信息，包括AR开关状态
//
//	uint32_t PIP;                       /// 画中画
//	uint32_t ImgStabilize;              /// 电子稳像  0.关闭  1.开启
//	uint32_t ImgDefog;                  /// 去雾   0.关闭  1.开启
//};
//
//struct GimbalPayloadInfos
//{
//	GimbalPosInfo GimbalPosInfo_p;
//	GimbalStatusInfo GimbalStatusInfo_p;
//	ImageProcessingBoardInfo ImageProcessingBoardInfo_p;
//};
//
///**
//* @brief 激光测距信息
//*/
//struct LaserDataInfos
//{
//	uint32_t LaserStatus{};  //*激光测距机的状态;0:表示无效；1：单次测距；2：连续测距；3：关闭*/
//	uint32_t LaserMeasVal{}; //*激光测距值,当测距无异常时，此值为激光测距值,单位米，否则显示超量程*/
//	uint32_t LaserMeasStatus{}; //*激光测距异常态监测：bit0---FPGA系统状态，bit1-激光出光状态，bit2-主波监测状态，bit3-回波检测状态，bit4-偏压开关状态，bit5-偏压输出状态，bit6-温度状态，bit7-出光关断状态，上述状态位中1表示正常，0表示异常*/
//	int32_t Laserlat{};//*激光测距点纬度值,单位度，1E7*/
//	int32_t Laserlon{};//*激光测距点经度值，单位度，1E7*/
//	int32_t LaserHMSL{};//*激光测距点高度值，单位米*/
//	uint32_t LaserRefStatus{};//*激光测距靶点状态；0：无效；1：表示估距，2：表示激光测距*/
//	int32_t LaserRefLat{};//*激光测距靶点纬度值,单位度，1E7*/
//	int32_t LaserRefLon{};//*激光测距靶点经度值,单位度，1E7*/
//	int32_t LaserRefHMSL{};//*激光测距靶点高度值，单位米*/
//	uint32_t LaserCurStatus{};//*激光测距弹着点状态；0：无效；1：表示估距，2：表示激光测距*/
//	int32_t LaserCurLat{};//*激光测距弹着点纬度值,单位度，1E7*/
//	int32_t LaserCurLon{};//*激光测距弹着点经度值,单位度，1E7*/
//	int32_t LaserCurHMSL{};//*激光测距弹着点高度值，单位米*/
//	int32_t LaserDist{};//*激光测距靶点与弹着点的距离，单位米*/
//	int32_t LaserAngle{};//*激光测距靶点与弹着点的角度，单位弧度，1E4*/
//	uint32_t LaserModel{};//*测距结果模式，0x00:测距结果为单目标,0x01:测距结果为前目标；0x02:测距结果为后目标；0x03:测距结果为前目标和后目标；0x04:测距结果为超距*/
//	uint32_t LaserFreq{};//*若测距模式为连续测距，则此值为连续测距频率；若测距模式为单次测距，则此值为单次测距个数*/
//	uint32_t res1{};//*保留1*/
//	uint32_t res2{};//*保留2*/
//};
//
//
///**
//* @brief 检测对象参数
//*/
//struct TargetTempData
//{
//	uint32_t id{};		//目标编号
//	uint32_t width{};	//目标像素高度
//	uint32_t height{};	//目标像素高度
//	uint32_t pixelX{};	//目标位置中心点坐标X
//	uint32_t pixelY{};	//目标位置中心点坐标Y
//	int32_t maxT{};		//区域最高温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
//	int32_t minT{};		//区域最低温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
//	int32_t centreT{};	//区域中心点温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
//	int32_t meanT{};	//区域平均温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
//};
//
///**
//* @brief 红外测温
//*/
//struct IrThermometerBackData
//{
//	uint32_t msgid{};	//消息包ID
//	uint32_t Time{};	//消息产生时戳
//	uint32_t num{};		//目标区域数量
//	std::vector<TargetTempData> TargetTempsList{};
//};
//
///**
//* @brief 元数据数据基础
//*/
//struct JoFmvMetaDataBasic
//{
//	CarrierVehiclePosInfo CarrierVehiclePosInfo_p;       ///< 无人机POS数据
//	CarrierVehicleStatusInfo CarrierVehicleStatusInfo_p; ///< 无人机状态数据
//
//	/**
//	* @brief 标明载荷类型
//	* @details \n
//	*	PayloadOptions & 0x01 不等于0代表存在吊舱载荷
//	*/
//	uint8_t PayloadOptions;
//
//	GimbalPayloadInfos GimbalPayloadInfos_p;             ///< 吊舱POS数据
//
//	LaserDataInfos LaserDataInfos_p;					///<激光测距信息
//
//};
//
///**
//* @brief 直接地理定位返回数据类型
//*/
//struct GeographicLatLng
//{
//	double Latitude;
//	double Longitude;
//	double Altitude;
//};
//
//#pragma pack()
//
//#endif // !EAPS_META_DATA_STRUCTURE_H
