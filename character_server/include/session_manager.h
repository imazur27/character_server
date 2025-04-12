#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include "database_manager.h"
#include "protocol.h"

class SessionManager {
public:
    explicit SessionManager(boost::asio::io_context& ioContext);
    ~SessionManager();

    void startAccept(boost::asio::ip::tcp::acceptor& acceptor);
    void stop();

private:
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(boost::asio::io_context& ioContext, SessionManager& manager);
        void start();
        boost::asio::ip::tcp::socket& socket() { return m_socket; }

    private:
        void readHeader();
        void readBody();
        /*!
         * \brief processMessage Processes a received binary message
         * \param message The binary message data (moved into the function)
         * \note need to use && and move semantics, cause processing messages performingin thread pool
         * with lambdas
         */
        void processMessage(std::vector<uint8_t> &&message);
        void sendResponse(std::vector<uint8_t> &&data);
        void close();
        void handleTimeout(const boost::system::error_code& ec);

        boost::asio::ip::tcp::socket m_socket;
        boost::asio::steady_timer m_timeoutTimer;
        SessionManager& m_manager;
        std::vector<uint8_t> m_readBuffer;
        std::vector<uint8_t> m_writeBuffer;
        uint8_t m_currentCommand = 0;
    };

    void handleAccept(
            std::shared_ptr<Session> session,
            boost::asio::ip::tcp::acceptor& acceptor,
            const boost::system::error_code& error
            );

    boost::asio::io_context& m_ioContext;
    boost::asio::thread_pool m_threadPool;
    std::atomic<size_t> m_activeConnections{0};
    std::mutex m_mutex;
    bool m_stopping = false;
};

#endif // SESSIONMANAGER_H
