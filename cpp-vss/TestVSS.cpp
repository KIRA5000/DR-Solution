#include <libvss.hpp>

#include <iostream>

int wmain(int argc, wchar_t *argv[])
{
  std::vector<std::wstring> arguments;

  for(int i = 1; i < argc; i++)
  {
    arguments.push_back(argv[i]);
  }

  return vss::Execute(
    arguments,
    [] (auto writer, auto pMetadata, auto pComponent) {
      std::wcout << 
        L" OnComponent : " << 
        writer << L"\n";
      return true;
    },
    [] (auto prop) {
      std::wcout << 
        L" OnSnapshot : " << 
        prop.m_pwszOriginalVolumeName << " -> " << 
        prop.m_pwszSnapshotDeviceObject << L"\n";
    }
  );

}
