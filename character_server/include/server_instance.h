/**
 * @file ServerInstance.h
 * @brief Main server control singleton
 */

#ifndef SERVERINSTANCE_H
#define SERVERINSTANCE_H

#include <boost/asio.hpp>
#include <memory>
#include "protocol.h"

class SessionManager;

class ServerInstance {
public:
    static ServerInstance& getInstance();

    bool initialize(unsigned short port);
    void run();
    void stop();

    boost::asio::io_context& getIoContext() { return m_ioContext; }

private:
    ServerInstance();
    ~ServerInstance();

    // No copy, it's singleton
    ServerInstance(const ServerInstance&) = delete;
    ServerInstance& operator=(const ServerInstance&) = delete;

    bool enforceSingleInstance();
    void cleanupSingleInstanceLock();

    boost::asio::io_context m_ioContext;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
    std::shared_ptr<SessionManager> m_sessionManager;

    // for one, single instance ultimately
#ifdef _WIN32
    HANDLE m_instanceMutex = nullptr;
#else
    int m_lockFileDescriptor = -1;
#endif
};

#endif // SERVERINSTANCE_H
