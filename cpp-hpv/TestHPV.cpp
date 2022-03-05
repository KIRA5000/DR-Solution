#include <hpv.hpp>

#include <iostream>

void PrintUsage()
{
  std::cout << "\n";
  std::cout << "TestHpv.exe -b <vm-name> <destination>\n";
  std::cout << "TestHpv.exe -r <backup component> <file map document> <backup location>";
  std::cout << "\n";
}

int wmain(int argc, wchar_t* argv[])
{
  int fRet = -1;

  if ((argc != 4) && (argc != 5))
  {
    PrintUsage();
  }
  else
  {
    std::wstring type = argv[1];

    if ((type == L"-b") && (argc == 4))
    {
      fRet = HPV::Backup(argv[2], argv[3]);
    }
    else if ((type == L"-r") && (argc == 5))
    {
      fRet = HPV::Restore(argv[2], argv[3], argv[4]);
    }
    else
    {
      PrintUsage();
    }
  }

  return fRet;
}