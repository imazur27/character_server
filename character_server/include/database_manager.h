/**
 * @file DatabaseManager.h
 * @brief Thread-safe MySQL database manager
 */

#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <mysql/mysql.h>
#include <mutex>
#include <optional>
#include <vector>

#include "protocol.h"

class DatabaseManager {
public:
    static DatabaseManager& getInstance();

    bool initialize(const std::string& host, const std::string& user,
                   const std::string& pass, const std::string& db);

    bool addCharacter(const CharacterData& character);
    bool updateCharacter(int id, const CharacterData& character);
    bool deleteCharacter(int id);
    std::vector<CharacterData> getAllCharacters();
    std::optional<CharacterData> getCharacter(int id);

private:
    DatabaseManager() = default;
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    MYSQL* m_connection = nullptr;
    std::mutex m_dbMutex;

    bool executeQuery(const std::string& query);
};

#endif // DATABASEMANAGER_H
