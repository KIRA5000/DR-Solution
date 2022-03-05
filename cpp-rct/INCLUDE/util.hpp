#ifndef UT_HPP
#define UT_HPP

#include <vector>
#include <string>
#include <sstream>

namespace RCT
{
  auto ltrim(std::string& str, const std::string& chars)
  {
    str.erase(0, str.find_first_not_of(chars));
    return str;
  }
 
  auto rtrim(std::string& str, const std::string& chars)
  {
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
  }
 
  auto trim(std::string& str, const std::string& chars)
  {
    return ltrim(rtrim(str, chars), chars);
  }

  std::vector<std::string> split(const std::string &s, char delim)
  {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;  

    while (std::getline(ss, item, delim))
    {
      elems.push_back(item);
    }

    return elems;
  }

  std::vector<std::string> split(std::string s, std::string delim) 
  {
    size_t pos_start = 0, pos_end, delim_len = delim.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find (delim, pos_start)) != std::string::npos) 
    {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
  }

  auto ReadPipe(HANDLE h)
  {
    std::string data;

    while (true)
    {
      char _buf[2048];
      DWORD nBytesRead = 0;

      bool fRet = ReadFile(
          h, 
          _buf, 
          2048, 
          &nBytesRead, 
          NULL);

      if (!fRet || nBytesRead == 0)
      {
        break;
      }

      data += std::string(_buf, nBytesRead);
    }

    return (data.size() ? rtrim(data, "\r\n") : data);
  }

  auto LaunchTask(const std::string& cmdline, bool bWait = true)
  {
    SECURITY_ATTRIBUTES sa;

    sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
    sa.bInheritHandle = TRUE; 
    sa.lpSecurityDescriptor = NULL; 

    HANDLE er, ew, or, ow;
    CreatePipe(&er, &ew, &sa, 0);
    CreatePipe(&or, &ow, &sa, 0);
    SetHandleInformation(er, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(or, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };

    si.cb = sizeof(STARTUPINFO); 
    si.hStdError = ew;
    si.hStdOutput = ow;
    si.dwFlags |= STARTF_USESTDHANDLES;

    BOOL fRet = CreateProcessA(
      NULL,
      (char *) cmdline.c_str(),
      NULL,
      NULL,
      TRUE,
      0,
      NULL,
      NULL,
      &si,
      &pi
    );

    CloseHandle(ew);
    CloseHandle(ow);

    if (fRet == 0)
    {
      std::stringstream ess;
      ess << "Failed to launch process : [";
      ess << cmdline;
      ess << "], error : ";
      ess << GetLastError();
      return std::make_tuple(std::string(), ess.str());
    }

    auto& out = ReadPipe(or);
    auto& err = ReadPipe(er);

    if (bWait)
    {
      WaitForSingleObject(pi.hProcess, INFINITE);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return std::make_tuple(out, err);
  }
}

#endif