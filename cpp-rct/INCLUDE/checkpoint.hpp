#ifndef CP_HPP
#define CP_HPP

#include <ps.hpp>

#include <string>
#include <iostream>

namespace RCT
{
  auto CheckPointVM(const std::string& vm, const std::string consistency)
  {
    std::string cmd;

    cmd = "Import-Module .\\DR_HyperV.psm1" + ED();
    cmd += "$BackupCheckpoint = New-VmBackupCheckpoint -VmName '" + vm + "' -ConsistencyLevel " + consistency + ED();
    cmd += "$BackupCheckpoint.InstanceID";
    
    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "CheckPointVM Error : " << err << "\n";
      return std::string();
    }

    return out;
  }

  auto CheckPointToRefrencePoint(const std::string& vm, const std::string& checkpoint)
  {
    std::string cmd;

    cmd = "Import-Module .\\DR_HyperV.psm1" + ED();
    cmd += "$BackupCheckpoint = Get-VmBackupCheckpoints -VmName '" + vm + "' |  Where-Object -Property InstanceID -eq -Value " + checkpoint + ED();
    cmd += "$ReferencePoint = Convert-VmBackupCheckpoint -BackupCheckpoint $BackupCheckpoint" + ED();
    cmd += "$ReferencePoint.ResilientChangeTrackingIdentifiers";

    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "CheckPointToRefrencePoint Error :" << err << "\n";
      return std::string();
    }

    return out;
  }

  auto GetLastReferencePointID(const std::string& vm)
  {
    // check if we can open the metadata file
    auto& backups = GetBackupList(vm);

    if (!backups.size())
    {
      std::cout << "GetBackupList failed to retrieve vm metadata\n";
      return std::string();
    }

    //read the last registered backup's reference point
    auto rctid = GetMetadataEntry(vm, backups.size() - 1, "rctid");

    std::string cmd;

    // check if the refrence point is still available and
    // is equal to the one retrieved from the vm metadata
    cmd = "Import-Module .\\DR_HyperV.psm1" + ED();
    cmd += "Get-VmReferencePoints -VmName '" + vm + "' |  Where-Object -Property ResilientChangeTrackingIdentifiers -eq -Value " + rctid + ED();

    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "GetBackupList Error : last referencepoint not found.\n";
      return std::string();
    }

    //return true is all checks successful
    return rctid;
  }
}

#endif