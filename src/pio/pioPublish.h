#ifndef PioPublish_H
#define PioPublish_H
#include "pioManage.h"
#ifdef ENABLE_AI
#include "Track.h"
#endif
#include <atomic>
namespace eap
{
    namespace sma {
        typedef struct 
        {
            uint8_t SYNC0;			// 0xb5
            uint8_t SYNC1;			// 0x62
            uint8_t DEST;				//
            uint8_t SOURCE;			//
            uint8_t MSGID;			// 消息ID（0xEC）
            uint8_t SEQ;				//
            uint8_t ACK;				//
            uint8_t LEN;				// 负载数据长度
        }Head_t;//消息头
        //消息尾
        // uint8_t CKa;		  // 校验A
        // uint8_t CKb;		  // 校验B

        /**
        * @brief 检测对象参数(火点烟雾)
        */
        struct AIDetcInfoPio
        {
            uint32_t DetLefttopX{};				//< 跟踪框左上角X坐标
            uint32_t DetLefttopY{};				//< 跟踪框左上角Y坐标
            uint32_t DetWidth{};				//< 跟踪框宽度
            uint32_t DetHeight{};				//< 跟踪框高度

            int32_t DetTGTclass{};              //< 目标类别,3(fire),10(smok)
            uint32_t TgtConfidence{};			//< 目标置信度 ，1e2
        };
        /**
        * @brief AI相关信息，payloat_t内容为AiInfos
        */
        struct AiInfosPio
        {
            uint8_t MsaId;
            uint8_t SubMsgID=106;
            uint8_t AIStatus{};//AI状态 0关闭 1开启
            uint8_t AIDetcInfoSize{};//AI检测数据 数量
            AIDetcInfoPio AIDetcInfoArray[10];//AI检测数据，仅展示置信度最高的10个目标(火点、烟雾目标各前5)
        };

        struct IRAIInfoPio
	    {
            uint16_t DetLefttopX;	//< 跟踪框左上角X坐标
            uint16_t DetLefttopY;	//< 跟踪框左上角Y坐标
            uint16_t DetWidth;	    //< 跟踪框宽度
            uint16_t DetHeight;	    //< 跟踪框高度
            uint16_t DetTGTclass;     // < 目标类别, 0(fire)
            uint16_t TgtConfidence;	 // < 目标置信度(单位：%) ，1e2
            uint32_t FireArea;       // 火点面积
            int32_t FireTemperature; // 火点温度，单位摄氏度（-127,127）
	    };
        typedef struct
        {
            UINT8 SYNC0;  // 0xb5
            UINT8 SYNC1;  // 0x62
            UINT8 DEST;   //
            UINT8 SOURCE; //
            UINT8 MSGID;  // 消息ID（0xEC）
            UINT8 SEQ;    //
            UINT8 ACK;    //
            UINT8 LEN;    // 负载数据长度
            UINT8 MsaId;
            UINT8 SubMsgID = 107; // 6B
            UINT8 AIStatus;         // AI状态 0关闭 1开启
            UINT8 AIDetcInfoSize;   // AI检测数据 数量
            IRAIInfoPio AIDetcInfoArray[10];    // AI检测数据，仅展示温度最高的10个目标
            UINT8 CKa;              // 校验A
            UINT8 CKb;              // 校验B
        }IrAiMsg;


#ifdef ENABLE_PIO
        class PioPublish
        {
        public:
            using Ptr = std::shared_ptr<PioPublish>;
            PioPublish();
            ~PioPublish();
#ifdef ENABLE_AI
            AiInfosPio AiInfosPreprocess(const std::vector<joai::Result> &detect_objects, const int &img_width, const int &img_height,
                                         const float & fire_conf_thresh, const float & smoke_conf_thresh);
            void PioAiInfosPublish(const AiInfosPio &ai_infos);
#endif
            void SubscribeIrAiMsgPio();
            bool GetIrAiMsgPio(IrAiMsg& ir_ai_msg);
        private:
            void MessageCheckSum2(std::uint8_t *msg, std::uint8_t *CKA, std::uint8_t *CKB, std::uint32_t len);
            jo_com_raw_t _piomanage{};
            // PioManage::Ptr _piomanage{};
#ifdef ENABLE_AI
            //按类别分组
            mutable std::unordered_map<int, std::vector<joai::Result>> _class_groups;
            //存储筛选并排序后的 AIDetcInfoPio
            mutable std::vector<AIDetcInfoPio> _selected_detections;
#endif
            std::array<std::uint8_t, 256> _ai_msg_out{};  // 零初始化
            std::array<std::uint8_t, 4096> _ir_ai_msg_in{};  // 零初始化
            std::mutex  _ir_ai_msg_mtx{};
            IrAiMsg _ir_ai_msg{};
            std::atomic<bool> _has_ir_ai_msg_in{false}; 
        };
#endif
    }
}
// #endif
#endif