#pragma once
#include <cstdint>
#include <string>

// Minimal Arduino String compatibility for production text helpers exercised by
// the native renderer. No Arduino platform or drawing code is emulated here.
class String : public std::string {
public:
    using std::string::string;
    String(const std::string& value) : std::string(value) {}
    String(int value) : std::string(std::to_string(value)) {}
    bool reserve(size_t size) { std::string::reserve(size); return true; }
    size_t write(uint8_t c) { push_back(static_cast<char>(c)); return 1; }
    size_t write(const uint8_t* data, size_t size) { append(reinterpret_cast<const char*>(data), size); return size; }
    bool startsWith(const char* prefix) const { return rfind(prefix, 0) == 0; }
    bool isEmpty() const { return empty(); }
    void remove(size_t offset) { erase(offset); }
    void remove(size_t offset, size_t count) { erase(offset, count); }
    int indexOf(char character) const {
        const auto position = find(character);
        return position == npos ? -1 : static_cast<int>(position);
    }
    String substring(size_t begin, size_t end) const { return substr(begin, end - begin); }
    void trim() {
        const auto first = find_first_not_of(" \t\r\n");
        if (first == npos) {
            clear();
            return;
        }
        *this = substr(first, find_last_not_of(" \t\r\n") - first + 1);
    }
};

#include <cstddef>
#include <cstdint>
