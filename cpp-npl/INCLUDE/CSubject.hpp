#ifndef SUBJECT_HPP
#define SUBJECT_HPP

#define NS_NPL namespace NPL {
#define NS_END }

#include <map>
#include <any>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include <utility>
#include <iostream>
#include <algorithm>
#include <functional>

NS_NPL

template <typename T1, typename T2>
class CSubject : public std::enable_shared_from_this<CSubject<T1, T2>>
{
  public:

    using SPCSubject = std::shared_ptr<CSubject<T1, T2>>;
    using WPCSubject = std::weak_ptr<CSubject<T1, T2>>;

    CSubject() = default;

    virtual ~CSubject()
    {
      std::lock_guard<std::mutex> lg(iLock);
      RemoveAllEventListenersInternal();
      std::cout << "~CSubject : " + iName + "\n";
    }

    virtual void SetTarget(const WPCSubject& target)
    {
      std::lock_guard<std::mutex> lg(iLock);
      iTarget = target;
    }

    virtual void QueuePendingContext(SPCSubject s, void *c)
    {
      auto target = iTarget.lock();

      if (target)
      {
        return target->QueuePendingContext(s, c);
      }      
    }

    virtual void * Read(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0)
    {
      std::lock_guard<std::mutex> lg(iLock);

      auto target = iTarget.lock();

      if (target)
      {
        return target->Read(b, l, o);
      }

      return nullptr;
    }

    virtual void Write(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0)
    {
      std::lock_guard<std::mutex> lg(iLock);

      auto target = iTarget.lock();

      if (target)
      {
        target->Write(b, l, o);
      }
    }

    virtual int32_t ReadSync(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0)
    {
      std::lock_guard<std::mutex> lg(iLock);

      auto target = iTarget.lock();

      if (target)
      {
        return target->ReadSync(b, l, o);
      }

      return -1;
    }

    virtual int32_t WriteSync(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0)
    {
      std::lock_guard<std::mutex> lg(iLock);

      auto target = iTarget.lock();

      if (target)
      {
        return target->WriteSync(b, l, o);
      }

      return -1;
    }

    virtual void OnAccept(void)
    {
      std::lock_guard<std::mutex> lg(iLock);
      NotifyAccept();
    }

    virtual void OnConnect(void)
    {
      std::lock_guard<std::mutex> lg(iLock);
      iConnected = true;
      NotifyConnect();
    }

    virtual void OnRead(const T1 *b, size_t n)
    {
      std::lock_guard<std::mutex> lg(iLock);
      NotifyRead((const T2 *)b, n);
    }

    virtual void OnWrite(const T1 *b, size_t n)
    {
      std::lock_guard<std::mutex> lg(iLock);
      NotifyWrite(b, n);
    }

    virtual void OnDisconnect(void)
    {
      std::lock_guard<std::mutex> lg(iLock);
      iConnected = false;
      NotifyDisconnect();
      MarkRemoveAllListeners();
      MarkRemoveSelfAsListener();
    }

    virtual void OnEvent(std::any e)
    {
      std::lock_guard<std::mutex> lg(iLock);
      NotifyEvent(e);
    }

    virtual const SPCSubject& AddEventListener(const SPCSubject& observer)
    {
      std::lock_guard<std::mutex> lg(iLock);
      iObservers.push_back(observer);
      observer->SetTarget(this->weak_from_this());
      return observer;
    }

    virtual void RemoveEventListener(const SPCSubject& observer)
    {
      std::lock_guard<std::mutex> lg(iLock);
      RemoveEventListenerInternal(observer);
    }

    virtual void RemoveAllEventListeners(void)
    {
      std::lock_guard<std::mutex> lg(iLock);
      RemoveAllEventListenersInternal();
    }

    virtual void MarkRemoveAllListeners(void)
    {
      iMarkRemoveAllListeners = true;
    }

    virtual void MarkRemoveSelfAsListener(void)
    {
      iMarkRemoveSelfAsListener = true;
    }

    virtual bool IsMarkRemoveSelfAsListener(void)
    {
      return iMarkRemoveSelfAsListener;
    }

    virtual bool IsConnected(void)
    {
      return iConnected;
    }

    virtual bool IsDispatcher(void)
    {
      return false;
    }
    
    virtual std::string GetName(void)
    {
      return iName;
    }

    virtual void SetName(const std::string& aName)
    {
      iName = aName;
    }

    SPCSubject GetDispatcher(void)
    {
       auto target = iTarget;

       while (true)
       {
         auto sp = target.lock();

         if (sp)
         {
           if (sp->IsDispatcher())
           {
             return sp;
           }
           else
           {
             target = sp->iTarget;
           }
         }
         else
         {
           return nullptr;
         }
       } 
    }
    
    virtual void SetProperty(const std::string& key, const std::string& value)
    {
      std::lock_guard<std::mutex> lg(iLock);
      iPropertyMap[key] = value;
    }

    virtual std::string GetProperty(const std::string& key)
    {
      std::lock_guard<std::mutex> lg(iLock);

      std::string value = "";

      try
      {
        value = iPropertyMap.at(key);
      }
      catch(const std::exception& e)
      {
        std::cout << e.what() << '\n';
      }
      
      return value;
    }

    virtual int GetPropertyAsInt(const std::string& key)
    {
      auto value = GetProperty(key);
      return std::stoi(value);
    }

    virtual bool GetPropertyAsBool(const std::string& key)
    {
      auto value = GetProperty(key); 
      return ((value.size() && value == "true") ? true : false);
    }

  protected:

    std::mutex iLock;

    WPCSubject iTarget;

    bool iConnected = false;

    std::string iName = "Subject";

    bool iMarkRemoveAllListeners = false;

    bool iMarkRemoveSelfAsListener = false;

    std::vector<SPCSubject> iObservers;

    std::map<std::string, std::string> iPropertyMap;

    virtual void ResetSubject(SPCSubject& subject)
    {
      subject->MarkRemoveAllListeners();
      subject->MarkRemoveSelfAsListener();
      subject.reset();
    }

    virtual void ProcessMarkRemoveAllListeners(void)
    {
      if (this->iMarkRemoveAllListeners)
      {
        this->RemoveAllEventListenersInternal();        
      }

      for (auto it = iObservers.begin(); it != iObservers.end(); )
      {
        if ((*it)->IsMarkRemoveSelfAsListener())
        {
          it = this->RemoveEventListenerInternal(*it);
        }
        else
        {
          it++;
        }
      }
    }

    virtual void RemoveAllEventListenersInternal()
    {
      iObservers.clear();
      iMarkRemoveAllListeners = false;      
    }

    auto RemoveEventListenerInternal(const SPCSubject& consumer)
    {
      return iObservers.erase(
        std::remove(
          iObservers.begin(), iObservers.end(), consumer),
        iObservers.end()
      );
    }

    virtual void NotifyConnect()
    {
      for (auto& observer : iObservers)
      {
        observer->OnConnect();
      }
      ProcessMarkRemoveAllListeners();
    }

    virtual void NotifyRead(const T2 *b, size_t n)
    {
      for (auto& observer : iObservers)
      {
        observer->OnRead(b, n);
      }
      ProcessMarkRemoveAllListeners();
    }

    virtual void NotifyWrite(const T2 *b, size_t n)
    {
      for (auto& observer : iObservers)
      {
        observer->OnWrite(b, n);
      }
      ProcessMarkRemoveAllListeners();
    }

    virtual void NotifyDisconnect()
    {
      for (auto& observer : iObservers)
      {
        observer->OnDisconnect();
      }
      ProcessMarkRemoveAllListeners();
    }

    virtual void NotifyAccept(void)
    {
      for (auto& observer : iObservers)
      {
        observer->OnAccept();
      }
      ProcessMarkRemoveAllListeners();
    }

    virtual void NotifyEvent(std::any e)
    {
      for (auto& observer : iObservers)
      {
        observer->OnEvent(e);
      }
      ProcessMarkRemoveAllListeners();
    }
};

template <typename T1, typename T2>
using SPCSubject = std::shared_ptr<CSubject<T1, T2>>;

template<typename T1, typename T2>
using WPCSubject = std::weak_ptr<CSubject<T1, T2>>;

NS_END

#endif //COMPONENT_HPP
