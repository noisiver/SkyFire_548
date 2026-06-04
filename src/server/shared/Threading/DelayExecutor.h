/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef _M_DELAY_EXECUTOR_H
#define _M_DELAY_EXECUTOR_H

#include <memory>

class DelayTask
{
public:
    virtual ~DelayTask() { }
    virtual int call() = 0;
};

class DelayExecutor
{
public:
    DelayExecutor();
    virtual ~DelayExecutor();
    static DelayExecutor* instance();
    int execute(std::unique_ptr<DelayTask> new_req);
    int start(int num_threads = 1, std::unique_ptr<DelayTask> pre_svc_hook = std::unique_ptr<DelayTask>(), std::unique_ptr<DelayTask> post_svc_hook = std::unique_ptr<DelayTask>());
    int deactivate();
    bool activated();
    int svc();

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;

    void activated(bool s);
};

#endif // _M_DELAY_EXECUTOR_H
