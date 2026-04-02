#include "pioPublish.h"
#include <functional>
#include <string>
namespace eap
{
    namespace sma {
#ifdef ENABLE_PIO
        PioPublish::PioPublish()
        {
            try{
                char piopilotcfg[256];
                memset(&piopilotcfg, 0, 256);
                snprintf(piopilotcfg, sizeof piopilotcfg, R"({"target":{"ip":"%s"}})", "127.0.0.1");
                 
                env_handle_t env_pilot = jo_sdk::env_create(NULL);
                int ret = jo_sdk::get_user_access(env_pilot, "jouav");
                std::cout << "ret: "<<ret<<std::endl; 
                eap_information_printf("ret: %d",ret);
                _piomanage = jo_sdk::create_com_raw(env_pilot);
                 // _piomanage = std::make_shared<PioManage>();
                if(!_piomanage){
                    eap_error("It is failed to create PioManage Ptr, pio function is disable, please fix it!");
                }else{
                    #if 0
                    _piomanage->subscribe("/msg/joedge/ai/out", [this](const std::string &topic, const void* data, size_t len)
                                                    {
                                                        eap_information("/msg/joedge/ai/out subscirbe success!");
                                                        return 0;
                                                    });
                    #endif
                }
               
            }
            catch(const std::exception &e){
                eap_error_printf("It is failed to create PioManage Ptr, pio function is disable, please fix it: %s", e.what());
            }

        }

        PioPublish::~PioPublish()
        {

        }
#ifdef ENABLE_AI
        AiInfosPio PioPublish::AiInfosPreprocess(const std::vector<joai::Result> &detect_objects, const int &img_width, const int &img_height, const float & fire_conf_thresh, const float & smoke_conf_thresh)
        {
            _class_groups.clear();
            _selected_detections.clear();
            for (const auto& obj : detect_objects) {
                // 获取检测框
                float x = obj.Bounding_box.x;
                float y = obj.Bounding_box.y;
                float w = obj.Bounding_box.width;
                float h = obj.Bounding_box.height;
                // 只要有一点在画面内就保留,用于检测
                if (x >= img_width || y >= img_height || (x + w) <= 0 || (y + h) <= 0) {
                    eap_information_printf("skip this target which is out of image, cls: %d,TgtConfidence: %d",(int)obj.cls,(int)obj.confidence);
                    continue; // 跳过这个目标
                }
                bool fire_condition = (obj.cls == 0) && (obj.confidence > fire_conf_thresh);
                bool smoke_condition = (obj.cls == 1) && (obj.confidence > smoke_conf_thresh);
                if (fire_condition || smoke_condition) {  // 只处理 fire 和 smoke
                    _class_groups[obj.cls].push_back(obj);
                    #if 0
                    const auto cls_str = std::to_string(obj.cls);
                    const auto conf_str = std::to_string(obj.confidence);
                    eap_information_printf("passed in pio! pobj.cls: %s,obj.confidence: %s",cls_str,conf_str);
                    #endif
                }
            }

            // 对每个类别按置信度降序排序，并取前5个
            for (auto& [cls, objs] : _class_groups) {
                std::sort(objs.begin(), objs.end(), [](const joai::Result& a, const joai::Result& b) {
                    return a.confidence > b.confidence;
                });

                size_t count = std::min(objs.size(), static_cast<size_t>(5));
                for (size_t i = 0; i < count; ++i) {
                    const auto& obj = objs[i];
                    AIDetcInfoPio info;
                    info.DetLefttopX = static_cast<uint32_t>(obj.Bounding_box.x);
                    info.DetLefttopY = static_cast<uint32_t>(obj.Bounding_box.y);
                    info.DetWidth    = static_cast<uint32_t>(obj.Bounding_box.width);
                    info.DetHeight   = static_cast<uint32_t>(obj.Bounding_box.height);
                    info.DetTGTclass = obj.cls;
                    info.TgtConfidence = static_cast<uint32_t>(obj.confidence * 100); // float [0,1] -> uint32_t [0,100]

                    _selected_detections.push_back(info);
                }
            }

            // 构造 AiInfos 结构体
            AiInfosPio ai_infos{};
            ai_infos.SubMsgID=106;
            ai_infos.AIStatus=1;
            // 填充 AIDetcInfoArray，最多10个
            ai_infos.AIDetcInfoSize = static_cast<uint32_t>(std::min(_selected_detections.size(), static_cast<size_t>(10)));

            #if 1
            time_t now = time(NULL);
            static time_t last_print = 0;
            bool print_condition= (now - last_print >= 1);
            if(print_condition && ai_infos.AIDetcInfoSize >0 ){
                last_print = now;
                eap_information_printf("AIDetcInfoSize: %d",(int)ai_infos.AIDetcInfoSize);
            }
            #endif

            for (size_t i = 0; i < ai_infos.AIDetcInfoSize; ++i) {
                ai_infos.AIDetcInfoArray[i] = _selected_detections[i];
                if(print_condition){
                    eap_information_printf("DetTGTclass:%d,confidence: %s,index: %d,DetLefttopX: %d",
                    (int)_selected_detections[i].DetTGTclass,std::to_string(_selected_detections[i].TgtConfidence),(int)i+1,(int)_selected_detections[i].DetLefttopX);
                }
            }


            // 如果不足10个，其余保持初始化为0（已由 {} 初始化）
            return ai_infos;
        }
        void PioPublish::PioAiInfosPublish(const AiInfosPio &ai_infos)
        {
            // 1. 定义总缓冲区（足够大）
            
            std::uint8_t* data = _ai_msg_out.data();      // 直接获取可写指针，无需 const_cast

           // 2. 设置 header
            Head_t hdr{};
            hdr.SYNC0 = 0xb5;
            hdr.SYNC1 = 0x62;
            hdr.MSGID = 0xEC;
            // 3. 计算负载大小
            size_t ai_infos_size = sizeof(AiInfosPio);
            hdr.LEN = static_cast<uint8_t>(ai_infos_size); 

            // 4. 拼接结构体：先写 header，再写 payload
            // 合并：先写头部，再写 AiInfos
            std::memcpy(data, &hdr, sizeof(hdr));
            std::memcpy(data + sizeof(hdr), &ai_infos, sizeof(ai_infos));

            // 计算总长度（含头，不含 cka/ckb）
            size_t total_len = sizeof(hdr) + sizeof(ai_infos); // 不包括 cka,ckb
            size_t packet_len = total_len + 2; // 包括 cka,ckb

            // 5. 计算校验和
            std::uint8_t cka = 0;
            std::uint8_t ckb = 0;

            // 校验范围：从 data[0] (SYNC0) 开始，长度为 total_len（即整个包，不含 cka/ckb）
            //更正：从去掉头两位开始
            MessageCheckSum2(&data[2], &cka, &ckb, total_len-2);

            // 6. 写入校验码
            data[total_len]     = cka;
            data[total_len + 1] = ckb;
        #if 0
            printf("send topic /msg/joedge/ai/out :::");
            for (int i=0;i<packet_len;i++)
            {
                printf("%02x ", _ai_msg_out.data()[i]);
            }
            printf("\n");
        #endif
            // 7. 发送
        #if 1
            if(_piomanage){
                auto publish_result=_piomanage->publish("/msg/joedge/ai/out", _ai_msg_out.data(), packet_len);
                if(publish_result!=0){
                    eap_information_printf("pio publish_result:%d",publish_result);
                }
            }
        #endif
        }
#endif
        void PioPublish::MessageCheckSum2(std::uint8_t *msg, std::uint8_t *CKA, std::uint8_t *CKB, std::uint32_t len)
        {
            *CKA = 0;
            *CKB = 0;

            for (std::uint32_t i = 0; i < len; i++)
            {
                *CKA += *(msg++);
                *CKB += *CKA;
            }
        }

        bool PioPublish::GetIrAiMsgPio(IrAiMsg& ir_ai_msg)
        {  
            std::lock_guard<std::mutex> ir_ai_msg_lock(_ir_ai_msg_mtx);
            if(!_has_ir_ai_msg_in.load()){
                return false;
            }else{
                ir_ai_msg = _ir_ai_msg;
                _has_ir_ai_msg_in.store(false);
                return true;
            }
        }

        void PioPublish::SubscribeIrAiMsgPio()
        {
            if(_piomanage){
                _piomanage->subscribe("/msg/joedge/infrared/ai/out", [this](const std::string &topic, const void* data, size_t len)
                                                    {
                                                        if (len <= 0)
                                                        {
                                                            eap_information(" GetIrAiMsgPio msg length is : " + std::to_string(len));
                                                            return 1;
                                                        }
                                                        if (len > _ir_ai_msg_in.size()) {
                                                            eap_error("IR AI msg too long: "+ std::to_string(len));
                                                            return 1;
                                                        }
                                                        std::uint8_t *data_ = const_cast<std::uint8_t *>(_ir_ai_msg_in.data());
                                                        memcpy(data_, data, len);
                                                        std::uint8_t cka = 0;
                                                        std::uint8_t ckb = 0;
                                                        if ((data_[0] == 0xb5) && (data_[1] == 0x62) && (data_[4] == 0xEC))
                                                        {
                                                            // 判断校验和
                                                            int l = len - 4;
                                                            MessageCheckSum2(&data_[2], &cka, &ckb, l);
                                                            if (cka == data_[len - 2] && ckb == data_[len - 1])
                                                            {
                                                               
                                                                std::lock_guard<std::mutex> ir_ai_msg_lock(_ir_ai_msg_mtx);
                                                                std::memcpy(&_ir_ai_msg, data_, sizeof(IrAiMsg));
                                                                _has_ir_ai_msg_in.store(true);
                                                                #if 0
                                                                eap_information("AIStatus: " + std::to_string(int(_ir_ai_msg.AIStatus)));
                                                                eap_information("AIDetcInfoSize: " + std::to_string(int(_ir_ai_msg.AIDetcInfoSize)));
                                                                for(int i=0;i<int(_ir_ai_msg.AIDetcInfoSize);++i){
                                                                    auto ir_ai_pio=_ir_ai_msg.AIDetcInfoArray[i];
                                                                    eap_information("i: " + std::to_string(i));
                                                                    eap_information("DetLefttopX: " + std::to_string(int(ir_ai_pio.DetLefttopX)));
                                                                    eap_information("DetLefttopY: " + std::to_string(int(ir_ai_pio.DetLefttopY)));
                                                                    eap_information("DetWidth: " + std::to_string(int(ir_ai_pio.DetWidth)));
                                                                    eap_information("DetHeight: " + std::to_string(int(ir_ai_pio.DetHeight)));
                                                                    eap_information("DetTGTclass: " + std::to_string(int(ir_ai_pio.DetTGTclass)));
                                                                    eap_information("TgtConfidence: " + std::to_string(int(ir_ai_pio.TgtConfidence)));
                                                                    eap_information("FireArea: " + std::to_string(int(ir_ai_pio.FireArea)));
                                                                    eap_information("FireTemperature: " + std::to_string(int(ir_ai_pio.FireTemperature)));
                                                                    eap_information("--------------------------------------------");
                                                                }
                                                                #endif
                                                                return 0;

                                                            }
                                                            else
                                                            {
                                                                eap_error("GetIrAiMsgPio msgid !=0xEC  cka or ckb error");
                                                                return 1;
                                                            }
                                                        }
                                                    });
            }
        }
#endif
    }
}