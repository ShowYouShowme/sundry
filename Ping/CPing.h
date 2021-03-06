/*
* File:   CPing.h
* Author: scotte.ye
*
* Created on 2011年1月26日, 下午3:12
*/

#ifndef CPING_H
#define	CPING_H
#include <string>
#include <thread> 

/*
* 进程同时能打开的文件数量是有限制的,Linux上面是65535,如果socket不使用了记得关闭,可以用shared_ptr来回收这种资源
*/

class CPing
{
public:
	virtual ~CPing();
	void Close();
	unsigned short checksum(unsigned short* buff, int size);
	void init(const char * ip, int timeout);
	void reset();

	void ClientIOThreadProc();

	int GetTime(){ return m_nPintTime; }

	static CPing* getInstance()
	{
		static CPing* pInstance(nullptr);
		if (pInstance == nullptr)
		{
			pInstance = new CPing();
		}
		return pInstance;
	}
private:
	CPing();
private:
	int sRaw;
	int m_nPintTime;
	std::thread*	m_hThread;	// 线程句柄

	std::string m_Ip;
	bool m_flg;
};
#endif	/* CPING_H */
