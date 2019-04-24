#include <gtest/gtest.h>
#include "uvcpp.h"

using namespace uvcpp;

TEST(Prepare, LoopCount) {
  auto loop = std::make_shared<Loop>();
  ASSERT_TRUE(loop->init());

  auto prepare = Prepare::createUnique(loop);

  prepare->on<EvError>([](const auto &e, auto &prepare) {
    FAIL() << "prepare failed with status: " << e.status;
  });
  prepare->on<EvClose>([](const auto &e, auto &prepare) {
    LOG_D("prepare closed");
  });

  const auto CHECK_COUNT = 1;
  auto count = 0;
  prepare->on<EvPrepare>([&count](const auto &e, auto &prepare) {
    LOG_D("counting: %d", count);
    if (++count == CHECK_COUNT) {
      prepare.stop();
      prepare.close();
    }
  });

  prepare->start();

  loop->run();

  ASSERT_EQ(count, CHECK_COUNT);
}

