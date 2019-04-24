#ifndef UVCPP_UTIL_H_
#define UVCPP_UTIL_H_
#include <cstdlib>
#include <cerrno>
#include <memory>
#include <functional>
#include <string>
#include <netdb.h>
#include <unistd.h>
#include <pwd.h>
#include "defs.h"
#include "util/log.hpp"

namespace {
  static const auto CPointerDeleter = [](void *p) { free(p); };
}

namespace uvcpp {
  using CPointerDeleterType = std::function<void(void *p)>;

  class Util {
    public:
      template <typename T>
      static auto makeCStructUniquePtr() {
        return std::unique_ptr<T, CPointerDeleterType>(
            reinterpret_cast<T *>(malloc(sizeof(T))), CPointerDeleter);
      }

      static std::size_t charCount(const std::string &s, char c) {
        auto count = 0;
        for (std::size_t i = 0; i < s.size(); ++i) {
          if (s[i] == c) {
            ++count;
          }
        }
        return count;
      }
  };

  class NetUtil {
    public:
      static void log_ipv4_and_port(void *ipv4, int port, const char *msg) {
        char data[INET_ADDRSTRLEN];
        uv_inet_ntop(AF_INET, ipv4, data, INET_ADDRSTRLEN);
        LOG_V("%s: %s:%d", msg, data, port);
      }

      static void log_ipv6_and_port(void *ipv6, int port, const char *msg) {
        char data[INET6_ADDRSTRLEN];
        uv_inet_ntop(AF_INET6, ipv6, data, INET6_ADDRSTRLEN);
        LOG_V("%s: [%s]:%d", msg, data, port);
      }

      static bool convertIPAddress(
          const std::string &host, uint16_t port, SockAddrStorage *sas) {
        return
          uv_ip4_addr(host.c_str(), port,
              reinterpret_cast<SockAddr4 *>(sas)) == 0 ||
          uv_ip6_addr(host.c_str(), port,
              reinterpret_cast<SockAddr6 *>(sas)) == 0;
      }

      static std::string ip(const struct sockaddr *addr) {
        char ipstr[INET6_ADDRSTRLEN];
        if (addr->sa_family == AF_INET) {
          struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
          uv_inet_ntop(addr4->sin_family, &addr4->sin_addr, ipstr, sizeof(ipstr));
          return ipstr;

        } else if (addr->sa_family == AF_INET6) {
          struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
          uv_inet_ntop(addr6->sin6_family, &addr6->sin6_addr, ipstr, sizeof(ipstr));
          return ipstr;
        }
        LOG_W("cannot extract ip address");

        return "";
      }

      static uint16_t port(const struct sockaddr *addr) {
        return ntohs(reinterpret_cast<const sockaddr_in *>(addr)->sin_port);
      }

      static int fillIPAddress(
          struct sockaddr *addr, int port,  char *ipstr, 
          int ipstr_len, struct addrinfo *ai) {
        if (ai->ai_family == AF_INET) {
          struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
          memcpy(addr4, ai->ai_addr, sizeof(struct sockaddr_in));
          addr4->sin_port = port;
          uv_inet_ntop(addr4->sin_family, &addr4->sin_addr, ipstr, ipstr_len);

        } else if (ai->ai_family == AF_INET6) {
          struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
          memcpy(addr6, ai->ai_addr, sizeof(struct sockaddr_in6));
          addr6->sin6_port = port;
          uv_inet_ntop(addr6->sin6_family, &addr6->sin6_addr, ipstr, ipstr_len);

        } else {
          LOG_W("unexpected ai_family: %d", ai->ai_family);
          return -1;
        }

        return 0;
      }

      static void copyIPAddress(struct sockaddr_storage *addr, struct addrinfo *ai) {
        memset(addr, 0, sizeof(struct sockaddr_storage));
        if (ai->ai_family == AF_INET) {
          memcpy(addr, ai->ai_addr, sizeof(struct sockaddr_in));

        } else if (ai->ai_family == AF_INET6) {
          memcpy(addr, ai->ai_addr, sizeof(struct sockaddr_in6));

        } else {
          LOG_W("unexpected ai_family: %d", ai->ai_family);
        }
      }

      static void toBinaryIPv4(uint32_t *intip, const char *ip) {
        *intip = (ip[0] << 24) + (ip[1] << 16) + (ip[2] << 8) + ip[3];
      }

      static int isIPv4Any(const char *ip) {
        return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
      }

      static int isIPv4Local(const char *ip) {
        return ip[0] == 127 && ip[1] == 0 && ip[2] == 0 && ip[3] == 1;
      }

      static int isIPv6Any(const char *ip) {
        for (int i = 0; i < 16; ++i) {
          if (ip[i] != 0) { 
            return 0;
          }
        }
        return 1;
      }

      static int isIPv6Local(const char *ip) {
        for (int i = 0; i < 15; ++i) {
          if (ip[i] != 0) {
            return 0;
          }
        }
        return ip[15] == 1;
      }

      //static int doSetUID(const char *user) {
        //const std::size_t pwd_buf_size = 8192;
        //struct passwd pwd, *result;
        //char pwd_buf[pwd_buf_size];
        //int status = getpwnam_r(user, &pwd, pwd_buf, pwd_buf_size, &result);
        //if (result == NULL) {
          //if (status > 0) {
            //LOG_E("getpwnam_r failed: %s", strerror(errno));
          //} else {
            //LOG_E("user %s not found", user);
          //}
          //return -1;
        //}

        //int euid = geteuid();
        //int egid = getegid();

        //status = setegid(pwd.pw_gid);
        //status = seteuid(pwd.pw_uid);
        //if (status == EINVAL || status == EPERM) {
          //LOG_E("setegid or seteuid failed");
          //return -1;
        //}

        //LOG_I("switched egid from %d to %d, euid from %d to %d", 
            //egid, getegid(), euid, geteuid());
        //return 0;
      //}

      //static void redirect_stderr_to_file(const char *log_file) {
        //FILE *old_stderr = stderr;
        //stderr = fopen(log_file, "w");
        //if (stderr == NULL) {
          //fprintf(old_stderr, "failed to open log file: %s", log_file);
          //exit(1);
        //}
      //}

  };

} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_UTIL_H_ */
