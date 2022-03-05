#include <string>
#include <iostream>

#include <rct.hpp>

void PrintUsage(void);

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    PrintUsage();
    return 0;
  }

  std::string type = argv[1];

  if ((argc == 3) && (type == "metadata"))
  {
    std::cout << "\n List of available backups : \n";
    RCT::DumpBackups(argv[2]);
  }
  else if ((argc == 6) && (type == "backup"))
  {
    std::cout << "\n";
    std::string vm = argv[2];
    std::string level = argv[3];
    std::string destination = argv[4];
    std::string consistency = argv[5];

    RCT::CreateRCTBackup(
        RCT::VMBackupJob(
          vm,
          level,
          destination,
          consistency
      ));
  }
  else if ((argc == 5) && (type == "restore"))
  {
    std::cout << "\n";
    std::string vm = argv[2];
    int index = atoi(argv[3]);
    std::string destination = argv[4];

    RCT::RestoreRCTBackup(
      RCT::VMBackupJob(
        vm,
        std::string(),
        destination,
        std::string()
      ),
      index
    );
  }
  else
  {
    PrintUsage();
  }
  
  return 0;
}

void PrintUsage(void)
{
  std::cout << "\n";
  std::cout << " Test.exe <backup> <vm> <full/incr> <destination> <consistency>\n";
  std::cout << " Test.exe <restore> <vm> <backup_index> <destination>\n";
  std::cout << " Test.exe <metadata> <vm>\n";
}