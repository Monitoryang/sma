#ifndef PioManage_H
#define PioManage_H
// #if 1
#include "jo_sdk.h"
#include "jo_sdk/jo_states.h" // use "jo_sdk/states.h" for TX;"jo_sdk/jo_states.h" for NX

#include <iostream>
#include "ThreadPool.h"
#include "Config.h"
#include "Logger.h"
#include <vector>
#include <queue>
#include <mutex>

using namespace jo_sdk;
namespace eap
{
    namespace sma {
        typedef struct {
            uint8_t sync0;
            uint8_t sync1;
            uint8_t dest;
            uint8_t source;
            uint8_t msgID;
            uint8_t SeqNum;
            uint8_t ACK_NAK;
            uint8_t len;
        } PilotMsmHead;
        
        class PioManage
        {
        public:
            using Ptr = std::shared_ptr<PioManage>;
            PioManage();
            ~PioManage();
            bool publishMsg(std::string topic, const void* data, size_t len);
            void getUAVFilghtStates();
            uint8_t* initPioTopic(std::string topicIn);
            uint8_t* procDatalinkInMsg(const void *data, size_t len);
            void uavSDKInit();
            //std::queue<uint8_t*> piomsg_vector;
            std::queue<std::pair<int,std::array<std::uint8_t,256>>> piomsg_vector;
            env_handle_t _uav_env = nullptr;
            jo_com_raw_t _uav_com = nullptr;
            jo_state_t _uav_sta = nullptr;
            std::mutex piomsg_vector_lock{};
            
        private:
            
            uint8_t* lastProcResult = nullptr;  // 用于存储 procDatalinkInMsg 的返回值

            bool _getUAVStates_run = true;
            bool _get_uav_com_run = true;
            int _247msgSeq_temp = 0; 
        
        };
    }
}
// #endif
#endif