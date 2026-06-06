/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Network/BoostAsioUtils.h"
#include "Threading/BoostAsioWork.h"

#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <chrono>
#include <iostream>

int main()
{
    boost::asio::io_context ioContext;
    boost::asio::ip::tcp::acceptor acceptor(ioContext);
    boost::asio::ip::tcp::socket peer(ioContext);

    boost::system::error_code error;
    acceptor.open(boost::asio::ip::tcp::v4(), error);
    if (error)
    {
        std::cerr << "Failed to open acceptor: " << error.message() << '\n';
        return 1;
    }

    acceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0), error);
    if (error)
    {
        std::cerr << "Failed to bind acceptor: " << error.message() << '\n';
        return 1;
    }

    acceptor.listen(boost::asio::socket_base::max_listen_connections, error);
    if (error)
    {
        std::cerr << "Failed to listen on acceptor: " << error.message() << '\n';
        return 1;
    }

    bool completed = false;
    boost::system::error_code acceptError;
    acceptor.async_accept(peer, [&completed, &acceptError](boost::system::error_code const& callbackError)
    {
        completed = true;
        acceptError = callbackError;
    });

    Skyfire::Net::CloseTcpAcceptor(acceptor);
    Skyfire::Net::CloseTcpAcceptor(acceptor);
    ioContext.run();

    if (acceptor.is_open())
    {
        std::cerr << "CloseTcpAcceptor left the acceptor open\n";
        return 1;
    }

    if (!completed)
    {
        std::cerr << "CloseTcpAcceptor did not complete the pending accept\n";
        return 1;
    }

    if (acceptError != boost::asio::error::operation_aborted)
    {
        std::cerr << "Pending accept completed with " << acceptError.message() << '\n';
        return 1;
    }

    bool postedAfterStop = false;
    boost::asio::post(ioContext, [&postedAfterStop]
    {
        postedAfterStop = true;
    });

    Skyfire::Net::RestartIoContext(ioContext);
    ioContext.run();

    if (!postedAfterStop)
    {
        std::cerr << "RestartIoContext did not allow queued work to run after stop\n";
        return 1;
    }

    boost::asio::io_context guardedContext;
    std::unique_ptr<Skyfire::Asio::IoContextWorkGuard> guard = Skyfire::Asio::MakeIoContextWorkGuard(guardedContext);

    Skyfire::Net::StopIoContext(guardedContext);

    if (guardedContext.run() != 0)
    {
        std::cerr << "StopIoContext unexpectedly allowed guarded work to run\n";
        return 1;
    }

    if (!guardedContext.stopped())
    {
        std::cerr << "StopIoContext did not leave the io_context stopped\n";
        return 1;
    }

    Skyfire::Asio::ResetWorkGuard(guard);
    Skyfire::Net::RestartIoContext(guardedContext);

    bool postedAfterExplicitStop = false;
    boost::asio::post(guardedContext, [&postedAfterExplicitStop]
    {
        postedAfterExplicitStop = true;
    });

    guardedContext.run();

    if (!postedAfterExplicitStop)
    {
        std::cerr << "RestartIoContext did not recover after StopIoContext\n";
        return 1;
    }

    boost::asio::io_context socketContext;
    boost::asio::ip::tcp::acceptor socketAcceptor(socketContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    boost::asio::ip::tcp::socket clientSocket(socketContext);
    boost::asio::ip::tcp::socket serverSocket(socketContext);

    clientSocket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), socketAcceptor.local_endpoint().port()), error);
    if (error)
    {
        std::cerr << "Failed to connect client socket: " << error.message() << '\n';
        return 1;
    }

    socketAcceptor.accept(serverSocket, error);
    if (error)
    {
        std::cerr << "Failed to accept server socket: " << error.message() << '\n';
        return 1;
    }

    std::array<char, 1> readBuffer = {};
    bool readCompleted = false;
    boost::system::error_code readError;
    serverSocket.async_read_some(boost::asio::buffer(readBuffer),
        [&readCompleted, &readError](boost::system::error_code const& callbackError, size_t)
        {
            readCompleted = true;
            readError = callbackError;
        });

    Skyfire::Net::CloseTcpSocket(serverSocket);
    Skyfire::Net::CloseTcpSocket(serverSocket);
    socketContext.run();

    if (serverSocket.is_open())
    {
        std::cerr << "CloseTcpSocket left the server socket open\n";
        return 1;
    }

    if (!readCompleted)
    {
        std::cerr << "CloseTcpSocket did not complete the pending read\n";
        return 1;
    }

    if (readError != boost::asio::error::operation_aborted)
    {
        std::cerr << "Pending read completed with " << readError.message() << '\n';
        return 1;
    }

    boost::asio::io_context timerContext;
    boost::asio::steady_timer timer(timerContext);
    timer.expires_after(std::chrono::hours(1));

    bool timerCompleted = false;
    boost::system::error_code timerError;
    timer.async_wait([&timerCompleted, &timerError](boost::system::error_code const& callbackError)
    {
        timerCompleted = true;
        timerError = callbackError;
    });

    Skyfire::Net::CancelTimer(timer);
    Skyfire::Net::CancelTimer(timer);
    timerContext.run();

    if (!timerCompleted)
    {
        std::cerr << "CancelTimer did not complete the pending wait\n";
        return 1;
    }

    if (timerError != boost::asio::error::operation_aborted)
    {
        std::cerr << "Pending timer completed with " << timerError.message() << '\n';
        return 1;
    }

    return 0;
}
