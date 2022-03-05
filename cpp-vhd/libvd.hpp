#ifndef LIBVD_HPP
#define LIBVD_HPP

#include <vhd.hpp>
#include <vhdx.hpp>

using namespace std::string_literals;

namespace VHD
{
  auto make_virtual_disk(const std::string& path)
  {
    SPCBaseDisk disk = nullptr;

    uint8_t buf[512] = { 0 };

    auto file = NPL::make_file(path, false);

    if (!file)
    {
      std::cout << "Failed to open file " << path << "\n";
      return disk;
    }

    file->ReadSync(buf, sizeof(buf), 0);

    file.reset();

    std::cout << "\n";

    if (memcmp(buf, "conectix", strlen("conectix")) == 0)
    {
      std::cout << "Dynamic VHD detected\n";
      disk = std::make_shared<CDynamicVHD>(path);
    }
    else if (memcmp(buf, "vhdxfile", strlen("vhdxfile")) == 0)
    {
      std::cout << "Dynamic VHDx detected\n";
      disk = std::make_shared<CDynamicVHDx>(path);
    }
    else
    {
      std::cout << "Unknown image file format\n";
    }

    disk->Initialize();

    return disk;
  }
}

#endif