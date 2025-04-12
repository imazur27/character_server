#include "server_instance.h"
#include "session_manager.h"
#include <iostream>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#endif

ServerInstance& ServerInstance::getInstance() {
    static ServerInstance instance;
    return instance;
}

ServerInstance::ServerInstance() {
    if (!enforceSingleInstance()) {
        throw std::runtime_error("Another server instance is already running");
    }
}

ServerInstance::~ServerInstance() {
    cleanupSingleInstanceLock();
}

bool ServerInstance::enforceSingleInstance() {
#ifdef WIN32
    m_instanceMutex = CreateMutexA(nullptr, TRUE, "CharacterServerInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return false;
    }
    return m_instanceMutex != nullptr;
#else
    std::string lockPath = "/var/lock/character_server.lock";
    m_lockFileDescriptor = open(lockPath.c_str(), O_RDWR | O_CREAT, 0644);
    if (m_lockFileDescriptor == -1) {
        return false;
    }
    if (flock(m_lockFileDescriptor, LOCK_EX | LOCK_NB) == -1) {
        close(m_lockFileDescriptor);
        return false;
    }
    return true;
#endif
}

void ServerInstance::cleanupSingleInstanceLock() {
#ifdef WIN32
    if (m_instanceMutex) {
        ReleaseMutex(m_instanceMutex);
        CloseHandle(m_instanceMutex);
    }
#else
    if (m_lockFileDescriptor != -1) {
        flock(m_lockFileDescriptor, LOCK_UN);
        close(m_lockFileDescriptor);
    }
#endif
}

bool ServerInstance::initialize(unsigned short port) {
    try {
        if (!DatabaseManager::getInstance().initialize("localhost", "character_user", "secure_password_123", "character_db")) {
            return false;
        }

        m_sessionManager = std::make_shared<SessionManager>(m_ioContext);
        m_acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(
            m_ioContext,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)
        );

        m_sessionManager->startAccept(*m_acceptor);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Initialization error: " << e.what() << std::endl;
        return false;
    }
}

void ServerInstance::run() {
    std::cout << "Server started. Press Ctrl+C to exit." << std::endl;
    m_ioContext.run();
}

void ServerInstance::stop() {
    m_ioContext.stop();
    if (m_acceptor) {
        m_acceptor->close();
    }
    if (m_sessionManager) {
        m_sessionManager->stop();
    }
}
