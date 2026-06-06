/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_BOOST_ASIO_TASK_RUNNER_H
#define SF_BOOST_ASIO_TASK_RUNNER_H

#include "Threading/BoostAsioExecutor.h"
#include "Threading/BoostAsioThread.h"

#include <functional>
#include <utility>

namespace Skyfire
{
namespace Asio
{
    class IoContextTaskRunner
    {
    public:
        typedef std::function<void()> Task;

        ~IoContextTaskRunner()
        {
            Join();
        }

        bool IsRunning() const { return _thread.IsRunning(); }

        int Start(Task task)
        {
            if (!task || _thread.IsRunning())
                return -1;

            _executor.Restart();
            _executor.KeepAlive();

            if (_thread.Start(_executor) == -1)
            {
                _executor.ResetWork();
                _executor.Restart();
                return -1;
            }

            try
            {
                _executor.Post([this, task = std::move(task)]() mutable
                {
                    struct WorkReset
                    {
                        explicit WorkReset(IoContextExecutor& executor) : Executor(executor) { }
                        ~WorkReset() { Executor.ResetWork(); }

                        IoContextExecutor& Executor;
                    } reset(_executor);

                    task();
                });
            }
            catch (...)
            {
                _executor.ResetWork();
                _executor.Stop();
                _thread.Join();
                _executor.Restart();
                return -1;
            }

            return 0;
        }

        int Join()
        {
            _executor.ResetWork();
            int const result = _thread.Join();
            _executor.Restart();
            return result;
        }

    private:
        IoContextExecutor _executor;
        IoContextThread _thread;
    };
}
}

#endif
