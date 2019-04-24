#ifndef UVCPP_UDP_H_
#define UVCPP_UDP_H_
#include "handle.hpp"
#include "defs.h"
#include "util.hpp"
#include "req.hpp"
#include <deque>

namespace uvcpp {
  
  struct EvSend : public Event { };
  struct EvRecv : public Event {
    EvRecv(const char *buf, ssize_t nread, const SockAddr *addr) :
      buf(buf), nread(nread), addr(addr) { }

    const char *buf;
    ssize_t nread;
    const SockAddr *addr;
  };

  template <int PACKET_BUF = 32768>
  class Udp : public Handle<uv_udp_t, Udp<PACKET_BUF>> {
    public:
      Udp(const std::shared_ptr<Loop> &loop) :
        Handle<uv_udp_t, Udp<PACKET_BUF>>(loop) { }

      virtual bool init() override {
        if (uv_udp_init(this->getLoop()->getRaw(), this->get()) != 0) {
          LOG_E("uv_udp_init failed");
          return false;
        }

        this->template once<EvError>([this](const auto &e, auto &udp){
          if (!pendingReqs_.empty()) {
            for (auto &r : pendingReqs_) {
              this->template publish<EvBufferRecycled>(
                EvBufferRecycled{ std::move(r->buffer) });
            }
            pendingReqs_.clear();
          }
        });
        return true;
      }

      void recvStart() {
        int err;
        if ((err = uv_udp_recv_start(
              reinterpret_cast<uv_udp_t *>(this->get()),
              onAllocCallback, onRecvCallback)) != 0) {
          this->reportError("uv_udp_recv_start", err);
        }
      }

      void recvStop() {
        int err;
        if ((err = uv_udp_recv_stop(
              reinterpret_cast<uv_udp_t *>(this->get()))) != 0) {
          this->reportError("uv_udp_recv_stop", err);
        }
      }

      bool bind(SockAddr *sa) {
        int err = -1;
        if (sa->sa_family == AF_INET || sa->sa_family == AF_INET6) {
          //// the port starts from sa_data
          //*reinterpret_cast<uint16_t *>(sa->sa_data) = htons(port);

          if ((err = uv_udp_bind(this->get(), sa, 0)) != 0) {
            this->reportError("uv_udp_bind", err);
            LOG_W("failed to bind on %s:%d, reason: %s",
                  NetUtil::ip(reinterpret_cast<SockAddr *>(sa)).c_str(),
                  NetUtil::port(reinterpret_cast<SockAddr *>(sa)),
                  uv_strerror(err));
          }
        }

        return err == 0;
      }

      bool bind(const std::string &ip, uint16_t port) {
        SockAddrStorage sas;
        if (NetUtil::convertIPAddress(ip, port, &sas)) {
          return bind(reinterpret_cast<SockAddr *>(&sas));
        } else {
          LOG_E("[%s] is not a valid ip address", ip.c_str());
          return false;
        }
      }

      bool open(SockHandle sockHandle) {
        int err = -1;
        if ((err = uv_udp_open(this->get(), sockHandle)) != 0) {
          this->reportError("uv_udp_open", err);
        }
        return err == 0;
      }

      bool send(std::unique_ptr<nul::Buffer> buffer, const SockAddr *sa) {
        auto rawBuffer = buffer->asPod();

        auto req = UdpSendReq::createUnique(this->getLoop(), std::move(buffer));
        auto rawReq = req->get();

        pendingReqs_.push_back(std::move(req));

        int err;
        if ((err = uv_udp_send(
              rawReq,
              reinterpret_cast<uv_udp_t *>(this->get()),
              reinterpret_cast<uv_buf_t *>(rawBuffer),
              1, sa, onSendCallback)) != 0) {
          this->reportError("uv_udp_send", err);
          return false;
        }
        return true;
      }

      bool send(std::unique_ptr<nul::Buffer> buffer) {
        if (!sas_) {
          return false;
          LOG_E("call setDesitinationAddr to set the address first");
        }

        return send(std::move(buffer), sas_.get());
      }

      void setDesitinationAddr(const std::string &ip, uint16_t port) {
        SockAddrStorage sas;
        if (NetUtil::convertIPAddress(ip, port, &sas)) {
          setDesitinationAddr(&sas);

        } else {
          LOG_E("[%s] is not a valid ip address", ip.c_str());
        }
      }

      const SockAddr *getLocalSockAddr() {
        fillLocalSockAddrIfNeeded();
        return localSa_.get();
      }

      void setDesitinationAddr(SockAddrStorage *addr) {
        auto p = sas_.get();
        if (!p) {
          p = reinterpret_cast<SockAddr *>(malloc(sizeof(addr)));
          sas_.reset(p);
        }
        memcpy(p, &addr, sizeof(addr));
      }

      std::string getIP() {
        fillLocalSockAddrIfNeeded();
        return localSa_ ?
          NetUtil::ip(reinterpret_cast<SockAddr *>(localSa_.get())) : "";
      }

      uint16_t getPort() {
        fillLocalSockAddrIfNeeded();
        return localSa_ ?
          NetUtil::port(reinterpret_cast<SockAddr *>(localSa_.get())) : 0;
      }

    private:
      void fillLocalSockAddrIfNeeded() {
        if (!localSa_) {
          SockAddrStorage sas;
          int len;
          if (uv_udp_getsockname(
              reinterpret_cast<uv_udp_t *>(this->get()),
              reinterpret_cast<SockAddr *>(&sas), &len) == 0) {
            auto p = reinterpret_cast<SockAddr *>(malloc(sizeof(sas)));
            memcpy(p, &sas, sizeof(sas));
            localSa_.reset(p);

          } else {
            LOG_W("uv_udp_getsockname failed, handle may not be bound");
          }
        }
      }

      static void onAllocCallback(
          uv_handle_t *handle, std::size_t size, uv_buf_t *buf) {
        auto udp = reinterpret_cast<Udp *>(handle->data);
        buf->base = udp->recvBuf_;
        buf->len = sizeof(udp->recvBuf_);
      }

      static void onRecvCallback(
          uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf,
          const SockAddr* addr, unsigned int flags) {
        if (nread == 0 && !addr) {
          // nothing to read
          return;
        }

        auto udp = reinterpret_cast<Udp *>(handle->data);
        if (nread < 0) {
          LOG_E("TCP read failed: %s", uv_strerror(nread));
          udp->close();
          return;
        }

        // nread may be 0 for empty packet
        udp->template publish<EvRecv>(EvRecv{ buf->base, nread, addr });
      }

      static void onSendCallback(uv_udp_send_t *req, int status) {
        auto udp = reinterpret_cast<Udp *>(req->handle->data);
        if (!udp->pendingReqs_.empty()) {
          auto req = std::move(udp->pendingReqs_.front());
          udp->pendingReqs_.pop_front();

          udp->template publish<EvBufferRecycled>(
            EvBufferRecycled{ std::move(req->buffer) });
        }

        if (status < 0) {
          udp->reportError("send", status);
        } else {
          udp->template publish<EvSend>(EvSend{});
        }
      }

    private:
      std::deque<std::unique_ptr<UdpSendReq>> pendingReqs_{};
      std::unique_ptr<SockAddr, CPointerDeleterType> localSa_{nullptr, CPointerDeleter};
      std::unique_ptr<SockAddr, CPointerDeleterType> sas_{nullptr, CPointerDeleter};
      char recvBuf_[PACKET_BUF];
  };

} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_UDP_H_ */
