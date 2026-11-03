#pragma once
#include "crash/formats/nsf/nsf_archive.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CrashEdit::Crash {
struct NsfGoolOpcodeInfo {
    std::uint8_t opcode = 0;
    std::string_view mnemonic = {};
    bool known = false;
};
enum class NsfGoolOperandKind { Value, Reference, DestinationReference, ProcessField, Signed };
struct NsfGoolOperand {
    char name = '?';
    std::uint8_t bitWidth = 0;
    std::uint32_t rawValue = 0;
    std::int32_t value = 0;
    NsfGoolOperandKind kind = NsfGoolOperandKind::Value;
    std::string resolvedText;
};
const NsfGoolOpcodeInfo& goolOpcodeInfo(NsfGoolVersion version, std::uint8_t opcode) noexcept;
std::string_view goolOpcodeFormat(NsfGoolVersion version, std::uint8_t opcode) noexcept;
std::vector<NsfGoolOperand> decodeGoolOperands(NsfGoolVersion version, std::uint8_t opcode, std::uint32_t operandBits);
std::string formatGoolOperands(const std::vector<NsfGoolOperand>& operands);
std::string formatGoolControlFlow(std::uint8_t opcode, const std::vector<NsfGoolOperand>& operands);
void decodeMipsInstruction(std::uint32_t word, std::string& mnemonic, std::string& arguments, bool& known);
void resolveGoolOperands(std::vector<NsfGoolOperand>& operands, const NsfGool& gool, const std::vector<std::int32_t>* parentData = nullptr);
}
