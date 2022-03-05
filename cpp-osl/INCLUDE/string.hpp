#ifndef STRING_HPP
#define STRING_HPP

#include <vector>
#include <sstream>

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

std::vector<std::wstring> wsplit(std::wstring s, std::wstring delim) 
{
    size_t pos_start = 0, pos_end, delim_len = delim.length();
    std::wstring token;
    std::vector<std::wstring> res;

    while ((pos_end = s.find (delim, pos_start)) != std::string::npos) 
    {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));

    return res;
}

std::string& ltrim(std::string& str, const std::string& chars)
{
    str.erase(0, str.find_first_not_of(chars));
    return str;
}
 
std::string& rtrim(std::string& str, const std::string& chars)
{
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

std::string& trim(std::string& str, const std::string& chars)
{
    return ltrim(rtrim(str, chars), chars);
}

bool StartsWith(std::wstring& str, std::wstring prefix)
{
  bool fRet = false;

  if (str.rfind(prefix, 0) == 0)
  {
    fRet = true;
  }

  return fRet;
}

#endif //