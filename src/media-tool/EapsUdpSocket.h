/**
 *	@Created Date 	: 2024/9/5
 *	@Description	: udp socket м╗пе
 *	@Author 		: chenxin
 */

#ifndef JO_EAPS_UDP_SOCKET_H
#define JO_EAPS_UDP_SOCKET_H

#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/MulticastSocket.h"

#include <iostream>
#include <memory>

namespace eap
{
	class UdpSocket
	{
	public:
		using Ptr = std::shared_ptr<UdpSocket>;

	public:
		static Ptr createInstance(std::string dst_ip, int dst_port, std::string local_ip = "0.0.0.0", int local_port = 0, bool multicast = false);

		~UdpSocket();

	public:
		void sendData(const uint8_t* buffer, const int& length);
		void sendData(const uint8_t* buffer, const int& length, std::string ip, int port);

    private:
		std::string _dst_ip{};
		std::string _dst_port{};
		std::shared_ptr<Poco::Net::DatagramSocket> _udp_socket{};
        std::shared_ptr<Poco::Net::MulticastSocket> _multi_udp_socket{};

    private:
        UdpSocket(std::string local_ip, int local_port, std::string dst_ip, int dst_port, bool multicast);
		UdpSocket(UdpSocket& other) = delete;
		UdpSocket(UdpSocket&& other) = delete;
		UdpSocket& operator=(UdpSocket& other) = delete;
		UdpSocket& operator=(UdpSocket&& other) = delete;
	};
}

#endif
