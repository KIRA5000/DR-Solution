#ifndef DB_HPP
#define DB_HPP

#include <ps.hpp>

#include <string>
#include <iostream>
#include <fstream>

namespace RCT
{
  using VMBackupJob = 
    std::tuple<std::string&, std::string&, std::string&, std::string&>;

  auto GetMetadataFileName(const std::string& vm)
  {
    std::string cmd;

    cmd  = "$vm = Get-VM -Name '" + vm + "' | Select-Object *" + ED();
    cmd += "[string]$vm.Id";

    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "Failed to retrieve id for vm : " << vm << " Error : " << err << "\n";
      return std::string();
    }

    return std::string(".\\" + out + ".metadata.txt");
  }

  auto GetBackupList(const std::string& vm)
  {
    std::vector<std::string> backups;

    auto metaFile = GetMetadataFileName(vm);

    auto& [out, err] = PSExecuteCommand(std::string("Test-Path ") + metaFile);

    if (out == "True")
    {
      std::string line;

      std::ifstream file(metaFile.c_str());

      while (std::getline(file, line))
      {
        backups.push_back(line);
      }

      file.close();
    }

    return backups;
  }

  auto DumpBackups(const std::string& vm)
  {
    auto& backups = GetBackupList(vm);

    for (int i = 0; i < backups.size(); i++)
    {
      std::cout << i + 1 << ". " << backups.at(i) << std::endl;
    }
  }

  auto GetMetadataEntry(const std::string& vm, size_t index, const std::string& key)
  {
    auto& backups = GetBackupList(vm);

    auto entry = backups[index];

    auto values = split(entry, " ");

    if (key == "type")
    {
      return values.at(0);
    }
    else if (key == "consistency")
    {
      return values.at(1);
    }
    else if (key == "time")
    {
      return values.at(2);
    }
    else if (key == "rctid")
    {
      return values.at(3);
    }
    else if (key == "path")
    {
      return values.at(4);
    }
    else
    {
      std::cout << "GetMetadataEntry Error : unknown key\n";
      return std::string();
    }
  }

  auto GetLastFullIndex(const std::string& vm, int index)
  {
    auto& backups = GetBackupList(vm);

    for (int i = index - 1; i >= 0 ; i--)
    {
      if ((backups.at(i)).find("full") != std::string::npos)
      {
        return i;
      }
    }

    return -1;
  }

  bool SetBackupMetadata(const VMBackupJob& job, const std::string& rctID)
  {
    auto& [vm, level, destination, consistency] = job;

    std::string cmd;

    cmd = "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " " + level + ED();
    cmd += "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " ' '" + ED();
    cmd += "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " " + consistency + ED();
    cmd += "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " ' '" + ED();
    cmd += "$time = Get-Date -Format \"MM/dd/yyyy-HH:mm\"" + ED();
    cmd += "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " $time" + ED();
    cmd += "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " ' '" + ED();
    cmd += "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " " + rctID + ED();
    cmd += "Add-Content -NoNewline -Path " + GetMetadataFileName(vm) + " ' '" + ED();
    cmd += "Add-Content -Path " + GetMetadataFileName(vm) + " " + destination;

    auto& [out, err] = PSExecuteCommand(cmd);

    if (err.size())
    {
      std::cout << "SetBackupMetadata Error : " << err << "\n";
      return false;
    }

    return true;
  }
}

#endif