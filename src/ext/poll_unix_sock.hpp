#ifndef UVCPP_POLL_UNIX_SOCK_H_
#define UVCPP_POLL_UNIX_SOCK_H_
#include "poll.hpp"
#include <sys/un.h>

namespace uvcpp {
  struct EvPollAccept : public Event {
    EvPollAccept(std::unique_ptr<Poll> poll) :
      poll(std::move(poll)) {}
    std::unique_ptr<Poll> poll;
  };

  class PollUnixSock : public Poll {
    public:
      PollUnixSock(const std::shared_ptr<Loop> &loop) : Poll(loop) {
        memset(&sockAddr_, 0, sizeof(sockAddr_));
        sockAddr_.sun_family = AF_UNIX;
      }

      template <typename U = PollUnixSock, typename ...Args>
      static auto createUnique(const std::shared_ptr<Loop> &loop, Args ...args) {
        auto handle = Resource<uv_poll_t, U>::template
          createUnique<U, Args...>(loop, std::forward<Args>(args)...);
        return handle->init() ? std::move(handle) : nullptr;
      }

      template <typename U = PollUnixSock, typename ...Args>
      static auto createShared(const std::shared_ptr<Loop> &loop, Args ...args) {
        auto handle = Resource<uv_poll_t, U>::template
          createShared<U, Args...>(loop, std::forward<Args>(args)...);
        return handle->init() ? handle : nullptr;
      }

      bool bind(const std::string &sockPath) {
        if ((fd_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
          LOG_E("socket() failed: %s", strerror(errno));
          return false;
        }

        initWithSockHandle(fd_);

        strncpy(
          sockAddr_.sun_path,
          sockPath.c_str(),
          sizeof(sockAddr_.sun_path) - 1);

        if (::bind(fd_, (SockAddr *)&sockAddr_, sizeof(sockAddr_)) == -1) {
          LOG_E("bind() failed with sock_path (%s): %s",
                sockPath.c_str(), strerror(errno));
          return false;
        }
        return true;
      }

      bool listen(int backlog) {
        this->on<EvPoll>([this](const auto &e, auto &p){
          if (e.events & Poll::Event::READABLE) {

            struct sockaddr_un c_addr;
            socklen_t len = sizeof(c_addr);
            int cSock;

            if ((cSock = accept(fd_, (SockAddr *)&c_addr, &len)) == -1) {
              LOG_W("accept() failed: %s", strerror(errno));

            } else {
              auto cPoll = Poll::createUnique(p.getLoop());
              cPoll->initWithSockHandle(cSock);

              this->publish<EvPollAccept>(
                EvPollAccept{ std::move(cPoll) });
            }
          }
        });

        if (::listen(fd_, backlog) == -1) {
          LOG_E("listen() failed: %s", strerror(errno));
          return false;
        }
        return true;
      }

      bool connect(const std::string &sockPath) {
        if ((fd_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
          LOG_E("socket() failed: %s", strerror(errno));
          return false;
        }
        initWithSockHandle(fd_);

        strncpy(
          sockAddr_.sun_path,
          sockPath.c_str(),
          sizeof(sockAddr_.sun_path) - 1);

        if (::connect(fd_, (SockAddr *)&sockAddr_, sizeof(sockAddr_)) == -1) {
          LOG_E("connect() failed with sock_path (%s): %s",
                sockPath.c_str(), strerror(errno));
          return false;
        }
        return true;
      }

      int getSocket() const {
        return fd_;
      }

    private:
      struct sockaddr_un sockAddr_;
  };
} /* end of namspace: uvcpp */


#endif /* end of include guard: UVCPP_POLL_UNIX_SOCK_H_ */
