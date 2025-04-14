/**
 * \file ServerInstance.h
 * \brief Main server control singleton
 */

#ifndef SERVERINSTANCE_H
#define SERVERINSTANCE_H

#include <boost/asio.hpp>
#include <memory>
#include "protocol.h"

class SessionManager;

/**
 * \class ServerInstance
 * \brief Singleton class that manages the server instance.
 *
 * This class is responsible for initializing, running, and stopping the server.
 * It ensures that only one instance of the server is running at any time.
 */
class ServerInstance {
public:
    /**
     * \brief Gets the singleton instance of the ServerInstance.
     * \return Reference to the single instance of ServerInstance.
     */
    static ServerInstance& getInstance();

    /**
     * \brief Initializes the server on the specified port.
     * \param port The port number on which the server will listen for incoming connections.
     * \return True if initialization was successful, false otherwise.
     */
    bool initialize(unsigned short port);

    /**
     * \brief Runs the server, starting the asynchronous operations.
     */
    void run();

    /**
     * \brief Stops the server and cleans up resources.
     */
    void stop();

    /**
     * \brief Gets the IO context for asynchronous operations.
     * \return Reference to the boost::asio::io_context used by the server.
     */
    boost::asio::io_context& getIoContext() { return m_ioContext; }

private:
    /// Private constructor for singleton pattern.
    ServerInstance();

    /// Private destructor.
    ~ServerInstance();

    // Deleted copy constructor and assignment operator to enforce singleton.
    ServerInstance(const ServerInstance&) = delete;
    ServerInstance& operator=(const ServerInstance&) = delete;

    /**
     * \brief Enforces that only a single instance of the server can run.
     * \return True if the single instance enforcement was successful, false otherwise.
     */
    bool enforceSingleInstance();

    /**
     * \brief Cleans up the lock used for single instance enforcement.
     */
    void cleanupSingleInstanceLock();

    boost::asio::io_context m_ioContext; ///< IO context for asynchronous operations.
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor; ///< Accepts incoming connections.
    std::shared_ptr<SessionManager> m_sessionManager; ///< Manages active sessions.

    // For enforcing a single instance across platforms
#ifdef _WIN32
    HANDLE m_instanceMutex = nullptr; ///< Mutex handle for Windows.
#else
    int m_lockFileDescriptor = -1; ///< File descriptor for lock on Unix-like systems.
#endif
};

#endif // SERVERINSTANCE_H
