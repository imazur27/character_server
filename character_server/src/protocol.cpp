#include "protocol.h"

#include <cstring>

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

    character.id = read_from_buffer<int>(data, offset);
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
