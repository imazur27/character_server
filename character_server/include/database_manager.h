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

struct CharacterData {
    int32_t id = 0;
    std::string name{};
    std::string surname{};
    uint8_t age = 1;
    std::string bio{};

    // Serialize/deserialize
    std::vector<uint8_t> serialize() const;
    static CharacterData deserialize(const std::vector<uint8_t>& data);

    // Helper methods to serialize/deserialize all characters
    static std::vector<uint8_t> serializeVector(const std::vector<CharacterData>& characters);
    static std::vector<CharacterData> deserializeVector(const std::vector<uint8_t>& data);

    // Helper methods
    static void write_string(std::vector<uint8_t>& buffer, const std::string& str);
    static std::string read_string(const std::vector<uint8_t>& buffer, size_t& offset);
};

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
