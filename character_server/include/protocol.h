/**
 * \file protocol.h
 * \brief Common protocol definitions for client-server communication
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>
#include <string>

/**
 * \struct CharacterData
 * \brief Represents character information
 */
struct CharacterData {
    int32_t id = 0; ///< Unique identifier for the character
    std::string name{}; ///< Character's first name
    std::string surname{}; ///< Character's surname
    uint8_t age = 1; ///< Character's age
    std::string bio{}; ///< Character's biography

    /**
     * \brief Serializes the CharacterData into a byte vector.
     * \return A vector of bytes representing the serialized character data.
     */
    std::vector<uint8_t> serialize() const;

    /**
     * \brief Deserializes a byte vector into a CharacterData object.
     * \param data A vector of bytes containing serialized character data.
     * \return A CharacterData object populated with the deserialized data.
     */
    static CharacterData deserialize(const std::vector<uint8_t>& data);

    /**
     * \brief Serializes a vector of CharacterData objects into a byte vector.
     * \param characters A vector of CharacterData objects to serialize.
     * \return A vector of bytes representing the serialized character data.
     */
    static std::vector<uint8_t> serializeVector(const std::vector<CharacterData>& characters);

    /**
     * \brief Deserializes a byte vector into a vector of CharacterData objects.
     * \param data A vector of bytes containing serialized character data.
     * \return A vector of CharacterData objects populated with the deserialized data.
     */
    static std::vector<CharacterData> deserializeVector(const std::vector<uint8_t>& data);

    /**
     * \brief Writes a string to a byte buffer.
     * \param buffer The buffer to write to.
     * \param str The string to write.
     */
    static void write_string(std::vector<uint8_t>& buffer, const std::string& str);

    /**
     * \brief Reads a string from a byte buffer.
     * \param buffer The buffer to read from.
     * \param offset The current offset in the buffer, which will be updated.
     * \return The read string.
     */
    static std::string read_string(const std::vector<uint8_t>& buffer, size_t& offset);
};

namespace Protocol {
// Command bytes
constexpr uint8_t GET_ALL = 0x01; ///< Command to get all characters
constexpr uint8_t ADD_CHARACTER = 0x02; ///< Command to add a new character
constexpr uint8_t REMOVE_CHARACTER = 0x03; ///< Command to remove a character
constexpr uint8_t GET_ONE = 0x04; ///< Command to get a specific character
constexpr uint8_t UPDATE_CHARACTER = 0x05; ///< Command to update character information

// Response codes
constexpr uint8_t RESP_SUCCESS = 0x80; ///< Response indicating success
constexpr uint8_t RESP_ERROR = 0x81; ///< Response indicating an error

// Connection limits
constexpr size_t MAX_CONNECTIONS = 1000; ///< Maximum number of concurrent connections
// 2x typical core count
constexpr size_t THREAD_POOL_SIZE = 16; ///< Size of the thread pool for handling requests

// Timeouts (milliseconds)
// 30000 seconds
constexpr unsigned READ_TIMEOUT = 30'000'000; ///< Timeout for read operations
// 10000 seconds
constexpr unsigned WRITE_TIMEOUT = 10'000'000; ///< Timeout for write operations

// Network settings
// This is the hardcoded server port
constexpr int PORT = 12345; ///< Server port for communication

// Message delimiter
constexpr std::string_view MESSAGE_DELIMITER = "\r\n"; ///< Delimiter for messages
constexpr uint8_t MESSAGE_DELIMITER_SIZE = 2; ///< Size of the message delimiter
}

#endif // PROTOCOL_H
