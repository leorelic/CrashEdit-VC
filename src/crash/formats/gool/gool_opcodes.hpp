#pragma once
#include "crash/formats/nsf/nsf_archive.hpp"
#include <cstdint>
#include <string_view>

namespace CrashEdit::Crash {
struct NsfGoolOpcodeInfo {
    std::uint8_t opcode = 0;
    std::string_view mnemonic = {};
    bool known = false;
};

// Crash 2 (GOOL version 2) opcode names transcribed from the original
// CrashEdit GOOLInstructionList.cs. Unknown/reserved opcodes remain explicit.
const NsfGoolOpcodeInfo& goolOpcodeInfo(NsfGoolVersion version, std::uint8_t opcode) noexcept;
}
