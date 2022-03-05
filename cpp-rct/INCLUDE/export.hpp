#ifndef EX_HPP
#define Ex_HPP

#include <ps.hpp>
#include <virtdisk.h>

#include <string>
#include <iostream>

namespace RCT
{
  bool ExportCheckpoint(const VMBackupJob& job, const std::string& checkpoint, const std::string&  rctid)
  {
    std::string cmd;
    std::string reference;

    auto& [vm, level, destination, consistency] = job;

    cmd = "Import-Module .\\DR_HyperV.psm1" + ED();
    cmd += "$BackupCheckpoint = Get-VmBackupCheckpoints -VmName '" + vm + "' |  Where-Object -Property InstanceID -eq -Value " + checkpoint + ED();

    if (level == "incr" && rctid.size())
    {
      cmd += "$Referencepoint = Get-VmReferencePoints -VmName '" + vm + "' |  Where-Object -Property ResilientChangeTrackingIdentifiers -eq -Value " + rctid + " | Select -Last 1" + ED();
      reference = " -ReferencePoint $Referencepoint";
    }

    cmd += "Export-VMBackupCheckpoint -VmName '" + vm + "' -DestinationPath " + destination + " -BackupCheckPoint $BackupCheckpoint" + reference;
    
    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "ExportCheckpoint Error : " << err << "\n";
      return false;
    }

    return true;
  }
}

#endif