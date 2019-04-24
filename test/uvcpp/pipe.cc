#include <gtest/gtest.h>
#include "uvcpp.h"
#include "uv.h"
#include <thread>
#include <chrono>

using namespace uvcpp;
using namespace std::chrono;

void testSharedRef(const std::shared_ptr<Loop> &loop) {
  auto pipe = Pipe::createShared(loop);
  pipe->sharedRefUntil<EvClose>();
  pipe->close();
}

TEST(Pipe, Connection) {
  const auto EXPECTED_DESTROY_COUNT = 3;
  auto destroyCount = 0;
  const auto EXPECTED_WRITE_COUNT = 1;
  auto writeCount = 0;

  {
    auto loop = std::make_shared<Loop>();
    ASSERT_TRUE(loop->init());

    testSharedRef(loop);

    auto server = Pipe::createUnique(loop);
    auto client = Pipe::createUnique(loop);
    ASSERT_TRUE(!!server);
    ASSERT_TRUE(!!client);
    server->once<EvDestroy>([&destroyCount](const auto &e, auto &pipe) {
      ++destroyCount;
    });
    client->once<EvDestroy>([&destroyCount](const auto &e, auto &pipe) {
      ++destroyCount;
    });

    server->on<EvError>([](const auto &e, auto &pipe) {
      FAIL() << "server failed with status: " << e.status;
    });
    client->on<EvError>([](const auto &e, auto &pipe) {
      FAIL() << "client failed with status: " << e.status;
    });
    client->on<EvClose>([](const auto &e, auto &handle) {
      LOG_D("client closed: isValid=%d", handle.isValid());
    });

    auto serverMsg = std::string{"greet from server!"};
    auto clientMsg = std::string{"greet from client!"};

    std::shared_ptr<Pipe> acceptedClient;

    server->on<EvAccept<Pipe>>([&](const auto &e, auto &server) {
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
      acceptedClient = std::move(const_cast<EvAccept<Pipe> &>(e).client);
      acceptedClient->once<EvDestroy>([&destroyCount](const auto &e, auto &pipe) {
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

    ASSERT_TRUE(server->bind("uvcpp_TestPipe"));
    ASSERT_TRUE(server->listen(50));
    client->connect("uvcpp_TestPipe");

    loop->run();
  }

  ASSERT_EQ(writeCount, EXPECTED_WRITE_COUNT);
  ASSERT_EQ(destroyCount, EXPECTED_DESTROY_COUNT);
}

TEST(Pipe, ImmediateClose) {
  auto loop = std::make_shared<Loop>();
  ASSERT_TRUE(loop->init());

  auto server = Pipe::createUnique(loop);
  auto client = Pipe::createUnique(loop);
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

  std::shared_ptr<Pipe> acceptedClient;
  server->on<EvAccept<Pipe>>([&](const auto &e, auto &s) {
    e.client->template on<EvRead>([&](const auto &e, auto &c){
      ASSERT_EQ(e.nread, 1);
      LOG_D("server receive 1 byte");
      c.close();
      s.close();
    });
    e.client->readStart();
    acceptedClient = std::move(const_cast<EvAccept<Pipe> &>(e).client);
  });

  ASSERT_TRUE(server->bind("haha_pipe"));
  ASSERT_TRUE(server->listen(50));
  client->connect("haha_pipe");

  loop->run();
}

TEST(Pipe, TestSendListeningTcpHandle) {
  auto mainLoop = std::make_shared<Loop>();
  ASSERT_TRUE(mainLoop->init());

  auto pipeServerName = std::string{"test_pipe"};

  // start tcp server
  auto tcpServer = Tcp::createShared(mainLoop);
  ASSERT_TRUE(!!tcpServer);
  ASSERT_TRUE(tcpServer->bind("127.0.0.1", 12345));
  ASSERT_TRUE(tcpServer->listen(2100));
  tcpServer->once<EvClose>([tcpServer](const auto &e, auto &client) {
    LOG_I("main thread tcp server closed");
  });
  tcpServer->on<EvAccept<Tcp>>([](const auto &e, auto &client) {
    std::shared_ptr<Tcp> c =
      std::move(const_cast<EvAccept<Tcp> &>(e).client);
    c->sharedRefUntil<EvClose>();
    c->close();

    LOG_D("received a client connection in MAIN thread");
  });

  // start pipe server
  auto pipeServer = Pipe::createShared(mainLoop);
  ASSERT_TRUE(!!pipeServer);
  ASSERT_TRUE(pipeServer->bind(pipeServerName));
  ASSERT_TRUE(pipeServer->listen(50));
  pipeServer->sharedRefUntil<EvClose>();

  // listen on the pipe server for EvAccept event, and send the tcp server
  // handle over the pipe when pipe clients connect to the pipe server
  pipeServer->on<EvAccept<Pipe>>([&tcpServer](const auto &e, auto &server) {
    std::shared_ptr<Pipe> pipe =
      std::move(const_cast<EvAccept<Pipe> &>(e).client);
    pipe->sharedRefUntil<EvClose>();
    pipe->sendTcpHandle(*tcpServer);
    pipe->close();
    LOG_D("tcpServer handle sent");
  });

  auto bgLoop = std::make_shared<Loop>();
  ASSERT_TRUE(bgLoop->init());

  std::shared_ptr<Tcp> bgThreadTcpServer = nullptr;
  auto bgServerThread = std::thread([&bgLoop, &pipeServerName, &bgThreadTcpServer](){
    auto pipeClient = Pipe::createShared(bgLoop);
    pipeClient->sharedRefUntil<EvClose>();
    ASSERT_TRUE(!!pipeClient);

    pipeClient->once<EvConnect>([](const auto &e, auto &client) {
      LOG_D("pipe connected");
      client.readStart();
    });
    pipeClient->once<EvAccept<Tcp>>([&bgThreadTcpServer](const auto &e, auto &client) {
      client.close();

      LOG_D("receive tcp server handle from main thread");

      // received the tcp server handle from the pipe, now we can listen on it
      // in the current background thread
      bgThreadTcpServer = std::move(const_cast<EvAccept<Tcp> &>(e).client);
      bgThreadTcpServer->once<EvClose>([bgThreadTcpServer](const auto &e, auto &client) {
        LOG_I("bg thread tcp server closed");
      });
      bgThreadTcpServer->on<EvAccept<Tcp>>([](const auto &e, auto &client) {
        std::shared_ptr<Tcp> c =
          std::move(const_cast<EvAccept<Tcp> &>(e).client);
        c->sharedRefUntil<EvClose>();
        c->close();

        LOG_D("received a client connection in BACKGROUND thread");
      });

      LOG_I("will start background thread server");
      ASSERT_TRUE(bgThreadTcpServer->listen(2100));
    });

    pipeClient->connect(pipeServerName);

    bgLoop->run();
    LOG_I("background server thread quit");
  });

  // this thread will start a loop in which connect requests
  // to the TCP server will be sent
  auto clientThread = std::thread([&](){
    const auto clientCount = 200;
    auto connectCount = 0;

    auto clientLoop = std::make_shared<Loop>();
    ASSERT_TRUE(clientLoop->init());

    for (auto i = 0; i < clientCount; ++i) {
      auto tcpClient = Tcp::createShared(clientLoop);
      tcpClient->sharedRefUntil<EvClose>();
      tcpClient->once<EvConnect>([&](const auto &e, auto &client) {
        client.close();

        if (++connectCount == clientCount) {
          LOG_I("will close servers");

          // must close the server handles in loop threads where
          // they are created
          auto mainWork = Work::createShared(mainLoop);
          mainWork->once<EvAfterWork>(
            [mainWork, &tcpServer, &pipeServer](const auto &e, auto &w) {
            tcpServer->close();
            pipeServer->close();
          });
          mainWork->start();

          // close the background tcp server the its own thread
          auto bgWork = Work::createShared(bgLoop);
          bgWork->once<EvAfterWork>(
            [bgWork, &bgThreadTcpServer](const auto &e, auto &w) {
            bgThreadTcpServer->close();
          });
          bgWork->start();
          LOG_I("finished %d connects, close all servers", clientCount);
        }
      });
      ASSERT_TRUE(tcpClient->connect("127.0.0.1", 12345));
    }

    clientLoop->run();
    LOG_I("client thread quit");
  });

  mainLoop->run();

  bgServerThread.join();
  clientThread.join();

  LOG_I("main thread quit");
}




TEST(Pipe, TestSendAccpetedTcpHandle) {
  auto mainLoop = std::make_shared<Loop>();
  ASSERT_TRUE(mainLoop->init());

  auto pipeServerName = std::string{"test_pipe2"};

  // start pipe server
  auto pipeServer = Pipe::createShared(mainLoop);
  ASSERT_TRUE(!!pipeServer);
  ASSERT_TRUE(pipeServer->bind(pipeServerName));
  ASSERT_TRUE(pipeServer->listen(500));
  pipeServer->sharedRefUntil<EvClose>();
  pipeServer->once<EvClose>([pipeServer](const auto &e, auto &server) {
    LOG_I("pipe server closed");
  });

  // listen on the pipe server for EvAccept event, and send the tcp server
  // handle over the pipe when pipe clients connect to the pipe server
  pipeServer->on<EvAccept<Pipe>>([](const auto &e, auto &server) {
    std::shared_ptr<Pipe> pipe =
      std::move(const_cast<EvAccept<Pipe> &>(e).client);
    pipe->sharedRefUntil<EvClose>();
    pipe->readStart();

    // on receiving a tcp handle from the pipe, send a message through
    // the handle to the remote end
    pipe->on<EvAccept<Tcp>>([](const auto &e, auto &pipe) {
      LOG_I("received tcp handle from pipe client");
      std::shared_ptr<Tcp> tcpConn =
        std::move(const_cast<EvAccept<Tcp> &>(e).client);
      tcpConn->sharedRefUntil<EvClose>();

      auto buf = std::make_unique<nul::Buffer>(6);
      buf->assign("hello\0", 6);
      if (!tcpConn->writeAsync(std::move(buf))) {
        LOG_E("write failed");
      }
      tcpConn->close();

      pipe.close();

      LOG_I("received tcp handle through connected pipe");
    });

    LOG_D("received pipe connection");
  });


  auto bgLoop = std::make_shared<Loop>();
  ASSERT_TRUE(bgLoop->init());

  auto bgThreadTcpServer = Tcp::createShared(bgLoop);
  ASSERT_TRUE(!!bgThreadTcpServer);

  auto bgServerThread = std::thread([&bgLoop, &bgThreadTcpServer, &pipeServerName](){
    // start tcp server
    ASSERT_TRUE(bgThreadTcpServer->bind("127.0.0.1", 12345));
    ASSERT_TRUE(bgThreadTcpServer->listen(2100));
    bgThreadTcpServer->once<EvClose>([bgThreadTcpServer](const auto &e, auto &server) {
      LOG_I("background thread tcp server closed");
    });
    bgThreadTcpServer->on<EvAccept<Tcp>>([&pipeServerName](const auto &e, auto &server) {
      std::shared_ptr<Tcp> tcpConnHandle =
        std::move(const_cast<EvAccept<Tcp> &>(e).client);
      tcpConnHandle->sharedRefUntil<EvClose>();

      LOG_I("accepted new tcp connection");

      auto pipeClient = Pipe::createShared(server.getLoop());
      pipeClient->template sharedRefUntil<EvClose>();
      pipeClient->template once<EvConnect>([tcpConnHandle](const auto &e, auto &client) {
        client.sendTcpHandle(*tcpConnHandle);
        client.close();
        tcpConnHandle->close();

        LOG_D("sent the tcp handle to the pipe server");
      });
      pipeClient->connect(pipeServerName);
    });

    bgLoop->run();
    LOG_I("background server thread quit");
  });


  // this thread will start a loop in which connect requests
  // to the TCP server will be sent
  auto clientThread = std::thread([&](){
    const auto clientCount = 200;
    auto connectCount = 0;

    auto clientLoop = std::make_shared<Loop>();
    ASSERT_TRUE(clientLoop->init());

    for (auto i = 0; i < clientCount; ++i) {
      auto tcpClient = Tcp::createShared(clientLoop);
      tcpClient->sharedRefUntil<EvClose>();
      tcpClient->once<EvConnect>([](const auto &e, auto &client) {
        client.readStart();
      });
      tcpClient->once<EvClose>([&, i](const auto &e, auto &client) {
        if (++connectCount == clientCount) {
          LOG_I("will close servers");

          // must close the server handles in loop threads where
          // they are created
          auto mainWork = Work::createShared(mainLoop);
          mainWork->once<EvAfterWork>(
            [mainWork, &pipeServer](const auto &e, auto &w) {
              pipeServer->close();
            });
          mainWork->start();

          // close the background tcp server the its own thread
          auto bgWork = Work::createShared(bgLoop);
          bgWork->once<EvAfterWork>(
            [bgWork, &bgThreadTcpServer](const auto &e, auto &w) {
              bgThreadTcpServer->close();
            });
          bgWork->start();
          LOG_I("finished %d connects, close all servers", clientCount);
        }
      });
      tcpClient->once<EvRead>([&, i](const auto &e, auto &client) {
        LOG_D("received message : %s, %d", e.buf, i);
        client.close();
      });
      ASSERT_TRUE(tcpClient->connect("127.0.0.1", 12345));
    }

    clientLoop->run();
    LOG_I("client thread quit");
  });

  mainLoop->run();
  bgServerThread.join();
  clientThread.join();

  LOG_I("main thread quit");
}
