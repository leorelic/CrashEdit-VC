#ifdef NDEBUG
#undef NDEBUG
#endif

#include "crash/formats/gool/gool_ir.hpp"
#include "crash/formats/gool/gool_decompiler.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

using namespace CrashEdit::Crash;

int main() {
    NsfGool source;
    source.version = NsfGoolVersion::Version2;
    source.id = 17;
    source.classId = 23;
    source.format = 2;
    source.heapBase = 0x120;
    source.eventCount = 4;
    source.entryCount = 1;
    source.dataPool = {0x0200007B, -7};
    source.stateMap = {0, 1};

    NsfGoolInstruction known;
    known.byteOffset = 0x40;
    known.word = 0x00123456;
    known.opcode = 0;
    known.operandBits = 0x123456;
    known.mnemonic = "ADD";
    source.instructions.push_back(known);

    NsfGoolInstruction native;
    native.byteOffset = 0x44;
    native.word = 0x012A4021;
    native.isMips = true;
    native.mipsKnown = true;
    native.mnemonic = "addu";
    native.arguments = "$t0, $t1, $t2";
    source.instructions.push_back(native);

    NsfGoolInstruction unknown;
    unknown.byteOffset = 0x48;
    unknown.word = 0xFA123456;
    unknown.opcode = 250;
    unknown.operandBits = 0x123456;
    unknown.mnemonic = "UNK";
    source.instructions.push_back(unknown);

    source.stateDescriptors.push_back({1, 2, 3, 4, 5, 6});
    source.stateReferences.push_back({0, 2, 3, 1, 0x0200007B, true, NsfGoolStateReferenceStatus::External});
    source.stateHookReferences.push_back({0, 1, 2, NsfGoolStateHookKind::Event,
        0x5234, 0x1234, true, true, NsfGoolStateHookStatus::External});
    source.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Transition,
        0x0007, 7, false, true, NsfGoolStateHookStatus::Local});
    source.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0x3FFF, 0x3FFF, false, false, NsfGoolStateHookStatus::Absent});
    source.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Event,
        0x4009, 9, true, true, NsfGoolStateHookStatus::OffsetOutOfRange});
    source.frameGroupBytes = 12;
    source.frameGroupsDecoded = true;
    source.frameGroups.push_back({3, 8, 0x0200007B, 0x60, 0x18, 2, 9, true});

    const auto ir = buildGoolIR(source);
    assert(ir.targetVersion == static_cast<std::uint8_t>(NsfGoolVersion::Version2));
    assert(ir.id == 17 && ir.classId == 23 && ir.format == 2);
    assert(ir.heapBase == 0x120 && ir.eventCount == 4 && ir.entryCount == 1);
    assert(ir.dataPool == source.dataPool && ir.stateMap == source.stateMap);
    assert(ir.instructions.size() == 3);

    const auto& knownIR = ir.instructions[0];
    assert(knownIR.sourceByteOffset == 0x40 && knownIR.rawWord == known.word);
    assert(knownIR.rawOperandBits == known.operandBits && knownIR.opcode == 0);
    assert(!knownIR.nativeMips && knownIR.opcodeKnown && !knownIR.operands.empty());

    const auto& nativeIR = ir.instructions[1];
    assert(nativeIR.sourceByteOffset == 0x44 && nativeIR.rawWord == native.word);
    assert(nativeIR.nativeMips && nativeIR.nativeInstructionDecoded);
    assert(nativeIR.mnemonic == "addu" && nativeIR.nativeArguments == "$t0, $t1, $t2");
    assert(nativeIR.operands.empty());

    const auto& unknownIR = ir.instructions[2];
    assert(unknownIR.sourceByteOffset == 0x48 && unknownIR.rawWord == unknown.word);
    assert(!unknownIR.nativeMips && !unknownIR.opcodeKnown);
    assert(unknownIR.rawOperandBits == unknown.operandBits);

    assert(ir.stateDescriptors.size() == 1);
    assert(ir.stateDescriptors[0].stateFlags == 1 && ir.stateDescriptors[0].blockFlags == 2);
    assert(ir.stateDescriptors[0].goolIndex == 3 && ir.stateDescriptors[0].eventHook == 4);
    assert(ir.stateDescriptors[0].transitionHook == 5 && ir.stateDescriptors[0].codeHook == 6);
    assert(ir.stateReferences.size() == 1 && ir.stateReferences[0].eid == 0x0200007B);
    assert(ir.stateReferences[0].targetFound && ir.stateReferences[0].targetChunkIndex == 2);
    assert(ir.stateHooks.size() == 4 && ir.stateHooks[0].rawValue == 0x5234);
    assert(ir.stateHooks[0].external && ir.stateHooks[0].targetFound);
    assert(ir.hookDispatches.size() == 4);
    assert(ir.hookDispatches[0].kind == GoolIRHookKind::Event);
    assert(ir.hookDispatches[0].resolution == GoolIRHookResolution::External);
    assert(ir.hookDispatches[0].instructionOffset == 0x1234);
    assert(ir.hookDispatches[0].targetChunkIndex == 1 && ir.hookDispatches[0].targetEntryIndex == 2);
    assert(ir.hookDispatches[1].kind == GoolIRHookKind::Transition);
    assert(ir.hookDispatches[1].resolution == GoolIRHookResolution::Local);
    assert(ir.hookDispatches[2].kind == GoolIRHookKind::Code);
    assert(ir.hookDispatches[2].resolution == GoolIRHookResolution::Absent);
    assert(ir.hookDispatches[3].resolution == GoolIRHookResolution::OffsetOutOfRange);
    assert(ir.routineEntries.empty() && ir.routines.empty());
    assert(ir.frameGroupBytes == 12 && ir.frameGroupsDecoded && ir.frameGroups.size() == 1);
    assert(ir.frameGroups[0].sourceByteOffset == 0x60 && ir.frameGroups[0].interpolated);

    NsfGool flowSource;
    flowSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction branch;
    branch.opcode = 50; // BRA with zero displacement targets the next instruction.
    branch.operandBits = 0;
    flowSource.instructions.push_back(branch);
    NsfGoolInstruction sequential;
    sequential.opcode = 0;
    flowSource.instructions.push_back(sequential);
    NsfGoolInstruction ret;
    ret.opcode = 49;
    flowSource.instructions.push_back(ret);

    const auto flowIR = buildGoolIR(flowSource);
    assert(flowIR.basicBlocks.size() == 2);
    assert(flowIR.instructionToBlock.size() == 3);
    assert(flowIR.instructionToBlock[0] == 0 && flowIR.instructionToBlock[1] == 1);
    assert(flowIR.instructionToBlock[2] == 1);
    assert(!flowIR.flowEdges.empty());
    assert(flowIR.flowEdges[0].kind == GoolIRFlowEdgeKind::Branch);
    assert(flowIR.flowEdges[0].targetValid && flowIR.flowEdges[0].targetInstruction == 1);
    bool hasDynamicReturn = false;
    for (const auto& edge : flowIR.flowEdges)
        if (edge.kind == GoolIRFlowEdgeKind::Return && !edge.targetValid)
            hasDynamicReturn = true;
    assert(hasDynamicReturn);

    // Calls, returns, and state transitions have explicit source-mapped IR records.
    NsfGool semanticSource;
    semanticSource.version = NsfGoolVersion::Version2;
    semanticSource.dataPool = {static_cast<std::int32_t>(0x0200007BU)};
    semanticSource.stateDescriptors.push_back({0, 0, 0, 0, 0, 0});
    semanticSource.stateReferences.push_back({0, 0, 0, 0, 0x0200007B, true,
        NsfGoolStateReferenceStatus::Local});
    NsfGoolInstruction localCall;
    localCall.opcode = 59;
    localCall.byteOffset = 0x80;
    localCall.operandBits = 2U; // I = instruction 2; E = local; V = 0
    semanticSource.instructions.push_back(localCall);
    NsfGoolInstruction go;
    go.opcode = 53;
    go.byteOffset = 0x84;
    go.operandBits = 0U; // S = descriptor 0; V = 0
    semanticSource.instructions.push_back(go);
    NsfGoolInstruction retSemantic;
    retSemantic.opcode = 49;
    retSemantic.byteOffset = 0x88;
    semanticSource.instructions.push_back(retSemantic);
    const auto semanticIR = buildGoolIR(semanticSource);
    assert(semanticIR.controlOperations.size() == 3);
    assert(semanticIR.controlOperations[0].kind == GoolIRControlOperationKind::Call);
    assert(semanticIR.controlOperations[0].sourceByteOffset == 0x80);
    assert(semanticIR.controlOperations[0].targetInstructionValid);
    assert(semanticIR.controlOperations[0].targetInstruction == 2);
    assert(semanticIR.routineEntries.size() == 1);
    assert(semanticIR.routineEntries[0].instruction == 2);
    assert(semanticIR.routineEntries[0].fromLocalCall);
    assert(semanticIR.routines.size() == 1);
    assert(semanticIR.routines[0].entryInstruction == 2);
    assert((semanticIR.routines[0].instructions == std::vector<std::size_t>{2}));
    assert(!semanticIR.controlOperations[0].external);
    assert(semanticIR.controlOperations[1].kind == GoolIRControlOperationKind::StateTransition);
    assert(semanticIR.controlOperations[1].targetDescriptorValid);
    assert(semanticIR.controlOperations[1].targetGoolEidValid);
    assert(semanticIR.controlOperations[1].targetGoolEid == 0x0200007B);
    assert(semanticIR.controlOperations[1].hasTargetReferenceStatus);
    assert(semanticIR.controlOperations[2].kind == GoolIRControlOperationKind::Return);
    assert(!semanticIR.controlOperations[2].targetInstructionValid);

    NsfGool mipsSource;
    mipsSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction jalr;
    jalr.isMips = true;
    jalr.word = 0x02A0F809U; // jalr $ra, $s5
    jalr.byteOffset = 0x44;
    jalr.mnemonic = "jalr";
    jalr.arguments = "$ra,$s5";
    jalr.mipsKnown = true;
    mipsSource.instructions.push_back(jalr);
    const auto mipsIR = buildGoolIR(mipsSource);
    assert(mipsIR.instructions.size() == 1);
    const auto& jalrIR = mipsIR.instructions[0];
    assert(jalrIR.nativeMips && jalrIR.nativeFieldsDecoded);
    assert(jalrIR.nativeFields.format == GoolIRMipsFormat::Register);
    assert(jalrIR.nativeFields.primaryOpcode == 0);
    assert(jalrIR.nativeFields.rs == 21);
    assert(jalrIR.nativeFields.rd == 31);
    assert(jalrIR.nativeFields.function == 9);
    assert(jalrIR.nativeEffects.known);
    assert((jalrIR.nativeEffects.gprReadMask & (std::uint32_t{1} << 21U)) != 0U);
    assert((jalrIR.nativeEffects.gprWriteMask & (std::uint32_t{1} << 31U)) != 0U);
    assert(jalrIR.nativeEffects.controlTransfer);
    assert(jalrIR.nativeEffects.controlTransferHasDelaySlot);
    assert(jalrIR.rawWord == 0x02A0F809U);
    assert(jalrIR.sourceByteOffset == 0x44);

    // Preserve the exact R3000A control-transfer form, not just a generic
    // branch/jump flag, for later source reconstruction and CFG integration.
    NsfGool mipsControlSource;
    mipsControlSource.version = NsfGoolVersion::Version2;
    const auto addMipsControlWord = [&mipsControlSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = mipsControlSource.instructions.size() * 4U;
        mipsControlSource.instructions.push_back(instruction);
    };
    addMipsControlWord((8U << 21U) | 8U); // jr t0
    addMipsControlWord((8U << 21U) | (31U << 11U) | 9U); // jalr ra,t0
    addMipsControlWord((2U << 26U) | 0x12345U); // j
    addMipsControlWord((3U << 26U) | 0x12345U); // jal
    addMipsControlWord((4U << 26U) | (8U << 21U) | (9U << 16U)); // beq
    addMipsControlWord((5U << 26U) | (8U << 21U) | (9U << 16U)); // bne
    addMipsControlWord((6U << 26U) | (8U << 21U)); // blez
    addMipsControlWord((7U << 26U) | (8U << 21U)); // bgtz
    addMipsControlWord((1U << 26U) | (8U << 21U) | (0U << 16U)); // bltz
    addMipsControlWord((1U << 26U) | (8U << 21U) | (1U << 16U)); // bgez
    addMipsControlWord((1U << 26U) | (8U << 21U) | (16U << 16U)); // bltzal
    addMipsControlWord((1U << 26U) | (8U << 21U) | (17U << 16U)); // bgezal
    addMipsControlWord((16U << 26U) | (8U << 21U)); // bc0f
    addMipsControlWord((16U << 26U) | (8U << 21U) | (1U << 16U)); // bc0t
    addMipsControlWord((18U << 26U) | (8U << 21U)); // bc2f
    addMipsControlWord((18U << 26U) | (8U << 21U) | (1U << 16U)); // bc2t
    const auto mipsControlIR = buildGoolIR(mipsControlSource);
    const GoolIRMipsControlOperation expectedMipsControl[] = {
        GoolIRMipsControlOperation::JumpRegister,
        GoolIRMipsControlOperation::JumpAndLinkRegister,
        GoolIRMipsControlOperation::Jump,
        GoolIRMipsControlOperation::JumpAndLink,
        GoolIRMipsControlOperation::BranchEqual,
        GoolIRMipsControlOperation::BranchNotEqual,
        GoolIRMipsControlOperation::BranchLessOrEqualZero,
        GoolIRMipsControlOperation::BranchGreaterThanZero,
        GoolIRMipsControlOperation::BranchLessThanZero,
        GoolIRMipsControlOperation::BranchGreaterEqualZero,
        GoolIRMipsControlOperation::BranchLessThanZeroAndLink,
        GoolIRMipsControlOperation::BranchGreaterEqualZeroAndLink,
        GoolIRMipsControlOperation::BranchCoprocessor0False,
        GoolIRMipsControlOperation::BranchCoprocessor0True,
        GoolIRMipsControlOperation::BranchCoprocessor2False,
        GoolIRMipsControlOperation::BranchCoprocessor2True
    };
    assert(mipsControlIR.instructions.size() == std::size(expectedMipsControl));
    for (std::size_t i = 0; i < std::size(expectedMipsControl); ++i) {
        const auto& effects = mipsControlIR.instructions[i].nativeEffects;
        assert(effects.known && effects.controlTransfer && effects.controlTransferHasDelaySlot);
        assert(effects.controlOperation == expectedMipsControl[i]);
    }

    // Resolve PC-relative native branch targets using the instruction's
    // PC+4 base, and only bind an instruction index for a unique stream offset.
    NsfGool nativeBranchTargets;
    nativeBranchTargets.version = NsfGoolVersion::Version2;
    const auto addBranchAt = [&nativeBranchTargets](std::size_t offset, std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = offset;
        nativeBranchTargets.instructions.push_back(instruction);
    };
    addBranchAt(0x100U, (4U << 26U) | (8U << 21U) | (9U << 16U) | 2U); // beq -> 0x10C
    addBranchAt(0x104U, 0U); // delay slot
    addBranchAt(0x108U, 0U); // fallthrough
    addBranchAt(0x10CU, 0U); // target
    addBranchAt(0x110U, (5U << 26U) | (8U << 21U) | (9U << 16U) | 0xFFFCU); // bne -> 0x104
    addBranchAt(0x114U, 0U); // delay slot
    addBranchAt(0x118U, (4U << 26U) | (8U << 21U) | (9U << 16U) | 0x7FFFU); // valid offset, absent target
    addBranchAt(0x11CU, 0U);
    const auto nativeBranchTargetIR = buildGoolIR(nativeBranchTargets);
    assert(nativeBranchTargetIR.instructions.size() == 8);
    assert(nativeBranchTargetIR.instructions[0].nativeBranchTargetByteOffsetValid);
    assert(nativeBranchTargetIR.instructions[0].nativeBranchTargetByteOffset == 0x10CU);
    assert(nativeBranchTargetIR.instructions[0].nativeBranchTargetInstructionValid);
    assert(nativeBranchTargetIR.instructions[0].nativeBranchTargetInstruction == 3U);
    assert(nativeBranchTargetIR.instructions[4].nativeBranchTargetByteOffsetValid);
    assert(nativeBranchTargetIR.instructions[4].nativeBranchTargetByteOffset == 0x104U);
    assert(nativeBranchTargetIR.instructions[4].nativeBranchTargetInstructionValid);
    assert(nativeBranchTargetIR.instructions[4].nativeBranchTargetInstruction == 1U);
    assert(nativeBranchTargetIR.instructions[6].nativeBranchTargetByteOffsetValid);
    assert(!nativeBranchTargetIR.instructions[6].nativeBranchTargetInstructionValid);
    assert(!nativeBranchTargetIR.instructions[1].nativeBranchTargetByteOffsetValid);

    // J/JAL preserve their pseudo-direct low 28 target bits without inventing
    // the runtime PC's upper nibble from a GOOL-relative byte offset.
    NsfGool nativeJumpTargets;
    nativeJumpTargets.version = NsfGoolVersion::Version2;
    NsfGoolInstruction directJump;
    directJump.isMips = true;
    directJump.word = (2U << 26U) | 0x0123456U;
    directJump.byteOffset = 0x200U;
    nativeJumpTargets.instructions.push_back(directJump);
    NsfGoolInstruction directLink;
    directLink.isMips = true;
    directLink.word = (3U << 26U) | 0x03FFFFFFU;
    directLink.byteOffset = 0x204U;
    nativeJumpTargets.instructions.push_back(directLink);
    NsfGoolInstruction ordinaryInstruction;
    ordinaryInstruction.isMips = true;
    ordinaryInstruction.word = 0U;
    ordinaryInstruction.byteOffset = 0x208U;
    nativeJumpTargets.instructions.push_back(ordinaryInstruction);
    const auto nativeJumpTargetIR = buildGoolIR(nativeJumpTargets);
    assert(nativeJumpTargetIR.instructions[0].nativeJumpTargetLow28Valid);
    assert(nativeJumpTargetIR.instructions[0].nativeJumpTargetLow28 == 0x048D158U);
    assert(nativeJumpTargetIR.instructions[1].nativeJumpTargetLow28Valid);
    assert(nativeJumpTargetIR.instructions[1].nativeJumpTargetLow28 == 0x0FFFFFFCU);
    assert(!nativeJumpTargetIR.instructions[2].nativeJumpTargetLow28Valid);

    // Negative target arithmetic and duplicate instruction offsets must never
    // produce a falsely resolved instruction index.
    NsfGool invalidNativeBranchTargets;
    invalidNativeBranchTargets.version = NsfGoolVersion::Version2;
    NsfGoolInstruction underflowBranch;
    underflowBranch.isMips = true;
    underflowBranch.word = (4U << 26U) | (8U << 21U) | (9U << 16U) | 0xFFFEU;
    underflowBranch.byteOffset = 0U;
    invalidNativeBranchTargets.instructions.push_back(underflowBranch);
    NsfGoolInstruction duplicateA;
    duplicateA.isMips = true; duplicateA.word = 0U; duplicateA.byteOffset = 0x20U;
    invalidNativeBranchTargets.instructions.push_back(duplicateA);
    NsfGoolInstruction duplicateBranch;
    duplicateBranch.isMips = true;
    duplicateBranch.word = (4U << 26U) | (8U << 21U) | (9U << 16U) | 6U;
    duplicateBranch.byteOffset = 0x24U; // target 0x40
    invalidNativeBranchTargets.instructions.push_back(duplicateBranch);
    NsfGoolInstruction duplicateB;
    duplicateB.isMips = true; duplicateB.word = 0U; duplicateB.byteOffset = 0x40U;
    invalidNativeBranchTargets.instructions.push_back(duplicateB);
    NsfGoolInstruction duplicateC = duplicateB;
    invalidNativeBranchTargets.instructions.push_back(duplicateC);
    const auto invalidNativeBranchIR = buildGoolIR(invalidNativeBranchTargets);
    assert(!invalidNativeBranchIR.instructions[0].nativeBranchTargetByteOffsetValid);
    assert(invalidNativeBranchIR.instructions[2].nativeBranchTargetByteOffsetValid);
    assert(invalidNativeBranchIR.instructions[2].nativeBranchTargetByteOffset == 0x40U);
    assert(!invalidNativeBranchIR.instructions[2].nativeBranchTargetInstructionValid);

    // Conservative R3000A register and memory effects are tracked independently
    // from mnemonic text, with unsupported encodings left explicitly unknown.
    NsfGool effectSource;
    effectSource.version = NsfGoolVersion::Version2;
    const auto addMipsWord = [&effectSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = effectSource.instructions.size() * 4U;
        effectSource.instructions.push_back(instruction);
    };
    addMipsWord((9U << 26U) | (29U << 21U) | (8U << 16U) | 0xFFF0U); // addiu t0,sp,-16
    addMipsWord((35U << 26U) | (8U << 21U) | (9U << 16U) | 4U); // lw t1,4(t0)
    addMipsWord((43U << 26U) | (8U << 21U) | (9U << 16U) | 8U); // sw t1,8(t0)
    addMipsWord((8U << 21U) | (9U << 16U) | 24U); // mult t0,t1
    addMipsWord((4U << 26U) | (8U << 21U) | (9U << 16U) | 2U); // beq t0,t1,...
    const auto effectIR = buildGoolIR(effectSource);
    assert(effectIR.instructions.size() == 5);
    const auto& addiuEffects = effectIR.instructions[0].nativeEffects;
    assert(addiuEffects.known && addiuEffects.memory == GoolIRMipsMemoryEffect::None);
    assert((addiuEffects.gprReadMask & (std::uint32_t{1} << 29U)) != 0U);
    assert((addiuEffects.gprWriteMask & (std::uint32_t{1} << 8U)) != 0U);
    const auto& loadEffects = effectIR.instructions[1].nativeEffects;
    assert(loadEffects.known && loadEffects.memory == GoolIRMipsMemoryEffect::Read);
    assert((loadEffects.gprReadMask & (std::uint32_t{1} << 8U)) != 0U);
    assert((loadEffects.gprWriteMask & (std::uint32_t{1} << 9U)) != 0U);
    assert(effectIR.instructions[1].gprReadInLoadDelaySlotMask == 0U);
    assert(effectIR.instructions[2].gprReadInLoadDelaySlotMask == (std::uint32_t{1} << 9U));
    const auto& storeEffects = effectIR.instructions[2].nativeEffects;
    assert(storeEffects.known && storeEffects.memory == GoolIRMipsMemoryEffect::Write);
    assert((storeEffects.gprReadMask & (std::uint32_t{1} << 9U)) != 0U);
    const auto& multiplyEffects = effectIR.instructions[3].nativeEffects;
    assert(multiplyEffects.known && multiplyEffects.writesHi && multiplyEffects.writesLo);
    assert(!multiplyEffects.controlTransferHasDelaySlot);
    const auto& branchEffects = effectIR.instructions[4].nativeEffects;
    assert(branchEffects.known && branchEffects.controlTransfer);
    assert(branchEffects.controlTransferHasDelaySlot);
    assert((branchEffects.gprReadMask & (std::uint32_t{1} << 8U)) != 0U);
    assert((branchEffects.gprReadMask & (std::uint32_t{1} << 9U)) != 0U);
    assert(!effectIR.instructions[0].isControlTransferDelaySlot);
    assert(!effectIR.instructions[4].isControlTransferDelaySlot);

    // Keep the precise HI/LO operation distinct from its coarse dependency
    // flags so later source reconstruction can distinguish signed arithmetic.
    NsfGool hiLoSource;
    hiLoSource.version = NsfGoolVersion::Version2;
    const auto addHiLoWord = [&hiLoSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = hiLoSource.instructions.size() * 4U;
        hiLoSource.instructions.push_back(instruction);
    };
    addHiLoWord((8U << 21U) | (9U << 16U) | 24U); // mult
    addHiLoWord((8U << 21U) | (9U << 16U) | 25U); // multu
    addHiLoWord((8U << 21U) | (9U << 16U) | 26U); // div
    addHiLoWord((8U << 21U) | (9U << 16U) | 27U); // divu
    addHiLoWord((16U << 11U) | 16U); // mfhi t0
    addHiLoWord((8U << 21U) | 17U); // mthi t0
    addHiLoWord((18U << 11U) | 18U); // mflo t0
    addHiLoWord((8U << 21U) | 19U); // mtlo t0
    const auto hiLoIR = buildGoolIR(hiLoSource);
    assert(hiLoIR.instructions.size() == 8);
    const auto& hiLoInstructions = hiLoIR.instructions;
    assert(hiLoInstructions[0].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::MultiplySigned);
    assert(hiLoInstructions[1].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::MultiplyUnsigned);
    assert(hiLoInstructions[2].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::DivideSigned);
    assert(hiLoInstructions[3].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::DivideUnsigned);
    assert(hiLoInstructions[4].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::MoveFromHi);
    assert(hiLoInstructions[5].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::MoveToHi);
    assert(hiLoInstructions[6].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::MoveFromLo);
    assert(hiLoInstructions[7].nativeEffects.hiLoOperation == GoolIRMipsHiLoOperation::MoveToLo);
    assert(hiLoInstructions[0].nativeEffects.writesHi && hiLoInstructions[0].nativeEffects.writesLo);
    assert(hiLoInstructions[2].nativeEffects.writesHi && hiLoInstructions[2].nativeEffects.writesLo);
    assert(hiLoInstructions[4].nativeEffects.readsHi && !hiLoInstructions[4].nativeEffects.writesHi);
    assert(hiLoInstructions[5].nativeEffects.writesHi && !hiLoInstructions[5].nativeEffects.readsHi);
    assert(hiLoInstructions[6].nativeEffects.readsLo && !hiLoInstructions[6].nativeEffects.writesLo);
    assert(hiLoInstructions[7].nativeEffects.writesLo && !hiLoInstructions[7].nativeEffects.readsLo);

    // LWC2/SWC2 impose a two-instruction access delay on the addressed GTE
    // data register. Track the overlap separately from GPR load delays.
    NsfGool gteTransferDelaySource;
    gteTransferDelaySource.version = NsfGoolVersion::Version2;
    const auto addGteDelayWord = [&gteTransferDelaySource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = gteTransferDelaySource.instructions.size() * 4U;
        gteTransferDelaySource.instructions.push_back(instruction);
    };
    addGteDelayWord((50U << 26U) | (8U << 21U)); // lwc2 $0,0(t0)
    addGteDelayWord((18U << 26U) | (8U << 16U)); // mfc2 t0,$0
    addGteDelayWord((18U << 26U) | (16U << 21U) | 0x01U); // rtps reads V0
    addGteDelayWord((18U << 26U) | (8U << 16U)); // mfc2 t0,$0, outside delay window
    addGteDelayWord((58U << 26U) | (8U << 21U)); // swc2 $0,0(t0)
    addGteDelayWord((18U << 26U) | (16U << 21U) | 0x01U); // rtps reads V0
    addGteDelayWord((18U << 26U) | (8U << 16U)); // mfc2 t0,$0, second delay slot
    addGteDelayWord((18U << 26U) | (8U << 16U)); // mfc2 t0,$0, outside delay window
    const auto gteTransferDelayIR = buildGoolIR(gteTransferDelaySource);
    assert(gteTransferDelayIR.instructions.size() == 8);
    const auto gteRegisterZeroMask = std::uint32_t{1};
    assert(gteTransferDelayIR.instructions[0].gteDataAccessInTransferDelayMask == 0U);
    assert(gteTransferDelayIR.instructions[1].gteDataAccessInTransferDelayMask == gteRegisterZeroMask);
    assert(gteTransferDelayIR.instructions[2].gteDataAccessInTransferDelayMask == gteRegisterZeroMask);
    assert(gteTransferDelayIR.instructions[3].gteDataAccessInTransferDelayMask == 0U);
    assert(gteTransferDelayIR.instructions[5].gteDataAccessInTransferDelayMask == gteRegisterZeroMask);
    assert(gteTransferDelayIR.instructions[6].gteDataAccessInTransferDelayMask == gteRegisterZeroMask);
    assert(gteTransferDelayIR.instructions[7].gteDataAccessInTransferDelayMask == 0U);

    // R3000A permits an immediately adjacent complementary LWL/LWR pair to
    // merge through the ordinary load-delay slot; the second load still has
    // its own delay before a subsequent consumer.
    NsfGool unalignedPairSource;
    unalignedPairSource.version = NsfGoolVersion::Version2;
    const auto addUnalignedLoad = [&unalignedPairSource](std::uint8_t opcode, std::uint8_t rt) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = (static_cast<std::uint32_t>(opcode) << 26U) |
            (8U << 21U) | (static_cast<std::uint32_t>(rt) << 16U);
        instruction.byteOffset = unalignedPairSource.instructions.size() * 4U;
        unalignedPairSource.instructions.push_back(instruction);
    };
    const auto addUnalignedConsumer = [&unalignedPairSource](std::uint8_t source, std::uint8_t destination) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = (9U << 26U) | (static_cast<std::uint32_t>(source) << 21U) |
            (static_cast<std::uint32_t>(destination) << 16U);
        instruction.byteOffset = unalignedPairSource.instructions.size() * 4U;
        unalignedPairSource.instructions.push_back(instruction);
    };
    addUnalignedLoad(34U, 9U); // lwl t1,0(t0)
    addUnalignedLoad(38U, 9U); // lwr t1,0(t0)
    addUnalignedConsumer(9U, 10U); // addiu t2,t1,0 -- still in the second load's delay slot
    addUnalignedLoad(38U, 11U); // lwr t3,0(t0)
    addUnalignedLoad(34U, 11U); // lwl t3,0(t0)
    addUnalignedConsumer(11U, 12U); // addiu t4,t3,0 -- still in the second load's delay slot
    const auto unalignedPairIR = buildGoolIR(unalignedPairSource);
    assert(unalignedPairIR.instructions.size() == 6);
    const auto t1Mask = std::uint32_t{1} << 9U;
    const auto t3Mask = std::uint32_t{1} << 11U;
    assert(unalignedPairIR.instructions[1].gprLoadDelayPairMergeMask == t1Mask);
    assert(unalignedPairIR.instructions[1].gprReadInLoadDelaySlotMask == 0U);
    assert(unalignedPairIR.instructions[2].gprReadInLoadDelaySlotMask == t1Mask);
    assert(unalignedPairIR.instructions[4].gprLoadDelayPairMergeMask == t3Mask);
    assert(unalignedPairIR.instructions[4].gprReadInLoadDelaySlotMask == 0U);
    assert(unalignedPairIR.instructions[5].gprReadInLoadDelaySlotMask == t3Mask);

    // Preserve the encoded effective-address recipe and transfer width for
    // scalar R3000A loads/stores and PlayStation GTE memory transfers.
    NsfGool memoryAccessSource;
    memoryAccessSource.version = NsfGoolVersion::Version2;
    const auto addMemoryWord = [&memoryAccessSource](std::uint8_t opcode, std::uint8_t rt,
            std::uint16_t offset) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = (static_cast<std::uint32_t>(opcode) << 26U) |
            (8U << 21U) | (static_cast<std::uint32_t>(rt) << 16U) | offset;
        instruction.byteOffset = memoryAccessSource.instructions.size() * 4U;
        memoryAccessSource.instructions.push_back(instruction);
    };
    addMemoryWord(32U, 9U, 0xFFF0U); // lb t1,-16(t0)
    addMemoryWord(33U, 9U, 2U);      // lh t1,2(t0)
    addMemoryWord(34U, 9U, 3U);      // lwl t1,3(t0)
    addMemoryWord(35U, 9U, 4U);      // lw t1,4(t0)
    addMemoryWord(36U, 9U, 5U);      // lbu t1,5(t0)
    addMemoryWord(37U, 9U, 6U);      // lhu t1,6(t0)
    addMemoryWord(38U, 9U, 7U);      // lwr t1,7(t0)
    addMemoryWord(40U, 9U, 8U);      // sb t1,8(t0)
    addMemoryWord(41U, 9U, 10U);    // sh t1,10(t0)
    addMemoryWord(42U, 9U, 11U);    // swl t1,11(t0)
    addMemoryWord(43U, 9U, 12U);    // sw t1,12(t0)
    addMemoryWord(46U, 9U, 15U);    // swr t1,15(t0)
    addMemoryWord(50U, 9U, 16U);    // lwc2 t1,16(t0)
    addMemoryWord(58U, 9U, 20U);    // swc2 t1,20(t0)
    const auto memoryAccessIR = buildGoolIR(memoryAccessSource);
    assert(memoryAccessIR.instructions.size() == 14U);
    const auto& lbAccess = memoryAccessIR.instructions[0].nativeEffects.memoryAccess;
    assert(lbAccess.decoded && lbAccess.baseRegister == 8U && lbAccess.signedOffset == -16);
    assert(lbAccess.accessBytes == 1U && lbAccess.signExtendsLoad && !lbAccess.unalignedWordMerge);
    const auto& lhAccess = memoryAccessIR.instructions[1].nativeEffects.memoryAccess;
    assert(lhAccess.decoded && lhAccess.accessBytes == 2U && lhAccess.signExtendsLoad);
    const auto& lwlAccess = memoryAccessIR.instructions[2].nativeEffects.memoryAccess;
    assert(lwlAccess.decoded && lwlAccess.accessBytes == 4U && lwlAccess.unalignedWordMerge);
    const auto& lwAccess = memoryAccessIR.instructions[3].nativeEffects.memoryAccess;
    assert(lwAccess.decoded && lwAccess.accessBytes == 4U && !lwAccess.unalignedWordMerge);
    const auto& lbuAccess = memoryAccessIR.instructions[4].nativeEffects.memoryAccess;
    assert(lbuAccess.decoded && lbuAccess.accessBytes == 1U && !lbuAccess.signExtendsLoad);
    const auto& lhuAccess = memoryAccessIR.instructions[5].nativeEffects.memoryAccess;
    assert(lhuAccess.decoded && lhuAccess.accessBytes == 2U && !lhuAccess.signExtendsLoad);
    assert(memoryAccessIR.instructions[6].nativeEffects.memoryAccess.unalignedWordMerge);
    assert(memoryAccessIR.instructions[7].nativeEffects.memoryAccess.accessBytes == 1U);
    assert(memoryAccessIR.instructions[8].nativeEffects.memoryAccess.accessBytes == 2U);
    assert(memoryAccessIR.instructions[9].nativeEffects.memoryAccess.unalignedWordMerge);
    assert(memoryAccessIR.instructions[10].nativeEffects.memoryAccess.accessBytes == 4U);
    assert(memoryAccessIR.instructions[11].nativeEffects.memoryAccess.unalignedWordMerge);
    assert(memoryAccessIR.instructions[12].nativeEffects.memoryAccess.accessBytes == 4U);
    assert(memoryAccessIR.instructions[13].nativeEffects.memoryAccess.accessBytes == 4U);
    for (const auto& instruction : memoryAccessIR.instructions) {
        assert(instruction.nativeEffects.memoryAccess.decoded);
        assert(instruction.nativeEffects.memoryAccess.baseRegister == 8U);
    }

    // A validated branch marks exactly its following instruction as the
    // architectural delay slot; a final branch with no following word has no slot.
    NsfGool delaySlotSource;
    delaySlotSource.version = NsfGoolVersion::Version2;
    const auto addDelaySlotWord = [&delaySlotSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = delaySlotSource.instructions.size() * 4U;
        delaySlotSource.instructions.push_back(instruction);
    };
    addDelaySlotWord((4U << 26U) | (8U << 21U) | (9U << 16U) | 1U); // beq t0,t1,+1
    addDelaySlotWord((9U << 26U) | (29U << 21U) | (29U << 16U) | 4U); // addiu sp,sp,4
    const auto delaySlotIR = buildGoolIR(delaySlotSource);
    assert(delaySlotIR.instructions.size() == 2);
    assert(!delaySlotIR.instructions[0].isControlTransferDelaySlot);
    assert(delaySlotIR.instructions[1].isControlTransferDelaySlot);
    assert(!delaySlotIR.instructions[0].nativeDelaySlotOwnerInstructionValid);
    assert(delaySlotIR.instructions[1].nativeDelaySlotOwnerInstructionValid);
    assert(delaySlotIR.instructions[1].nativeDelaySlotOwnerInstruction == 0U);

    NsfGool terminalBranchSource;
    terminalBranchSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction terminalBranch;
    terminalBranch.isMips = true;
    terminalBranch.word = (2U << 26U) | 1U; // j 1
    terminalBranchSource.instructions.push_back(terminalBranch);
    const auto terminalBranchIR = buildGoolIR(terminalBranchSource);
    assert(terminalBranchIR.instructions.size() == 1);
    assert(terminalBranchIR.instructions[0].nativeEffects.controlTransferHasDelaySlot);
    assert(!terminalBranchIR.instructions[0].isControlTransferDelaySlot);

    // R3000A SPECIAL instructions are only considered analyzed when their
    // reserved fields satisfy the encoding constraints. Exception instructions
    // are recognized without inventing GPR or memory effects.
    NsfGool validationSource;
    validationSource.version = NsfGoolVersion::Version2;
    const auto addValidationWord = [&validationSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = validationSource.instructions.size() * 4U;
        validationSource.instructions.push_back(instruction);
    };
    addValidationWord((1U << 21U) | (2U << 16U) | (3U << 11U) | (4U << 6U)); // invalid sll: rs != 0
    addValidationWord((8U << 21U) | (9U << 16U) | (10U << 11U) | (1U << 6U) | 33U); // invalid addu: shamt != 0
    addValidationWord(0x0000000CU); // syscall
    addValidationWord(0x0000000DU); // break
    addValidationWord((21U << 21U) | (1U << 16U) | 8U); // invalid jr: rt != 0
    const auto validationIR = buildGoolIR(validationSource);
    assert(validationIR.instructions.size() == 5);
    assert(!validationIR.instructions[0].nativeEffects.known);
    assert(!validationIR.instructions[1].nativeEffects.known);
    for (std::size_t i = 2; i <= 3; ++i) {
        const auto& exceptionEffects = validationIR.instructions[i].nativeEffects;
        assert(exceptionEffects.known);
        assert(exceptionEffects.gprReadMask == 0U && exceptionEffects.gprWriteMask == 0U);
        assert(exceptionEffects.memory == GoolIRMipsMemoryEffect::None);
        assert(!exceptionEffects.controlTransfer);
    }
    assert(!validationIR.instructions[4].nativeEffects.known);

    // Immediate-format instructions also have reserved-field constraints.
    NsfGool immediateValidationSource;
    immediateValidationSource.version = NsfGoolVersion::Version2;
    const auto addImmediateWord = [&immediateValidationSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = immediateValidationSource.instructions.size() * 4U;
        immediateValidationSource.instructions.push_back(instruction);
    };
    addImmediateWord((6U << 26U) | (4U << 21U) | (1U << 16U) | 0x10U); // invalid blez: rt != 0
    addImmediateWord((7U << 26U) | (5U << 21U) | (2U << 16U) | 0x20U); // invalid bgtz: rt != 0
    addImmediateWord((15U << 26U) | (3U << 21U) | (6U << 16U) | 0x1234U); // invalid lui: rs != 0
    addImmediateWord((6U << 26U) | (4U << 21U) | 0x10U); // valid blez
    addImmediateWord((15U << 26U) | (6U << 16U) | 0x1234U); // valid lui
    const auto immediateValidationIR = buildGoolIR(immediateValidationSource);
    assert(immediateValidationIR.instructions.size() == 5);
    assert(!immediateValidationIR.instructions[0].nativeEffects.known);
    assert(!immediateValidationIR.instructions[1].nativeEffects.known);
    assert(!immediateValidationIR.instructions[2].nativeEffects.known);
    assert(immediateValidationIR.instructions[3].nativeEffects.known);
    assert(immediateValidationIR.instructions[3].nativeEffects.controlTransfer);
    assert(immediateValidationIR.instructions[4].nativeEffects.known);
    assert(immediateValidationIR.instructions[4].nativeEffects.gprWriteMask == (std::uint32_t{1} << 6U));

    // COP0 transfers distinguish general-purpose and coprocessor register
    // effects. Reserved low fields and unsupported COP0 operations stay unknown.
    NsfGool cop0Source;
    cop0Source.version = NsfGoolVersion::Version2;
    const auto addCop0Word = [&cop0Source](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = cop0Source.instructions.size() * 4U;
        cop0Source.instructions.push_back(instruction);
    };
    addCop0Word((16U << 26U) | (2U << 16U) | (12U << 11U)); // mfc0 v0,status
    addCop0Word((16U << 26U) | (4U << 21U) | (3U << 16U) | (13U << 11U)); // mtc0 cause,v1
    addCop0Word((16U << 26U) | (2U << 16U) | (12U << 11U) | 1U); // invalid mfc0 reserved bit
    addCop0Word((16U << 26U) | (8U << 21U)); // bc0f: branch on false COP0 condition
    const auto cop0IR = buildGoolIR(cop0Source);
    assert(cop0IR.instructions.size() == 4);
    const auto& mfc0Effects = cop0IR.instructions[0].nativeEffects;
    assert(mfc0Effects.known && mfc0Effects.memory == GoolIRMipsMemoryEffect::None);
    assert(mfc0Effects.gprWriteMask == (std::uint32_t{1} << 2U));
    assert(mfc0Effects.coprocessor0ReadMask == (std::uint32_t{1} << 12U));
    assert(mfc0Effects.coprocessor0WriteMask == 0U);
    const auto& mtc0Effects = cop0IR.instructions[1].nativeEffects;
    assert(mtc0Effects.known && mtc0Effects.gprReadMask == (std::uint32_t{1} << 3U));
    assert(mtc0Effects.coprocessor0ReadMask == 0U);
    assert(mtc0Effects.coprocessor0WriteMask == (std::uint32_t{1} << 13U));
    assert(!cop0IR.instructions[2].nativeEffects.known);
    const auto& bc0fEffects = cop0IR.instructions[3].nativeEffects;
    assert(bc0fEffects.known && bc0fEffects.controlTransfer);
    assert(bc0fEffects.controlTransferHasDelaySlot);
    assert(bc0fEffects.readsCoprocessor0Condition);
    assert(bc0fEffects.gprReadMask == 0U && bc0fEffects.gprWriteMask == 0U);

    addCop0Word((16U << 26U) | (8U << 21U) | (1U << 16U) | 4U); // bc0t
    addCop0Word((16U << 26U) | (16U << 21U) | 16U); // rfe
    addCop0Word((16U << 26U) | (8U << 21U) | (2U << 16U) | 4U); // invalid bc0 rt
    const auto cop0ControlIR = buildGoolIR(cop0Source);
    assert(cop0ControlIR.instructions.size() == 7);
    const auto& bc0Effects = cop0ControlIR.instructions[4].nativeEffects;
    assert(bc0Effects.known && bc0Effects.controlTransfer);
    assert(bc0Effects.readsCoprocessor0Condition);
    assert(bc0Effects.gprReadMask == 0U && bc0Effects.gprWriteMask == 0U);
    const auto& rfeEffects = cop0ControlIR.instructions[5].nativeEffects;
    assert(rfeEffects.known && !rfeEffects.controlTransfer);
    assert(rfeEffects.coprocessor0ReadMask == (std::uint32_t{1} << 12U));
    assert(rfeEffects.coprocessor0WriteMask == (std::uint32_t{1} << 12U));
    assert(!cop0ControlIR.instructions[6].nativeEffects.known);

    // COP2/GTE register transfers use separate data/control register banks.
    // Condition branches are modeled. Selected GTE commands have conservative,
    // command-specific register effects; other commands remain unknown.
    NsfGool cop2Source;
    cop2Source.version = NsfGoolVersion::Version2;
    const auto addCop2Word = [&cop2Source](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
        instruction.byteOffset = cop2Source.instructions.size() * 4U;
        cop2Source.instructions.push_back(instruction);
    };
    addCop2Word((18U << 26U) | (0U << 21U) | (2U << 16U) | (3U << 11U)); // mfc2 v0,v3
    addCop2Word((18U << 26U) | (2U << 21U) | (4U << 16U) | (4U << 11U)); // cfc2 a0,c4
    addCop2Word((18U << 26U) | (4U << 21U) | (5U << 16U) | (6U << 11U)); // mtc2 a1,d6
    addCop2Word((18U << 26U) | (6U << 21U) | (7U << 16U) | (8U << 11U)); // ctc2 a3,c8
    addCop2Word((18U << 26U) | (8U << 21U) | (1U << 16U) | 4U); // bc2t
    addCop2Word((18U << 26U) | (0U << 21U) | (2U << 16U) | (3U << 11U) | 1U); // malformed mfc2
    addCop2Word((18U << 26U) | (8U << 21U) | (2U << 16U) | 4U); // malformed bc2
    const std::uint32_t gteCommandWord = (18U << 26U) | (16U << 21U) |
        (1U << 17U) | (2U << 15U) | (1U << 13U) |
        (1U << 19U) | (1U << 10U) | 0x12U; // MVMVA fields, sf=1, lm=1
    addCop2Word(gteCommandWord);
    addCop2Word((18U << 26U) | (16U << 21U) | 0x06U); // nclip
    addCop2Word((18U << 26U) | (16U << 21U) | 0x2DU); // avsz3
    addCop2Word((18U << 26U) | (16U << 21U) | 0x2EU); // avsz4
    addCop2Word((18U << 26U) | (16U << 21U) | 0x28U); // sqr
    addCop2Word((18U << 26U) | (16U << 21U) | 0x3DU); // gpf
    addCop2Word((18U << 26U) | (16U << 21U) | 0x3EU); // gpl
    addCop2Word((18U << 26U) | (16U << 21U) | 0x0CU); // op
    addCop2Word((18U << 26U) | (16U << 21U) | 0x10U); // dpcs
    addCop2Word((18U << 26U) | (16U << 21U) | 0x11U); // intpl
    addCop2Word((18U << 26U) | (16U << 21U) | 0x29U); // dcpl
    addCop2Word((18U << 26U) | (16U << 21U) | 0x2AU); // dpct
    addCop2Word((18U << 26U) | (16U << 21U) | 0x1CU); // cc
    addCop2Word((18U << 26U) | (16U << 21U) | 0x14U); // cdp
    addCop2Word((18U << 26U) | (16U << 21U) | 0x1BU); // nccs
    addCop2Word((18U << 26U) | (16U << 21U) | 0x1EU); // ncs
    addCop2Word((18U << 26U) | (16U << 21U) | 0x13U); // ncds
    addCop2Word((18U << 26U) | (16U << 21U) | 0x16U); // ncdt
    addCop2Word((18U << 26U) | (16U << 21U) | 0x20U); // nct
    addCop2Word((18U << 26U) | (16U << 21U) | 0x3FU); // ncct
    addCop2Word((18U << 26U) | (16U << 21U) | 0x01U); // rtps
    addCop2Word((18U << 26U) | (16U << 21U) | 0x30U); // rtpt
    addCop2Word((50U << 26U) | (4U << 21U) | (7U << 16U) | 0x20U); // lwc2 d7,0x20(a0)
    addCop2Word((58U << 26U) | (5U << 21U) | (12U << 16U) | 0x24U); // swc2 d12,0x24(a1)
    addCop2Word((18U << 26U) | (0U << 21U) | (2U << 16U) | (29U << 11U)); // mfc2 v0,orgb
    addCop2Word((18U << 26U) | (4U << 21U) | (4U << 16U) | (28U << 11U)); // mtc2 a0,irgb
    addCop2Word((18U << 26U) | (4U << 21U) | (5U << 16U) | (30U << 11U)); // mtc2 a1,lzcs
    addCop2Word((50U << 26U) | (4U << 21U) | (28U << 16U) | 0x28U); // lwc2 irgb,0x28(a0)
    addCop2Word((58U << 26U) | (5U << 21U) | (29U << 16U) | 0x2CU); // swc2 orgb,0x2c(a1)
    addCop2Word((50U << 26U) | (4U << 21U) | (30U << 16U) | 0x30U); // lwc2 lzcs,0x30(a0)
    addCop2Word((18U << 26U) | (0U << 21U) | (2U << 16U) | (15U << 11U)); // mfc2 v0,sxyp
    addCop2Word((18U << 26U) | (4U << 21U) | (3U << 16U) | (15U << 11U)); // mtc2 v1,sxyp
    addCop2Word((50U << 26U) | (4U << 21U) | (15U << 16U) | 0x34U); // lwc2 sxyp,0x34(a0)
    addCop2Word((58U << 26U) | (5U << 21U) | (15U << 16U) | 0x38U); // swc2 sxyp,0x38(a1)
    addCop2Word((18U << 26U) | (0U << 21U) | (6U << 16U) | (28U << 11U)); // mfc2 a2,irgb
    addCop2Word((58U << 26U) | (5U << 21U) | (28U << 16U) | 0x3CU); // swc2 irgb,0x3c(a1)
    addCop2Word((18U << 26U) | (8U << 21U) | (0U << 16U) | 4U); // bc2f
    addCop2Word((35U << 26U) | (4U << 21U) | (8U << 16U) | 0x10U); // lw t0,0x10(a0)
    addCop2Word((18U << 26U) | (0U << 21U) | (0U << 16U) | (3U << 11U)); // mfc2 zero,v3
    const auto cop2IR = buildGoolIR(cop2Source);
    assert(cop2IR.instructions.size() == 46);
    const auto& mfc2Effects = cop2IR.instructions[0].nativeEffects;
    assert(mfc2Effects.known && mfc2Effects.gprWriteMask == (std::uint32_t{1} << 2U));
    assert(mfc2Effects.coprocessor2DataReadMask == (std::uint32_t{1} << 3U));
    assert(mfc2Effects.coprocessor2ControlReadMask == 0U);
    assert(mfc2Effects.gprWriteHasLoadDelay);
    const auto& cfc2Effects = cop2IR.instructions[1].nativeEffects;
    assert(cfc2Effects.known && cfc2Effects.gprWriteMask == (std::uint32_t{1} << 4U));
    assert(cfc2Effects.coprocessor2ControlReadMask == (std::uint32_t{1} << 4U));
    assert(cfc2Effects.gprWriteHasLoadDelay);
    const auto& mtc2Effects = cop2IR.instructions[2].nativeEffects;
    assert(mtc2Effects.known && mtc2Effects.gprReadMask == (std::uint32_t{1} << 5U));
    assert(mtc2Effects.coprocessor2DataWriteMask == (std::uint32_t{1} << 6U));
    const auto& ctc2Effects = cop2IR.instructions[3].nativeEffects;
    assert(ctc2Effects.known && ctc2Effects.gprReadMask == (std::uint32_t{1} << 7U));
    assert(ctc2Effects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 8U));
    const auto& bc2Effects = cop2IR.instructions[4].nativeEffects;
    assert(bc2Effects.known && bc2Effects.controlTransfer);
    assert(bc2Effects.controlTransferHasDelaySlot);
    assert(bc2Effects.readsCoprocessor2Condition);
    assert(bc2Effects.coprocessor2ConditionKnownFalse);
    assert(bc2Effects.gprReadMask == 0U && bc2Effects.gprWriteMask == 0U);
    assert(!cop2IR.instructions[5].nativeEffects.known);
    assert(!cop2IR.instructions[6].nativeEffects.known);
    const auto& mvmvaEffects = cop2IR.instructions[7].nativeEffects;
    assert(mvmvaEffects.known);
    // Fixture selects the light matrix, V2, and background translation.
    assert(mvmvaEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 4U) | (std::uint32_t{1} << 5U)));
    assert(mvmvaEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U)));
    assert(mvmvaEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 25U) |
         (std::uint32_t{1} << 26U) | (std::uint32_t{1} << 27U)));
    assert(mvmvaEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& nclipEffects = cop2IR.instructions[8].nativeEffects;
    assert(nclipEffects.known);
    assert(nclipEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) | (std::uint32_t{1} << 14U)));
    assert(nclipEffects.coprocessor2DataWriteMask == (std::uint32_t{1} << 24U));
    assert(nclipEffects.coprocessor2ControlReadMask == 0U);
    assert(nclipEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& avsz3Effects = cop2IR.instructions[9].nativeEffects;
    assert(avsz3Effects.known);
    assert(avsz3Effects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 17U) | (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U)));
    assert(avsz3Effects.coprocessor2ControlReadMask == (std::uint32_t{1} << 29U));
    assert(avsz3Effects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 24U) | (std::uint32_t{1} << 7U)));
    assert(avsz3Effects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& avsz4Effects = cop2IR.instructions[10].nativeEffects;
    assert(avsz4Effects.known);
    assert(avsz4Effects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 16U) | (std::uint32_t{1} << 17U) |
         (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U)));
    assert(avsz4Effects.coprocessor2ControlReadMask == (std::uint32_t{1} << 30U));
    assert(avsz4Effects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 24U) | (std::uint32_t{1} << 7U)));
    assert(avsz4Effects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& sqrEffects = cop2IR.instructions[11].nativeEffects;
    assert(sqrEffects.known);
    assert(sqrEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U)));
    assert(sqrEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) | (std::uint32_t{1} << 27U)));
    assert(sqrEffects.coprocessor2ControlReadMask == 0U);
    assert(sqrEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& gpfEffects = cop2IR.instructions[12].nativeEffects;
    assert(gpfEffects.known);
    assert(gpfEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U) |
         (std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 21U) |
         (std::uint32_t{1} << 22U)));
    assert(gpfEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(gpfEffects.coprocessor2ControlReadMask == 0U);
    assert(gpfEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& gplEffects = cop2IR.instructions[13].nativeEffects;
    assert(gplEffects.known);
    assert(gplEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U) |
         (std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 21U) |
         (std::uint32_t{1} << 22U) | (std::uint32_t{1} << 25U) |
         (std::uint32_t{1} << 26U) | (std::uint32_t{1} << 27U)));
    assert(gplEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(gplEffects.coprocessor2ControlReadMask == 0U);
    assert(gplEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& opEffects = cop2IR.instructions[14].nativeEffects;
    assert(opEffects.known);
    assert(opEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U)));
    assert(opEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 2U) |
         (std::uint32_t{1} << 4U)));
    assert(opEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 25U) |
         (std::uint32_t{1} << 26U) | (std::uint32_t{1} << 27U)));
    assert(opEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& dpcsEffects = cop2IR.instructions[15].nativeEffects;
    assert(dpcsEffects.known);
    assert(dpcsEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U)));
    assert(dpcsEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 23U)));
    assert(dpcsEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(dpcsEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& intplEffects = cop2IR.instructions[16].nativeEffects;
    assert(intplEffects.known);
    assert(intplEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U) |
         (std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U)));
    assert(intplEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 23U)));
    assert(intplEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(intplEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& dcplEffects = cop2IR.instructions[17].nativeEffects;
    assert(dcplEffects.known);
    assert(dcplEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U) |
         (std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U)));
    assert(dcplEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 23U)));
    assert(dcplEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(dcplEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& dpctEffects = cop2IR.instructions[18].nativeEffects;
    assert(dpctEffects.known);
    // DPCT consumes the three RGB FIFO entries over its three passes; RGBC
    // supplies only the CODE byte, while IR0 and far color drive depth cueing.
    assert(dpctEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U) |
         (std::uint32_t{1} << 20U) | (std::uint32_t{1} << 21U) |
         (std::uint32_t{1} << 22U)));
    assert(dpctEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 23U)));
    assert(dpctEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(dpctEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& ccEffects = cop2IR.instructions[19].nativeEffects;
    assert(ccEffects.known);
    assert(ccEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U)));
    assert(ccEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 13U) | (std::uint32_t{1} << 14U) |
         (std::uint32_t{1} << 15U) | (std::uint32_t{1} << 16U) |
         (std::uint32_t{1} << 17U) | (std::uint32_t{1} << 18U) |
         (std::uint32_t{1} << 19U) | (std::uint32_t{1} << 20U)));
    assert(ccEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(ccEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& cdpEffects = cop2IR.instructions[20].nativeEffects;
    assert(cdpEffects.known);
    assert(cdpEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U) |
         (std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U)));
    assert(cdpEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 13U) | (std::uint32_t{1} << 14U) |
         (std::uint32_t{1} << 15U) | (std::uint32_t{1} << 16U) |
         (std::uint32_t{1} << 17U) | (std::uint32_t{1} << 18U) |
         (std::uint32_t{1} << 19U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 23U)));
    assert(cdpEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(cdpEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& nccsEffects = cop2IR.instructions[21].nativeEffects;
    assert(nccsEffects.known);
    assert(nccsEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 6U)));
    assert(nccsEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U) |
         (std::uint32_t{1} << 16U) | (std::uint32_t{1} << 17U) |
         (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U) |
         (std::uint32_t{1} << 20U)));
    assert(nccsEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(nccsEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& ncsEffects = cop2IR.instructions[22].nativeEffects;
    assert(ncsEffects.known);
    // NCS shares NCCS lighting stages but does not consume RGBC; its RGB FIFO
    // output still carries the command's color result.
    assert(ncsEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U)));
    assert(ncsEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U) |
         (std::uint32_t{1} << 16U) | (std::uint32_t{1} << 17U) |
         (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U) |
         (std::uint32_t{1} << 20U)));
    assert(ncsEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(ncsEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& ncdsEffects = cop2IR.instructions[23].nativeEffects;
    assert(ncdsEffects.known);
    assert(ncdsEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U)));
    assert(ncdsEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U) |
         (std::uint32_t{1} << 16U) | (std::uint32_t{1} << 17U) |
         (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U) |
         (std::uint32_t{1} << 20U) | (std::uint32_t{1} << 21U) |
         (std::uint32_t{1} << 22U) | (std::uint32_t{1} << 23U)));
    assert(ncdsEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(ncdsEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& ncdtEffects = cop2IR.instructions[24].nativeEffects;
    assert(ncdtEffects.known);
    assert(ncdtEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 2U) | (std::uint32_t{1} << 3U) |
         (std::uint32_t{1} << 4U) | (std::uint32_t{1} << 5U) |
         (std::uint32_t{1} << 6U) | (std::uint32_t{1} << 8U)));
    assert(ncdtEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U) |
         (std::uint32_t{1} << 16U) | (std::uint32_t{1} << 17U) |
         (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U) |
         (std::uint32_t{1} << 20U) | (std::uint32_t{1} << 21U) |
         (std::uint32_t{1} << 22U) | (std::uint32_t{1} << 23U)));
    assert(ncdtEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(ncdtEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& nctEffects = cop2IR.instructions[25].nativeEffects;
    assert(nctEffects.known);
    assert(nctEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 2U) | (std::uint32_t{1} << 3U) |
         (std::uint32_t{1} << 4U) | (std::uint32_t{1} << 5U)));
    assert(nctEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U) |
         (std::uint32_t{1} << 16U) | (std::uint32_t{1} << 17U) |
         (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U) |
         (std::uint32_t{1} << 20U)));
    assert(nctEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(nctEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& ncctEffects = cop2IR.instructions[26].nativeEffects;
    assert(ncctEffects.known);
    assert(ncctEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 2U) | (std::uint32_t{1} << 3U) |
         (std::uint32_t{1} << 4U) | (std::uint32_t{1} << 5U) |
         (std::uint32_t{1} << 6U)));
    assert(ncctEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U) |
         (std::uint32_t{1} << 16U) | (std::uint32_t{1} << 17U) |
         (std::uint32_t{1} << 18U) | (std::uint32_t{1} << 19U) |
         (std::uint32_t{1} << 20U)));
    assert(ncctEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 9U) | (std::uint32_t{1} << 10U) |
         (std::uint32_t{1} << 11U) | (std::uint32_t{1} << 20U) |
         (std::uint32_t{1} << 21U) | (std::uint32_t{1} << 22U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(ncctEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& rtpsEffects = cop2IR.instructions[27].nativeEffects;
    assert(rtpsEffects.known);
    assert(rtpsEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U)));
    assert(rtpsEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 2U) | (std::uint32_t{1} << 3U) |
         (std::uint32_t{1} << 4U) | (std::uint32_t{1} << 5U) |
         (std::uint32_t{1} << 6U) | (std::uint32_t{1} << 7U) |
         (std::uint32_t{1} << 24U) | (std::uint32_t{1} << 25U) |
         (std::uint32_t{1} << 26U) | (std::uint32_t{1} << 27U) |
         (std::uint32_t{1} << 28U)));
    assert(rtpsEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 16U) |
         (std::uint32_t{1} << 17U) | (std::uint32_t{1} << 18U) |
         (std::uint32_t{1} << 19U) | (std::uint32_t{1} << 24U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(rtpsEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& rtptEffects = cop2IR.instructions[28].nativeEffects;
    assert(rtptEffects.known);
    assert(rtptEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 2U) | (std::uint32_t{1} << 3U) |
         (std::uint32_t{1} << 4U) | (std::uint32_t{1} << 5U)));
    assert(rtptEffects.coprocessor2ControlReadMask ==
        ((std::uint32_t{1} << 0U) | (std::uint32_t{1} << 1U) |
         (std::uint32_t{1} << 2U) | (std::uint32_t{1} << 3U) |
         (std::uint32_t{1} << 4U) | (std::uint32_t{1} << 5U) |
         (std::uint32_t{1} << 6U) | (std::uint32_t{1} << 7U) |
         (std::uint32_t{1} << 24U) | (std::uint32_t{1} << 25U) |
         (std::uint32_t{1} << 26U) | (std::uint32_t{1} << 27U) |
         (std::uint32_t{1} << 28U)));
    assert(rtptEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 8U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U) |
         (std::uint32_t{1} << 12U) | (std::uint32_t{1} << 13U) |
         (std::uint32_t{1} << 14U) | (std::uint32_t{1} << 16U) |
         (std::uint32_t{1} << 17U) | (std::uint32_t{1} << 18U) |
         (std::uint32_t{1} << 19U) | (std::uint32_t{1} << 24U) |
         (std::uint32_t{1} << 25U) | (std::uint32_t{1} << 26U) |
         (std::uint32_t{1} << 27U)));
    assert(rtptEffects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 31U));
    const auto& lwc2Effects = cop2IR.instructions[29].nativeEffects;
    assert(lwc2Effects.known && lwc2Effects.memory == GoolIRMipsMemoryEffect::Read);
    assert(lwc2Effects.gprReadMask == (std::uint32_t{1} << 4U));
    assert(lwc2Effects.gprWriteMask == 0U);
    assert(lwc2Effects.coprocessor2DataWriteMask == (std::uint32_t{1} << 7U));
    assert(lwc2Effects.coprocessor2DataReadMask == 0U);
    const auto& swc2Effects = cop2IR.instructions[30].nativeEffects;
    assert(swc2Effects.known && swc2Effects.memory == GoolIRMipsMemoryEffect::Write);
    assert(swc2Effects.gprReadMask == (std::uint32_t{1} << 5U));
    assert(swc2Effects.gprWriteMask == 0U);
    assert(swc2Effects.coprocessor2DataReadMask == (std::uint32_t{1} << 12U));
    assert(swc2Effects.coprocessor2DataWriteMask == 0U);
    const auto& orgbReadEffects = cop2IR.instructions[31].nativeEffects;
    assert(orgbReadEffects.known);
    assert(orgbReadEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 29U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U)));
    const auto& irgbWriteEffects = cop2IR.instructions[32].nativeEffects;
    assert(irgbWriteEffects.known);
    assert(irgbWriteEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 28U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U)));
    const auto& lzcsWriteEffects = cop2IR.instructions[33].nativeEffects;
    assert(lzcsWriteEffects.known);
    assert(lzcsWriteEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 30U) | (std::uint32_t{1} << 31U)));
    const auto& irgbLoadEffects = cop2IR.instructions[34].nativeEffects;
    assert(irgbLoadEffects.known && irgbLoadEffects.memory == GoolIRMipsMemoryEffect::Read);
    assert(irgbLoadEffects.gprReadMask == (std::uint32_t{1} << 4U));
    assert(irgbLoadEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 28U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U)));
    const auto& orgbStoreEffects = cop2IR.instructions[35].nativeEffects;
    assert(orgbStoreEffects.known && orgbStoreEffects.memory == GoolIRMipsMemoryEffect::Write);
    assert(orgbStoreEffects.gprReadMask == (std::uint32_t{1} << 5U));
    assert(orgbStoreEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 29U) | (std::uint32_t{1} << 9U) |
         (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U)));
    const auto& lzcsLoadEffects = cop2IR.instructions[36].nativeEffects;
    assert(lzcsLoadEffects.known && lzcsLoadEffects.memory == GoolIRMipsMemoryEffect::Read);
    assert(lzcsLoadEffects.coprocessor2DataWriteMask ==
        ((std::uint32_t{1} << 30U) | (std::uint32_t{1} << 31U)));
    const std::uint32_t sxyFifoMask = (std::uint32_t{1} << 12U) |
        (std::uint32_t{1} << 13U) | (std::uint32_t{1} << 14U);
    const auto& sxypReadEffects = cop2IR.instructions[37].nativeEffects;
    assert(sxypReadEffects.known);
    assert(sxypReadEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U)));
    const auto& sxypWriteEffects = cop2IR.instructions[38].nativeEffects;
    assert(sxypWriteEffects.known);
    assert(sxypWriteEffects.coprocessor2DataWriteMask ==
        (sxyFifoMask | (std::uint32_t{1} << 15U)));
    const auto& sxypLoadEffects = cop2IR.instructions[39].nativeEffects;
    assert(sxypLoadEffects.known && sxypLoadEffects.memory == GoolIRMipsMemoryEffect::Read);
    assert(sxypLoadEffects.coprocessor2DataWriteMask ==
        (sxyFifoMask | (std::uint32_t{1} << 15U)));
    const auto& sxypStoreEffects = cop2IR.instructions[40].nativeEffects;
    assert(sxypStoreEffects.known && sxypStoreEffects.memory == GoolIRMipsMemoryEffect::Write);
    assert(sxypStoreEffects.coprocessor2DataReadMask ==
        ((std::uint32_t{1} << 14U) | (std::uint32_t{1} << 15U)));
    const std::uint32_t irColorMask = (std::uint32_t{1} << 9U) |
        (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U);
    const auto& irgbReadEffects = cop2IR.instructions[41].nativeEffects;
    assert(irgbReadEffects.known);
    assert(irgbReadEffects.coprocessor2DataReadMask ==
        (irColorMask | (std::uint32_t{1} << 28U)));
    const auto& irgbStoreEffects = cop2IR.instructions[42].nativeEffects;
    assert(irgbStoreEffects.known && irgbStoreEffects.memory == GoolIRMipsMemoryEffect::Write);
    assert(irgbStoreEffects.coprocessor2DataReadMask ==
        (irColorMask | (std::uint32_t{1} << 28U)));
    const auto& bc2fEffects = cop2IR.instructions[43].nativeEffects;
    assert(bc2fEffects.known && bc2fEffects.controlTransfer);
    assert(bc2fEffects.controlTransferHasDelaySlot);
    assert(bc2fEffects.readsCoprocessor2Condition);
    assert(bc2fEffects.coprocessor2ConditionKnownFalse);
    const auto& lwEffects = cop2IR.instructions[44].nativeEffects;
    assert(lwEffects.known && lwEffects.gprWriteHasLoadDelay);
    assert(lwEffects.gprWriteMask == (std::uint32_t{1} << 8U));
    const auto& zeroMfc2Effects = cop2IR.instructions[45].nativeEffects;
    assert(zeroMfc2Effects.known && zeroMfc2Effects.gprWriteMask == 0U);
    assert(!zeroMfc2Effects.gprWriteHasLoadDelay);
    const auto& gteFields = cop2IR.instructions[7].nativeFields.gteCommand;
    assert(gteFields.decoded);
    assert(gteFields.command == 0x12U);
    assert(gteFields.commandKnown && gteFields.mnemonic == "mvmva");
    assert(gteFields.shiftFraction);
    assert(gteFields.matrix == 1U);
    assert(gteFields.vector == 2U);
    assert(gteFields.translation == 1U);
    assert(gteFields.limitMode);
    assert(gteFields.ignoredCommandBits == 0U);
    assert(gteFields.reservedBits11To12 == 0U);
    assert(gteFields.reservedBits6To9 == 0U);

    // Preserve unknown command numbers without assigning a guessed mnemonic.
    NsfGool unknownGteSource;
    unknownGteSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction unknownGte;
    unknownGte.isMips = true;
    unknownGte.word = (18U << 26U) | (16U << 21U) | 0x02U;
    unknownGteSource.instructions.push_back(unknownGte);
    const auto unknownGteIR = buildGoolIR(unknownGteSource);
    const auto& unknownGteFields = unknownGteIR.instructions[0].nativeFields.gteCommand;
    assert(unknownGteFields.decoded);
    assert(unknownGteFields.command == 0x02U);
    assert(!unknownGteFields.commandKnown && unknownGteFields.mnemonic.empty());
    assert(!unknownGteIR.instructions[0].nativeEffects.known);

    // Known command IDs with nonzero reserved command bits remain losslessly
    // decoded but do not receive guessed execution effects.
    NsfGool reservedGteSource;
    reservedGteSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction reservedGte;
    reservedGte.isMips = true;
    reservedGte.word = (18U << 26U) | (16U << 21U) | (1U << 6U) | 0x06U;
    reservedGteSource.instructions.push_back(reservedGte);
    const auto reservedGteIR = buildGoolIR(reservedGteSource);
    assert(reservedGteIR.instructions[0].nativeFields.gteCommand.commandKnown);
    assert(reservedGteIR.instructions[0].nativeFields.gteCommand.mnemonic == "nclip");
    assert(reservedGteIR.instructions[0].nativeFields.gteCommand.reservedBits6To9 == 1U);
    assert(!reservedGteIR.instructions[0].nativeEffects.known);

    // Native instructions remain source-mapped to their GOOL stream position.
    // The listing exposes branch destinations, delay-slot ownership, CPU/GTE
    // pipeline hazards, memory operands, and decoded GTE command selectors.
    NsfGool nativeListingSource;
    nativeListingSource.version = NsfGoolVersion::Version2;
    const auto addNativeListingWord = [&nativeListingSource](
        std::uint32_t word, const char* mnemonic, const char* arguments) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.mipsKnown = true;
        instruction.word = word;
        instruction.byteOffset = 0x300U + nativeListingSource.instructions.size() * 4U;
        instruction.mnemonic = mnemonic;
        instruction.arguments = arguments;
        nativeListingSource.instructions.push_back(instruction);
    };
    addNativeListingWord((4U << 26U) | (8U << 21U) | (9U << 16U) | 2U,
        "beq", "$t0, $t1, 0x2");
    addNativeListingWord((9U << 26U) | (10U << 16U) | 1U,
        "addiu", "$t2, $zero, 1");
    addNativeListingWord((9U << 26U) | (2U << 16U),
        "addiu", "$v0, $zero, 0");
    addNativeListingWord((35U << 26U) | (29U << 21U) | (11U << 16U) | 4U,
        "lw", "$t3, 4($sp)");
    addNativeListingWord((11U << 21U) | (8U << 16U) | (12U << 11U) | 33U,
        "addu", "$t4, $t3, $t0");
    addNativeListingWord((50U << 26U) | (4U << 21U) | (7U << 16U) | 0x20U,
        "lwc2", "$7, 0x20($a0)");
    addNativeListingWord((9U << 26U) | (2U << 16U),
        "addiu", "$v0, $zero, 0");
    addNativeListingWord((18U << 26U) | (8U << 16U) | (7U << 11U),
        "mfc2", "$t0, $7");
    addNativeListingWord((18U << 26U) | (16U << 21U) | 0x06U,
        "nclip", "");
    addNativeListingWord((18U << 26U) | (16U << 21U) | 0x02U,
        "cop2", "");
    const auto nativeListingIR = buildGoolIR(nativeListingSource);
    assert(nativeListingIR.instructions[0].nativeBranchTargetInstructionValid);
    assert(nativeListingIR.instructions[0].nativeBranchTargetInstruction == 3U);
    assert(nativeListingIR.instructions[1].nativeDelaySlotOwnerInstructionValid);
    assert(nativeListingIR.instructions[1].nativeDelaySlotOwnerInstruction == 0U);
    assert(nativeListingIR.instructions[4].gprReadInLoadDelaySlotMask ==
        (std::uint32_t{1} << 11U));
    assert(nativeListingIR.instructions[7].gteDataAccessInTransferDelayMask ==
        (std::uint32_t{1} << 7U));
    const auto nativeListing = decompileGoolIR(nativeListingIR);
    assert(nativeListing.text.find("control=beq") != std::string::npos);
    assert(nativeListing.text.find("branch-target=@0x030C (instruction 3)") != std::string::npos);
    assert(nativeListing.text.find("delay-slot-of instruction 0") != std::string::npos);
    assert(nativeListing.text.find("load-delay-reads={$t3}") != std::string::npos);
    assert(nativeListing.text.find("memory-read=4B [$sp + 4]") != std::string::npos);
    assert(nativeListing.text.find("gte-transfer-delay-access={d7}") != std::string::npos);
    assert(nativeListing.text.find("gte=nclip(sf=0,mx=0,v=0,cv=0,lm=0)") != std::string::npos);
    assert(nativeListing.text.find("gte=command 0x02") != std::string::npos);
    assert(!nativeListing.diagnostics.empty());

    // The first source-like decompiler keeps branches, calls, transitions,
    // returns, and unsupported words traceable to their original instruction.
    NsfGool decompileSource;
    decompileSource.version = NsfGoolVersion::Version2;
    decompileSource.id = 0x0200007B;
    decompileSource.dataPool = {static_cast<std::int32_t>(0x0200007B)};
    decompileSource.stateDescriptors.push_back({0, 0, 0, 0, 0, 0});
    decompileSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        4, 4, false, true, NsfGoolStateHookStatus::Local});

    NsfGoolInstruction conditionalBranch;
    conditionalBranch.byteOffset = 0x100;
    conditionalBranch.opcode = 51; // BNEZ subtype, relative target = instruction 2.
    conditionalBranch.operandBits = (29U << 14U) | 1U;
    decompileSource.instructions.push_back(conditionalBranch);

    NsfGoolInstruction call;
    call.byteOffset = 0x104;
    call.opcode = 59;
    call.operandBits = 3U | (1U << 20U); // Local instruction 3, one argument.
    decompileSource.instructions.push_back(call);

    NsfGoolInstruction transition;
    transition.byteOffset = 0x108;
    transition.opcode = 53; // GO state descriptor 0.
    transition.operandBits = 0U;
    decompileSource.instructions.push_back(transition);

    NsfGoolInstruction returnInstruction;
    returnInstruction.byteOffset = 0x10C;
    returnInstruction.opcode = 49;
    decompileSource.instructions.push_back(returnInstruction);

    NsfGoolInstruction unknownInstruction;
    unknownInstruction.byteOffset = 0x110;
    unknownInstruction.word = 0xFA123456U;
    unknownInstruction.opcode = 250;
    unknownInstruction.operandBits = 0x123456U;
    decompileSource.instructions.push_back(unknownInstruction);

    NsfGoolInstruction unknownMips;
    unknownMips.byteOffset = 0x114;
    unknownMips.word = 0xFFFFFFFFU;
    unknownMips.isMips = true;
    decompileSource.instructions.push_back(unknownMips);

    const auto decompileIR = buildGoolIR(decompileSource);
    const auto decompiled = decompileGoolIR(decompileIR);
    assert(decompiled.text.find("if (subtype != 0) goto L00002") != std::string::npos);
    assert(decompiled.text.find("call L00003(args=1)") != std::string::npos);
    assert(decompiled.text.find("transition state_descriptor[0]") != std::string::npos);
    assert(decompiled.text.find("GOOL 0x0200007B") != std::string::npos);
    assert(decompiled.text.find("return") != std::string::npos);
    assert(decompiled.text.find("L00004:") != std::string::npos);
    assert(decompiled.text.find(".gool_word 0xFA123456") != std::string::npos);
    assert(decompiled.text.find(".mips_word 0xFFFFFFFF") != std::string::npos);
    assert(decompiled.text.find("@0x0100") != std::string::npos);
    assert(decompiled.diagnostics.size() == 2);

    GoolIRDecompileOptions selectedRange;
    selectedRange.startInstruction = 2;
    selectedRange.endInstruction = 4;
    selectedRange.includeHeader = false;
    const auto selectedDecompilation = decompileGoolIR(decompileIR, selectedRange);
    assert(selectedDecompilation.text.find("L00002:") == 0);
    assert(selectedDecompilation.text.find("transition state_descriptor[0]") != std::string::npos);
    assert(selectedDecompilation.text.find("return") != std::string::npos);
    assert(selectedDecompilation.text.find(".gool_word") == std::string::npos);

    // A forward conditional branch whose fallthrough arm ends in BRA to a
    // later join can be represented as a structured if block without changing
    // the branch condition or dropping source mapping.
    NsfGool structuredSource;
    structuredSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction skipBody;
    skipBody.opcode = 52; // BEQZ branches to instruction 3; fallthrough means field != 0.
    skipBody.operandBits = 2U | (29U << 14U); // displacement 2, field subtype.
    skipBody.byteOffset = 0x200;
    structuredSource.instructions.push_back(skipBody);
    NsfGoolInstruction bodyInstruction;
    bodyInstruction.opcode = 0;
    bodyInstruction.byteOffset = 0x204;
    structuredSource.instructions.push_back(bodyInstruction);
    NsfGoolInstruction joinBranch;
    joinBranch.opcode = 50; // BRA from instruction 2 to instruction 4.
    joinBranch.operandBits = 1U;
    joinBranch.byteOffset = 0x208;
    structuredSource.instructions.push_back(joinBranch);
    NsfGoolInstruction alternateInstruction;
    alternateInstruction.opcode = 47;
    alternateInstruction.byteOffset = 0x20C;
    structuredSource.instructions.push_back(alternateInstruction);
    NsfGoolInstruction joinInstruction;
    joinInstruction.opcode = 47;
    joinInstruction.byteOffset = 0x210;
    structuredSource.instructions.push_back(joinInstruction);

    const auto structuredIR = buildGoolIR(structuredSource);
    const auto structuredText = decompileGoolIR(structuredIR);
    assert(structuredText.text.find("if (subtype != 0) {") != std::string::npos);
    assert(structuredText.text.find("    }\n") != std::string::npos);
    assert(structuredText.text.find("goto L00004") == std::string::npos);
    assert(structuredText.text.find("@0x0204") != std::string::npos);
    assert(structuredText.text.find("@0x0208") == std::string::npos);

    // Two adjacent forward-if clauses with a shared join become else-if.
    GoolIRProgram chainProgram;
    auto appendChainInstruction = [&](std::uint8_t opcode, std::uint32_t offset,
                                      std::vector<GoolIROperand> operands = {}) {
        GoolIRInstruction item;
        item.opcode = opcode;
        item.opcodeKnown = true;
        item.sourceByteOffset = offset;
        item.mnemonic = opcode == 4U ? "EQL" : opcode == 52U ? "BEQZ" :
                        opcode == 50U ? "BRA" : "NOP";
        item.operands = std::move(operands);
        chainProgram.instructions.push_back(std::move(item));
    };
    const GoolIROperand chainField{'L', 12, 0, 0,
        GoolIROperandKind::ProcessField, "self->subtype"};
    const GoolIROperand chainValue2{'R', 12, 2, 2,
        GoolIROperandKind::Value, "2"};
    const auto chainDisplacement = [](std::int32_t value) {
        return GoolIROperand{'I', 12, static_cast<std::uint32_t>(value), value,
            GoolIROperandKind::Signed, std::to_string(value)};
    };
    appendChainInstruction(4, 0x400, {chainField, chainValue2});
    appendChainInstruction(52, 0x404, {chainDisplacement(2)});
    appendChainInstruction(47, 0x408);
    appendChainInstruction(50, 0x40C, {chainDisplacement(5)});
    appendChainInstruction(4, 0x410, {chainField,
        GoolIROperand{'R', 12, 10, 10, GoolIROperandKind::Value, "10"}});
    appendChainInstruction(52, 0x414, {chainDisplacement(2)});
    appendChainInstruction(47, 0x418);
    appendChainInstruction(50, 0x41C, {chainDisplacement(1)});
    appendChainInstruction(47, 0x420);
    appendChainInstruction(47, 0x424);
    const auto chainOutput = decompileGoolIR(chainProgram);
    assert(chainOutput.text.find("if (self->subtype == 2) {") != std::string::npos);
    assert(chainOutput.text.find("} else if (self->subtype == 10) {") != std::string::npos);

    // A unique fallback target becomes a plain else arm.
    GoolIRProgram elseProgram;
    auto appendElseInstruction = [&](std::uint8_t opcode, std::uint32_t offset,
                                     std::vector<GoolIROperand> operands = {}) {
        GoolIRInstruction item;
        item.opcode = opcode;
        item.opcodeKnown = true;
        item.sourceByteOffset = offset;
        item.mnemonic = opcode == 4U ? "EQL" : opcode == 52U ? "BEQZ" :
                        opcode == 50U ? "BRA" : "NOP";
        item.operands = std::move(operands);
        elseProgram.instructions.push_back(std::move(item));
    };
    const GoolIROperand elseField{'L', 12, 0, 0,
        GoolIROperandKind::ProcessField, "self->subtype"};
    const GoolIROperand elseValue{'R', 12, 2, 2,
        GoolIROperandKind::Value, "2"};
    const auto elseDisplacement = [](std::int32_t value) {
        return GoolIROperand{'I', 12, static_cast<std::uint32_t>(value), value,
            GoolIROperandKind::Signed, std::to_string(value)};
    };
    appendElseInstruction(4, 0x500, {elseField, elseValue});
    appendElseInstruction(52, 0x504, {elseDisplacement(3)});
    appendElseInstruction(47, 0x508);
    appendElseInstruction(50, 0x50C, {elseDisplacement(2)});
    appendElseInstruction(47, 0x510);
    appendElseInstruction(47, 0x514);
    appendElseInstruction(47, 0x518); // common join
    const auto elseOutput = decompileGoolIR(elseProgram);
    assert(elseOutput.text.find("if (self->subtype == 2) {") != std::string::npos);
    assert(elseOutput.text.find("} else {") != std::string::npos);
    assert(elseOutput.text.find("L00004:") == std::string::npos);
    assert(elseOutput.text.find("@0x0514") != std::string::npos);

    // A pre-test loop with one backward BRA is reconstructed as while.
    GoolIRProgram loopProgram;
    auto appendLoopInstruction = [&](std::uint8_t opcode, std::uint32_t offset,
                                     std::vector<GoolIROperand> operands = {}) {
        GoolIRInstruction item;
        item.opcode = opcode;
        item.opcodeKnown = true;
        item.sourceByteOffset = offset;
        item.mnemonic = opcode == 4U ? "EQL" : opcode == 52U ? "BEQZ" :
                        opcode == 50U ? "BRA" : "NOP";
        item.operands = std::move(operands);
        loopProgram.instructions.push_back(std::move(item));
    };
    const GoolIROperand loopField{'L', 12, 0, 0,
        GoolIROperandKind::ProcessField, "self->counter"};
    const GoolIROperand loopLimit{'R', 12, 3, 3,
        GoolIROperandKind::Value, "3"};
    const auto loopDisplacement = [](std::int32_t value) {
        return GoolIROperand{'I', 12, static_cast<std::uint32_t>(value), value,
            GoolIROperandKind::Signed, std::to_string(value)};
    };
    appendLoopInstruction(4, 0x600, {loopField, loopLimit});
    appendLoopInstruction(52, 0x604, {loopDisplacement(3)});
    appendLoopInstruction(47, 0x608);
    appendLoopInstruction(47, 0x60C);
    appendLoopInstruction(50, 0x610, {loopDisplacement(-4)});
    appendLoopInstruction(47, 0x614);
    const auto loopOutput = decompileGoolIR(loopProgram);
    assert(loopOutput.text.find("while (self->counter == 3) {") != std::string::npos);
    assert(loopOutput.text.find("goto L") == std::string::npos);
    assert(loopOutput.text.find("@0x0614") != std::string::npos);

    // A comparison immediately feeding BEQZ should become the source
    // expression, not expose the temporary stack result as "sp != 0".
    GoolIRProgram comparisonProgram;
    GoolIRInstruction comparison;
    comparison.opcode = 4;
    comparison.opcodeKnown = true;
    comparison.mnemonic = "EQL";
    comparison.sourceByteOffset = 0x300;
    comparison.operands.push_back({'L', 12, 0, 0, GoolIROperandKind::ProcessField, "self->subtype"});
    comparison.operands.push_back({'R', 12, 2, 2, GoolIROperandKind::Value, "2"});
    comparisonProgram.instructions.push_back(comparison);

    GoolIRInstruction conditional;
    conditional.opcode = 52;
    conditional.opcodeKnown = true;
    conditional.mnemonic = "BEQZ";
    conditional.sourceByteOffset = 0x304;
    conditional.operands.push_back({'I', 12, 2, 2, GoolIROperandKind::Signed, "2"});
    conditional.operands.push_back({'V', 4, 0, 0, GoolIROperandKind::Value, "0"});
    conditional.operands.push_back({'R', 12, 0, 0, GoolIROperandKind::ProcessField, "sp"});
    comparisonProgram.instructions.push_back(conditional);

    GoolIRInstruction comparisonBody;
    comparisonBody.opcode = 47;
    comparisonBody.opcodeKnown = true;
    comparisonBody.mnemonic = "NOP";
    comparisonBody.sourceByteOffset = 0x308;
    comparisonProgram.instructions.push_back(comparisonBody);

    GoolIRInstruction comparisonJoinBranch;
    comparisonJoinBranch.opcode = 50;
    comparisonJoinBranch.opcodeKnown = true;
    comparisonJoinBranch.mnemonic = "BRA";
    comparisonJoinBranch.sourceByteOffset = 0x30C;
    comparisonJoinBranch.operands.push_back({'I', 12, 1, 1, GoolIROperandKind::Signed, "1"});
    comparisonProgram.instructions.push_back(comparisonJoinBranch);

    GoolIRInstruction comparisonElse;
    comparisonElse.opcode = 47;
    comparisonElse.opcodeKnown = true;
    comparisonElse.mnemonic = "NOP";
    comparisonElse.sourceByteOffset = 0x310;
    comparisonProgram.instructions.push_back(comparisonElse);

    GoolIRInstruction comparisonJoin;
    comparisonJoin.opcode = 47;
    comparisonJoin.opcodeKnown = true;
    comparisonJoin.mnemonic = "NOP";
    comparisonJoin.sourceByteOffset = 0x314;
    comparisonProgram.instructions.push_back(comparisonJoin);

    const auto comparisonOutput = decompileGoolIR(comparisonProgram);
    assert(comparisonOutput.text.find("if (self->subtype == 2) {") != std::string::npos);
    assert(comparisonOutput.text.find("if (sp != 0)") == std::string::npos);


    // Routine discovery is shared across GOOL objects: local Code hooks
    // establish entry points, while RET prevents traversal into the next routine.
    NsfGool routineSource;
    routineSource.version = NsfGoolVersion::Version2;
    for (std::size_t i = 0; i < 6; ++i) {
        NsfGoolInstruction instruction;
        instruction.opcode = (i == 2U || i == 4U) ? 49U : 47U;
        instruction.byteOffset = i * 4U;
        routineSource.instructions.push_back(instruction);
    }
    routineSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    routineSource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        3, 3, false, true, NsfGoolStateHookStatus::Local});
    const auto routineIR = buildGoolIR(routineSource);
    assert(routineIR.routineEntries.size() == 2);
    assert(routineIR.routines.size() == 2);
    assert(routineIR.routines[0].entryInstruction == 0);
    assert((routineIR.routines[0].instructions == std::vector<std::size_t>{0, 1, 2}));
    assert(routineIR.routines[1].entryInstruction == 3);
    assert((routineIR.routines[1].instructions == std::vector<std::size_t>{3, 4}));
    assert(!routineIR.routines[0].hasSharedInstructions);
    assert(!routineIR.routines[1].hasSharedInstructions);
    assert((routineIR.unassignedInstructions == std::vector<std::size_t>{5}));

    // Reaching another known entry point is reported as a boundary rather than
    // silently treating it as shared code or absorbing it into the first routine.
    NsfGool boundarySource;
    boundarySource.version = NsfGoolVersion::Version2;
    for (std::size_t i = 0; i < 4; ++i) {
        NsfGoolInstruction instruction;
        instruction.opcode = 47U;
        instruction.byteOffset = i * 4U;
        boundarySource.instructions.push_back(instruction);
    }
    boundarySource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    boundarySource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        2, 2, false, true, NsfGoolStateHookStatus::Local});
    const auto boundaryIR = buildGoolIR(boundarySource);
    assert(boundaryIR.routines.size() == 2);
    assert((boundaryIR.routines[0].instructions == std::vector<std::size_t>{0, 1}));
    assert((boundaryIR.routines[0].entryPointBoundaries == std::vector<std::size_t>{2}));
    assert((boundaryIR.routines[1].instructions == std::vector<std::size_t>{2, 3}));
    assert(!boundaryIR.routines[0].hasSharedInstructions);
    assert(!boundaryIR.routines[1].hasSharedInstructions);

    assert(boundaryIR.routines[0].exits.size() == 1);
    assert(boundaryIR.routines[0].exits[0].kind == GoolIRRoutineExitKind::EntryPointBoundary);
    assert(boundaryIR.routines[0].exits[0].sourceInstruction == 1);
    assert(boundaryIR.routines[0].exits[0].targetInstruction == 2);
    const auto firstRoutineText = decompileGoolIRRoutine(boundaryIR, 0);
    assert(firstRoutineText.text.find("Routine 0 entry: L00000") != std::string::npos);
    assert(firstRoutineText.text.find("@0x0000") != std::string::npos);
    assert(firstRoutineText.text.find("@0x0004") != std::string::npos);
    assert(firstRoutineText.text.find("@0x0008") == std::string::npos);
    assert(firstRoutineText.text.find("entry-point boundary") != std::string::npos);
    assert(!decompileGoolIRRoutine(boundaryIR, 99).diagnostics.empty());

    // A contiguous discovered routine is decompiled as one control-flow
    // region, allowing the existing conservative conditional recognizer to
    // structure the complete routine rather than isolated instruction words.
    GoolIRProgram structuredRoutineProgram;
    const auto structuredField = GoolIROperand{'L', 12, 0, 0,
        GoolIROperandKind::ProcessField, "self->subtype"};
    const auto structuredValue = GoolIROperand{'R', 12, 2, 2,
        GoolIROperandKind::Value, "2"};
    const auto structuredDisp = [](std::int32_t value) {
        return GoolIROperand{'I', 12, static_cast<std::uint32_t>(value), value,
            GoolIROperandKind::Signed, std::to_string(value)};
    };
    GoolIRInstruction structuredCompare;
    structuredCompare.opcode = 4U;
    structuredCompare.opcodeKnown = true;
    structuredCompare.mnemonic = "EQL";
    structuredCompare.operands = {structuredField, structuredValue};
    structuredRoutineProgram.instructions.push_back(structuredCompare);
    GoolIRInstruction structuredBranch;
    structuredBranch.opcode = 52U;
    structuredBranch.opcodeKnown = true;
    structuredBranch.mnemonic = "BEQZ";
    structuredBranch.operands.push_back(structuredDisp(2));
    structuredBranch.operands.push_back({'V', 4, 0, 0, GoolIROperandKind::Value, "0"});
    structuredBranch.operands.push_back({'R', 12, 0, 0,
        GoolIROperandKind::ProcessField, "sp"});
    structuredRoutineProgram.instructions.push_back(structuredBranch);
    GoolIRInstruction structuredBody;
    structuredBody.opcode = 47U;
    structuredBody.opcodeKnown = true;
    structuredBody.mnemonic = "NOP";
    structuredRoutineProgram.instructions.push_back(structuredBody);
    GoolIRInstruction structuredJoinBranch;
    structuredJoinBranch.opcode = 50U;
    structuredJoinBranch.opcodeKnown = true;
    structuredJoinBranch.mnemonic = "BRA";
    structuredJoinBranch.sourceByteOffset = 0x000C;
    structuredJoinBranch.operands.push_back(structuredDisp(1));
    structuredRoutineProgram.instructions.push_back(structuredJoinBranch);
    GoolIRInstruction structuredElse;
    structuredElse.opcode = 47U;
    structuredElse.opcodeKnown = true;
    structuredElse.mnemonic = "NOP";
    structuredRoutineProgram.instructions.push_back(structuredElse);
    GoolIRInstruction structuredJoin;
    structuredJoin.opcode = 49U;
    structuredJoin.opcodeKnown = true;
    structuredJoin.mnemonic = "RET";
    structuredRoutineProgram.instructions.push_back(structuredJoin);
    GoolIRRoutine structuredRoutine;
    structuredRoutine.entryInstruction = 0;
    structuredRoutine.instructions = {0, 1, 2, 3, 4, 5};
    structuredRoutineProgram.routines.push_back(structuredRoutine);
    const auto structuredRoutineText = decompileGoolIRRoutine(
        structuredRoutineProgram, 0);
    assert(structuredRoutineText.text.find("if (self->subtype == 2) {") !=
        std::string::npos);
    assert(structuredRoutineText.text.find("} else {") != std::string::npos);
    assert(structuredRoutineText.text.find("goto L") == std::string::npos);

    // A non-contiguous routine still gets routine-wide structuring inside
    // each maximal adjacent region, without absorbing instructions owned by
    // another routine or left unassigned.
    GoolIRProgram discontiguousStructuredProgram = structuredRoutineProgram;
    GoolIRInstruction unrelatedInstruction;
    unrelatedInstruction.opcode = 47U;
    unrelatedInstruction.opcodeKnown = true;
    unrelatedInstruction.mnemonic = "NOP";
    unrelatedInstruction.sourceByteOffset = 0x7F0;
    discontiguousStructuredProgram.instructions.push_back(unrelatedInstruction);
    GoolIRInstruction laterRoutineInstruction;
    laterRoutineInstruction.opcode = 49U;
    laterRoutineInstruction.opcodeKnown = true;
    laterRoutineInstruction.mnemonic = "RET";
    laterRoutineInstruction.sourceByteOffset = 0x7F4;
    discontiguousStructuredProgram.instructions.push_back(laterRoutineInstruction);
    discontiguousStructuredProgram.routines[0].instructions.push_back(7);
    const auto discontiguousStructuredText = decompileGoolIRRoutine(
        discontiguousStructuredProgram, 0);
    assert(discontiguousStructuredText.text.find("if (self->subtype == 2) {") !=
        std::string::npos);
    assert(discontiguousStructuredText.text.find("} else {") != std::string::npos);
    assert(discontiguousStructuredText.text.find("Discontiguous routine region [7, 8)") !=
        std::string::npos);
    assert(discontiguousStructuredText.text.find("@0x07F0") == std::string::npos);
    assert(discontiguousStructuredText.text.find("@0x07F4") != std::string::npos);

    // The routine-wide CFG view can structure a conditional whose two
    // arms are separated by an unrelated instruction in the containing stream.
    GoolIRProgram crossRegionProgram = structuredRoutineProgram;
    GoolIRInstruction gapInstruction;
    gapInstruction.opcode = 47U;
    gapInstruction.opcodeKnown = true;
    gapInstruction.mnemonic = "NOP";
    gapInstruction.sourceByteOffset = 0x7F0;
    crossRegionProgram.instructions.insert(crossRegionProgram.instructions.begin() + 3,
        gapInstruction);
    crossRegionProgram.instructions[1].operands[0].value = 3; // old target 4 -> new 5
    crossRegionProgram.instructions[4].operands[0].value = 1; // new source 4 -> target 6
    crossRegionProgram.routines[0].instructions = {0, 1, 2, 4, 5, 6};
    const auto crossRegionText = decompileGoolIRRoutine(crossRegionProgram, 0);
    assert(crossRegionText.text.find("if (self->subtype == 2) {") != std::string::npos);
    assert(crossRegionText.text.find("} else {") != std::string::npos);
    assert(crossRegionText.text.find("@0x07F0") == std::string::npos);
    assert(crossRegionText.text.find("@0x000C") != std::string::npos);

    // A routine listing must define labels for branch destinations even
    // when the routine's reachable instructions are non-contiguous in the
    // containing GOOL instruction stream.
    GoolIRProgram labeledRoutineProgram;
    GoolIRInstruction routineBranch;
    routineBranch.opcode = 50U;
    routineBranch.opcodeKnown = true;
    routineBranch.mnemonic = "BRA";
    routineBranch.sourceByteOffset = 0x700;
    routineBranch.operands.push_back({'I', 12, 1, 1,
        GoolIROperandKind::Signed, "1"}); // instruction 0 -> instruction 2
    labeledRoutineProgram.instructions.push_back(routineBranch);
    GoolIRInstruction skippedInstruction;
    skippedInstruction.opcode = 47U;
    skippedInstruction.opcodeKnown = true;
    skippedInstruction.mnemonic = "NOP";
    skippedInstruction.sourceByteOffset = 0x704;
    labeledRoutineProgram.instructions.push_back(skippedInstruction);
    GoolIRInstruction routineTarget;
    routineTarget.opcode = 49U;
    routineTarget.opcodeKnown = true;
    routineTarget.mnemonic = "RET";
    routineTarget.sourceByteOffset = 0x708;
    labeledRoutineProgram.instructions.push_back(routineTarget);
    GoolIRRoutine labeledRoutine;
    labeledRoutine.entryInstruction = 0;
    labeledRoutine.instructions = {0, 2};
    labeledRoutineProgram.routines.push_back(labeledRoutine);
    const auto labeledRoutineText = decompileGoolIRRoutine(labeledRoutineProgram, 0);
    assert(labeledRoutineText.text.find("goto L00002") != std::string::npos);
    assert(labeledRoutineText.text.find("L00002:\n") != std::string::npos);
    assert(labeledRoutineText.text.find("@0x0704") == std::string::npos);

    // A valid branch destination outside the selected routine must not
    // become a dangling local label. Keep its absolute target explicit.
    GoolIRProgram leavingRoutineProgram;
    GoolIRInstruction leavingBranch;
    leavingBranch.opcode = 50U;
    leavingBranch.opcodeKnown = true;
    leavingBranch.mnemonic = "BRA";
    leavingBranch.sourceByteOffset = 0x720;
    leavingBranch.operands.push_back({'I', 12, 1, 1,
        GoolIROperandKind::Signed, "1"}); // instruction 0 -> instruction 2
    leavingRoutineProgram.instructions.push_back(leavingBranch);
    GoolIRInstruction middleInstruction;
    middleInstruction.opcode = 47U;
    middleInstruction.opcodeKnown = true;
    middleInstruction.mnemonic = "NOP";
    middleInstruction.sourceByteOffset = 0x724;
    leavingRoutineProgram.instructions.push_back(middleInstruction);
    GoolIRInstruction otherRoutineEntry;
    otherRoutineEntry.opcode = 49U;
    otherRoutineEntry.opcodeKnown = true;
    otherRoutineEntry.mnemonic = "RET";
    otherRoutineEntry.sourceByteOffset = 0x728;
    leavingRoutineProgram.instructions.push_back(otherRoutineEntry);
    GoolIRRoutine leavingRoutine;
    leavingRoutine.entryInstruction = 0;
    leavingRoutine.instructions = {0};
    leavingRoutineProgram.routines.push_back(leavingRoutine);
    const auto leavingRoutineText = decompileGoolIRRoutine(leavingRoutineProgram, 0);
    assert(leavingRoutineText.text.find("goto instruction 2") != std::string::npos);
    assert(leavingRoutineText.text.find("branch leaves this routine") != std::string::npos);
    assert(leavingRoutineText.text.find("goto L00002") == std::string::npos);
    assert(leavingRoutineText.text.find("L00002:\n") == std::string::npos);

    // RET is an explicit routine exit, while a final ordinary instruction is
    // reported as an incomplete end-of-program path rather than an invented RET.
    NsfGool exitSource;
    exitSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction exitReturn;
    exitReturn.opcode = 49U;
    exitSource.instructions.push_back(exitReturn);
    NsfGoolInstruction exitTail;
    exitTail.opcode = 47U;
    exitSource.instructions.push_back(exitTail);
    exitSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    exitSource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        1, 1, false, true, NsfGoolStateHookStatus::Local});
    const auto exitIR = buildGoolIR(exitSource);
    assert(exitIR.routines.size() == 2);
    assert(exitIR.routines[0].exits.size() == 1);
    assert(exitIR.routines[0].exits[0].kind == GoolIRRoutineExitKind::Return);
    assert(exitIR.routines[0].exits[0].sourceInstruction == 0);
    assert(exitIR.routines[1].exits.size() == 1);
    assert(exitIR.routines[1].exits[0].kind == GoolIRRoutineExitKind::EndOfProgram);
    assert(!exitIR.routines[1].exits[0].targetValid);
    assert(!exitIR.routines[1].diagnostics.empty());

    // Invalid branch destinations remain explicit routine exits. The
    // analysis must not silently drop an out-of-range edge or invent a return.
    NsfGool invalidBranchSource;
    invalidBranchSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction invalidBranch;
    invalidBranch.opcode = 50U; // BRA to instruction 6, outside this one-word program.
    invalidBranch.operandBits = 5U;
    invalidBranchSource.instructions.push_back(invalidBranch);
    invalidBranchSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    const auto invalidBranchIR = buildGoolIR(invalidBranchSource);
    assert(invalidBranchIR.routines.size() == 1);
    assert(invalidBranchIR.routines[0].instructions == std::vector<std::size_t>{0});
    assert(invalidBranchIR.routines[0].exits.size() == 1);
    assert(invalidBranchIR.routines[0].exits[0].kind ==
        GoolIRRoutineExitKind::InvalidBranchTarget);
    assert(invalidBranchIR.routines[0].exits[0].sourceInstruction == 0);
    assert(invalidBranchIR.routines[0].exits[0].flowKind == GoolIRFlowEdgeKind::Branch);
    assert(!invalidBranchIR.routines[0].exits[0].targetValid);
    assert(!invalidBranchIR.routines[0].exits[0].diagnostic.empty());
    const auto invalidRoutineText = decompileGoolIRRoutine(invalidBranchIR, 0);
    assert(invalidRoutineText.text.find("invalid branch target") != std::string::npos);

    // Local calls are represented as call relationships, not traversed as
    // ordinary control flow. The destination remains its own discovered routine.
    NsfGool callSource;
    callSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction routineLocalCall;
    routineLocalCall.opcode = 59U;
    routineLocalCall.operandBits = 1U; // local instruction target 1
    callSource.instructions.push_back(routineLocalCall);
    NsfGoolInstruction callReturn;
    callReturn.opcode = 49U;
    callSource.instructions.push_back(callReturn);
    callSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    const auto callIR = buildGoolIR(callSource);
    assert(callIR.routines.size() == 2);
    assert(callIR.routines[0].calls.size() == 1);
    assert(callIR.routines[0].calls[0].sourceInstruction == 0);
    assert(callIR.routines[0].calls[0].targetInstructionValid);
    assert(callIR.routines[0].calls[0].targetInstruction == 1);
    assert(callIR.routines[0].calls[0].targetRoutineFound);
    assert(callIR.routines[0].calls[0].targetRoutineIndex == 1);
    assert(callIR.routines[1].entryInstruction == 1);
    assert(callIR.routines[1].exits.size() == 1);
    assert(callIR.routines[1].exits[0].kind == GoolIRRoutineExitKind::Return);

    // Two independent entries can converge on ordinary code that is not itself
    // a known entry point. Preserve the exact shared instruction, not just a flag.
    NsfGool sharedSource;
    sharedSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction firstConditional;
    firstConditional.opcode = 51U; // BNEZ: branch from 0 to 3, or fall through to 1.
    firstConditional.operandBits = 2U | (29U << 14U);
    sharedSource.instructions.push_back(firstConditional);
    NsfGoolInstruction firstJump;
    firstJump.opcode = 50U; // BRA from 1 to the shared instruction at 3.
    firstJump.operandBits = 1U;
    sharedSource.instructions.push_back(firstJump);
    NsfGoolInstruction secondEntry;
    secondEntry.opcode = 47U;
    sharedSource.instructions.push_back(secondEntry);
    NsfGoolInstruction sharedReturn;
    sharedReturn.opcode = 49U;
    sharedSource.instructions.push_back(sharedReturn);
    sharedSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    sharedSource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        2, 2, false, true, NsfGoolStateHookStatus::Local});
    const auto sharedIR = buildGoolIR(sharedSource);
    assert(sharedIR.routines.size() == 2);
    assert((sharedIR.routines[0].instructions == std::vector<std::size_t>{0, 1, 3}));
    assert((sharedIR.routines[1].instructions == std::vector<std::size_t>{2, 3}));
    assert(sharedIR.routines[0].hasSharedInstructions);
    assert(sharedIR.routines[1].hasSharedInstructions);
    assert((sharedIR.routines[0].sharedInstructions == std::vector<std::size_t>{3}));
    assert((sharedIR.routines[1].sharedInstructions == std::vector<std::size_t>{3}));

    // Real Crash 2 sample: DispC's Code hook at instruction 0. These twenty
    // words are copied from the extracted entry; this is a focused IR/decompiler
    // regression, not a replacement for parsing the complete NSF entry.
    NsfGool dispC;
    dispC.version = NsfGoolVersion::Version2;
    dispC.id = static_cast<std::int32_t>(0x4E938CCDU);
    dispC.format = 1;
    dispC.entryCount = 2;
    dispC.dataPool.resize(21, 0);
    dispC.dataPool[0] = static_cast<std::int32_t>(0x4E938CCDU);
    dispC.dataPool[2] = -256;
    dispC.dataPool[3] = 6;
    dispC.dataPool[4] = 0x333;
    dispC.dataPool[20] = 0x40000;
    dispC.stateDescriptors.resize(4);
    dispC.stateDescriptors[3].goolIndex = 0;
    dispC.stateReferences.push_back({3, 0, 0, 0, 0x4E938CCD, true,
        NsfGoolStateReferenceStatus::Local});
    const std::uint32_t dispCWords[] = {
        0x2086480DU, 0x11014E1BU, 0x07002C38U, 0x08003E1FU,
        0x11E1FC38U, 0x11004E0EU, 0x11802E0FU, 0x11802E10U,
        0x11882E08U, 0x118BEE18U, 0x11800E0AU, 0x11800E4AU,
        0x11841E4BU, 0x11800E4CU, 0x11800E19U, 0x38C41D00U,
        0x1C305E4BU, 0x1C400E4BU, 0x35494003U, 0x31894000U
    };
    const char* dispCMnemonics[] = {
        "GVAW", "SETF", "ANDL", "ORL", "SETF", "SETF", "SETF",
        "SETF", "SETF", "SETF", "SETF", "SETF", "SETF", "SETF",
        "SETF", "ANIM", "MISC", "MISC", "GO", "RET"
    };
    for (std::size_t i = 0; i < 20; ++i) {
        NsfGoolInstruction instruction;
        instruction.byteOffset = i * 4U;
        instruction.word = dispCWords[i];
        instruction.opcode = static_cast<std::uint8_t>(dispCWords[i] >> 24U);
        instruction.operandBits = dispCWords[i] & 0x00FFFFFFU;
        instruction.mnemonic = dispCMnemonics[i];
        dispC.instructions.push_back(std::move(instruction));
    }
    // DispC descriptor 0's resolved local Code hook begins at instruction 0.
    dispC.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    const auto dispCIR = buildGoolIR(dispC);
    assert(dispCIR.instructions.size() == 20);
    assert(dispCIR.routineEntries.size() == 1);
    assert(dispCIR.routineEntries[0].instruction == 0);
    assert(dispCIR.routineEntries[0].fromCodeHook);
    assert(dispCIR.routines.size() == 1);
    assert(dispCIR.routines[0].entryInstruction == 0);
    assert(dispCIR.routines[0].instructions.size() == 20);
    assert(dispCIR.routines[0].instructions.front() == 0);
    assert(dispCIR.routines[0].instructions.back() == 19);
    assert(!dispCIR.routines[0].hasSharedInstructions);
    assert(dispCIR.routines[0].entryPointBoundaries.empty());
    assert(dispCIR.unassignedInstructions.empty());
    assert(dispCIR.instructions.front().rawWord == 0x2086480DU);
    assert(dispCIR.instructions[18].rawWord == 0x35494003U);
    assert(dispCIR.instructions[19].rawWord == 0x31894000U);
    assert(dispCIR.controlOperations.size() == 2);
    assert(dispCIR.controlOperations[0].kind == GoolIRControlOperationKind::StateTransition);
    assert(dispCIR.controlOperations[0].sourceInstruction == 18);
    assert(dispCIR.controlOperations[0].targetDescriptorValid);
    assert(dispCIR.controlOperations[0].targetDescriptorIndex == 3);
    assert(dispCIR.controlOperations[0].targetGoolEidValid);
    assert(dispCIR.controlOperations[0].targetGoolEid == 0x4E938CCD);
    assert(dispCIR.controlOperations[1].kind == GoolIRControlOperationKind::Return);
    assert(dispCIR.routines[0].exits.size() == 1);
    assert(dispCIR.routines[0].exits[0].kind == GoolIRRoutineExitKind::Return);
    assert(dispCIR.routines[0].exits[0].sourceInstruction == 19);
    const auto goBlock = dispCIR.instructionToBlock[18];
    const auto returnBlock = dispCIR.instructionToBlock[19];
    assert(goBlock != returnBlock);
    bool hasGoTransitionEdge = false;
    bool hasGoFallthroughEdge = false;
    for (const auto& edge : dispCIR.flowEdges) {
        if (edge.sourceBlock != goBlock) continue;
        if (edge.kind == GoolIRFlowEdgeKind::StateTransition && !edge.targetValid)
            hasGoTransitionEdge = true;
        if (edge.kind == GoolIRFlowEdgeKind::Fallthrough &&
            edge.targetValid && edge.targetInstruction == 19)
            hasGoFallthroughEdge = true;
    }
    assert(hasGoTransitionEdge && hasGoFallthroughEdge);
    const auto dispCText = decompileGoolIR(dispCIR);
    assert(dispCText.text.find("transition state_descriptor[3]") != std::string::npos);
    assert(dispCText.text.find("GOOL 0x4E938CCD") != std::string::npos);
    assert(dispCText.text.find("return") != std::string::npos);
    assert(dispCText.text.find("@0x0048") != std::string::npos);
    assert(dispCText.text.find("@0x004C") != std::string::npos);

    // SETF is a proven destination/source operation: render it as an assignment
    // while retaining the original instruction address and raw word comment.
    NsfGool assignmentSource;
    assignmentSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction setField;
    setField.opcode = 17U;
    setField.operandBits = (0x805U << 12U) | 0x408U; // x = immediate 5
    setField.byteOffset = 0x24;
    setField.word = 0x11000000U | setField.operandBits;
    assignmentSource.instructions.push_back(setField);
    const auto assignmentIR = buildGoolIR(assignmentSource);
    const auto assignmentText = decompileGoolIR(assignmentIR);
    assert(assignmentText.text.find("x = 5") != std::string::npos);
    assert(assignmentText.text.find("@0x0024") != std::string::npos);
    assert(assignmentText.text.find("raw=0x11805408") != std::string::npos);

    // Common arithmetic, logical, comparison, random, and unary GOOL
    // operations are rendered as source expressions using their documented
    // operand order and reference resolution.
    NsfGool expressionSource;
    expressionSource.version = NsfGoolVersion::Version2;
    const auto addExpressionInstruction = [&expressionSource](
        std::uint8_t opcode, std::uint32_t operands) {
        NsfGoolInstruction instruction;
        instruction.opcode = opcode;
        instruction.operandBits = operands;
        instruction.word = (static_cast<std::uint32_t>(opcode) << 24U) | operands;
        instruction.byteOffset = expressionSource.instructions.size() * 4U;
        expressionSource.instructions.push_back(instruction);
    };
    const std::uint32_t leftImmediate = 0x805U;
    const std::uint32_t rightImmediate = 0x80AU;
    const std::uint32_t binaryOperands = leftImmediate | (rightImmediate << 12U);
    for (const std::uint8_t opcode : {
        std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3},
        std::uint8_t{4}, std::uint8_t{5}, std::uint8_t{6}, std::uint8_t{7},
        std::uint8_t{8}, std::uint8_t{9}, std::uint8_t{10}, std::uint8_t{11},
        std::uint8_t{12}, std::uint8_t{13}, std::uint8_t{14}, std::uint8_t{15},
        std::uint8_t{21}})
        addExpressionInstruction(opcode, binaryOperands);
    addExpressionInstruction(16U, leftImmediate | (rightImmediate << 12U));
    const std::uint32_t unaryOperands = 5U | (leftImmediate << 12U);
    for (const std::uint8_t opcode : {
        std::uint8_t{18}, std::uint8_t{20}, std::uint8_t{23},
        std::uint8_t{25}, std::uint8_t{39}, std::uint8_t{43},
        std::uint8_t{44}})
        addExpressionInstruction(opcode, unaryOperands);
    const auto expressionIR = buildGoolIR(expressionSource);
    const auto expressionText = decompileGoolIR(expressionIR);
    assert(expressionText.text.find("(5 + 10)") != std::string::npos);
    assert(expressionText.text.find("(5 - 10)") != std::string::npos);
    assert(expressionText.text.find("(5 * 10)") != std::string::npos);
    assert(expressionText.text.find("(5 / 10)") != std::string::npos);
    assert(expressionText.text.find("(5 == 10)") != std::string::npos);
    assert(expressionText.text.find("(5 && 10)") != std::string::npos);
    assert(expressionText.text.find("(5 || 10)") != std::string::npos);
    assert(expressionText.text.find("(5 & 10)") != std::string::npos);
    assert(expressionText.text.find("(5 | 10)") != std::string::npos);
    assert(expressionText.text.find("(5 < 10)") != std::string::npos);
    assert(expressionText.text.find("(5 <= 10)") != std::string::npos);
    assert(expressionText.text.find("(5 > 10)") != std::string::npos);
    assert(expressionText.text.find("(5 >= 10)") != std::string::npos);
    assert(expressionText.text.find("(5 % 10)") != std::string::npos);
    assert(expressionText.text.find("(5 ^ 10)") != std::string::npos);
    assert(expressionText.text.find("(((5 & 10) ^ 10) == 0)") != std::string::npos);
    assert(expressionText.text.find("(10 < 0 ? (5 >> -10) : (5 << 10))") != std::string::npos);
    assert(expressionText.text.find("(5 + (rand() % (10 - 5)))") != std::string::npos);
    assert(expressionText.text.find("stack[5] = !5") != std::string::npos);
    assert(expressionText.text.find("stack[5] = &5") != std::string::npos);
    assert(expressionText.text.find("stack[5] = ~5") != std::string::npos);
    assert(expressionText.text.find("stack[5] = abs(5)") != std::string::npos);
    assert(expressionText.text.find("stack[5] = &anim[5]") != std::string::npos);
    assert(expressionText.text.find("stack[5] = sin(5)") != std::string::npos);
    assert(expressionText.text.find("stack[5] = cos(5)") != std::string::npos);
    assert(expressionText.diagnostics.empty());

    // A single reaching SETF definition can be substituted into a later
    // expression. Ambiguous and unmodeled values remain explicit references.
    NsfGool chainedExpressionSource;
    chainedExpressionSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction stackAssignment;
    stackAssignment.opcode = 17U;
    stackAssignment.operandBits = (0x805U << 12U) | 0x805U; // stack[5] = 5
    stackAssignment.byteOffset = 0;
    chainedExpressionSource.instructions.push_back(stackAssignment);
    NsfGoolInstruction chainedAdd;
    chainedAdd.opcode = 0U;
    chainedAdd.operandBits = (0x80AU << 12U) | 0xB05U; // stack[5] + 10
    chainedAdd.byteOffset = 4;
    chainedExpressionSource.instructions.push_back(chainedAdd);
    const auto chainedExpressionIR = buildGoolIR(chainedExpressionSource);
    const auto chainedExpressionText = decompileGoolIR(chainedExpressionIR);
    assert(chainedExpressionText.text.find("stack[5] = 5") != std::string::npos);
    assert(chainedExpressionText.text.find("(5 + 10)") != std::string::npos);
    assert(chainedExpressionText.diagnostics.empty());

    std::cout << "GOOL IR preservation and control-flow checks passed\\n";
}
