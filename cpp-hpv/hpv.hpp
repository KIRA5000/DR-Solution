#ifndef HPV_HPP
#define HPV_HPP

#include <osl.hpp>
#include <libvss.hpp>

#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace HPV
{
  auto GetVolumeMountPoint(const std::wstring& str)
  {
    DWORD bLen = 3;
    DWORD retLen;
    std::wstring mountPoint(3, L'\0');

    bool fRet = GetVolumePathNamesForVolumeNameW(
      str.c_str(),
      (LPWCH)mountPoint.c_str(),
      bLen,
      &retLen
    );

    return mountPoint;
  }

  auto Backup(const std::wstring& vmName, const std::wstring& destination)
  {
    std::set<std::wstring> snapshotSet;
    std::map<std::wstring, std::wstring> backupList, rawBackupList;

    std::vector<std::wstring> args;
    args.push_back(L"-wm2");

    vss::Execute(
      args,
      [&](auto writer, auto metadata, auto component)
      {
        if (writer == L"Microsoft Hyper-V VSS Writer")
        {
          PVSSCOMPONENTINFO componentInfo;
          HRESULT hr = component->GetComponentInfo(&componentInfo);

          if (hr != S_OK)
          {
            std::cout << "GetComponentInfo failed " << std::hex << hr << "\n";
            return false;
          }

          std::wstring caption(
            componentInfo->bstrCaption,
            SysStringLen(componentInfo->bstrCaption)
          );

          bool isTargetVm = (caption.find(vmName) != std::wstring::npos);

          if (isTargetVm)
          {
            CComPtr<IVssWMFiledesc> file;

            for (UINT i = 0; i < componentInfo->cFileCount; i++)
            {
              hr = component->GetFile(i, &file);

              if (hr != S_OK)
              {
                std::cout << "GetFile failed " << std::hex << hr << "\n";
                return false;
              }

              BSTR filePath;
              BSTR fileName;

              hr = file->GetPath(&filePath);

              if (hr != S_OK)
              {
                std::cout << "GetPath failed " << std::hex << hr << "\n";
                return false;
              }

              hr = file->GetFilespec(&fileName);

              if (hr != S_OK)
              {
                std::cout << "GetFilespec failed " << std::hex << hr << "\n";
                return false;
              }

              std::wstring wFilePath(
                filePath,
                SysStringLen(filePath)
              );

              if (wFilePath.back() != L'\\')
              {
                wFilePath += L"\\";
              }

              BOOL bSnapshotable = vss::IsVolumeSupported(wFilePath.substr(0, 3));

              if (bSnapshotable == TRUE)
              {
                snapshotSet.insert(wFilePath.substr(0, 3));

                backupList.insert({std::wstring(fileName, SysStringLen(fileName)), wFilePath});
              }
              else
              {
                rawBackupList.insert({std::wstring(fileName, SysStringLen(fileName)), wFilePath});
              }
            }
          }
        }

        return true;
      }
    );

    std::wofstream backupListFile;
    backupListFile.open(destination + L"\\filemap.txt");

    for (auto& it : backupList)
    {
      backupListFile << it.first + L"  " + it.second << std::endl;
    }

    backupListFile.close();

    args.clear();

    args.push_back(L"-bc=" + destination + L"\\bc.xml");

    for (auto& vol : snapshotSet)
    {
      args.push_back(vol);
    }

    vss::Execute(
      args,
      [&](auto writer, auto metadata, auto component)
      { 
        if (writer == L"Microsoft Hyper-V VSS Writer")
        {
          PVSSCOMPONENTINFO componentInfo;
          HRESULT hr = component->GetComponentInfo(&componentInfo);

          if (hr != S_OK)
          {
            std::cout << "GetComponentInfo failed " << std::hex << hr << "\n";
            return false;
          }

          std::wstring caption(
            componentInfo->bstrCaption,
            SysStringLen(componentInfo->bstrCaption)
          );

          return (caption.find(vmName) != std::wstring::npos);
        }

        return false;
      },
      [&](auto prop)
      {
        BOOL fRet;

        std::wstring volume = GetVolumeMountPoint(prop.m_pwszOriginalVolumeName);

        std::wstring deviceName = prop.m_pwszSnapshotDeviceObject;

        auto deviceIndex = deviceName.find(L"HarddiskVolumeShadowCopy");

        for (auto& it : backupList)
        {
          auto fileName = it.first;
          auto filePath = it.second;

          if (filePath.find(volume) != std::wstring::npos)
          {
            std::wstring source = L"\\\\?\\" + deviceName.substr(deviceIndex, deviceName.size() - deviceIndex) + L"\\" + filePath.substr(3, filePath.size() - 3) + fileName;
            std::wstring target = destination + L"\\" + fileName;

            if (fileName.find(L"*") == std::wstring::npos)
            {
              std::wcout << L"Backing up file: " << source << L"\n";

              fRet = CopyFileW(source.c_str(), target.c_str(), FALSE);

              if (fRet != TRUE)
              {
                std::cout << "CopyFileW failed " << GetLastError() << "\n";
              }
            }
            else
            {
              std::wcout << L"Skipping file: " << source << L"\n";
            }
          }
        }
      }

      // todo: backup for files in rawBackupList.
    );

    return 0;
  }

  auto Restore(const std::wstring& bcdFile, const std::wstring& mapFile, const std::wstring& backupPath)
  {
    std::vector<std::wstring> args;

    args.push_back(L"-r=" + bcdFile);

    vss::Execute(
      args,
      nullptr,
      nullptr,
      [&]()
      {
        std::wifstream wif(mapFile);

        if (wif.is_open())
        {
          std::wstring wline;
    
          while (std::getline(wif, wline))
          {
            auto& value = wsplit(wline, L"  ");

            if(value[0] != L"*")
            {
              std::wstring source = backupPath + L"\\" + value[0];
              std::wstring target = value[1] + value[0];

              std::wcout << L"Restoring File: " << value[0] << L"\n";

              BOOL fRet = CopyFileW(source.c_str(), target.c_str(), false);

              if (fRet == FALSE)
              {
                std::cout << "CopyFileW failed " << GetLastError() << "\n";
                return false;
              }
            }
          }
          
          wif.close();
        }

        return true;
      }
    );
    
    return 0;
  }

}

#endif