#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <Common.hpp>
#include <CSubject.hpp>

#include <memory>
#include <iostream>
#include <assert.h>
#include <inttypes.h>

#ifdef linux
#include <string.h>
#endif

NS_NPL

enum EDeviceType : uint8_t
{
  EDevNone = 0,
  EDevFile,
  EDevSock
};

enum EIOTYPE : uint8_t
{
  CONNECT = 0,
  ACCEPT,
  READ,
  WRITE,
  IOCTL
};

struct Context
{
  #ifdef linux

  #endif
  #ifdef WIN32
    OVERLAPPED      ol;
  #endif
    EIOTYPE         type;
    void          * k;
    const uint8_t * b;
    unsigned long   n;
    bool            bFree;
};

constexpr uint32_t DEVICE_BUFFER_SIZE = 256;

class CDevice : public CSubject<uint8_t, uint8_t>
{
  public:

    FD iFD;

    FD iFDsync;

    CDevice() = default;

    CDevice(const std::string& aFilename, bool bCreateNew)
    {
      #ifdef linux

      int flags = 0|O_RDWR ;

      if (bCreateNew)
      {
        flags |= O_CREAT;
      }

      iFD = open(aFilename.c_str(), flags);
      
      if (iFD >= 0)
      {
        iConnected = true;
      }
      else
      {
        std::cout << "CDevice() " << aFilename << ", Error : " << strerror(errno) << "\n";
      }

      #endif

      #ifdef WIN32

      iConnected = true;

      iFD = CreateFileA(
        aFilename.c_str(),
        GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        (bCreateNew ? CREATE_ALWAYS : OPEN_EXISTING),
        FILE_FLAG_OVERLAPPED|FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);

      if (iFD == INVALID_HANDLE_VALUE)
      {
        iConnected = false;
        std::cout << "CDevice() " << aFilename << ", Error : " << GetLastError() << "\n";
      }

      iFDsync = CreateFileA(
              aFilename.c_str(),
              GENERIC_READ|GENERIC_WRITE,
              FILE_SHARE_READ|FILE_SHARE_WRITE,
              NULL,
              OPEN_EXISTING,
              FILE_ATTRIBUTE_NORMAL,
              NULL);

      if (iFDsync == INVALID_HANDLE_VALUE)
      {
        iConnected = false;
        std::cout << "CDevice() " << aFilename << ", sync handle Error : " << GetLastError() << "\n";
      }

      #endif 

      iDevicetype = EDeviceType::EDevFile;
    }

    virtual ~CDevice()
    {
      if (iDevicetype == EDeviceType::EDevFile)
      {
        CloseHandle(iFD);
        CloseHandle(iFDsync);
      }
    };

    virtual EDeviceType GetDeviceType(void)
    {
      return iDevicetype;
    }

    virtual void * Read(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0) override
    {
      if (!iConnected)
      {
        std::cout << iName << " CDevice::Read() not connected\n";
        return nullptr;
      }

      Context *ctx = (Context *) calloc(1, sizeof(Context));

      ctx->type = EIOTYPE::READ;

      if (b)
      {
        ctx->b = b;
        ctx->bFree = false;
      }
      else
      {
        ctx->b = (uint8_t *) calloc(1, DEVICE_BUFFER_SIZE);
        ctx->bFree = true;
        l = DEVICE_BUFFER_SIZE;
      }

      #ifdef linux

      ctx->n = read(iFD, (void *) ctx->b, l);

      if ((int)ctx->n == -1)
      {
        std::cout << iName << " read() failed, error : " << strerror(errno) << "\n";

        if (!b)
        {
          free ((void *)ctx->b);
        }

        free (ctx);

        return nullptr;
      }
      else
      {
        return (void *) ctx;
      }

      #else

      (ctx->ol).Offset = o & 0x00000000FFFFFFFF;
      (ctx->ol).OffsetHigh = (o & 0xFFFFFFFF00000000) >> 32;

      BOOL fRet = ReadFile(iFD, (LPVOID) ctx->b, static_cast<DWORD>(l), NULL, &ctx->ol);

      if (!fRet && GetLastError() != ERROR_IO_PENDING)
      {
        std::cout << iName << " ReadFile failed : " << GetLastError() << "\n";
      }

      return nullptr;

      #endif
    }

    virtual void Write(const uint8_t *b = nullptr , size_t l = 0, uint64_t o = 0) override
    {
      if (!iConnected)
      {
        std::cout << iName << " CDevice::Write() not connected\n";
        return;
      }

      assert(b && l);

      #ifdef linux

      int rc = write(iFD, b, l);

      if (rc == -1)
      {
        std::cout << iName << "CDevice::Write() failed, error : " << strerror(errno) << "\n";
        assert(false);
      }

      #else

      Context *ctx = (Context *) calloc(1, sizeof(Context));

      ctx->type = EIOTYPE::WRITE;

      ctx->b = (uint8_t *) calloc(l, 1);

      memmove((void *)ctx->b, b, l);

      ctx->bFree = true;

      (ctx->ol).Offset = o & 0x00000000FFFFFFFF;
      (ctx->ol).OffsetHigh = (o & 0xFFFFFFFF00000000) >> 32;

      BOOL fRet = WriteFile(iFD, (LPVOID) ctx->b, static_cast<DWORD>(l), NULL, &ctx->ol);

      if (!fRet && GetLastError() != ERROR_IO_PENDING)
      {
        std::cout << iName << " WriteFile() failed : " << GetLastError() << "\n";
      }

      #endif
    }

    virtual int32_t ReadSync(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0) override
    {
      DWORD nBytesRead;
      LARGE_INTEGER offset;
      offset.QuadPart = o;

      BOOL fRet = SetFilePointerEx(
                    iFDsync,
                    offset,
                    NULL,
                    FILE_BEGIN);

      if (fRet)
      {
        fRet = ReadFile(iFDsync, (LPVOID) b, static_cast<DWORD>(l), &nBytesRead, NULL);

        if (fRet == FALSE)
        {
          std::cout << iName << " ReadSync ReadFile failed : " << GetLastError() << "\n";
        }
        else
        {
          return nBytesRead;
        }
      }

      return -1;
    }

    virtual int32_t WriteSync(const uint8_t *b = nullptr, size_t l = 0, uint64_t o = 0) override
    {
      DWORD nBytesWritten;
      LARGE_INTEGER offset;
      offset.QuadPart = o;

      BOOL fRet = SetFilePointerEx(
                    iFDsync,
                    offset,
                    NULL,
                    FILE_BEGIN);

      if (fRet)
      {
        fRet = WriteFile(iFDsync, (LPVOID) b, static_cast<DWORD>(l), &nBytesWritten, NULL);

        if (fRet == FALSE)
        {
          std::cout << iName << " WriteSync WriteFile failed : " << GetLastError() << "\n";
        }
        else
        {
          return nBytesWritten;
        }
      }

      return -1;
    }

  protected:

    EDeviceType iDevicetype = EDeviceType::EDevNone;
};

const char EIOToChar(EIOTYPE t)
{
  switch(t)
  {
    case EIOTYPE::ACCEPT:
     return 'A';
     break;
    case EIOTYPE::CONNECT:
     return 'C';
     break;
    case EIOTYPE::READ:
     return 'R';    
     break;
    case EIOTYPE::WRITE:
     return 'W';
     break;
    default:
     return 'Z';
  }
}

using SPCDevice = std::shared_ptr<CDevice>;
using WPCDevice = std::weak_ptr<CDevice>;

NS_END

#endif //DEVICE_HPP