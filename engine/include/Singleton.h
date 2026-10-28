#pragma once
/** @file Singleton.h
 * @brief Typed manager singleton lifecycle support.
 */

#include "ErrorHandler.h"

template <class T>
/** @brief Restricts manager instances to one engine-owned object per type.
 */
class Singleton
{
public:
    /** @brief Returns the instance of Singleton.
     */
    inline static T& GetInstance();
    /** @brief Returns the instance ptr of Singleton.
     */
    inline static T* GetInstancePtr();
    /** @brief Returns the instance ptr no error msg of Singleton.
     */
    inline static T* GetInstancePtr_NO_ERROR_MSG();

protected:
    /** @brief Creates create singleton for the current scene or service.
     */
    inline static bool CreateSingleton();
    /** @brief Removes or clears the destroy singleton target.
     */
    inline static bool DestroySingleton();

    /** @brief Constructs a Singleton instance.
     */
    Singleton() = default;
    /** @brief Releases the Singleton instance and any owned resources.
     */
    ~Singleton() = default;

    /** @brief Constructs a Singleton instance.
     */
    Singleton(const Singleton&) = delete;
    /** @brief Implements operator= for this value or property type.
     */
    Singleton& operator=(const Singleton&) = delete;
    /** @brief Constructs a Singleton instance.
     */
    Singleton(Singleton&&) = delete;
    /** @brief Implements operator= for this value or property type.
     */
    Singleton& operator=(Singleton&&) = delete;

private:
    static T* mSingleton;
};

/** @brief Value named Singleton< T >::mSingleton used by Singleton.h.
 */
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