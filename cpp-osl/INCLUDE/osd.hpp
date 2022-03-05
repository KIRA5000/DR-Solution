#ifndef OSD_HPP
#define OSD_HPP

#include <virtdisk.h>

#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace OSL
{
  class DllHelper
  {
    public:

      DllHelper(const std::string& dll)
      {
        hDll = LoadLibraryA(dll.c_str());
      }

      ~DllHelper()
      {
        FreeLibrary(hDll);
      }

      auto operator[](const std::string& proc) const 
      {
        return GetProcAddress(hDll, proc.c_str());
      }

    private:

      HMODULE hDll;
  };

  HANDLE OpenVirtualDisk(const std::wstring& file)
  {
    DWORD fRet;
    HANDLE hvhd = INVALID_HANDLE_VALUE;
    OPEN_VIRTUAL_DISK_PARAMETERS params = { 0 };
    params.Version = OPEN_VIRTUAL_DISK_VERSION_3;

    VIRTUAL_STORAGE_TYPE vst = 
    {
      VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN,
      { 0xEC984AEC, 0xA0F9, 0x47e9, { 0x90, 0x1F, 0x71, 0x41, 0x5A, 0x66, 0x34, 0x5B } }
    };

    DllHelper virtDiskAPI("virtdisk.dll");

    auto pfn = (decltype(::OpenVirtualDisk) *) virtDiskAPI["OpenVirtualDisk"];

    if (pfn == NULL)
    {
      std::cout << "failed to get poinetr to OpenVirtualDisk " << GetLastError() << "\n";
      return hvhd;
    }

    fRet = pfn(
      &vst,
      file.c_str(),
      (VIRTUAL_DISK_ACCESS_MASK) 0,
      OPEN_VIRTUAL_DISK_FLAG_NONE,
      &params,
      &hvhd);

    if (fRet != ERROR_SUCCESS)
    {
      std::cout << "OpenVirtualDisk failed " << fRet << "\n";
    }

    return hvhd;
  }

  HANDLE AttachVHD(const std::string& source)
  {
    HANDLE hvhd = OpenVirtualDisk(
      std::wstring(source.begin(), source.end())
    );

    ATTACH_VIRTUAL_DISK_PARAMETERS params = { 0 };
    params.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    DllHelper virtDiskAPI("virtdisk.dll");

    auto pfn = (decltype(AttachVirtualDisk) *) virtDiskAPI["AttachVirtualDisk"];

    if (pfn == NULL)
    {
      std::cout << "Failed to get poinetr to AttachVirtualDisk " << GetLastError() << "\n";
      return INVALID_HANDLE_VALUE;
    }

    auto fRet = pfn(
       hvhd,
       NULL,
       ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY|ATTACH_VIRTUAL_DISK_FLAG_NO_DRIVE_LETTER,
       0,
       &params,
       NULL);

    if (fRet != ERROR_SUCCESS)
    {
      std::cout << "AttachVirtualDisk failed, error : " << fRet << "\n";
      return INVALID_HANDLE_VALUE;
    }

    return hvhd;
  }

  auto GetPhysicalDiskPath(HANDLE hvhd)
  {
    wchar_t buf[128] = { L'\0' };
    ULONG bufSize = sizeof(buf);

    DllHelper virtDiskAPI("virtdisk.dll");

    auto pfn = (decltype(GetVirtualDiskPhysicalPath) *) virtDiskAPI["GetVirtualDiskPhysicalPath"];

    if (pfn == NULL)
    {
      std::cout << "Failed to get poinetr to GetVirtualDiskPhysicalPath " << GetLastError() << "\n";
      return std::string();
    }

    auto fRet = pfn(hvhd, &bufSize, buf);

    if (fRet != ERROR_SUCCESS)
    {
      std::cout << "GetVirtualDiskPhysicalPath failed, error : " << fRet << "\n";
      return std::string();
    }

    char phyDiskPath[128] = { '\0' };
    wchar_t *wc = buf;

    while (*wc)
    {
      phyDiskPath[strlen(phyDiskPath)] = *((char *)wc); 
      wc++;
    }

    return std::string(phyDiskPath);
  }

  PVOLUME_BITMAP_BUFFER GetVolumeInUseBitmap(HANDLE hvolume)
  {
    STARTING_LCN_INPUT_BUFFER startLCN;
    PVOLUME_BITMAP_BUFFER bitmap = NULL;
    DWORD dwAllocatedSize = 64 * 1024;
    DWORD BytesReturned = 0;

    startLCN.StartingLcn.QuadPart = 0;

    for (;;)
    {
      bitmap = (VOLUME_BITMAP_BUFFER *) LocalAlloc(LMEM_FIXED, dwAllocatedSize);

      BOOL fRet = DeviceIoControl(
                    hvolume, 
                    FSCTL_GET_VOLUME_BITMAP, 
                    &startLCN,
                    sizeof(startLCN),
                    bitmap, 
                    dwAllocatedSize, 
                    &BytesReturned, 
                    NULL);

      if (fRet)
      {
        // DumpData("bitmap", bitmap->Buffer, (bitmap->BitmapSize.QuadPart)/8 + ( ((bitmap->BitmapSize.QuadPart)%8) ? 1 : 0 ));
        return bitmap;
      }
      else if (GetLastError() != ERROR_MORE_DATA)
      {
        std::cout << "FSCTL_GET_VOLUME_BITMAP failed, error : " << GetLastError() << "\n";
        return NULL;
      }

      LocalFree(bitmap);
      dwAllocatedSize *= 2;
    }
  }

  NTFS_VOLUME_DATA_BUFFER GetNTFSVolumeData(HANDLE hDevice)
  {
    DWORD fRet, BytesReturned;
    NTFS_VOLUME_DATA_BUFFER nvdb = {0};

    fRet = DeviceIoControl(
             hDevice,
             FSCTL_ALLOW_EXTENDED_DASD_IO,
             nullptr,
             0,
             nullptr,
             0,
             &BytesReturned,
             nullptr);

    if (!fRet)
    {
      std::cout << "FSCTL_ALLOW_EXTENDED_DASD_IO failed, error : " << GetLastError() << "\n";
      return {};
    }

    fRet = DeviceIoControl(
             hDevice,
             FSCTL_GET_NTFS_VOLUME_DATA,
             nullptr,
             0,
             &nvdb,
             sizeof (NTFS_VOLUME_DATA_BUFFER),
             &BytesReturned,
             nullptr);

    if (!fRet) 
    {
      std::cout << "FSCTL_GET_NTFS_VOLUME_DATA failed, error : " << GetLastError() << "\n";
      return {};
    }

    return nvdb;
  }

  LONGLONG GetPartitionLength(HANDLE hVolume)
  {
    DWORD nReturned;
    GET_LENGTH_INFORMATION  gli = { 0 };

    if (!DeviceIoControl(
           hVolume,
           IOCTL_DISK_GET_LENGTH_INFO,
           NULL,
           0,
           &gli,
           sizeof (GET_LENGTH_INFORMATION),
           &nReturned,
           NULL)) 
    {
      std::cout << "IOCTL_DISK_GET_LENGTH_INFO failed, error : " << GetLastError() << "\n";
      return 0;
    }

    return gli.Length.QuadPart;
  }

  auto EnumerateVolumes(void)
  {
    char _guid[128];
    BOOL fRet = TRUE;
    char _buf[MAX_PATH];
    DWORD nReturned = 0;

    std::vector<std::vector<std::string>> out;

    HANDLE hFind = FindFirstVolumeA(_guid, sizeof(_guid) / sizeof(char));

    if (hFind == INVALID_HANDLE_VALUE)
    {
      std::cout << "FindFirstVolumeW failed, error " << GetLastError() << "\n";
      return out;
    }

    do
    {
      std::vector<std::string> names = { _guid };

      fRet = GetVolumePathNamesForVolumeNameA(
        _guid,
        _buf,
        MAX_PATH,
        &nReturned);

      if (!fRet)
      {
        std::cout << "GetVolumePathNamesForVolumeNameA failed, error " << GetLastError() << "\n";
      }

      char *path = _buf;

      while (*path)
      {
        names.push_back(path);
        path += strlen(path) + 1;
      }

      out.push_back(names);

      fRet = FindNextVolumeA(hFind, _guid, sizeof(_guid) / sizeof(char));

      if (!fRet)
      {
        if (GetLastError() != ERROR_NO_MORE_FILES) 
        {
          std::cout << "FindNextVolumeA failed, error : " << GetLastError() << "\n";
        }
      }

    } while(fRet);

    FindVolumeClose(hFind);

    return out;
  }

  auto GetFileHandleW(const std::wstring& file)
  {
    return CreateFileW(
      file.c_str(),
      GENERIC_READ|GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
  }

  auto GetFileHandle(const std::string& file)
  {
    return CreateFileA(
      file.c_str(),
      GENERIC_READ|GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
  }

  auto GetVolumeHandle(const std::string& name)
  {
    char _guid[128] = { 0 };
    HANDLE h = INVALID_HANDLE_VALUE;

    BOOL fRet = GetVolumeNameForVolumeMountPointA(
      (name.back() == '\\') ? 
         name.c_str() : (name + "\\").c_str(), 
      _guid, 
      sizeof(_guid)
    );

    if (fRet)
    {
      _guid[strlen(_guid) - 1] = '\0';

      h = GetFileHandle(_guid);
    }

    return h;
  }

  auto GetVolumeDiskExtents(const std::wstring& volume)
  {
    std::vector<int> disks;

    HANDLE hSource = GetFileHandleW(volume.c_str());

    if (hSource == INVALID_HANDLE_VALUE)
    {
      std::cout << "Failed to open source volume, error : " << GetLastError() << "\n";
      return disks;
    }

    VOLUME_DISK_EXTENTS volumeDiskExtents;
    DWORD dwBytesReturned = 0;

    BOOL fRet = DeviceIoControl(
                  hSource,
                  IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                  NULL,
                  0,
                  &volumeDiskExtents,
                  sizeof(volumeDiskExtents),
                  &dwBytesReturned,
                  NULL);

    CloseHandle(hSource);

    if (fRet == FALSE)
    {
      std::cout << "IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed on source, error : " << GetLastError() << "\n";
      return disks;
    }

    for (DWORD i = 0; i < volumeDiskExtents.NumberOfDiskExtents; i++)
    {
      PDISK_EXTENT extent = &volumeDiskExtents.Extents[i];

      disks.push_back(extent->DiskNumber);
    }

    return disks;
  }

  auto SetRegistry(HKEY hKey, std::wstring subKey, DWORD dwType, std::wstring valueName, std::wstring value)
  {
    HKEY hkResult;
    LSTATUS status;

    status = RegCreateKeyExW(
               hKey,
               subKey.c_str(),
               0,
               NULL,
               REG_OPTION_NON_VOLATILE,
               KEY_ALL_ACCESS,
               NULL,
               &hkResult,
               NULL);

    if (status != ERROR_SUCCESS)
    {
      std::cout << "RegCreateKeyExW() failed " << status << "\n";
      return false;
    }
  
    status = RegSetValueExW(
               hkResult,
               valueName.c_str(),
               0,
               dwType,
               (BYTE*)value.c_str(),
               (DWORD)value.length() * sizeof(WCHAR));

    if (status != ERROR_SUCCESS)
    {
      std::cout << "RegSetValueExW() failed " << status << "\n";
    }

    RegCloseKey(hkResult);
  
    return (status == ERROR_SUCCESS);
  }

  BSTR ReadFileAsBSTR(const std::wstring& fileName)
  {
    HANDLE hFile = GetFileHandleW(fileName.c_str());

    if (hFile == INVALID_HANDLE_VALUE)
    {
      std::cout << "CreateFile failed : "<< GetLastError() << "\n";
      return NULL;
    }

    DWORD dwFileSize = GetFileSize(hFile, 0);
    auto buf = std::make_unique<uint8_t []>(dwFileSize);

    DWORD dwRead;
    BOOL fRet = ReadFile(hFile, buf.get(), dwFileSize, &dwRead, NULL);

    if (fRet == FALSE)
    {
      return NULL;
    }

    BSTR bstrContents = SysAllocStringLen((OLECHAR *)buf.get(), dwRead);

    return bstrContents;
  }
}

#endif