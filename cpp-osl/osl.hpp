#ifndef OSL_HPP
#define OSL_HPP

#include <json.hpp>
#include <util.hpp>
#include <osd.hpp>
#include <string.hpp>

#include <string>
#include <filesystem>

namespace OSL
{
  void GetVolumeList(Json& json)
  {
    std::vector<Json> list;

    auto volumes = EnumerateVolumes();

    for(auto& names : volumes)
    {
      Json j;

      j.SetKey("guid", names[0]);

      std::string paths;

      for(auto i = 1; i < names.size(); i++)
      {
        paths += names[i] + " ";
      }

      j.SetKey("paths", rtrim(paths, " "));

      auto h = GetVolumeHandle(names[0]);

      j.SetKey("length", std::to_string(GetPartitionLength(h)));

      auto nvdb = GetNTFSVolumeData(h);

      j.SetKey("TotalClusters", std::to_string(nvdb.TotalClusters.QuadPart));
      j.SetKey("FreeClusters", std::to_string(nvdb.FreeClusters.QuadPart));

      CloseHandle(h);

      list.push_back(j);
    }

    json.SetKey("List", Json::JsonListToArray(list));
  }

  void GetSnapshotList(Json& json)
  {

  }

  void GetDirectoryList(Json& json)
  {
    std::string dir;

    if (json.HasKey("Directory"))
    {
      dir = json.GetKey("Directory");
    }

    if (!dir.size())
    {
      dir = getenv("USERPROFILE");//std::filesystem::current_path().string();
    }

    std::vector<Json> list;

    for (auto& e : std::filesystem::directory_iterator(dir))
    {
      Json j;

      j.SetKey("name", e.path().filename().string());
    
      if (std::filesystem::is_symlink(e))
      {
        j.SetKey("type", "symlink");
      }
      else if (std::filesystem::is_directory(e))
      {
        j.SetKey("type", "directory");
      }
      else
      {
        j.SetKey("type", "file");
      }

     list.push_back(j);
    }

    json.SetKey("Directory", dir);
    json.SetKey("List", Json::JsonListToArray(list));
  }

  void GetFreeDriveList(Json& json)
  {

  }
}

#endif //OSL_HPP