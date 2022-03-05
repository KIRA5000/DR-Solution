#include <libvd.hpp>

#include <string>
#include <iostream>

void PrintUsage(void);

int main(int argc, char *argv[])
{
  if (argc < 3)
  {
    PrintUsage();
  }
  else
  {
    auto ns = argv[2];

    if (argv[1] == "-d"s)
    {
      auto disk = VHD::make_virtual_disk(argv[2]);

      if (disk)
      {
        disk->Dump();
      }
    }
    else if (argv[1] == "-f"s)
    {
      if (ns == "vhd"s)
      {
        VHD::create_base_vhd(argv[3], argv[4]);
      }
      else if (ns == "vhdx"s)
      {
        VHDX::create_base_vhdx(argv[3], argv[4]);
      }
    }
    else if (argv[1] == "-i"s)
    {
      if (ns == "vhd"s)
      {
        VHD::create_child_vhd(
          argv[3], // live source checkpointed base vhd
          argv[4], // parent path link absolute/relative
          argv[5], // incr child vhd path
          argv[6]  // rctid
        );
      }
      else if (ns == "vhdx"s)
      {
        VHDX::create_child_vhdx(
          argv[3], // live source checkpointed base vhdx
          argv[4], // parent path link absolute/relative
          argv[5], // incr child vhdx path
          argv[6]  // rctid
        );
      }
    }
    else if (argv[1] == "-r"s)
    {
      RCT::ResilientChangeTrackingToDataBlockIO(
        argv[2], // source live vhd
        argv[3], // rctid
        _2M
      );
    }
  }

  return 0;
}

void PrintUsage(void)
{
  std::cout << " \n";
  std::cout << " TestVHD.exe <-d[ump]> <image-file>\n";
  std::cout << " TestVHD.exe <-f[ull]> <vhd/vhdx> <source-block-dev> <image-file>\n";
  std::cout << " TestVHD.exe <-i[ncr]> <vhd/vhdx> <source-block-dev> <parent-path> <target-file> <rctid>\n";
  std::cout << " \n";
}