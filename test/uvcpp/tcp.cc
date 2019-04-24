#include <gtest/gtest.h>
#include "uvcpp.h"
#include "uv.h"

using namespace uvcpp;

void testSharedRef(const std::shared_ptr<Loop> &loop) {
  auto tcp = Tcp::createShared(loop, Tcp::Domain::INET);
  tcp->sharedRefUntil<EvClose>();
  tcp->close();
}

TEST(Tcp, Connection) {
  const auto EXPECTED_DESTROY_COUNT = 3;
  auto destroyCount = 0;
  const auto EXPECTED_WRITE_COUNT = 1;
  auto writeCount = 0;

  {
    auto loop = std::make_shared<Loop>();
    ASSERT_TRUE(loop->init());

    testSharedRef(loop);

    auto server = Tcp::createUnique(loop, Tcp::Domain::INET);
    auto client = Tcp::createUnique(loop);
    ASSERT_TRUE(!!server);
    ASSERT_TRUE(!!client);
    server->once<EvDestroy>([&destroyCount](const auto &e, auto &tcp) {
      ++destroyCount;
    });
    client->once<EvDestroy>([&destroyCount](const auto &e, auto &tcp) {
      ++destroyCount;
    });

    server->on<EvError>([](const auto &e, auto &tcp) {
      FAIL() << "server failed with status: " << e.status;
    });
    client->on<EvError>([](const auto &e, auto &tcp) {
      FAIL() << "client failed with status: " << e.status;
    });
    client->on<EvClose>([](const auto &e, auto &handle) {
      LOG_D("client closed: isValid=%d", handle.isValid());
    });

    auto serverMsg = std::string{"greet from server!"};
    auto clientMsg = std::string{"greet from client!"};

    std::shared_ptr<Tcp> acceptedClient;

    server->on<EvAccept<Tcp>>([&](const auto &e, auto &server) {
      auto buf = std::make_unique<nul::Buffer>(serverMsg.size() + 1);
      buf->assign(serverMsg.c_str(), serverMsg.size());
      if (!e.client->writeAsync(std::move(buf))) {
        return;
      }

      e.client->template on<EvShutdown>([&](const auto &e, auto &client) {
        LOG_D("received client shutdown");
      });
      e.client->shutdown();

      e.client->readStart();
      e.client->template on<EvRead>([&](const auto &e, auto &client) {
        ((char *)e.buf)[e.nread] = '\0';
        ASSERT_STREQ(clientMsg.c_str(), e.buf);

        LOG_D("server received: %s", e.buf);

        acceptedClient->close();
      });
      acceptedClient = std::move(const_cast<EvAccept<Tcp> &>(e).client);
      acceptedClient->once<EvDestroy>([&destroyCount](const auto &e, auto &tcp) {
        ++destroyCount;
      });
    });

    client->on<EvConnect>([&clientMsg, &writeCount](const auto &e, auto &client) {
      LOG_D("client connected");
      client.template on<EvBufferRecycled>([&](const auto &e, auto &client) {
        LOG_D("buffer recycled: %p", e.buffer.get());
      });
      client.template once<EvWrite>([&](const auto &e, auto &client) {
        ++writeCount;
      });

      auto buf = std::make_unique<nul::Buffer>(clientMsg.size() + 1);
      buf->assign(clientMsg.c_str(), clientMsg.size());
      if (!client.writeAsync(std::move(buf))) {
        return;
      }

      client.template on<EvShutdown>([&](const auto &e, auto &client) {
        LOG_D("client shutdown");
      });
      client.shutdown();

      client.readStart();
    });

    client->on<EvRead>([&](const auto &e, auto &client){
      ((char *)e.buf)[e.nread] = '\0';
      ASSERT_STREQ(serverMsg.c_str(), e.buf);

      LOG_D("client received: %s", e.buf);

      client.close();
      server->close();
    });

    int on = 1;
    server->setSockOption(
      SO_REUSEADDR, reinterpret_cast<void *>(&on), sizeof(on));
    server->setSockOption(
      SO_REUSEPORT, reinterpret_cast<void *>(&on), sizeof(on));

    ASSERT_TRUE(server->bind("127.0.0.1", 22334));
    ASSERT_TRUE(server->listen(50));
    ASSERT_TRUE(client->connect(server->getIP(), 22334));

    loop->run();
  }

  ASSERT_EQ(writeCount, EXPECTED_WRITE_COUNT);
  ASSERT_EQ(destroyCount, EXPECTED_DESTROY_COUNT);
}

TEST(Tcp, ImmediateClose) {
  auto loop = std::make_shared<Loop>();
  ASSERT_TRUE(loop->init());

  auto server = Tcp::createUnique(loop, Tcp::Domain::INET);
  auto client = Tcp::createUnique(loop);
  ASSERT_TRUE(!!server);
  ASSERT_TRUE(!!client);

  client->on<EvError>([&](const auto &e, auto &client){
    LOG_E("error: %s", e.message.c_str());
    client.close();
    server->close();
  });

  client->once<EvConnect>([](const auto &e, auto &client) {
    LOG_D("client connected");
    auto buf = std::make_unique<nul::Buffer>(1);
    buf->assign("1", 1);
    client.writeAsync(std::move(buf));
    client.close();
    LOG_D("wrote 1 byte");
  });
  client->on<EvBufferRecycled>([&](const auto &e, auto &client) {
    LOG_D("buffer recycled");
    ASSERT_EQ(1, e.buffer->getCapacity());
  });

  std::shared_ptr<Tcp> acceptedClient;
  server->on<EvAccept<Tcp>>([&](const auto &e, auto &s) {
    e.client->template on<EvRead>([&](const auto &e, auto &c){
      ASSERT_EQ(e.nread, 1);
      LOG_D("server receive 1 byte");
      c.close();
      s.close();
    });
    e.client->readStart();
    acceptedClient = std::move(const_cast<EvAccept<Tcp> &>(e).client);
  });

  int on = 1;
  server->setSockOption(
    SO_REUSEADDR, reinterpret_cast<void *>(&on), sizeof(on));
  server->setSockOption(
    SO_REUSEPORT, reinterpret_cast<void *>(&on), sizeof(on));

  ASSERT_TRUE(server->bind("127.0.0.1", 22334));
  ASSERT_TRUE(server->listen(50));
  ASSERT_TRUE(client->connect(server->getIP(), 22334));

  loop->run();
}
