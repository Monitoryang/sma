#include "EapsMqttClient.h"
#include "Logger.h"
#include <cmath>
#include <regex>
#include <sstream>
#include <thread>
#include <chrono>
namespace eap {
namespace sma {

EapsMqttClient::Ptr EapsMqttClient::createInstance() {
    return std::make_shared<EapsMqttClient>();
}

EapsMqttClient::EapsMqttClient() : _client(nullptr) {}

EapsMqttClient::~EapsMqttClient() {
    stop();
}

void EapsMqttClient::start(const std::string &broker_host,
                           int               broker_port,
                           const std::string &client_id,
                           const std::string &username,
                           const std::string &password,
                           const std::string &topic,
                           int               keep_alive)
{
    _topic = topic;
    _client_id = client_id;
    _username = username;
    _password = password;
    _keep_alive = keep_alive;

    std::ostringstream server_uri;
    server_uri << "tcp://" << broker_host << ":" << broker_port;
    _server_uri = server_uri.str();

    int rc = MQTTClient_create(&_client, _server_uri.c_str(), _client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTCLIENT_SUCCESS) {
        eap_error_printf("[MQTT] Failed to create client, return code %d", rc);
        return;
    }

    rc = MQTTClient_setCallbacks(_client, this, onConnectionLost, onMessageArrived, onDeliveryComplete);
    if (rc != MQTTCLIENT_SUCCESS) {
        eap_error_printf("[MQTT] Failed to set callbacks, return code %d", rc);
        return;
    }

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = _keep_alive;
    conn_opts.cleansession = 1;
    if (!_username.empty()) {
        conn_opts.username = _username.c_str();
        conn_opts.password = _password.c_str();
    }

    _is_running.store(true);
	conn_opts.connectTimeout = 1; // 仅阻塞1秒试探
    // 尝试第一次连接（不阻塞死，失败了也没关系，交给守护线程）
    rc = MQTTClient_connect(_client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        eap_error_printf("[MQTT] Initial connect failed, code %d. Will retry in background.", rc);
        _connected.store(false);
    } else {
        handleConnected(); // 包含 subscribe
    }

    // 启动后台重连守护线程
    _reconnect_thread = std::thread(&EapsMqttClient::reconnectTask, this);
}

void EapsMqttClient::reconnectTask() {
    while (_is_running.load()) {
        if (!_connected.load()) {
            eap_information("[MQTT] Reconnecting to broker...");
            
            MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
            conn_opts.keepAliveInterval = _keep_alive;
            conn_opts.cleansession = 1;
            if (!_username.empty()) {
                conn_opts.username = _username.c_str();
                conn_opts.password = _password.c_str();
            }

            int rc = MQTTClient_connect(_client, &conn_opts);
            if (rc == MQTTCLIENT_SUCCESS) {
                eap_information("[MQTT] Reconnect successful.");
                handleConnected(); // 这个函数内部会帮我们重新 subscribe
            } else {
                eap_error_printf("[MQTT] Reconnect failed, code %d. Retrying later...", rc);
            }
        }
        
        // 睡眠 3 秒后再次检查（为防止线程阻塞无法快速退出，可切分多次小睡眠或使用 condition_variable）
        for (int i = 0; i < 30 && _is_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void EapsMqttClient::stop() {
    _is_running.store(false); // 停止守护线程循环

    if (_reconnect_thread.joinable()) {
        _reconnect_thread.join();
    }

    if (_client) {
        if (_connected.load()) {
            MQTTClient_disconnect(_client, 1000);
        }
        MQTTClient_destroy(&_client);
        _client = nullptr;
        _connected.store(false);
    }
}

void EapsMqttClient::onConnectionLost(void* context, char* cause) {
    auto* self = static_cast<EapsMqttClient*>(context);
    self->handleConnectionLost(cause ? cause : "unknown");
}

int EapsMqttClient::onMessageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    auto* self = static_cast<EapsMqttClient*>(context);
    if (message && message->payload) {
        std::string payload_str(static_cast<char*>(message->payload), message->payloadlen);
        self->handleMessage(payload_str);
    }
    
    // C API 要求手动释放 message 和 topicName
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1; // 返回 1 表示消息确认处理完成
}

void EapsMqttClient::onDeliveryComplete(void* context, MQTTClient_deliveryToken token) {
    // 发布完成回调（仅发布时需要处理，这里保持为空即可）
}

// ── 实例响应处理 ──

void EapsMqttClient::handleConnected() {
    eap_information("[MQTT] Connected.");
    _connected.store(true);

    int rc = MQTTClient_subscribe(_client, _topic.c_str(), _qos);
    if (rc != MQTTCLIENT_SUCCESS) {
        eap_error_printf("[MQTT] Failed to start subscribe, return code %d", rc);
    } else {
        eap_information_printf("[MQTT] Subscribed to topic: %s", _topic);
    }
}

void EapsMqttClient::handleConnectionLost(const std::string& cause) {
    eap_warning_printf("[MQTT] Connection lost: %s.", cause);
    _connected.store(false);
}

void EapsMqttClient::handleMessage(const std::string& payload) {
    TelemetryData tmp;
    if (parseMqttPayload(payload, tmp)) {
        std::unique_lock<std::shared_mutex> lock(_data_mutex);
        _telemetry = tmp;
        _has_data.store(true);
    }
}

bool EapsMqttClient::getTelemetryData(TelemetryData &out) const {
    if (!_has_data.load())
        return false;
    std::shared_lock<std::shared_mutex> lock(_data_mutex);
    out = _telemetry;
    return true;
}

static void eulerToRotMat(double yaw_deg, double pitch_deg, double roll_deg, double R[3][3])
{
	constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;
	double cy = std::cos(yaw_deg * DEG2RAD), sy = std::sin(yaw_deg * DEG2RAD);
	double cp = std::cos(pitch_deg * DEG2RAD), sp = std::sin(pitch_deg * DEG2RAD);
	double cr = std::cos(roll_deg * DEG2RAD), sr = std::sin(roll_deg * DEG2RAD);

	// R = Rz(yaw) * Ry(pitch) * Rx(roll)
	R[0][0] = cy * cp;
	R[0][1] = cy * sp * sr - sy * cr;
	R[0][2] = cy * sp * cr + sy * sr;

	R[1][0] = sy * cp;
	R[1][1] = sy * sp * sr + cy * cr;
	R[1][2] = sy * sp * cr - cy * sr;

	R[2][0] = -sp;
	R[2][1] = cp * sr;
	R[2][2] = cp * cr;
}

static void rotMatToEuler(const double R[3][3], double& yaw_deg, double& pitch_deg, double& roll_deg)
{
	constexpr double RAD2DEG = 180.0 / 3.14159265358979323846;

	// pitch = asin(-R[2][0])，范围 [-90°, 90°]
	pitch_deg = std::asin(std::max(-1.0, std::min(1.0, -R[2][0]))) * RAD2DEG;

	// 检查万向节锁（Gimbal Lock）：|pitch| ≈ 90°
	if (std::abs(R[2][0]) > 0.9999)
	{
		// 万向节锁情形：令 roll = 0，yaw 吸收全部旋转
		roll_deg = 0.0;
		yaw_deg = std::atan2(-R[1][2], R[1][1]) * RAD2DEG;
	}
	else
	{
		roll_deg = std::atan2(R[2][1], R[2][2]) * RAD2DEG;
		yaw_deg = std::atan2(R[1][0], R[0][0]) * RAD2DEG;
	}

	// 将 yaw 归一化到 [0°, 360°)
	if (yaw_deg < 0.0)
		yaw_deg += 360.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// 解析逻辑
bool EapsMqttClient::parseMqttPayload(const std::string& payload, TelemetryData& out)
{
	// 使用 nlohmann/json 解析大疆 MQTT OSD 消息
	nlohmann::json j;
	try
	{
		j = nlohmann::json::parse(payload);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		std::cerr << "[MQTT] JSON parse error: " << e.what() << std::endl;
		return false;
	}

	// 提取顶层时间戳
	if (j.contains("timestamp") && j["timestamp"].is_number_integer())
	{
		out.timestamp = j["timestamp"].get<uint64_t>();
	}

	// 必须有 data 字段
	if (!j.contains("data") || !j["data"].is_object())
		return false;

	const auto& data = j["data"];

	// ---- 大疆 OSD 消息：直接字段中含经纬度 ----
	if (data.contains("latitude") && data.contains("longitude"))
	{
		double lat = data["latitude"].get<double>();
		double lon = data["longitude"].get<double>();

		// 过滤无效坐标 (0,0)
		if (std::abs(lat) < 0.0001 && std::abs(lon) < 0.0001)
			return false;

		out.lat = lat;
		out.lon = lon;

		// height: 绝对高度 (m)
		if (data.contains("height") && data["height"].is_number())
		{
			out.alt = data["height"].get<double>();
		}

		// elevation: 相对相对起飞点高度
		if (data.contains("elevation") && data["elevation"].is_number())
		{
			out.rel_alt = data["elevation"].get<double>();
		}

		// attitude_head: 机头朝向 (度)
		if (data.contains("attitude_head") && data["attitude_head"].is_number())
			out.yaw = data["attitude_head"].get<double>();

		// attitude_pitch: 俯仰角 (度)
		if (data.contains("attitude_pitch") && data["attitude_pitch"].is_number())
			out.pitch = data["attitude_pitch"].get<double>();

		// attitude_roll: 横滚角 (度)
		if (data.contains("attitude_roll") && data["attitude_roll"].is_number())
			out.roll = data["attitude_roll"].get<double>();

		// horizontal_speed / vertical_speed (m/s)
		if (data.contains("horizontal_speed") && data["horizontal_speed"].is_number())
			out.VGnd = data["horizontal_speed"].get<double>();

		if (data.contains("vertical_speed") && data["vertical_speed"].is_number())
			out.Vdown = -data["vertical_speed"].get<double>(); // 大疆向上为正，转为 NED 向下为正

		// 负载索引键格式严格为 "数字-数字-数字"，如 "0-0-0", "99-0-0"
		static const std::regex payload_key_re(R"(^\d+-\d+-\d+$)");
		for (auto it = data.begin(); it != data.end(); ++it)
		{
			const std::string& key = it.key();
			// 严格匹配 "数字-数字-数字" 格式，且值必须是 object
			if (!it.value().is_object())
				continue;
			if (!std::regex_match(key, payload_key_re))
				continue;

			const auto& payload_obj = it.value();

			// 云台角度 gimbal_yaw/pitch/roll -> psi/theta/delta (度)
			if (payload_obj.contains("gimbal_yaw") && payload_obj["gimbal_yaw"].is_number())
				out.gimbalPan = payload_obj["gimbal_yaw"].get<double>();

			if (payload_obj.contains("gimbal_pitch") && payload_obj["gimbal_pitch"].is_number())
				out.gimbalTilt = payload_obj["gimbal_pitch"].get<double>();

			if (payload_obj.contains("gimbal_roll") && payload_obj["gimbal_roll"].is_number())
				out.gimbalRoll = payload_obj["gimbal_roll"].get<double>();

			// 测距目标信息
			if (payload_obj.contains("measure_target_latitude") && payload_obj["measure_target_latitude"].is_number())
				out.trg_lat = payload_obj["measure_target_latitude"].get<double>();

			if (payload_obj.contains("measure_target_longitude") && payload_obj["measure_target_longitude"].is_number())
				out.trg_lon = payload_obj["measure_target_longitude"].get<double>();

			if (payload_obj.contains("measure_target_altitude") && payload_obj["measure_target_altitude"].is_number())
				out.trg_alt = payload_obj["measure_target_altitude"].get<double>();

			if (payload_obj.contains("measure_target_distance") && payload_obj["measure_target_distance"].is_number())
				out.slantRange = payload_obj["measure_target_distance"].get<double>();

			// 只处理第一个符合格式的负载，如需多负载可扩展为 vector
			break;
		}

		// 变焦因子 -> dzoom_ratio
		if (data.contains("cameras") && data["cameras"].is_array())
		{
			for (const auto& cam : data["cameras"])
			{
				if (cam.contains("zoom_factor") && cam["zoom_factor"].is_number())
				{
					out.dzoom_ratio = cam["zoom_factor"].get<double>();
					out.focalDistance = out.dzoom_ratio * 24.0; // 基础焦距为 24mm
					if (out.focalDistance < 69)
					{
						out.dfov = 82.0; // 24mm 对应 82° DFOV
					}
					else if (out.focalDistance < 160)
					{
						out.dfov = 35.0; // 70mm 对应 35° DFOV
					}
					else
					{
						out.dfov = 15.0; // 162 - 168mm 对应 15° DFOV
					}
					break; // 只取第一个摄像头的变焦因子
				}
			}
		}

		// -------------------------------------------------------
		// 反算框架相对角（psi, theta, delta）
		// 已知：飞机绝对姿态 (yaw, pitch, roll) 和云台绝对角 (gimbalPan, gimbalTilt, gimbalRoll)
		// 关系：R_total = R_body * R_sensor
		// 反算：R_sensor = R_body^T * R_total
		// -------------------------------------------------------
		{
			double R_body[3][3], R_total[3][3], R_sensor[3][3];

			// 1. 飞机平台旋转矩阵
			eulerToRotMat(out.yaw, out.pitch, out.roll, R_body);

			// 2. 云台绝对姿态旋转矩阵
			eulerToRotMat(out.gimbalPan, out.gimbalTilt, out.gimbalRoll, R_total);

			// 3. 反算传感器相对旋转矩阵：R_sensor = R_body^T * R_total
			for (int i = 0; i < 3; ++i)
				for (int j = 0; j < 3; ++j)
				{
					R_sensor[i][j] = 0.0;
					for (int k = 0; k < 3; ++k)
						R_sensor[i][j] += R_body[k][i] * R_total[k][j]; // R_body 转置
				}

			// 4. 从相对旋转矩阵提取框架欧拉角
			rotMatToEuler(R_sensor, out.framePan, out.frameTilt, out.frameRoll);
		}

		return true;
	}

	return false;
}

} // namespace sma
} // namespace eap
