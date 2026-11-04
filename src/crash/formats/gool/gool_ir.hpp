#pragma once

#include "crash/formats/nsf/nsf_archive.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CrashEdit::Crash {

// Game-independent semantic snapshot of a GOOL object. Binary-format details are
// retained alongside decoded meaning so unsupported instructions remain lossless.
enum class GoolIROperandKind : std::uint8_t {
    Value, Reference, DestinationReference, ProcessField, Signed
};

struct GoolIROperand {
    char name = '?';
    std::uint8_t bitWidth = 0;
    std::uint32_t rawValue = 0;
    std::int32_t value = 0;
    GoolIROperandKind kind = GoolIROperandKind::Value;
    std::string resolvedText;
};

struct GoolIRInstruction {
    std::size_t sourceByteOffset = 0;
    std::uint32_t rawWord = 0;
    std::uint32_t rawOperandBits = 0;
    std::uint8_t opcode = 0;
    bool nativeMips = false;
    bool opcodeKnown = false;
    bool nativeInstructionDecoded = false;
    std::string mnemonic;
    std::string nativeArguments;
    std::vector<GoolIROperand> operands;
};

struct GoolIRStateDescriptor {
    std::int32_t stateFlags = 0;
    std::int32_t blockFlags = 0;
    std::int16_t goolIndex = 0;
    std::int16_t eventHook = 0;
    std::int16_t transitionHook = 0;
    std::int16_t codeHook = 0;
};

struct GoolIRStateReference {
    std::size_t descriptorIndex = 0;
    std::int32_t dataPoolIndex = 0;
    std::uint32_t eid = 0;
    bool targetFound = false;
    std::uint8_t status = 0;
    std::size_t targetChunkIndex = 0;
    std::size_t targetEntryIndex = 0;
};

struct GoolIRStateHook {
    std::size_t descriptorIndex = 0;
    std::uint8_t kind = 0;
    std::uint8_t status = 0;
    std::uint16_t rawValue = 0;
    std::uint16_t instructionOffset = 0;
    bool external = false;
    bool targetFound = false;
    std::size_t targetChunkIndex = 0;
    std::size_t targetEntryIndex = 0;
};

struct GoolIRFrameGroup {
    std::uint16_t type = 0;
    std::int16_t frameCount = 0;
    std::uint32_t eid = 0;
    std::size_t sourceByteOffset = 0;
    std::size_t sourceByteSize = 0;
    std::uint32_t partCount = 0;
    std::int32_t font = 0;
    bool interpolated = false;
};

struct GoolIRProgram {
    std::uint8_t targetVersion = 0;
    std::int32_t id = 0;
    std::int32_t classId = 0;
    std::int32_t format = 0;
    std::int32_t heapBase = 0;
    std::int32_t eventCount = 0;
    std::int32_t entryCount = 0;
    std::vector<GoolIRInstruction> instructions;
    std::vector<std::int32_t> dataPool;
    std::vector<std::int16_t> stateMap;
    std::vector<GoolIRStateDescriptor> stateDescriptors;
    std::vector<GoolIRStateReference> stateReferences;
    std::vector<GoolIRStateHook> stateHooks;
    std::size_t frameGroupBytes = 0;
    bool frameGroupsDecoded = false;
    std::vector<GoolIRFrameGroup> frameGroups;
    std::string frameGroupError;
};

// Lower a decoded Crash 2/GOOL-format object into the shared semantic model.
// Raw words, offsets, and unresolved fields are retained; this is not a decompiler.
GoolIRProgram buildGoolIR(const NsfGool& source);

}
