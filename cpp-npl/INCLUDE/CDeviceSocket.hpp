#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <CDevice.hpp>

#include <memory>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

NS_NPL

using TOnHandshake = std::function<void (void)>;

enum class TLS : uint8_t
{
  NO = 0,
  YES,
  Implicit //ftp
};

enum ESocketType : uint8_t
{
  ESocketInvalid = 0,
  EClientSocket,
  EListeningSocket,
  EAcceptedSocket,
};

class CDeviceSocket : public CDevice
{
  using SPCDeviceSocket = std::shared_ptr<CDeviceSocket>;
  using WPCDeviceSocket = std::weak_ptr<CDeviceSocket>;

  public:

    CDeviceSocket()
    {
      iFD = (FD) socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      iDevicetype = EDeviceType::EDevSock;      
    }

    CDeviceSocket(FD aSocket)
    {
      iFD = aSocket;
      iDevicetype = EDeviceType::EDevSock;
    }

    ~CDeviceSocket()
    {
      std::cout << "~CDeviceSocket : " << iName << " ->\n";
      StopSocket();
      shutdown((SOCKET)iFD, 0); //sd_recv
      closesocket((SOCKET)iFD);
      if (ssl) 
      {
        SSL_free(ssl);
      }
      std::cout << "~" << iName << " shutdown(sd_recv)\n";
    }

    virtual void StopSocket(void)
    {
      if (!iStopped)
      {
        if (ssl)
        {
          int flag = SSL_get_shutdown(ssl);

          if (!(flag & SSL_SENT_SHUTDOWN))
          {
            int rc = SSL_shutdown(ssl);
            std::cout << iName << " StopSocket : ssl_shutdown() rc : " << rc << "\n";          
            UpdateWBIO();
          }
        }

        iStopped = true;

        shutdown((SOCKET)iFD, 1); //sd_send

        std::cout << iName << " StopSocket : shutdown(sd_send)\n";
      }
    }

    virtual void StartSocketClient(void)
    {
      assert(iHost.size() && iPort);

      iSocketType = ESocketType::EClientSocket;

      #ifdef linux

        SetSocketBlockingEnabled(iFD, false);

        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;

        inet_pton(AF_INET, iHost.c_str(), &sa.sin_addr);

        sa.sin_port = htons(iPort);

        int rc = connect((SOCKET)iFD, (const sockaddr *) &sa, sizeof(sa));

      #else

        struct sockaddr_in addr;
        ZeroMemory(&addr, sizeof(addr));
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_family = AF_INET;
        addr.sin_port = 0;

        int rc = bind((SOCKET)iFD, (SOCKADDR*) &addr, sizeof(addr));

        assert(rc == 0);

        Context *ctx = (Context *) calloc(1, sizeof(Context));

        ctx->type = EIOTYPE::CONNECT;

        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;

        inet_pton(AF_INET, iHost.c_str(), &sa.sin_addr);

        sa.sin_port = htons(iPort);

        auto ConnectEx = GetExtentionPfn(WSAID_CONNECTEX, iFD);

        bool fRet = ((LPFN_CONNECTEX)ConnectEx)(
          (SOCKET)iFD,
          (SOCKADDR*) &sa,
          sizeof(sa), 
          NULL, 0, NULL,
          (LPOVERLAPPED)ctx);

        if (fRet)
        {
          #ifdef WIN32
          setsockopt((SOCKET)iFD, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0 );
          #endif          
          OnConnect();
        }
        else if (WSAGetLastError() == ERROR_IO_PENDING)
        {

        }
        else
        {
          std::cout << "ConnectEx failed : " << WSAGetLastError() << "\n";
        }

      #endif
    }

    virtual void StartSocketServer(void)
    {
      assert(iPort);

      sockaddr_in sa;
      memset(&sa, 0, sizeof(sa));
      sa.sin_family = AF_INET;

      inet_pton(AF_INET, iHost.c_str(), &sa.sin_addr);
      
      sa.sin_port = htons(iPort);

      int fRet = bind((SOCKET)iFD, (const sockaddr *) &sa, sizeof(sa));

      if (fRet == -1)
      {
        std::cout << "bind failed \n";
      }

      assert (fRet == 0);

      fRet = listen((SOCKET)iFD, SOMAXCONN);

      if (fRet == -1)
      {
        std::cout << "listen failed \n";
      }

      assert(fRet == 0);

      #ifdef linux
      SetSocketBlockingEnabled(iFD, false);
      #endif

      iSocketType = ESocketType::EListeningSocket;

      #ifdef WIN32
      AcceptNewConnection();
      #endif
    }

    #ifdef WIN32
    virtual void AcceptNewConnection(void)
    {
      auto AcceptEx = GetExtentionPfn(WSAID_ACCEPTEX, iFD);

      uint8_t *b = (uint8_t *) calloc(1, 2 * (sizeof(SOCKADDR_STORAGE) + 16) + sizeof(Context));

      ((Context *)b)->type = EIOTYPE::ACCEPT;

      iAS = (FD) socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

      DWORD bytesReceived;

      bool rc = ((LPFN_ACCEPTEX)AcceptEx)(
        (SOCKET)iFD,
        (SOCKET)iAS,
        b + sizeof(Context),
        0,
        sizeof(SOCKADDR_STORAGE) + 16,
        sizeof(SOCKADDR_STORAGE) + 16,
        &bytesReceived,
        (LPOVERLAPPED)b);
    }
    #endif

    virtual bool SetSocketBlockingEnabled(FD sock, bool blocking)
    {
      bool fret = false;

      #ifdef linux
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) return false;
        flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl((SOCKET)sock, F_SETFL, flags) == 0) ? true : false;
      #else
        unsigned long mode = blocking ? 0 : 1;
        return (ioctlsocket((SOCKET)sock, FIONBIO, &mode) == 0) ? true : false;
      #endif
    }

    virtual TLS GetTLS(void)
    {
      return iTLS;
    }

    virtual void SetTLS(TLS tls)
    {
      iTLS = tls;
    }

    virtual bool IsClientSocket(void)
    {
      return (iSocketType == ESocketType::EClientSocket);
    }

    virtual bool IsListeningSocket(void)
    {
      return (iSocketType == ESocketType::EListeningSocket);
    }

    virtual bool IsAcceptedSocket(void)
    {
      return (iSocketType == ESocketType::EAcceptedSocket);
    }

    virtual void SetHostAndPort(const std::string& aHostname, int aPort)
    {
      iHost = aHostname;
      iPort = aPort;
    }

    virtual void CheckPeerSSLShutdown()
    {
      int flag = SSL_get_shutdown(ssl);

      if (flag & SSL_RECEIVED_SHUTDOWN)
      {
        std::cout << iName << " CheckPeerSSLShutdown : SSL_RECEIVED_SHUTDOWN, flags : " << flag << "\n";

        if (!(flag & SSL_SENT_SHUTDOWN))
        {
          StopSocket();
        }
      }
    }

    virtual void UpdateWBIO()
    {
      CheckPeerSSLShutdown();

      int pending = BIO_pending(wbio);

      constexpr int BioRBufSize = 32;

      uint8_t buf[BioRBufSize];

      while (pending)
      {
        int rc = BIO_read(wbio, buf, BioRBufSize);

        CDevice::Write(buf, rc);

        pending = BIO_pending(wbio);
      }
    }

    virtual void InitializeSSL(TOnHandshake cbk = nullptr)
    {
      iOnHandShake = cbk;
      ctx = SSL_CTX_new(TLS_client_method());
      SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
      ssl = SSL_new(ctx);
      rbio = BIO_new(BIO_s_mem());
      wbio = BIO_new(BIO_s_mem());
      SSL_set_bio(ssl, rbio, wbio);

      if (IsClientSocket())
      {
        SSL_set_connect_state(ssl);

        SSL_do_handshake(ssl);

        UpdateWBIO();
      }
      else
      {
        SSL_set_accept_state(ssl);
      }
    }

    virtual void OnAccept(void)
    {
      assert(IsListeningSocket());

      iConnectedClient.reset();

      iConnectedClient = std::make_shared<CDeviceSocket>(iAS);

      iConnectedClient->iSocketType = ESocketType::EAcceptedSocket;

      iConnectedClient->SetName("AS");

      iConnectedClient->iConnected = true;

      auto D = GetDispatcher();

      D->AddEventListener(iConnectedClient);

      CDevice::OnAccept();

      #ifdef WIN32
      iConnectedClient->Read();
      setsockopt((SOCKET)iAS, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&(iFD), sizeof(iFD));
      AcceptNewConnection();
      #endif
    }

    virtual void OnConnect() override
    {
      assert(IsClientSocket());

      CDevice::OnConnect();

      #ifdef WIN32
      CDevice::Read();
      setsockopt((SOCKET)iFD, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0 );
      #endif
    }

    virtual void OnDisconnect() override
    {
      if (ssl)
      {
        assert(SSL_get_shutdown(ssl) & SSL_SENT_SHUTDOWN);
      }

      CDevice::OnDisconnect();
    }

    virtual void OnRead(const uint8_t *b, size_t n) override
    {
      #ifdef WIN32
      CDevice::Read();
      #endif

      size_t _n = n;
      std::string msg;
      const uint8_t *_b = b;

      if (ssl)
      {
        int rc = BIO_write(rbio, b, static_cast<int>(n));

        assert(rc == n);

        if (!iHandshakeDone)
        {
          rc = SSL_do_handshake(ssl);

          if (rc == 1)
          {
            iHandshakeDone = true;

            std::cout << iName << " handshake done\n";

            if (iOnHandShake)
            {
              iOnHandShake();
            }
          }
        }

        if (iHandshakeDone)
        {
          while (true)
          {
            char buf[DEVICE_BUFFER_SIZE];

            rc = SSL_read(ssl, buf, DEVICE_BUFFER_SIZE);

            if (rc > 0) 
            {
              msg.append(buf, rc);
            }
            else
            {
              break;
            }
          }
        }

        UpdateWBIO();

        if (msg.size())
        {
          _b = (uint8_t *)msg.data(), _n = msg.size();
        }
        else
        {
          return;
        }
      }

      CDevice::OnRead(_b, _n);
    }

    virtual void OnWrite(const uint8_t *b, size_t n) override
    {
      if (ssl)
      {
        if (!iHandshakeDone)
        {
          SSL_do_handshake(ssl);
          return;
        }
      }

      CDevice::OnWrite(b, n);
    }

    virtual void * Read(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0) override
    {
      #ifdef linux
      if (IsListeningSocket())
      {
        struct sockaddr_storage ca;
        socklen_t alen = sizeof(struct sockaddr_storage);

        iAS = (FD) accept((SOCKET)iFD, (struct sockaddr *) &ca, &alen);

        if (iAS == -1)
        {
          std::cout << "accept failed, error : " << strerror(errno) << " " << iSocketType << "\n";
          return nullptr;
        }

        SetSocketBlockingEnabled(iAS, false);

        Context *ctx = (Context *) calloc(1, sizeof(Context));

        ctx->type = EIOTYPE::ACCEPT;

        return ctx;
      }
      #endif

      return CDevice::Read(b, l, o);
    }

    virtual void Write(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0) override
    {
      if (ssl)
      {
        SSL_write(ssl, b, static_cast<int>(l));
        UpdateWBIO();
      }
      else
      {
        CDevice::Write(b, l);
      }
    }

    SPCDeviceSocket iConnectedClient = nullptr;
    
  protected:

    int iPort = 0;

    std::string iHost = "";

    uint32_t iSocketType = ESocketType::ESocketInvalid;

    bool iStopped = false;

    TLS iTLS = TLS::NO;

    SSL_CTX *ctx = nullptr;

    SSL *ssl = nullptr;

    BIO *rbio = nullptr;

    BIO *wbio = nullptr;

    bool iHandshakeDone = false;

    TOnHandshake iOnHandShake = nullptr;

    FD iAS;

    #ifdef WIN32
    void * GetExtentionPfn(GUID guid, FD fd)
    {
      void *pfn;
      DWORD n;

      int rc = WSAIoctl(
        (SOCKET)fd,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &pfn, sizeof(pfn),
        &n, NULL, NULL);

      assert(rc == 0);

      return pfn;
    }
    #endif
};

using SPCDeviceSocket = std::shared_ptr<CDeviceSocket>;
using WPCDeviceSocket = std::weak_ptr<CDeviceSocket>;

NS_END

#ifdef WIN32
 WSADATA wsaData;
 int winsockinit = WSAStartup(MAKEWORD(2, 2), &wsaData);
 void * GetExtentionPfn(GUID guid, FD fd); 
#endif 

#endif //SOCKET_HPP