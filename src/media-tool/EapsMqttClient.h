#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <regex>
#include <iostream>
#include <memory>
#include <thread>

// nlohmann JSON
#include <nlohmann/json.hpp>

// Paho MQTT C 同步库
#include "MQTTClient.h"

#include "Logger.h"

namespace eap {
namespace sma {

// ── 遥测数据结构 ──────────────────────────────────────────────────────
struct TelemetryData
{
	// 基础信息
	uint16_t checksum = 0;
	uint8_t ldsVersion = 0;
	uint64_t timestamp = 0;

	// 平台(飞机)位置和姿态
	double lat = 0.0, lon = 0.0, alt = 0.0;    // 经纬高 (alt 可作为 abs_alt 或 true altitude)
	double yaw = 0.0, pitch = 0.0, roll = 0.0; // 欧拉角 (相当于 platformHeading/Pitch/Roll)

	// 速度信息
	double Vnorth = 0.0, Veast = 0.0, Vdown = 0.0; // 北东地速度分量
	double sensorSpeed = 0.0;                      // 传感器地速
	double flightDirection = 0.0;                  // 飞行方向
	double VGnd = 0.0;                             // 地速
	double Tas = 0.0;                              // 空速

	// 传感器/云台角度及视场
	double framePan = 0.0, frameTilt = 0.0, frameRoll = 0.0;    // 云台角(框架角)
	double gimbalPan = 0.0, gimbalTilt = 0.0, gimbalRoll = 0.0; // 吊舱欧拉角
	double focalDistance = 0.0;                                 // 焦距 (mm)
	double dfov = 0.0;                                          // 对角视场角
	double sensorHorizontalFov = 0.0;                           // 水平视场角
	double sensorVerticalFov = 0.0;                             // 垂直视场角
	double infaredHorizontalFov = 0.0;                          // 红外水平视场角
	double infaredVerticalFov = 0.0;                            // 红外垂直视场角

	// 载荷扩展信息(激光打点/AI)
	bool hasLaser = false;
	double laserDist = 0.0;                                         // 激光测距距离(m)
	double laserCurLat = 0.0, laserCurLon = 0.0, laserCurAlt = 0.0; // 激光打点坐标

	// 观测目标及画面中心信息
	double trg_lat = 0.0, trg_lon = 0.0, trg_alt = 0.0; // 目标经纬高
	double slantRange = 0.0;                            // 斜距
	double targetWidth = 0.0;                           // 目标宽度

	// 大疆字幕流扩展字段
	double dzoom_ratio = 1.0; // 数字变焦比
	double rel_alt = 0.0;     // 相对高度 m
};

class EapsMqttClient {
public:
    using Ptr = std::shared_ptr<EapsMqttClient>;

    static Ptr createInstance();

    EapsMqttClient();
    ~EapsMqttClient();

    void start(const std::string &broker_host,
               int               broker_port,
               const std::string &client_id,
               const std::string &username,
               const std::string &password,
               const std::string &topic,
               int               keep_alive = 60);

    void stop();

    bool getTelemetryData(TelemetryData &out) const;
    bool isConnected() const { return _connected.load(); }

private:
    void reconnectTask(); // 重连线程的工作函数

    std::thread       _reconnect_thread;
    std::atomic<bool> _is_running{false}; // 标志整个客户端是否处于工作状态
    // C API 回调（静态函数）
    static void onConnectionLost(void* context, char* cause);
    static int  onMessageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message);
    static void onDeliveryComplete(void* context, MQTTClient_deliveryToken token);
    
    // 供静态函数调用的实例方法
    void handleConnected();
    void handleConnectionLost(const std::string& cause);
    void handleMessage(const std::string& payload);

    static bool parseMqttPayload(const std::string &payload, TelemetryData &out);

private:
    MQTTClient _client;
    std::string _server_uri;
    std::string _client_id;
    std::string _username;
    std::string _password;
    std::string _topic;
    int         _qos = 0;
    int         _keep_alive = 60;

    std::atomic<bool> _connected{false};
    std::atomic<bool> _has_data{false};

    mutable std::shared_mutex _data_mutex;
    TelemetryData             _telemetry;
};

} // namespace sma
} // namespace eap
