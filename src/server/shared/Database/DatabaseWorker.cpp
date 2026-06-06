/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "DatabaseEnv.h"
#include "DatabaseWorker.h"
#include "MySQLConnection.h"
#include "MySQLThreading.h"
#include "SQLOperation.h"
#include "Threading/BoostAsioThread.h"

#include <stdexcept>

DatabaseWorker::DatabaseWorker(Skyfire::DatabaseQueue* new_queue, MySQLConnection* con) :
    m_queue(new_queue),
    m_conn(con)
{
    if (!m_queue || !m_conn)
        return;

    m_thread.reset(new Skyfire::Asio::IoContextThread);

    if (m_thread->Start(m_queue->GetExecutor(),
        [this]
        {
            m_queue->BindConnection(m_conn);
        },
        [this]
        {
            m_queue->ClearConnection();
        }) == -1)
        throw std::runtime_error("Failed to start database worker thread");
}

DatabaseWorker::~DatabaseWorker()
{
    wait();
}

int DatabaseWorker::svc()
{
    if (!m_queue)
        return -1;

    return m_queue->run(m_conn);
}

int DatabaseWorker::wait()
{
    if (!m_thread)
        return 0;

    return m_thread->Join();
}
