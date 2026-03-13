#include <arpa/inet.h>
#include <array>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

inline static constexpr const char *PORT = "3490";

int main() {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC; // Either IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo *servinfo{};

  if (int rv = getaddrinfo(nullptr, PORT, &hints, &servinfo); rv != 0) {
    std::cerr << "gai: " << gai_strerror(rv) << '\n';
    return -1; // TODO: throw + free gai
  }

  int sockfd;

  for (addrinfo *p = servinfo; p != nullptr; p = p->ai_next) {
    void *addr;
    if (p->ai_family == AF_INET) {
      auto *ipv4 = reinterpret_cast<struct sockaddr_in *>(p->ai_addr); // TODO
      addr = &(ipv4->sin_addr);
    } else {
      auto *ipv6 = reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr); // TODO
      addr = &(ipv6->sin6_addr);
    }

    // char ipstr[INET6_ADDRSTRLEN];
    std::array<char, INET6_ADDRSTRLEN> ipstr{};
    inet_ntop(p->ai_family, addr, ipstr.data(), ipstr.size());
    std::clog << "binding to " << ipstr.data() << '\n';
  }
  freeaddrinfo(servinfo); // TODO

  //...
  return 0;
}
