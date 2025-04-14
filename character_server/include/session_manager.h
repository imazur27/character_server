#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include "database_manager.h"
#include "protocol.h"

/**
 * \class SessionManager
 * \brief Manages client sessions for the server.
 *
 * This class is responsible for accepting new client connections and managing
 * the lifecycle of sessions. It handles the asynchronous operations related
 * to client communication.
 */
class SessionManager {
public:
    /**
     * \brief Constructs a SessionManager with the given IO context.
     * \param ioContext The IO context used for asynchronous operations.
     */
    explicit SessionManager(boost::asio::io_context& ioContext);

    /// Destructor for SessionManager.
    ~SessionManager();

    /**
     * \brief Starts accepting new client connections.
     * \param acceptor The acceptor used to listen for incoming connections.
     */
    void startAccept(boost::asio::ip::tcp::acceptor& acceptor);

    /**
     * \brief Stops the SessionManager and cleans up resources.
     */
    void stop();

private:
    /**
     * \class Session
     * \brief Represents a single client session.
     *
     * This class handles communication with a connected client, including
     * reading messages and sending responses.
     */
    class Session : public std::enable_shared_from_this<Session> {
    public:
        /**
         * \brief Constructs a Session with the given IO context and manager.
         * \param ioContext The IO context used for asynchronous operations.
         * \param manager Reference to the SessionManager managing this session.
         */
        Session(boost::asio::io_context& ioContext, SessionManager& manager);

        /**
         * \brief Starts the session, initiating communication with the client.
         */
        void start();

        /**
         * \brief Gets the socket associated with this session.
         * \return Reference to the socket used for communication.
         */
        boost::asio::ip::tcp::socket& socket() { return m_socket; }

    private:
        /**
         * \brief Reads the header of a message from the client.
         */
        void readHeader();

        /**
         * \brief Reads the body of a message from the client.
         */
        void readBody();

        /**
         * \brief Processes a received binary message.
         * \param message The binary message data (moved into the function).
         * \note This function uses move semantics to efficiently handle
         *       message processing in a thread pool with lambdas.
         */
        void processMessage(std::vector<uint8_t> &&message);

        /**
         * \brief Sends a response back to the client.
         * \param data The response data to send (moved into the function).
         */
        void sendResponse(std::vector<uint8_t> &&data);

        /**
         * \brief Closes the session and cleans up resources.
         */
        void close();

        /**
         * \brief Handles session timeout.
         * \param ec The error code associated with the timeout.
         */
        void handleTimeout(const boost::system::error_code& ec);

        boost::asio::ip::tcp::socket m_socket; ///< Socket for client communication.
        boost::asio::steady_timer m_timeoutTimer; ///< Timer for session timeouts.
        SessionManager& m_manager; ///< Reference to the managing SessionManager.
        std::vector<uint8_t> m_readBuffer; ///< Buffer for reading incoming messages.
        std::vector<uint8_t> m_writeBuffer; ///< Buffer for outgoing messages.
        uint8_t m_currentCommand = 0; ///< Current command being processed.
    };

    /**
     * \brief Handles the acceptance of a new client session.
     * \param session The newly created session.
     * \param acceptor The acceptor used to listen for incoming connections.
     * \param error The error code, if any occurred during acceptance.
     */
    void handleAccept(
            std::shared_ptr<Session> session,
            boost::asio::ip::tcp::acceptor& acceptor,
            const boost::system::error_code& error
            );

    boost::asio::io_context& m_ioContext; ///< IO context for asynchronous operations.
    boost::asio::thread_pool m_threadPool; ///< Thread pool for processing messages.
    std::atomic<size_t> m_activeConnections{0}; ///< Count of active connections.
    std::mutex m_mutex; ///< Mutex for synchronizing access to shared resources.
    bool m_stopping = false; ///< Flag indicating if the manager is stopping.
};

#endif // SESSIONMANAGER_H
