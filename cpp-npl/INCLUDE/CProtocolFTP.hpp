#ifndef PROTOCOLFTP_HPP
#define PROTOCOLFTP_HPP

#include <CProtocol.hpp>
#include <CListener.hpp>

#include <list>
#include <tuple>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <functional>

NS_NPL

enum class DCProt : uint8_t
{
  Clear = 0,
  Protected
};

using TTransferCbk = std::function<bool (const char *, size_t)>;
using TResponseCbk = std::function<void (const std::string&)>;

class CProtocolFTP : public CProtocol<uint8_t, uint8_t>
{
  public:

    CProtocolFTP()
    {
      iCmdInProgress.test_and_set();
    }

    virtual ~CProtocolFTP() {}

    virtual void Upload(TTransferCbk cbk, const std::string& fRemote, const std::string& fLocal, DCProt P = DCProt::Clear)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        if (!fRemote.size() || !fLocal.size()) assert(false);

        SetDCProtLevel(P);

        iCmdQ.emplace_back("PASV", "", "", nullptr, nullptr);

        iCmdQ.emplace_back("STOR", fRemote, fLocal, nullptr, cbk);
      }

      ProcessNextCmd();
    }

    virtual void Download(TTransferCbk cbk, const std::string& fRemote, const std::string& fLocal, DCProt P = DCProt::Clear)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        if (!fRemote.size() || (!cbk && !fLocal.size())) assert(false);

        SetDCProtLevel(P);

        iCmdQ.emplace_back("PASV", "", "", nullptr, nullptr);

        iCmdQ.emplace_back("RETR", fRemote, fLocal, nullptr, cbk);
      }

      ProcessNextCmd();
    }

    virtual void ListDirectory(TTransferCbk cbk, const std::string& fRemote = "", DCProt P = DCProt::Clear)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        if (!cbk) assert(false);

        SetDCProtLevel(P);

        iCmdQ.emplace_back("TYPE", "I", "", nullptr, nullptr);

        iCmdQ.emplace_back("PASV", "", "", nullptr, nullptr);

        iCmdQ.emplace_back("LIST", fRemote, "", nullptr, cbk);
      }

      ProcessNextCmd();        
    }

    virtual void GetCurrentDir(TResponseCbk cbk = nullptr)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        iCmdQ.emplace_back("PWD", "", "", cbk, nullptr);
      }

      ProcessNextCmd();
    }

    virtual void SetCurrentDir(const std::string& dir, TResponseCbk cbk = nullptr)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        iCmdQ.emplace_back("CWD", dir, "", cbk, nullptr);
      }

      ProcessNextCmd();       
    }

    virtual void CreateDir(const std::string& dir, TResponseCbk cbk)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        iCmdQ.emplace_back("MKD", dir, "", cbk, nullptr);
      }

      ProcessNextCmd();    
    }

    virtual void RemoveDir(const std::string& dir, TResponseCbk cbk)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        iCmdQ.emplace_back("RMD", dir, "", cbk, nullptr);
      }

      ProcessNextCmd();  
    }

    virtual void Quit(TResponseCbk cbk = nullptr)
    {
      {
        std::lock_guard<std::mutex> lg(iLock);

        iCmdQ.emplace_back("QUIT", "", "", cbk, nullptr);
      }

      ProcessNextCmd();       
    }

    virtual void Stop(void) override
    {
      std::lock_guard<std::mutex> lg(iLock);
      
      if (iDataChannel)
      {
        std::dynamic_pointer_cast<CDeviceSocket>(iDataChannel)->StopSocket();
        iDataChannel.reset();
      }

      CProtocol::Stop();
    }

  protected:

    DCProt iDCProt = DCProt::Clear;

    bool iContinueTransfer = false;

    uint64_t iCurrentFileOffset = 0;

    SPCSubject iFileDevice = nullptr;

    SPCSubject iDataChannel = nullptr;

    std::atomic_flag iCmdInProgress = ATOMIC_FLAG_INIT;

    std::list<
      std::tuple<
       std::string,      // command
       std::string,      // fRemote
       std::string,      // fLocal
       TResponseCbk,     // rcbk
       TTransferCbk      // ucbk
    >> iCmdQ;

    using TStateFn = std::function<void (void)>;

    struct Transition
    {
      std::string  iTransitionState;
      char         iResponseCode;
      std::string  iNextState;
      TStateFn     iTransitionFn;
    };

    Transition FSM[32] =
    {
      // Connection states
      { "CONNECTED" , '1', "CONNECTED" , nullptr                                        },
      { "CONNECTED" , '2', "CHECK"     , [this](){ CheckExplicitFTPS(); }               },
      { "CONNECTED" , '4', "CONNECTED" , nullptr                                        },
      { "AUTH"      , '2', "TLS"       , [this](){ DoCCHandshake();     }               },
      { "AUTH"      , '3', "ADAT"      , nullptr                                        },
      { "AUTH"      , '4', "USER"      , [this](){ SendCommand("USER", iUserName); }    },
      { "AUTH"      , '5', "USER"      , [this](){ SendCommand("USER", iUserName); }    },
      // USER states
      { "USER"      , '1', "USER"      , [this](){ }                                    },
      { "USER"      , '2', "READY"     , [this](){ }                                    },
      { "USER"      , '3', "PASS"      , [this](){ SendCommand("PASS", iPassword); }    },
      { "USER"      , '4', "USER"      , [this](){ }                                    },
      { "USER"      , '5', "USER"      , [this](){ }                                    },
      // PASS states
      { "PASS"      , '1', "USER"      , [this]() { ProcessLoginEvent(false); }         },
      { "PASS"      , '2', "READY"     , [this]() { ProcessLoginEvent(true); }          },
      { "PASS"      , '3', "ACCT"      , [this]() { SendCommand("ACCT");}               },
      { "PASS"      , '4', "USER"      , [this]() { ProcessLoginEvent(false); }         },
      { "PASS"      , '5', "USER"      , [this]() { ProcessLoginEvent(false); }         },
      // PASV states
      { "PASV"      , '1', "DATA"      , [this]() { SkipCommand(2);        }            },
      { "PASV"      , '2', "DATA"      , [this]() { ProcessPasvResponse(); }            },
      { "PASV"      , '4', "READY"     , [this]() { SkipCommand(2);        }            },
      { "PASV"      , '5', "READY"     , [this]() { SkipCommand(2);        }            },
      // DATA command (LIST, RETR, STOR) states
      { "DATA"      , '1', "1yz"       , [this] () { ProcessDataCmdResponse('1'); }     },
      { "1yz"       , '2', "xyz"       , [this] () { ProcessDataCmdResponse('2'); }     },
      { "1yz"       , '4', "xyz"       , [this] () { ProcessDataCmdResponse('4'); }     },      
      { "DATA"      , '4', "xyz"       , [this] () { ProcessDataCmdResponse('4'); }     },
      { "DATA"      , '5', "xyz"       , [this] () { ProcessDataCmdResponse('5'); }     },
      { "GEN"       , '1', "READY"     , [this] () { ProcessGenCmdEvent();  }           },
      { "GEN"       , '2', "READY"     , [this] () { ProcessGenCmdEvent();  }           },
      { "GEN"       , '3', "READY"     , [this] () { ProcessGenCmdEvent();  }           },
      { "GEN"       , '4', "READY"     , [this] () { ProcessGenCmdEvent();  }           },
      { "GEN"       , '5', "READY"     , [this] () { ProcessGenCmdEvent();  }           }
    };

    virtual void StateMachine(SPCMessage msg) override
    {
      auto l = msg->GetPayloadLength();
      auto b = msg->GetPayloadBuffer();

      for (size_t i = 0; i < l; i++)
      {
        std::cout << b[i];
      }

      std::cout << "\n";

      for (int i = 0; i < sizeof(FSM) / sizeof(FSM[0]); i++)
      {
        Transition t = FSM[i];

        if ((iProtocolState == t.iTransitionState) && (t.iResponseCode ==  b[0]))
        {
          iProtocolState = t.iNextState;

          if (t.iTransitionFn)
          {
            t.iTransitionFn();
          }

          break;
        }
      }      
    }

    virtual SPCMessage IsMessageComplete(const std::vector<uint8_t>& b) override
    {
      size_t l = b.size();

      if (l >= 4)
      {
        if ((b[l-2] == '\r') && (b[l-1] == '\n'))
        {
          uint8_t code[4] = { 0, 0, 0, ' '};

          memmove(code, b.data(), 3);

          for (size_t i = 0; i < l; i++)
          {
            if (0 == memcmp(b.data() + i, code, 4))
            {
              return std::make_shared<CFTPMessage>(b);
            }
          }
        }
      }

      return nullptr;
    }

    virtual void SendCommand(const std::string& c, const std::string& arg = "")
    {
      auto cmd = c + " " + arg + "\r\n";
      std::cout << cmd;
      Write((uint8_t *)cmd.c_str(), cmd.size(), 0);
    }

    virtual void SetDCProtLevel(DCProt P)
    {
      TLS tls = GetChannelTLS(iTarget.lock());

      if (tls == TLS::YES || tls == TLS::Implicit)
      {
        iCmdQ.emplace_back("PBSZ", "0", "", nullptr, nullptr);

        auto level = (P == DCProt::Clear) ? "C" : "P";

        iCmdQ.emplace_back("PROT", level, "", 
          [this, lvl = level](const std::string& res){
            if (res[0] == '2')
            {
              iDCProt = (lvl == "C") ? DCProt::Clear : DCProt::Protected;
            }
          }, nullptr);
      }
    }

    virtual void SkipCommand(int count = 1)
    {
      for (int i = 0; i < count; i++)
      {
        iCmdQ.pop_front();
      }

      iCmdInProgress.clear();

      ProcessNextCmd();
    }

    virtual void ProcessNextCmd(void)
    {
      if (iCmdInProgress.test_and_set() == false)
      {
        if (!iCmdQ.size())
        {
          iCmdInProgress.clear();
          return;
        }

        auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();

        if (IsTransferCommand(cmd))
        {       
          iProtocolState = "DATA";
        }
        else if (cmd == "PASV")
        {
          iProtocolState = "PASV";
        }
        else
        {
          iProtocolState = "GEN";
        }

        SendCommand(cmd, fRemote);
      }
    }

    virtual void CheckExplicitFTPS(void)
    {
      TLS tls = GetChannelTLS(iTarget.lock());

      if (tls == TLS::YES)
      {
        iProtocolState = "AUTH";
        SendCommand("AUTH", "TLS");
      }
      else
      {
        iProtocolState = "USER";
        SendCommand("USER", iUserName);
      }
    }

    virtual void ProcessGenCmdEvent(void)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();

      if (rcbk)
      {
        auto& m = iMessages.back();
        rcbk(m->GetPayloadString());
      }

      SkipCommand(1);
    }

    virtual void ProcessPasvResponse(void)
    {
      auto& m = iMessages.back();

      auto& pasv = m->GetPayloadString();

      auto spec = pasv.substr(pasv.find('('));

      uint32_t h1, h2, h3, h4, p1, p2;

      int fRet = sscanf_s(spec.c_str(), "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);

      if (fRet < 6)
      {
        std::cout << "Faled to parse PASV response\n";
      }

      auto host = std::to_string(h1) + "." +
                  std::to_string(h2) + "." +
                  std::to_string(h3) + "." +
                  std::to_string(h4);

      auto port = (p1 << 8) + p2;

      SkipCommand(1);

      OpenDataChannel(host, port);
    }

    virtual void OpenDataChannel(const std::string& host, int port)
    {
      iDataChannel = std::make_shared<CDeviceSocket>();

      iDataChannel->SetName("ftp-dc");

      auto observer = std::make_shared<CListener>(
        [this]() {
          OnDataChannelConnect();
        },
        [this](const uint8_t *b, size_t n) {
          OnDataChannelRead(b, n);
        },
        [this](const uint8_t *b, size_t n) {
          OnDataChannelWrite(b, n);
        },
        [this](){
          OnDataChannelDisconnect();
        });

      observer->SetName("dc-ob");

      auto D = GetDispatcher();

      D->AddEventListener(iDataChannel)->AddEventListener(observer);

      auto dc = std::dynamic_pointer_cast<CDeviceSocket>(iDataChannel);

      dc->SetHostAndPort(host, port);

      dc->StartSocketClient();
    }

    virtual void ProcessDataCmdResponse(char code)
    {
      if (IsPositivePreliminaryReply(code))
      {
        InitializeFileDevice();

        if (iDCProt == DCProt::Protected)
        {
          std::dynamic_pointer_cast<CDeviceSocket>
            (iDataChannel)->InitializeSSL(
              [this] () {
                TriggerDataTransfer();
              });
        }
        else
        {
          TriggerDataTransfer();
        }
      }
      else if (IsPositiveCompletionReply(code))
      {
        if (!iDataChannel)
        {
          SkipCommand();
        }
      }
      else
      {
        if (iDataChannel)
        {
          ResetSubject(iDataChannel);
        }  

        if (iFileDevice)
        {
          ResetSubject(iFileDevice);
        }

        if (iProtocolState == "xyz")
        {
          SkipCommand();
        }
      }
    }

    virtual void InitializeFileDevice(void)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();      

      if (fLocal.size())
      {
        assert(!iFileDevice);

        if (cmd == "LIST") assert(!fLocal.size());

        iFileDevice = std::make_shared<CDevice>(
          fLocal.c_str(),
          cmd == "RETR" ? true : false);

        iFileDevice->SetName("fl");

        iCurrentFileOffset = 0;

        auto observer = std::make_shared<CListener>(
          nullptr,
          [this] (const uint8_t *b, size_t n) {
            OnFileRead(b, n);
          },
          [this] (const uint8_t *b, size_t n) {
            OnFileWrite(b, n);
          },
          [this] () {
            OnFileDisconnect();
          }
        );

        observer->SetName("fl-ob");

        auto D = GetDispatcher();

        D->AddEventListener(iFileDevice)->AddEventListener(observer);
      }

      iContinueTransfer = true;
    }

    virtual void OnDataChannelConnect(void)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();
    }

    virtual void OnDataChannelRead(const uint8_t *b, size_t n)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();

      if (tcbk)
      {
        if (iContinueTransfer)
        {
          iContinueTransfer = tcbk((const char *)b, n);
        }
        else
        {
          std::dynamic_pointer_cast<CDeviceSocket>(iDataChannel)->StopSocket();
          return;
        }
      }

      if (iFileDevice)
      {
        iFileDevice->Write(b, n, iCurrentFileOffset);
        iCurrentFileOffset += n;
      }
    }

    virtual void OnDataChannelWrite(const uint8_t *b, size_t n)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();
    }

    virtual void OnDataChannelDisconnect(void)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();

      if (tcbk)
      {
        tcbk(nullptr, 0);
      }

      ProcessDataCmdResponse('0');
    }

    virtual void OnFileRead(const uint8_t *b, size_t n)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();

      if (tcbk)
      {
        if (iContinueTransfer)
        {
          iContinueTransfer = tcbk((const char *)b, n);
        }
        else
        {
          std::dynamic_pointer_cast<CDeviceSocket>(iDataChannel)->StopSocket();
          return;
        }
      }

      iDataChannel->Write(b, n);

      iCurrentFileOffset += n;

      auto ctx = iFileDevice->Read(nullptr, 0, iCurrentFileOffset);

      #ifdef linux
      QueuePendingContext(iFileDevice, ctx);
      #endif
    }

    virtual void OnFileWrite(const uint8_t *b, size_t n)
    {
    }

    virtual void OnFileDisconnect(void)
    {
      std::dynamic_pointer_cast<CDeviceSocket>(iDataChannel)->StopSocket();
    }

    virtual void TriggerDataTransfer(void)
    {
      auto& [cmd, fRemote, fLocal, rcbk, tcbk] = iCmdQ.front();

      if (cmd == "STOR")
      {
        auto ctx = iFileDevice->Read(nullptr, 0, iCurrentFileOffset);
        #ifdef linux
        QueuePendingContext(iFileDevice, ctx);
        #endif        
      }
    }

    virtual void ProcessLoginEvent(bool status)
    {
      if (status)
      {
        iCmdInProgress.clear();
        ProcessNextCmd();
      }
      else
      {
        iCmdQ.clear();
        iCmdInProgress.clear();
      }      
    }

    virtual bool IsTransferCommand(const std::string& cmd)
    {
      return (cmd == "RETR" || 
              cmd == "LIST" ||
              cmd == "STOR");
    }

    virtual bool IsPositiveCompletionReply(char c)
    {
      return (c == '2');
    }

    virtual bool IsPositivePreliminaryReply(char c)
    {
      return (c == '1');
    }

    virtual void OnConnect(void) override
    {
      CProtocol::OnConnect();

      TLS tls = GetChannelTLS(iTarget.lock());

      if (tls == TLS::Implicit)
      {
        DoCCHandshake();
      }
    }

    virtual void DoCCHandshake()
    {
      auto sock = GetTargetSocketDevice();

      if (sock)
      {
        sock->InitializeSSL([this] () {
          TLS tls = GetChannelTLS(iTarget.lock()); 
          if (tls == TLS::YES)
          {
            iProtocolState = "USER";
            SendCommand("USER", iUserName);
          }
        });
      }
    }
};

using SPCProtocolFTP = std::shared_ptr<CProtocolFTP>;

NS_END

#endif //PROTOCOLFTP_HPP
