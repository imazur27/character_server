#include "database_manager.h"
#include <stdexcept>
#include <sstream>

// for memcpy()
#include <cstring>
#include <iostream>

// Helper method to write primitive types to buffer
template<typename T>
void write_to_buffer(std::vector<uint8_t>& buffer, const T& value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

// Helper methods to read primitive types from buffer
template<typename T>
T read_from_buffer(const std::vector<uint8_t>& buffer, size_t& offset) {
    T value;
    memcpy(&value, buffer.data() + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

void CharacterData::write_string(std::vector<uint8_t>& buffer, const std::string& str) {
    uint32_t length = static_cast<uint32_t>(str.size());
    write_to_buffer(buffer, length);
    buffer.insert(buffer.end(), str.begin(), str.end());
}

std::string CharacterData::read_string(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t length = read_from_buffer<uint32_t>(buffer, offset);
    std::string str(buffer.begin() + offset, buffer.begin() + offset + length);
    offset += length;
    return str;
}

std::vector<uint8_t> CharacterData::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(
                // id
                sizeof(id) +
                // name size + name
                sizeof(uint32_t) + name.size() +
                // surname size + name
                sizeof(uint32_t) + surname.size() +
                // age
                sizeof(uint8_t) +
                // bio size + bio
                sizeof(uint32_t) + bio.size()
                );

    write_to_buffer(buffer, id);
    write_string(buffer, name);
    write_string(buffer, surname);
    write_to_buffer<uint8_t>(buffer, age);

    write_string(buffer, bio);

    return buffer;
}

CharacterData CharacterData::deserialize(const std::vector<uint8_t>& data) {
    CharacterData character;
    size_t offset = 0;

    character.id = read_from_buffer<int32_t>(data, offset);
    character.name = read_string(data, offset);
    character.surname = read_string(data, offset);
    character.age = read_from_buffer<uint8_t>(data, offset);
    character.bio = read_string(data, offset);

    return character;
}

std::vector<uint8_t> CharacterData::serializeVector(const std::vector<CharacterData>& characters) {
    std::vector<uint8_t> buffer;
    uint32_t count = static_cast<uint32_t>(characters.size());
    write_to_buffer(buffer, count);

    for (const auto& character : characters) {
        std::vector<uint8_t> char_data = character.serialize();
        uint32_t size = static_cast<uint32_t>(char_data.size());
        write_to_buffer(buffer, size);
        buffer.insert(buffer.end(), char_data.begin(), char_data.end());
    }

    return buffer;
}

std::vector<CharacterData> CharacterData::deserializeVector(const std::vector<uint8_t>& data) {
    size_t offset = 0;
    uint32_t count = read_from_buffer<uint32_t>(data, offset);
    std::vector<CharacterData> characters;
    characters.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t size = read_from_buffer<uint32_t>(data, offset);
        std::vector<uint8_t> char_data(data.begin() + offset,
                                       data.begin() + offset + size);
        characters.push_back(deserialize(char_data));
        offset += size;
    }

    return characters;
}

DatabaseManager::~DatabaseManager() {
    if (m_connection) {
        mysql_close(m_connection);
    }
}

DatabaseManager &DatabaseManager::getInstance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize(const std::string& host, const std::string& user,
                                 const std::string& pass, const std::string& db) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    m_connection = mysql_init(nullptr);
    if (!m_connection) return false;

    // 5 seconds
    unsigned int timeout = 5;
    mysql_options(m_connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(m_connection, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(m_connection, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

    if (!mysql_real_connect(m_connection, host.c_str(), user.c_str(),
                            pass.c_str(), db.c_str(), 0, nullptr, 0)) {
        mysql_close(m_connection);
        m_connection = nullptr;
        return false;
    }

    const char* createTable =
            "CREATE TABLE IF NOT EXISTS characters ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "name VARCHAR(50) NOT NULL, "
            "surname VARCHAR(50) NOT NULL, "
            "age INT NOT NULL, "
            "bio TEXT NOT NULL) ENGINE=InnoDB";

    return executeQuery(createTable);
}

bool DatabaseManager::addCharacter(const CharacterData& character) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    // Prepare statement
    std::string query = "INSERT INTO characters (name, surname, age, bio) VALUES (?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(m_connection);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    // Bind parameters
    MYSQL_BIND bind[4]{};

    // Name
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)character.name.c_str();
    bind[0].buffer_length = character.name.length();

    // Surname
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)character.surname.c_str();
    bind[1].buffer_length = character.surname.length();

    // Age
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = (void*)&character.age;
    bind[2].is_unsigned = false;

    // Bio
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)character.bio.c_str();
    bind[3].buffer_length = character.bio.length();

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    // Execute
    bool result = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    return result;
}

bool DatabaseManager::updateCharacter(int id, const CharacterData& character) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    std::string query =
            "UPDATE characters SET name = ?, surname = ?, age = ?, bio = ? "
            "WHERE id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(m_connection);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[5]{};

    // Name
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)character.name.c_str();
    bind[0].buffer_length = character.name.length();

    // Surname
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)character.surname.c_str();
    bind[1].buffer_length = character.surname.length();

    // Age
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = (void*)&character.age;
    bind[2].is_unsigned = false;

    // Bio
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)character.bio.c_str();
    bind[3].buffer_length = character.bio.length();

    // ID
    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = (void*)&id;
    bind[4].is_unsigned = false;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool result = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    return result;
}

bool DatabaseManager::deleteCharacter(int id) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    std::string query = "DELETE FROM characters WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(m_connection);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind{};
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = (void*)&id;
    bind.is_unsigned = false;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool result = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    return result;
}

std::vector<CharacterData> DatabaseManager::getAllCharacters() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<CharacterData> characters;

    std::string query = "SELECT id, name, surname, age, bio FROM characters";
    if (mysql_query(m_connection, query.c_str())) {
        return characters;
    }

    MYSQL_RES* result = mysql_store_result(m_connection);
    if (!result) {
        return characters;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        CharacterData character;
        character.id = std::stoi(row[0]);
        character.name = row[1] ? row[1] : "";
        character.surname = row[2] ? row[2] : "";
        character.age = std::stoi(row[3]);
        character.bio = row[4] ? row[4] : "";
        characters.push_back(character);
    }

    mysql_free_result(result);
    return characters;
}

std::optional<CharacterData> DatabaseManager::getCharacter(int id) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::string query = "SELECT id, name, surname, age, bio FROM characters WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(m_connection);
    if (!stmt) {
        return std::nullopt;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    // Bind input parameter
    MYSQL_BIND bind{};
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = &id;
    bind.is_unsigned = false;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    // Setup result bindings
    CharacterData character;
    my_bool is_null[5] = {0};
    my_bool error[5] = {0};

    MYSQL_BIND result_bind[5]{};

    // ID
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &character.id;
    result_bind[0].is_null = &is_null[0];
    result_bind[0].error = &error[0];

    // Name
    char name_buffer[51] = {0};
    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name_buffer;
    result_bind[1].buffer_length = sizeof(name_buffer);
    result_bind[1].is_null = &is_null[1];
    result_bind[1].error = &error[1];

    // Surname
    char surname_buffer[51] = {0};
    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = surname_buffer;
    result_bind[2].buffer_length = sizeof(surname_buffer);
    result_bind[2].is_null = &is_null[2];
    result_bind[2].error = &error[2];

    // Age
    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &character.age;
    result_bind[3].is_null = &is_null[3];
    result_bind[3].error = &error[3];

    // Bio
    char bio_buffer[4096] = {0};
    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = bio_buffer;
    result_bind[4].buffer_length = sizeof(bio_buffer);
    result_bind[4].is_null = &is_null[4];
    result_bind[4].error = &error[4];

    if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    // Fetch results
    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    // Copy string data
    character.name = name_buffer;
    character.surname = surname_buffer;
    character.bio = bio_buffer;

    mysql_stmt_close(stmt);
    return character;
}

bool DatabaseManager::executeQuery(const std::string& query) {
    return mysql_query(m_connection, query.c_str()) == 0;
}
