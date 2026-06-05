/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_BOOST_ASIO_EXECUTOR_H
#define SF_BOOST_ASIO_EXECUTOR_H

#include "Threading/BoostAsioWork.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <cstddef>
#include <memory>
#include <utility>

namespace Skyfire
{
namespace Asio
{
    class IoContextExecutor
    {
    public:
        boost::asio::io_context& GetIoContext() { return _ioContext; }

        void Restart()
        {
            _ioContext.restart();
        }

        void KeepAlive()
        {
            _workGuard = MakeIoContextWorkGuard(_ioContext);
        }

        void ResetWork()
        {
            ResetWorkGuard(_workGuard);
        }

        std::size_t Run()
        {
            return _ioContext.run();
        }

        template<class Handler>
        void Post(Handler&& handler)
        {
            boost::asio::post(_ioContext, std::forward<Handler>(handler));
        }

    private:
        boost::asio::io_context _ioContext;
        std::unique_ptr<IoContextWorkGuard> _workGuard;
    };
}
}

#endif
