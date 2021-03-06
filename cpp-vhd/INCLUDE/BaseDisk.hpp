#ifndef BASEDISK_HPP
#define BASEDISK_HPP

#include <npl.hpp>

#include <formats.hpp>

#include <tuple>
#include <vector>
#include <string>
#include <memory>
#include <ctime>
#include <filesystem>
#include <assert.h>

struct DataBlockIO
{
  const uint8_t *buffer = nullptr;
  uint64_t offset = 0;
  size_t length = 0;
};

class CBaseDisk : public NPL::CSubject<uint8_t, uint8_t>
{
  public:

    MBR iMBR = { 0 };

    std::filesystem::path iPath;

    NPL::SPCSubject<uint8_t, uint8_t> iFile = nullptr;

    std::unique_ptr<uint8_t []> iPayloadBlock = nullptr;

  public:

    using SPCBaseDisk = std::shared_ptr<CBaseDisk>;

    virtual ~CBaseDisk()
    {
      if (iFile)
      {
        iFile->GetDispatcher()->RemoveEventListener(iFile);
      }
    }

    CBaseDisk() {}

    CBaseDisk(uint64_t size, uint32_t blocksize)
    {
      if (size < _2T)
      {
        InitializeMBR(size, blocksize);
      }
      else
      {
        InitializeGPT(size, blocksize);
      }
    }

    CBaseDisk(const std::string& path, uint32_t len)
    {
      iFile = NPL::make_file(path);
      iPath = std::wstring(path.begin(), path.end());
      iRawSectors = std::make_unique<uint8_t []>(len);
      iFile->ReadSync(iRawSectors.get(), len, 0);
    }

    virtual bool IsDifferencing() = 0;

    virtual uint32_t GetBlockSize() = 0;

    virtual uint64_t GetLogicalDiskLength() = 0;

    virtual std::vector<std::wstring> GetParentLocators(void) = 0;

    virtual SPCBaseDisk GetBaseParent(void)
    {
      auto base = std::dynamic_pointer_cast<CBaseDisk>(shared_from_this());

      while (base->iParent)
      {
        base = base->iParent;
      }

      return base;
    }

    virtual uint64_t GetPartitionStartOffset(int n)
    {
      return iMBR.partitions[n].start_sector * 512;
    }

    virtual uint64_t GetPartitionLength(int n)
    {
      return iMBR.partitions[n].total_sectors * 512ULL;
    }

    virtual int32_t ReadSync(const uint8_t *b, size_t l, uint64_t o) override
    {
      uint64_t delta = 0;
      size_t nBytesRead = 0;

      auto fragments = LogicalToDataBlock(o, l, GetBlockSize());

      memset((void *)b, 0 ,l );

      for (auto& f : fragments)
      {
        f.buffer = b + delta;

        auto level = GetBaseParent();

        while (level)
        {
          auto fRet = level->DataBlockRead(f);

          assert(fRet == f.length);

          level = level->iChild;
        }

        nBytesRead += f.length;

        delta += f.length;
      }

      return static_cast<int32_t>(nBytesRead);
    }

    virtual int32_t WriteSync(const uint8_t *b, size_t l, uint64_t o) override
    {
      uint64_t delta = 0;
      size_t nBytesWritten = 0;

      auto fragments = LogicalToDataBlock(o, l, GetBlockSize());

      for (auto& f : fragments)
      {
        f.buffer = b + delta;

        auto fRet = this->DataBlockWrite(f);

        assert(fRet == f.length);

        nBytesWritten += f.length;

        delta += f.length;
      }

      return static_cast<int32_t>(nBytesWritten);
    }

    static std::vector<DataBlockIO> LogicalToDataBlock(uint64_t off, size_t len, uint32_t bs)
    {
      size_t done = 0;
      size_t pending = len;

      uint64_t chunk_off = 0;
      size_t chunk_len = 0;

      std::vector<DataBlockIO> fragments;

      while (done != len)
      {
        chunk_off = (off + done) % bs;
        chunk_len = (chunk_off + pending > bs) ? (bs - chunk_off) : pending;
        fragments.push_back({nullptr, off + done, chunk_len});
        done += chunk_len;
        pending = len - done;
      }

      return fragments;
    }

    virtual void Initialize(void)
    {
      if(iFile)
      {
        iFile->AddEventListener(shared_from_this());
        ReadSync((uint8_t *) &iMBR, sizeof(MBR), 0);
      }
    }

    virtual void Dump(void)
    {
      std::cout << "\n";
      std::cout << "Master Boot Record : \n\n";
      
      std::cout << " DiskID                       : ";
      
      DumpBytes(iMBR.DiskID, 4, true);

      for (int i = 0; i < 4; i++)
      {
        std::cout << " start_sector                 : " << iMBR.partitions[i].start_sector << " sector \n";
        std::cout << " total_sectors                : " << iMBR.partitions[i].total_sectors << " sector \n";
        std::cout << " ------------------" << "\n";
      }

      std::cout << " signature                    : " << iMBR.signature << "\n"; 
    }
    
  protected:

    SPCBaseDisk iParent = nullptr;

    SPCBaseDisk iChild = nullptr;

    std::unique_ptr<uint8_t []> iRawSectors = nullptr;

    virtual size_t DataBlockRead(DataBlockIO& bio) = 0;

    virtual size_t DataBlockWrite(DataBlockIO& bio) = 0;

    void InitializeMBR(uint64_t size, uint32_t blocksize)
    {
      /* signature */
      iMBR.signature = 0xAA55;

      /* diskid = current epoch */
      uint32_t epoch = static_cast<uint32_t>(std::time(nullptr));
      memmove(&(iMBR.DiskID), &epoch, 4);

       /* NTFS */
      iMBR.partitions[0].type = 0x07;

      /* blocksize alignment */
      iMBR.partitions[0].start_sector = blocksize / 512;

      /* total sectors in the partition(volume) */
      iMBR.partitions[0].total_sectors = static_cast<uint32_t>(size / 512);
    }

    void InitializeGPT(uint64_t size, uint32_t blocksize)
    {
      //todo
    }
};

using SPCBaseDisk = std::shared_ptr<CBaseDisk>;

#endif