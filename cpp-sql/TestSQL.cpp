#include <sql.hpp>

#include <iostream>

void PrintUsage()
{
  std::cout << "\n";
  std::cout << "VSS:\n";
  std::cout << "TestSQL.exe -b[backup] <db-name> <destination>\n";
  std::cout << "TestSQL.exe -r[restore] <backup component> <file list document> <backup location>";
  std::cout << "\n";
  std::cout << "VDI:\n";
  std::cout << "TestSQL.exe -d[dump backup record] <dbName>\n";
  std::cout << "TestSQL.exe -bv[backup vdi] <full/diff/log/tail> <dbName> <fileName>\n";
  std::cout << "TestSQL.exe -rv[restore vdi] <dbName> <index to restore>\n";
  std::cout << "\n";
}

int wmain(int argc, wchar_t* argv[])
{
  int fRet = -1;

  if ((argc != 3) && (argc != 4) && (argc != 5))
  {
    PrintUsage();
  }
  else
  {
    std::wstring type = argv[1];

    if ((type == L"-b") && (argc == 4))
    {
      fRet = SQL::BackupVSS(argv[2], argv[3]);
    }
    else if ((type == L"-r") && (argc == 5))
    {
      fRet = SQL::RestoreVSS(argv[2], argv[3], argv[4]);
    }
    else if ((type == L"-d") || (argc == 3))
    {
      fRet = SQL::DumpBackups(argv[2]);
    }
    else if ((type == L"-bv") && (argc == 5))
    {
      fRet = SQL::BackupVDI(argv[2], argv[3], argv[4]);
    }
    else if ((type == L"-rv") && (argc == 4))
    {
      int index = std::stoi(argv[3]);

      fRet = SQL::RestoreVDI(argv[2], index);
    }
    else
    {
      PrintUsage();
    }
  }

  return fRet;
}