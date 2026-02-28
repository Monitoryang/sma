// #if 1
#include "pioManage.h"
using namespace eap;
namespace eap
{
    namespace sma {
        PioManage::PioManage()
        {
            uavSDKInit();
            eap_information("new PioManage");
        }

        PioManage::~PioManage()
        {
            _get_uav_com_run = false;
            eap_information("delete PioManage");
        }

        void PioManage::uavSDKInit()
        {
            while (_get_uav_com_run)
            {
                char str[256];
                snprintf(str, sizeof(str), R"({"target":{"ip":"%s"}})", "127.0.0.1");
                std::cout << str << "\n"
                        << std::endl;
                _uav_env = env_create(str);
                int ret = jo_sdk::get_user_access(_uav_env, "jouav");
                if (ret == 0)
                {
                    _uav_com = create_com_raw(_uav_env);
                    _uav_sta = create_states_cli(_uav_env);
                    _get_uav_com_run = false;
                    eap_information("uavSdk init successful");
                }
                else
                {
                    eap_information("uavSdk init faild, ret is: " + std::to_string(ret));
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
        }

        uint8_t *PioManage::initPioTopic(std::string topicIn)
        {
            if (!_uav_com)
            {
                eap_information("_uav_com uninit");
            }
            else
            {
                _uav_com->subscribe(topicIn, [this](const std::string &topic, const void *data, size_t len)
                                    {
                lastProcResult = procDatalinkInMsg(data, len);
                return 0; });
            }
            return lastProcResult;
        }

        uint8_t *PioManage::procDatalinkInMsg(const void *data, size_t len)
        {
            const uint8_t *cur_data = static_cast<const uint8_t *>(data);
            if (cur_data == nullptr)
            {
                return NULL;
            }

            if (cur_data[0] == 0xb5 && cur_data[1] == 0x62)
            { 
                if ((cur_data[4] == 0xF7) || (cur_data[4] == 0x69)) // 105、 247
                {
                    //if (_247msgSeq_temp != cur_data[5])
                    {
                        _247msgSeq_temp = cur_data[5];
                        // eap_information("receive 247 msg 2222222222222222222222222222222222222222");
                        
                        std::array<std::uint8_t,256> array_pio;
                        memcpy(&array_pio,cur_data,len);
                        // printf("pioManage\n");
                        // for (int i = 0; i < len; i++)
                        // {
                        //     printf(" %02x", array_pio[i]);
                        // }
                        // printf("\n");
                        piomsg_vector_lock.lock();
                        //piomsg_vector.push(const_cast<uint8_t *>(cur_data));
                        piomsg_vector.push(std::make_pair(len,array_pio));
                        piomsg_vector_lock.unlock();
                        return const_cast<uint8_t *>(cur_data);
                    }
                }
                else
                {
                    return const_cast<uint8_t *>(cur_data);
                }
            }
            else /* if (cur_data[0] == 0xFD) // 成至载荷 */
            {
    #if 0
                static int printf_req0 = 0;
                //if (printf_req0 % 200 == 0)
                {
                    printf("receive pio \n");
                    for (int i = 0; i < 25; i++)
                        printf("%02X ", cur_data[i]);
                    printf("\n");
                }
                printf_req0++;
    #endif
                //Poco::Thread::sleep(200);
                return const_cast<uint8_t *>(cur_data);
            }
        }

        bool PioManage::publishMsg(std::string topic, const void *data, size_t len)
        {
            try
            {
                if (_uav_com)
                {
                    auto ret = _uav_com->publish(topic, data, len);
                    if (ret == 0)
                    {
                        // eap_information("send data to " + topic + " successful");
                        return true;
                    }
                    else
                    {
                        eap_information("send data to " + topic + " faild");
                        return false;
                    }
                }
                else
                {
                    eap_error("_uav_com ptr is null");
                    return false;
                }
            }
            catch (const std::exception &e)
            {
                eap_error("_uav_com publishMsg faild, error is: " + std::string(e.what()));
            }
        }
    }
}

// #endif