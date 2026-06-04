/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "DelayExecutor.h"
#include "Platform/Singleton.h"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <mutex>
#include <thread>
#include <vector>

struct DelayExecutor::Impl
{
    typedef boost::asio::executor_work_guard<boost::asio::io_context::executor_type> WorkGuard;

    Impl()
        : activated(false)
    {
    }

    boost::asio::io_context ioContext;
    std::unique_ptr<WorkGuard> workGuard;
    std::unique_ptr<DelayTask> preSvcHook;
    std::unique_ptr<DelayTask> postSvcHook;
    std::vector<std::thread> threads;
    std::mutex stateLock;
    bool activated;
};

DelayExecutor* DelayExecutor::instance()
{
    return Skyfire::Singleton<DelayExecutor, Skyfire::Mutex>::instance();
}

DelayExecutor::DelayExecutor()
    : impl_(new Impl) { }

DelayExecutor::~DelayExecutor()
{
    deactivate();
}

int DelayExecutor::deactivate()
{
    {
        std::lock_guard<std::mutex> guard(impl_->stateLock);

        if (!impl_->activated)
            return -1;

        impl_->activated = false;
        impl_->workGuard.reset();
    }

    for (std::thread& thread : impl_->threads)
        if (thread.joinable())
            thread.join();

    impl_->threads.clear();
    impl_->preSvcHook.reset();
    impl_->postSvcHook.reset();

    return 0;
}

int DelayExecutor::svc()
{
    if (impl_->preSvcHook)
        impl_->preSvcHook->call();

    impl_->ioContext.run();

    if (impl_->postSvcHook)
        impl_->postSvcHook->call();

    return 0;
}

int DelayExecutor::start(int num_threads, std::unique_ptr<DelayTask> pre_svc_hook, std::unique_ptr<DelayTask> post_svc_hook)
{
    if (activated())
        return -1;

    if (num_threads < 1)
        return -1;

    impl_->preSvcHook = std::move(pre_svc_hook);
    impl_->postSvcHook = std::move(post_svc_hook);
    impl_->ioContext.restart();
    impl_->workGuard.reset(new Impl::WorkGuard(boost::asio::make_work_guard(impl_->ioContext)));

    activated(true);

    try
    {
        for (int i = 0; i < num_threads; ++i)
            impl_->threads.push_back(std::thread(&DelayExecutor::svc, this));
    }
    catch (...)
    {
        deactivate();
        return -1;
    }

    return 0;
}

int DelayExecutor::execute(std::unique_ptr<DelayTask> new_req)
{
    if (!new_req)
        return -1;

    {
        std::lock_guard<std::mutex> guard(impl_->stateLock);

        if (!impl_->activated)
            return -1;
    }

    boost::asio::post(impl_->ioContext,
        [task = std::move(new_req)]() mutable
        {
            task->call();
        });

    return 0;
}

bool DelayExecutor::activated()
{
    std::lock_guard<std::mutex> guard(impl_->stateLock);
    return impl_->activated;
}

void DelayExecutor::activated(bool s)
{
    std::lock_guard<std::mutex> guard(impl_->stateLock);
    impl_->activated = s;
}
