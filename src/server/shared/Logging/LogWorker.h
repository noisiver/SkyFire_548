/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef LOGWORKER_H
#define LOGWORKER_H

#include "LogOperation.h"

#include <memory>

class LogWorker
{
public:
    LogWorker();
    ~LogWorker();

    enum
    {
        HIGH_WATERMARK = 8 * 1024 * 1024,
        LOW_WATERMARK = 8 * 1024 * 1024
    };

    int enqueue(LogOperation* op);

private:
    struct Impl;

    int svc();

    std::unique_ptr<Impl> m_impl;
};

#endif
