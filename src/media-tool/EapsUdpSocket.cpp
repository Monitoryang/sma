#include "EapsUdpSocket.h"
#include "Logger.h"

namespace eap
{
    UdpSocket::Ptr UdpSocket::createInstance(std::string dst_ip, int dst_port, std::string local_ip, int local_port, bool multicast)
    {
        try {
            return Ptr(new UdpSocket(local_ip, local_port, dst_ip, dst_port, multicast));
        }
        catch (const std::exception& e) {
            eap_error(std::string(e.what()));
        }
        return nullptr;
    }

    UdpSocket::UdpSocket(std::string local_ip, int local_port, std::string dst_ip, int dst_port, bool multicast)
    {
        _dst_ip = dst_ip;
        _dst_port = dst_port;

        // 本地网卡 IP 地址，端口设为 0 表示让操作系统自动分配
        Poco::Net::SocketAddress local_addr(local_ip, local_port);

        if (multicast) {
            _multi_udp_socket->bind(local_addr);
            _multi_udp_socket->joinGroup(Poco::Net::IPAddress(dst_ip));
        } 
        else {
            _udp_socket = std::make_shared<Poco::Net::DatagramSocket>(local_addr, true);
            Poco::Net::SocketAddress dst_addr(dst_ip, dst_port);
            _udp_socket->connect(dst_addr);
        }
    }

    UdpSocket::~UdpSocket()
    {
        if (_multi_udp_socket) {
            _multi_udp_socket->close();
        }
    }

    void UdpSocket::sendData(const uint8_t* buffer, const int& length)
    {
        if (_udp_socket && !_udp_socket->isNull()) {
            _udp_socket->sendBytes(buffer, length);
        }
    }

    void UdpSocket::sendData(const uint8_t* buffer, const int& length, std::string ip, int port)
    {
        if (_udp_socket) {
            Poco::Net::SocketAddress dst_addr(_dst_ip, _dst_port);
            _udp_socket->sendTo(buffer, length, dst_addr);
        }
    }

}
