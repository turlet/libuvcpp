#ifndef UVCPP_TCP_H_
#define UVCPP_TCP_H_
#include "stream.hpp"

namespace uvcpp {
  class Tcp : public Stream<uv_tcp_t, Tcp> {
    // in Pipe class, sas_ may be accessed when a Tcp handle is accepted there
    friend class Pipe;

    public:
      enum class Domain {
        UNSPEC = AF_UNSPEC,
        INET = AF_INET,
        INET6 = AF_INET6
      };

      Tcp(const std::shared_ptr<Loop> &loop, Domain domain = Domain::UNSPEC) :
        Stream(loop), domain_(domain) { }

      virtual bool init() override {
        auto rawLoop = this->getLoop()->getRaw();
        int err = 0;
        if (!Stream::init() ||
            (err = uv_tcp_init_ex(
                rawLoop, get(), static_cast<int>(domain_))) != 0) {
          LOG_E("failed to init Tcp, reason: %s", uv_strerror(err));
          return false;
        }
        return true;
      }

      bool bind(SockAddr *sa) {
        int err = -1;
        if (sa->sa_family == AF_INET || sa->sa_family == AF_INET6) {
          //// the port starts from sa_data
          //*reinterpret_cast<uint16_t *>(sa->sa_data) = htons(port);

          memcpy(&sas_, sa, sa->sa_family == AF_INET ?
              sizeof(SockAddr4) : sizeof(SockAddr6));

          if ((err = uv_tcp_bind(get(), sa, 0)) != 0) {
            LOG_W("failed to bind on %s:%d, reason: %s",
                getIP().c_str(), getPort(), uv_strerror(err));
          } else {
            LOG_I("server bound on %s:%d", getIP().c_str(), getPort());
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

      bool connect(SockAddr *sa) {
        if (!connectReq_) {
          connectReq_ = ConnectReq::createUnique(this->getLoop());
        }

        memcpy(&sas_, sa, sa->sa_family == AF_INET ?
               sizeof(SockAddr4) : sizeof(SockAddr6));

        //// the port starts from sa_data
        //*reinterpret_cast<uint16_t *>(sa->sa_data) = htons(port);
        //connectReq_->setData(this);

        int err = -1;
        if ((err = uv_tcp_connect(
              connectReq_->get(), get(), sa, onConnect)) != 0) {
          LOG_W("failed to connect to %s:%d, reason: %s",
                getIP().c_str(), getPort(), uv_strerror(err));
        }

        return err == 0;
      }

      bool connect(const std::string &ip, uint16_t port) {
        if (NetUtil::convertIPAddress(ip, port, &sas_)) {
          return connect(reinterpret_cast<SockAddr *>(&sas_));

        } else {
          LOG_E("[%s] is not a valid ip address", ip.c_str());
          return false;
        }
      }

      void setKeepAlive(bool enable) {
        int err;
        if ((err = uv_tcp_keepalive(get(), enable ? 1 : 0, 60)) != 0) {
          this->reportError("uv_tcp_keepalive", err);
        }
      }

      void setNoDelay(bool enable) {
        int err;
        if ((err = uv_tcp_nodelay(get(), enable ? 1 : 0)) != 0) {
          this->reportError("uv_tcp_nodelay", err);
        }
      }

      void setSockOption(int option, void *value, socklen_t optionLength) {
        uv_os_fd_t fd;
        if (uv_fileno(reinterpret_cast<uv_handle_t *>(get()),
              &fd) == UV_EBADF) {
          LOG_W("uv_fileno failed on server_tcp");
        } else {
          int err;
          if ((err = setsockopt(
                fd, SOL_SOCKET, option, value, optionLength)) == -1) {
            this->reportError("setsockopt", err);
          }
        }
      }

      const SockAddr *getSockAddr() const {
        return reinterpret_cast<const SockAddr *>(&sas_);
      }

      std::string getIP() const {
        return NetUtil::ip(reinterpret_cast<const SockAddr *>(&sas_));
      }

      uint16_t getPort() const {
        return NetUtil::port(reinterpret_cast<const SockAddr *>(&sas_));
      }

    protected:
      virtual void doAccept() override {
        auto client = Tcp::createUnique(this->getLoop());
        if (!client) {
          return;
        }

        int err;
        if ((err = uv_accept(reinterpret_cast<uv_stream_t *>(get()),
                reinterpret_cast<uv_stream_t *>(client->get()))) != 0) {
          LOG_E("uv_accept failed: %s", uv_strerror(err));
          return;
        }

        int len = sizeof(client->sas_);
        if ((err = uv_tcp_getpeername(client->get(),
              reinterpret_cast<SockAddr *>(&client->sas_), &len)) != 0) {
          LOG_E("uv_tcp_getpeername failed: %s", uv_strerror(err));
          std::shared_ptr<Tcp> sharedClient = std::move(client);
          sharedClient->sharedRefUntil<EvClose>();
          sharedClient->close();
          return;
        }

        LOG_V("client: %s:%d", client->getIP().c_str(), client->getPort());
        publish<EvAccept<Tcp>>(EvAccept<Tcp>{ std::move(client) });
      }

    private:
      static void onConnect(uv_connect_t *req, int status) {
        auto tcp = reinterpret_cast<Tcp *>(req->handle->data);
        if (status < 0) {
          LOG_E("onConnect failed: %s", uv_strerror(status));
          tcp->reportError("uv_tcp_connect", status);
        } else {
          tcp->template publish<EvConnect>(EvConnect{});
        }
      }

    private:
      Domain domain_;
      std::unique_ptr<ConnectReq> connectReq_{nullptr};
      SockAddrStorage sas_;
  };
} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_TCP_H_ */
