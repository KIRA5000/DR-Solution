#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <CMessage.hpp>
#include <CSubject.hpp>

#include <functional>

NS_NPL

template <typename T1 = uint8_t, typename T2 = uint8_t>
class CProtocol : public CSubject<T1, T2>
{
  public:

    using SPCProtocol = std::shared_ptr<CProtocol<T1, T2>>;

    using TOnConnectCbk = std::function<void (SPCProtocol)>;

    using TOnClientMessageCbk = std::function<void (SPCProtocol, const std::string&)>;

    CProtocol() = default;

    virtual ~CProtocol() {}

    virtual void StartClient(TOnConnectCbk cbk = nullptr)
    {
      iConnectCbk = cbk;

      auto sock = GetTargetSocketDevice();

      if (sock)
      {
        sock->StartSocketClient();
      }
    }

    virtual void StartServer(void)
    {
      auto sock = GetTargetSocketDevice();

      if (sock)
      {
        sock->StartSocketServer();
      }
    }

    virtual void Stop(void)
    {
      auto sock = GetTargetSocketDevice();

      if (sock)
      {
        sock->StopSocket();
      }      
    }

    virtual size_t GetMessageCount(void)
    {
      return iMessages.size();
    }

    virtual void SetCredentials(const std::string& user, const std::string& pass)
    {
      iUserName = user;
      iPassword = pass;
    }

    virtual void SetClientCallback(TOnClientMessageCbk cbk)
    {
      iClientMessageCallback = cbk;
    }

    virtual void SendProtocolMessage(const uint8_t *message, size_t len)
    {
      return;
    }

    virtual void OnConnect(void) override
    {
      if (iConnectCbk)
      {
        iConnectCbk(std::dynamic_pointer_cast<CProtocol<T1, T2>>(this->shared_from_this()));
      }

      iProtocolState = "CONNECTED";
    }

    virtual void OnDisconnect(void) override
    {
      iProtocolState = "DISCONNECTED";
    }

    virtual void OnRead(const T1 *b, size_t n) override
    {
      for (size_t i = 0; i < n; i++)
      {
        iBuffer.push_back(b[i]);

        auto message = IsMessageComplete(iBuffer);

        if (message)
        {
          iMessages.push_back(message);
          StateMachine(message);
          CSubject<T1, T2>::OnRead(
            iBuffer.data(),
            iBuffer.size());
          iBuffer.clear();
        }
      }
    }

    virtual TLS GetChannelTLS(SPCSubject channel)
    {
      auto sock = std::dynamic_pointer_cast<CDeviceSocket>(channel);

      if (sock)
      {
        return sock->GetTLS();
      }

      return TLS::NO;
    }

  protected:

    virtual SPCMessage IsMessageComplete(const std::vector<T1>& b) = 0;

    virtual void StateMachine(SPCMessage message)
    {
    }

    virtual SPCDeviceSocket GetTargetSocketDevice(void)
    {
      auto target = (this->iTarget).lock();

      if (target)
      {
        return std::dynamic_pointer_cast<CDeviceSocket>(target);
      }

      return nullptr;
    }

    std::string iUserName;

    std::string iPassword;

    std::vector<T1> iBuffer;

    std::vector<SPCMessage> iMessages;

    TOnConnectCbk iConnectCbk = nullptr;

    TOnClientMessageCbk iClientMessageCallback = nullptr;

    std::string iProtocolState = "CONNECTING";
};

using SPCProtocol = std::shared_ptr<CProtocol<uint8_t, uint8_t>>;

using TOnClientMessageCbk = 
  std::function<
    void (SPCProtocol, const std::string&)
  >;

NS_END

#endif //PROTOCOL_HPP
