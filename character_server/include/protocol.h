/**
 * @file Protocol.h
 * @brief Server-client communication protocol definitions
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>

namespace Protocol {
// Command bytes
constexpr uint8_t GET_ALL = 0x01;
constexpr uint8_t ADD_CHARACTER = 0x02;
constexpr uint8_t REMOVE_CHARACTER = 0x03;
constexpr uint8_t GET_ONE = 0x04;
constexpr uint8_t UPDATE_CHARACTER = 0X05;

// Response codes
constexpr uint8_t RESP_SUCCESS = 0x80;
constexpr uint8_t RESP_ERROR = 0x81;

// Connection limits
constexpr size_t MAX_CONNECTIONS = 1000;
// 2x typical core count
constexpr size_t THREAD_POOL_SIZE = 16;

// Timeouts (milliseconds)
// 30000 seconds
constexpr unsigned READ_TIMEOUT = 30'000'000;
 // 10000 seconds
constexpr unsigned WRITE_TIMEOUT = 10'000'000;

// Network settings
constexpr int PORT = 12345;

// message delimiter
constexpr std::string_view MESSAGE_DELIMITER = "\r\n";
constexpr uint8_t MESSAGE_DELIMITER_SIZE = 2;
}

#endif // PROTOCOL_H
