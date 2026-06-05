/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

/// \addtogroup Skyfired
/// @{
/// \file

#ifndef _RASOCKET_H
#define _RASOCKET_H

#include "Common.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

typedef boost::asio::ip::tcp::socket RASocketHandle;

/// Remote Administration socket
class RASocket : public std::enable_shared_from_this<RASocket>
{
public:
    RASocket(std::shared_ptr<boost::asio::io_context> ioContext, std::unique_ptr<RASocketHandle> socket, std::string const& remoteAddress);
    virtual ~RASocket();

    void start();

private:
    typedef std::function<void(bool)> WriteCallback;
    typedef std::function<void(bool, std::string)> ReadLineCallback;

    struct PendingWrite
    {
        std::string Data;
        WriteCallback Callback;
    };

    void close();
    bool is_open() const;
    void start_subnegotiation();     ///< Used by telnet protocol RFC 854 / 855
    void handle_subnegotiation_read(boost::system::error_code const& error, size_t transferredBytes);
    void finish_subnegotiation();
    void send_authentication_required();
    void prompt_username();
    void prompt_password();
    void handle_credentials();
    void prompt_command();
    void handle_command(std::string const& command);
    void start_command(std::string const& command);
    void queue_write(std::string data, WriteCallback callback = WriteCallback());
    void start_async_write();
    void handle_async_write(boost::system::error_code const& error);
    void async_read_line(ReadLineCallback callback);
    void handle_read_line(boost::system::error_code const& error, size_t transferredBytes, ReadLineCallback callback);
    void drain_command_output();
    int check_access_level(const std::string& user);
    int check_password(const std::string& user, const std::string& pass);

    static void zprint(void* callbackArg, const char* szText);
    static void commandFinished(void* callbackArg, bool success);

private:
    std::shared_ptr<boost::asio::io_context> _ioContext;
    std::unique_ptr<RASocketHandle> _socket;
    boost::asio::steady_timer _subnegotiationTimer;
    std::string _remoteAddress;
    std::array<char, 1024> _subnegotiationBuffer;
    boost::asio::streambuf _lineBuffer;
    std::deque<PendingWrite> _writeQueue;
    bool _writeInProgress;
    std::atomic<bool> _closed;
    bool _subnegotiationDone;
    bool _subnegotiationTimedOut;
    std::string _user;
    std::string _pass;
    uint8 _minLevel; ///< Minimum security level required to connect
    std::atomic<bool> _commandExecuting;
    std::mutex _commandLock;
    std::queue<std::string> _commandOutput;
    bool _commandComplete;
    bool _commandOutputDrainInProgress;
    std::shared_ptr<RASocket> _commandSelf;
};

#endif

/// @}
