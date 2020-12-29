// Copyright 2017, 2020. Starry, Inc. All Rights Reserved.
#pragma once

#include <unordered_set>

#include "decibel/Traits.h"
#include "decibel/messaging/TcpServer.h"
#include "decibel/niceuv/EventLoop.h"
#include "decibel/niceuv/OneShotTimerEvent.h"

#include <folly/Executor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>
#include <folly/MoveWrapper.h>

#include <unordered_set>

namespace decibel
{
namespace messaging
{
class Protocol;
class IProtocolFactory;

class Reactor : public folly::Timekeeper
{
public:
    Reactor();

    virtual ~Reactor();

    void SetDebugTimer(std::unique_ptr<niceuv::DebugTimer> timer);

    folly::Future<folly::Unit>
    ConnectTcp(const std::string& host, uint16_t port, ProtocolPtr protocol);

    void
    ServeTcp(const std::string& host, uint16_t port, IProtocolFactory* factory);

    virtual void Start();

    virtual void Stop();

    template <typename F>
    auto CallSoon(F&& fn) -> decltype(folly::makeFutureWith(fn))
    {
        return CallLater(0, std::move(fn));
    }

    niceuv::EventLoop* GetEventLoop();

    template <typename F,
              typename Result = typename folly::lift_unit_t<ResultOf<F>>>
    folly::Future<Result> CallLater(ssize_t timeout, F&& fn)
    {
        auto pPromise = std::make_shared<folly::Promise<Result>>();
        auto pTimer =
            std::make_shared<niceuv::OneShotTimerEvent>(mpTimer.get(), timeout);
        std::weak_ptr<niceuv::OneShotTimerEvent> weakTimer(pTimer);
        auto wrappedFn = folly::makeMoveWrapper(std::move(fn));
        pTimer->SetCallback([this, pPromise, weakTimer, wrappedFn]() {
            pPromise->setWith(*wrappedFn);
            if (!weakTimer.expired())
            {
                this->CancelCall(weakTimer.lock());
            }
        });
        mDelayedCalls.insert(pTimer);
        pTimer->Start();
        return pPromise->getFuture();
    }

    // This doesn't compile on OS X, but, leave it here for posterity's sake.
    // You can uncomment this to turn this class into a folly::Executor.
    // virtual void add(folly::Func fn);

    // folly::TimeKeeper
    virtual folly::SemiFuture<folly::Unit>
    after(folly::HighResDuration duration);

    niceuv::ITimer* GetTimer();

private:
    void CancelCall(std::shared_ptr<niceuv::OneShotTimerEvent> pTimer);

    void Shutdown();

    niceuv::EventLoop mEventLoop;
    std::unique_ptr<niceuv::ITimer> mpTimer;
    std::unordered_set<std::shared_ptr<niceuv::OneShotTimerEvent>>
        mDelayedCalls;
    std::unordered_set<std::unique_ptr<TcpServer>> mServers;
    std::unordered_set<std::unique_ptr<Protocol>> mClients;
};

} // messaging
} // decibel
