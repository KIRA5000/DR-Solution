#ifndef BMR_HPP
#define BMR_HPP

#include <libvd.hpp>
#include <libvss.hpp>
#include <osl.hpp>

#include <iostream>

using namespace std::string_literals;

#define _1M 1048576

namespace BMR
{
  auto ProcessASRComponent(IVssWMComponent* component, std::vector<std::wstring>& includedDisks)
  {
    PVSSCOMPONENTINFO componentInfo;
    HRESULT hr = component->GetComponentInfo(&componentInfo);

    if (hr != S_OK)
    {
      std::cout << "GetComponentInfo failed " << std::hex << hr << "\n";
      return false;
    }

    std::wstring componentName(
      componentInfo->bstrComponentName,
      SysStringLen(componentInfo->bstrComponentName)
    );

    bool isVolume = (componentName.find(L"Volume") != std::wstring::npos);
    bool isDrive = (componentName.find(L"harddisk") != std::wstring::npos);

    if (componentInfo->bSelectable == true)
    {
      if (isVolume)
      {
        return false;
      }

      if (isDrive)
      {
        bool isIncluded = (std::find(includedDisks.begin(), includedDisks.end(), componentName) != includedDisks.end());

        if (isIncluded != true)
        {
          return false;
        }
      }
    }

    return true;
  }

  auto ReleaseTargetBlockDevice(HANDLE hTarget)
  {
    BOOL fRet = DeviceIoControl(
            hTarget,
            FSCTL_UNLOCK_VOLUME,
            NULL,
            0,
            NULL,
            0,
            NULL,
            NULL);

    if (fRet == FALSE)
    {
      std::cout << "FSCTL_UNLOCK_VOLUME failed, error " << GetLastError() << "\n";
    }

    CloseHandle(hTarget);

    return fRet;
  }

  auto InitTargetBlockDevice(HANDLE hTarget)
  {
    BOOL fRet = DeviceIoControl(
             hTarget,
             FSCTL_LOCK_VOLUME,
             NULL,
             0,
             NULL,
             0,
             NULL,
             NULL);

    if (fRet == FALSE)
    {
      std::cout << "Failed to lock target, error " << GetLastError() << "\n";
      return FALSE;
    }

    fRet = DeviceIoControl(
             hTarget,
             FSCTL_DISMOUNT_VOLUME,
             NULL,
             0,
             NULL,
             0,
             NULL,
             NULL);

    if (fRet == FALSE)
    {
      std::cout << "Failed to dismount target, error " << GetLastError() << "\n";
      return FALSE;
    }

    fRet = DeviceIoControl(
             hTarget,
             FSCTL_ALLOW_EXTENDED_DASD_IO,
             NULL,
             0,
             NULL,
             0,
             NULL,
             NULL);

    if (fRet == FALSE)
    {
      std::cout << "EXTENDED_DASD_IO failed on target, error " << GetLastError() << "\n";
      return FALSE;
    }

    return fRet;
  }

  bool ImageRecover(const std::string& file, const std::string& target)
  {
    auto disk = VHD::make_virtual_disk(file);

    if (!disk)
    {
      std::cout << "Failed to create virtaul disk object\n";
      return false;
    }

    HANDLE hTarget = OSL::GetVolumeHandle(target.c_str());

    if (hTarget == INVALID_HANDLE_VALUE)
    {
      std::cout << "Failed to open target " << target.c_str() << ", error " << GetLastError() << "\n";
      return false;
    }

    std::unique_ptr<uint8_t[]> buf;

    bool fRet = InitTargetBlockDevice(hTarget);

    if (fRet == FALSE)
    {
      std::cout << "InitTargetBlockDevice failed\n";
      goto _end;
    }

    uint64_t nDone = 0;
    DWORD dwBytesWritten;
    auto bs = disk->GetBlockSize();
    buf = std::make_unique<uint8_t []>(bs);

    uint64_t nTotal = disk->GetPartitionLength(0);
    uint64_t offset = disk->GetPartitionStartOffset(0);

    do
    {
      auto pending = nTotal - nDone;

      size_t nToRead = pending > bs ? bs : pending;

      auto n = disk->ReadSync(buf.get(), nToRead, offset + nDone);

      fRet = false;

      if (n != nToRead)
      {
        std::cout << "ReadSync on source returned " << n << "\n";
        break;
      }

      fRet = WriteFile(hTarget, buf.get(), n, &dwBytesWritten, NULL);

      if(!fRet)
      {
        std::cout << "WriteFile on target failed : " << GetLastError();
        break;
      }

      nDone += n;

      std::cout << "n : " << n << " nDone : " << nDone << "\n";
  
    } while (nDone != nTotal);

    std::cout << "nTotal : " << nTotal << "\n";

    _end:

    if (hTarget != INVALID_HANDLE_VALUE)
    {
      ReleaseTargetBlockDevice(hTarget);
    }

    return fRet;
  }

  bool BlockCopy(const std::string& source, const std::string& target) 
  {
    BOOL fRet = FALSE;
    std::unique_ptr<uint8_t[]> _buf;
    HANDLE hSource = INVALID_HANDLE_VALUE;
    HANDLE hTarget = INVALID_HANDLE_VALUE;

    hSource = OSL::GetVolumeHandle(source.c_str());

    if (hSource == INVALID_HANDLE_VALUE)
    {
      std::cout << "Failed to open source volume, error : " << GetLastError() << "\n";
      goto _end;
    }

    fRet = DeviceIoControl(
             hSource,
             FSCTL_ALLOW_EXTENDED_DASD_IO,
             NULL,
             0,
             NULL,
             0,
             NULL,
             NULL);

    if (fRet == FALSE)
    {
      std::cout << "EXTENDED_DASD_IO failed on source, error : " << GetLastError() << "\n";
      goto _end;
    }

    hTarget = OSL::GetVolumeHandle(target.c_str());

    if (hTarget == INVALID_HANDLE_VALUE)
    {
      std::cout << "Failed to open target " << target.c_str() << ", error " << GetLastError() << "\n";
      goto _end;
    }

    fRet = InitTargetBlockDevice(hTarget);

    if (fRet == FALSE)
    {
      std::cout << "InitTargetBlockDevice failed\n";
      goto _end;
    }

    _buf = std::make_unique<uint8_t []>(_1M);

    DWORD dwBytesWritten = 0;
    DWORD lpNumberOfBytesRead = 0;

    do
    {
      fRet = ReadFile(hSource, _buf.get(), _1M, &lpNumberOfBytesRead, NULL);

      if (fRet)
      {
        fRet = WriteFile(hTarget, _buf.get(), lpNumberOfBytesRead, &dwBytesWritten, NULL);

        if (fRet == FALSE)
        {
          std::cout << "WriteFile on target failed, error : " << GetLastError() << "\n";
        }
      }
      else
      {
        std::cout << "ReadFile on source failed, error : " << GetLastError() << "\n";
      }

      if (lpNumberOfBytesRead < _1M) break;

    } while (fRet);

    _end:

    if (hSource != INVALID_HANDLE_VALUE)
    {
      CloseHandle(hSource);
    }

    if (hTarget != INVALID_HANDLE_VALUE)
    {
      ReleaseTargetBlockDevice(hTarget);
    }

    return fRet;
  }

  auto RestoreOperation(CComPtr<IVssBackupComponents> backupComponent, const std::string& backupPath)
  {
    CComPtr<IVssAsync> async;
    std::vector<std::string> restored;

    //PreRestore() formats the include disks
    HRESULT hr = backupComponent->PreRestore(&async);

    if (hr != S_OK)
    {
      std::cout << "PreRestore failed " << std::hex << hr << "\n";
      return restored;
    }

    std::cout << "Waiting for PreRestore to finish ...\n";

    hr = vss::AsyncWait(async);

    if (hr != S_OK)
    {
      std::cout << "AsyncWait failed for PreRestore " << std::hex << hr << "\n";
      return restored;
    }

    std::string choice;

    for (const auto& file : std::filesystem::directory_iterator(backupPath))
    {
      std::string fileName = (file.path()).string();

      bool isBackupFile = ((fileName.find("Volume") != std::string::npos) && (fileName.find(".vhd") != std::string::npos));

      char choice;

      if (isBackupFile)
      {
        std::cout << "\n";
        std::cout << "BackupFile found: " << fileName << "\n";
        std::cout << "Do you wanna restore this vhd ? [Y/N]";

        std::cin >> choice;

        if ((choice != 'n') && (choice != 'N'))
        {
          auto index = fileName.find("Volume");

          auto target = "\\\\?\\" + fileName.substr(index, fileName.size() - index - 4) + "\\";

          auto vHandle = OSL::GetVolumeHandle(target);

          if (vHandle != INVALID_HANDLE_VALUE)
          {
            CloseHandle(vHandle);

            std::cout << "Restoring volume: " << target << "\n";
            
            ImageRecover(fileName, target);
            
            restored.push_back(target);
          }
          else
          {
            std::cout << "No such volume exist: " << target << "\n";
            std::cout << "Skipping vhd: " << fileName << "\n";
          }
        }
      }
    }

    std::cout << "Restore completed\n";

    return restored;
  }

  auto Backup(const std::string& level, const std::string& destination)
  {
    std::vector<std::wstring> args;
    args.push_back(L"-wm2");

    std::vector<std::wstring> snapset, rawset, includedDisks;

    vss::Execute(
      args,
      [&](auto writer, auto metadata, auto component)
      {
        bool fRet = true;

        if (writer == L"ASR Writer")
        {
          PVSSCOMPONENTINFO componentInfo;
          HRESULT hr = component->GetComponentInfo(&componentInfo);

          if (hr != S_OK)
          {
            std::cout << "GetComponentInfo failed " << std::hex << hr << "\n";
            return false;
          }

          std::wstring componentName(
            componentInfo->bstrComponentName,
            SysStringLen(componentInfo->bstrComponentName)
          );

          bool isVolume = (componentName.find(L"Volume") != std::wstring::npos);

          if (isVolume == true)
          {
            if (componentInfo->bSelectable == false)
            {
              std::wstring volumePath(L"\\\\?\\" + componentName);

              auto disks = OSL::GetVolumeDiskExtents(volumePath);

              for (int n : disks)
              {
                includedDisks.push_back(L"harddisk" + std::to_wstring(n));
              }

              volumePath += L"\\";

              BOOL isSupported = vss::IsVolumeSupported(volumePath);

              if (isSupported == TRUE)
              {
                snapset.push_back(volumePath);
              }
              else
              {
                rawset.push_back(volumePath);
              }
            }
          }
        }
        return fRet;
      }
    );

    args.clear();

    std::wstring target(destination.begin(), destination.end());

    args.push_back(L"-bc=" + target + L"\\bc.xml" );

    for (auto& v : snapset)
    {
      args.push_back(v);
    }

    vss::Execute(
      args,
      [&](auto writer, auto metadata, auto component)
      {
        bool fRet = true;

        if (writer == L"ASR Writer")
        {
          return ProcessASRComponent(component, includedDisks);
        }

        return fRet;
      },
      [&](auto prop)
      {
        std::string deviceName = vss::WString2String(prop.m_pwszSnapshotDeviceObject);

        std::string volumeName = vss::WString2String(prop.m_pwszOriginalVolumeName);

        VHD::create_base_vhd(
          deviceName,
          destination + "\\" + volumeName.substr(4, volumeName.size() - 5) + ".vhd"
          );

      }
    );

    // todo: call image recovery for raw set volumes

    return true;
  }

  auto Restore(const std::string& bcdFile, const std::string& backupPath)
  {
    HRESULT hr = E_ABORT;
    std::wstring volumes;
    std::vector<std::string> restored;

    CComPtr<IVssAsync> async;
    CComPtr<IVssBackupComponents> backupComponent;

    BSTR document = OSL::ReadFileAsBSTR(std::wstring(bcdFile.begin(), bcdFile.end()));

    if (document == NULL)
    {
      goto end;
    }
  
    backupComponent = vss::InitializationStage(true, document);

    if (backupComponent == NULL)
    {
      goto end;
    }

    restored = RestoreOperation(backupComponent, backupPath + "\\");

    if (restored.size() == 0)
    {
      goto end;
    }

    UUID id;
    WCHAR* strId;

    UuidCreate(&id);
    UuidToStringW(&id, (RPC_WSTR*)&strId);

    bool fRet = OSL::SetRegistry(
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ASR\\RestoreSession",
      REG_SZ,
      L"LastInstance",
      strId);

    if (fRet != true)
    {
      goto end;
    }
    
    for (auto& v : restored)
    {
      volumes +=  std::wstring(v.begin(), v.end()) + L'\0';
    }
    
    fRet = OSL::SetRegistry(
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ASR\\RestoreSession",
      REG_MULTI_SZ,
      L"RestoredVolumes",
      volumes);
    
    if (fRet != true)
    {
      goto end;
    }
    
    hr = backupComponent->PostRestore(&async);

    if (hr != S_OK)
    {
      std::cout << "PostRestore failed " << std::hex << hr << "\n";
      goto end;
    }

    hr = vss::AsyncWait(async);

    if (hr != S_OK)
    {
      std::cout << "AsyncWait failed for PostRestore " << std::hex << hr << "\n";
    }

    end:

    CoUninitialize();

    return (hr != S_OK) ? false : true;
  }
}

#endif