#pragma once
#include "FvSingleton.h"
#include "cocos2d.h"
#include "network/HttpResponse.h"
#include "network/HttpClient.h"
#include <algorithm>
#include <memory>


struct ImagicDownInfo
{
    ImagicDownInfo()
    {
        fActTime = 0;
        pRequest = NULL;
        pButtonSprite = NULL;
    }
    cocos2d::Sprite* pSprite;//待设置icon的精灵
    cocos2d::Menu* pButtonSprite;////待设置icon的精灵
    cocos2d::network::HttpRequest* pRequest;
    std::string kUrl;//请求URL
    std::string kImagicName;//本地缓存文件路径
    float fActTime;//生存时间
};

/*
    功能:加载一个网络图片来设置sprite的skin--cocos内存管理这块我没管理,就是release和retain这两个
*/
typedef std::vector<ImagicDownInfo> ImagicDownInfoList;
class ImagicDownManager : public FvSingleton<ImagicDownManager>, public cocos2d::Ref
{
public:
    ImagicDownManager()
    {
        cocos2d::Director::getInstance()->getScheduler()->schedule(CC_SCHEDULE_SELECTOR(ImagicDownManager::upTime), this, 0.0f, false);
    }
    ~ImagicDownManager()
    {
        cocos2d::Director::getInstance()->getScheduler()->unschedule(CC_SCHEDULE_SELECTOR(ImagicDownManager::upTime), this);
    }
    void OnImagic()
    {
        if (this->m_pGetList.empty())
        {
            return;
        }
        cocos2d::network::HttpResponse * response = this->m_pGetList.front();
        this->m_pGetList.erase(this->m_pGetList.begin());
        std::string Url = response->getHttpRequest()->getUrl();
        std::string kImagicName = "ScenceHome/head_img_male.png";
        auto it = std::find_if(this->m_pDownList.begin(), this->m_pDownList.end(), [&Url](const ImagicDownInfo& elem){
            return elem.kUrl == Url;
        });
        //获取图片文件的路径
        if (it != this->m_pDownList.end())
        {
            kImagicName = it->kImagicName;
        }
        //把图片保存到本地
        std::vector<char>* buffer = response->getResponseData();
        cocos2d::CCImage* img = new cocos2d::CCImage();
        img->initWithImageData((unsigned char*)buffer->data(), buffer->size());
        bool bSuccess = false;
        if (img->getData())
        {
            img->saveToFile(kImagicName, false);
            bSuccess = true;
        }
        img->release();

        std::vector<ImagicDownInfoList::iterator> deleteIterVec;
        for (auto it = this->m_pDownList.begin(); it != this->m_pDownList.end(); ++it)
        {
            ImagicDownInfo& kInfo = *it;
            if (kInfo.kUrl == Url)
            {
                if (kInfo.pSprite)
                {
                    if (bSuccess)
                    {
                        kInfo.pSprite->setTexture(kInfo.kImagicName);
                    }
                }
                deleteIterVec.push_back(it);
            }
        }
        //只能从后往前删除,否则会导致迭代器失效
        std::for_each(deleteIterVec.rbegin(), deleteIterVec.rend(), [this](ImagicDownInfoList::iterator it)
        {
            this->m_pDownList.erase(it);
        });
        response->release();
    }
    void addDown(cocos2d::Node* pNode, std::string kUrl, int iUserID)
    {
        int iIdex = 0;
        std::for_each(kUrl.begin(), kUrl.end(), [&iIdex](char param){
            iIdex += static_cast<int>(param);
        });
        std::string fileName = std::to_string(iUserID) + "Idex" + std::to_string(iIdex);
        this->addDown(pNode, kUrl, fileName);
    }
    void addDown(cocos2d::Node* pNode, std::string kUrl, std::string kFileName)
    {
        cocos2d::Sprite* pSprite = dynamic_cast<cocos2d::Sprite*>(pNode);
        kFileName = cocos2d::FileUtils::getInstance()->getWritablePath() + kFileName + ".png";
        std::string StrSavePath = kFileName;
        if (cocos2d::CCTextureCache::sharedTextureCache()->addImage(StrSavePath.c_str()))
        {
            pSprite->setTexture(StrSavePath.c_str());
            return;
        }
        bool bHaveGet = false;
        for (auto it = this->m_pDownList.begin(); it != this->m_pDownList.end(); ++it)
        {
            ImagicDownInfo& kInfo = *it;
            if (kInfo.kImagicName == kFileName && kInfo.pSprite == pSprite)
            {
                return;
            }
            if (kInfo.kImagicName == kFileName)
            {
                bHaveGet = true;
            }
        }

        //构造http请求
        ImagicDownInfo kInfo;
        kInfo.pSprite = pSprite;
        kInfo.kImagicName = kFileName;
        kInfo.kUrl = kUrl;
        if (bHaveGet == false)
        {
            cocos2d::network::HttpRequest* pRequest = this->getFileEx(kUrl, CC_CALLBACK_2(ImagicDownManager::GetImagic, this));
            kInfo.pRequest = pRequest;
        }
        this->m_pDownList.push_back(kInfo);
    }
    cocos2d::network::HttpRequest* getFileEx(std::string kUrl, const cocos2d::network::ccHttpRequestCallback &callback)
    {
        cocos2d::network::HttpRequest *request = new cocos2d::network::HttpRequest();
        request->setUrl(kUrl.c_str());

        request->setResponseCallback(callback);

        request->setRequestType(cocos2d::network::HttpRequest::Type::GET);

        cocos2d::network::HttpClient::getInstance()->sendImmediate(request);
        request->release();
        return request;
    }
    void upTime(float fTime)
    {
        this->OnImagic();
        for (std::vector<ImagicDownInfo>::iterator it = this->m_pDownList.begin(); it != this->m_pDownList.end(); )
        {
            auto& kInfo = *it;
            kInfo.fActTime += fTime;
            if (kInfo.fActTime > 20.0f)
            {
                if (kInfo.pSprite)
                {
                    kInfo.pSprite->release();
                }
                it = this->m_pDownList.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void GetImagic(cocos2d::network::HttpClient *sender, cocos2d::network::HttpResponse *response)
    {
        response->retain();
        this->m_pGetList.push_back(response);
    }
private:
    int m_iIdexCout;
    ImagicDownInfoList m_pDownList;
    std::vector<cocos2d::network::HttpResponse*> m_pGetList;
};