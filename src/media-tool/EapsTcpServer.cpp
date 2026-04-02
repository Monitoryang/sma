#include "EapsTcpServer.h"
#include "Logger.h"
#include <iostream>
#ifdef ENABLE_AIRBORNE	
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;
namespace eap {
    namespace common {
        JoTcpServer::InitParameterPtr JoTcpServer::MakeInitParameter()
        {
            return InitParameterPtr(new InitParameter());
        }

        JoTcpServer::JoTcpServer(InitParameterPtr init_param_ptr) :_IoContext()
            , _Acceptor(_IoContext), _Signals(_IoContext), _Connections()
        {
            _Port = init_param_ptr->port;
            _MaxConnections = init_param_ptr->max_connections;
            _ConnectionTimeout = init_param_ptr->timeout;
            _Stopped.store(false);

            // 设置信号处理
            _Signals.add(SIGINT);
            _Signals.add(SIGTERM);

            _SetupSignals();
            _StartAccept();
        }

        JoTcpServer::~JoTcpServer()
        {
            stop();
        }



        void JoTcpServer::setConnectionCallback(ConnectionCallback cb)
        {
            _ConnectionCb = std::move(cb);
        }

        void JoTcpServer::setDisConnectionCallback(DisconnectCallback cb)
        {
            _DisConnectionCb = std::move(cb);
        }

        void JoTcpServer::setDataCallback(DataCallback cb)
        {
            _DataCb = std::move(cb);
        }

        void JoTcpServer::broadcast(const std::string data)
        {
            std::lock_guard<std::mutex> lock(_ConnectionsMutex);
            for (auto& [socket, _] : _Connections) {
                _EnqueueMessage(socket, data);
            }
        }

        void JoTcpServer::sendTo(std::shared_ptr<tcp::socket> client, const std::string& data)
        {
            if (client) {
                std::lock_guard<std::mutex> lock(_ConnectionsMutex);
                if (_Connections.find(client) != _Connections.end()) {
                    _EnqueueMessage(client, data);
                }
            }
        }

        uint64_t JoTcpServer::connectionCount()
        {
            std::lock_guard<std::mutex> lock(_ConnectionsMutex);
            return _Connections.size();
        }

        bool JoTcpServer::_IsStopped()
        {
            return _Stopped;
        }

        void JoTcpServer::run()
        {
            eap_information("_IoContext.run() start start start start start start ");
            _IoContext.run();
            eap_information("_IoContext.run() end end end end end end ");
        }

        void JoTcpServer::stop()
        {
            if (!_Stopped.exchange(true)) {
                // 取消所有异步操作
                asio::post(_IoContext, [this]() {
                    std::lock_guard<std::mutex> lock(_ConnectionsMutex);

                    // 关闭所有连接
                    for (auto& [socket, conn_info] : _Connections) {
                        // 取消定时器
                        conn_info->timeout_timer->cancel();

                        // 关闭socket
                        boost::system::error_code ec;
                        socket->shutdown(tcp::socket::shutdown_both, ec);
                        socket->close(ec);

                        // 调用断开回调
                        if (_DisConnectionCb) {
                            _DisConnectionCb(socket);
                        }
                    }
                    _Connections.clear();

                    // 关闭acceptor
                    boost::system::error_code ec;
                    _Acceptor.close(ec);

                    // 取消信号处理
                    _Signals.cancel();
                });

                // 停止io_context
                if (!_IoContext.stopped()) {
                    _IoContext.stop();
                }
            }
        }


        void JoTcpServer::_SetupSignals()
        {
            _Signals.async_wait([this](const boost::system::error_code& ec, int signo) {
                if (!ec) {
                    eap_warning_printf("received signal: {}, shutting down... ", signo);
                    stop();
                }
            });
        }

        void JoTcpServer::_StartAccept()
        {
            try
            {
                // 打开并绑定acceptor
                _Acceptor.open(tcp::v4());
                _Acceptor.set_option(tcp::acceptor::reuse_address(true));
                _Acceptor.bind(tcp::endpoint(tcp::v4(), _Port));
                _Acceptor.listen();

                _AsyncAccept();
            }
            catch (const std::exception& e)
            {
                eap_error_printf("failed to start acceptor: {}", e.what());
                stop();
            }
        }

        void JoTcpServer::_AsyncAccept()
        {
            auto my_socket = std::make_shared<tcp::socket>(_IoContext);
            _Acceptor.async_accept(*my_socket, [this, my_socket](const boost::system::error_code& ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lock(_ConnectionsMutex);
                    // 检查连接数限制
                    if (_Connections.size() >= _MaxConnections) {
                        eap_error_printf("connection limit reached: {} , rejecting new connection", _MaxConnections);
                        my_socket->close();
                        _AsyncAccept();
                        return;
                    }

                    // 创建连接信息
                    auto conn_info = std::make_shared<ConnectionInfo>();
                    conn_info->last_activity = std::chrono::steady_clock::now();
                    conn_info->timeout_timer = std::make_shared<asio::steady_timer>(_IoContext);

                    // 添加到连接集合
                    _Connections[my_socket] = conn_info;

                    // 设置超时检查
                    _StartTimeoutCheck(my_socket, conn_info);

                    // 新连接回调
                    if (_ConnectionCb) {
                        _ConnectionCb(my_socket);
                    }

                    // 开始接受数据
                    _StartReceive(my_socket, conn_info);

                    // 继续接受新连接
                    _AsyncAccept();
                }
                else if (ec != asio::error::operation_aborted) {
                    eap_error_printf("accept error: {}", ec.message());
                    if (!_Stopped) {
                        // 尝试重新接受连接
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        _AsyncAccept();
                    }
                }
            });
        }

        void JoTcpServer::_StartReceive(std::shared_ptr<asio::ip::tcp::socket> socket,
            std::shared_ptr<ConnectionInfo> conn_info)
        {
            auto buffer = std::make_shared<std::array<char, 1024>>();
            socket->async_read_some(asio::buffer(*buffer),
                [this, socket, conn_info, buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
                if (!ec) {
                    // 更新最后活动时间
                    conn_info->last_activity = std::chrono::steady_clock::now();

                    // 处理接收到的数据
                    if (_DataCb) {
                        _DataCb(socket, std::string(buffer->data(), bytes_transferred));
                    }

                    // 继续接收
                    _StartReceive(socket, conn_info);
                }
                else {
                    // 连接断开
                    _HandleDisconnect(socket);
                }
            });
        }

        void JoTcpServer::_EnqueueMessage(std::shared_ptr<tcp::socket> socket, const std::string& data)
        {
            auto it = _Connections.find(socket);
            if (it != _Connections.end()) {
                auto& conn_info = it->second;
                conn_info->send_queue.push(data);

                // eap_warning_printf("conn_info->send_queue.size() =================== {} ", (int)conn_info->send_queue.size());

                if (!conn_info->is_sending) {
                    conn_info->is_sending = true;
                    _StartSend(socket, conn_info);
                }
            }
        }

        void JoTcpServer::_StartSend(std::shared_ptr<tcp::socket> socket, std::shared_ptr<ConnectionInfo> conn_info)
        {
            if (conn_info->send_queue.empty()) {
                conn_info->is_sending = false;
                return;
            }

            const std::string& message = conn_info->send_queue.front();

            static int send_message_count = 0;
            // eap_warning_printf("send_message_count =================== {} ", send_message_count);
            send_message_count++;

            asio::async_write(*socket, asio::buffer(message),
                [this, socket, conn_info](const boost::system::error_code& ec, size_t) {
                // std::lock_guard<std::mutex> lock(_ConnectionsMutex);
                std::unique_lock<std::mutex> lock(_ConnectionsMutex);
                if (!ec) {
                    // 发送成功，从队列移除
                    conn_info->send_queue.pop();

                    // 继续发送队列中的下一条消息
                    _StartSend(socket, conn_info);
                }
                else {
                    lock.unlock();
                    // 发送失败，断开连接
                    _HandleDisconnect(socket);
                }
            });
        }

        void JoTcpServer::_StartTimeoutCheck(std::shared_ptr<tcp::socket> socket, std::shared_ptr<ConnectionInfo> conn_info)
        {
            conn_info->timeout_timer->expires_after(std::chrono::seconds(_ConnectionTimeout));
            conn_info->timeout_timer->async_wait(
                [this, socket, conn_info](const boost::system::error_code& ec) {
                if (!ec) {
                    // std::lock_guard<std::mutex> lock(_ConnectionsMutex);
                    std::unique_lock<std::mutex> lock(_ConnectionsMutex);
                    // 再次检查连接是否还存在
                    if (_Connections.find(socket) == _Connections.end()) {
                        return;
                    }

                    auto now = std::chrono::steady_clock::now();
                    auto idle_time = now - conn_info->last_activity;
                    auto idle_time_sec = std::chrono::duration_cast<std::chrono::seconds>(idle_time).count();

                    if (idle_time_sec >= _ConnectionTimeout) {
                        lock.unlock();
                        eap_error("connection timeout: "/*, socket->remote_endpoint()*/);
                        _HandleDisconnect(socket);
                    }
                    else {
                        // 重新设置定时器
                        _StartTimeoutCheck(socket, conn_info);
                    }
                }
            });
        }

        void JoTcpServer::_HandleDisconnect(std::shared_ptr<tcp::socket> socket)
        {
            // 从连接集合中移除
            std::shared_ptr<ConnectionInfo> conn_info;
            {
                std::lock_guard<std::mutex> lock(_ConnectionsMutex);
                auto it = _Connections.find(socket);
                if (it == _Connections.end()) {
                    return;
                }
                conn_info = it->second;
                _Connections.erase(it);
            }

            // 取消定时器
            conn_info->timeout_timer->cancel();

            // 关闭socket
            boost::system::error_code ec;
            socket->shutdown(tcp::socket::shutdown_both, ec);
            socket->close(ec);

            // 调用断开回调
            if (_DisConnectionCb) {
                _DisConnectionCb(socket);
            }
        }
    }
}
#endif
