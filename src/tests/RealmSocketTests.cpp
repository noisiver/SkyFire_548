/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "RealmSocket.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace
{
    using boost::asio::ip::tcp;

#ifdef _WIN32
    class WinsockScope
    {
    public:
        WinsockScope()
        {
            WSAStartup(MAKEWORD(2, 2), &_data);
        }

        ~WinsockScope()
        {
            WSACleanup();
        }

    private:
        WSADATA _data;
    };
#endif

    std::shared_ptr<RealmSocket> CreateConnectedRealmSocket(boost::asio::io_context& ioContext, tcp::socket& peer)
    {
        tcp::acceptor acceptor(ioContext, tcp::endpoint(tcp::v4(), 0));

        peer = tcp::socket(ioContext);
        peer.connect(tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()));
        peer.non_blocking(true);

        std::unique_ptr<RealmSocketHandle> serverSocket(new RealmSocketHandle(ioContext));
        acceptor.accept(*serverSocket);

        return std::make_shared<RealmSocket>(std::move(serverSocket), "127.0.0.1", peer.local_endpoint().port());
    }

    bool IsWouldBlock(boost::system::error_code const& error)
    {
        return error == boost::asio::error::would_block || error == boost::asio::error::try_again;
    }

    bool TryRead(tcp::socket& peer, std::string& received, std::string& errorText)
    {
        std::array<char, 64> buffer = {};
        boost::system::error_code error;
        size_t bytes = peer.read_some(boost::asio::buffer(buffer), error);

        if (!error)
        {
            received.append(buffer.data(), bytes);
            return true;
        }

        if (IsWouldBlock(error))
            return false;

        errorText = error.message();
        return false;
    }

    class CountingSession : public RealmSocket::Session
    {
    public:
        explicit CountingSession(uint32& closeCount) : _closeCount(closeCount)
        {
        }

        void OnRead() override
        {
        }

        void OnAccept() override
        {
        }

        void OnClose() override
        {
            ++_closeCount;
        }

    private:
        uint32& _closeCount;
    };
}

int main()
{
#ifdef _WIN32
    WinsockScope winsock;
#endif

    boost::asio::io_context ioContext;
    tcp::socket peer(ioContext);
    std::shared_ptr<RealmSocket> socket = CreateConnectedRealmSocket(ioContext, peer);

    char const payload[] = "queued-auth-write";
    if (!socket->send(payload, std::strlen(payload)))
    {
        std::cerr << "RealmSocket::send rejected an open socket\n";
        return 1;
    }

    std::string received;
    std::string errorText;
    if (TryRead(peer, received, errorText))
    {
        std::cerr << "RealmSocket::send wrote before the io_context was polled\n";
        return 1;
    }

    if (!errorText.empty())
    {
        std::cerr << "Unexpected peer read error before io_context poll: " << errorText << '\n';
        return 1;
    }

    for (uint32 i = 0; i < 200 && received.size() < std::strlen(payload); ++i)
    {
        ioContext.restart();
        ioContext.poll();

        errorText.clear();
        TryRead(peer, received, errorText);
        if (!errorText.empty())
        {
            std::cerr << "Unexpected peer read error after io_context poll: " << errorText << '\n';
            return 1;
        }

        if (received.size() < std::strlen(payload))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (received != payload)
    {
        std::cerr << "Queued write payload mismatch. Expected '" << payload << "', got '" << received << "'\n";
        return 1;
    }

    socket->shutdown();

    tcp::socket closePeer(ioContext);
    std::shared_ptr<RealmSocket> closeSocket = CreateConnectedRealmSocket(ioContext, closePeer);
    uint32 closeCount = 0;
    closeSocket->set_session(std::unique_ptr<RealmSocket::Session>(new CountingSession(closeCount)));

    closeSocket->shutdown();
    closeSocket->shutdown();

    if (closeCount != 1)
    {
        std::cerr << "RealmSocket notified close " << closeCount << " times\n";
        return 1;
    }

    return 0;
}
