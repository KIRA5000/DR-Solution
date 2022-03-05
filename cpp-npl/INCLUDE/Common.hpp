#ifndef COMMON_HPP
#define COMMON_HPP

#ifdef linux

  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  using FD = int;
  using SOCKET = int;
  #define closesocket close;

#else

  #include <winsock2.h>
  #include <MSWSock.h>
  #include <Ws2tcpip.h>
  using FD = HANDLE;

#endif

#endif //COMMON_HPP