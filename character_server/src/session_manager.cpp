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
      m_timeoutTimer(ioContext),
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

        // Start reading
        readHeader();

    } catch (const std::exception& e) {
        std::cerr << "Session construction failed: " << e.what() << std::endl;
        close();
        // Re-throw to prevent incorrect session from being used by anybody
        throw;
    }
}

void SessionManager::Session::readHeader() {
    // Set timeout
    m_timeoutTimer.expires_after(std::chrono::milliseconds(Protocol::READ_TIMEOUT));
    m_timeoutTimer.async_wait(
        [self = shared_from_this()](const boost::system::error_code& ec) {
            if (!ec) self->m_socket.cancel();
        });

    // Command byte
    m_readBuffer.resize(1);

    boost::asio::async_read(m_socket,
        boost::asio::buffer(m_readBuffer),
        [self = shared_from_this()](const boost::system::error_code& ec, size_t) {
            self->m_timeoutTimer.cancel();
            if (ec) {
                self->close();
                return;
            }
            // get the command byte and read the rest of message
            self->m_currentCommand = self->m_readBuffer[0];
            self->readBody();
        });
}

void SessionManager::Session::readBody() {
    // Clear previous content but keep capacity
    m_readBuffer.clear();
    // Set timeout
    m_timeoutTimer.expires_after(std::chrono::milliseconds(Protocol::READ_TIMEOUT));
    m_timeoutTimer.async_wait(
        [self = shared_from_this()](const boost::system::error_code& ec) {
            if (!ec) self->m_socket.cancel();
        });

    // Read until message delimiter
    boost::asio::async_read_until(m_socket,
                                  boost::asio::dynamic_buffer(m_readBuffer),  // Create temporary dynamic buffer
                                  Protocol::MESSAGE_DELIMITER,
                                  [self = shared_from_this()](const boost::system::error_code& ec, size_t bytes_transferred) {
        self->m_timeoutTimer.cancel();
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                self->close();
            }
            return;
        }

        // Check that we actually found the delimiter
        auto delim_it = std::search(
                    self->m_readBuffer.begin(), self->m_readBuffer.end(),
                    Protocol::MESSAGE_DELIMITER.begin(), Protocol::MESSAGE_DELIMITER.end()
                    );

        // No delimiter, tcp framed the message, so read further
        if (delim_it == self->m_readBuffer.end()) {
            self->readBody();
            return;
        }

        // Extract message (excluding CRLF)
        std::vector<uint8_t> message(self->m_readBuffer.begin(), delim_it);

        // Remove processed data (including CRLF)
        self->m_readBuffer.erase(
                    self->m_readBuffer.begin(),
                    self->m_readBuffer.begin() + bytes_transferred
                    );

        // Process message in thread pool
        boost::asio::post(self->m_manager.m_threadPool,
                          [self, message = std::move(message)]() mutable {
            self->processMessage(std::move(message));
        });

        // tcp stacks messages, so read further
        if (!self->m_readBuffer.empty()) {
            self->readBody();
        }
    });
}

void SessionManager::Session::processMessage(std::vector<uint8_t>&& message) {
    try {
        std::vector<uint8_t> response;
        size_t offset = 0;

        switch (m_currentCommand) {
        case Protocol::GET_ALL: {
            auto characters = DatabaseManager::getInstance().getAllCharacters();
            response.push_back(Protocol::GET_ALL);

            if (!characters.empty()) {
                auto char_data = CharacterData::serializeVector(characters);
                response.insert(response.end(), char_data.begin(), char_data.end());
            }
            sendResponse(std::move(response));
            break;
        }

        case Protocol::GET_ONE: {
            if (message.size() < sizeof(int)) {
                throw std::runtime_error("Invalid message size for GET_ONE");
                return;
            }
            int id = 0;
            std::memcpy(&id, message.data(), sizeof(id));

            if (auto character = DatabaseManager::getInstance().getCharacter(id)) {
                auto charData = character->serialize();
                response.push_back(Protocol::GET_ONE);
                response.insert(response.end(), charData.begin(), charData.end());
                sendResponse(std::move(response));
            } else {
                sendResponse({Protocol::RESP_ERROR});
            }
            break;
        }

        case Protocol::ADD_CHARACTER: {
            CharacterData character = CharacterData::deserialize(message);
            if (DatabaseManager::getInstance().addCharacter(character)) {
                sendResponse({Protocol::RESP_SUCCESS});
            } else {
                throw std::runtime_error("Failed to add character");
            }
            break;
        }

        case Protocol::REMOVE_CHARACTER: {
            if (message.size() < sizeof(int)) {
                throw std::runtime_error("Invalid message size for REMOVE_CHARACTER");
            }
            int32_t id;
            std::memcpy(&id, message.data(), sizeof(id));

            if (DatabaseManager::getInstance().deleteCharacter(id)) {
                sendResponse({Protocol::RESP_SUCCESS});
            } else {
                sendResponse({Protocol::RESP_ERROR});
            }
            break;
        }

        case Protocol::UPDATE_CHARACTER: {
            if (message.size() < sizeof(int)) {
                throw std::runtime_error("Invalid message size for UPDATE_CHARACTER");
            }
            int id;
            std::memcpy(&id, message.data(), sizeof(id));

            CharacterData character = CharacterData::deserialize(message);

            if (DatabaseManager::getInstance().updateCharacter(id, character)) {
                sendResponse({Protocol::RESP_SUCCESS});
            } else {
                throw std::runtime_error("Failed to update character");
            }
            break;
        }

        default: {
            std::cerr << "Unknown command received: 0x" << std::hex
                      << static_cast<int>(m_currentCommand) << std::endl;
            sendResponse({Protocol::RESP_ERROR});
            close();
            break;
        }
        }
    } catch (const std::exception& e) {
        std::cerr << "Processing error: " << e.what() << std::endl;
        sendResponse({Protocol::RESP_ERROR});
        close();
    }
}

void SessionManager::Session::sendResponse(std::vector<uint8_t> &&data) {
    // Set timeout
    m_timeoutTimer.expires_after(std::chrono::milliseconds(Protocol::WRITE_TIMEOUT));
    m_timeoutTimer.async_wait(
        [self = shared_from_this()](const boost::system::error_code& ec) {
            if (!ec) self->m_socket.cancel();
        });

    data.insert(data.end(), Protocol::MESSAGE_DELIMITER.begin(), Protocol::MESSAGE_DELIMITER.end());

    boost::asio::async_write(m_socket,
        boost::asio::buffer(data),
        [self = shared_from_this()](const boost::system::error_code& ec, size_t) {
            self->m_timeoutTimer.cancel();
            if (!ec) {
                // Continue processing
                self->readHeader();
            } else {
                self->close();
            }
        });
}

void SessionManager::Session::close() {
    boost::system::error_code ec;
    m_timeoutTimer.cancel(ec);
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
    --m_manager.m_activeConnections;
}
