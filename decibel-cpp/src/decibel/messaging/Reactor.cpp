// Copyright 2017, 2020. Starry, Inc. All Rights Reserved.
#include "decibel/messaging/Reactor.h"

#include "decibel/messaging/Exchanges.h"
#include "decibel/messaging/IProtocolFactory.h"
#include "decibel/messaging/Protocol.h"
#include "decibel/messaging/TcpTransport.h"
#include "decibel/niceuv/EventLoop.h"
#include "decibel/niceuv/ITimer.h"
#include "decibel/niceuv/TcpConn.h"
#include "decibel/stdlib.h"

#include <folly/Format.h>
#include "folly/IPAddress.h"
#include <folly/MoveWrapper.h>
#include <folly/futures/Promise.h>
#include <log4cxx/logger.h>

using namespace decibel::niceuv;
using namespace std::placeholders;

namespace
{
log4cxx::LoggerPtr spLogger(log4cxx::Logger::getLogger("Reactor"));
}

namespace decibel
{
namespace messaging
{
Reactor::Reactor()
    : folly::Timekeeper()
    , mEventLoop()
    , mpTimer(
          std::make_unique<EventLoopTimer>(niceuv::EventLoopTimer(&mEventLoop)))
    , mDelayedCalls()
    , mServers()
    , mClients()
{
}

Reactor::~Reactor()
{
    Shutdown();
    for (auto timer : mDelayedCalls)
    {
        timer->Stop();
    }
    mDelayedCalls.clear();
}

void Reactor::SetDebugTimer(std::unique_ptr<niceuv::DebugTimer> timer)
{
    mpTimer = std::move(timer);
}

folly::Future<folly::Unit> Reactor::ConnectTcp(const std::string& host,
                                               uint16_t port,
                                               ProtocolPtr pProtocol)
{
    auto pResolved = std::make_shared<folly::Promise<std::string>>();
    if (!folly::IPAddress::validate(host))
    {
        mEventLoop.ResolveHostname(host).thenValue(
            [pResolved](const std::vector<std::string>& addrs)
            {
                if (addrs.size() > 0)
                {
                    pResolved->setValue(addrs[0]);
                }
                else
                {
                    pResolved->setException(std::runtime_error("Couldn't resolve hostname"));
                }
            }
        ).thenError(
            folly::tag_t<DnsResolutionError>{},
            [pResolved](const auto& err)
            {
                pResolved->setException(err);
            }
        );
    }
    else
    {
        pResolved->setValue(host);
    }

    TcpConnPtr pTcpConn(new TcpConn(&mEventLoop));
    auto rawTcpConn = pTcpConn.get();
    auto rawpProtocol = pProtocol.get();
    pTcpConn->RegisterReadCallback(
        std::bind(&Protocol::OnDataReceived, rawpProtocol, _1));
    auto pConnectPromise = std::make_shared<folly::Promise<folly::Unit>>();
    pTcpConn->RegisterConnectErrorCallback(
        [host, port, pConnectPromise](int code) {
            pConnectPromise->setException(std::runtime_error(
                folly::sformat("Failed to connect to {}:{} : {}",
                               host,
                               port,
                               niceuv::strerror(code))));
        });
    auto rawpTcpConn = pTcpConn.get();
    rawpTcpConn->RegisterConnectCallback(
        [pConnectPromise](int) {
            pConnectPromise->setValue();
        });
    mClients.insert(std::move(pProtocol));

    folly::MoveWrapper<TcpConnPtr> tcpConnWrapped(std::move(pTcpConn));

    return pResolved->getFuture().thenValue(
        [rawpProtocol, port, pConnectPromise, rawTcpConn, tcpConnWrapped]
        (const std::string& ipaddr) mutable
        {
            rawTcpConn->Connect(ipaddr, port);
            rawTcpConn->Start();
            return pConnectPromise->getFuture()
                .thenError(
                folly::tag_t<std::runtime_error>{},
                [rawTcpConn](const auto& err)
                {
                    rawTcpConn->ClearCallbacks();
                    throw err;
                })
                .thenValue([rawpProtocol, tcpConnWrapped, ipaddr, port](auto /*unused*/) mutable
                {
                    std::unique_ptr<TcpTransport> pTransport(
                        new TcpTransport(tcpConnWrapped.move(), ipaddr, port));
                    rawpProtocol->MakeConnection(std::move(pTransport));
                });
        }
    );
}

void Reactor::ServeTcp(const std::string& host,
                       uint16_t port,
                       IProtocolFactory* factory)
{
    mServers.emplace(new TcpServer(&mEventLoop, host, port, factory));
}

void Reactor::Start()
{
    mEventLoop.RunForever();
}

void Reactor::Stop()
{
    mEventLoop.Stop();
}

void Reactor::CancelCall(std::shared_ptr<OneShotTimerEvent> pTimer)
{
    mDelayedCalls.erase(pTimer);
    pTimer->Stop();
}

// see include/decibel/messaging/Reactor.h for the reason behind this being
// commented out.
// void Reactor::add(folly::Func fn)
// {
//     CallSoon(fn);
// }

folly::SemiFuture<folly::Unit> Reactor::after(folly::HighResDuration duration)
{
    auto pPromise = std::make_shared<folly::Promise<folly::Unit>>();
    CallLater(duration.count(), [pPromise] { pPromise->setValue(); });
    return pPromise->getFuture().semi();
}

void Reactor::Shutdown()
{
    std::vector<folly::Future<folly::Unit>> futures;
    for (auto& client : mClients)
    {
        futures.push_back(client->Shutdown());
    }
    folly::collectAll(futures)
        .toUnsafeFuture()
        .thenValue([this](auto /*unused*/) { this->Stop(); });
    Start();
}

niceuv::EventLoop* Reactor::GetEventLoop()
{
    return &mEventLoop;
}

niceuv::ITimer* Reactor::GetTimer()
{
    return mpTimer.get();
}

} // messaging
} // decibel
