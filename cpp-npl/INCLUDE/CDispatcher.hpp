#ifndef DISPATCHER_HPP
#define DISPATCHER_HPP

#include <CDevice.hpp>
#include <CSubject.hpp>
#include <CListener.hpp>
#include <CDeviceSocket.hpp>

#include <thread>
#include <iostream>

#ifdef linux
 #include <unistd.h>
 #include <sys/epoll.h>
 #include <string.h>
#endif

NS_NPL

class CDispatcher : public CSubject<uint8_t, uint8_t>
{
  public:

    CDispatcher()
    {
      iName = "D";

      #ifdef linux
      iEventPort = epoll_create1(0);
      #else
      iEventPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
      #endif

      if (!iWorker.joinable())
      {
        iWorker = std::thread(&CDispatcher::Worker, this);
      }
    }

    ~CDispatcher()
    {
      #ifdef linux

        iWorker.join();

        if (iEventPort >= 0)
        {
          close(iEventPort);
        }

      #else

        PostQueuedCompletionStatus(iEventPort, 0, 0, 0);

        iWorker.join();

        if (iEventPort != INVALID_HANDLE_VALUE)
        {
          CloseHandle(iEventPort);
        }

      #endif
    }

    void InitializeControl(void)
    {
      iTarget = weak_from_this();

      iDServer = std::make_shared<CDeviceSocket>();

      iDServer->SetHostAndPort("0.0.0.0", 1234);

      auto lso = std::make_shared<CListener>(
        nullptr, nullptr, nullptr, nullptr,
        [this] ()
        {
          auto aso = std::make_shared<CListener>(
            nullptr,
            [this, m = std::string("")] 
            (const uint8_t *b, size_t n) mutable {
              m.append((char *)b, n);
              if (m.size() == sizeof(Context))
              {
                Context *ctx = (Context *) calloc(1, sizeof(Context));
                memmove(ctx, m.data(), sizeof(Context));
                ProcessContext((CSubject *) ctx->k, ctx, 0);
                m.clear();                
              }
            }
          );
          this->iDServer->iConnectedClient->AddEventListener(aso);
        }
      );

      GetDispatcher()->AddEventListener(iDServer)->AddEventListener(lso);

      iDServer->SetName("DC-LS");

      iDServer->StartSocketServer();

      iDClient = std::make_shared<CDeviceSocket>();

      iDClient->SetHostAndPort("127.0.0.1", 1234);

      GetDispatcher()->AddEventListener(iDClient);

      iDClient->SetName("DC-CT");

      iDClient->StartSocketClient();
    }

    virtual const SPCSubject& AddEventListener(const SPCSubject& observer)
    {
      CSubject::AddEventListener(observer);

      const SPCDevice device = std::dynamic_pointer_cast<CDevice>(observer);

      #ifdef linux

      assert(iEventPort != -1);

      if (device->GetDeviceType() == EDeviceType::EDevSock)
      {
        struct epoll_event e;

        e.events = EPOLLIN | EPOLLOUT;

        e.data.ptr = device.get();

        int rc = epoll_ctl(iEventPort, EPOLL_CTL_ADD, device->iFD, &e);

        if (rc == -1)
        {
          std::cout << "epoll_ctl failed, error " << strerror(errno) << "\n";
        }

        assert(rc == 0);
      }

      #endif

      #ifdef WIN32

      assert(iEventPort != INVALID_HANDLE_VALUE);

      HANDLE port = CreateIoCompletionPort(
        device->iFD,
        iEventPort,
        (ULONG_PTR) device.get(),
        0);

      assert(port);

      #endif

      return observer;
    }

  private:

    FD iEventPort;

    std::thread iWorker;

    SPCDeviceSocket iDServer;

    SPCDeviceSocket iDClient;

    void Worker(void)
    {
      while (true)
      {
        Context *ctx = nullptr;

        void *k = nullptr;

        unsigned long n;

        uint32_t e = 0;

        #ifdef linux

          struct epoll_event ee;

          int fRet = epoll_wait(iEventPort, &ee, 1, -1);

          if (fRet < 0 && errno == EINTR)
          {
            continue;
          }

          k = ee.data.ptr;

          e = ee.events;

        #endif

        #ifdef WIN32

          LPOVERLAPPED o;

          bool fRet = GetQueuedCompletionStatus(iEventPort, &n, (PULONG_PTR) &k, &o, INFINITE);

          if (!fRet)
          {
            std::cout << k  << " GQCS failed : " << GetLastError() << "\n";
          }

          if (n == 0 && k == 0 && o == 0)
          {
            break;
          }

          ctx = (Context *) o;

          ctx->n = n;

        #endif

        ProcessContext(k, ctx, e);
      }

      std::cout << "Dispatcher thread returning. Observers : " << iObservers.size() << "\n";
    }

    void ProcessContext(void *k, Context *ctx, uint32_t e)
    {
        std::unique_lock<std::mutex> ul(iLock);

        for (auto o : iObservers)
        {
          if (k == (void *)o.get())
          {
            ul.unlock();

            #ifdef linux

            if ((e & EPOLLOUT) && !o->IsConnected())
            {
              o->OnConnect();
            }
            else if (e & EPOLLIN)
            {
              ctx = (Context *) o->Read();
            }

            if (!ctx)
            {
              break;
            }

            #endif

            //std::cout << NPL::EIOToChar(ctx->type) << " " << o->GetName() << " : " << (void *)k << ", n " << ctx->n << "\n";            

            if (ctx->type == EIOTYPE::READ)
            {
              if (ctx->n != 0)
              {
                o->OnRead(ctx->b, ctx->n);
              }
              else
              {
                o->OnDisconnect();
              }
            }
            else if (ctx->type == EIOTYPE::WRITE)
            {
              o->OnWrite(ctx->b, ctx->n);
            }
            else if (ctx->type == EIOTYPE::CONNECT)
            {
              o->OnConnect();
            }
            else if (ctx->type == EIOTYPE::ACCEPT)
            {
              o->OnAccept();
            }
            else
            {
              assert (false);
            }

            if (ctx->bFree) free((void *)ctx->b);

            free(ctx);

            ul.lock();

            break;
          }
        }

        ProcessMarkRemoveAllListeners();
    }

    virtual void QueuePendingContext(SPCSubject s, void *c) override
    {
      ((Context *)c)->k = s.get();

      iDClient->Write((const uint8_t *)c, sizeof(Context));

      free (c);
    }

    virtual bool IsDispatcher(void) override
    {
      return true;
    }
};

using SPCDispatcher = std::shared_ptr<CDispatcher>;
using UPCDispatcher = std::unique_ptr<CDispatcher>;

NS_END

#endif //DISPATCHER_HPP;