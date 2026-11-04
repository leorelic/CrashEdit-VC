#include "crash/formats/gool/gool_ir.hpp"
#include "crash/formats/gool/gool_control_flow.hpp"
#include <utility>
#include "crash/formats/gool/gool_opcodes.hpp"

namespace CrashEdit::Crash {
namespace {
GoolIROperandKind convertOperandKind(NsfGoolOperandKind kind) {
    switch (kind) {
    case NsfGoolOperandKind::Reference: return GoolIROperandKind::Reference;
    case NsfGoolOperandKind::DestinationReference: return GoolIROperandKind::DestinationReference;
    case NsfGoolOperandKind::ProcessField: return GoolIROperandKind::ProcessField;
    case NsfGoolOperandKind::Signed: return GoolIROperandKind::Signed;
    case NsfGoolOperandKind::Value: default: return GoolIROperandKind::Value;
    }
}
}

GoolIRProgram buildGoolIR(const NsfGool& source) {
    GoolIRProgram result;
    result.targetVersion = static_cast<std::uint8_t>(source.version);
    result.id = source.id;
    result.classId = source.classId;
    result.format = source.format;
    result.heapBase = source.heapBase;
    result.eventCount = source.eventCount;
    result.entryCount = source.entryCount;
    result.dataPool = source.dataPool;
    result.stateMap = source.stateMap;
    result.frameGroupBytes = source.frameGroupBytes;
    result.frameGroupsDecoded = source.frameGroupsDecoded;
    result.frameGroupError = source.frameGroupError;

    result.instructions.reserve(source.instructions.size());
    for (std::size_t index = 0; index < source.instructions.size(); ++index) {
        const auto& instruction = source.instructions[index];
        GoolIRInstruction ir;
        ir.sourceByteOffset = instruction.byteOffset;
        ir.rawWord = instruction.word;
        ir.rawOperandBits = instruction.operandBits;
        ir.opcode = instruction.opcode;
        ir.nativeMips = instruction.isMips;
        ir.nativeInstructionDecoded = instruction.mipsKnown;
        ir.mnemonic = instruction.mnemonic;
        ir.nativeArguments = instruction.arguments;
        if (!instruction.isMips) {
            const auto& info = goolOpcodeInfo(source.version, instruction.opcode);
            ir.opcodeKnown = info.known && !goolOpcodeFormat(source.version, instruction.opcode).empty();
            auto operands = decodeGoolOperands(source.version, instruction.opcode, instruction.operandBits);
            resolveGoolOperands(operands, source);
            ir.operands.reserve(operands.size());
            for (const auto& operand : operands) {
                ir.operands.push_back({operand.name, operand.bitWidth, operand.rawValue,
                    operand.value, convertOperandKind(operand.kind), operand.resolvedText});
            }
        }
        result.instructions.push_back(std::move(ir));
    }

    const auto controlFlow = analyzeGoolControlFlow(source);
    result.basicBlocks.reserve(controlFlow.blocks.size());
    for (const auto& block : controlFlow.blocks)
        result.basicBlocks.push_back({block.startInstruction, block.endInstruction});
    result.flowEdges.reserve(controlFlow.edges.size());
    for (const auto& edge : controlFlow.edges) {
        result.flowEdges.push_back({edge.sourceBlock, edge.targetInstruction,
            static_cast<GoolIRFlowEdgeKind>(edge.kind), edge.targetValid, edge.diagnostic});
    }
    result.controlFlowDiagnostics = controlFlow.diagnostics;
    result.instructionToBlock.assign(source.instructions.size(), 0U);
    for (std::size_t blockIndex = 0; blockIndex < controlFlow.blocks.size(); ++blockIndex) {
        const auto& block = controlFlow.blocks[blockIndex];
        for (std::size_t instructionIndex = block.startInstruction;
             instructionIndex < block.endInstruction && instructionIndex < result.instructionToBlock.size();
             ++instructionIndex) {
            result.instructionToBlock[instructionIndex] = blockIndex;
        }
    }

    result.stateDescriptors.reserve(source.stateDescriptors.size());
    for (const auto& descriptor : source.stateDescriptors) {
        result.stateDescriptors.push_back({descriptor.stateFlags, descriptor.blockFlags,
            descriptor.goolIndex, descriptor.eventHook, descriptor.transHook, descriptor.codeHook});
    }
    result.stateReferences.reserve(source.stateReferences.size());
    for (const auto& reference : source.stateReferences) {
        result.stateReferences.push_back({reference.descriptorIndex, reference.dataPoolIndex,
            reference.eid, reference.targetFound, static_cast<std::uint8_t>(reference.status),
            reference.targetChunkIndex, reference.targetEntryIndex});
    }
    result.stateHooks.reserve(source.stateHookReferences.size());
    for (const auto& hook : source.stateHookReferences) {
        result.stateHooks.push_back({hook.descriptorIndex, static_cast<std::uint8_t>(hook.kind),
            static_cast<std::uint8_t>(hook.status), hook.rawValue, hook.instructionOffset,
            hook.external, hook.targetFound, hook.targetChunkIndex, hook.targetEntryIndex});
    }
    result.frameGroups.reserve(source.frameGroups.size());
    for (const auto& group : source.frameGroups) {
        result.frameGroups.push_back({group.type, group.frameCount, group.eid,
            group.byteOffset, group.byteSize, group.partCount, group.font, group.interpolated});
    }
    return result;
}

}
