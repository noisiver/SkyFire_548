/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

/** \file
    \ingroup Skyfired
*/

#include "AccountMgr.h"
#include "Common.h"
#include "Configuration/Config.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "RASocket.h"
#include "SRP6.h"
#include "World.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstring>
#include <istream>
#include <sstream>
#include <utility>

RASocket::RASocket(std::shared_ptr<boost::asio::io_context> ioContext, std::unique_ptr<RASocketHandle> socket, std::string const& remoteAddress) :
    _ioContext(std::move(ioContext)),
    _socket(std::move(socket)),
    _subnegotiationTimer(*_ioContext),
    _remoteAddress(remoteAddress),
    _subnegotiationBuffer(),
    _lineBuffer(),
    _writeQueue(),
    _writeInProgress(false),
    _closed(false),
    _subnegotiationDone(false),
    _subnegotiationTimedOut(false),
    _user(),
    _pass(),
    _minLevel(3),
    _commandExecuting(false),
    _commandOutput(),
    _commandComplete(false),
    _commandOutputDrainInProgress(false),
    _commandSelf()
{
    _minLevel = uint8(sConfigMgr->GetIntDefault("RA.MinLevel", 3));
}

RASocket::~RASocket()
{
    close();
}

void RASocket::start()
{
    std::shared_ptr<RASocket> self = shared_from_this();
    boost::asio::post(*_ioContext, [self]
    {
        self->start_subnegotiation();
    });
}

void RASocket::close()
{
    if (_closed.exchange(true))
        return;

    SF_LOG_INFO("commands.ra", "Closing connection");

    boost::system::error_code ignored;
    _subnegotiationTimer.cancel(ignored);

    if (_socket && _socket->is_open())
    {
        _socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        _socket->close(ignored);
    }
}

bool RASocket::is_open() const
{
    return !_closed && _socket && _socket->is_open();
}

void RASocket::start_subnegotiation()
{
    if (!is_open())
    {
        close();
        return;
    }

    _subnegotiationDone = false;
    _subnegotiationTimedOut = false;

    std::shared_ptr<RASocket> self = shared_from_this();
    _subnegotiationTimer.expires_after(std::chrono::milliseconds(1000));
    _subnegotiationTimer.async_wait([self](boost::system::error_code const& error)
    {
        if (error || self->_subnegotiationDone)
            return;

        self->_subnegotiationDone = true;
        self->_subnegotiationTimedOut = true;

        boost::system::error_code ignored;
        self->_socket->cancel(ignored);
        self->finish_subnegotiation();
    });

    _socket->async_read_some(boost::asio::buffer(_subnegotiationBuffer),
        [self](boost::system::error_code const& error, size_t transferredBytes)
        {
            self->handle_subnegotiation_read(error, transferredBytes);
        });
}

void RASocket::handle_subnegotiation_read(boost::system::error_code const& error, size_t transferredBytes)
{
    if (_subnegotiationDone)
        return;

    if (error == boost::asio::error::operation_aborted && _subnegotiationTimedOut)
        return;

    _subnegotiationDone = true;

    boost::system::error_code ignored;
    _subnegotiationTimer.cancel(ignored);

    if (error)
    {
        close();
        return;
    }

    if (transferredBytes == 0)
    {
        finish_subnegotiation();
        return;
    }

    if (transferredBytes >= _subnegotiationBuffer.size())
    {
        SF_LOG_DEBUG("commands.ra", "RASocket::subnegotiate: allocated buffer 1024 bytes was too small for negotiation packet, size: %u", uint32(transferredBytes));
        close();
        return;
    }

#ifdef _DEBUG
    for (size_t i = 0; i < transferredBytes;)
    {
        uint8 iac = uint8(_subnegotiationBuffer[i]);
        if (iac == 0xFF)   // "Interpret as Command" (IAC)
        {
            if (i + 2 >= transferredBytes)
            {
                close();
                return;
            }

            uint8 command = uint8(_subnegotiationBuffer[++i]);
            std::stringstream ss;
            switch (command)
            {
                case 0xFB:        // WILL
                    ss << "WILL ";
                    break;
                case 0xFC:        // WON'T
                    ss << "WON'T ";
                    break;
                case 0xFD:        // DO
                    ss << "DO ";
                    break;
                case 0xFE:        // DON'T
                    ss << "DON'T ";
                    break;
                default:
                    close();
                    return;
            }

            uint8 param = uint8(_subnegotiationBuffer[++i]);
            ss << uint32(param);
            SF_LOG_DEBUG("commands.ra", "%s", ss.str().c_str());
        }
        ++i;
    }
#endif

    uint8 const reply[2] = { 0xFF, 0xF0 };
    std::shared_ptr<RASocket> self = shared_from_this();
    queue_write(std::string(reinterpret_cast<char const*>(reply), sizeof(reply)), [self](bool success)
    {
        if (success)
            self->finish_subnegotiation();
        else
            self->close();
    });
}

void RASocket::finish_subnegotiation()
{
    send_authentication_required();
}

void RASocket::send_authentication_required()
{
    std::shared_ptr<RASocket> self = shared_from_this();
    queue_write("Authentication required\r\n", [self](bool success)
    {
        if (success)
            self->prompt_username();
        else
            self->close();
    });
}

void RASocket::prompt_username()
{
    std::shared_ptr<RASocket> self = shared_from_this();
    queue_write("Username: ", [self](bool success)
    {
        if (!success)
        {
            self->close();
            return;
        }

        self->async_read_line([self](bool read, std::string line)
        {
            if (!read)
            {
                self->close();
                return;
            }

            self->_user = std::move(line);
            self->prompt_password();
        });
    });
}

void RASocket::prompt_password()
{
    std::shared_ptr<RASocket> self = shared_from_this();
    queue_write("Password: ", [self](bool success)
    {
        if (!success)
        {
            self->close();
            return;
        }

        self->async_read_line([self](bool read, std::string line)
        {
            if (!read)
            {
                self->close();
                return;
            }

            self->_pass = std::move(line);
            self->handle_credentials();
        });
    });
}

void RASocket::handle_credentials()
{
    SF_LOG_INFO("commands.ra", "Login attempt for user: %s", _user.c_str());

    if (check_access_level(_user) == -1 || check_password(_user, _pass) == -1)
    {
        std::shared_ptr<RASocket> self = shared_from_this();
        queue_write("Authentication failed\r\n", [self](bool)
        {
            self->close();
        });
        return;
    }

    SF_LOG_INFO("commands.ra", "User login: %s", _user.c_str());

    std::shared_ptr<RASocket> self = shared_from_this();
    queue_write(std::string(sWorld->GetMotd()) + "\r\n", [self](bool success)
    {
        if (success)
            self->prompt_command();
        else
            self->close();
    });
}

void RASocket::prompt_command()
{
    std::shared_ptr<RASocket> self = shared_from_this();
    queue_write("TC> ", [self](bool success)
    {
        if (!success)
        {
            self->close();
            return;
        }

        self->async_read_line([self](bool read, std::string line)
        {
            if (!read)
            {
                self->close();
                return;
            }

            self->handle_command(line);
        });
    });
}

void RASocket::handle_command(std::string const& command)
{
    if (command.empty())
    {
        prompt_command();
        return;
    }

    SF_LOG_INFO("commands.ra", "Received command: %s", command.c_str());

    if (command == "quit" || command == "exit" || command == "logout")
    {
        std::shared_ptr<RASocket> self = shared_from_this();
        queue_write("Bye\r\n", [self](bool)
        {
            self->close();
        });
        return;
    }

    start_command(command);
}

void RASocket::start_command(std::string const& command)
{
    _commandExecuting = true;

    {
        std::lock_guard<std::mutex> guard(_commandLock);
        _commandComplete = false;
        _commandOutputDrainInProgress = false;
        std::queue<std::string> empty;
        std::swap(_commandOutput, empty);
        _commandSelf = shared_from_this();
    }

    CliCommandHolder* cmd = new CliCommandHolder(this, command.c_str(), &RASocket::zprint, &RASocket::commandFinished);
    sWorld->QueueCliCommand(cmd);
}

void RASocket::queue_write(std::string data, WriteCallback callback)
{
    if (_closed)
    {
        if (callback)
            callback(false);
        return;
    }

    bool startWrite = !_writeInProgress && _writeQueue.empty();
    _writeQueue.push_back(PendingWrite{ std::move(data), std::move(callback) });

    if (startWrite)
        start_async_write();
}

void RASocket::start_async_write()
{
    if (_closed || _writeInProgress || _writeQueue.empty())
        return;

    _writeInProgress = true;

    std::shared_ptr<RASocket> self = shared_from_this();
    boost::asio::async_write(*_socket, boost::asio::buffer(_writeQueue.front().Data),
        [self](boost::system::error_code const& error, size_t)
        {
            self->handle_async_write(error);
        });
}

void RASocket::handle_async_write(boost::system::error_code const& error)
{
    WriteCallback callback;

    if (!_writeQueue.empty())
    {
        callback = std::move(_writeQueue.front().Callback);
        _writeQueue.pop_front();
    }

    _writeInProgress = false;

    if (error)
    {
        if (callback)
            callback(false);

        close();
        return;
    }

    if (callback)
        callback(true);

    if (!_writeQueue.empty())
        start_async_write();
}

void RASocket::async_read_line(ReadLineCallback callback)
{
    if (_closed)
    {
        callback(false, std::string());
        return;
    }

    std::shared_ptr<RASocket> self = shared_from_this();
    boost::asio::async_read_until(*_socket, _lineBuffer, '\n',
        [self, callback = std::move(callback)](boost::system::error_code const& error, size_t transferredBytes) mutable
        {
            self->handle_read_line(error, transferredBytes, std::move(callback));
        });
}

void RASocket::handle_read_line(boost::system::error_code const& error, size_t /*transferredBytes*/, ReadLineCallback callback)
{
    if (error)
    {
        callback(false, std::string());
        close();
        return;
    }

    std::istream input(&_lineBuffer);
    std::string line;
    std::getline(input, line);

    if (!line.empty() && line.back() == '\r')
        line.pop_back();

    callback(true, std::move(line));
}

void RASocket::drain_command_output()
{
    if (_closed)
    {
        std::lock_guard<std::mutex> guard(_commandLock);
        if (_commandComplete)
            _commandSelf.reset();
        return;
    }

    std::string output;
    bool hasOutput = false;
    bool commandComplete = false;

    {
        std::lock_guard<std::mutex> guard(_commandLock);
        if (_commandOutputDrainInProgress)
            return;

        if (!_commandOutput.empty())
        {
            output = _commandOutput.front();
            _commandOutput.pop();
            _commandOutputDrainInProgress = true;
            hasOutput = true;
        }
        else if (_commandComplete)
        {
            commandComplete = true;
            _commandSelf.reset();
        }
    }

    if (hasOutput)
    {
        std::shared_ptr<RASocket> self = shared_from_this();
        queue_write(std::move(output), [self](bool success)
        {
            {
                std::lock_guard<std::mutex> guard(self->_commandLock);
                self->_commandOutputDrainInProgress = false;
            }

            if (success)
                self->drain_command_output();
            else
                self->close();
        });
        return;
    }

    if (commandComplete)
        prompt_command();
}

int RASocket::check_access_level(const std::string& user)
{
    std::string safeUser = user;

    AccountMgr::normalizeString(safeUser);

    PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_ACCESS);
    stmt->setString(0, safeUser);
    PreparedQueryResult result = LoginDatabase.Query(stmt);

    if (!result)
    {
        SF_LOG_INFO("commands.ra", "User %s does not exist in database", user.c_str());
        return -1;
    }

    Field* fields = result->Fetch();

    if (fields[1].GetUInt8() < _minLevel)
    {
        SF_LOG_INFO("commands.ra", "User %s has no privilege to login", user.c_str());
        return -1;
    }
    else if (fields[2].GetInt32() != -1)
    {
        SF_LOG_INFO("commands.ra", "User %s has to be assigned on all realms (with RealmID = '-1')", user.c_str());
        return -1;
    }

    return 0;
}

int RASocket::check_password(const std::string& user, const std::string& pass)
{
    std::string safe_user = user;
    AccountMgr::normalizeString(safe_user);

    std::string safe_pass = pass;
    AccountMgr::normalizeString(safe_pass);

    PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_CHECK_PASSWORD_BY_NAME);

    stmt->setString(0, safe_user);
    if (PreparedQueryResult result = LoginDatabase.Query(stmt))
    {
        SkyFire::Crypto::SRP6::Salt salt = (*result)[0].GetBinary<SkyFire::Crypto::SRP6::SALT_LENGTH>();
        SkyFire::Crypto::SRP6::Verifier verifier = (*result)[1].GetBinary<SkyFire::Crypto::SRP6::VERIFIER_LENGTH>();

        if (SkyFire::Crypto::SRP6::CheckLogin(safe_user, safe_pass, salt, verifier))
            return 0;
    }

    SF_LOG_INFO("commands.ra", "Wrong password for user: %s", user.c_str());
    return -1;
}

void RASocket::zprint(void* callbackArg, const char* szText)
{
    if (!szText || !callbackArg)
        return;

    RASocket* socket = static_cast<RASocket*>(callbackArg);
    if (socket->_closed)
        return;

    size_t sz = strlen(szText);
    std::shared_ptr<RASocket> self;

    {
        std::lock_guard<std::mutex> guard(socket->_commandLock);
        socket->_commandOutput.push(std::string(szText, sz));
        self = socket->_commandSelf;
    }

    if (self)
        boost::asio::post(*self->_ioContext, [self]
        {
            self->drain_command_output();
        });
}

void RASocket::commandFinished(void* callbackArg, bool /*success*/)
{
    if (!callbackArg)
        return;

    RASocket* socket = static_cast<RASocket*>(callbackArg);
    std::shared_ptr<RASocket> self;

    {
        std::lock_guard<std::mutex> guard(socket->_commandLock);
        socket->_commandComplete = true;
        socket->_commandExecuting = false;
        self = socket->_commandSelf;

        if (socket->_closed)
        {
            socket->_commandSelf.reset();
            return;
        }
    }

    if (self)
        boost::asio::post(*self->_ioContext, [self]
        {
            self->drain_command_output();
        });
}
