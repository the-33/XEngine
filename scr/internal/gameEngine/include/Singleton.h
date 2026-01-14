#pragma once

#include "ErrorHandler.h"

template <class T>
class Singleton
{
public:
    inline static T& GetInstance();
    inline static T* GetInstancePtr();
    inline static T* GetInstancePtr_NO_ERROR_MSG();

protected:
    inline static bool CreateSingleton();
    inline static bool DestroySingleton();

    Singleton() = default;
    ~Singleton() = default;

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

private:
    static T* mSingleton;
};

template<class T> T* Singleton<T>::mSingleton = nullptr;

template<class T> inline bool Singleton<T>::CreateSingleton() {
    if (!mSingleton) {
        mSingleton = new T();
        return true;
    }
    else {
        LogError("Singleton warning", "CreateSingleton(): Singleton already created. FIX IMMEDIATELY.");
        return false;
    }
}

template<class T> inline bool Singleton<T>::DestroySingleton() {
    if (mSingleton) {
        delete mSingleton;
        mSingleton = NULL;
        return true;
    }
    else {
        LogError("Singleton warning", "DestroySingleton(): Singleton already destroyed or never created. FIX IMMEDIATELY.");
        return false;
    }
}

template<class T> inline T& Singleton<T>::GetInstance() {
    if (mSingleton) {
        return *mSingleton;
    }
    else {
        LogError("Singleton warning", "GetInstance(): Singleton not created, Call CreateSingleton().");
        static T* dummy = nullptr;
        return *dummy;
    }
}

template<class T> inline T* Singleton<T>::GetInstancePtr() {
    if (mSingleton) {
        return mSingleton;
    }
    else {
        LogError("Singleton warning", "GetInstancePtr(): Singleton not created, Call CreateSingleton().");
        return nullptr;
    }
}

template<class T> inline T* Singleton<T>::GetInstancePtr_NO_ERROR_MSG() {
    if (mSingleton) {
        return mSingleton;
    }
    else {
        return nullptr;
    }
}