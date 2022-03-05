# cpp-hpv
hyper-v backup and recovery

### Build
 - Download and install latest cmake
 - Fork the main repo
 - Clone the forked repo under a folder say c:\cpp-rct
 - From inside a VS 2019 x64 command prompt:

```sh
cd c:\cpp-hpv
md build
cd build
cmake ..
cmake --build . --config Release
```

### component selection
 - Vm Name specified by the user is the component selected for backup
