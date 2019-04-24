#ifndef UVCPP_HANDLE_H_
#define UVCPP_HANDLE_H_
#include "resource.hpp"
#include "util/buffer.hpp"

namespace uvcpp {

  struct EvClose : public Event { };
  struct EvBufferRecycled : public Event {
    EvBufferRecycled(std::unique_ptr<nul::Buffer> &&buffer) :
      buffer(std::forward<std::unique_ptr<nul::Buffer>>(buffer)) { }
    std::unique_ptr<nul::Buffer> buffer;
  };

  template <typename T, typename Derived>
  class Handle : public Resource<T, Derived> {
    public:
      Handle(const std::shared_ptr<Loop> &loop) : Resource<T, Derived>(loop) { }

      void close() {
        if (!uv_is_closing(reinterpret_cast<uv_handle_t *>(this->get()))) {
          uv_close((uv_handle_t *)this->get(), closeCallback);
        }
      }

      bool isValid() {
        return uv_is_closing(reinterpret_cast<uv_handle_t *>(this->get())) == 0;
      }

      virtual bool init() {
        this->template once<EvError>([this](const auto &e, auto &handle){
          this->close();
        });
        return true;
      }

      template<typename E>
      void on(EventCallback<E, Derived> &&callback) {
        if (std::is_same<E, EvClose>::value) {
          Resource<T, Derived>::template once<E>(
            std::forward<EventCallback<E, Derived>>(callback));
        } else {
          Resource<T, Derived>::template on<E>(
            std::forward<EventCallback<E, Derived>>(callback));
        }
      }

      template <typename U = Derived, typename ...Args>
      static auto createUnique(const std::shared_ptr<Loop> &loop, Args ...args) {
        auto handle = Resource<T, U>::template
          createUnique<U, Args...>(loop, std::forward<Args>(args)...);
        return handle->init() ? std::move(handle) : nullptr;
      }

      template <typename U = Derived, typename ...Args>
      static auto createShared(const std::shared_ptr<Loop> &loop, Args ...args) {
        auto handle = Resource<T, U>::template
          createShared<U, Args...>(loop, std::forward<Args>(args)...);
        return handle->init() ? handle : nullptr;
      }

    private:
      static void closeCallback(uv_handle_t *h) {
        reinterpret_cast<Handle *>(h->data)->template
          publish<EvClose>(EvClose{});
      }
  };

} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_HANDLE_H_ */
