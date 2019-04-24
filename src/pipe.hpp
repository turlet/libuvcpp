#ifndef UVCPP_PIPE_H_
#define UVCPP_PIPE_H_
#include "tcp.hpp"
#include <cassert>

namespace uvcpp {

  class Pipe : public Stream<uv_pipe_t, Pipe> {
    public:
      Pipe(const std::shared_ptr<Loop> &loop, bool crossProcess = true) :
        Stream(loop), crossProcess_(crossProcess) { }

      virtual bool init() override {
        auto rawLoop = this->getLoop()->getRaw();
        int err = 0;
        if (!Stream::init() ||
            (err = uv_pipe_init(rawLoop, get(), crossProcess_ ? 1 : 0)) != 0) {
          LOG_E("failed to init Pipe, reason: %s", uv_strerror(err));
          return false;
        }

        // stream handles can be sent over pipe between different processes
        // or threads, stream handles can be sent with uv_write2, in the
        // receiving end of the pipe, we can listen for EvRead events, and
        // check pending handles with uv_pipe_pending_count(), if it is 1,
        // it means there's a pending stream handle for the receiving end
        // to accept
        this->template once<EvRead>([this](const auto &e, auto &handle){
          auto pendingCount = uv_pipe_pending_count(this->get());
          if (pendingCount != 1) {
            return;
          }

          std::unique_ptr<Stream> conn = nullptr;
          auto handleType = uv_pipe_pending_type(this->get());

          if (handleType == UV_TCP) {
            auto tcp = Tcp::createUnique(this->getLoop());
            if (!tcp) {
              return;
            }

            // accept the Tcp handle passed through the current pipe
            // from another process or thread
            int err;
            if ((err = uv_accept(
                  reinterpret_cast<uv_stream_t *>(this->get()),
                  reinterpret_cast<uv_stream_t *>(tcp->get()))) != 0) {
              LOG_E("uv_accept failed: %s", uv_strerror(err));
              return;
            }

            int len = sizeof(tcp->sas_);
            if ((err = uv_tcp_getpeername(
                  tcp->get(),
                  reinterpret_cast<SockAddr *>(&tcp->sas_), &len)) != 0 &&
              (err = uv_tcp_getsockname(
                  tcp->get(),
                  reinterpret_cast<SockAddr *>(&tcp->sas_), &len)) != 0) {
              LOG_E("uv_tcp_getsockname/uv_tcp_getpeername failed: %s",
                    uv_strerror(err));

              std::shared_ptr<Tcp> sharedClient = std::move(tcp);
              sharedClient->sharedRefUntil<EvClose>();
              sharedClient->close();
              return;
            }

            LOG_V("tcp: %s:%d", tcp->getIP().c_str(), tcp->getPort());
            this->publish<EvAccept<Tcp>>(EvAccept<Tcp>{ std::move(tcp) });

          } else if (handleType == UV_NAMED_PIPE) {
            this->doAccept();

          } else {
            LOG_W("unexpected handle type: %d", handleType);
          }
        });
        return true;
      }

      bool bind(const std::string &name) {
        name_ = name;

        std::unique_ptr<Stream> handle;
        int err = -1;
        if ((err = uv_pipe_bind(get(), name.c_str())) != 0) {
          LOG_W("failed to bind on [%s], reason: %s",
                name.c_str(), uv_strerror(err));
        } else {
          LOG_I("server bound on [%s]", name.c_str());
        }

        return err == 0;
      }

      void connect(const std::string &name) {
        name_ = name;
        if (!connectReq_) {
          connectReq_ = ConnectReq::createUnique(this->getLoop());
        }
        uv_pipe_connect(connectReq_->get(), get(), name.c_str(), onConnect);
      }

      bool sendTcpHandle(Tcp &tcp) {
        return sendHandle(reinterpret_cast<uv_stream_t *>(tcp.get()));
      }

      bool sendPipeHandle(Pipe &pipe) {
        return sendHandle(reinterpret_cast<uv_stream_t *>(pipe.get()));
      }

      std::string getName() {
        return name_;
      }

    protected:
      virtual void doAccept() override {
        auto pipe = Pipe::createUnique(this->getLoop());
        if (!pipe) {
          return;
        }

        int err;
        if ((err = uv_accept(reinterpret_cast<uv_stream_t *>(get()),
                reinterpret_cast<uv_stream_t *>(pipe->get()))) != 0) {
          LOG_E("uv_accept failed: %s", uv_strerror(err));
          return;
        }

        char name[PATH_MAX];
        std::size_t nameLength = sizeof(name);
        int size = 0;
        if ((size = uv_pipe_getpeername(pipe->get(),
              name, &nameLength)) == UV_ENOBUFS) {
          LOG_W("uv_pipe_getpeername failed, UV_ENOBUFS");
        } else if (size > 0 && size < PATH_MAX) {
          pipe->name_ = std::string(name, size);
        }

        LOG_V("pipe: [%s]", pipe->getName().c_str());
        publish<EvAccept<Pipe>>(EvAccept<Pipe>{ std::move(pipe) });
      }

    private:
      static void onConnect(uv_connect_t *req, int status) {
        auto pipe = reinterpret_cast<Pipe *>(req->handle->data);
        if (status < 0) {
          LOG_E("onConnect failed: %s", uv_strerror(status));
          pipe->reportError("uv_pipe_connect", status);
        } else {
          pipe->template publish<EvConnect>(EvConnect{});
        }
      }

    private:
      bool crossProcess_;
      std::unique_ptr<ConnectReq> connectReq_{nullptr};
      std::string name_;
  };
} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_PIPE_H_ */
