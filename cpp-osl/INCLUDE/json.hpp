#ifndef JSON_HPP
#define JSON_HPP

#include <map>
#include <regex>
#include <string>
#include <vector>
#include <sstream>

#include <string.hpp>

class Json
{
  public:

    Json()
    {

    }

    Json(const std::string& s)
    {
      iJsonString = s;
      Parse();
    }

    ~Json()
    {
    }

    bool IsOk()

    {
      return false;
    }

    std::string GetKey(const std::string& key)
    {
      return iMap[key];
    }

    void SetKey(const std::string& key, const std::string& value)
    {
      iMap[key] = std::regex_replace(value, std::regex(R"(\\)"), R"(\\)");
    }

    bool HasKey(const std::string& key)
    {
      auto fRet = iMap.find(key);

      if (fRet != iMap.end())
      {
        return true;
      }
      else
      {
        return false;
      }
    }

    std::string Stringify(void)
    {
      std::string js;

      js += "{";

      for (const auto &element : iMap)
      {
        if (js.length() > 1)
        {
          js += ", ";
        }

        js += "\"" + element.first + "\"" + ": ";
    
        if ((element.second)[0] != '[') 
        {
          js += "\"" + element.second + "\"";
        }
        else
        {
          js += element.second;
        }
      }

      js += "}";

      return js;
    }
    
    static std::string JsonListToArray(std::vector<Json>& list)
    {
      std::string value = "[";

      for(auto& j : list)
      {
        value += j.Stringify();
        value += ",";
      }
  
      value = trim(value, ",");
    
      value += "]";

      return value;
    }

    /*
     * This assumes that the json has been stringifie'd and has only 
     * string kv pairs, otherwise this would fail miserably
     * {"service":"ftp","request":"connect","list":"/","id":"0","host":"w","port":"w","user":"w","pass":"w"}
     */
    void Parse(void)
    {
      ltrim(iJsonString, "{\"");
      rtrim(iJsonString, "\"}");

      auto pp = split(iJsonString, "\",\"");

      for (auto& p : pp)
      {
        auto kv = split(p, "\":\"");

        if (kv.size() == 2)
        {
          iMap.insert(
            std::make_pair(kv[0], kv[1])
          );
        }
      }
    }
    
    void Dump(void)
    {

    }

  private:

    std::string iJsonString;

    std::map<std::string, std::string> iMap;

};

#endif //JSON_HPP
