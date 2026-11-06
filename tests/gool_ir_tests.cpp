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
    assert(jalrIR.rawWord == 0x02A0F809U);
    assert(jalrIR.sourceByteOffset == 0x44);

    // Conservative R3000A register and memory effects are tracked independently
    // from mnemonic text, with unsupported encodings left explicitly unknown.
    NsfGool effectSource;
    effectSource.version = NsfGoolVersion::Version2;
    const auto addMipsWord = [&effectSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
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
    const auto& storeEffects = effectIR.instructions[2].nativeEffects;
    assert(storeEffects.known && storeEffects.memory == GoolIRMipsMemoryEffect::Write);
    assert((storeEffects.gprReadMask & (std::uint32_t{1} << 9U)) != 0U);
    const auto& multiplyEffects = effectIR.instructions[3].nativeEffects;
    assert(multiplyEffects.known && multiplyEffects.writesHi && multiplyEffects.writesLo);
    const auto& branchEffects = effectIR.instructions[4].nativeEffects;
    assert(branchEffects.known && branchEffects.controlTransfer);
    assert((branchEffects.gprReadMask & (std::uint32_t{1} << 8U)) != 0U);
    assert((branchEffects.gprReadMask & (std::uint32_t{1} << 9U)) != 0U);

    // R3000A SPECIAL instructions are only considered analyzed when their
    // reserved fields satisfy the encoding constraints. Exception instructions
    // are recognized without inventing GPR or memory effects.
    NsfGool validationSource;
    validationSource.version = NsfGoolVersion::Version2;
    const auto addValidationWord = [&validationSource](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
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
    // Condition branches are modeled. NCLIP, SQR, GPF, GPL, and AVSZ3/4 have
    // conservative, command-specific register effects; other commands remain unknown.
    NsfGool cop2Source;
    cop2Source.version = NsfGoolVersion::Version2;
    const auto addCop2Word = [&cop2Source](std::uint32_t word) {
        NsfGoolInstruction instruction;
        instruction.isMips = true;
        instruction.word = word;
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
    const auto cop2IR = buildGoolIR(cop2Source);
    assert(cop2IR.instructions.size() == 15);
    const auto& mfc2Effects = cop2IR.instructions[0].nativeEffects;
    assert(mfc2Effects.known && mfc2Effects.gprWriteMask == (std::uint32_t{1} << 2U));
    assert(mfc2Effects.coprocessor2DataReadMask == (std::uint32_t{1} << 3U));
    assert(mfc2Effects.coprocessor2ControlReadMask == 0U);
    const auto& cfc2Effects = cop2IR.instructions[1].nativeEffects;
    assert(cfc2Effects.known && cfc2Effects.gprWriteMask == (std::uint32_t{1} << 4U));
    assert(cfc2Effects.coprocessor2ControlReadMask == (std::uint32_t{1} << 4U));
    const auto& mtc2Effects = cop2IR.instructions[2].nativeEffects;
    assert(mtc2Effects.known && mtc2Effects.gprReadMask == (std::uint32_t{1} << 5U));
    assert(mtc2Effects.coprocessor2DataWriteMask == (std::uint32_t{1} << 6U));
    const auto& ctc2Effects = cop2IR.instructions[3].nativeEffects;
    assert(ctc2Effects.known && ctc2Effects.gprReadMask == (std::uint32_t{1} << 7U));
    assert(ctc2Effects.coprocessor2ControlWriteMask == (std::uint32_t{1} << 8U));
    const auto& bc2Effects = cop2IR.instructions[4].nativeEffects;
    assert(bc2Effects.known && bc2Effects.controlTransfer);
    assert(bc2Effects.readsCoprocessor2Condition);
    assert(bc2Effects.gprReadMask == 0U && bc2Effects.gprWriteMask == 0U);
    assert(!cop2IR.instructions[5].nativeEffects.known);
    assert(!cop2IR.instructions[6].nativeEffects.known);
    assert(!cop2IR.instructions[7].nativeEffects.known);
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

    std::cout << "GOOL IR preservation and control-flow checks passed\\n";
}
