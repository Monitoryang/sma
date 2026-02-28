#include "EapsDispatchCenter.h"
#include "EapsNoticeCenter.h"
#include "EapsConfig.h"
#include "EapsUtils.h"
#include "jo_rtc_global.h"
#include "Logger.h"
#include "Poco/ThreadPool.h"
#include "Poco/File.h"
#include "Poco/JSON/Parser.h"
#include <Poco/DirectoryIterator.h>
#include "OnceToken.h"
#ifdef ENABLE_AI
#include "jo_ai_object_detect.h"
#include <cuda_runtime_api.h>
#endif

#include <stdexcept>
#include <system_error>
#include <sys/stat.h>
#include <fstream>

namespace eap {
    namespace sma {
        std::mutex DispatchCenter::s_inst_mutex{};
        DispatchCenterPtr DispatchCenter::s_instance{};

        DispatchCenter::DispatchCenter()
        {
            std::string tasks_file_path = exeDir() + "tasks/";
            bool result = createPath(tasks_file_path);
            if (!result) {
                eap_error("create task file path fail");
            }
            clearTasksFile();
        }

        DispatchCenterPtr DispatchCenter::Instance()
        {
            std::lock_guard<std::mutex> lock(s_inst_mutex);
            if (!s_instance) {
                s_instance = DispatchCenterPtr(new DispatchCenter());
            }

            return s_instance;
        }

        static void ffmpeg_log_callback(void* avcl, int log_level, const char* format, va_list args) {
            char out[4096];
            vsnprintf(out, sizeof(out), format, args);

            std::string out_str = out;
            if (out_str.find("SEI") != std::string::npos || out_str.find("deprecated") != std::string::npos) {
                return;
            }

            auto n = out_str.find_last_not_of('\n');
            if (n != std::string::npos) {
                //out_str.erase(n + 1, out_str.size() - n);
                out[n + 1] = '\0';
            }

            switch (log_level)
            {
            case AV_LOG_ERROR:
                eap_error_printf("av error: %s", out_str);
                break;
            case AV_LOG_PANIC:
                eap_error_printf("av panic: %s", out_str);
                break;
            case AV_LOG_FATAL:
                eap_error_printf("av fatal: %s", out_str);
                break;
            case AV_LOG_INFO:
                eap_information_printf("av info: %s", out_str);
                break;
            default:
                break;
            }
        }

        static void rtc_log_callback(joRtcLogLevel log_level, const char* msg, void* opaque)
        {
            (void(opaque));

            if (!msg) {
                return;
            }
            std::string message(msg);

            switch (log_level)
            {
            case JO_RTC_LOG_NONE:
                eap_information_printf("rtc none: %s", message);
                break;
            case JO_RTC_LOG_FATAL:
                eap_information_printf("rtc fatal: %s", message);
                break;
            case JO_RTC_LOG_ERROR:
                eap_information_printf("rtc error: %s", message);
                break;
            case JO_RTC_LOG_WARNING:
                eap_information_printf("rtc warning: %s", message);
                break;
            case JO_RTC_LOG_INFO:
                eap_information_printf("rtc info: %s", message);
                break;
            case JO_RTC_LOG_DEBUG:
                eap_information_printf("rtc debug: %s", message);
                break;
            case JO_RTC_LOG_VERBOSE:
                eap_information_printf("rtc verbose: %s", message);
                break;
            default:
                eap_information_printf("rtc log: %s", message);
                break;
            }
        }

        void DispatchCenter::cudaWarmUp()
        {
            static std::once_flag once_flag{};
            std::call_once(once_flag, []()
            {
#ifdef ENABLE_CUDA
                auto start_t = std::chrono::system_clock::now();
                void* cuda_mem{};
                size_t cuda_size{};
                cudaError_t err = cudaMallocPitch(&cuda_mem, &cuda_size, 1820 * 4, 1080);
                if (cuda_mem && err == cudaSuccess) {
                    cudaFree(cuda_mem);
                }

                auto end_t = std::chrono::system_clock::now();
                eap_information_printf("cuda warm up elpsed time: %lldms",
                    std::chrono::duration_cast<std::chrono::milliseconds>(end_t - start_t).count());
#endif
            });
        }

        void DispatchCenter::receiveVersinData(std::string version_data)
        {
            try {
                // 没有实现多路流的，所以这里也就只调用了单路流task中的
                DispatchTaskImpl::receiveVersinData(version_data);
            } catch (const std::exception& e) {
                eap_error_printf("receiveVersinData throw exception: %s", std::string(e.what()));
            } catch (...) {
                eap_error_printf("receiveVersinData throw exception: %s", std::string("unknown error"));
            }
        }

        DispatchCenter::~DispatchCenter()
        {
            stop();
            clearTasksFile();
        }

        void DispatchCenter::start()
        {

#ifndef ENABLE_3588
            av_register_all();
#endif // !ENABLE_3588
            av_log_set_flags(AV_LOG_SKIP_REPEATED);
            av_log_set_level(AV_LOG_ERROR);
            av_log_set_callback(ffmpeg_log_callback);

            eap_information("ffmpeg log inited");

            jo_rtc_init_log(JO_RTC_LOG_ERROR, rtc_log_callback);

            eap_information("rtc log inited");
        }

        void DispatchCenter::stop()
        {
            try {
                {
                    std::lock_guard<std::mutex> lock(_task_mutex);
                    for (auto& task : _dispatch_task_map) {
                        if (task.second) {
                            task.second->stop();
                        }
                    }
                    _dispatch_task_map.clear();

                    for (auto& task : _dispatch_task_map_default) {
                        if (task.second) {
                            task.second->stop();
                        }
                    }
                    _dispatch_task_map_default.clear();
                }

                {
                    std::lock_guard<std::mutex> lock(_task_mutex_multiple);
                    for (auto& task : _dispatch_task_map_multiple) {
                        if (task.second) {
                            task.second->stop();
                        }
                    }
                    _dispatch_task_map_multiple.clear();
                }


                { // 此种写法会导致如果上一个锁无法获取到，就会等上一个锁获取到后直到解锁后才能执行当前。而当前有可能并没有被锁住
                    std::lock_guard<std::mutex> lock(_record_task_mutex);
                    for (auto& task : _record_task_map) {
                        if (task.second) {
                            task.second->stop();
                        }
                    }
                    _record_task_map.clear();
                }
            }
            catch (const std::exception& e) {
                eap_information_printf("----DispatchCenter stop failed, msg: %s", std::string(e.what()));
            }
        }

        void DispatchCenter::addDefaultVisualTasks(std::string& visual_guid)
        {
            std::string visual_input_url{};
            std::string visual_multicast_url{};
            std::string visual_sms_url{};
            std::string record_hd_file_prefix{};
            std::string record_sd_file_prefix{};
            int func_mask{ 0 };
            try {
                GET_CONFIG(std::string, getString, my_visual_input_url, Media::kVisualInputUrl);
                GET_CONFIG(std::string, getString, my_visual_multicast_url, Media::kVisualMulticastUrl);
                GET_CONFIG(std::string, getString, my_visual_sms_url, Media::kVisualSmsUrl);
                GET_CONFIG(std::string, getString, my_record_hd_file_prefix, Media::kRecordHDFilePrefix);
                GET_CONFIG(std::string, getString, my_record_sd_file_prefix, Media::kRecordSDFilePrefix);
                visual_input_url = my_visual_input_url;
                visual_multicast_url = my_visual_multicast_url;
                visual_sms_url = my_visual_sms_url;
                record_hd_file_prefix = my_record_hd_file_prefix;
                record_sd_file_prefix = my_record_sd_file_prefix;
                if (eap::configInstance().has(Media::kVisualFuncmask)) {
                    GET_CONFIG(int, getInt, my_visual_funcmask, Media::kVisualFuncmask);
                    func_mask = my_visual_funcmask;
                }
                else {
                    eap::configInstance().setInt(Media::kVisualFuncmask, func_mask);
                    eap::saveConfig();
                }
            }
            catch (const std::exception& e) {
                eap_error_printf("get config kSecret throw exception: %s", e.what());
            }

            std::string record_time_str = get_current_time_string_second_compact();

            //visual
            DispatchTask::InitParameter init_paramter_visual;
            init_paramter_visual.id = visual_guid;

            if (!visual_input_url.empty()) {
                init_paramter_visual.pull_url = visual_input_url;
            }
            else {
                throw std::invalid_argument("kVisualInputUrl is null");
            }

            if (!visual_multicast_url.empty()) {
                init_paramter_visual.push_url = visual_multicast_url;
                init_paramter_visual._push_urls.push_back(visual_multicast_url);
            }
            else {
                throw std::invalid_argument("kVisualMulticastUrl is null");
            }

            if (!visual_sms_url.empty()) {
                //支持新建任务推两个地址
                init_paramter_visual._push_urls.push_back(visual_sms_url);
            }

            init_paramter_visual.func_mask = func_mask;//默认不开启录像
            init_paramter_visual.record_file_prefix = record_hd_file_prefix;
            init_paramter_visual.ar_vector_file = "";
            init_paramter_visual.ar_camera_config = "";
            init_paramter_visual.record_time_str = record_time_str;
            init_paramter_visual.is_pilotcontrol_task = true;
            init_paramter_visual.need_decode = true;

            DispatchTaskPtr task_ptr{};
            auto task = findTaskUrl(init_paramter_visual.pull_url, init_paramter_visual.push_url, task_ptr);
            if (task_ptr) {
                auto task_id = task_ptr->getId();
                auto ret = removeTask(task_id);
                eap_information_printf("Task already existed, removeTask id: %s, pull url: %s, push_url: %s, ret: %d", task_id, init_paramter_visual.pull_url, init_paramter_visual.push_url, (int)ret);
            }

            int64_t duration{};
            try {
                auto device_callback = [this](std::string content) {
                    if (_rpc_server)
                        _rpc_server->publish("mrpAddRPRecord", content);
                };

                eap_information_printf("visual default task start, pull url is: %s, push url is: %s", init_paramter_visual.pull_url, init_paramter_visual.push_url);
                auto task_now = DispatchTaskImpl::createInstance(init_paramter_visual);
                task_now->setId(init_paramter_visual.id);
                task_now->setDeviceInfoCallback(device_callback);
                {
                    std::lock_guard<std::mutex> lock(_task_mutex);
                    _dispatch_task_map[init_paramter_visual.id] = task_now;
                }
                task_now->start();
            }
            catch (const std::exception& e) {
                removeTaskUrl(init_paramter_visual.pull_url, init_paramter_visual.push_url);
                eap_error(e.what());
                sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice
                (init_paramter_visual.id, ApiErr::Exception, std::string(e.what()), std::to_string(0), std::to_string(0)));
                // return; 默认任务需要抛出异常
                throw std::system_error(-1, std::system_category(), e.what());
            }

            std::string description = "enable airborne, have activate visual default tasks";
            eap_information(description);
            sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice
            (init_paramter_visual.id, ApiErr::Success, std::string("success"), std::to_string(duration), std::to_string(0)));
        }

        void DispatchCenter::addDefaultInfraredTasks(std::string& infrared_guid)
        {
            std::string infrared_input_url{};
            std::string infrared_multicast_url{};
            std::string infrared_sms_url{};
            std::string record_hd_file_prefix{};
            std::string record_sd_file_prefix{};
            int func_mask{ 0 };
            try {
                GET_CONFIG(std::string, getString, my_infrared_input_url, Media::kInfraredInputUrl);
                GET_CONFIG(std::string, getString, my_infrared_multicast_url, Media::kInfraredMulticastUrl);
                GET_CONFIG(std::string, getString, my_infrared_sms_url, Media::kInfraredSmsUrl);
                GET_CONFIG(std::string, getString, my_record_hd_file_prefix, Media::kRecordHDFilePrefix);
                GET_CONFIG(std::string, getString, my_record_sd_file_prefix, Media::kRecordSDFilePrefix);
                if (eap::configInstance().has(Media::kInfraredFuncmask)) {
                    GET_CONFIG(int, getInt, my_infrared_funcmask, Media::kInfraredFuncmask);
                    func_mask = my_infrared_funcmask;
                }
                else {
                    eap::configInstance().setInt(Media::kInfraredFuncmask, func_mask);
                    eap::saveConfig();
                }

                infrared_input_url = my_infrared_input_url;
                infrared_multicast_url = my_infrared_multicast_url;
                infrared_sms_url = my_infrared_sms_url;
                record_hd_file_prefix = my_record_hd_file_prefix;
                record_sd_file_prefix = my_record_sd_file_prefix;

            }
            catch (const std::exception& e) {
                eap_error_printf("get config kSecret throw exception: %s", e.what());
            }

            std::string record_time_str = get_current_time_string_second_compact();

            //infrared
            DispatchTask::InitParameter init_paramter_infrared;
            init_paramter_infrared.id = infrared_guid;

            if (!infrared_input_url.empty()) {
                init_paramter_infrared.pull_url = infrared_input_url;
            }
            else {
                throw std::invalid_argument("kInfraredInputUrl is null");
            }

            if (!infrared_multicast_url.empty()) {
                init_paramter_infrared.push_url = infrared_multicast_url;
                init_paramter_infrared._push_urls.push_back(infrared_multicast_url);
            }
            else {
                throw std::invalid_argument("kInfraredMulticastUrl is null");
            }

            if (!infrared_sms_url.empty()) {
                //支持新建任务推两个地址
                init_paramter_infrared._push_urls.push_back(infrared_sms_url);
            }

            init_paramter_infrared.func_mask = func_mask;//默认不开启录像
            init_paramter_infrared.record_file_prefix = record_sd_file_prefix;
            init_paramter_infrared.ar_vector_file = "";
            init_paramter_infrared.ar_camera_config = "";
            init_paramter_infrared.record_time_str = record_time_str;
            init_paramter_infrared.is_pilotcontrol_task = true;
            init_paramter_infrared.need_decode = true;
            DispatchTaskPtr task_ptr{};
            auto task = findTaskUrl(init_paramter_infrared.pull_url, init_paramter_infrared.push_url, task_ptr);
            if (task_ptr) {
                auto task_id = task_ptr->getId();
                auto ret = removeTask(task_id);
                eap_information_printf("Task already existed, removeTask id: %s, pull url: %s, push_url: %s, ret: %d", task_id, init_paramter_infrared.pull_url, init_paramter_infrared.push_url, (int)ret);
            }

            int64_t duration{};
            try {
                eap_information_printf("infrared default task start, pull url is: %s, push url is: %s", init_paramter_infrared.pull_url, init_paramter_infrared.push_url);
                auto task_now = DispatchTaskImpl::createInstance(init_paramter_infrared);
                task_now->setId(init_paramter_infrared.id);
                {
                    std::lock_guard<std::mutex> lock(_task_mutex);
                    _dispatch_task_map[init_paramter_infrared.id] = task_now;
                }
                task_now->start();
            }
            catch (const std::exception& e) {
                removeTaskUrl(init_paramter_infrared.pull_url, init_paramter_infrared.push_url);
                eap_error(e.what());

                sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice
                (init_paramter_infrared.id, ApiErr::Exception, std::string(e.what()), std::to_string(0), std::to_string(0)));
                // return; 默认任务需要抛出异常
                throw std::system_error(-1, std::system_category(), e.what());
            }

            std::string description = "enable airborne, have activate infrared default tasks";
            eap_information(description);
            sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice
            (init_paramter_infrared.id, ApiErr::Success, std::string("success"), std::to_string(duration), std::to_string(0)));
        }

        void DispatchCenter::addTask(DispatchTask::InitParameter init_paramter)
        {
            auto task = findTaskUrl(init_paramter.pull_url, init_paramter.push_url);
            if (task) {
                deleteTaskFile(init_paramter.id);
                std::string exception_description = std::string("Task already existed, ")
                    + std::string("pull url: ") + init_paramter.pull_url
                    + std::string(", push url: ") + init_paramter.push_url
                    + std::string(", id: ") + init_paramter.id;
                eap_information(exception_description);
                throw std::invalid_argument(exception_description);
            }
            eap::ThreadPool::defaultPool().start([this, init_paramter]() {
                int retry_cnt{ 3 };
                int64_t duration{};
                while (retry_cnt--) {
                    try {
                        auto task_now = DispatchTaskImpl::createInstance(init_paramter);
                        task_now->setId(init_paramter.id);
                        if (init_paramter.is_pilotcontrol_task) { //配置文件默认任务放入列表李
                            _dispatch_task_map_default[init_paramter.id] = task_now;
                            eap_information_printf("----insert default task map id: %s", init_paramter.id);
                        }
                        task_now->start();

                        std::lock_guard<std::mutex> lock(_task_mutex);
                        if (!init_paramter.is_pilotcontrol_task) { //配置文件默认任务不放入列表李
                            _dispatch_task_map[init_paramter.id] = task_now;
                            eap_information_printf("----insert task map id: %s", init_paramter.id);
                            saveTaskInfo(init_paramter);
                        }
                        else {
                            _dispatch_task_map_default[init_paramter.id] = task_now;
                            eap_information_printf("----insert default task map id: %s", init_paramter.id);
                        }
                        auto duration_vec = task_now->getVideoDuration();
                        if (!duration_vec.empty())
                            duration = duration_vec.front();
                        break;
                    } catch (const std::exception& e) {
                        std::string err_msg = e.what();
                        eap_error(err_msg);
                        auto err_code = ApiErr::Exception;
                        if (err_msg.find("Failed to create specified HW device") != std::string::npos) {
                            err_code = ApiErr::HWFailed;

                        } else {
                            retry_cnt = 0;
                        }
                        if (retry_cnt == 0) {
                            removeTaskUrl(init_paramter.pull_url, init_paramter.push_url);
                            sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice
                            (init_paramter.id, err_code, err_msg, std::to_string(0), std::to_string(0)));
                            // return; 默认任务需要抛出异常
                            throw std::system_error(-1, std::system_category(), e.what());
                        }
                    }
                }

                sma::NoticeCenter::Instance()->getCenter().postNotification(
                    new AddTaskResultNotice(init_paramter.id, ApiErr::Success, std::string("success"), std::to_string(duration), std::to_string(0)));
            });
        }

        void DispatchCenter::addTaskMultiple(DispatchTask::InitParameter init_paramter)
        {
            auto task = findTaskMultiple(init_paramter._pull_urls, init_paramter._push_urls);
            if (task) {
                deleteTaskFile(init_paramter.id);
                throw std::invalid_argument("Multiple Task already existed");
            }

            eap::ThreadPool::defaultPool().start([this, init_paramter]()
            {
                int64_t duration{}, duration_sd{};
                try {
                    auto task_now = DispatchTaskImplMultiple::createInstance(init_paramter);
                    task_now->setId(init_paramter.id);
                    task_now->setExceptionCallback([this](std::string id, std::string err) {
                        eap_error(err);
                        ApiErr err_code = ApiErr::Exception;
                        if (err.find("Failed to create specified HW device") != std::string::npos) {
                            err_code = ApiErr::HWFailed;
                        }
                        sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice(id, err_code, err, std::to_string(0), std::to_string(0)));
                    });
                    task_now->start();

                    std::lock_guard<std::mutex> lock(_task_mutex_multiple);
                    _dispatch_task_map_multiple[init_paramter.id] = task_now;
                    auto video_duration = task_now->getVideoDuration();
                    if (video_duration.size() >= 2) {
                        duration = video_duration[0];
                        duration_sd = video_duration[1];
                    }
                    saveTaskInfo(init_paramter);
                }
                catch (const std::exception& e) {
                    std::string pull_url{}, push_url{};
                    for (auto url : init_paramter._pull_urls)
                        pull_url += url + "##";
                    for (auto url : init_paramter._push_urls)
                        push_url += url + "##";
                    removeTaskUrl(pull_url, push_url);
                    std::string err_msg = e.what();
                    eap_error(err_msg);
                    ApiErr err_code = ApiErr::Exception;
                    if (err_msg.find("Failed to create specified HW device") != std::string::npos) {
                        err_code = ApiErr::HWFailed;
                    }
                    sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice
                    (init_paramter.id, err_code, err_msg, std::to_string(0), std::to_string(0)));
                    // return; 默认任务需要抛出异常
                    throw std::system_error(-1, std::system_category(), e.what());
                }

                sma::NoticeCenter::Instance()->getCenter().postNotification(new AddTaskResultNotice
                (init_paramter.id, ApiErr::Success, std::string("success"), std::to_string(duration), std::to_string(duration_sd)));
            });
        }

        void DispatchCenter::addTaskRecord(PlaybackAnnotation::InitParam init_paramter)
        {
            eap::ThreadPool::defaultPool().start([this, init_paramter]() {
                try {
                    auto init_param = PlaybackAnnotation::makeInitParam();
                    init_param->task_id = init_paramter.task_id;
                    init_param->playback_address = init_paramter.playback_address;
                    init_param->metadata_file_directory = init_paramter.metadata_file_directory;
                    init_param->video_out_url = init_paramter.video_out_url;
                    auto task_record = PlaybackAnnotation::createInstance(init_param);
                    task_record->start();

                    std::lock_guard<std::mutex> lock(_record_task_mutex);
                    _record_task_map[init_paramter.task_id] = task_record;
                }
                catch (const std::exception& e) {
                    eap_error(e.what());

                    sma::NoticeCenter::Instance()->getCenter().postNotification(new PlayBackMarkRecordNotice(init_paramter.task_id, false, std::string(e.what())));
                    // return; 默认任务需要抛出异常
                    throw std::system_error(-1, std::system_category(), e.what());
                    return;
                }
                sma::NoticeCenter::Instance()->getCenter().postNotification(new PlayBackMarkRecordNotice(init_paramter.task_id, true, std::string("success")));

            });
        }

        bool DispatchCenter::removeTask(std::string id)
        {
            if (id.empty()) {
                return false;
            }
            eap_information_printf("removeTask id: %s", id);
            {
                std::lock_guard<std::mutex> lock(_task_mutex);
                auto it = _dispatch_task_map.find(id);
                if (it != _dispatch_task_map.end()) {
                    std::string pull_url{}, push_url{};
                    pull_url = it->second->getPullUrl();
                    push_url = it->second->getPushUrl();
                    removeTaskUrl(pull_url, push_url);
                    deleteTaskFile(id);

                    it->second->stop();
                    _dispatch_task_map.erase(it);
                    eap_information_printf("---remove task successed! id: %s", id);
                    return true;
                }
                auto it_defalut = _dispatch_task_map_default.find(id);
                if (it_defalut != _dispatch_task_map_default.end()) {
                    std::string pull_url{}, push_url{};
                    pull_url = it_defalut->second->getPullUrl();
                    push_url = it_defalut->second->getPushUrl();
                    removeTaskUrl(pull_url, push_url);
                    deleteTaskFile(id);

                    it_defalut->second->stop();
                    _dispatch_task_map_default.erase(it_defalut);
                    eap_information_printf("---remove defalut task successed! id: %s", id);
                    return true;
                }
            }
            {
                std::lock_guard<std::mutex> lock_mulit(_task_mutex_multiple);
                auto it_multi = _dispatch_task_map_multiple.find(id);
                if (it_multi != _dispatch_task_map_multiple.end()) {
                    std::string pull_url{}, push_url{};
                    for (auto url : it_multi->second->getPullUrls())
                        pull_url += url + "##";
                    for (auto url : it_multi->second->getPushUrls())
                        push_url += url + "##";
                    removeTaskUrl(pull_url, push_url);

                    it_multi->second->stop();
                    _dispatch_task_map_multiple.erase(it_multi);
                    deleteTaskFile(id);
                    eap_information_printf("---remove multi task successed! id: %s", id);
                    return true;
                }
            }

            {
                std::lock_guard<std::mutex> lock_playback(_record_task_mutex);
                auto it_playback = _record_task_map.find(id);
                if (it_playback != _record_task_map.end()) {
                    _record_task_map.erase(it_playback);
                    deleteTaskFile(id);
                    return true;
                }
            }

            return false;
        }

        void DispatchCenter::updateFuncMask(std::string id, int function_mask, std::string ar_camera_config, std::string ar_vector_file, int time_count)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }
#ifdef ENABLE_AIRBORNE
            //机载视频原来开启了ai或者录像，msn微服务修改的时候也开启原来的功能
            int old_function_mask = task->getFunctionMask();
            if ((old_function_mask & FUNCTION_MASK_AI) == FUNCTION_MASK_AI)
                function_mask += FUNCTION_MASK_AI;
            if ((old_function_mask & FUNCTION_MASK_VIDEO_RECORD) == FUNCTION_MASK_VIDEO_RECORD)
                function_mask += FUNCTION_MASK_VIDEO_RECORD;
            eap_information_printf("task id: %s, func_mask: %d, time_count: %d", id, function_mask, time_count);
            task->clipSnapShotParam(time_count);
#endif
            if (!ar_camera_config.empty() || !ar_vector_file.empty()) {
                task->updateFuncMask(function_mask, ar_camera_config, ar_vector_file);
            }
            else {
                task->updateFuncMask(function_mask);
            }
            updateTaskInfo(id, function_mask, ar_camera_config, ar_vector_file);
        }
        
        void DispatchCenter::clipSnapShotParam(std::string id, int time_count)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }
            
            try {
                task->clipSnapShotParam(time_count);
            }
            catch (const std::exception& e) {
                eap_error_printf("clipSnapShotParam throw exception: %s", std::string(e.what()));
            }
            catch (...) {
                eap_error_printf("clipSnapShotParam throw exception: %s", std::string("unknown error"));
            }
        }

        void DispatchCenter::updateTaskInfo(std::string id, int function_mask, std::string ar_camera_config, std::string ar_vector_file)
        {
            if (id.empty()) {
                eap_information("id is empty!");
                return;
            }
            std::string filepath = exeDir() + "tasks";
            Poco::File dir(filepath);
            if (!dir.exists()) {
                return;
            }
            filepath += "/";
            std::list<std::string> task_paths{};
            listFilesRecursively(filepath, task_paths, "", ".txt");
            for (auto path : task_paths) {
                if (path.find(id) != std::string::npos) {
                    DispatchTask::InitParameter init_paramter;
                    try {
                        // 打开文件进行读取
                        std::ifstream file(path);
                        if (!file.is_open()) {
                            eap_error_printf("Failed to open file for reading: %s", path);
                            continue;
                        }

                        // 读取文件内容到字符串
                        std::stringstream buffer;
                        buffer << file.rdbuf();
                        std::string jsonString = buffer.str();
                        file.close();

                        Poco::JSON::Parser parser;
                        auto jsonData = parser.parse(jsonString);
                        auto data_json = *(jsonData.extract<Poco::JSON::Object::Ptr>());
                        data_json.set("ar_vector_file", ar_vector_file);
                        data_json.set("ar_camera_config", ar_camera_config);
                        data_json.set("func_mask", function_mask);
                        // 打开文件进行写入
                        std::ofstream task_file(path);
                        if (!task_file.is_open()) {
                            eap_error_printf("Failed to open file for writing: %s", path);
                            return;
                        }
                        task_file << jsonToString(data_json);
                        task_file.close();

                    }
                    catch (const std::exception& e) {
                        eap_error_printf("---update task failed: %s", std::string(e.what()));
                        break;
                    }
                    catch (...) {
                    }
                    break;
                }
            }
        }

        void DispatchCenter::receivePilotData(std::string id, std::string param_str)
        {
#ifdef ENABLE_AIRBORNE
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }

            //eap::ThreadPool::defaultPool().start([this, task, param_str]()
           // {
            if (task)
                task->receivePilotData(param_str);
            //});
#else
            std::string description = "not airborne, receive not supported pilot data";
            throw std::invalid_argument(description);
            eap_warning(description);
#endif // ENABLE_AIRBORNE
        }

        void DispatchCenter::receivePayloadData(std::string id, std::string param_str)
        {
#ifdef ENABLE_AIRBORNE
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }

            if (eap::ThreadPool::defaultPool().available() > 0) {
                eap::ThreadPool::defaultPool().start([this, task, param_str]() {
                    if (task)
                        task->receivePayloadData(param_str);
                });
            }
            else {
                if (task)
                    task->receivePayloadData(param_str);
            }
#else
            std::string description = "not airborne, receive not supported payload data";
            throw std::invalid_argument(description);
            eap_warning(description);
#endif // ENABLE_AIRBORNE
        }

        void DispatchCenter::addAnnotationElements(std::string id, std::string ar_camera_path, std::string annotation_elements_json, bool isHd)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("Task not existed");
            }

            task->addAnnotationElements(ar_camera_path, annotation_elements_json, isHd);
        }

        void DispatchCenter::deleteAnnotationElements(std::string id, std::string mark_guid, bool isHd)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("Task not existed");
            }

            task->deleteAnnotationElements(mark_guid, isHd);
        }

        DispatchTaskPtr DispatchCenter::findTaskPullUrl(const std::string pull_url)
        {
            if (pull_url.empty()) {
                return nullptr;
            }

            std::lock_guard<std::mutex> lock(_task_mutex);

            DispatchTaskPtr ret;
            for (auto& task : _dispatch_task_map) {
                if (task.second && task.second->getPullUrl() == pull_url) {
                    ret = task.second;
                }
            }

            return ret;
        }

        bool DispatchCenter::findTaskUrl(const std::string pull_url, const std::string push_url, DispatchTaskPtr task_ptr)
        {
            bool ret{};
            if (pull_url.empty() || push_url.empty()) {
                return ret;
            }

            std::lock_guard<std::mutex> lock(_task_mutex);
            for (auto iter : _dispatch_task_map) {
                if (iter.second && iter.second->getPushUrl() == push_url) {
                    ret = true;
                    task_ptr = iter.second;
                    break;
                }
            }
            for (auto iter : _dispatch_task_map_default) {
                if (iter.second && iter.second->getPushUrl() == push_url) {
                    ret = true;
                    task_ptr = iter.second;
                    break;
                }
            }
            auto info = std::make_pair(pull_url, push_url);
            if(_tasks_url_set.find(info) != _tasks_url_set.end()){
                    ret = true;
            }else{
                _tasks_url_set.insert(info);
            }
            return ret;            
        }

        bool DispatchCenter::findTaskMultiple(const std::vector<std::string> pull_urls, const std::vector<std::string> push_urls)
        {
            bool ret{};
            if(!allStringNonEmpty(pull_urls) || !allStringNonEmpty(push_urls)){
                return ret;
            }

            std::lock_guard<std::mutex> lock(_task_mutex_multiple);
            for (auto& task : _dispatch_task_map_multiple) {
                auto task_pull_urls = task.second->getPullUrls();
                auto task_push_urls = task.second->getPushUrls();

                std::set<std::string> task_pull_urls_set(task_pull_urls.begin(), task_pull_urls.end());
                std::set<std::string> task_push_urls_set(task_push_urls.begin(), task_push_urls.end());
                std::set<std::string> pull_urls_set(pull_urls.begin(), pull_urls.end());
                std::set<std::string> push_urls_set(push_urls.begin(), push_urls.end());

                if(task_pull_urls_set == pull_urls_set && task_push_urls_set == push_urls_set){
                    ret = true;
                    break;
                }
            }
            std::string pull_url{}, push_url{};
            for (auto url : pull_urls)
                pull_url += url + "##";
            for (auto url : push_urls)
                push_url += url + "##";
            auto info = std::make_pair(pull_url, push_url);
            if (_tasks_url_set.find(info) != _tasks_url_set.end()) {
                ret = true;
            } else {
                _tasks_url_set.insert(info);
            }
            return ret;
        }

        DispatchTaskPtr DispatchCenter::findTaskId(const std::string id)
        {
            if (id.empty()) {
                return nullptr;
            }

            std::lock_guard<std::mutex> lock(_task_mutex);

            auto it = _dispatch_task_map.find(id);
            if (it != _dispatch_task_map.end()) {
                return it->second;
            }
            auto multi_it = _dispatch_task_map_multiple.find(id);
            if (multi_it != _dispatch_task_map_multiple.end()) {
                return multi_it->second;
            }
            return nullptr;
        }

        void DispatchCenter::for_each_task(const std::function<void(const DispatchTaskPtr& task)>& cb)
        {
            std::deque<DispatchTaskPtr> task_list;
            {
                std::lock_guard<std::mutex> lock(_task_mutex);
                for (auto& task : _dispatch_task_map) {
                    task_list.push_back(task.second);
                }
                for (auto& task : _dispatch_task_map_multiple) {
                    task_list.push_back(task.second);
                }
            }
            for (auto &task : task_list) {
                cb(task);
            }
        }

        void DispatchCenter::setRpcServer(rest_rpc::rpc_service::rpc_server* server)
        {
            _rpc_server = server;
        }

        void DispatchCenter::saveTaskInfo(DispatchTask::InitParameter init_paramter)
        {
            std::string filepath = exeDir() + "tasks";
            Poco::File dir(filepath);
            if (!dir.exists()) {
                return;
            }
            deleteTaskFile(init_paramter.id);
            Poco::JSON::Object task_json;
            Poco::JSON::Array pull_url_array, push_url_array;
            task_json.set("id", init_paramter.id);
            if (init_paramter.pull_url.empty()) { //多路视频
                for(auto pull_url:init_paramter._pull_urls)
                    pull_url_array.add(pull_url);
            }
            else {
                pull_url_array.add(init_paramter.pull_url);
            }
            if (init_paramter.push_url.empty()) {
                for (auto push_url : init_paramter._push_urls)
                    push_url_array.add(push_url);
            }
            else {
                push_url_array.add(init_paramter.push_url);
            }
            task_json.set("pull_url_array", pull_url_array);
            task_json.set("push_url_array", push_url_array);
            task_json.set("func_mask", init_paramter.func_mask);
            task_json.set("ar_vector_file", init_paramter.ar_vector_file);
            task_json.set("ar_camera_config", init_paramter.ar_camera_config);
            task_json.set("record_file_prefix", init_paramter.record_file_prefix);
            task_json.set("record_time_str", init_paramter.record_time_str);
            task_json.set("is_pilotcontrol_task", init_paramter.is_pilotcontrol_task);
            
            std::lock_guard<std::mutex> lock(_task_file_mutex);
            std::string task_str = jsonToString(task_json);
            std::string filename = exeDir() + "tasks/"+ init_paramter.id + ".txt";
            // 打开文件进行写入
            std::ofstream file(filename);
            if (!file.is_open()) {
                eap_error_printf( "Failed to open file for writing: %s", filename);
                return;
            }

            // 将 JSON 字符串写入文件
            file << task_str;
            file.close();
        }

        void DispatchCenter::deleteTaskFile(std::string id)
        {
            std::string filepath = exeDir() + "tasks";
            Poco::File dir(filepath);
            if (!dir.exists()) {
                return;
            }
            std::lock_guard<std::mutex> lock(_task_file_mutex);
            std::string filename = exeDir() + "tasks/" + id + ".txt";
            if (fileExist(filename)) {
                deleteFile(filename);
            }
        }

        void DispatchCenter::removeTaskUrl(const std::string pull_url, const std::string push_url)
        {
            auto info = std::make_pair(pull_url, push_url);
            if (_tasks_url_set.find(info) != _tasks_url_set.end()) {
                _tasks_url_set.erase(info);
            }
        }

        int DispatchCenter::getTaskCount()
        {
            int cnt = 0;
            std::lock_guard<std::mutex> lock(_task_mutex);
            cnt = _dispatch_task_map.size() + _dispatch_task_map_multiple.size() + _dispatch_task_map_default.size();
            return cnt;
        }

        void DispatchCenter::snapshot(std::string id, std::string recordNo, int interval, int total_time)
        {
            auto task = findTaskId(id);
            if (!task)
            {
                throw std::invalid_argument("Task not existed");
            }
            task->snapshot(recordNo, interval, total_time);
        }

        void DispatchCenter::videoClipRecord(std::string id, int record_duration, std::string recordNo)
        {
            auto task = findTaskId(id);
            if (!task)
            {
                throw std::invalid_argument("Task not existed");
            }
            task->videoClipRecord(record_duration, recordNo);
        }

        void DispatchCenter::updatePullUrl(const std::string id, const std::string pull_url, std::string push_url)
        {
            DispatchTaskPtr task{};
            DispatchTask::InitParameter init_paramter;
            std::string task_id = id;
            if (pull_url.empty() || id.empty()) {
                //传统链路，地面sma中继选控切换视频，更换推流地址
                GET_CONFIG(std::string, getString, my_hd_push_url, Vehicle::KHdPushUrl);
                GET_CONFIG(std::string, getString, my_hd_url_src, Vehicle::KHdUrlSrc);
                for (auto iter : _dispatch_task_map_default) {
                    if (iter.second && iter.second->getPullUrl() == my_hd_url_src
                        && iter.second->getPushUrl() == my_hd_push_url) {
                        task = iter.second;
                        break;
                    }
                }
//#ifdef ENABLE_AIRBORNE
//                //4、5G情况下，机载sma中继选控切换视频，更换推流地址
//                GET_CONFIG(std::string, getString, my_visual_input_url, Media::kVisualInputUrl);
//                GET_CONFIG(std::string, getString, my_visual_sms_url, Media::kVisualSmsUrl);
//                my_hd_url_src = my_visual_input_url;
//                my_hd_push_url = my_visual_sms_url;
//                for (auto iter : _dispatch_task_map) {
//                    if (iter.second && iter.second->getPullUrl() == my_visual_input_url
//                        && iter.second->getPushUrl() == my_visual_sms_url) {
//                        task = iter.second;
//                        break;
//                    }
//                }
//#endif
                if (task) {
                    init_paramter.pull_url = my_hd_url_src;
                    init_paramter.push_url = push_url;
                    init_paramter.id = task->getId();
                    //推流地址不一样时记录原来的任务id
                    if (my_hd_push_url != push_url)
                        task_id = init_paramter.id;
                    eap_information_printf("default task id: %s, old push url: %s, new push url: %s, pull url: %s", init_paramter.id, my_hd_push_url, push_url, init_paramter.pull_url);
//#ifdef ENABLE_AIRBORNE
//                    eap::configInstance().setString(Media::kVisualSmsUrl, push_url);
//#else
                    eap::configInstance().setString(Vehicle::KHdPushUrl, push_url);
//#endif
                    eap::saveConfig();
                }
            }
            else {
                task = findTaskId(id);
                init_paramter.pull_url = pull_url;
                init_paramter.id = id;
                if (task)
                    init_paramter.push_url = task->getPushUrl();
            }
            
            init_paramter.is_pilotcontrol_task = true;
            if (task) {
                init_paramter.record_file_prefix = task->getRecordFilePrefix();
                init_paramter.record_time_str = task->getRecordTimeStr();
                auto ret = removeTask(task_id);
                eap_information_printf("removeTask id: %s, push_url: %s, ret: %d", task_id, init_paramter.push_url, (int)ret);
            }
            if (!init_paramter.pull_url.empty() && !init_paramter.push_url.empty()) {
                addTask(init_paramter);
                eap_information_printf("updatePullUrl id: %s, pull url: %s, push url: %s", task_id, pull_url, push_url);
            }
        }

        std::string DispatchCenter::fireSearchInfo(const std::string id, const double target_lat, const double target_lon, const double target_alt)
        {
            {//森防项目，第一次火点定位出结果的时候通知云端sma，快照图片给云平台
                std::lock_guard<std::mutex> lock(_task_mutex);
                for (auto& task : _dispatch_task_map) {
                    if (task.second) {
                        auto push_url = task.second->getPushUrl();
                        if (!push_url.empty()) {
                            std::size_t index = push_url.rfind("/");
                            std::size_t second_index = push_url.rfind("/", index - 1);
                            auto pilot_id = push_url.substr(second_index + 1, index - second_index - 1);
                            if (pilot_id == id) {
                                task.second->fireSearchInfo(id, target_lat, target_lon, target_alt);
                                return task.second->getId();
                            }
                        }
                    }
                }
            }
            return "";
        }

        void DispatchCenter::setAirborne45G(std::string id, const int airborne_45G)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }
            task->setAirborne45G(airborne_45G);
        }

        bool DispatchCenter::startDefalutTask() 
        {
            std::string hanger_in1_url_pusher{};
            std::string hanger_in1_url_src{};
            std::string hanger_in2_url_pusher{};
            std::string hanger_in2_url_src{};
            std::string hanger_out_url_pusher{};
            std::string hanger_out_url_src{};
            try {
                GET_CONFIG(std::string, getString, my_hanger_in1_url_pusher, Vehicle::KHangerIn1UrlPusher);
                GET_CONFIG(std::string, getString, my_hanger_in1_url_src, Vehicle::KHangerIn1UrlSrc);
                GET_CONFIG(std::string, getString, my_hanger_in2_url_pusher, Vehicle::KHangerIn2UrlPusher);
                GET_CONFIG(std::string, getString, my_hanger_in2_url_src, Vehicle::KHangerIn2UrlSrc);
                GET_CONFIG(std::string, getString, my_hanger_out_url_pusher, Vehicle::KHangerOutUrlPusher);
                GET_CONFIG(std::string, getString, my_hanger_out_url_src, Vehicle::KHangerOutUrlSrc);
                hanger_in1_url_pusher = my_hanger_in1_url_pusher;
                hanger_in1_url_src = my_hanger_in1_url_src;
                hanger_in2_url_pusher = my_hanger_in2_url_pusher;
                hanger_in2_url_src = my_hanger_in2_url_src;
                hanger_out_url_pusher = my_hanger_out_url_pusher;
                hanger_out_url_src = my_hanger_out_url_src;
            }
            catch (const std::exception& e) {
                eap_error_printf("get config throw exception: %s", e.what());
                return false;
            }

            std::queue<std::pair<std::string, std::string>> defalut_task_queue;
            defalut_task_queue.push(std::make_pair(hanger_out_url_src, hanger_out_url_pusher));
            defalut_task_queue.push(std::make_pair(hanger_in2_url_src, hanger_in2_url_pusher));
            defalut_task_queue.push(std::make_pair(hanger_in1_url_src, hanger_in1_url_pusher));
            while (defalut_task_queue.size()) {
                auto task_info = defalut_task_queue.front();
                defalut_task_queue.pop();
                auto pull_url = task_info.first;
                auto push_url = task_info.second;
                if (!pull_url.empty() && !push_url.empty()) {
                    DispatchTask::InitParameter init_paramter;
                    init_paramter.id = generate_guid(16);
                    try {
                        init_paramter.pull_url = pull_url;
                        init_paramter.push_url = push_url;
                        init_paramter.is_pilotcontrol_task = true;

                        ThreadPool::defaultPool().start([init_paramter]() {
                            DispatchCenter::Instance()->addTask(init_paramter);
                            eap_information_printf("---add config task id: %s, pull url:%s, push url:%s", init_paramter.id, init_paramter.pull_url, init_paramter.push_url);
                            });
                    }
                    catch (const std::exception& e) {
                        eap_error_printf("initTasks failed: %s ", std::string(e.what()));
                        return false;

                    }
                    catch (...) {
                        return false;
                    }
                }
            }
            return true;
        }

        bool DispatchCenter::stopDefalutTask() {
            try {
                std::lock_guard<std::mutex> lock(_task_mutex);
                for (auto it = _dispatch_task_map_default.begin(); it != _dispatch_task_map_default.end();) {
                // 过滤掉hd sd
                if (it->second == nullptr) {
                    ++it;
                    continue;
                }

                std::string pull_url = it->second->getPullUrl();
                std::string push_url = it->second->getPushUrl();

                // 修正：正确检查是否包含子串
                if (pull_url.find("in") != std::string::npos || 
                    pull_url.find("out") != std::string::npos) {
                    
                    eap_information_printf("----_dispatch_task_map_default: %d", 
                                        (int)_dispatch_task_map_default.size());
                    eap_information_printf("----DispatchCenter stop default, task id: %s", 
                                        it->first);
                    eap_information_printf("pull_url:%s, push_url%s",pull_url, push_url);
                    removeTaskUrl(pull_url, push_url);
                    deleteTaskFile(it->first);
                
                    it->second->stop();//停止监控任务
                    eap_information_printf("----DispatchCenter stop default success, task id: %s", 
                                        it->first);
                    
                    it = _dispatch_task_map_default.erase(it);
                    
                    eap_information_printf("----_dispatch_task_map_default: %d", 
                                        (int)_dispatch_task_map_default.size());
                } else {
                    ++it;
                }
}
                return true;
            }
            catch (const std::exception& e) {
                eap_information_printf("----DispatchCenter stop default failed, msg: %s", std::string(e.what()));
                return false;
            }
        }


        bool DispatchCenter::allStringNonEmpty(const std::vector<std::string> strs)
        {
            if(strs.empty()){
            return false;
            }

            for (const auto &str : strs) {
                if (str.empty()) {
                    return false;
                }
            }

            return true;
        }

        void DispatchCenter::clearTasksFile()
        {
#ifdef _WIN32
            try {
                std::string filepath = exeDir() + "tasks/";
                Poco::File dir(filepath);
                if (!dir.exists()) {
                    return;
                }
                if (!dir.isDirectory()) {
                    eap_information_printf("Path is not a directory: %s", filepath);
                    return;
                }

                // 遍历目录内容并删除
                Poco::DirectoryIterator it(dir);
                Poco::DirectoryIterator end;
                while (it != end) {
                    try {
                        it->remove(true); // true 表示递归删除子目录
                        ++it;
                    }
                    catch (const std::exception& e) {
                        eap_information_printf("Error deleting %s , error: %s", it->path(), std::string(e.what()));
                        ++it;
                    }
                }

                eap_information_printf("Directory emptied successfully: %s", filepath);
            }
            catch (const std::exception& e) {
                eap_information_printf("Error: %s", std::string(e.what()));
            }
#endif
        }

        void DispatchCenter::enableOnnxToEngine(bool is_fp16, std::string inputNamed, std::string shape)
        {
#ifdef ENABLE_AI
    #ifdef ENABLE_ONNXTOENGINE
            eap::ThreadPool::defaultPool().start([this, is_fp16, inputNamed, shape]()
            {
                std::string onnx_file_full_name{};
                std::string engine_file_full_name{};

                try {
                    GET_CONFIG(std::string, getString, my_onnx_file_full_name, AI::kOnnxFileFullName);
                    GET_CONFIG(std::string, getString, my_engine_file_full_name, AI::kEngineFileFullName);
                    onnx_file_full_name = my_onnx_file_full_name;
                    engine_file_full_name = my_engine_file_full_name;
                }
                catch (const std::exception& e) {
                    eap_error_printf("get config kSecret throw exception: %s", e.what());
                }

                auto off_1 = engine_file_full_name.find_last_of("/");
                auto off_2 = onnx_file_full_name.find_last_of("/");
                auto suffix_off = onnx_file_full_name.find_last_of(".");
                std::string engine_out_name = engine_file_full_name.substr(0, off_1 + 1)
                    + onnx_file_full_name.substr(off_2 + 1, suffix_off - off_2) + std::string("engine");

                    #ifdef ENABLE_OPENSET_DETECTION
                    joai::OnnxToEngineStatus onnx_to_engine_resilt = joai::ObjectDetect::ONNXtoEngine(onnx_file_full_name.c_str(), engine_out_name.c_str(), is_fp16, inputNamed.c_str(), shape.c_str(),
                        [this](float percentage_of_progress) {
                            _onnx_to_engine_percent = percentage_of_progress;
                        });
                    #else
                    joai::OnnxToEngineStatus onnx_to_engine_resilt = joai::ObjectDetect::ONNXtoEngine(onnx_file_full_name.c_str(), engine_out_name.c_str(), is_fp16,
                        [this](float percentage_of_progress) {
                            _onnx_to_engine_percent = percentage_of_progress;
                        });
                    #endif // ENABLE_OPENSET_DETECTION

                if (joai::OnnxToEngineStatus::ONNX_TO_ENGINE_SUCCESS != onnx_to_engine_resilt) {
                    std::string fail_desc = "onnx to engine fail, onnx name: " + onnx_file_full_name
                        + std::string(". status code: ") + std::to_string(onnx_to_engine_resilt);
                    sma::NoticeCenter::Instance()->getCenter().postNotification(new OnnxToEngineResultNotice(false, fail_desc));
                    return;
                }

                std::string success_desc = "onnx to engine success, engine name: " + engine_out_name;
                sma::NoticeCenter::Instance()->getCenter().postNotification(new OnnxToEngineResultNotice(true, success_desc));
            });
    #endif // ENABLE_ONNXTOENGINE
#endif // ENABLE_AI
        }

        float DispatchCenter::getOnnxToEnginePercent()
        {
            return _onnx_to_engine_percent;
        }

        std::string DispatchCenter::aiAssistTrack(std::string id, int track_cmd, int track_pixelpos_x, int track_pixelpos_y)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }
            return task->aiAssistTrack(track_cmd, track_pixelpos_x, track_pixelpos_y);
        }

        void DispatchCenter::saveSnapShot(std::string id)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }
            task->saveSnapShot();        
        }

        void DispatchCenter::updateArLevelDistance(std::string id, int level_one_distance, int level_two_distance)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("Task not existed");
            }

            task->updateArLevelDistance(level_one_distance, level_two_distance);
        }
        void DispatchCenter::updateArTowerHeight(std::string id, bool is_tower, double tower_height, bool buffer_sync_height)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("Task not existed");
            }
            task->updateArTowerHeight(is_tower, tower_height, buffer_sync_height);
        }

        void DispatchCenter::updateAiPosCor(std::string id, int ai_pos_cor)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("Task not existed");
            }
            task->updateAiCorPos(ai_pos_cor);
        }

        void DispatchCenter::setSeekPercent(std::string id, float percent)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("Task not existed");
            }
            task->setSeekPercent(percent);
        }

        void DispatchCenter::pause(std::string id, int paused)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("Task not existed");
            }
            task->pause(paused);
        }

        std::vector<std::tuple<std::vector<long>, std::vector<int>>> DispatchCenter::getHeatmapTotal(std::string id)
        {
            auto task = findTaskId(id);
            if (!task) {
                throw std::invalid_argument("task not existed");
            }
            
            std::string ai_heatmap_file_path{};
            try {
                GET_CONFIG(std::string, getString, my_ai_heatmap_file_path, AI::kAiHeatmapFilePath);
                ai_heatmap_file_path = my_ai_heatmap_file_path;
            }
            catch (const std::exception& e) {
                eap_error_printf("get config kSecret throw exception: %s", e.what());
            }
            
            std::string heatmap_file_path = ai_heatmap_file_path + "/heatmap_" + id + ".txt";
            std::vector<std::tuple<std::vector<long>, std::vector<int>>> heatmap_data;
            std::ifstream infile(heatmap_file_path);

            if (!infile.is_open()) {
                eap_error("Failed to open heatmap file for reading");
                return heatmap_data;
            }

            long lat, lon;
            int class_count;
            char comma;
            
            while (infile >> lat >> comma >> lon ) {
                std::vector<long> coordinates = {lat, lon};
                std::vector<int> class_counts;

                // 读取类别数据直到行结束
                while (infile >> comma >> class_count) {
                    class_counts.push_back(class_count);
                    if (infile.peek() == '\n' || infile.eof()) {
                        break;
                    }
                }

                heatmap_data.emplace_back(coordinates, class_counts);
            }

            infile.close();
            return heatmap_data;
        }
    }
}