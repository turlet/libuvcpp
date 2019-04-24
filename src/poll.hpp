#ifndef UVCPP_POLL_H_
#define UVCPP_POLL_H_
#include "handle.hpp"
#include "defs.h"
#include "util.hpp"

namespace uvcpp {
  
  struct EvPoll : public Event {
    EvPoll(int events) : events(events) {}
    int events;
  };

  class Poll : public Handle<uv_poll_t, Poll> {
    public:
      enum Event {
        READABLE    = 1,
        WRITABLE    = 2,
        DISCONNECT  = 4,
        PRIORITIZED = 8,
      };

      Poll(const std::shared_ptr<Loop> &loop) :
        Handle<uv_poll_t, Poll>(loop) { }

      bool initWithFd(int fd) {
        int err;
        if ((err = uv_poll_init(
              this->getLoop()->getRaw(),
              reinterpret_cast<uv_poll_t *>(get()),
              fd)) != 0) {
          this->reportError("uv_poll_init", err);

        } else {
          fd_ = fd;
        }
        return err == 0;
      }

      bool initWithSockHandle(SockHandle sockHandle) {
        int err;
        if ((err = uv_poll_init_socket(
              this->getLoop()->getRaw(),
              reinterpret_cast<uv_poll_t *>(get()),
              sockHandle)) != 0) {
          this->reportError("uv_poll_init", err);

        } else {
          fd_ = sockHandle;
        }
        return err == 0;
      }

      void poll(int events) {
        int err;
        if ((err = uv_poll_start(
              reinterpret_cast<uv_poll_t *>(this->get()),
              events, onPollCallback)) != 0) {
          this->reportError("uv_poll_start", err);
        }
      }

      void stop() {
        int err;
        if ((err = uv_poll_stop(
              reinterpret_cast<uv_poll_t *>(this->get()))) != 0) {
          this->reportError("uv_poll_stop", err);
        }
      }

      int getFd() const {
        return fd_;
      }

    private:
      static void onPollCallback(uv_poll_t *handle, int status, int events) {
        auto poll = reinterpret_cast<Poll *>(handle->data);
        if (status < 0) {
          poll->reportError("send", status);

        } else {
          poll->template publish<EvPoll>(EvPoll{ events });
        }
      }

    protected:
      int fd_{-1};
  };

} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_POLL_H_ */
