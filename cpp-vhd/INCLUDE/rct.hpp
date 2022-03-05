#ifndef RCT_HPP
#define RCT_HPP

#include <osl.hpp>

#include <string>
#include <iostream>

namespace RCT
{
  using RCTCBT = std::tuple <
    std::unique_ptr<QUERY_CHANGES_VIRTUAL_DISK_RANGE []>, ULONG
  >;

  RCTCBT GetRCTRanges(const std::wstring& file, const std::wstring& rctid)
  {
    HANDLE hvhd = OSL::OpenVirtualDisk(file);

    if (hvhd == INVALID_HANDLE_VALUE)
    {
      return {};
    }

    // query rctid 
    DWORD fRet;
    WCHAR vdinfo[2048];

    memset(vdinfo, 0, sizeof(vdinfo));
    ((GET_VIRTUAL_DISK_INFO *)vdinfo)->Version = GET_VIRTUAL_DISK_INFO_CHANGE_TRACKING_STATE;
    ULONG vdinfoSize = sizeof(vdinfo);

    OSL::DllHelper virtDiskAPI("virtdisk.dll");

    auto pfn = (decltype(GetVirtualDiskInformation) *) virtDiskAPI["GetVirtualDiskInformation"];

    if (pfn == NULL)
    {
      std::cout << "Failed to get poinetr to GetVirtualDiskInformation " << GetLastError() << "\n";
      return {};
    }

    fRet = pfn(
      hvhd,
      &vdinfoSize,
      (PGET_VIRTUAL_DISK_INFO)vdinfo,
      NULL);

    if (fRet != ERROR_SUCCESS)
    {
      std::cout << "GetVirtualDiskInformation GET_VIRTUAL_DISK_INFO_CHANGE_TRACKING_STATE failed " << fRet << "\n";
      return {};
    }

    std::wcout << "RCT ID : " << std::wstring(((GET_VIRTUAL_DISK_INFO *)vdinfo)->ChangeTrackingState.MostRecentId) << "\n";
  
    // query disk length
    memset(vdinfo, 0, sizeof(vdinfo));
    ((GET_VIRTUAL_DISK_INFO *)vdinfo)->Version = GET_VIRTUAL_DISK_INFO_SIZE;
    vdinfoSize = sizeof(vdinfo);

    fRet = pfn(
      hvhd,
      &vdinfoSize,
      (PGET_VIRTUAL_DISK_INFO)vdinfo,
      NULL);

    if (fRet != ERROR_SUCCESS)
    {
      std::cout << "GetVirtualDiskInformation GET_VIRTUAL_DISK_INFO_SIZE failed " << fRet << "\n";
      return {};
    }

    std::cout << "Disk length : " << ((GET_VIRTUAL_DISK_INFO *)vdinfo)->Size.VirtualSize << "\n";

    ULONG count = 4096;
    ULONG64 ProcessedLength = 0;

    auto ranges = std::make_unique<QUERY_CHANGES_VIRTUAL_DISK_RANGE []>(4096);

    auto pfnQCVD = (decltype(QueryChangesVirtualDisk) *) virtDiskAPI["QueryChangesVirtualDisk"];

    if (pfnQCVD == NULL)
    {
      std::cout << "Failed to get poinetr to QueryChangesVirtualDisk " << GetLastError() << "\n";
      return {};
    }

    fRet = pfnQCVD(
      hvhd,
      rctid.c_str(), //((GET_VIRTUAL_DISK_INFO *)vdinfo)->ChangeTrackingState.MostRecentId,
      0,
      ((GET_VIRTUAL_DISK_INFO *)vdinfo)->Size.VirtualSize,
      QUERY_CHANGES_VIRTUAL_DISK_FLAG_NONE,
      ranges.get(),
      &count,
      &ProcessedLength);

    if (fRet != ERROR_SUCCESS)
    {
      std::cout << "QueryChangesVirtualDisk failed " << fRet << "\n";
      return {};
    }

    std::cout << "Range count : " << count << "\n";
    std::cout << "Processed length : " << ProcessedLength << "\n";

    CloseHandle(hvhd);

    return {std::move(ranges), count};
  }

  auto ResilientChangeTrackingToDataBlockIO(const std::string& source, const std::string& rctid, uint32_t bs)
  {
    std::map<uint64_t, std::vector<DataBlockIO>> dbiomap;

    auto& [ranges, count] = GetRCTRanges(
      std::wstring(source.begin(), source.end()),
      std::wstring(rctid.begin(), rctid.end())
    );

    for (ULONG i = 0; i < count; i++)
    {
      auto off = ranges[i].ByteOffset;
      auto len = ranges[i].ByteLength;

      auto fragments = CBaseDisk::LogicalToDataBlock(off, len, bs);

      for (auto& fragmet : fragments)
      {
        uint64_t batindex = fragmet.offset / bs;
        auto& dbio = dbiomap[batindex];
        dbio.push_back({nullptr, fragmet.offset, fragmet.length});
      }
    }

    std::cout << "Data block io map size : " << dbiomap.size() << "\n";

    for (auto& kv : dbiomap)
    { 
      uint64_t nTotalLength = 0;

      for (auto& dbio : kv.second)
      {
        nTotalLength += dbio.length;
      }

      assert(nTotalLength <= bs);
    }

    return dbiomap;
  }
}

#endif