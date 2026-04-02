#ifndef EAPS_TCP_SERVER
#define EAPS_TCP_SERVER

#include "asio.hpp"
#include <memory>
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <string>
#ifdef ENABLE_AIRBORNE	
using namespace boost;
namespace eap {
    namespace common {

        class JoTcpServer
        {
        public:
            using ConnectionCallback = std::function<void(std::shared_ptr<asio::ip::tcp::socket>)>;
            using DisconnectCallback = std::function<void(std::shared_ptr<asio::ip::tcp::socket>)>;
            using DataCallback = std::function<void(std::shared_ptr<asio::ip::tcp::socket>, const std::string&)>;

            struct InitParameter
            {
                uint32_t port;
                uint64_t max_connections;
                uint32_t timeout;
            };
            using InitParameterPtr = std::shared_ptr<InitParameter>;

        private:
            struct ConnectionInfo {
                std::queue<std::string> send_queue{};
                std::shared_ptr<asio::steady_timer> timeout_timer{};
                std::chrono::steady_clock::time_point last_activity{};
                bool is_sending{};
            };

        public:
            static InitParameterPtr MakeInitParameter();

            ~JoTcpServer();
            JoTcpServer(InitParameterPtr init_param_ptr);

            void setConnectionCallback(ConnectionCallback cb);
            void setDisConnectionCallback(DisconnectCallback cb);
            void setDataCallback(DataCallback cb);
            void broadcast(const std::string data);
            void sendTo(std::shared_ptr<asio::ip::tcp::socket> client, const std::string& data);
            uint64_t connectionCount();
            bool _IsStopped();
            void run();
            void stop();

        private:
            void _SetupSignals();
            void _StartAccept();
            void _AsyncAccept();
            void _StartReceive(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<ConnectionInfo> conn_info);
            void _EnqueueMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& data);
            void _StartSend(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<ConnectionInfo> conn_info);
            void _StartTimeoutCheck(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<ConnectionInfo> conn_info);
            void _HandleDisconnect(std::shared_ptr<asio::ip::tcp::socket> socket);

        private:
            uint32_t _Port{};
            uint64_t _MaxConnections{};
            uint32_t _ConnectionTimeout{};

            asio::io_context _IoContext;
            asio::ip::tcp::acceptor _Acceptor;
            asio::signal_set _Signals;

            std::mutex _ConnectionsMutex{};
            std::unordered_map<std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<ConnectionInfo>> _Connections;

            std::atomic<bool> _Stopped{};

            ConnectionCallback _ConnectionCb{};
            DisconnectCallback _DisConnectionCb{};
            DataCallback _DataCb{};

        private:
            JoTcpServer(JoTcpServer& other) = delete;
            JoTcpServer(JoTcpServer&& other) = delete;
            JoTcpServer& operator=(JoTcpServer& other) = delete;
            JoTcpServer& operator=(JoTcpServer&& other) = delete;
        };
    }
}
#endif
#endif // EAPS_TCP_SERVER

