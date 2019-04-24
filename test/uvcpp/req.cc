#include <gtest/gtest.h>
#include "uvcpp.h"

using namespace uvcpp;

TEST(Req, DNSRequestResolveLocalHost) {
  auto loop = std::make_shared<Loop>();
  ASSERT_TRUE(loop->init());

  DNSRequest req{loop};
  req.on<EvError>([](const auto &e, auto &r) {
    FAIL() << "failed with status: " << e.status;
  });

  req.once<EvDNSResult>([](const auto &e, auto &tcp) {
    ASSERT_GT(e.dnsResults.size(), 0);
    for (auto &ip : e.dnsResults) {
      auto result = "::1" == ip || "127.0.0.1" == ip;
      ASSERT_TRUE(result);
    }
  });
  req.resolve("localhost");

  loop->run();
}

TEST(Req, DNSRequest0000) {
  auto loop = std::make_shared<Loop>();
  ASSERT_TRUE(loop->init());

  DNSRequest req{loop};
  req.on<EvError>([](const auto &e, auto &r) {
    FAIL() << "failed with status: " << e.status;
  });

  req.once<EvDNSResult>([](const auto &e, auto &tcp) {
    ASSERT_EQ(e.dnsResults.size(), 1);
    ASSERT_STREQ("0.0.0.0", e.dnsResults[0].c_str());
  });
  req.resolve("0.0.0.0");

  loop->run();
}
