# cpp-vss

VSS snapshot library for Hyper-V (2012), ASR, Exchange and MSSQL applications

### Build
 - Download and install latest cmake
 - Fork the main repo
 - Clone the forked repo under a folder say c:\cpp-vss
 - From inside a VS 2019 x64 command prompt:

```sh
cd c:\cpp-vss && md build && cd build
cmake ..
cmake --build . --config Release
```

### asr writer component selection
 - Every critical component is selected for backup
 - Volumes that are critical as per writer are selected for backup and their respective disks are noted
 - Only disks that are noted during volume selection are selected for backup

### original vshadow
```sh
https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/VShadowVolumeShadowCopy
```