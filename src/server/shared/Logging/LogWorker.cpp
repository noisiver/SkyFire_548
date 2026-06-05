/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "LogWorker.h"
#include "Threading/BoostAsioExecutor.h"

#include <mutex>
#include <thread>

struct LogWorker::Impl
{
    Impl()
        : executor(), active(true)
    {
        executor.KeepAlive();
    }

    Skyfire::Asio::IoContextExecutor executor;
    std::mutex queueLock;
    std::thread thread;
    bool active;
};

LogWorker::LogWorker()
    : m_impl(new Impl)
{
    m_impl->thread = std::thread(&LogWorker::svc, this);
}

LogWorker::~LogWorker()
{
    {
        std::lock_guard<std::mutex> guard(m_impl->queueLock);
        m_impl->active = false;
        m_impl->executor.ResetWork();
    }

    if (m_impl->thread.joinable())
        m_impl->thread.join();
}

int LogWorker::enqueue(LogOperation* op)
{
    if (!op)
        return -1;

    {
        std::lock_guard<std::mutex> guard(m_impl->queueLock);

        if (!m_impl->active)
            return -1;
    }

    m_impl->executor.Post(
        [op]
        {
            op->call();
            delete op;
        });

    return 0;
}

int LogWorker::svc()
{
    m_impl->executor.Run();
    return 0;
}
