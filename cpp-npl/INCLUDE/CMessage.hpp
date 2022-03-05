#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Util.hpp>

#include <map>
#include <string>
#include <vector>
#include <sstream>

NS_NPL

class CMessage
{
  public:
  
    CMessage(const std::string& m)
    {
      iMessage = m;
    }

    CMessage(const uint8_t *b, size_t l)
    {
      iMessage = std::string((char *)b, l);
    }

    CMessage(const std::vector<uint8_t>& m)
    {
      iMessage = std::string((char *) m.data(), m.size());
    }

    virtual size_t GetPayloadLength(void)
    {
      return iMessage.size();
    }

    virtual const std::string& GetPayloadString(void)
    {
      return iMessage;
    }

    virtual const char * GetPayloadBuffer(void)
    {
      return iMessage.c_str();
    }

  protected:

    std::string iMessage;

    virtual void ParseMessage(void) { }
};

class CFTPMessage : public CMessage
{
  public:

    CFTPMessage(const std::vector<uint8_t>& m) : CMessage(m)
    {
    }  
};

class CHTTPMessage : public CMessage
{
  public:

    CHTTPMessage(const std::vector<uint8_t>& m) : CMessage(m)
    {
      ParseMessage();
    }

    virtual std::string GetHeader(const std::string& key)
    {
      return iHeaders[key];
    }

    virtual void SetHeader(const std::string& key, const std::string& value)
    {
      iHeaders[key] = value;
    }

    virtual size_t GetPayloadLength(void) override
    {
      auto h = GetHeader("Content-Length");

      if (h.size())
      {
        return std::stoi(h);
      }

      return 0;
    }

    virtual const std::string& GetPayloadString(void)
    {
      //do not use
      assert(false); 
      return "";
    }    

    virtual const char * GetPayloadBuffer(void) override
    {
      if (GetHeader("Content-Length").size())
      {
        return iMessage.c_str() + 
               iMessage.find("\r\n\r\n") + 
               strlen("\r\n\r\n");
      }

      return nullptr;
    }

  protected:

    std::map<std::string, std::string> iHeaders;

    virtual void ParseMessage() override
    {
      std::istringstream ss(iMessage);

      std::string line;

      while (std::getline(ss, line, '\n'))
      {
        line.pop_back();

        size_t index = line.find(": ");

        if (index != std::string::npos)
        {
          std::string key, value;

          key = line.substr(0, index);

          value = line.substr(index + 2);

          SetHeader(key, value);
        }
      }
    }
};

class CWSMessage : public CMessage
{
  public:

    CWSMessage(const uint8_t *b, size_t l) : CMessage(b, l)
    {
      assert(l >= 2);

      size_t payloadLength = 0;
      size_t maskingKeyIndex = 0;

      unsigned char indicator = b[1] & 0x7F;

      if (indicator <= 125)
      { /*
         * if 0-125, that is the payload length
         */
        payloadLength = indicator;
        maskingKeyIndex = 2; /** third byte */
      }
      else if (indicator == 126 && l >= (2 + 2))
      { /*
         * If 126, the following 2 bytes interpreted as a 
         * 16-bit unsigned integer are the payload length
         */
        payloadLength = BTOL(b + 2, 2);
        maskingKeyIndex = 4;
      }
      else if (indicator == 127 && l >= (2 + 8))
      { /*
         * If 127, the following 8 bytes interpreted as a 
         * 64-bit unsigned integer (the most significant bit 
         * MUST be 0) are the payload length
         */
        payloadLength = BTOL(b + 2, 8);
        maskingKeyIndex = 10;
      }
      else
      {
        return;
      }

      unsigned char maskingKey[4];

      if (IsMasked() && (l >= (maskingKeyIndex + 4)))
      {
        for (int i = 0; i < 4; i++)
        {
          maskingKey[i] = b[maskingKeyIndex + i];
        }
      }

      size_t payloadIndex = maskingKeyIndex + (IsMasked() ? 4 : 0);

      if ((payloadIndex + payloadLength) == l)
      {
        for (int i = 0; i < payloadLength; i++)
        {
          iPayload += b[payloadIndex + i] ^ maskingKey[(i % 4)];
        }
      }      
    }

    uint8_t GetOpCode(void)
    {
      return (iMessage[0] & 0x0F);
    }

    bool IsControlFrame(void)
    {
      return (GetOpCode() & 0x08);
    }

    bool IsMasked(void)
    {
      return (iMessage[1] & 0x80);
    }

    virtual size_t GetPayloadLength(void) override
    {
      return iPayload.length();
    }

    virtual const std::string& GetPayloadString(void) override
    {
      return iPayload;
    }

  protected:

    std::string iPayload;

    virtual void ParseMessage() override
    {

    }
}; 

using SPCMessage = std::shared_ptr<CMessage>;
using SPCWSMessage = std::shared_ptr<CWSMessage>;
using SPCHTTPMessage = std::shared_ptr<CHTTPMessage>;

NS_END

#endif //MESSAGE_HPP