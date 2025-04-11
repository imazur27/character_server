#include "session_manager.h"
#include <sstream>
#include <iostream>

SessionManager::SessionManager(boost::asio::io_context& ioContext)
    : m_ioContext(ioContext),
      m_threadPool(Protocol::THREAD_POOL_SIZE) {}

SessionManager::~SessionManager() {
    stop();
}

void SessionManager::startAccept(boost::asio::ip::tcp::acceptor& acceptor) {
    if (m_stopping) return;

    auto session = std::make_shared<Session>(m_ioContext, *this);

    acceptor.async_accept(session->socket(),
        [this, session, &acceptor](const boost::system::error_code& error) {
            handleAccept(session, acceptor, error);
        });
}

void SessionManager::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stopping = true;
    m_threadPool.join();
}

void SessionManager::handleAccept(std::shared_ptr<Session> session,
                                boost::asio::ip::tcp::acceptor& acceptor,
                                const boost::system::error_code& error) {
    if (error) {
        if (error != boost::asio::error::operation_aborted) {
            std::cerr << "Accept error: " << error.message() << std::endl;
        }
        return;
    }

    if (m_activeConnections >= Protocol::MAX_CONNECTIONS) {
        std::cerr << "Connection limit reached ("
                 << Protocol::MAX_CONNECTIONS << ")" << std::endl;
        return;
    }

    ++m_activeConnections;
    session->start();
    startAccept(acceptor);
}

// Session implementation
SessionManager::Session::Session(boost::asio::io_context& ioContext,
                               SessionManager& manager)
    : m_socket(ioContext),
      m_manager(manager)
{

}

void SessionManager::Session::start() {
    try {
        // Verify socket is open before setting options
        if (!m_socket.is_open()) {
            throw std::runtime_error("Socket not open during construction");
        }

        // Set TCP_NODELAY option (disable Nagle's algorithm)
        boost::system::error_code ec;
        m_socket.set_option(boost::asio::ip::tcp::no_delay(true), ec);
        if (ec) {
            throw std::runtime_error("Failed to set TCP_NODELAY: " + ec.message());
        }

        // Set keep-alive
        m_socket.set_option(boost::asio::socket_base::keep_alive(true), ec);
        if (ec) {
            throw std::runtime_error("Warning: Keep-alive failed: " + ec.message());
        }

//        // Platform-specific timeout settings
//#ifdef _WIN32
//        // Windows timeout settings (milliseconds)
//        DWORD timeout = Protocol::READ_TIMEOUT;
//        if (setsockopt(m_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
//                       reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR) {
//            throw std::runtime_error("Failed to set Windows receive timeout");
//        }
//#else
//        // Linux/Unix timeout settings
//        struct timeval tv;
//        tv.tv_sec = Protocol::READ_TIMEOUT / 1000;
//        tv.tv_usec = (Protocol::READ_TIMEOUT % 1000) * 1000;
//        if (setsockopt(m_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
//                       &tv, sizeof(tv)) != 0) {
//            throw std::runtime_error("Failed to set Unix receive timeout");
//        }
//#endif

        // Start reading
        readHeader();

    } catch (const std::exception& e) {
        std::cerr << "Session construction failed: " << e.what() << std::endl;
        close();
        throw; // Re-throw to prevent invalid session from being used
    }
}

void SessionManager::Session::readHeader() {
    m_readBuffer.resize(1);

    boost::asio::async_read(m_socket,
        boost::asio::buffer(m_readBuffer),
        [self = shared_from_this()](const boost::system::error_code& ec, size_t) {
            if (ec) {
                self->close();
                return;
            }
            self->m_currentCommand = self->m_readBuffer[0];
            self->readBody();
        });
}

void SessionManager::Session::readBody() {
    // Clear previous content but keep capacity
    m_readBuffer.clear();
    // Read until newline ('\n' as separator) using the member buffer
    boost::asio::async_read_until(m_socket,
                                  boost::asio::dynamic_buffer(m_readBuffer),  // Create temporary dynamic buffer
                                  '\n',
                                  [self = shared_from_this()](const boost::system::error_code& ec, size_t bytes_transferred) {
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                self->close();
            }
            return;
        }

        // Extract the complete message (excluding delimiter)
        std::string message(self->m_readBuffer.begin(),
                            self->m_readBuffer.begin() + bytes_transferred);

        // Remove processed data from buffer
        self->m_readBuffer.erase(self->m_readBuffer.begin(),
                                 self->m_readBuffer.begin() + bytes_transferred);

        // Process message in thread pool
        boost::asio::post(self->m_manager.m_threadPool,
                          [self, message = std::move(message)]() {
            self->processMessage(message);
        });
    });
}

void SessionManager::Session::processMessage(const std::string &message) {
    try {
        std::istringstream iss(message);

        // TODO: Get some data from db, use MySql

        switch (m_currentCommand) {
        case Protocol::GET_ALL: {

            break;
        }

        case Protocol::GET_ONE: {

            break;
        }

        case Protocol::ADD_CHARACTER: {

            break;
        }

        case Protocol::REMOVE_CHARACTER: {

            break;
        }

        case Protocol::UPDATE_CHARACTER: {

            break;
        }

        default: {
            // Unknown command
            std::cerr << "Unknown command received: 0x" << std::hex
                      << static_cast<int>(m_currentCommand) << std::endl;
            std::vector<uint8_t> response{Protocol::RESP_ERROR};
            sendResponse(response);
            close();
            break;
        }
        }
    } catch (const std::exception& e) {
        std::cerr << "Processing error: " << e.what() << std::endl;
        std::vector<uint8_t> response{Protocol::RESP_ERROR};
        sendResponse(response);
        close();
    }
}

void SessionManager::Session::sendResponse(const std::vector<uint8_t>& data) {
    boost::asio::async_write(m_socket,
        boost::asio::buffer(data),
        [self = shared_from_this()](const boost::system::error_code& ec, size_t) {
            if (!ec) {
                self->readHeader(); // Continue processing
            } else {
                self->close();
            }
        });
}

void SessionManager::Session::close() {
    boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
    --m_manager.m_activeConnections;
}
