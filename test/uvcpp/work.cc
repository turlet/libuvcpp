#include <gtest/gtest.h>
#include "uvcpp.h"

using namespace uvcpp;

TEST(Work, Repeat) {
  auto loop = std::make_shared<Loop>();
  ASSERT_TRUE(loop->init());

  auto work = Work::createUnique(loop);
  ASSERT_TRUE(!!work);

  work->on<EvError>([](const auto &e, auto &work) {
    FAIL() << "work failed with status: " << e.status;
  });
  work->on<EvClose>([](const auto &e, auto &work) {
    LOG_D("work closed");
  });

  const auto CHECK_COUNT = 2;
  auto count = 0;
  work->on<EvWork>([&count](const auto &e, auto &work) {
    ++count;
  });
  work->on<EvAfterWork>([&count](const auto &e, auto &work) {
    ++count;
  });

  work->start();

  loop->run();

  ASSERT_EQ(count, CHECK_COUNT);
}

