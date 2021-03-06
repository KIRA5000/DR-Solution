#ifndef DYNVHD_HPP
#define DYNVHD_HPP

#include <BaseDisk.hpp>
#include <formats.hpp>

#include <osl.hpp>

#include <string>

class CDynamicVHD : public CBaseDisk
{
  public:

    uint64_t iBATSize = 0;

    uint64_t iBlockCount = 0;

    VHD_DISK_FOOTER iFooter = { 0 };

    VHD_SPARSE_HEADER iHeader = { 0 };

    std::unique_ptr<uint32_t []> iBAT = nullptr;

  public:

    virtual ~CDynamicVHD() {}

    // new base VHD contstructor
    CDynamicVHD(uint64_t size, uint32_t blocksize) : CBaseDisk(size, blocksize)
    {
      InitializeFooter(blocksize + size, false);
      InitializeSparseHeader(blocksize, nullptr);
      InitializeBAT();
      iPayloadBlock = std::make_unique<uint8_t []>(512 + GetBlockSize());
    }

    // new child VHD contstructor
    CDynamicVHD(CDynamicVHD *parent)
    {
      auto blocksize = parent->GetBlockSize();
      InitializeFooter(parent->GetLogicalDiskLength(), true);
      InitializeSparseHeader(blocksize, parent);
      InitializeBAT();
      iPayloadBlock = std::make_unique<uint8_t []>(512 + GetBlockSize());
    }
 
    // Existing VHD constructor
    CDynamicVHD(const std::string& path) : CBaseDisk(path, 512 * 3)
    {
      memmove(&iFooter, iRawSectors.get(), 512);
      memmove(&iHeader, iRawSectors.get() + 512, 1024);
      InitializeBAT();
      iFile->ReadSync(
        (uint8_t *) iBAT.get(), 
        iBATSize, 
        BTOL64((uint8_t *) &(iHeader.TableOffset))
      );
    }

    virtual uint32_t GetBlockSize() override
    {
      return BTOL32((uint8_t *) &(iHeader.BlockSize));
    }

    virtual uint64_t GetTotalBATEntries(void)
    {
      return BTOL32((uint8_t *) &(iHeader.MaxTableEntries));
    }

    virtual uint64_t GetLogicalDiskLength() override
    {
      return BTOL((uint8_t *) &(iFooter.CurrentSize), 8);
    }

    virtual bool IsDifferencing() override
    {
      return ((BTOL32((uint8_t *) &(iFooter.DiskType)) == 4) ? true : false);
    }

    virtual std::vector<std::wstring> GetParentLocators(void) override
    {
      std::vector<std::wstring> out;

      for (int i = 0; i < 8; i++)
      {
        auto off = BTOL64((uint8_t *) &iHeader.ParentLocatorTable[i].PlatformDataOffset);
        auto len = BTOL32((uint8_t *) &iHeader.ParentLocatorTable[i].PlatformDataSpace);

        auto _buf = std::make_unique<uint8_t []>(len);

        if (off && len)
        {
          iFile->ReadSync(_buf.get(), len, off);
          out.push_back((wchar_t *) _buf.get());
        }
      }

      return out;
    }

    virtual uint64_t FirstDataBlockOffset()
    {
      return sizeof(VHD_FOOTER_HEADER) + (IsDifferencing() ? (2 * PLDataSpaceSize) : 0) + iBATSize;
    }

    /*
          |
       ------------------------------------
      |   |     |_____reqLen____|          |
      |   |     |               |          |
       ------------------------------------
          |     ^
             blkOffset

    bit_off_in_bmp represents the bit offset inside the sector bitmap

   ------bit_off_in_bmp------|
     . . . . . . . . . . . . . . . . . . . . . . . . . . . .
         |             |     ^         |           ^   |
                         first sec bit            last sec bit
    */

    static bool SetSectorBitmap(unsigned char *bitmap, uint64_t blockoff, uint64_t blocklen, uint64_t bs)
    {
      uint64_t bmb_index;
      uint64_t bit_off_in_bmp;
      uint64_t len_in_sectors = blocklen / 512;

      //we should have an offset inside the block
      assert(blockoff <= bs);

      //requested window must fall inside the block
      assert((blockoff + blocklen) <= bs);

      //requested length must be less than the block
      assert(blocklen <= bs);
      //
      // | s  | s  | s  |
      //              ^
      bit_off_in_bmp = blockoff / 512;

      bmb_index = bit_off_in_bmp / 8;
      /*
       * bmb_index is the starting byte inside the bitmap which hold the sector usage for the requested
       * starting sector. This loop runs number of sector times which would take to cover the requested
       * length. j varies from 0-7 do this here as once j is aligned on 8 bits we need to retain the bit
       * were the run breaks.
       */
      int j = (bit_off_in_bmp % 8);

      uint64_t i = 0;

      for ( ; i < len_in_sectors; )
      {
        /*
         * j = 0 ---> j < 8
         *  _ _ _ _ _ _ _ _
         *  7 6 5 4 3 2 1 0
         *            |
         *  0 0 0 0 0 0 0 1
         *  - - - - - - - - 0x01
         */
        for (; j < 8 && i < len_in_sectors; j++, i++)
        {
          bitmap[bmb_index] |= 1 << (7 - j);
        }
        /*
         * at this point, we have updated a single and
         * potentially partial byte of the sector bitmap.
         */
        if (j && ((j % 8) == 0)) 
        {
          //reached the end of a single sector bitmap byte
          bmb_index++;
          j = 0;
        }
      }

      //we parsed all sectors in the requested length
      assert(i == len_in_sectors);

      // for (int b = 0; b < 512; b++) 
      // {
      //   logit(L"%2X ", bitmap[b]);
      //   if (!((b+1)%24))
      // }

      return true;
    }

    virtual void Dump(void) override
    {
      std::cout << "\n";
      std::cout << "VHD Footer : \n\n";

      std::cout << " Cookie                       : " << std::string((char *)iFooter.Cookie, 8) << "\n";
      std::cout << " DataOffset                   : " << BTOL64((uint8_t *) &(iFooter.DataOffset)) << "\n";   
      std::cout << " Original Size                : " << BTOL64((uint8_t *) &(iFooter.OriginalSize)) << "\n";
      std::cout << " Current Size                 : " << BTOL64((uint8_t *) &(iFooter.CurrentSize)) << "\n";
      std::cout << " Creator Application          : " << std::string((char *)iFooter.CreatorApplication, 4) << "\n";

      std::cout << "\n";
      std::cout << "VHD Sparse Header : \n\n";
      
      std::cout << " Cookie                       : " << std::string((char *)iHeader.Cookie, 8) << "\n";
      std::cout << " DataOffset                   : " << BTOL64((uint8_t *) &(iHeader.DataOffset)) << "\n";
      std::cout << " Table Offset                 : " << BTOL64((uint8_t *) &(iHeader.TableOffset)) << "\n";
      std::cout << " Block Size                   : " << BTOL32((uint8_t *) &(iHeader.BlockSize)) << "\n";

      std::cout << " Parent UniqueId              : ";
      DumpBytes(iHeader.ParentUniqueId, 16, true);

      std::cout << " Parent TimeStamp             : ";
      DumpBytes(iHeader.ParentTimeStamp, 4, true);

      std::wcout << L" Parent Name                  : " << (wchar_t *)(iHeader.ParentName + 1) << L"\n"; // skip bom

      auto parentLocators = GetParentLocators();

      for(const auto& pl : parentLocators)
      {
        std::wcout << L" Parent Locator Table Entry   : " << pl << L"\n";
      }

      auto mte = GetTotalBATEntries();

      std::cout << " Max Table Entries            : " << mte << "\n";

      uint32_t count = 0;

      for (uint32_t i = 0; i < mte; i++)
      {
        if (iBAT[i] != 0xFFFFFFFF)
        {
          count++;
        }
      }

      std::cout << " Valid BAT count              : " << count << "\n";

      CBaseDisk::Dump();

      std::cout << "\n\n";
    }

  protected:

    virtual size_t DataBlockRead(DataBlockIO& f) override
    {
      uint64_t batindex = f.offset / GetBlockSize();
      uint32_t batentry = BTOL32((uint8_t *) (iBAT.get() + batindex));

      uint64_t blockoff = f.offset % GetBlockSize();

      if (batentry == ~((uint32_t)0)) // unused block
      {
        memset((void *)f.buffer, 0, f.length);
      }
      else // existing data block
      {
        auto fRet = CSubject::ReadSync(f.buffer, f.length, (batentry * 512ULL) + 512ULL + blockoff);
        assert(fRet == f.length);
      }
      
      return f.length;
    }

    virtual size_t DataBlockWrite(DataBlockIO& f) override
    {
      uint64_t batindex = f.offset / GetBlockSize();
      uint32_t batentry = BTOL32((uint8_t *) (iBAT.get() + batindex));

      auto blockoff = f.offset % GetBlockSize();

      if (batentry == ~((uint32_t)0)) // new data block
      {
        if (IsDifferencing())
        {
          memset(iPayloadBlock.get(), 0, 512);
          SetSectorBitmap(iPayloadBlock.get(), blockoff, f.length, GetBlockSize());
        }
        else
        {
          memset(iPayloadBlock.get(), 0xff, 512);
        }

        // zero out the payload block, past the sector bitmap
        memset(iPayloadBlock.get() + 512, 0, GetBlockSize());

        // position fragment buffer inside the new payload block
        memmove(iPayloadBlock.get() + 512 + blockoff, f.buffer, f.length);

        // new block entry
        uint64_t fileoffset = FirstDataBlockOffset() + (iBlockCount * (512 + GetBlockSize()));
        LTOB(fileoffset / 512, (unsigned char *) (iBAT.get() + batindex), 4);

        // write the new payload block
        auto fRet = CSubject::WriteSync(iPayloadBlock.get(), 512 + GetBlockSize(), fileoffset);

        assert(fRet == 512 + GetBlockSize());

        iBlockCount++;
      }
      else // existing data block
      {
        if (IsDifferencing())
        {
          // zero out the complete data block (sb + data)
          memset(iPayloadBlock.get(), 0, 512 + GetBlockSize());

          // read the complete data block (sb + data) off the disk.
          CSubject::ReadSync(iPayloadBlock.get(), 512 + GetBlockSize(), batentry * 512);

          SetSectorBitmap(iPayloadBlock.get(), blockoff, f.length, GetBlockSize());

          // position fragment buffer inside the payload block we just read
          memmove(iPayloadBlock.get() + 512 + blockoff, f.buffer, f.length);

          auto fRet = CSubject::WriteSync(iPayloadBlock.get(), 512 + GetBlockSize(), batentry * 512ULL);

          assert(fRet == 512 + GetBlockSize());
        }
        else
        {
          memset(iPayloadBlock.get(), 0xff, 512); // ??
          auto fRet = CSubject::WriteSync(f.buffer, f.length, (batentry * 512ULL) + 512ULL + blockoff);
          assert(fRet == f.length);
        }
      }

      return f.length;
    }

    void InitializeFooter(uint64_t size, bool differencing)
    {
      memmove(&iFooter, "conectix", strlen("conectix"));

      iFooter.Features[3] = 0x02;

      iFooter.FileFormatVersion[1] = 0x01;

      LTOB(sizeof(VHD_DISK_FOOTER), (unsigned char *) &(iFooter.DataOffset), 8);

      LTOB((std::time(nullptr) - 946684800), (unsigned char *) &(iFooter.TimeStamp), 4);

      memmove(&(iFooter.CreatorApplication), "MSys", strlen("MSys"));

      iFooter.CreatorHostOS[0] = 0x57;
      iFooter.CreatorHostOS[1] = 0x69;
      iFooter.CreatorHostOS[2] = 0x32;
      iFooter.CreatorHostOS[3] = 0x6B;

      LTOB(size, (unsigned char *) &(iFooter.OriginalSize), 8);
      LTOB(size, (unsigned char *) &(iFooter.CurrentSize), 8);

      LTOB32(differencing ? 4 : 3, (unsigned char *) &(iFooter.DiskType));

      memset(&(iFooter.Checksum), 0, 4);

      iFooter.SavedState = 0;

      memset(&(iFooter.Reserved), 0, 427);

      uint64_t checksum = 0;

      for (int i = 0; i < sizeof(iFooter); i++)
      {
        checksum += ((uint8_t *) &iFooter)[i];
      }

      LTOB32(~checksum, (unsigned char *) &(iFooter.Checksum));
    }

    void InitializeSparseHeader(uint32_t blocksize, CDynamicVHD *parent = nullptr)
    {
      memmove(&iHeader, "cxsparse", strlen("cxsparse"));

      memset(&(iHeader.DataOffset), 0xFF, 8);

      uint64_t tableOffset = sizeof(VHD_FOOTER_HEADER) + (IsDifferencing() ? (2 * PLDataSpaceSize) : 0);
      LTOB(tableOffset, (unsigned char *) &(iHeader.TableOffset), 8);

      memset(&(iHeader.HeaderVersion), 0, 4);
      iHeader.HeaderVersion[1] = 0x01;

      LTOB32(blocksize, (unsigned char *) &(iHeader.BlockSize));

      uint64_t maxTableEntries = GetLogicalDiskLength() / GetBlockSize();

      if (GetLogicalDiskLength() % GetBlockSize())
      {
        maxTableEntries += 1;
      }

      LTOB32(maxTableEntries, (unsigned char *) &(iHeader.MaxTableEntries));

      if (IsDifferencing())
      {
        assert(parent);

        memmove(iHeader.ParentUniqueId, parent->iFooter.UniqueId, 16);
        memmove(iHeader.ParentTimeStamp, parent->iFooter.TimeStamp, 4);
        memmove(iHeader.ParentName + 1, parent->iPath.wstring().c_str(), parent->iPath.wstring().size() * sizeof(wchar_t));

        // ple 1
        memmove(iHeader.ParentLocatorTable[0].PlatformCode, "W2ku" , 4);
        LTOB32(PLDataSpaceSize, iHeader.ParentLocatorTable[0].PlatformDataSpace);
        LTOB32(parent->iPath.wstring().size() * sizeof(wchar_t), iHeader.ParentLocatorTable[0].PlatformDataLength);
        LTOB64(sizeof(VHD_FOOTER_HEADER), iHeader.ParentLocatorTable[0].PlatformDataOffset);

        std::wstring ru = L".\\" + parent->iPath.filename().wstring();

        // ple 2
        memmove(iHeader.ParentLocatorTable[1].PlatformCode, "W2kru" , 5);
        LTOB32(PLDataSpaceSize, iHeader.ParentLocatorTable[1].PlatformDataSpace);
        LTOB32(ru.size() * sizeof(wchar_t), iHeader.ParentLocatorTable[1].PlatformDataLength);
        LTOB64(sizeof(VHD_FOOTER_HEADER) + PLDataSpaceSize, iHeader.ParentLocatorTable[1].PlatformDataOffset);        
      }

      memset(&(iHeader.Checksum), 0, 4);

      uint64_t checksum = 0;

      for (int i = 0; i < sizeof(iHeader); i++)
      {
        checksum += ((uint8_t *) &iHeader)[i];
      }

      LTOB32(~checksum, (unsigned char *) &(iHeader.Checksum));
    }

    void InitializeBAT()
    {
      uint64_t n = BTOL((uint8_t *) &(iHeader.MaxTableEntries), 4);

      iBATSize = n * sizeof(uint32_t);

      if (iBATSize % 512)
      {
        iBATSize += 512 - (iBATSize % 512);
      }

      iBAT = std::make_unique<uint32_t []>(iBATSize / sizeof(uint32_t));

      memset(iBAT.get(), 0xFF, iBATSize);
    }
};

using SPCDynamicVHD = std::shared_ptr<CDynamicVHD>;

#endif