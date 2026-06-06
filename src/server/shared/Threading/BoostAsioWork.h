/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_BOOST_ASIO_WORK_H
#define SF_BOOST_ASIO_WORK_H

#include <boost/asio/io_context.hpp>

#include <memory>

namespace Skyfire
{
namespace Asio
{
    class IoContextWorkGuard
    {
    public:
        explicit IoContextWorkGuard(boost::asio::io_context& ioContext)
            : _executor(ioContext.get_executor()), _ownsWork(true)
        {
            _executor.on_work_started();
        }

        ~IoContextWorkGuard()
        {
            Reset();
        }

        void Reset()
        {
            if (!_ownsWork)
                return;

            _ownsWork = false;
            _executor.on_work_finished();
        }

    private:
        boost::asio::io_context::executor_type _executor;
        bool _ownsWork;

        IoContextWorkGuard(IoContextWorkGuard const& right) = delete;
        IoContextWorkGuard& operator=(IoContextWorkGuard const& right) = delete;
    };

    inline std::unique_ptr<IoContextWorkGuard> MakeIoContextWorkGuard(boost::asio::io_context& ioContext)
    {
        return std::unique_ptr<IoContextWorkGuard>(new IoContextWorkGuard(ioContext));
    }

    inline void ResetWorkGuard(std::unique_ptr<IoContextWorkGuard>& workGuard)
    {
        if (workGuard)
            workGuard->Reset();

        workGuard.reset();
    }
}
}

#endif
