#ifndef SQL_HPP
#define SQL_HPP

#include <set>
#include <fstream>
#include <iostream>
#include <filesystem>

#include <vdi.h>
#include <process.h>
#include <vdierror.h>
#include <vdiguid.h>

#include <osl.hpp>
#include <libvss.hpp>

namespace SQL
{
  auto CurrentTime()
  {
    time_t rawtime;
    struct tm timeinfo;
    char buffer[80];

    time(&rawtime);

    auto err = localtime_s(&timeinfo, &rawtime);

    if (err != 0)
    {
      std::cout << "localtime_s failed " << err << "\n";
      return std::string();
    }

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

    return std::string(buffer);
  }

  auto GetFileLines(const std::wstring& fileName)
  {
    std::wifstream wif(fileName);

    std::vector<std::wstring> lines;

    if (wif.is_open())
    {
      std::wstring wline;

      while (std::getline(wif, wline))
      {
        lines.push_back(wline);
      }
    }
    else
    {
      std::cout << "Failed to open file " << GetLastError() << "\n";
    }
    
    return lines;
  }

  auto PerformTransfer(bool isBackup, IClientVirtualDevice* vDevice, const std::wstring& fileName)
  {
    HRESULT hr;
    FILE* fHandle;
    VDC_Command* cmd;
    DWORD completionCode;
    size_t bytesTransferred;

    auto ret = _wfopen_s(&fHandle, fileName.c_str(), (isBackup) ? L"wb" : L"rb");

    if (ret != 0)
    {
      std::cout << "_wfopen_s failed " << errno << "\n";
      return false;
    }

    if (fHandle == NULL)
    {
      std::wcout << L"Failed to get file handle " << fileName << L"\n";
      return false;
    }

    while (1)
    {
      hr = vDevice->GetCommand(INFINITE, &cmd);

      if (SUCCEEDED(hr))
      {
        bytesTransferred = 0;

        if (cmd->commandCode == VDC_Read)
        {
          bytesTransferred = fread(cmd->buffer, 1, cmd->size, fHandle);

          if (bytesTransferred == cmd->size)
          {
            completionCode = ERROR_SUCCESS;
          }
          else
          {
            completionCode = ERROR_HANDLE_EOF;
          }
        }
        else if (cmd->commandCode == VDC_Write)
        {
          bytesTransferred = fwrite(cmd->buffer, 1, cmd->size, fHandle);

          if (bytesTransferred == cmd->size )
          {
            completionCode = ERROR_SUCCESS;
          }
          else
          {
            completionCode = ERROR_DISK_FULL;
          }
        }
        else if (cmd->commandCode == VDC_Flush)
        {
          fflush(fHandle);

          completionCode = ERROR_SUCCESS;
        }
        else if (cmd->commandCode == VDC_ClearError)
        {
          completionCode = ERROR_SUCCESS;
        }
        else
        {
          completionCode = ERROR_NOT_SUPPORTED;
        }

        hr = vDevice->CompleteCommand(cmd, completionCode, (DWORD)bytesTransferred, 0);

        if (!SUCCEEDED(hr))
        {
          std::cout << "CompleteCommand failed " << std::hex << hr << "\n";
          break;
        }
      }
      else
      {
        break;
      }
    }

    fclose(fHandle);

    return true;
  }

  auto ExecSQL(bool isBackup, bool isLog, const std::wstring& dbName, const std::wstring& vdName, const std::wstring& flag)
  {
    std::wstring sqlCommand = L"osql -E -b ";

    sqlCommand += (isBackup) ? L"-Q\"Backup " : L"-Q\"Restore ";
    sqlCommand += (isLog) ? L"Log " : L"Database "; 
    sqlCommand += dbName;
    sqlCommand += (isBackup) ? L" To " : L" From ";
    sqlCommand += L" VIRTUAL_DEVICE='" + vdName + L"' ";
    sqlCommand += flag;
    sqlCommand += L"\"";

    std::wcout << L"Executing query: " << sqlCommand << L"\n";

    auto hProcess = _wspawnlp( _P_NOWAIT, L"osql", sqlCommand.c_str(), NULL);

    if (hProcess == -1)
    {
      std::cout << "Spawn failed with error: " << errno << "\n";
    }

    return hProcess;
  }

  auto MainRoutine(bool isBackup, bool isLog, const std::wstring& dbName, const std::wstring& fileName, const std::wstring& flag)
  {
    HRESULT hr;
    int termCode;
    VDConfig config;
    intptr_t hProcess;
    WCHAR wVdsName[50];
    IClientVirtualDevice* vd=NULL;
    IClientVirtualDeviceSet2* vds = NULL ; 
 
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (!SUCCEEDED(hr))
    {
      std::cout << "CoInitializeEx fails " << std::hex << hr << "\n";
      return E_ABORT;
    }

    hr = CoCreateInstance( 
      CLSID_MSSQL_ClientVirtualDeviceSet,  
      NULL, 
      CLSCTX_INPROC_SERVER,
      IID_IClientVirtualDeviceSet2,
      (void**)&vds
    );

    if (!SUCCEEDED(hr))
    {
      std::cout << "CoCreateInstance failed " << std::hex << hr << "\n";
      goto exit;
    }

    memset(&config, 0, sizeof(config));
    config.deviceCount = 1;

    GUID	vdsId;
    CoCreateGuid(&vdsId);
    StringFromGUID2(vdsId, wVdsName, 49);

    hr = vds->CreateEx(NULL, wVdsName, &config);

    if (!SUCCEEDED(hr))
    {
      std::cout << "CreateEx fails " << std::hex << hr << "\n";
      goto exit;
    }

    hProcess = ExecSQL(isBackup, isLog, dbName, wVdsName, flag);
    
    if (hProcess == -1)
    {
      std::cout << "ExecSQL failed.\n";
      hr = E_ABORT;
      goto shutdown;
    }

    hr = vds->GetConfiguration(10000, &config);

    if (!SUCCEEDED(hr))
    {
      std::cout << "GetConfiguration failed " << std::hex << hr << "\n";

      if (hr == VD_E_TIMEOUT)
      {
        std::cout << "Timed out\n";
      }

      goto shutdown;
    }

    hr = vds->OpenDevice(wVdsName, &vd);

    if (!SUCCEEDED(hr))
    {
      std::cout << "OpenDevice failed " << std::hex << hr << "\n";
      goto shutdown;
    }

    std::cout << "\nPerforming data transfer...\n";
    
    bool ret = PerformTransfer(isBackup, vd, fileName);

    if (!ret)
    {
      std::cout << "PerformTransfer failed\n";
      hr = E_ABORT;
    }

shutdown:
 
    vds->Close();

    hProcess = _cwait(&termCode, hProcess, NULL);
    
    if (termCode == 0)
    {
      std::cout << "\nThe SQL command executed successfully.\n";
    }
    else
    {
      std::cout << "\nThe SQL command failed.\n";
      hr = E_ABORT;
    }

    vds->Release();

exit:

    CoUninitialize();

    return hr;
  }

  auto DumpBackups(const std::wstring& dbName)
  {
    int index = 1;
    std::wstring fileName = dbName + L"_log.txt";

    auto lines = GetFileLines(fileName);

    std::cout << "\n";

    for (auto& line : lines)
    {
      auto data = wsplit(line, L",");

      std::wcout << index++ << L". " << data[0] << L"  " << data[1] << L"  " << data[2] << L"\n";
    }

    return 0;
  }

  auto BackupVDI(const std::wstring& type, const std::wstring& dbName, const std::wstring& fileName)
  {
    std::wstring flag;

    if (type == L"diff")
    {
      flag = L"with differential";
    }
    else if (type == L"tail")
    {
      flag = L"with norecovery";
    }

    bool isLog = ((type == L"log") || (type == L"tail"));

    std::wcout << L"\nBacking up " << type << L" of: " << dbName << L"\n";

    auto timestamp = CurrentTime();

    if (timestamp.size() == 0)
    {
      std::cout << "CurrentTime failed\n";
      return false;
    }
    
    auto hr = MainRoutine(true, isLog, dbName, fileName, flag);

    if (!SUCCEEDED(hr))
    {
      std::cout << "MainRoutine failed " << std::hex << hr << "\n";
      return SUCCEEDED(hr);
    }

    std::wstring data;
    
    data += type + L",";
    
    data += fileName + L",";
    
    data += std::wstring(timestamp.begin(), timestamp.end());
    
    std::wofstream logFile;
    logFile.open(dbName + L"_log.txt", std::ios_base::app);
    
    logFile << data << std::endl;
    
    logFile.close();

    return SUCCEEDED(hr);
  }

  auto RestoreVDI(const std::wstring& dbName, int index)
  {
    HRESULT hr;
    std::wstring logFile = dbName + L"_log.txt";

    auto lines = GetFileLines(logFile);

    bool isFullFound = false;
    bool isDiffFound = false;
    std::vector<std::wstring> logsToRestore;

    for (int i = index - 1; i >= 0; i--)
    {
      auto log = lines[i];

      if (isFullFound)
      {
        break;
      }
      else if (isDiffFound)
      {
        if (log.find(L"full") == std::wstring::npos)
        {
          continue;
        }
        
        logsToRestore.push_back(log);

        isFullFound = true;
      }
      else
      {
        logsToRestore.push_back(log);

        if (log.find(L"diff") != std::wstring::npos)
        {
          isDiffFound = true;
        }

        if (log.find(L"full") != std::wstring::npos)
        {
          isFullFound = true;
        }
      }
    }

    for (auto& log = logsToRestore.rbegin(); log != logsToRestore.rend(); log++)
    {
      auto data = wsplit(*log, L",");

      bool isLog = false;
      std::wstring flag = L" with norecovery";
      
      if ((data[0] == L"log") || (data[0]) == L"tail")
      {
        isLog = true;
      }
      
      if (log == (logsToRestore.rend() - 1))
      {
        flag = L" with recovery";

        if (isLog)
        {
          std::string choice;

          std::wcout << L"Do you wanna recover at: " << data[2] << L"?[Y,N] ";
          std::cin >> choice;

          if (choice == "N")
          {
            std::wcin.clear();
            std::wcin.ignore(INT_MAX, '\n');

            std::wstring restoreTime;

            std::cout << "TimeStamp: ";

            std::getline(std::wcin, restoreTime);

            flag = L" with STOPAT='" + restoreTime + L"', recovery";
          }
        }
      }

      std::wcout << L"\nRestoring: " << data[1] << L"\n";

      hr = MainRoutine(false, isLog, dbName, data[1], flag);

      if (!SUCCEEDED(hr))
      {
        std::cout << "MainRoutine failed " << std::hex << hr << "\n";
        break;
      }
    }

    return SUCCEEDED(hr);
  }

  auto BackupVSS(const std::wstring& dbName, const std::wstring& destination)
  {
    std::set<std::wstring> snapshotSet;
    std::vector<std::wstring> backupList, rawBackupList;

    std::vector<std::wstring> args;
    args.push_back(L"-wm2");

    vss::Execute(
      args,
      [&](auto writer, auto metadata, auto component)
      {
        if (writer == L"SqlServerWriter")
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

          bool isTargetDb = (componentName.find(dbName) != std::wstring::npos);

          if (isTargetDb)
          {
            auto files = vss::GetComponentFileList(component);

            if (files.size() == 0)
            {
              std::cout << "GetComponentFileList failed\n";
              return false;
            }

            for (auto& file : files)
            {
              BOOL bSnapshotable = vss::IsVolumeSupported(file.substr(0, 3));

              if (bSnapshotable == TRUE)
              {
                snapshotSet.insert(file.substr(0, 3));

                backupList.push_back(file);
              }
              else
              {
                rawBackupList.push_back(file);
              }
            }
          }
        }

        return false;
      }
    );

    std::wofstream backupListFile;
    backupListFile.open(destination + L"\\filelist.txt");

    for (auto& it : backupList)
    {
      backupListFile << it << std::endl;
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
        if (writer == L"SqlServerWriter")
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

          return (componentName.find(dbName) != std::wstring::npos);
        }

        return false;
      },
      [&](auto prop)
      {
        BOOL fRet;

        std::vector<std::string> volumePaths;
        auto volumeList = OSL::EnumerateVolumes();

        for (auto& volume : volumeList)
        {
          if (prop.m_pwszOriginalVolumeName == std::wstring(volume[0].begin(), volume[0].end()))
          {
            volumePaths = volume;
          }
        }

        std::filesystem::path deviceName(prop.m_pwszSnapshotDeviceObject);

        for (auto& it : backupList)
        {
          for (size_t i = 1; i < volumePaths.size(); i++)
          {
            std::wstring volumePath(volumePaths[i].begin(), volumePaths[i].end());

            if (it.find(volumePath) != std::wstring::npos)
            {
              std::filesystem::path file(it);

              std::wstring source = L"\\\\?\\" + deviceName.filename().wstring() + L"\\" + file.wstring().substr(volumePath.size());
              std::wstring target = destination + L"\\" + file.filename().wstring();

              std::wcout << L"Backing up file: " << source << L"\n";

              fRet = CopyFileW(source.c_str(), target.c_str(), FALSE);

              if (fRet != TRUE)
              {
                std::cout << "CopyFileW failed " << GetLastError() << "\n";
              }            
            }
          }
        }
      }
    );

    return 0;
  }

  auto RestoreVSS(const std::wstring& bcdFile, const std::wstring& backupListFile, const std::wstring& backupPath)
  {
    std::vector<std::wstring> args;
    args.push_back(L"-r=" + bcdFile);

    vss::Execute(
      args,
      nullptr,
      nullptr,
      [&]()
      {
        std::wifstream wif(backupListFile);

        if (wif.is_open())
        {
          std::wstring wline;
  
          while (std::getline(wif, wline))
          {
            std::filesystem::path file(wline);

            std::cout << "Restoring File: " << file.filename().string() << "\n";

            std::wstring src = backupPath + L"\\" + file.filename().wstring();

            BOOL fRet = CopyFileW(src.c_str(), wline.c_str(), FALSE);

            if (fRet != TRUE)
            {
              std::cout << "CopyFileW failed " << GetLastError() << "\n";
              return false;
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