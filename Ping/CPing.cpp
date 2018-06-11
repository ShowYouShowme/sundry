/*
* File:   CPing.cpp
* Author: scotte.ye
*
* Created on 2011年1月26日, 下午3:12
*/

#include "CPing.h"

#include <signal.h>
#include <sys/types.h>
#include <setjmp.h>
#include <errno.h>
#include <iostream>

#ifndef _WIN32
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <signal.h>

#else

#include <winsock2.h>

#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "ws2_32.lib")
#endif

using namespace std;

#pragma pack(1)
typedef struct icmp_hdr
{
	unsigned char   icmp_type;        // 消息类型
	unsigned char   icmp_code;        // 代码
	unsigned short  icmp_checksum;    // 校验和
	// 下面是回显头
	unsigned short  icmp_id2;        // 用来惟一标识此请求的ID号，通常设置为进程ID
	unsigned short  icmp_sequence;    // 序列号
	unsigned int   icmp_timestamp; // 时间戳
} ICMP_HDR, *PICMP_HDR;

typedef struct _IPHeader        // 20字节的IP头
{
	unsigned char     iphVerLen;      // 版本号和头长度（各占4位）
	unsigned char     ipTOS;          // 服务类型 
	unsigned short    ipLength;       // 封包总长度，即整个IP报的长度
	unsigned short    ipID;              // 封包标识，惟一标识发送的每一个数据报
	unsigned short    ipFlags;          // 标志
	unsigned char     ipTTL;          // 生存时间，就是TTL
	unsigned char     ipProtocol;     // 协议，可能是TCP、UDP、ICMP等
	unsigned short    ipChecksum;     // 校验和
	unsigned int     ipSource;       // 源IP地址
	unsigned int     ipDestination;  // 目标IP地址
} IPHeader, *PIPHeader;

#pragma pack()

bool set_socket_addr2(struct sockaddr_in *addr, const char *ip, int port)
{
	struct hostent *hp;
	//struct in_addr in;
	struct sockaddr_in local_addr;
	// 支持域名直接解析处理
	if (!(hp = gethostbyname(ip)))
	{
		return false;
	}

	memcpy(&local_addr.sin_addr.s_addr, hp->h_addr, 4);

	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = local_addr.sin_addr.s_addr;
	return true;
}


int close_tcp_socket(int sockfd)
{
	shutdown(sockfd, 2);

#ifdef _WIN32
	return closesocket(sockfd);
#else
	return close(sockfd);

#endif 

}

int getsockerr(int sockfd)
{
	int error = -1;
	int len = sizeof(int);
	getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&error, (socklen_t *)&len);
	if (error != 0)
	{
		return error;
	}

	return error;
}


CPing::CPing()
{
	m_nPintTime = 0;
	sRaw = -1;
	m_hThread = 0;
}


CPing::~CPing()
{


}

void CPing::Close()
{
	m_flg = true;
	//if (m_hThread)
	//{
	//	m_hThread->join();
	//	delete m_hThread;
	//	m_hThread = 0;
	//}
	m_nPintTime = -1;
	close_tcp_socket(sRaw);
	sRaw = -1;
}

unsigned short CPing::checksum(unsigned short* buff, int size)
{
	unsigned int cksum = 0;
	while (size > 1)
	{
		cksum += *buff++;
		size -= sizeof(unsigned short);
	}

	// 是奇数
	if (size)
	{
		cksum += *(unsigned char*)buff;
	}
	// 将32位的chsum高16位和低16位相加，然后取反
	while (cksum >> 16)
		cksum = (cksum >> 16) + (cksum & 0xffff);
	return (unsigned short)(~cksum);
}
void CPing::init(const char * ip, int timeout)
{
#if WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	m_Ip = ip;
	m_flg = false;
	m_hThread = new std::thread(&CPing::ClientIOThreadProc, this);
}

void CPing::reset()
{
	if (m_nPintTime >= 0)
	{
		return;
	}

	m_flg = false;
	m_hThread = new std::thread(&CPing::ClientIOThreadProc, this);
}

void CPing::ClientIOThreadProc()
{
	//    cocos2d::log("CPing 1 ");
	//    struct protoent *protocol;
	//    if((protocol = getprotobyname("icmp")) == 0)
	//    {
	//        return;
	//    }
	//    cocos2d::log("CPing 2 ");
#ifndef WIN32
	setuid(getuid());
#endif
#if WIN32
	sRaw = (int)socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
#else

	sRaw = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);

#endif 

	//	 cocos2d::log("CPing 3 ");
	if (sRaw == -1)
	{
		return;
	}

	// cocos2d::log("CPing 4 ");
	// 设置接收超时
	int timeout = 1000;
	setsockopt(sRaw, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(int));
	setsockopt(sRaw, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(int));
	timeout = 128;
	setsockopt(sRaw, IPPROTO_IP, IP_TTL, (const char*)&timeout, sizeof(int));
	// 接收缓冲区
	int nRecvBuf = 65535;
	setsockopt(sRaw, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(int));

	int nBufSize = 65535;
	setsockopt(sRaw, SOL_SOCKET, SO_SNDBUF, (char*)&nBufSize, sizeof(int));

	// 设置目的地址
	struct sockaddr_in dest;
	if (!set_socket_addr2(&dest, m_Ip.c_str(), 0))
	{
		close_tcp_socket(sRaw);
		sRaw = -1;
		return;
	}

	// 创建ICMP封包
	char buff[sizeof(ICMP_HDR) + 32];
	ICMP_HDR* pIcmp = (ICMP_HDR*)buff;
	// 填写ICMP封包数据
	pIcmp->icmp_type = 8;    // 请求一个ICMP回显
	pIcmp->icmp_code = 0;
	pIcmp->icmp_id2 = 1000;
	pIcmp->icmp_checksum = 0;
	pIcmp->icmp_sequence = 0;
	// 填充数据部分，可以为任意

	memset(&buff[sizeof(ICMP_HDR)], 'E', 32);
	// 开始发送和接收ICMP封包
	unsigned short    nSeq = 0;
	char recvBuf[1024];

	sockaddr_in from;
	int nLen = sizeof(from);

	while (1)
	{
		if (m_flg)
		{
			m_nPintTime = -1;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		int  error = getsockerr(sRaw);
		if (error < 0)
		{
			//	printf("Request timed out\n");
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		if (error == ECONNREFUSED || error == ETIMEDOUT)
		{
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}

		int nRet;
		unsigned int   times1 = clock();
		pIcmp->icmp_checksum = 0;
		pIcmp->icmp_timestamp = times1;
		pIcmp->icmp_sequence = nSeq++;
		pIcmp->icmp_checksum = checksum((unsigned short*)buff, sizeof(ICMP_HDR) + 32);
		nRet = sendto(sRaw, buff, sizeof(ICMP_HDR) + 32, 0, (sockaddr *)&dest, sizeof(dest));
		if (nRet == -1)
		{
			//	printf(" sendto() failed: %d \n", ::WSAGetLastError());
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		error = getsockerr(sRaw);
		if (error < 0)
		{
			//	printf("Request timed out\n");
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		if (error == ECONNREFUSED || error == ETIMEDOUT)
		{
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		memset(recvBuf, 0, 1024);

		while (1)
		{
			nRet = recvfrom(sRaw, recvBuf, 1024, 0, (struct sockaddr*)&from, (socklen_t *)&nLen);
			if (std::string(inet_ntoa(from.sin_addr)) == std::string(inet_ntoa(dest.sin_addr)))
			{
				break;
			}
		}
		std::cout << std::string("dest ip:") + inet_ntoa(from.sin_addr) << std::endl;
		if (nRet == -1)
		{
			m_nPintTime = -3;
			//printf(" recvfrom() failed: %d\n", ::WSAGetLastError());
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		error = getsockerr(sRaw);
		if (error < 0)
		{
			//	printf("Request timed out\n");
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		if (error == ECONNREFUSED || error == ETIMEDOUT)
		{
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		// 下面开始解析接收到的ICMP封包
		unsigned int nTick = clock();
		if (nRet < sizeof(IPHeader) + sizeof(ICMP_HDR))
		{
		}
		// cocos2d::log(" Too few bytes from %s  len=%d\n", inet_ntoa(from.sin_addr),nRet);

		// 接收到的数据中包含IP头，IP头大小为20个字节，所以加20得到ICMP头
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
		ICMP_HDR* pRecvIcmp = (ICMP_HDR*)(recvBuf); // (ICMP_HDR*)(recvBuf + sizeof(IPHeader));
#else
		ICMP_HDR* pRecvIcmp = (ICMP_HDR*)(recvBuf + 20); // (ICMP_HDR*)(recvBuf + sizeof(IPHeader));


#endif
		//	ICMP_HDR* pRecvIcmp = (ICMP_HDR*)(recvBuf + 20); // (ICMP_HDR*)(recvBuf + sizeof(IPHeader));
		//		if (pRecvIcmp->icmp_type != 0)    // 回显
		//		{
		//			cocos2d::log(" nonecho type %d recvd \n", pRecvIcmp->icmp_type);
		//			m_nPintTime = -1;
		//			cocos2d::close_tcp_socket(sRaw);
		//			sRaw = -1;
		//			return;
		//		}
		//
		//		if (pRecvIcmp->icmp_id2 != 1000)
		//		{
		//			m_nPintTime = -1;
		//			cocos2d::log(" someone else's packet! %d \n",pRecvIcmp->icmp_id2 );
		//			cocos2d::close_tcp_socket(sRaw);
		//			sRaw = -1;
		//			return ;
		//		}


		//printf(" %d bytes from %s:", nRet, inet_ntoa(from.sin_addr));
		//	cocos2d::log(" icmp_seq = %d. ", pRecvIcmp->icmp_sequence);

		m_nPintTime = static_cast<double>(nTick - times1) / CLOCKS_PER_SEC * 1000;
		std::cout << std::string("pingTime:") + std::to_string(m_nPintTime) + "\n";
		if (m_nPintTime > 1000)
		{
			m_nPintTime = -2;
			close_tcp_socket(sRaw);
			sRaw = -1;
			return;
		}
		//cocos2d::log("ping time: %d ms ", m_nPintTime);
#if _WIN32
		Sleep(2000);
#else
		sleep(2);
#endif
	}
	close_tcp_socket(sRaw);
	sRaw = -1;
}
