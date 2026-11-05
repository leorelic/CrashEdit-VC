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

enum class GoolIRMipsFormat : std::uint8_t { Unknown, Register, Immediate, Jump };

// Structural R3000A word fields. These are encoding facts, not a claim that
// the instruction's runtime side effects have been fully audited.
struct GoolIRMipsFields {
    GoolIRMipsFormat format = GoolIRMipsFormat::Unknown;
    std::uint8_t primaryOpcode = 0;
    std::uint8_t rs = 0;
    std::uint8_t rt = 0;
    std::uint8_t rd = 0;
    std::uint8_t shiftAmount = 0;
    std::uint8_t function = 0;
    std::uint16_t immediate = 0;
    std::int16_t signedImmediate = 0;
    std::uint32_t jumpTarget = 0;
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
    bool nativeFieldsDecoded = false;
    GoolIRMipsFields nativeFields;
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

// Typed hook dispatch extracted from a state descriptor. This records the
// encoded destination and its resolution; it does not assume when the engine
// invokes a hook or what the target instructions do.
enum class GoolIRHookKind : std::uint8_t { Event, Transition, Code };
enum class GoolIRHookResolution : std::uint8_t {
    Absent, InvalidDataIndex, MissingTarget, AmbiguousTarget,
    TargetUndecoded, OffsetOutOfRange, Local, External
};

struct GoolIRHookDispatch {
    std::size_t descriptorIndex = 0;
    GoolIRHookKind kind = GoolIRHookKind::Event;
    std::uint16_t rawValue = 0;
    std::uint16_t instructionOffset = 0;
    bool external = false;
    GoolIRHookResolution resolution = GoolIRHookResolution::Absent;
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

enum class GoolIRControlOperationKind : std::uint8_t { Call, Return, StateTransition };

struct GoolIRControlOperation {
    GoolIRControlOperationKind kind = GoolIRControlOperationKind::Call;
    std::size_t sourceInstruction = 0;
    std::size_t sourceByteOffset = 0;
    bool conditional = false;
    std::string conditionField;
    std::uint32_t argumentCount = 0;
    std::size_t targetInstruction = 0;
    bool targetInstructionValid = false;
    bool external = false;
    std::size_t targetDescriptorIndex = 0;
    bool targetDescriptorValid = false;
    std::uint32_t targetGoolEid = 0;
    bool targetGoolEidValid = false;
    bool hasTargetReferenceStatus = false;
    std::uint8_t targetReferenceStatus = 0;
    std::string diagnostic;
};

enum class GoolIRFlowEdgeKind : std::uint8_t {
    Fallthrough, Branch, BranchTrue, BranchFalse, Call, ExternalCall,
    StateTransition, StateTransitionTrue, StateTransitionFalse, Return
};

struct GoolIRBasicBlock {
    std::size_t startInstruction = 0;
    std::size_t endInstruction = 0; // exclusive
};

enum class GoolIRRoutineEntryKind : std::uint8_t { CodeHook, LocalCall };
struct GoolIRRoutineEntry {
    std::size_t instruction = 0;
    bool fromCodeHook = false;
    bool fromLocalCall = false;
};
struct GoolIRRoutine {
    std::size_t entryInstruction = 0;
    bool fromCodeHook = false;
    bool fromLocalCall = false;
    std::vector<std::size_t> instructions;
    bool hasSharedInstructions = false;
    std::vector<std::string> diagnostics;
};

struct GoolIRFlowEdge {
    std::size_t sourceBlock = 0;
    std::size_t targetInstruction = 0;
    GoolIRFlowEdgeKind kind = GoolIRFlowEdgeKind::Fallthrough;
    bool targetValid = false;
    std::string diagnostic;
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
    std::vector<std::size_t> instructionToBlock;
    std::vector<GoolIRBasicBlock> basicBlocks;
    std::vector<GoolIRRoutineEntry> routineEntries;
    std::vector<GoolIRRoutine> routines;
    std::vector<std::size_t> instructionRoutineMembership; // number of discovered routines reaching each instruction
    std::vector<std::size_t> unassignedInstructions; // no discovered entry point reaches these instructions
    std::vector<GoolIRFlowEdge> flowEdges;
    std::vector<GoolIRControlOperation> controlOperations;
    std::vector<std::string> controlFlowDiagnostics;
    std::vector<std::int32_t> dataPool;
    std::vector<std::int16_t> stateMap;
    std::vector<GoolIRStateDescriptor> stateDescriptors;
    std::vector<GoolIRStateReference> stateReferences;
    std::vector<GoolIRStateHook> stateHooks; // Loss-preserving source snapshot.
    std::vector<GoolIRHookDispatch> hookDispatches; // Typed descriptor-to-entrypoint relationships.
    std::size_t frameGroupBytes = 0;
    bool frameGroupsDecoded = false;
    std::vector<GoolIRFrameGroup> frameGroups;
    std::string frameGroupError;
};

// Lower a decoded Crash 2/GOOL-format object into the shared semantic model.
// Raw words, offsets, and unresolved fields are retained; this is not a decompiler.
GoolIRProgram buildGoolIR(const NsfGool& source);

}
