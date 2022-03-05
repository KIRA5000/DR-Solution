#ifndef RCT_HPP
#define RCT_HPP

#include <db.hpp>
#include <export.hpp>
#include <checkpoint.hpp>

namespace RCT
{
  bool rct_backup(const VMBackupJob& job, const std::string& lastReferencePointID)
  {
    auto& [vm, level, destination, consistency] = job;

    auto checkpoint = CheckPointVM(vm, consistency);

    if (!checkpoint.size())
    {
      return false;
    }

    bool fRet = ExportCheckpoint(job, checkpoint, lastReferencePointID);

    if (!fRet)
    {
      return false;
    }

    auto rctid = CheckPointToRefrencePoint(vm, checkpoint);

    if (!rctid.size())
    {
      return false;
    }

    fRet = SetBackupMetadata(job, rctid);

    if (!fRet)
    {
      return false;
    }

    return true;
  }

  void CreateRCTBackup(const VMBackupJob& job)
  {
    auto& [vm, level, destination, consistency] = job;

    bool fRet = false;

    auto& [out, err] = PSExecuteCommand(std::string("Test-Path ") + destination.c_str());

    if (out == "True")
    {
      std::string lastReferencePointID;

      if (level == "incr")
      {
        lastReferencePointID = GetLastReferencePointID(vm);

        if (!lastReferencePointID.size())
        {
          std::cout << "CreateRCTBackup Error : reference point not available for incremental backup.\n";
          return;
        }
      }

      fRet = rct_backup(job, lastReferencePointID);
    }
    else
    {
      std::cout << "CreateRCTBackup Error : destination path does not exist.\n";
    }

    std::cout << "CreateRCTBackup " << (fRet ? "Successful" : "failed") << ".\n";
  }

  bool rct_incr_restore(int restoreindex, const std::string& vm, const std::string& destination)
  {
    int lastFullForRestoreIndex = GetLastFullIndex(vm, restoreindex - 1);

    auto fullpath = GetMetadataEntry(vm, lastFullForRestoreIndex, "path");

    auto cmd = "New-Item " + destination + "'\\Virtual Hard Disks' -ItemType 'directory'";

    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "Virtual Hard Disks file creation failed.\n";
    }
    else
    {
      std::cout << "Virtual Hard Disks file creation successful.\n";
    }

    std::string incrementalPath;

    for (int i = restoreindex - 2; i > lastFullForRestoreIndex; i--)
    {
      incrementalPath = GetMetadataEntry(vm, i, "path");

      cmd = "$diffBackupPath = '" + incrementalPath + "'" + ED();
      cmd += "$restorePath = '" + destination + "'" + ED();
      cmd += "$disksPathSource = Join-Path $diffBackupPath 'Virtual Hard Disks\\*'" + ED();
      cmd += "$diskPathDestination = Join-Path $restorePath 'Virtual Hard Disks'" + ED();
      cmd += "Copy-Item $disksPathSource $diskPathDestination -Recurse -Force -ErrorAction SilentlyContinue";

      auto& [out1, err1] = PSExecuteCommand(cmd);

      if (err1.size())
      {
        std::cout << "Incremental file copy failed\n";
        return false;
      }  
    }

    incrementalPath = GetMetadataEntry(vm, restoreindex - 1, "path");

    cmd = "$fullBackupPath = '" + fullpath + "'" + ED();
    cmd += "$diffBackupPath = '" + incrementalPath + "'" + ED();
    cmd += "$restorePath = '" + destination + "'" + ED();
    cmd += "$disksPathSource = Join-Path $fullBackupPath 'Virtual Hard Disks\\*'" + ED();
    cmd += "$diskPathDestination = Join-Path $restorePath 'Virtual Hard Disks'" + ED();
    cmd += "Copy-Item $disksPathSource $diskPathDestination -Recurse -Force -ErrorAction SilentlyContinue" + ED();
    cmd += "$vmConfigFile = Get-ChildItem -Path $diffBackupPath -Recurse -Include '*.vmcx'" + ED();
    cmd += "$imported = Import-VM -Path $vmConfigFile.FullName -GenerateNewId -Copy -VirtualMachinePath $restorePath -VhdDestinationPath " + destination + "'\\Virtual Hard Disks'";
    cmd += "$imported | Rename-VM -NewName '" + vm + "-restored'";

    auto& [out1, err1] = PSExecuteCommand(cmd);

    if (err1.size())
    {
      std::cout << "Incremental Restore failed.\n";
      return false;
    }

    return true;
  }

  bool rct_full_restore(const std::string& fullBackupPath, const std::string& vm, const std::string& restorePath)
  {
    std::string cmd;

    cmd = "$vmConfigFile = Get-ChildItem -Path " + fullBackupPath + " -Recurse -Include " + "'*.vmcx'" + ED();
    cmd += "$imported = Import-VM -Path $vmConfigFile.FullName -GenerateNewId -Copy -VirtualMachinePath " + restorePath + " -VhdDestinationPath " + restorePath + "'\\Virtual Hard Disks'" + ED();
    cmd += "$imported | Rename-VM -NewName '" + vm + "-restored'";

    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "rct_full_restore Error : " << err << "\n";
      return false;
    }

    return true;
  }

  void RestoreRCTBackup( const VMBackupJob& job, int index) 
  {
    auto& [vm, level, destination, consistency] = job;

    bool fRet;

    auto& [out, err] = PSExecuteCommand(std::string("Test-Path ") + destination);

    if (out == "True")
    {
      auto type = GetMetadataEntry(vm, index - 1, "type");

      if (type == "full")
      {
        auto fullBackupPath = GetMetadataEntry(vm, index - 1, "path");
        fRet = rct_full_restore(fullBackupPath, vm, destination);
      }
      else
      {
        fRet = rct_incr_restore(index, vm, destination);
      }
    }
    else
    {
      std::cout << "RestoreRCTBackup Error : Destination path does not exist.\n";
    }

    std::cout << "RestoreRCTBackup " << (fRet ? "successful" : "failed") << ".\n";
  }
}

#endif