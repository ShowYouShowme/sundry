#ifndef __FvSingleton_H__
#define __FvSingleton_H__
#include "cocos2d.h"
#include <functional>
#include <algorithm>
//��Ҫʹ�þ�̬��Ա����,��ȱ��
template <class T>
class FvSingleton
{
public:
	FvSingleton()
	{
		pInstance() = static_cast<T *>(this);
	}

	~FvSingleton()
	{
		pInstance() = nullptr;
	}

	static T*& pInstance()
	{
		static T* ms_pkInstance(nullptr);
		return ms_pkInstance;
	}
};

//TODO ��ʵ��û��ѭ�����ܵ�
struct Time_CBInfoBase
{
	Time_CBInfoBase()
		:iIdex(0)
		,m_loop(false)
	{}
	int iIdex;
	double fDestTime;
	double fPastTime;//interval
	bool   m_loop;
	std::function<void()> fun;
	void callFun()
	{
		return fun();
	}
	~Time_CBInfoBase()
	{
		cocos2d::log("delete Time_CBInfoBase interval = %f", fPastTime);
	}
};

class TimeManager
	:public FvSingleton<TimeManager>
	, cocos2d::Ref
{
private:
	using Ptr_Time_CBInfoBase = std::shared_ptr<Time_CBInfoBase>;
	using Time_CBInfoList = std::vector<Ptr_Time_CBInfoBase>;
public:
	TimeManager();
	~TimeManager();
public:
	void init();
	void update(float fTime);
	void addCBList(Ptr_Time_CBInfoBase pCB);
	void removeByID(int iID);
	void removeByTime_CBInfo(Time_CBInfoBase* pInfo);
public:
	Ptr_Time_CBInfoBase addCerterTimeCB(std::function<void()> fun, float fTime, bool loop = false)
	{
		m_iID++;
		Ptr_Time_CBInfoBase pInfo = std::make_shared<Time_CBInfoBase>();
		pInfo->iIdex = m_iID;
		pInfo->fun = fun;
		pInfo->fDestTime = fTime + m_fPastTime;
		pInfo->fPastTime = fTime;
		pInfo->m_loop = loop;
		addCBList(pInfo);
		return pInfo;
	}

private:
	int				m_iID;
	double			m_fPastTime;
	Time_CBInfoList	m_kCBList;
};

TimeManager::TimeManager() : m_fPastTime(0), m_iID(0)
{
	cocos2d::Director::getInstance()->getScheduler()->schedule(CC_SCHEDULE_SELECTOR(TimeManager::update), this, 0.0f, false);
}

TimeManager::~TimeManager()
{
	cocos2d::Director::getInstance()->getScheduler()->unschedule(CC_SCHEDULE_SELECTOR(TimeManager::update), this);
}

void TimeManager::addCBList(Ptr_Time_CBInfoBase pCB)
{
	this->m_kCBList.push_back(pCB);
}

void TimeManager::removeByID(int iID)
{
	//��m_kCBList����ɾ��
	auto it = std::find_if(this->m_kCBList.begin(), this->m_kCBList.end(), [iID](Ptr_Time_CBInfoBase param){
		return param->iIdex == iID;
	});
	if (it != this->m_kCBList.end())
	{
		this->m_kCBList.erase(it);
	}
}

void TimeManager::update(float fTime)
{
	this->m_fPastTime += fTime;
	Time_CBInfoList delCallList;
	std::for_each(this->m_kCBList.begin(), this->m_kCBList.end(), [this, &delCallList](Ptr_Time_CBInfoBase param){
		if (param->fDestTime <= this->m_fPastTime)
		{
			param->callFun();
			if (param->m_loop == false)
			{
				delCallList.push_back(param);
			}
			else
			{
				param->fDestTime = param->fPastTime + this->m_fPastTime;
			}
		}
	});

	//TODO ������Դ������,Ȼ��Ӻ�����ǰ��ɾ��,�����Ͳ���Ҫ������
	//���Ѿ�ִ�еĺ���ɾ��
	std::for_each(delCallList.begin(), delCallList.end(), [this](Ptr_Time_CBInfoBase param)
	{
		auto it = std::find_if(this->m_kCBList.begin(), this->m_kCBList.end(), [param](Ptr_Time_CBInfoBase subParam){
			return param->iIdex == subParam->iIdex;
		});
		if (it != this->m_kCBList.end())
		{
			this->m_kCBList.erase(it);
		}
	});

}


#endif