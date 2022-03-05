#ifndef VSS_HPP
#define VSS_HPP

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <atlbase.h>

#include <any>
#include <string>
#include <iostream>
#include <functional>

using TCBK = std::function<bool (std::any data)>;
using TSnapshotCBK = std::function<void (VSS_SNAPSHOT_PROP)>;
using TComponentCBK = std::function<bool (std::wstring, IVssExamineWriterMetadata *, IVssWMComponent *)>;
using TRestoreCBK = std::function<void (void)>;

int __cdecl vshadow_wmain(std::vector<std::wstring>& arguments, TCBK cbk);

namespace vss
{
  auto Execute(
    std::vector<std::wstring>& arguments, 
    TComponentCBK OnComponent,
    TSnapshotCBK OnSnapshot = nullptr,
    TRestoreCBK OnRestore = nullptr)
  {
    return vshadow_wmain(
      arguments,
      [OnComponent, OnSnapshot, OnRestore](std::any data)
      {
        if (!data.has_value())
        {
          return false;
        }

        try
        {
          auto pair = std::any_cast<
            std::pair<
             IVssExamineWriterMetadata *,
             IVssWMComponent *
            >
          >(data);

          if (OnComponent)
          {
            CComBSTR bstrWriterName;
            VSS_ID idWriter = GUID_NULL;
            VSS_ID idInstance = GUID_NULL;
            VSS_USAGE_TYPE usage = VSS_UT_UNDEFINED;
            VSS_SOURCE_TYPE source= VSS_ST_UNDEFINED;

            pair.first->GetIdentity (
              &idInstance,
              &idWriter,
              &bstrWriterName,
              &usage,
              &source
            );

            std::wstring writer(
              bstrWriterName,
              SysStringLen(bstrWriterName)
            );

            return OnComponent(writer, pair.first, pair.second);
          }

          return true;
        }
        catch(const std::bad_any_cast& e)
        {
          std::cout << e.what() << " " << data.type().name() << "\n";
        }

        try
        {
          auto prop = std::any_cast<VSS_SNAPSHOT_PROP>(data);

          if (OnSnapshot)
          {
            OnSnapshot(prop);
          }

          return true;
        }
        catch (const std::bad_any_cast& e)
        {
          std::cout << e.what() << " " << data.type().name() << "\n";
        }

        try
        {
          auto prop = std::any_cast<bool>(data);

          if (OnRestore)
          {
            OnRestore();
          }

          return true;
        }
        catch (const std::bad_any_cast& e)
        {
          std::cout << e.what() << " " << data.type().name() << "\n";
        }

        return false;
      }
    );
  }

  auto IsVolumeSupported(const std::wstring& volumePath)
  { 
    CComPtr<IVssBackupComponents> backupComponent;

    HRESULT hr = CreateVssBackupComponents(&backupComponent);

    if (hr != S_OK)
    {
      std::cout << "CreateVssBackupComponent failed " << std::hex << hr << "\n";
      return FALSE;
    }

    hr = backupComponent->InitializeForBackup();

    if (hr != S_OK)
    {
      std::cout << "InitializeForBackup failed " << std::hex << hr << "\n";
      return FALSE;
    }

    BOOL isVolumeSupported = FALSE;

    backupComponent->IsVolumeSupported(GUID_NULL, (wchar_t *)volumePath.c_str(), &isVolumeSupported);

    if (hr != S_OK)
    {
      std::cout << "IsVolumeSupported failed " << std::hex << hr << "\n";
      return FALSE;
    }

    return isVolumeSupported;
  }

  auto WString2String(const std::wstring& src)
  {
    std::vector<CHAR> chBuffer;
    int iChars = WideCharToMultiByte(CP_ACP, 0, src.c_str(), -1, NULL, 0, NULL, NULL);
    if (iChars > 0)
    {
      chBuffer.resize(iChars);
      WideCharToMultiByte(CP_ACP, 0, src.c_str(), -1, &chBuffer.front(), (int)chBuffer.size(), NULL, NULL);
    }
    
    return std::string(&chBuffer.front());
  }

  HRESULT AsyncWait(CComPtr<IVssAsync> async)
  {
    HRESULT hr = async->Wait();

    if (hr != S_OK)
    {
      std::cout << "Wait failed " << std::hex << hr << "\n";
      return hr;
    }

    HRESULT status;

    hr = async->QueryStatus(&status, NULL);

    if ((hr != S_OK) && (status != VSS_S_ASYNC_FINISHED))
    {
      std::cout << "QueryStatus failed " << std::hex << hr << "\n";
    }

    return hr;
  }

  auto InitializationStage(bool bForRestore = true, BSTR document = NULL)
  {
    CComPtr<IVssBackupComponents> backupComponent;

    HRESULT hr = CoInitialize(NULL);

    if (hr != S_OK)
    {
      std::cout << "CoInitialize failed " << std::hex << hr << std::endl;
      goto end;
    }

    hr = CoInitializeSecurity(
          NULL,
          -1,
          NULL,
          NULL,
          RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
          RPC_C_IMP_LEVEL_IMPERSONATE,
          NULL,
          EOAC_DYNAMIC_CLOAKING,
          NULL);

    if (hr != S_OK)
    {
      std::cout << "CoInitializeSecurity failed " << std::hex << hr << std::endl;
      goto end;
    }

    hr = CreateVssBackupComponents(&backupComponent);

    if (hr != S_OK)
    {
      std::cout << "CreateVssBackupComponents failed " << std::hex << hr << "\n";
      goto end;
    }

    if (bForRestore)
    {
      hr = backupComponent->InitializeForRestore(document);
    }
    else
    {
      hr = backupComponent->InitializeForBackup(document);
    }

    if (hr != S_OK)
    {
      std::cout << "InitializeFor" << (bForRestore ? "Restore" : "Backup") << " failed " << std::hex << hr << "\n";
    }

    end:

    if (hr != S_OK)
    {
      backupComponent.Release();
    }

    return backupComponent;
  }

  std::vector<std::wstring> GetComponentFileList(CComPtr<IVssWMComponent> component)
  {
    std::vector<std::wstring> files;
    CComPtr<IVssWMFiledesc> fileDesc;

    PVSSCOMPONENTINFO componentInfo;
    HRESULT hr = component->GetComponentInfo(&componentInfo);

    if (hr != S_OK)
    {
      std::cout << "GetComponentInfo failed " << std::hex << hr << "\n";
      return files;
    }

    std::wstring componentName(
      componentInfo->bstrComponentName,
      SysStringLen(componentInfo->bstrComponentName)
    );

    for (UINT i = 0; i < componentInfo->cFileCount; i++)
    {
      hr = component->GetFile(i, &fileDesc);

      if (hr != S_OK)
      {
        std::cout << "GetFile failed " << std::hex << hr << "\n";
        return {};
      }

      BSTR filePath;
      BSTR fileName;

      hr = fileDesc->GetPath(&filePath);

      if (hr != S_OK)
      {
        std::cout << "GetPath failed " << std::hex << hr << "\n";
        return {};
      }

      hr = fileDesc->GetFilespec(&fileName);

      if (hr != S_OK)
      {
        std::cout << "GetFilespec failed " << std::hex << hr << "\n";
        return {};
      }

      std::wstring wFilePath(
        filePath,
        SysStringLen(filePath)
      );

      if (wFilePath.back() != L'\\')
      {
        wFilePath += L"\\";
      }

      files.push_back(wFilePath + std::wstring(fileName, SysStringLen(fileName)));
    }

    return files;
  }
}

#endif