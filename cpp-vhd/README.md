# cpp-vhd
vhd and vhdx format generator library

## external dependencies:
```sh
https://github.com/n-mam/cpp-osl
https://github.com/n-mam/cpp-npl
```

## install openssl and crc32c via vcpkg
```sh
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install openssl::x64-windows
.\vcpkg\vcpkg install crc32c:x64-windows
.\vcpkg\vcpkg integrate install
```