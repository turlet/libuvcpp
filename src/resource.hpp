#ifndef UVCPP_RESOURCE_H_
#define UVCPP_RESOURCE_H_
#include <memory>
#include <string>
#include <type_traits>
#include <functional>
#include <vector>
#include "uv.h"
#include "loop.hpp"
#include "util/log.hpp"

namespace uvcpp {

  struct Event {
    virtual ~Event() { }
  };
  struct EvRef : public Event { };
  struct EvDestroy : public Event { };
  struct EvError : public Event {
    EvError(int status) : status(status), message(uv_strerror(status)) { }
    int status;
    std::string message;
  };

  template <typename E, typename Derived>
  using EventCallback = std::function<void(const E &event, Derived &handle)>;

  struct CallbackInterface {
    virtual ~CallbackInterface() { }
  };

  template <typename E, typename Derived>
  struct Callback : public CallbackInterface {
    Callback(EventCallback<E, Derived> callback) : callback(callback) { }
    EventCallback<E, Derived> callback;
  };

  template <typename T, typename Derived>
  class Resource : public std::enable_shared_from_this<Resource<T, Derived>> {
    enum class CallbackType {
      ALWAYS,
      ONCE
    };

    public:
      using Type = T;
      explicit Resource(const std::shared_ptr<Loop> &loop) : loop_(loop) {
        resource_.data = this;
      }
      virtual ~Resource() {
        publish(EvDestroy{});
      }

      T *get() {
        return &resource_;
      }

      std::shared_ptr<Loop> getLoop() {
        return loop_;
      }

      using std::enable_shared_from_this<Resource<T, Derived>>::shared_from_this;
      // only call this method on std::shread_ptr
      template<typename E, typename = std::enable_if_t<std::is_base_of<Event, E>::value, E>>
      void sharedRefUntil() {
        registerCallback<E, CallbackType::ONCE>(
          [_ = shared_from_this()](const auto &, auto &){});
      }

      template<typename E, typename = std::enable_if_t<std::is_base_of<Event, E>::value, E>>
      void on(EventCallback<E, Derived> &&callback) {
        const auto cbType =
          (std::is_same<E, EvError>::value ||
           std::is_same<E, EvRef>::value ||
           std::is_same<E, EvDestroy>::value) ?
          CallbackType::ONCE :
          CallbackType::ALWAYS;

        registerCallback<E, cbType>(
          std::forward<EventCallback<E, Derived>>(callback));
      }

      template<typename E, typename = std::enable_if_t<std::is_base_of<Event, E>::value, E>>
      void once(EventCallback<E, Derived> &&callback) {
        registerCallback<E, CallbackType::ONCE>(
          std::forward<EventCallback<E, Derived>>(callback));
      }

      template<typename E, typename = std::enable_if_t<std::is_base_of<Event, E>::value, E>>
      void publish(E &&event) {
        if (!std::is_same<E, EvError>::value &&
            !std::is_same<E, EvRef>::value &&
            !std::is_same<E, EvDestroy>::value) {
          doCallback<E, CallbackType::ALWAYS>(std::forward<E>(event));
        }
        doCallback<E, CallbackType::ONCE>(std::forward<E>(event));
      }

      template <typename U = Derived, typename ...Args, typename =
        std::enable_if_t<std::is_base_of<Derived, U>::value, U>>
      static auto createUnique(const std::shared_ptr<Loop> &loop, Args ...args) {
        return std::make_unique<U>(loop, std::forward<Args>(args)...);
      }

      template <typename U = Derived, typename ...Args, typename =
        std::enable_if_t<std::is_base_of<Derived, U>::value, U>>
      static auto createShared(const std::shared_ptr<Loop> &loop, Args ...args) {
        return std::make_shared<U>(loop, std::forward<Args>(args)...);
      }

    protected:
      void reportError(const char *funName, int err) {
        publish<EvError>(EvError{ err });
      }

    private:
      template<
        typename E, CallbackType t,
        typename = std::enable_if_t<std::is_base_of<Event, E>::value, E>>
      void registerCallback(EventCallback<E, Derived> &&callback) {
        auto &vec = t == CallbackType::ALWAYS ? callbacks_ : onceCallbacks_;
        auto index = getEventTypeIndex<E, t>();
        if (index >= vec.size()) {
          vec.resize(index + 1);
        }
        vec[index].push_back(std::make_unique<Callback<E, Derived>>(
            std::forward<EventCallback<E, Derived>>(callback)));
      }

      template<typename E, CallbackType t>
      void doCallback(E &&event) {
        auto index = getEventTypeIndex<E, t>();
        if (t == CallbackType::ALWAYS) {
          if (index < callbacks_.size()) {
            auto &tmp = callbacks_[index];
            for (auto &c : tmp) {
              static_cast<Callback<E, Derived> *>(c.get())
                ->callback(std::forward<E>(event), static_cast<Derived &>(*this));
            }
          }
        } else if (index < onceCallbacks_.size()) {
          std::vector<std::unique_ptr<CallbackInterface>> tmp;
          onceCallbacks_[index].swap(tmp);
          for (auto &c : tmp) {
            static_cast<Callback<E, Derived> *>(c.get())
              ->callback(std::forward<E>(event), static_cast<Derived &>(*this));
          }
        }
      }

      template <CallbackType>
      static std::size_t countEventTypeIndex() {
        static std::size_t index = 0;
        return index++;
      }

      template <typename E, CallbackType t>
      static std::size_t getEventTypeIndex() {
        static std::size_t index = countEventTypeIndex<t>();
        return index;
      }
    
    private:
      std::shared_ptr<Loop> loop_;
      T resource_;
      std::vector<std::vector<std::unique_ptr<CallbackInterface>>> callbacks_{};
      std::vector<std::vector<std::unique_ptr<CallbackInterface>>> onceCallbacks_{};
  };
} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_RESOURCE_H_ */
