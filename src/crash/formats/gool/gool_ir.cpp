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
        if (instruction.isMips) {
            const std::uint32_t word = instruction.word;
            auto& fields = ir.nativeFields;
            fields.primaryOpcode = static_cast<std::uint8_t>((word >> 26U) & 0x3FU);
            fields.rs = static_cast<std::uint8_t>((word >> 21U) & 0x1FU);
            fields.rt = static_cast<std::uint8_t>((word >> 16U) & 0x1FU);
            fields.rd = static_cast<std::uint8_t>((word >> 11U) & 0x1FU);
            fields.shiftAmount = static_cast<std::uint8_t>((word >> 6U) & 0x1FU);
            fields.function = static_cast<std::uint8_t>(word & 0x3FU);
            fields.immediate = static_cast<std::uint16_t>(word & 0xFFFFU);
            fields.signedImmediate = static_cast<std::int16_t>(fields.immediate);
            fields.jumpTarget = word & 0x03FFFFFFU;
            if (fields.primaryOpcode == 0U) fields.format = GoolIRMipsFormat::Register;
            else if (fields.primaryOpcode == 2U || fields.primaryOpcode == 3U)
                fields.format = GoolIRMipsFormat::Jump;
            else fields.format = GoolIRMipsFormat::Immediate;
            ir.nativeFieldsDecoded = true;
        }
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

    // Preserve higher-level control operations separately from CFG edges. These
    // records retain their originating instruction and only resolve targets when
    // the decoded data provides an unambiguous, bounds-checked destination.
    for (std::size_t index = 0; index < source.instructions.size(); ++index) {
        const auto& instruction = source.instructions[index];
        if (instruction.isMips) continue;
        const auto opcode = instruction.opcode;
        if (opcode != 49U && opcode != 53U && opcode != 54U &&
            opcode != 55U && opcode != 59U) continue;

        const auto operands = decodeGoolOperands(source.version, opcode, instruction.operandBits);
        const auto findOperand = [&operands](char name) -> const NsfGoolOperand* {
            for (const auto& operand : operands)
                if (operand.name == name) return &operand;
            return nullptr;
        };
        GoolIRControlOperation operation;
        operation.sourceInstruction = index;
        operation.sourceByteOffset = instruction.byteOffset;

        if (opcode == 49U) {
            operation.kind = GoolIRControlOperationKind::Return;
            operation.diagnostic = "Return destination is determined by the runtime call context.";
        } else if (opcode == 59U) {
            operation.kind = GoolIRControlOperationKind::Call;
            const auto* target = findOperand('I');
            const auto* external = findOperand('E');
            const auto* arguments = findOperand('V');
            operation.external = external && external->rawValue != 0U;
            operation.argumentCount = arguments ? arguments->rawValue : 0U;
            if (operation.external) {
                operation.diagnostic = "External call target is outside this GOOL program.";
            } else if (target && target->value >= 0 &&
                       static_cast<std::uint64_t>(target->value) < source.instructions.size()) {
                operation.targetInstruction = static_cast<std::size_t>(target->value);
                operation.targetInstructionValid = true;
            } else {
                operation.diagnostic = "Local call target is outside this GOOL program.";
            }
        } else {
            operation.kind = GoolIRControlOperationKind::StateTransition;
            operation.conditional = opcode == 54U || opcode == 55U;
            if (operation.conditional) {
                const auto* condition = findOperand('R');
                if (condition) operation.conditionField = goolObjectFieldName(condition->rawValue);
            }
            const auto* state = findOperand('S');
            const auto* arguments = findOperand('V');
            operation.argumentCount = arguments ? arguments->rawValue : 0U;
            if (state) {
                operation.targetDescriptorIndex = state->rawValue;
                operation.targetDescriptorValid =
                    operation.targetDescriptorIndex < source.stateDescriptors.size();
            }
            if (operation.targetDescriptorValid) {
                const auto dataIndex = source.stateDescriptors[operation.targetDescriptorIndex].goolIndex;
                if (dataIndex >= 0 && static_cast<std::size_t>(dataIndex) < source.dataPool.size()) {
                    operation.targetGoolEid =
                        static_cast<std::uint32_t>(source.dataPool[static_cast<std::size_t>(dataIndex)]);
                    operation.targetGoolEidValid = true;
                } else {
                    operation.diagnostic = "State descriptor references an invalid GOOL data-pool index.";
                }
                for (const auto& reference : source.stateReferences) {
                    if (reference.descriptorIndex != operation.targetDescriptorIndex) continue;
                    operation.hasTargetReferenceStatus = true;
                    operation.targetReferenceStatus = static_cast<std::uint8_t>(reference.status);
                    break;
                }
            } else {
                operation.diagnostic = "State transition references a descriptor outside this GOOL object.";
            }
        }
        result.controlOperations.push_back(std::move(operation));
    }
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
