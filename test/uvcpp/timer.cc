#include <gtest/gtest.h>
#include "uvcpp.h"

using namespace uvcpp;

//TEST(Timer, Repeat) {
  //auto loop = std::make_shared<Loop>();
  //ASSERT_TRUE(loop->init());

  //auto timer = Timer::createUnique(loop);
  //ASSERT_TRUE(!!timer);

  //timer->on<EvError>([](const auto &e, auto &timer) {
    //FAIL() << "timer failed with status: " << e.status;
  //});
  //timer->on<EvClose>([](const auto &e, auto &timer) {
    //LOG_D("timer closed");
  //});
  //timer->once<EvDestroy>([](const auto &e, auto &timer) {
    //LOG_D("timer destroyed");
  //});

  //const auto CHECK_COUNT = 5;
  //auto count = 0;
  //timer->on<EvTimer>([&count](const auto &e, auto &timer) {
    //LOG_D("counting: %d", count);
    //if (++count == CHECK_COUNT) {
      //timer.stop();
      //timer.close();
    //}
  //});

  //timer->start(0, 10);

  //loop->run();

  //ASSERT_EQ(count, CHECK_COUNT);
//}

TEST(Timer, RepeatShared) {
  auto destroyed = false;
  {
    auto loop = std::make_shared<Loop>();
    ASSERT_TRUE(loop->init());

    auto timer = Timer::createShared(loop);
    ASSERT_TRUE(!!timer);

    timer->on<EvError>([](const auto &e, auto &timer) {
      FAIL() << "timer failed with status: " << e.status;
    });
    // intentionally capture the shared_ptr to produce shared_ptr reference
    // cycle, but this callback will be registered as ONCE callback even
    // though 'on<>' instead of 'once<>' is used, when it is called,
    // the callback itself gets deleted, so the Timer object will be released,
    // and EvDestroy event will be fired
    timer->on<EvClose>([sharedTimer = timer](const auto &e, auto &timer) {
      LOG_D("timer closed: %li", sharedTimer.use_count());
    });
    timer->once<EvDestroy>([&destroyed, t = &timer](const auto &e, auto &timer) {
      LOG_D("timer destroyed: %li", t->use_count());
      destroyed = true;
    });

    auto count = 0;
    timer->on<EvTimer>([&count, t = &timer](const auto &e, auto &timer) {
      LOG_D("timer event: %li", t->use_count());
      timer.stop();
      timer.close();
    });

    timer->start(10, 0);

    loop->run();
    LOG_D("timer count: %li", timer.use_count());
  }

  ASSERT_TRUE(destroyed);
}

