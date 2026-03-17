#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <csignal>
#include <ctime>
#include <exception>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

inline constexpr const char *PORT = "3490";

inline constexpr int BACKLOG = 10;
inline constexpr size_t MAXDATASIZE = 4096;

namespace {

class GetAddrInfo {
  addrinfo *res{};

public:
  GetAddrInfo(const GetAddrInfo &) = delete;
  GetAddrInfo(GetAddrInfo &&) = delete;
  GetAddrInfo &operator=(const GetAddrInfo &) = delete;
  GetAddrInfo &operator=(GetAddrInfo &&) = delete;
  GetAddrInfo(const char *hostname, const char *servname,
              const struct addrinfo *hints, struct addrinfo **l_res) {
    if (int rv = getaddrinfo(hostname, servname, hints, l_res); rv != 0) {
      throw std::runtime_error("gai: " + std::string(gai_strerror(rv)) + '\n');
    }
    this->res = *l_res;
  }
  ~GetAddrInfo() { freeaddrinfo(res); }
};

} // namespace

static void sigchld_handler(int /* s */) {
  int saved_errno = errno;
  while (waitpid(-1, nullptr, WNOHANG) > 0) {
  }
  errno = saved_errno;
}

// TODO: use C++ methods
static void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &((reinterpret_cast<sockaddr_in *>(sa))->sin_addr);
  }

  return &((reinterpret_cast<sockaddr_in6 *>(sa))->sin6_addr);
}

static int setup_server() {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC; // Either IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo *servinfo = nullptr;
  GetAddrInfo gai_raii{nullptr, PORT, &hints, &servinfo};

  int sockfd = -1;
  addrinfo *p = servinfo;
  for (; p != nullptr; p = p->ai_next) {
    void *addr = nullptr;

    if (p->ai_family == AF_INET) {
      auto *ipv4 = reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
      addr = &(ipv4->sin_addr);
    } else {
      auto *ipv6 = reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr);
      addr = &(ipv6->sin6_addr);
    }

    std::array<char, INET6_ADDRSTRLEN> ipstr{};
    inet_ntop(p->ai_family, addr, ipstr.data(), ipstr.size());
    std::clog << "binding to " << ipstr.data() << '\n';

    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      throw std::runtime_error("socket");
    }

    const int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
      throw std::runtime_error("setsockopt");
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd); // TODO:
      std::cerr << "bind\n";
      continue;
    }
    break;
  }

  if (p == nullptr) {
    throw std::runtime_error("failed to bind");
  }

  if (listen(sockfd, BACKLOG) == -1) {
    throw std::runtime_error("listen");
  }
  return sockfd;
}

static void setup_sigchld() {
  struct sigaction sa{};
  sa.sa_handler = sigchld_handler;
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
    throw std::runtime_error("sigaction"); // TODO:errno
  }
}

static int server_loop(int server_fd) {
  while (true) {
    sockaddr_storage their_addr{};
    socklen_t sin_size = sizeof their_addr;
    int new_fd =
        accept(server_fd, reinterpret_cast<sockaddr *>(&their_addr), &sin_size);
    if (new_fd == -1) {
      std::cerr << "accept\n";
      continue;
    }

    std::array<char, INET6_ADDRSTRLEN> s{};
    inet_ntop(their_addr.ss_family,
              get_in_addr(reinterpret_cast<sockaddr *>(&their_addr)), s.data(),
              s.size());
    std::clog << "got connection from " << s.data() << '\n';

    pid_t pid = fork();
    if (pid == 0) { // child
      close(server_fd);

      long numbytes = 0;
      std::array<char, MAXDATASIZE> buf{};
      while (true) {
        numbytes = recv(new_fd, buf.data(), buf.size(), 0);
        if (numbytes == -1) {
          std::cerr << "recv\n";
          _exit(1);
        } else if (numbytes == 0) {
          std::cerr << s.data() << " disconnected\n";
          break;
        }
        if (send(new_fd, buf.data(), static_cast<size_t>(numbytes), 0) == -1) {
          std::cerr << "send\n";
        }
        std::cout << "echoed to " << s.data() << '\n';
      }
      close(new_fd);
      _exit(0);
      // parent
    } else if (pid == -1) {
      std::cerr << "fork\n";
    }
    close(new_fd); // close sender for parrent
  }
}

int main() {
  try {
    int server_fd = setup_server(); // TODO: RAII
    setup_sigchld();

    std::cout << "waiting connections...\n";

    server_loop(server_fd);

  } catch (const std::exception &ex) {
    std::cerr << "Err: " << ex.what() << '\n';
    return -1;
  }
}
