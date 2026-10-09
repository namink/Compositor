#include "compositor/model/uuid.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <random>

namespace compositor::model {
namespace {

constexpr std::array<std::size_t, 4> kHyphenPositions{8, 13, 18, 23};

[[nodiscard]] bool is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

}  // namespace

bool is_valid_uuid(const std::string& value) {
    if (value.size() != 36) {
        return false;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        const bool hyphen_here = i == kHyphenPositions[0] || i == kHyphenPositions[1] || i == kHyphenPositions[2] ||
                                 i == kHyphenPositions[3];
        const char c = value[i];
        if (hyphen_here ? (c != '-') : !is_hex(c)) {
            return false;
        }
    }
    return true;
}

std::string uppercase_uuid(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return value;
}

std::string generate_uuid() {
    static thread_local std::mt19937_64 engine{std::random_device{}()};
    std::uint64_t high = engine();
    std::uint64_t low = engine();
    // Version 4, variant 1 (RFC 4122), so the value is a well-formed random UUID.
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    static constexpr char kHex[] = "0123456789ABCDEF";
    std::array<std::uint8_t, 16> bytes{};
    for (int i = 0; i < 8; ++i) {
        bytes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(high >> (56 - 8 * i));
        bytes[static_cast<std::size_t>(8 + i)] = static_cast<std::uint8_t>(low >> (56 - 8 * i));
    }

    std::string out;
    out.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out.push_back('-');
        }
        out.push_back(kHex[bytes[i] >> 4]);
        out.push_back(kHex[bytes[i] & 0x0F]);
    }
    return out;
}

}  // namespace compositor::model
