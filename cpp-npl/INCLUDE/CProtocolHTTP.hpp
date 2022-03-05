#ifndef PROTOCOLHTTP_HPP
#define PROTOCOLHTTP_HPP

#include <CProtocol.hpp>

NS_NPL

class CProtocolHTTP : public CProtocol<uint8_t, uint8_t>
{
  public:

    void Post(const std::string& url, const std::string& body)
    {
      std::stringstream req;

      req << "POST " << url << " HTTP/1.1\r\n";
      req << "Host: 127.0.0.1\r\n";
      req << "Content-type: text/plain\r\n";
      req << "Content-length: " << body.size() << "\r\n";      
      req << "\r\n";
      req << body;

      Write((uint8_t *) req.str().c_str(), req.str().size(), 0);
    }

  protected:

    virtual void StateMachine(SPCMessage m) override
    {
      return;
    }

    virtual SPCMessage IsMessageComplete(const std::vector<uint8_t>& b) override
    {
      bool firstLineReceived = false;
      bool headersReceived = false;
      bool bodyReceived = false;
      int bodyLength = 0;

      std::string m((char *) b.data(), b.size());

      if (m.find("\r\n") != std::string::npos)
      {
        firstLineReceived = true;

        size_t endofHeaders = m.find("\r\n\r\n");

        if (endofHeaders != std::string::npos)
        {
          headersReceived = true;

          size_t pos = m.find("Content-Length");

          if (pos != std::string::npos)
          {
            pos += strlen("Content-Length: ");

            bodyLength = std::stoi(
              std::string(
                m, 
                pos, 
                m.find("\r\n", pos) - pos - 1));

            if (bodyLength)
            {
              size_t total = endofHeaders + strlen("\r\n\r\n") + bodyLength;

              if (m.size() == total)
              {
                bodyReceived = true;
              }
            }
          }
        }
      }

      if (firstLineReceived && 
          headersReceived && 
          (bodyLength ? bodyReceived : true))
      {
        return std::make_shared<CHTTPMessage>(b);
      }

      return nullptr;
    }

};

using SPCProtocolHTTP = std::shared_ptr<CProtocolHTTP>;

NS_END

#endif //PROTOCOLHTTP_HPP