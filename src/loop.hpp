#ifndef UVCPP_LOOP_H_
#define UVCPP_LOOP_H_
#include "uv.h"
#include <memory> 
#include <functional>

namespace uvcpp {

  class Loop final {
    public:
      virtual ~Loop() {
        uv_loop_close(&loop_);
      }

      bool init() {
        return uv_loop_init(&loop_) == 0;
      }

      uv_loop_t *getRaw() {
        return &loop_;
      }

      void run() {
        uv_run(&loop_, UV_RUN_DEFAULT);
      }

      void stop() {
        uv_stop(&loop_);
      }
    
    protected:
      uv_loop_t loop_;
  };
} /* end of namspace: uvcpp */

#endif /* end of include guard: UVCPP_LOOP_H_ */
