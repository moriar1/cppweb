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
  GetAddrInfo() = delete;
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

class Socket {
  int fd = -1;

public:
  explicit Socket(int s = -1) : fd(s) {}
  ~Socket() {
    if (fd != -1) {
      close(fd);
    }
  }

  // Move
  Socket(Socket &&other) noexcept : fd{other.fd} { other.fd = -1; }
  Socket &operator=(Socket &&other) noexcept {
    if (this != &other) {
      if (fd != -1) {
        close(fd);
      }
      fd = other.fd;
      other.fd = -1;
    }
    return *this;
  }

  // Copy deleted
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  // [[nodiscard]] int get() const { return fd; }
  void reset(int s = -1) {
    if (fd != -1) {
      close(fd);
    }
    fd = s;
  }
  operator int() const { return fd; }
};

} // namespace

static void sigchld_handler(int /* s */) noexcept {
  int saved_errno = errno;
  while (waitpid(-1, nullptr, WNOHANG) > 0) {
  }
  errno = saved_errno;
}

// TODO: use C++ methods
static void *get_in_addr(struct sockaddr *sa) noexcept {
  if (sa->sa_family == AF_INET) {
    return &((reinterpret_cast<sockaddr_in *>(sa))->sin_addr);
  }

  return &((reinterpret_cast<sockaddr_in6 *>(sa))->sin6_addr);
}

static Socket setup_server() {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC; // Either IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo *servinfo = nullptr;
  GetAddrInfo gai_raii{nullptr, PORT, &hints, &servinfo};

  Socket server_sock{-1}; // RAII Socket
  addrinfo *p = servinfo;
  for (; p != nullptr; p = p->ai_next) {
    std::array<char, INET6_ADDRSTRLEN> ipstr{};
    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), ipstr.data(),
              ipstr.size());
    std::clog << "binding to " << ipstr.data() << "...\n";

    int s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (s == -1) {
      std::cerr << "socket\n";
      continue;
    }
    server_sock.reset(s);

    const int yes = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ==
        -1) {
      throw std::runtime_error("setsockopt");
    }

    if (bind(server_sock, p->ai_addr, p->ai_addrlen) == -1) {
      std::cerr << "bind\n";
      server_sock.reset(-1);
      continue;
    }
    break;
  }

  if (p == nullptr) {
    throw std::runtime_error("failed to bind");
  }

  if (listen(server_sock, BACKLOG) == -1) {
    throw std::runtime_error("listen");
  }
  return server_sock;
}

static void setup_sigchld() {
  struct sigaction sa{};
  sa.sa_handler = sigchld_handler;
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
    throw std::runtime_error("sigaction");
  }
}

[[noreturn]]
static void server_loop(Socket server_fd) {
  while (true) {
    sockaddr_storage their_addr{};
    socklen_t sin_size = sizeof their_addr;
    int new_fd =
        accept(server_fd, reinterpret_cast<sockaddr *>(&their_addr), &sin_size);

    if (new_fd == -1) {
      std::cerr << "accept\n";
      continue;
    }
    // RAII Socket
    Socket accept_sock{new_fd};

    std::array<char, INET6_ADDRSTRLEN> s{};
    inet_ntop(their_addr.ss_family,
              get_in_addr(reinterpret_cast<sockaddr *>(&their_addr)), s.data(),
              s.size());
    std::clog << "got connection from " << s.data() << '\n';

    pid_t pid = fork();
    if (pid == 0) {      // child
      server_fd.reset(); // close listener for child

      long numbytes = 0;
      std::array<char, MAXDATASIZE> buf{};
      while (true) {
        numbytes = recv(accept_sock, buf.data(), buf.size(), 0);
        if (numbytes == -1) {
          std::cerr << "recv\n";
          accept_sock.reset();
          _exit(1);
        } else if (numbytes == 0) {
          std::cerr << s.data() << " disconnected\n";
          break;
        }
        if (send(accept_sock, buf.data(), static_cast<size_t>(numbytes), 0) ==
            -1) {
          std::cerr << "send\n";
        }
        std::clog << "echoed to " << s.data() << '\n';
      }
      accept_sock.reset();
      _exit(0);
      // parent
    } else if (pid == -1) {
      std::cerr << "fork\n";
    }
    close(accept_sock); // close sender for parrent
  }
}

// TODO: add errno output where it is possible
int main() {
  try {
    Socket server_fd = setup_server(); // TODO: RAII
    setup_sigchld();

    std::clog << "waiting connections...\n";

    server_loop(std::move(server_fd));
    // close(server_fd);

  } catch (const std::exception &ex) {
    std::cerr << "Err: " << ex.what() << '\n';
    return -1;
  }
}
