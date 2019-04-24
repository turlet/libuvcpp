#include <gtest/gtest.h>
#include "uvcpp.h"

using namespace uvcpp;

TEST(Udp, Simple) {
  const auto EXPECTED_DESTROY_COUNT = 2;
  auto destroyCount = 0;
  const auto EXPECTED_RECV_COUNT = 2;
  auto recvCount = 0;

  {
    auto loop = std::make_shared<Loop>();
    ASSERT_TRUE(loop->init());

    auto server = Udp<>::createUnique(loop);
    auto client = Udp<>::createUnique(loop);
    ASSERT_TRUE(!!server);
    ASSERT_TRUE(!!client);
    server->once<EvDestroy>([&destroyCount](const auto &e, auto &udp) {
      ++destroyCount;
    });
    client->once<EvDestroy>([&destroyCount](const auto &e, auto &udp) {
      ++destroyCount;
    });

    server->on<EvError>([](const auto &e, auto &udp) {
      FAIL() << "server failed with status: " << e.status;
    });
    client->on<EvError>([](const auto &e, auto &udp) {
      FAIL() << "client failed with status: " << e.status;
    });

    auto serverMsg = std::string{"greet from server!"};
    auto clientMsg = std::string{"greet from client!"};

    std::shared_ptr<Udp<>> acceptedClient;

    server->on<EvRecv>([&](const auto &e, auto &server) {
      ASSERT_STREQ(clientMsg.c_str(), e.buf);

      LOG_D("server received: %s", e.buf);
      ++recvCount;

      auto buf = std::make_unique<nul::Buffer>(serverMsg.size() + 1);
      buf->assign(serverMsg.c_str(), serverMsg.size());
      buf->getData()[serverMsg.size()] = '\0';

      server.send(std::move(buf), e.addr);
      server.close();
    });

    client->on<EvRecv>([&](const auto &e, auto &client) {
      ASSERT_STREQ(serverMsg.c_str(), e.buf);

      LOG_D("client received: %s", e.buf);
      ++recvCount;

      client.close();
      server->close();
    });

    ASSERT_TRUE(server->bind("127.0.0.1", 45678));

    server->recvStart();
    client->recvStart();

    auto buf = std::make_unique<nul::Buffer>(clientMsg.size() + 1);
    buf->assign(clientMsg.c_str(), clientMsg.size());
    buf->getData()[clientMsg.size()] = '\0';

    ASSERT_TRUE(client->send(std::move(buf), server->getLocalSockAddr()));

    loop->run();
  }

  ASSERT_EQ(recvCount, EXPECTED_RECV_COUNT);
  ASSERT_EQ(destroyCount, EXPECTED_DESTROY_COUNT);
}

SockHandle createSock() {
  return socket(AF_INET, SOCK_DGRAM, 0);
}

TEST(Udp, RawSocket) {
  const auto EXPECTED_DESTROY_COUNT = 2;
  auto destroyCount = 0;
  const auto EXPECTED_RECV_COUNT = 2;
  auto recvCount = 0;

  {
    auto loop = std::make_shared<Loop>();
    ASSERT_TRUE(loop->init());

    auto server = Udp<>::createUnique(loop);
    auto client = Udp<>::createUnique(loop);
    ASSERT_TRUE(!!server);
    ASSERT_TRUE(!!client);

    auto serverSock = createSock();
    auto clientSock = createSock();

    ASSERT_TRUE(serverSock > 0);
    ASSERT_TRUE(clientSock > 0);

    ASSERT_TRUE(server->open(serverSock));
    ASSERT_TRUE(client->open(clientSock));

    server->once<EvDestroy>([&destroyCount](const auto &e, auto &udp) {
      ++destroyCount;
    });
    client->once<EvDestroy>([&destroyCount](const auto &e, auto &udp) {
      ++destroyCount;
    });

    server->on<EvError>([](const auto &e, auto &udp) {
      FAIL() << "server failed with status: " << e.status;
    });
    client->on<EvError>([](const auto &e, auto &udp) {
      FAIL() << "client failed with status: " << e.status;
    });

    auto serverMsg = std::string{"greet from server!"};
    auto clientMsg = std::string{"greet from client!"};

    std::shared_ptr<Udp<>> acceptedClient;

    server->on<EvRecv>([&](const auto &e, auto &server) {
      ASSERT_STREQ(clientMsg.c_str(), e.buf);

      LOG_D("server received: %s", e.buf);
      ++recvCount;

      auto buf = std::make_unique<nul::Buffer>(serverMsg.size() + 1);
      buf->assign(serverMsg.c_str(), serverMsg.size());
      buf->getData()[serverMsg.size()] = '\0';

      server.send(std::move(buf), e.addr);
      server.close();
    });

    client->on<EvRecv>([&](const auto &e, auto &client) {
      ASSERT_STREQ(serverMsg.c_str(), e.buf);

      LOG_D("client received: %s", e.buf);
      ++recvCount;

      client.close();
      server->close();
    });

    ASSERT_TRUE(server->bind("127.0.0.1", 45678));

    server->recvStart();
    client->recvStart();

    auto buf = std::make_unique<nul::Buffer>(clientMsg.size() + 1);
    buf->assign(clientMsg.c_str(), clientMsg.size());
    buf->getData()[clientMsg.size()] = '\0';

    ASSERT_TRUE(client->send(std::move(buf), server->getLocalSockAddr()));

    loop->run();
  }

  ASSERT_EQ(recvCount, EXPECTED_RECV_COUNT);
  ASSERT_EQ(destroyCount, EXPECTED_DESTROY_COUNT);
}
