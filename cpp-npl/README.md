### cpp-np-lib
C++17 network protocol library

- Cross platform
- Header only
- Asynchronous
- Supported protocols
  * FTP
  * FTPS
  * Websockets
  * SSH (WIP)

Build

```sh
Install openssl using vcpkg
https://github.com/Microsoft/vcpkg
On Linux, export the following env variables:
export OPENSSL_ROOT_DIR=/home/nmam/vcpkg/installed/x64-linux
export OPENSSL_INCLUDE_DIR=/home/nmam/vcpkg/installed/x64-linux/include
```
```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

FTP example
```cpp
#include <iostream>

#include <npl.hpp>

int main(void)
{
  if (argc != 3)
  {
    std::cout << "usage : ftp <host> <port>\n";
    return 0;
  }

  auto host = std::string(argv[1]);
  auto port = std::stoi(argv[2]);

  /**
   * Create an FTP object.
   */
  auto ftp = NPL::make_ftp(host, port, NPL::FTPS::Explicit);

  /**
   * Set the login credentials.
   */
  ftp->SetCredentials("ftpuser", "ftpuser");

  /**
   * Start the protocol. This would asynchronously
   * trigger connect and the login sequence
   */
  ftp->Start();

  DCProt protection = DCProt::Protected;

  /**
   * Directory listing. The lambda argument is an output callback 
   * which is invoked multiple times with chunks of directory list
   * data. A null "b" (buffer) pointer indicates that there's no 
   * more data. Returning false at any point terminates the transfer.
   */
  ftp->ListDirectory(
    [list = std::string("")] (const char *b, size_t n) mutable {
    if (b)
     list.append(std::string(b, n));
    else
     std::cout << list;
    return true;
  }, "", protection);

  /**
   * Set the current directory
   */
  ftp->SetCurrentDir("vcpkg");

  /**
   * File download. Similar to the "List" API. 
   * The lambda argument is an output callback which is invoked multiple 
   * times with chunks of file data. A null "b" (buffer) pointer indicates 
   * that there's no more file data to download. Returning false at any point
   * terminates the transfer. local file arg is optional; if not specified then 
   * downloded data can only be accesed via the callback.
   */
  ftp->Download([](const char *b, size_t n) {
    if (b)
      std::cout << std::string(b, n);
    else
      std::cout << "Download complete.\n";
    return true;
  }, "bootstrap-vcpkg.bat", "./download.bat", protection);

  /**
   * File upload. The lambda argument is an output callback which is invoked
   * multiple times with chunks of file data as they are uploaded. A null "b"
   * (buffer) pointer indicates that there's no more file data to be uploaded.
   * Returning false from this callback closes the transfer. Data channel 
   * protection is 'C'lear for the upload implying a granular PROT levels on a 
   * per transfer basis.
   */
  ftp->Upload([](const char *b, size_t n) {
    return true;
  }, "README.txt", "../README.md", protection);

  /**
   * Get the current directory
   */
  ftp->GetCurrentDir([](const std::string& resp) {
    /**
     * "resp" holds the response of PWD command
     */
  });

  ftp->SetCurrentDir("/");

  /**
   * Quit the session. Sends FTP QUIT command
   * and triggeres the cleanup of "ftp" object
   */
  ftp->Quit();

  std::this_thread::sleep_for(std::chrono::milliseconds(60000));

  return 0;
}
```