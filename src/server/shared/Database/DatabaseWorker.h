/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef _WORKERTHREAD_H
#define _WORKERTHREAD_H

#include "DatabaseQueue.h"
#include "Define.h"
#include <memory>

class MySQLConnection;

namespace Skyfire
{
namespace Asio
{
    class IoContextThread;
}
}

class DatabaseWorker
{
public:
    DatabaseWorker(Skyfire::DatabaseQueue* new_queue, MySQLConnection* con);
    ~DatabaseWorker();

    int svc();
    int wait();

private:
    Skyfire::DatabaseQueue* m_queue;
    MySQLConnection* m_conn;
    std::unique_ptr<Skyfire::Asio::IoContextThread> m_thread;
    DatabaseWorker(DatabaseWorker const& right) = delete;
    DatabaseWorker& operator=(DatabaseWorker const& right) = delete;
};

#endif
