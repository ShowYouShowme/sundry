/*
* File:   CPing.cpp
* Author: scotte.ye
*
* Created on 2011��1��26��, ����3:12
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
	unsigned char   icmp_type;        // ��Ϣ����
	unsigned char   icmp_code;        // ����
	unsigned short  icmp_checksum;    // У���
	// �����ǻ���ͷ
	unsigned short  icmp_id2;        // ����Ωһ��ʶ�������ID�ţ�ͨ������Ϊ����ID
	unsigned short  icmp_sequence;    // ���к�
	unsigned int   icmp_timestamp; // ʱ���
} ICMP_HDR, *PICMP_HDR;

typedef struct _IPHeader        // 20�ֽڵ�IPͷ
{
	unsigned char     iphVerLen;      // �汾�ź�ͷ���ȣ���ռ4λ��
	unsigned char     ipTOS;          // �������� 
	unsigned short    ipLength;       // ����ܳ��ȣ�������IP���ĳ���
	unsigned short    ipID;              // �����ʶ��Ωһ��ʶ���͵�ÿһ�����ݱ�
	unsigned short    ipFlags;          // ��־
	unsigned char     ipTTL;          // ����ʱ�䣬����TTL
	unsigned char     ipProtocol;     // Э�飬������TCP��UDP��ICMP��
	unsigned short    ipChecksum;     // У���
	unsigned int     ipSource;       // ԴIP��ַ
	unsigned int     ipDestination;  // Ŀ��IP��ַ
} IPHeader, *PIPHeader;

#pragma pack()

bool set_socket_addr2(struct sockaddr_in *addr, const char *ip, int port)
{
	struct hostent *hp;
	//struct in_addr in;
	struct sockaddr_in local_addr;
	// ֧������ֱ�ӽ�������
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

	// ������
	if (size)
	{
		cksum += *(unsigned char*)buff;
	}
	// ��32λ��chsum��16λ�͵�16λ��ӣ�Ȼ��ȡ��
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
	// ���ý��ճ�ʱ
	int timeout = 1000;
	setsockopt(sRaw, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(int));
	setsockopt(sRaw, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(int));
	timeout = 128;
	setsockopt(sRaw, IPPROTO_IP, IP_TTL, (const char*)&timeout, sizeof(int));
	// ���ջ�����
	int nRecvBuf = 65535;
	setsockopt(sRaw, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuf, sizeof(int));

	int nBufSize = 65535;
	setsockopt(sRaw, SOL_SOCKET, SO_SNDBUF, (char*)&nBufSize, sizeof(int));

	// ����Ŀ�ĵ�ַ
	struct sockaddr_in dest;
	if (!set_socket_addr2(&dest, m_Ip.c_str(), 0))
	{
		close_tcp_socket(sRaw);
		sRaw = -1;
		return;
	}

	// ����ICMP���
	char buff[sizeof(ICMP_HDR) + 32];
	ICMP_HDR* pIcmp = (ICMP_HDR*)buff;
	// ��дICMP�������
	pIcmp->icmp_type = 8;    // ����һ��ICMP����
	pIcmp->icmp_code = 0;
	pIcmp->icmp_id2 = 1000;
	pIcmp->icmp_checksum = 0;
	pIcmp->icmp_sequence = 0;
	// ������ݲ��֣�����Ϊ����

	memset(&buff[sizeof(ICMP_HDR)], 'E', 32);
	// ��ʼ���ͺͽ���ICMP���
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
		// ���濪ʼ�������յ���ICMP���
		unsigned int nTick = clock();
		if (nRet < sizeof(IPHeader) + sizeof(ICMP_HDR))
		{
		}
		// cocos2d::log(" Too few bytes from %s  len=%d\n", inet_ntoa(from.sin_addr),nRet);

		// ���յ��������а���IPͷ��IPͷ��СΪ20���ֽڣ����Լ�20�õ�ICMPͷ
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
		ICMP_HDR* pRecvIcmp = (ICMP_HDR*)(recvBuf); // (ICMP_HDR*)(recvBuf + sizeof(IPHeader));
#else
		ICMP_HDR* pRecvIcmp = (ICMP_HDR*)(recvBuf + 20); // (ICMP_HDR*)(recvBuf + sizeof(IPHeader));


#endif
		//	ICMP_HDR* pRecvIcmp = (ICMP_HDR*)(recvBuf + 20); // (ICMP_HDR*)(recvBuf + sizeof(IPHeader));
		//		if (pRecvIcmp->icmp_type != 0)    // ����
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
