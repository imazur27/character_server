/**
 * \file DatabaseManager.h
 * \brief Header file for thread-safe MySQL database manager class
 *
 * This file contains the declaration of the DatabaseManager class which provides
 * a thread-safe interface for MySQL database operations related to character management.
 */

#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <mysql/mysql.h>
#include <mutex>
#include <optional>
#include <vector>

#include "protocol.h"

/**
 * \class DatabaseManager
 * \brief Singleton class for managing MySQL database connections and operations
 *
 * This class provides a thread-safe interface for performing CRUD operations
 * on character data in a MySQL database. It implements the singleton pattern
 * to ensure only one instance exists throughout the application.
 */
class DatabaseManager {
public:
    /**
     * \brief Gets the singleton instance of DatabaseManager
     * \return Reference to the single instance of DatabaseManager
     */
    static DatabaseManager& getInstance();

    /**
     * \brief Initializes the database connection
     * \param host MySQL server hostname or IP address
     * \param user MySQL username
     * \param pass MySQL password
     * \param db Database name to connect to
     * \return true if connection was successful, false otherwise
     */
    bool initialize(const std::string& host, const std::string& user,
                   const std::string& pass, const std::string& db);

    /**
     * \brief Adds a new character to the database
     * \param character CharacterData object containing character information
     * \return true if operation was successful, false otherwise
     */
    bool addCharacter(const CharacterData& character);

    /**
     * \brief Updates an existing character in the database
     * \param id ID of the character to update
     * \param character CharacterData object with updated information
     * \return true if operation was successful, false otherwise
     */
    bool updateCharacter(int id, const CharacterData& character);

    /**
     * \brief Deletes a character from the database
     * \param id ID of the character to delete
     * \return true if operation was successful, false otherwise
     */
    bool deleteCharacter(int id);

    /**
     * \brief Retrieves all characters from the database
     * \return Vector of CharacterData objects for all characters
     */
    std::vector<CharacterData> getAllCharacters();

    /**
     * \brief Retrieves a specific character from the database
     * \param id ID of the character to retrieve
     * \return Optional containing CharacterData if found, empty optional otherwise
     */
    std::optional<CharacterData> getCharacter(int id);

private:
    /**
     * \brief Private constructor for singleton pattern
     */
    DatabaseManager() = default;

    /**
     * \brief Destructor that cleans up database connection
     */
    ~DatabaseManager();

    // Prevent copying and assignment
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    /**
     * \brief Executes a SQL query on the database
     * \param query The SQL query string to execute
     * \return true if query executed successfully, false otherwise
     * \note This method is thread-safe
     */
    bool executeQuery(const std::string& query);

    /*!
     * \brief MySQL connection
     */
    MYSQL* m_connection = nullptr;
    /*!
     * \brief Mutex for thread-safe database access
     */
    std::mutex m_dbMutex;
};

#endif // DATABASEMANAGER_H
