#ifndef JO_VASKIT_COMMON_H
#define JO_VASKIT_COMMON_H

#include "EapsMetaDataStructure.h"
#include "jo_meta_data_structure.h"
//#include "EapsMetaDataProcessing.h"
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#include <opencv2/core/cuda.hpp>
#include <opencv2/core/core.hpp>
#endif

#ifndef _WIN32
#include <stdlib.h>
#endif

#include <stdint.h>
#include <iostream>
#include <list>
#include <vector>
#include <memory>
#include <queue>

namespace eap {
	namespace sma {

		typedef enum {
			HWFailed = -600, //硬件解码器插件不成功
			NotFound = -500,//未找到
			Exception = -400,//代码抛异常
			InvalidArgs = -300,//参数不合法
			SqlFailed = -200,//sql执行失败
			AuthFailed = -100,//鉴权失败
			OtherFailed = -1,//业务代码执行失败�?
			Success = 0,//执行成功
			Busy = 1
		} ApiErr;

		struct ArElementsInternal
		{
			uint32_t Type{};
			uint32_t DotQuantity{};
			int32_t X{};
			int32_t Y{};
			double lon{};
			double lat{};
			double HMSL{};
			uint32_t Category{};
			uint32_t CurIndex{};
			uint32_t NextIndex{};
			std::string Guid{};
		};

		struct ArInfosInternal
		{
			uint32_t ArStatus{};
			uint32_t ArTroubleCode{};
			uint32_t ArElementsNum{};
			std::list<ArElementsInternal> ArElementsArray{};
		};

		struct AiHeatmapInfo
		{
			int new_count_class0{};   
			int new_count_class1{};	
		};

		struct HeatmapData {
			int class0_count;
			int class1_count;
			ImageVertexCoordinate ImgCood;
		};
		
		struct MetaDataWrap
		{
			int64_t pts{};
			JoFmvMetaDataBasic meta_data_basic{};
			std::vector<uint8_t> meta_data_raw_binary{};
			bool meta_data_valid{};

			std::string ar_vector_file{};
			std::queue<int> ar_valid_point_index{};
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
			std::vector<cv::Point> pixel_points{};
			std::vector<std::vector<cv::Point>> pixel_lines{};// TODO: to meta data basic
			std::vector<std::vector<cv::Point>> pixel_warning_l1_regions{};
			std::vector<std::vector<cv::Point>> pixel_warning_l2_regions{};
#endif
			ArInfosInternal ar_mark_info{};
			int64_t original_pts{};
			int64_t current_time{};

			AiHeatmapInfo ai_heatmap_info{};
		};
		using MetaDataWrapPtr = std::shared_ptr<MetaDataWrap>;

		struct CodecImage {
#if defined(ENABLE_GPU) ||  defined(ENABLE_AI) ||  defined(ENABLE_AR)
#ifndef ENABLE_AIRBORNE
	#ifdef ENABLE_GPU
				cv::cuda::GpuMat bgr24_image;
				cv::cuda::GpuMat bgr32_image;
	#else
				cv::Mat bgr24_image;
				cv::Mat bgr32_image;
	#endif
#else
			cv::Mat bgr24_image;
			cv::Mat bgr32_image;
#endif
#endif
			MetaDataWrap meta_data;
			std::string base64_encoded;
			int width{};
			int height{};
			int format;
			bool enable_encode{};
		};
		using CodecImagePtr = std::shared_ptr<CodecImage>;

		struct PilotData
		{
			int64_t UnixTimeStamp{}; // ms

			int64_t Euler0{}; //!< 1E4，基座三轴欧拉角，单位弧度
			int64_t Euler1{}; //!< 1E4，基座三轴欧拉角，单位弧度
			int64_t Euler2{}; //!< 1E4，基座三轴欧拉角，单位弧度

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
			int64_t SeroveCmd{};  
			int64_t OptoSensorCmd{}; //!<  ��ѧ������ָ��

			int64_t CamaraPara{}; //!<��ѧ����ָ�����
			int64_t ServoCmd0{}; //!<吊舱航向指令，1E4，单位rad,rad/s

			int64_t ServoCmd1{};//!<吊舱俯仰指令，1E4，单位rad,rad/s
			int64_t year{}; //!< Integer year value 0����2000��.
			int64_t month{}; //!< Integer month of year 1-12.

			int64_t day{}; //!< Integer day of month 1-31.
			int64_t hour{}; // 
			int64_t minute{}; //
			int64_t second{};  //

			int64_t lat{};  // 1e7
			int64_t lon{};  //1e7

			int64_t HMSL{}; //  m
			int64_t VGnd{}; // 地速       m / s 1e1
			int64_t Tas{}; //����        m/s 1e1

			int64_t pdop{};  //���Ǿ���  1e1
			int64_t numSV{}; //���ǿ�

			int64_t TGTlat{}; //Ŀ��γ��   ��  1e7
			int64_t TGTlon{}; //Ŀ�꾭��   �ȣ� 1e7

			int64_t TGTHMSL{}; //Ŀ��߶�   m
			int64_t ModeState{}; //���չ���ģʽ

			int64_t SlantR{};  //�ɻ�Ŀ�����
			int64_t RPM{};  //������ת��  /60

			int64_t Vnorth{}; //���٣����� m/s��1e2
			int64_t Veast{}; //���٣����� m/s��1e2

			int64_t Vdown{};  //���٣��أ� m/s��1e2
			int64_t ThrottleCmd{}; //����ָ� 1e2
			int64_t DefectFlag{};//�Իٱ�־

			int64_t MPowerA{}; //动力电源 V, *50
			int64_t MpowerV{}; //主电源 V, *100

			int64_t DemSearchLat{}; //高程查找数据 *1e7
			int64_t DemSearchLon{}; //1e7
			int64_t DemSearchAlt{}; //

			int64_t DisFromHome{};  //离家距离
			int64_t HeadFromHome{}; //离家方位角 * 100 （rad）
			int64_t TargetVelocity{}; //目标速度 * 100

			int64_t TargetHeading{}; //目标速度方位角 * 100 (rad)
			/*
			区域搜索任务
			0:NO_TASK,           无任务			OFF
			1:SEARCH_TASK,       搜寻任务		SCH
			2:TP_OBSERVE_TASK,   临时观察任务		TPO
			3:CT_OBSERVE_TASK,   持续观察任务		CTO
			4:TRACK_TASK,        跟踪任务		TRK
			5:IDLE_TASK,         闲置任务		IDL
			*/
			int64_t ScoutTask{}; 
			int64_t apmodestates{}; 

			int64_t TasCmd{};  //空速指令 *10
			int64_t HeightCmd{}; //高度指令 （m）

			int64_t WypNum{}; //跟踪点；
			int64_t VeclType{};  //0:电机  1:油机
			int64_t GimbalDeployCmd{};  ////< 吊舱收放 0:收起 1:放下

			int64_t HMSLCM{}; //�ɻ����и߶ȣ���λcm//

			int64_t Wyplat{}; //��·��γ�ȶ�,  1e7
			int64_t Wyplon{}; //��·�㾭�ȶ�, 1e7

			int64_t WypHMSL{}; //��·��߶� m
			int64_t HMSLDEM{}; //�ɻ�����λ�ò�ѯ�ĸ߶�  m
		};

		struct GimbalData
		{
			int32_t pan{}; //!<����ŷ���ǣ�1e4,��λ����
			int32_t Tilt{}; //!<����ŷ���ǣ�1e4,��λ����

			uint32_t ViewAngle{}; //!<�ӳ���,1e4,��λ����
			int32_t Framepan{};  //!<�����ܽǣ�1e4,��λ����

			int32_t FrameTilt{};  //!<�����ܽǣ�1e4,��λ����
			int32_t DeltaPan{}; //!<����������1e4,��λ����

			int32_t DeltaTilt{};  //!<����������1e4,��λ����
			uint32_t GMPower{}; //���յ�Դ V, *50

			uint32_t DeltaTime{}; //!<�ӳ�ָ��ֵ2

			int32_t roll{}; //!<��ת��ܽǣ�1e4,��λ����
			int32_t FrameRoll{}; //!<��ת��ܽǣ�1e4,��λ����

			int32_t visual_GR{}; //!<�ɼ���ͼ���������ص�����ֱ��ʣ�1e2,��λ����
			uint32_t visual_ZoomAD{}; //�ɼ����о�佹λ��
			uint32_t visual_FocusAD{}; //�ɼ����о�۽�λ��

			int32_t IR_GR{}; 		  //!<����ͼ���������ص�����ֱ��ʣ�1e2,��λ����
			uint32_t IR_ZoomAD{};  //!<�����о�佹λ��
			uint32_t IR_FocusAD{};  //!<�����о�۽�λ��
		};

		struct TxData
		{
			uint32_t VersionNum{}; // !<ͼ������Ӧ�ó���汾��
			uint32_t SeroveCmd{}; //!<���չ���ģʽ
			uint32_t SeroveInit{}; //!<�����Ƿ���ɳ�ʼ��
			uint32_t VisualFOVH{}; //!<�ɼ���ˮƽ�ӳ��ǣ�1e4,��λ����
			uint32_t VisualFOVV{}; //!<�ɼ��ⴹֱ�ӳ��ǣ�1e4����λ����
			uint32_t InfaredFOVH{}; //!<����ˮƽ�ӳ��ǣ�1e4����λ����
			uint32_t InfaredFOVV{}; //!<���ⴹֱ�ӳ��ǣ�1e4����λ����
			uint32_t SearchWidth{}; // !<���������
			uint32_t SearchHeight{}; // !< ������߶�
			uint32_t ServoCrossX{}; // !<�涯X��������
			uint32_t ServoCrossY{}; // !<�涯Y��������
			uint32_t ServoCrossWidth{};  // !<�涯ʮ�ֿ���
			uint32_t ServoCrossheight{}; // !<�涯ʮ�ָ߶�
			uint32_t TrackLefttopX{}; // !<���ٿ����Ͻ�X����++++++++++
			uint32_t TrackLefttopY{}; // !< ���ٿ����Ͻ�Y����+++++++++++
			uint32_t TrackWidth{}; // !<���ٿ����++++++++++
			uint32_t TrackHeight{}; // !< ���ٿ�߶�++++++++++
			uint32_t SDMemory{}; // !<��λG��*10++++++++++++
			int32_t TimeZoneNum{}; //!<ʱ����
			uint32_t SDGainVal{}; //!�����������ֵ��*10
			uint32_t SnapNum{}; // !<������
			int32_t GimbalRoll{}; //!<��ת�ǣ�1e4,��λ����
			uint32_t OSDFlag{}; //!<OSD״̬
			uint32_t TrackFlag{}; //!<����״̬
			uint32_t StableFlag{}; //!<����״̬
			uint32_t ImageAdjust{}; //!<��ƫ����
			uint32_t SDFlag{}; //!<SD��״̬
			uint32_t RecordFlag{}; //!<¼��״̬
			uint32_t FlyFlag{}; //!<����״̬
			uint32_t MTI{}; //!<�˶����
			uint32_t AI_R{}; //!< ��������
			uint32_t IM{}; //!< �ں�
			uint32_t W_or_B{};  //!<���Ȼ����
			uint32_t IR{};//!<  α��
			uint32_t HDvsSD{};//!< ��������
			uint32_t CarTrack{};//!<��������״̬
			uint32_t TrackClass{};//!< ��������״̬
			uint32_t FlySeconds{}; //!<//�ɻ�����seconds//			
			uint32_t Encryption{}; //!< //!<����flag,����������
			uint32_t DetectionFlag{};//!< //���flag,����������//
			uint32_t DetNum{}; //Ŀ���⵽������ //
			uint32_t FovLock{}; //�ӳ����Ƿ�����//
			uint32_t TrackStatus{}; //!<����״̬��0x00���ǳ������� ��  0x01:�������������ʰ�������0x02:����״̬���ѣ������Ǹ���̬�����ر�RTT;0x03:����������
			uint32_t DetRltStatus{}; //�Ƿ��⵽Ŀ��//
			uint32_t VisualFOVHMul{}; //!<�ɼ����ӳ�����1e2
			uint32_t InfaredFOVHMul{}; //!<�����ӳ�����1e2
			uint32_t AiStatus{};//AI״̬  0.�ر�  1.����
			uint32_t PIP{};          //���л�  0.�ر�  1.����
			uint32_t ImgStabilize{};    //��������  0.�ر�  1.����
			uint32_t ImgDefog{};        //ȥ��   0.�ر�  1.����
			uint32_t Pintu{};   //ƴͼ
			uint32_t DzoomWho{};   //���ӷŴ�ִ�ж� :0. ���� 1.������
		};

		struct LaerDataProcMsg
		{
			uint32_t LaserStatus{};  //*激光测距机的状态;0:表示无效；1：单次测距；2：连续测距；3：关闭*/
			uint32_t LaserMeasVal{}; //*激光测距值,当测距无异常时，此值为激光测距值,单位米，否则显示超量程*/
			uint32_t LaserMeasStatus{}; //*激光测距异常态监测：bit0---FPGA系统状态，bit1-激光出光状态，bit2-主波监测状态，bit3-回波检测状态，bit4-偏压开关状态，bit5-偏压输出状态，bit6-温度状态，bit7-出光关断状态，上述状态位中1表示正常，0表示异常*/
			int32_t Laserlat{};//*激光测距点纬度值,单位度，1E7*/
			int32_t Laserlon{};//*激光测距点经度值，单位度，1E7*/
			int32_t LaserHMSL{};//*激光测距点高度值，单位米*/
			uint32_t LaserRefStatus{};//*激光测距靶点状态；0：无效；1：表示估距，2：表示激光测距*/
			int32_t LaserRefLat{};//*激光测距靶点纬度值,单位度，1E7*/
			int32_t LaserRefLon{};//*激光测距靶点经度值,单位度，1E7*/
			int32_t LaserRefHMSL{};//*激光测距靶点高度值，单位米*/
			uint32_t LaserCurStatus{};//*激光测距弹着点状态；0：无效；1：表示估距，2：表示激光测距*/
			int32_t LaserCurLat{};//*激光测距弹着点纬度值,单位度，1E7*/
			int32_t LaserCurLon{};//*激光测距弹着点经度值,单位度，1E7*/
			int32_t LaserCurHMSL{};//*激光测距弹着点高度值，单位米*/
			int32_t LaserDist{};//*激光测距靶点与弹着点的距离，单位米*/
			int32_t LaserAngle{};//*激光测距靶点与弹着点的角度，单位弧度，1E4*/
			uint32_t LaserModel{};//*测距结果模式，0x00:测距结果为单目标,0x01:测距结果为前目标；0x02:测距结果为后目标；0x03:测距结果为前目标和后目标；0x04:测距结果为超距*/
			uint32_t LaserFreq{};//*若测距模式为连续测距，则此值为连续测距频率；若测距模式为单次测距，则此值为单次测距个数*/
			uint32_t res1{};//*保留1*/
			uint32_t res2{};//*保留2*/
		};

		struct TargetTemp
		{
			uint32_t id{};				//目标编号
			uint32_t width{};			//目标像素高度
			uint32_t height{};			//目标像素高度
			uint32_t pixelX{};			//目标位置中心点坐标X
			uint32_t pixelY{};			//目标位置中心点坐标Y
			int32_t maxT{};				//区域最高温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
			int32_t minT{};				//区域最低温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
			int32_t centreT{};			//区域中心点温度（单位摄氏度，精度0.1摄氏度，全F表示无效）
			int32_t meanT{};			//区域平均温度（单位摄氏度，精度0.1摄氏度，全F表示无效)
			uint32_t irPointTemp{};		//红外点测温状态
			uint32_t irAreaTemp{};		//红外区域测温状态
		};

		struct IrThermometerBack
		{
			uint32_t msgid{};	//消息包ID
			uint32_t Time{};	//消息产生时戳
			uint32_t num{};		//目标区域数量
			TargetTemp TargetTemp_p{};	
		};

		struct PayloadData
		{
			int64_t UnixTimeStamp{}; // ms
			GimbalData GimbalData_p{};
			TxData TxData_p{};
			LaerDataProcMsg LaerDataProcMsg_p{};
			IrThermometerBack IrThermometerBack_p{};
		};

		struct JoEdgeVersion
		{
			uint8_t DataBackMajor{};
			uint8_t DataBackMinor{};
			uint8_t DataBackPatch{};
			uint8_t DataBackBuild{};
			uint8_t PayloadMajor{};
			uint8_t PayloadMinor{};
			uint8_t PayloadPatch{};
			uint8_t PayloadBuild{};
			uint8_t PilotMajor{};
			uint8_t PilotMinor{};
			uint8_t PilotPatch{};
			uint8_t PilotBuild{};
			uint8_t SmaMajor{};
			uint8_t SmaMinor{};
			uint8_t SmaPatch{};
			uint8_t SmaBuild{};
		};

		struct AiassistTrackResults
		{
			int track_cmd{};        // 跟踪指令 0.无动作 1.基于像素坐标进入跟踪 2.基于目标ID进入跟踪 3.退出跟踪
			// int track_id;         // 跟踪目标ID
			// int servo_track_flag; // 跟踪目标是否为舵机跟踪 0.伺服跟踪 1.伺服不跟踪
			int track_mask_plate; // 跟踪目标模版大小等级 0.
			int track_pixelpos_x{}; // 跟踪目标像素坐标x
			int track_pixelpos_y{}; // 跟踪目标像素坐标y
			int track_pixelpos_w{}; // 跟踪目标像素坐标宽度w
			int track_pixelpos_h{}; // 跟踪目标像素坐标宽度h
		};
		enum class SmaMsgId
		{
			DeviceMsgID = 901
		};

		std::string pluginGuid();
	}
}
#endif // !JO_VASKIT_COMMON_H
