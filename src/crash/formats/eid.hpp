#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace CrashEdit::Crash {
inline constexpr std::string_view ENameCharacterSet =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_!";

inline std::string eidToEName(std::uint32_t eid) {
    eid >>= 1U;
    char name[5]{};
    for (int i = 0; i < 5; ++i) {
        name[4 - i] = ENameCharacterSet[eid & 0x3FU];
        eid >>= 6U;
    }
    return std::string(name, 5);
}

inline std::uint32_t eNameToEid(std::string_view name) {
    if (name.size() != 5) throw std::invalid_argument("EName must contain exactly five characters.");
    std::uint32_t eid = 1U;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const std::size_t value = ENameCharacterSet.find(name[i]);
        if (value == std::string_view::npos) throw std::invalid_argument("EName contains an invalid character.");
        eid |= static_cast<std::uint32_t>(value) << (25U - 6U * static_cast<unsigned>(i));
    }
    return eid;
}

inline bool isValidEid(std::uint32_t eid) {
    return (eid & 1U) != 0U && (eid & 0x80000000U) == 0U;
}
} // namespace CrashEdit::Crash
