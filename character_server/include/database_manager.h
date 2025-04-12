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

#include <boost/serialization/vector.hpp>

#include "protocol.h"

struct CharacterData {
    int id;
    std::string name;
    std::string surname;
    int age;
    std::vector<uint8_t> image;
    std::string bio;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & id;
        ar & name;
        ar & surname;
        ar & age;
        ar & image;
        ar & bio;
    }
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
