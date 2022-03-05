#ifndef PS_HPP
#define PS_HPP

#include <windows.h>

#include <tuple>
#include <string>
#include <sstream>
#include <iostream>

#include <util.hpp>

namespace RCT 
{
  auto ED(){ return std::string(";"); }

  auto PSBuildTask(const std::string& block)
  {
    std::stringstream ss;
    ss << "Powershell.exe -NoLogo ";
    ss << "-Command ";
    ss << "\"& { Invoke-Command -ScriptBlock {";
    ss << block;
    ss << "} }\"";
    return ss.str();
  }

  auto PSExecuteCommand(const std::string& command)
  {
    auto& task = PSBuildTask(command);
    return LaunchTask(task);
  }

  void PSExecuteScriptBlock(const std::string& block, ...)
  {

  }

} //ns rct

#endif