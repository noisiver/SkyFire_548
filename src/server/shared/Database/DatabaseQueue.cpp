/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "DatabaseQueue.h"
#include "SQLOperation.h"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <mutex>

namespace Skyfire
{
    namespace
    {
        thread_local MySQLConnection* CurrentDatabaseConnection = nullptr;
    }

    struct DatabaseQueue::Impl
    {
        typedef boost::asio::executor_work_guard<boost::asio::io_context::executor_type> WorkGuard;

        Impl()
            : ioContext(), workGuard(new WorkGuard(boost::asio::make_work_guard(ioContext))), closed(false)
        {
        }

        boost::asio::io_context ioContext;
        std::unique_ptr<WorkGuard> workGuard;
        std::mutex stateLock;
        bool closed;
    };

    DatabaseQueue::DatabaseQueue()
        : _impl(new Impl)
    {
    }

    DatabaseQueue::~DatabaseQueue()
    {
        close();
    }

    void DatabaseQueue::enqueue(SQLOperation* operation)
    {
        if (!operation)
            return;

        std::lock_guard<std::mutex> guard(_impl->stateLock);
        if (_impl->closed)
            return;

        boost::asio::post(_impl->ioContext,
            [operation]
            {
                operation->SetConnection(CurrentDatabaseConnection);
                operation->call();
                delete operation;
            });
    }

    int DatabaseQueue::run(MySQLConnection* connection)
    {
        if (!connection)
            return -1;

        CurrentDatabaseConnection = connection;
        _impl->ioContext.run();
        CurrentDatabaseConnection = nullptr;
        return 0;
    }

    void DatabaseQueue::close()
    {
        std::lock_guard<std::mutex> guard(_impl->stateLock);
        if (_impl->closed)
            return;

        _impl->closed = true;
        _impl->workGuard.reset();
    }
}
