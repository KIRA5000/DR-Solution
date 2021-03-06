#ifndef VHD_HPP
#define VHD_HPP

#include <npl.hpp>

#include <osl.hpp>
#include <DynVHD.hpp>
#include <rct.hpp>

#include <string>

namespace VHD
{
  //create base parent vhd from a volume/snapshot block device 
  void create_base_vhd(const std::string& volume, const std::string& file)
  {
    auto source = NPL::make_file(volume, false);

    auto nvdb = OSL::GetNTFSVolumeData(source->iFDsync);

    uint64_t length = OSL::GetPartitionLength(source->iFDsync);

    std::cout << " Source volume length " << length << "\n";

    auto disk = std::make_shared<CDynamicVHD>(length, _2M);

    auto bs = disk->GetBlockSize();

    auto _buf = std::make_unique<uint8_t []>(bs);

    auto target = NPL::make_file(file, true);

    // footer
    target->WriteSync((uint8_t *) &(disk->iFooter), sizeof(VHD_DISK_FOOTER), 0);

    // sparse header
    target->WriteSync((uint8_t *) &(disk->iHeader), sizeof(VHD_SPARSE_HEADER), sizeof(VHD_DISK_FOOTER));

    // pre-compute the BAT
    auto bat = std::make_unique<uint32_t []>(disk->iBATSize / sizeof(uint32_t));
    memset(bat.get(), 0xFF, disk->iBATSize);

    // MBR block bat entry
    LTOB32(disk->FirstDataBlockOffset() / 512, (unsigned char *) (bat.get() + 0));

    uint64_t index = 1;
    uint64_t count = 1;

    auto blockInUse = false;

    auto bitmap = OSL::GetVolumeInUseBitmap(source->iFDsync);

    for (int64_t i = 1; i <= nvdb.TotalClusters.QuadPart; i++)
    {
      if (bitmap->Buffer[(i - 1) / 8])
      {
        blockInUse = true;
      }

      if (((i * nvdb.BytesPerCluster) % bs == 0) || (i == nvdb.TotalClusters.QuadPart))
      {
        if (blockInUse)
        {
          uint64_t blockFileOffset = disk->FirstDataBlockOffset() + (count * (512 + bs));
          LTOB32(blockFileOffset / 512, (unsigned char *) (bat.get() + index));
          blockInUse = false;
          count++;
        }
        index++;
      }
    }

    std::cout << " BAT constructed with " << count << " block entries. Last index " << index << "\n";

    uint64_t totalClusterLength = nvdb.TotalClusters.QuadPart * nvdb.BytesPerCluster;

    std::cout << " Source volume total cluster length " << totalClusterLength << "\n";

    uint64_t pendinglen = length - totalClusterLength;

    std::cout << " Source volume pending length " << pendinglen << "\n";

    if (pendinglen)
    {
      uint64_t pendingoff = bs + totalClusterLength;

      auto fragments = CBaseDisk::LogicalToDataBlock(pendingoff, pendinglen, bs);

      for (auto& f : fragments)
      {
        uint64_t batindex = f.offset / bs;

        uint32_t batentry = BTOL32((uint8_t *) (bat.get() + batindex));

        if (batentry == ~((uint32_t)0))
        {
          std::cout << " BAT setting entry for pending len at " << batindex << "\n";
          uint64_t blockFileOffset = disk->FirstDataBlockOffset() + (count * (512 + bs));
          LTOB32(blockFileOffset / 512, (unsigned char *) (bat.get() + batindex));
          count++;
        }
      }
    }

    // bat
    target->WriteSync((uint8_t *) bat.get(), disk->iBATSize, sizeof(VHD_FOOTER_HEADER));

    // set target for io operations on disk 
    target->AddEventListener(disk);

    // MBR block
    disk->WriteSync((uint8_t *) &(disk->iMBR), sizeof(MBR), 0);

    count--;

    // payload blocks

    for (index = 1; index < disk->GetTotalBATEntries(); index++)
    {
      uint32_t entry = BTOL32((uint8_t *) (bat.get() + index));

      if (entry != ~((uint32_t)0))
      {
        uint64_t len = bs;

        uint64_t off = bs * (index - 1);

        if ((off + bs) > length)
        {
          len = length % bs;
        }

        // read the source device
        auto fRet = source->ReadSync(_buf.get(), len, off);

        assert (fRet == len);

        // disk level write of volume level data block
        fRet = disk->WriteSync(_buf.get(), len, off + disk->GetPartitionStartOffset(0));

        assert (fRet == len);

        assert(bat[index] == disk->iBAT[index]);

        count--;
      }
    }

    assert(count == 0);

    // assert the fact that constructed bat matches the one 
    // which is dynamically maintained due to disk level writes
    if (memcmp(disk->iBAT.get(), bat.get(), disk->iBATSize))
    {
      DumpData("cbat.bin", (uint8_t *) bat.get(), disk->iBATSize);
      DumpData("rbat.bin", (uint8_t *) disk->iBAT.get(), disk->iBATSize);
      throw std::runtime_error("constructed bat does not match runtime bat");
    }

    // footer
    target->WriteSync(
      (uint8_t *) &(disk->iFooter),
      sizeof(VHD_DISK_FOOTER),
      disk->FirstDataBlockOffset() + (disk->iBlockCount * (512 + bs))
    );

    std::cout << " " << file << " created\n";
  }

  //create differencing child vhd using either RCT CBT ranges (disk level) or volume level CBT
  bool create_child_vhd(const std::string& source, const std::string& parent, const std::string& child, const std::string rctid)
  {
    auto base = std::make_shared<CDynamicVHD>(parent);

    auto diff = std::make_shared<CDynamicVHD>(base.get());

    auto target = NPL::make_file(child, true);

    auto fsBasePath = base->iPath;

    base.reset();

    // footer
    target->WriteSync((uint8_t *) &(diff->iFooter), sizeof(VHD_DISK_FOOTER), 0);

    // sparse header
    target->WriteSync((uint8_t *) &(diff->iHeader), sizeof(VHD_SPARSE_HEADER), sizeof(VHD_DISK_FOOTER));

    // parent locator 1
    uint8_t pl[PLDataSpaceSize] = { 0 };
    memmove(pl, fsBasePath.wstring().c_str(), fsBasePath.wstring().size() * sizeof(wchar_t));
    target->WriteSync(pl, PLDataSpaceSize, sizeof(VHD_FOOTER_HEADER));

    // parent locator 2
    memset(pl, 0, PLDataSpaceSize);
    std::wstring ru = L".\\" + fsBasePath.filename().wstring();
    memmove(pl, ru.c_str(), ru.size() * sizeof(wchar_t));
    target->WriteSync(pl, PLDataSpaceSize, sizeof(VHD_FOOTER_HEADER) + PLDataSpaceSize);

    // pre-compute the BAT
    auto bat = std::make_unique<uint32_t []>(diff->iBATSize / sizeof(uint32_t));
    memset(bat.get(), 0xFF, diff->iBATSize);

    auto bs = diff->GetBlockSize();

    auto dbiomap = RCT::ResilientChangeTrackingToDataBlockIO(source, rctid, bs);

    uint64_t blockCount = 0;

    for (auto& kv : dbiomap)
    {
      uint64_t fileoff = diff->FirstDataBlockOffset() + (blockCount * (512 + bs));
      LTOB32(fileoff / 512, (unsigned char *) (bat.get() + kv.first));
      blockCount++;
    }

    // BAT
    target->WriteSync((uint8_t *) bat.get(), diff->iBATSize, sizeof(VHD_FOOTER_HEADER) + (2 * PLDataSpaceSize));

    auto hvhd = OSL::AttachVHD(source);

    if (hvhd == INVALID_HANDLE_VALUE)
    {
      std::cout << "create_child_vhd failed to attach vhd\n";
      return false; 
    }

    auto phyDiskPath = OSL::GetPhysicalDiskPath(hvhd);

    if (!phyDiskPath.size())
    {
      std::cout << "create_child_vhd failed to get physical disk object\n";
      return false; 
    }

    auto phyDisk = NPL::make_file(phyDiskPath);

    // incremental data blocks
    auto _buf = std::make_unique<uint8_t []>(512 + bs);

    blockCount = 0;

    for (auto& kv : dbiomap)
    {
      std::cout << "BAT index : " << kv.first << "\n";

      memset(_buf.get(), 0, 512 + bs);

      for (auto& dbio : kv.second)
      {
        std::cout << " off : " << dbio.offset << " len : " << dbio.length << "\n";

        auto blockoff = dbio.offset % bs;

        CDynamicVHD::SetSectorBitmap(_buf.get(), blockoff, dbio.length, bs);

        auto fRet = phyDisk->ReadSync(_buf.get() + 512 + blockoff, dbio.length, dbio.offset);
      }

      target->WriteSync(
        (uint8_t *) _buf.get(),
        512 + bs,
        diff->FirstDataBlockOffset() + (blockCount * (512 + bs))
      );

      blockCount++;
    }

    OSL::DllHelper virtDiskAPI("virtdisk.dll");

    auto pfn = (decltype(DetachVirtualDisk) *) virtDiskAPI["DetachVirtualDisk"];

    if (pfn == NULL)
    {
      std::cout << "Failed to get poinetr to DetachVirtualDisk " << GetLastError() << "\n";
      return false;
    }

    pfn(hvhd, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);

    // footer
    target->WriteSync(
      (uint8_t *) &(diff->iFooter),
      sizeof(VHD_DISK_FOOTER),
      diff->FirstDataBlockOffset() + (blockCount * (512 + bs))
    );

    return true;
  }

}

#endif