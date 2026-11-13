#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint32_t kMaxFrameBytes = 4u * 1024u * 1024u;
std::mutex g_log_mutex;

void log_line(const std::string &line) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cout << line << std::endl;
}

struct Io {
  virtual ~Io() = default;
  virtual bool read_exact(char *data, std::size_t size) = 0;
  virtual bool write_all(const char *data, std::size_t size) = 0;
};

bool read_frame(Io &io, std::string &payload) {
  unsigned char header[4];
  if (!io.read_exact(reinterpret_cast<char *>(header), sizeof(header))) {
    return false;
  }
  const std::uint32_t size = (static_cast<std::uint32_t>(header[0]) << 24) |
                             (static_cast<std::uint32_t>(header[1]) << 16) |
                             (static_cast<std::uint32_t>(header[2]) << 8) |
                             static_cast<std::uint32_t>(header[3]);
  if (size > kMaxFrameBytes) {
    return false;
  }
  payload.resize(size);
  return size == 0 || io.read_exact(payload.data(), size);
}

bool write_frame(Io &io, const std::string &payload) {
  const std::uint32_t size = static_cast<std::uint32_t>(payload.size());
  const char header[4] = {
      static_cast<char>((size >> 24) & 0xFFu),
      static_cast<char>((size >> 16) & 0xFFu),
      static_cast<char>((size >> 8) & 0xFFu),
      static_cast<char>(size & 0xFFu),
  };
  return io.write_all(header, sizeof(header)) &&
         (payload.empty() || io.write_all(payload.data(), payload.size()));
}

void handle_connection(Io &io, int id) {
  log_line("CONNECTION " + std::to_string(id));
  std::string payload;
  while (read_frame(io, payload)) {
    log_line("FRAME " + std::to_string(id) + " " +
             std::to_string(payload.size()));
    if (!write_frame(io, payload)) {
      break;
    }
  }
  log_line("CLOSED " + std::to_string(id));
}

#ifdef _WIN32

class PipeIo final : public Io {
public:
  explicit PipeIo(HANDLE handle) : handle_(handle) {}

  bool read_exact(char *data, std::size_t size) override {
    std::size_t offset = 0;
    while (offset < size) {
      DWORD read = 0;
      const DWORD want = static_cast<DWORD>(size - offset);
      if (!ReadFile(handle_, data + offset, want, &read, nullptr) || read == 0) {
        return false;
      }
      offset += read;
    }
    return true;
  }

  bool write_all(const char *data, std::size_t size) override {
    std::size_t offset = 0;
    while (offset < size) {
      DWORD written = 0;
      const DWORD want = static_cast<DWORD>(size - offset);
      if (!WriteFile(handle_, data + offset, want, &written, nullptr) ||
          written == 0) {
        return false;
      }
      offset += written;
    }
    return true;
  }

private:
  HANDLE handle_;
};

class SocketIo final : public Io {
public:
  explicit SocketIo(SOCKET socket) : socket_(socket) {}

  bool read_exact(char *data, std::size_t size) override {
    std::size_t offset = 0;
    while (offset < size) {
      const int got = ::recv(socket_, data + offset,
                             static_cast<int>(size - offset), 0);
      if (got <= 0) {
        return false;
      }
      offset += static_cast<std::size_t>(got);
    }
    return true;
  }

  bool write_all(const char *data, std::size_t size) override {
    std::size_t offset = 0;
    while (offset < size) {
      const int sent = ::send(socket_, data + offset,
                              static_cast<int>(size - offset), 0);
      if (sent <= 0) {
        return false;
      }
      offset += static_cast<std::size_t>(sent);
    }
    return true;
  }

private:
  SOCKET socket_;
};

std::wstring widen(const std::string &text) {
  if (text.empty()) {
    return std::wstring();
  }
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                       static_cast<int>(text.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      out.data(), size);
  return out;
}

void serve_pipe(const std::string &name) {
  const std::wstring full = L"\\\\.\\pipe\\" + widen(name);
  log_line("LISTENING " + name);
  std::atomic<int> next_id{1};
  while (true) {
    HANDLE handle = CreateNamedPipeW(
        full.c_str(), PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
        kMaxFrameBytes + 4, kMaxFrameBytes + 4, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      log_line("ERROR create " + std::to_string(GetLastError()));
      return;
    }
    const BOOL connected = ConnectNamedPipe(handle, nullptr);
    const DWORD error = GetLastError();
    if (!connected && error != ERROR_PIPE_CONNECTED) {
      log_line("ERROR connect " + std::to_string(error));
      CloseHandle(handle);
      continue;
    }
    const int id = next_id.fetch_add(1);
    std::thread([handle, id]() {
      PipeIo io(handle);
      handle_connection(io, id);
      FlushFileBuffers(handle);
      DisconnectNamedPipe(handle);
      CloseHandle(handle);
    }).detach();
  }
}

void serve_tcp(int port) {
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    log_line("ERROR wsa");
    return;
  }
  SOCKET listener = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listener == INVALID_SOCKET) {
    log_line("ERROR socket " + std::to_string(WSAGetLastError()));
    return;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<u_short>(port));
  if (::bind(listener, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0) {
    log_line("ERROR bind " + std::to_string(WSAGetLastError()));
    return;
  }
  if (::listen(listener, SOMAXCONN) != 0) {
    log_line("ERROR listen " + std::to_string(WSAGetLastError()));
    return;
  }
  sockaddr_in bound{};
  int bound_size = sizeof(bound);
  getsockname(listener, reinterpret_cast<sockaddr *>(&bound), &bound_size);
  log_line("LISTENING " + std::to_string(ntohs(bound.sin_port)));
  std::atomic<int> next_id{1};
  while (true) {
    SOCKET client = ::accept(listener, nullptr, nullptr);
    if (client == INVALID_SOCKET) {
      log_line("ERROR accept " + std::to_string(WSAGetLastError()));
      return;
    }
    const int id = next_id.fetch_add(1);
    std::thread([client, id]() {
      SocketIo io(client);
      handle_connection(io, id);
      ::shutdown(client, SD_BOTH);
      ::closesocket(client);
    }).detach();
  }
}

#else

class SocketIo final : public Io {
public:
  explicit SocketIo(int socket) : socket_(socket) {}

  bool read_exact(char *data, std::size_t size) override {
    std::size_t offset = 0;
    while (offset < size) {
      const ssize_t got = ::recv(socket_, data + offset, size - offset, 0);
      if (got <= 0) {
        return false;
      }
      offset += static_cast<std::size_t>(got);
    }
    return true;
  }

  bool write_all(const char *data, std::size_t size) override {
    std::size_t offset = 0;
    while (offset < size) {
      const ssize_t sent = ::send(socket_, data + offset, size - offset, 0);
      if (sent <= 0) {
        return false;
      }
      offset += static_cast<std::size_t>(sent);
    }
    return true;
  }

private:
  int socket_;
};

void accept_loop(int listener) {
  std::atomic<int> next_id{1};
  while (true) {
    const int client = ::accept(listener, nullptr, nullptr);
    if (client < 0) {
      log_line("ERROR accept");
      return;
    }
    const int id = next_id.fetch_add(1);
    std::thread([client, id]() {
      SocketIo io(client);
      handle_connection(io, id);
      ::close(client);
    }).detach();
  }
}

void serve_unix(const std::string &path) {
  const int listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener < 0) {
    log_line("ERROR socket");
    return;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    log_line("ERROR path-too-long");
    return;
  }
  std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
  ::unlink(path.c_str());
  if (::bind(listener, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0) {
    log_line("ERROR bind");
    return;
  }
  if (::listen(listener, SOMAXCONN) != 0) {
    log_line("ERROR listen");
    return;
  }
  log_line("LISTENING " + path);
  accept_loop(listener);
}

void serve_tcp(int port) {
  const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    log_line("ERROR socket");
    return;
  }
  int reuse = 1;
  ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  if (::bind(listener, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0) {
    log_line("ERROR bind");
    return;
  }
  if (::listen(listener, SOMAXCONN) != 0) {
    log_line("ERROR listen");
    return;
  }
  sockaddr_in bound{};
  socklen_t bound_size = sizeof(bound);
  getsockname(listener, reinterpret_cast<sockaddr *>(&bound), &bound_size);
  log_line("LISTENING " + std::to_string(ntohs(bound.sin_port)));
  accept_loop(listener);
}

#endif

} // namespace

int main(int argc, char **argv) {
  std::string mode;
  std::string name;
  std::string path;
  int port = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--mode" && i + 1 < argc) {
      mode = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      name = argv[++i];
    } else if (arg == "--path" && i + 1 < argc) {
      path = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = std::atoi(argv[++i]);
    }
  }
#ifdef _WIN32
  if (mode == "pipe") {
    serve_pipe(name);
    return 0;
  }
  if (mode == "tcp") {
    serve_tcp(port);
    return 0;
  }
#else
  if (mode == "unix") {
    serve_unix(path);
    return 0;
  }
  if (mode == "tcp") {
    serve_tcp(port);
    return 0;
  }
#endif
  std::cerr << "usage: ipc_spike_server --mode pipe|unix|tcp [--name NAME] "
               "[--path PATH] [--port PORT]"
            << std::endl;
  return 2;
}
