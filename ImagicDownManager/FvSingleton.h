#pragma once
//不要使用静态成员变量,有缺陷
template <class T>
class FvSingleton
{
public:
	FvSingleton();

	~FvSingleton();

	static T*& pInstance();
};

template <class T>
T*& FvSingleton<T>::pInstance()
{
    static T* ms_pkInstance(nullptr);
    return ms_pkInstance;
}

template <class T>
FvSingleton<T>::~FvSingleton()
{
    pInstance() = nullptr;
}

template <class T>
FvSingleton<T>::FvSingleton()
{
    pInstance() = static_cast<T *>(this);
}
