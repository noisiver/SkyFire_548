/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Threading/BoostAsioTaskRunner.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>

namespace
{
    bool Expect(bool condition, char const* message)
    {
        if (condition)
            return true;

        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    bool passed = true;
    Skyfire::Asio::IoContextTaskRunner runner;

    passed &= Expect(!runner.IsRunning(), "New task runner reported running");
    passed &= Expect(runner.Start(Skyfire::Asio::IoContextTaskRunner::Task()) == -1, "Task runner accepted an empty task");

    std::mutex lock;
    std::condition_variable changed;
    bool entered = false;
    bool finish = false;
    int completions = 0;

    passed &= Expect(runner.Start([&lock, &changed, &entered, &finish, &completions]
    {
        {
            std::lock_guard<std::mutex> guard(lock);
            entered = true;
        }

        changed.notify_all();

        std::unique_lock<std::mutex> waitLock(lock);
        changed.wait(waitLock, [&finish]
        {
            return finish;
        });

        ++completions;
    }) == 0, "Task runner rejected the first task");

    passed &= Expect(runner.IsRunning(), "Task runner did not report running after start");

    {
        std::unique_lock<std::mutex> waitLock(lock);
        passed &= Expect(changed.wait_for(waitLock, std::chrono::seconds(2), [&entered]
        {
            return entered;
        }), "Task runner did not execute the first task");
    }

    passed &= Expect(runner.Start([] { }) == -1, "Task runner accepted a second task while running");

    {
        std::lock_guard<std::mutex> guard(lock);
        finish = true;
    }

    changed.notify_all();
    runner.Join();

    passed &= Expect(!runner.IsRunning(), "Task runner still reported running after join");
    passed &= Expect(completions == 1, "Task runner did not finish the first task exactly once");

    bool secondRan = false;
    passed &= Expect(runner.Start([&secondRan]
    {
        secondRan = true;
    }) == 0, "Task runner rejected a second task after join");

    runner.Join();

    passed &= Expect(secondRan, "Task runner did not execute the second task");

    runner.Join();

    return passed ? 0 : 1;
}
