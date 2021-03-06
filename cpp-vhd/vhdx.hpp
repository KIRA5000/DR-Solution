#ifndef VHDX_HPP
#define VHDX_HPP

#include <npl.hpp>

#include <osl.hpp>
#include <DynVHDx.hpp>
#include <rct.hpp>

#include <string>

namespace VHDX
{
  //create base parent vhdx from a volume/snapshot block device 
  void create_base_vhdx(const std::string& volume, const std::string& file)
  {
    auto source = NPL::make_file(volume, false);

    auto nvdb = OSL::GetNTFSVolumeData(source->iFDsync);

    uint64_t length = OSL::GetPartitionLength(source->iFDsync);

    std::cout << " Source volume length " << length << "\n";

    auto disk = std::make_shared<CDynamicVHDx>(length, _4M);

    auto bs = disk->GetBlockSize();

    auto _buf = std::make_unique<uint8_t []>(bs);

    auto target = NPL::make_file(file, true);

    memset(_buf.get(), 0, _1M);

    // file identifier
    memmove(_buf.get(), (void *) &disk->iFileIdentifier, sizeof(VHDX_FILE_IDENTIFIER));

    // vhdx Header 1
    memmove(_buf.get() + _64K, (void *) &disk->iHeader, sizeof(VHDX_HEADER));

    // vhdx Header 2
    disk->iHeader.SequenceNumber++;
    memset((void *) &disk->iHeader.Checksum, 0, 4);
    disk->iHeader.Checksum = crc32c_value((uint8_t *) &disk->iHeader, _4K);
    memmove(_buf.get() + _64K + _64K, (void *) &disk->iHeader, sizeof(VHDX_HEADER));

    // Region Table 1
    memmove(_buf.get() + _64K + _64K + _64K, (void *) &disk->iRegionTable, sizeof(VHDX_REGION_TABLE));

    // Region Table 2
    memmove(_buf.get() + _64K + _64K + _64K + _64K, (void *) &disk->iRegionTable, sizeof(VHDX_REGION_TABLE));

    // 1M vhdx header
    target->WriteSync(_buf.get(), _1M, 0);

    // log region
    memset(_buf.get(), 0, _1M);
    target->WriteSync(_buf.get(), _1M, _1M);

    memset(_buf.get(), 0, _1M);

    // metadata region: prolouge
    memmove(_buf.get(), (void *) &disk->iMetadata.iTable, sizeof(VHDX_METADATA_TABLE));

    // metadata region: objects
    memmove(_buf.get() + _64K, &disk->iMetadata.iObjects, sizeof(VHDX_METADATA_OBJECTS));

    target->WriteSync(_buf.get(), _1M, _2M);

    // pre-compute the BAT
    auto bat = std::make_unique<uint64_t []>(disk->iBATSize / sizeof(uint64_t));
    memset(bat.get(), 0x00, disk->iBATSize);

    // GPT block bat entry
    VHDX_BAT_ENTRY entry = { VHDX_BAT_ENTRY_PAYLOAD_BLOCK_FULLY_PRESENT, 0, disk->FirstDataBlockOffsetMB() };
    bat[0] = *((uint64_t *) &entry);

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
          uint64_t blockFileOffsetMB = disk->FirstDataBlockOffsetMB() + ((count * bs) / _1M);
          VHDX_BAT_ENTRY entry = { VHDX_BAT_ENTRY_PAYLOAD_BLOCK_FULLY_PRESENT, 0, blockFileOffsetMB };
          bat[index] = *((uint64_t *) &entry);
          blockInUse = false;
          count++;
        }

        index++;

        if ((index + 1) % (disk->iChunkRatio + 1) == 0)
        {
          index++;
        }
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
        uint64_t pb_entry_cnt = f.offset / bs;

        uint64_t sb_entry_cnt = pb_entry_cnt / disk->iChunkRatio;

        uint64_t batindex = pb_entry_cnt + sb_entry_cnt;

        if ((batindex + 1) % (disk->iChunkRatio + 1) == 0)
        {
          batindex++;
        }

        uint64_t batentry = bat[batindex];

        if (VHDX_BAT_ENTRY_GET_STATE(batentry) == VHDX_BAT_ENTRY_PAYLOAD_BLOCK_NOT_PRESENT)
        {
          std::cout << " BAT setting entry for pending len at " << batindex << "\n";
          uint64_t blockFileOffsetMB = disk->FirstDataBlockOffsetMB() + ((count * bs) / _1M);
          VHDX_BAT_ENTRY entry = { VHDX_BAT_ENTRY_PAYLOAD_BLOCK_FULLY_PRESENT, 0, blockFileOffsetMB };
          bat[batindex] = *((uint64_t *) &entry);
          count++;
        }
      }
    }

    // bat
    target->WriteSync((uint8_t *) bat.get(), disk->iBATSize, _3M);

    // set target for io operations on disk 
    target->AddEventListener(disk);

    // MBR block
    disk->WriteSync((uint8_t *) &(disk->iMBR), sizeof(MBR), 0);

    count--;

    // payload blocks

    for (index = 1; index < disk->GetTotalBATEntries(); index++)
    {
      if ((index + 1) % (disk->iChunkRatio + 1) == 0)
      {
        continue;
      }

      int state = VHDX_BAT_ENTRY_GET_STATE(bat[index]);

      if (state == VHDX_BAT_ENTRY_PAYLOAD_BLOCK_FULLY_PRESENT)
      {
        uint64_t len = bs;

        uint64_t sb_entry_cnt = (index + 1) / (disk->iChunkRatio + 1);

        uint64_t off = bs * (index - sb_entry_cnt - 1);

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

    std::cout << " " << file << " created\n";
  }

  //create differencing child vhd using either RCT CBT ranges (disk level) or volume level CBT
  bool create_child_vhdx(const std::string& source, const std::string& parent, const std::string& child, const std::string rctid)
  {
    auto base = std::make_shared<CDynamicVHDx>(parent);

    auto diff = std::make_shared<CDynamicVHDx>(base.get());

    auto target = NPL::make_file(child, true);

    base.reset();

    auto _buf = std::make_unique<uint8_t []>(_1M);

    memset(_buf.get(), 0, _1M);

    // file identifier
    memmove(_buf.get(), (void *) &diff->iFileIdentifier, sizeof(VHDX_FILE_IDENTIFIER));

    // vhdx Header 1
    memmove(_buf.get() + _64K, (void *) &diff->iHeader, sizeof(VHDX_HEADER));

    // vhdx Header 2
    diff->iHeader.SequenceNumber++;
    memset((void *) &diff->iHeader.Checksum, 0, 4);
    diff->iHeader.Checksum = crc32c_value((uint8_t *) &diff->iHeader, _4K);
    memmove(_buf.get() + _64K + _64K, (void *) &diff->iHeader, sizeof(VHDX_HEADER));

    // Region Table 1
    memmove(_buf.get() + _64K + _64K + _64K, (void *) &diff->iRegionTable, sizeof(VHDX_REGION_TABLE));

    // Region Table 2
    memmove(_buf.get() + _64K + _64K + _64K + _64K, (void *) &diff->iRegionTable, sizeof(VHDX_REGION_TABLE));

    // 1M vhdx header
    target->WriteSync(_buf.get(), _1M, 0);

    // log region
    memset(_buf.get(), 0, _1M);
    target->WriteSync(_buf.get(), _1M, _1M);

    memset(_buf.get(), 0, _1M);

    // metadata region: prolouge
    memmove(_buf.get(), (void *) &diff->iMetadata.iTable, sizeof(VHDX_METADATA_TABLE));

    // metadata region: objects
    memmove(_buf.get() + _64K, &diff->iMetadata.iObjects, sizeof(VHDX_METADATA_OBJECTS));

    target->WriteSync(_buf.get(), _1M, _2M);

    // pre-compute the BAT
    auto bat = std::make_unique<uint64_t []>(diff->iBATSize / sizeof(uint64_t));
    memset(bat.get(), 0, diff->iBATSize);

    auto bs = diff->GetBlockSize();

    auto dbiomap = RCT::ResilientChangeTrackingToDataBlockIO(source, rctid, bs);

    uint64_t blockCount = 0;

    for (auto& kv : dbiomap)
    {
      uint64_t pb_entry_cnt = kv.first;
      uint64_t sb_entry_cnt = pb_entry_cnt / diff->iChunkRatio;

      uint64_t batindex = pb_entry_cnt + sb_entry_cnt;

      VHDX_BAT_ENTRY entry = { VHDX_BAT_ENTRY_PAYLOAD_BLOCK_FULLY_PRESENT, 0, diff->FirstDataBlockOffsetMB() +  ((bs * blockCount) / _1M) };

      bat[batindex] = *((uint64_t *) &entry);

      std::cout << "Bat construction : setting bat entry : " << kv.first << "\n";

      blockCount++;
    }

    target->WriteSync((uint8_t *) bat.get(), diff->iBATSize, _3M);

    auto hvhd = OSL::AttachVHD(source);

    if (hvhd == INVALID_HANDLE_VALUE)
    {
      std::cout << "create_child_vhdx failed to attach vhd\n";
      return false; 
    }

    auto phyDiskPath = OSL::GetPhysicalDiskPath(hvhd);

    if (!phyDiskPath.size())
    {
      std::cout << "create_child_vhdx failed to get physical disk object\n";
      return false; 
    }

    auto phyDisk = NPL::make_file(phyDiskPath);

    // incremental data blocks
    _buf = std::make_unique<uint8_t []>(bs);

    blockCount = 0;

    for (auto& kv : dbiomap)
    {
      std::cout << "BAT index : " << kv.first << "\n";

      auto fRet = phyDisk->ReadSync(_buf.get(), bs, bs * kv.first);

      assert(fRet == bs);

      fRet = target->WriteSync(_buf.get(), bs, (diff->FirstDataBlockOffsetMB() * _1M) + (blockCount * bs));

      assert(fRet == bs);

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

    return true;
  }
}

#endif