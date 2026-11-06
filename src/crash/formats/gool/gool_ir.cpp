#include "crash/formats/gool/gool_ir.hpp"
#include "crash/formats/gool/gool_control_flow.hpp"
#include <algorithm>
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

GoolIRHookKind convertHookKind(NsfGoolStateHookKind kind) {
    switch (kind) {
    case NsfGoolStateHookKind::Transition: return GoolIRHookKind::Transition;
    case NsfGoolStateHookKind::Code: return GoolIRHookKind::Code;
    case NsfGoolStateHookKind::Event: default: return GoolIRHookKind::Event;
    }
}

GoolIRHookResolution convertHookResolution(NsfGoolStateHookStatus status) {
    switch (status) {
    case NsfGoolStateHookStatus::InvalidDataIndex: return GoolIRHookResolution::InvalidDataIndex;
    case NsfGoolStateHookStatus::MissingTarget: return GoolIRHookResolution::MissingTarget;
    case NsfGoolStateHookStatus::AmbiguousTarget: return GoolIRHookResolution::AmbiguousTarget;
    case NsfGoolStateHookStatus::TargetUndecoded: return GoolIRHookResolution::TargetUndecoded;
    case NsfGoolStateHookStatus::OffsetOutOfRange: return GoolIRHookResolution::OffsetOutOfRange;
    case NsfGoolStateHookStatus::Local: return GoolIRHookResolution::Local;
    case NsfGoolStateHookStatus::External: return GoolIRHookResolution::External;
    case NsfGoolStateHookStatus::Absent: default: return GoolIRHookResolution::Absent;
    }
}

const char* gteCommandMnemonic(std::uint8_t command) {
    switch (command) {
    case 0x01U: return "rtps";
    case 0x06U: return "nclip";
    case 0x0CU: return "op";
    case 0x10U: return "dpcs";
    case 0x11U: return "intpl";
    case 0x12U: return "mvmva";
    case 0x13U: return "ncds";
    case 0x14U: return "cdp";
    case 0x16U: return "ncdt";
    case 0x1BU: return "nccs";
    case 0x1CU: return "cc";
    case 0x1EU: return "ncs";
    case 0x20U: return "nct";
    case 0x28U: return "sqr";
    case 0x29U: return "dcpl";
    case 0x2AU: return "dpct";
    case 0x2DU: return "avsz3";
    case 0x2EU: return "avsz4";
    case 0x30U: return "rtpt";
    case 0x3DU: return "gpf";
    case 0x3EU: return "gpl";
    case 0x3FU: return "ncct";
    default: return nullptr;
    }
}

GoolIRMipsEffects analyzeMipsEffects(const GoolIRMipsFields& fields) {
    GoolIRMipsEffects effects;
    const auto read = [&effects](std::uint8_t reg) {
        effects.gprReadMask |= (std::uint32_t{1} << reg);
    };
    const auto write = [&effects](std::uint8_t reg) {
        // Writes to $zero are architecturally discarded.
        if (reg != 0U) effects.gprWriteMask |= (std::uint32_t{1} << reg);
    };
    const auto readRsRt = [&]() { read(fields.rs); read(fields.rt); };
    const auto readCop2Data = [&effects](std::uint8_t reg) {
        effects.coprocessor2DataReadMask |= (std::uint32_t{1} << reg);
        // ORGB is a read-only packed view of the three IR color components.
        if (reg == 29U) {
            effects.coprocessor2DataReadMask |= (std::uint32_t{1} << 9U) |
                (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U);
        } else if (reg == 15U) {
            // SXYP is a read-only mirror of the newest SXY FIFO entry (SXY2).
            effects.coprocessor2DataReadMask |= (std::uint32_t{1} << 14U);
        }
    };
    const auto writeCop2Data = [&effects](std::uint8_t reg) {
        effects.coprocessor2DataWriteMask |= (std::uint32_t{1} << reg);
        // IRGB expands packed RGB into IR1..IR3; LZCS updates the LZCR result.
        if (reg == 28U) {
            effects.coprocessor2DataWriteMask |= (std::uint32_t{1} << 9U) |
                (std::uint32_t{1} << 10U) | (std::uint32_t{1} << 11U);
        } else if (reg == 30U) {
            effects.coprocessor2DataWriteMask |= (std::uint32_t{1} << 31U);
        } else if (reg == 15U) {
            // Writing SXYP pushes a new value and shifts SXY1/SXY2 through
            // the three-entry FIFO; SXY0, SXY1, and SXY2 are all modified.
            effects.coprocessor2DataWriteMask |= (std::uint32_t{1} << 12U) |
                (std::uint32_t{1} << 13U) | (std::uint32_t{1} << 14U);
        }
    };

    if (fields.primaryOpcode == 0U) {
        switch (fields.function) {
        case 0U: case 2U: case 3U: // sll, srl, sra
            if (fields.rs != 0U) return effects;
            read(fields.rt); write(fields.rd); break;
        case 4U: case 6U: case 7U: // sllv, srlv, srav
            if (fields.shiftAmount != 0U) return effects;
            readRsRt(); write(fields.rd); break;
        case 8U: // jr
            if (fields.rt != 0U || fields.rd != 0U || fields.shiftAmount != 0U) return effects;
            read(fields.rs); effects.controlTransfer = true; break;
        case 9U: // jalr
            if (fields.rt != 0U || fields.shiftAmount != 0U) return effects;
            read(fields.rs); write(fields.rd); effects.controlTransfer = true; break;
        case 12U: case 13U: // syscall, break (exception instructions; code field is opaque)
            break;
        case 16U: // mfhi
            if (fields.rs != 0U || fields.rt != 0U || fields.shiftAmount != 0U) return effects;
            effects.readsHi = true; write(fields.rd); break;
        case 17U: // mthi
            if (fields.rt != 0U || fields.rd != 0U || fields.shiftAmount != 0U) return effects;
            read(fields.rs); effects.writesHi = true; break;
        case 18U: // mflo
            if (fields.rs != 0U || fields.rt != 0U || fields.shiftAmount != 0U) return effects;
            effects.readsLo = true; write(fields.rd); break;
        case 19U: // mtlo
            if (fields.rt != 0U || fields.rd != 0U || fields.shiftAmount != 0U) return effects;
            read(fields.rs); effects.writesLo = true; break;
        case 24U: case 25U: case 26U: case 27U: // mult[u], div[u]
            if (fields.rd != 0U || fields.shiftAmount != 0U) return effects;
            readRsRt(); effects.writesHi = true; effects.writesLo = true; break;
        case 32U: case 33U: case 34U: case 35U: // add[u], sub[u]
        case 36U: case 37U: case 38U: case 39U: // and, or, xor, nor
        case 42U: case 43U: // slt[u]
            if (fields.shiftAmount != 0U) return effects;
            readRsRt(); write(fields.rd); break;
        default: return effects;
        }
    } else {
        switch (fields.primaryOpcode) {
        case 1U: // REGIMM branches
            if (fields.rt != 0U && fields.rt != 1U &&
                fields.rt != 16U && fields.rt != 17U) return effects;
            read(fields.rs);
            if (fields.rt == 16U || fields.rt == 17U) write(31U);
            effects.controlTransfer = true;
            break;
        case 2U: // j
            effects.controlTransfer = true; break;
        case 3U: // jal
            write(31U); effects.controlTransfer = true; break;
        case 4U: case 5U: // beq, bne
            readRsRt(); effects.controlTransfer = true; break;
        case 6U: case 7U: // blez, bgtz
            if (fields.rt != 0U) return effects;
            read(fields.rs); effects.controlTransfer = true; break;
        case 8U: case 9U: case 10U: case 11U:
        case 12U: case 13U: case 14U: // immediate ALU operations
            read(fields.rs); write(fields.rt); break;
        case 15U: // lui
            if (fields.rs != 0U) return effects;
            write(fields.rt); break;
        case 16U: // COP0 transfers, branches, and R3000A RFE
            if (fields.rs == 0U) { // mfc0 rt, rd
                if (fields.shiftAmount != 0U || fields.function != 0U) return effects;
                effects.coprocessor0ReadMask |= (std::uint32_t{1} << fields.rd);
                write(fields.rt);
            } else if (fields.rs == 4U) { // mtc0 rd, rt
                if (fields.shiftAmount != 0U || fields.function != 0U) return effects;
                read(fields.rt);
                effects.coprocessor0WriteMask |= (std::uint32_t{1} << fields.rd);
            } else if (fields.rs == 8U) { // bc0f / bc0t
                if (fields.rt > 1U) return effects;
                effects.readsCoprocessor0Condition = true;
                effects.controlTransfer = true;
            } else if (fields.rs == 16U && fields.rt == 0U &&
                       fields.rd == 0U && fields.shiftAmount == 0U &&
                       fields.function == 16U) { // rfe: restore previous status mode
                effects.coprocessor0ReadMask |= (std::uint32_t{1} << 12U); // Status
                effects.coprocessor0WriteMask |= (std::uint32_t{1} << 12U);
            } else {
                return effects;
            }
            break;
        case 18U: // PlayStation COP2/GTE register transfers, branches, and commands
            if (fields.rs == 16U && fields.gteCommand.decoded) {
                // Only model commands whose complete register dependencies are
                // independently documented here. Other known GTE mnemonics stay
                // structurally decoded but semantically unknown.
                const auto dataRead = [&effects](std::uint8_t reg) {
                    effects.coprocessor2DataReadMask |= (std::uint32_t{1} << reg);
                };
                const auto dataWrite = [&effects](std::uint8_t reg) {
                    effects.coprocessor2DataWriteMask |= (std::uint32_t{1} << reg);
                };
                const auto controlRead = [&effects](std::uint8_t reg) {
                    effects.coprocessor2ControlReadMask |= (std::uint32_t{1} << reg);
                };
                const auto controlWrite = [&effects](std::uint8_t reg) {
                    effects.coprocessor2ControlWriteMask |= (std::uint32_t{1} << reg);
                };
                switch (fields.gteCommand.command) {
                case 0x12U: { // MVMVA: selector-dependent matrix x vector + translation -> MAC1..MAC3, IR1..IR3, FLAG
                    const auto& selectors = fields.gteCommand;
                    // Matrix selector 3 is the documented hardware-quirk matrix;
                    // leave it unknown until its nonstandard inputs are modeled.
                    if (selectors.matrix > 2U) return effects;
                    switch (selectors.matrix) {
                    case 0U: // rotation matrix
                        controlRead(0U); controlRead(1U); controlRead(2U);
                        controlRead(3U); controlRead(4U);
                        break;
                    case 1U: // light matrix
                        controlRead(8U); controlRead(9U); controlRead(10U);
                        controlRead(11U); controlRead(12U);
                        break;
                    case 2U: // color matrix
                        controlRead(16U); controlRead(17U); controlRead(18U);
                        controlRead(19U); controlRead(20U);
                        break;
                    default: return effects;
                    }
                    switch (selectors.vector) {
                    case 0U: dataRead(0U); dataRead(1U); break; // V0
                    case 1U: dataRead(2U); dataRead(3U); break; // V1
                    case 2U: dataRead(4U); dataRead(5U); break; // V2
                    case 3U: dataRead(9U); dataRead(10U); dataRead(11U); break; // IR1..IR3
                    default: return effects;
                    }
                    switch (selectors.translation) {
                    case 0U: // translation vector
                        controlRead(5U); controlRead(6U); controlRead(7U);
                        break;
                    case 1U: // background color
                        controlRead(13U); controlRead(14U); controlRead(15U);
                        break;
                    case 2U: // far color
                        controlRead(21U); controlRead(22U); controlRead(23U);
                        break;
                    case 3U: // zero vector
                        break;
                    default: return effects;
                    }
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    controlWrite(31U);
                    break;
                }
                case 0x01U: // RTPS: V0, rotation matrix, translation, projection and depth-cue parameters -> projected FIFOs
                    dataRead(0U); dataRead(1U);
                    controlRead(0U); controlRead(1U); controlRead(2U);
                    controlRead(3U); controlRead(4U);
                    controlRead(5U); controlRead(6U); controlRead(7U);
                    controlRead(24U); controlRead(25U); controlRead(26U);
                    controlRead(27U); controlRead(28U);
                    dataWrite(8U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(12U); dataWrite(13U); dataWrite(14U);
                    dataWrite(16U); dataWrite(17U); dataWrite(18U); dataWrite(19U);
                    dataWrite(24U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x30U: // RTPT: V0..V2, rotation matrix, translation, projection and depth-cue parameters -> projected FIFOs
                    dataRead(0U); dataRead(1U); dataRead(2U); dataRead(3U);
                    dataRead(4U); dataRead(5U);
                    controlRead(0U); controlRead(1U); controlRead(2U);
                    controlRead(3U); controlRead(4U);
                    controlRead(5U); controlRead(6U); controlRead(7U);
                    controlRead(24U); controlRead(25U); controlRead(26U);
                    controlRead(27U); controlRead(28U);
                    dataWrite(8U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(12U); dataWrite(13U); dataWrite(14U);
                    dataWrite(16U); dataWrite(17U); dataWrite(18U); dataWrite(19U);
                    dataWrite(24U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x06U: // NCLIP: SXY0..SXY2 -> MAC0, FLAG
                    dataRead(12U); dataRead(13U); dataRead(14U);
                    dataWrite(24U);
                    controlWrite(31U);
                    break;
                case 0x0CU: // OP: IR1..IR3 cross product with RT11, RT22, RT33 -> MAC1..MAC3, IR1..IR3, FLAG
                    dataRead(9U); dataRead(10U); dataRead(11U);
                    controlRead(0U); controlRead(2U); controlRead(4U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x10U: // DPCS: RGBC, IR0, far color -> MAC1..MAC3, IR1..IR3, RGB FIFO, FLAG
                    dataRead(6U); dataRead(8U);
                    controlRead(21U); controlRead(22U); controlRead(23U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x11U: // INTPL: IR1..IR3, IR0, far color -> MAC1..MAC3, IR1..IR3, RGB FIFO, FLAG
                    dataRead(6U); dataRead(8U); dataRead(9U); dataRead(10U); dataRead(11U);
                    controlRead(21U); controlRead(22U); controlRead(23U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x29U: // DCPL: RGBC, IR0, IR1..IR3, far color -> MAC1..MAC3, IR1..IR3, RGB FIFO, FLAG
                    dataRead(6U); dataRead(8U); dataRead(9U); dataRead(10U); dataRead(11U);
                    controlRead(21U); controlRead(22U); controlRead(23U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x2AU: // DPCT: RGB FIFO, RGBC code, IR0, far color -> MAC1..MAC3, IR1..IR3, RGB FIFO, FLAG
                    dataRead(6U); dataRead(8U);
                    dataRead(20U); dataRead(21U); dataRead(22U);
                    controlRead(21U); controlRead(22U); controlRead(23U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x1BU: // NCCS: V0 normal, light matrix, background, color matrix and RGBC -> lit color outputs
                    dataRead(0U); dataRead(1U); dataRead(6U);
                    controlRead(8U); controlRead(9U); controlRead(10U);
                    controlRead(11U); controlRead(12U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x13U: // NCDS: NCS lighting plus RGBC modulation and IR0/far-color depth cue
                    dataRead(0U); dataRead(1U); dataRead(6U); dataRead(8U);
                    controlRead(8U); controlRead(9U); controlRead(10U);
                    controlRead(11U); controlRead(12U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    controlRead(21U); controlRead(22U); controlRead(23U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x16U: // NCDT: NCDS triple-vector pass over V0..V2
                    dataRead(0U); dataRead(1U); dataRead(2U); dataRead(3U);
                    dataRead(4U); dataRead(5U); dataRead(6U); dataRead(8U);
                    controlRead(8U); controlRead(9U); controlRead(10U);
                    controlRead(11U); controlRead(12U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    controlRead(21U); controlRead(22U); controlRead(23U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x20U: // NCT: NCS triple-vector lighting pass over V0..V2
                    dataRead(0U); dataRead(1U); dataRead(2U); dataRead(3U);
                    dataRead(4U); dataRead(5U);
                    controlRead(8U); controlRead(9U); controlRead(10U);
                    controlRead(11U); controlRead(12U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x3FU: // NCCT: NCCS triple-vector lighting and RGBC modulation, without depth cue
                    dataRead(0U); dataRead(1U); dataRead(2U); dataRead(3U);
                    dataRead(4U); dataRead(5U); dataRead(6U);
                    controlRead(8U); controlRead(9U); controlRead(10U);
                    controlRead(11U); controlRead(12U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x1EU: // NCS: V0 normal, light matrix, background and color matrix -> lit color outputs (no RGBC input)
                    dataRead(0U); dataRead(1U);
                    controlRead(8U); controlRead(9U); controlRead(10U);
                    controlRead(11U); controlRead(12U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x1CU: // CC: RGBC, IR1..IR3, color matrix and background -> MAC1..MAC3, IR1..IR3, RGB FIFO, FLAG
                    dataRead(6U); dataRead(9U); dataRead(10U); dataRead(11U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x14U: // CDP: CC color inputs plus IR0 and far color -> depth-cued color outputs
                    dataRead(6U); dataRead(8U); dataRead(9U); dataRead(10U); dataRead(11U);
                    controlRead(13U); controlRead(14U); controlRead(15U);
                    controlRead(16U); controlRead(17U); controlRead(18U);
                    controlRead(19U); controlRead(20U);
                    controlRead(21U); controlRead(22U); controlRead(23U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x28U: // SQR: IR1..IR3 -> MAC1..MAC3, IR1..IR3, FLAG
                    dataRead(9U); dataRead(10U); dataRead(11U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    controlWrite(31U);
                    break;
                case 0x2DU: // AVSZ3: SZ1..SZ3, ZSF3 -> MAC0, OTZ, FLAG
                    dataRead(17U); dataRead(18U); dataRead(19U);
                    controlRead(29U);
                    dataWrite(24U); dataWrite(7U);
                    controlWrite(31U);
                    break;
                case 0x2EU: // AVSZ4: SZ0..SZ3, ZSF4 -> MAC0, OTZ, FLAG
                    dataRead(16U); dataRead(17U); dataRead(18U); dataRead(19U);
                    controlRead(30U);
                    dataWrite(24U); dataWrite(7U);
                    controlWrite(31U);
                    break;
                case 0x3DU: // GPF: IR0, IR1..IR3 -> MAC1..MAC3, IR1..IR3, RGB FIFO, FLAG
                    dataRead(6U); dataRead(8U); dataRead(9U); dataRead(10U); dataRead(11U);
                    dataRead(21U); dataRead(22U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                case 0x3EU: // GPL: previous MAC1..MAC3 plus IR0, IR1..IR3 -> same outputs as GPF
                    dataRead(6U); dataRead(8U); dataRead(9U); dataRead(10U); dataRead(11U);
                    dataRead(21U); dataRead(22U);
                    dataRead(25U); dataRead(26U); dataRead(27U);
                    dataWrite(9U); dataWrite(10U); dataWrite(11U);
                    dataWrite(20U); dataWrite(21U); dataWrite(22U);
                    dataWrite(25U); dataWrite(26U); dataWrite(27U);
                    controlWrite(31U);
                    break;
                default:
                    return effects;
                }
            } else if (fields.rs == 0U || fields.rs == 2U ||
                fields.rs == 4U || fields.rs == 6U) {
                // MFC2/CFC2/MTC2/CTC2 reserve bits 10..0.
                if (fields.shiftAmount != 0U || fields.function != 0U) return effects;
                if (fields.rs == 0U) { // mfc2 rt, rd
                    readCop2Data(fields.rd);
                    write(fields.rt);
                } else if (fields.rs == 2U) { // cfc2 rt, rd
                    effects.coprocessor2ControlReadMask |= (std::uint32_t{1} << fields.rd);
                    write(fields.rt);
                } else if (fields.rs == 4U) { // mtc2 rt, rd
                    read(fields.rt);
                    writeCop2Data(fields.rd);
                } else { // ctc2 rt, rd
                    read(fields.rt);
                    effects.coprocessor2ControlWriteMask |= (std::uint32_t{1} << fields.rd);
                }
            } else if (fields.rs == 8U) { // bc2f / bc2t
                if (fields.rt > 1U) return effects;
                effects.readsCoprocessor2Condition = true;
                effects.controlTransfer = true;
            } else {
                // COP2 command words (rs=16) and other operations need
                // command-specific GTE semantics; retain them as unknown.
                return effects;
            }
            break;
        case 32U: case 33U: case 34U: case 35U:
        case 36U: case 37U: case 38U: // byte/half/word loads and unaligned loads
            read(fields.rs);
            if (fields.primaryOpcode == 34U || fields.primaryOpcode == 38U)
                read(fields.rt); // merge with the old destination value
            write(fields.rt);
            effects.memory = GoolIRMipsMemoryEffect::Read;
            break;
        case 40U: case 41U: case 42U: case 43U: case 46U: // stores
            readRsRt();
            effects.memory = GoolIRMipsMemoryEffect::Write;
            break;
        case 50U: // LWC2 rt, offset(rs): load a word into GTE data register rt
            read(fields.rs);
            writeCop2Data(fields.rt);
            effects.memory = GoolIRMipsMemoryEffect::Read;
            break;
        case 58U: // SWC2 rt, offset(rs): store GTE data register rt
            read(fields.rs);
            readCop2Data(fields.rt);
            effects.memory = GoolIRMipsMemoryEffect::Write;
            break;
        default: return effects;
        }
    }

    effects.known = true;
    if (effects.memory == GoolIRMipsMemoryEffect::Unknown)
        effects.memory = GoolIRMipsMemoryEffect::None;
    return effects;
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
            if (fields.primaryOpcode == 18U && fields.rs == 16U) {
                // COP2 command words use a GTE-specific 25-bit layout rather
                // than the ordinary COP2 register-transfer fields.
                auto& gte = fields.gteCommand;
                gte.decoded = true;
                gte.ignoredCommandBits = static_cast<std::uint8_t>((word >> 20U) & 0x1FU);
                gte.shiftFraction = ((word >> 19U) & 1U) != 0U;
                gte.matrix = static_cast<std::uint8_t>((word >> 17U) & 0x3U);
                gte.vector = static_cast<std::uint8_t>((word >> 15U) & 0x3U);
                gte.translation = static_cast<std::uint8_t>((word >> 13U) & 0x3U);
                gte.reservedBits11To12 = static_cast<std::uint8_t>((word >> 11U) & 0x3U);
                gte.limitMode = ((word >> 10U) & 1U) != 0U;
                gte.reservedBits6To9 = static_cast<std::uint8_t>((word >> 6U) & 0xFU);
                gte.command = static_cast<std::uint8_t>(word & 0x3FU);
                if (const char* mnemonic = gteCommandMnemonic(gte.command)) {
                    gte.commandKnown = true;
                    gte.mnemonic = mnemonic;
                }
            }
            if (fields.primaryOpcode == 0U) fields.format = GoolIRMipsFormat::Register;
            else if (fields.primaryOpcode == 2U || fields.primaryOpcode == 3U)
                fields.format = GoolIRMipsFormat::Jump;
            else fields.format = GoolIRMipsFormat::Immediate;
            ir.nativeFieldsDecoded = true;
            ir.nativeEffects = analyzeMipsEffects(fields);
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
    result.hookDispatches.reserve(source.stateHookReferences.size());
    for (const auto& hook : source.stateHookReferences) {
        result.stateHooks.push_back({hook.descriptorIndex, static_cast<std::uint8_t>(hook.kind),
            static_cast<std::uint8_t>(hook.status), hook.rawValue, hook.instructionOffset,
            hook.external, hook.targetFound, hook.targetChunkIndex, hook.targetEntryIndex});
        result.hookDispatches.push_back({hook.descriptorIndex, convertHookKind(hook.kind),
            hook.rawValue, hook.instructionOffset, hook.external,
            convertHookResolution(hook.status), hook.targetFound,
            hook.targetChunkIndex, hook.targetEntryIndex});
    }
    // Discover routines from semantic entry points, then record their exits and
    // calls from the same generic control-flow graph. No object names or offsets
    // participate in this analysis.
    struct RoutineCandidate {
        std::size_t instruction = 0;
        bool codeHook = false;
        bool localCall = false;
    };
    std::vector<RoutineCandidate> candidates;
    const auto addCandidate = [&candidates](std::size_t instruction, bool codeHook, bool localCall) {
        auto found = std::find_if(candidates.begin(), candidates.end(),
            [instruction](const auto& item) { return item.instruction == instruction; });
        if (found == candidates.end()) candidates.push_back({instruction, codeHook, localCall});
        else {
            found->codeHook = found->codeHook || codeHook;
            found->localCall = found->localCall || localCall;
        }
    };
    for (const auto& hook : result.hookDispatches) {
        if (hook.kind == GoolIRHookKind::Code &&
            hook.resolution == GoolIRHookResolution::Local &&
            hook.instructionOffset < result.instructions.size()) {
            addCandidate(hook.instructionOffset, true, false);
        }
    }
    for (const auto& operation : result.controlOperations) {
        if (operation.kind == GoolIRControlOperationKind::Call &&
            !operation.external && operation.targetInstructionValid) {
            addCandidate(operation.targetInstruction, false, true);
        }
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.instruction < b.instruction; });
    for (const auto& candidate : candidates)
        result.routineEntries.push_back({candidate.instruction, candidate.codeHook, candidate.localCall});

    const std::size_t instructionCount = result.instructions.size();
    result.instructionRoutineMembership.assign(instructionCount, 0U);
    struct Successor {
        std::size_t target = 0;
        GoolIRFlowEdgeKind kind = GoolIRFlowEdgeKind::Fallthrough;
    };
    std::vector<std::vector<Successor>> blockSuccessors(result.basicBlocks.size());
    for (const auto& edge : result.flowEdges) {
        if (!edge.targetValid || edge.sourceBlock >= blockSuccessors.size() ||
            edge.targetInstruction >= instructionCount) continue;
        if (edge.kind == GoolIRFlowEdgeKind::Call ||
            edge.kind == GoolIRFlowEdgeKind::ExternalCall ||
            edge.kind == GoolIRFlowEdgeKind::StateTransition ||
            edge.kind == GoolIRFlowEdgeKind::StateTransitionTrue ||
            edge.kind == GoolIRFlowEdgeKind::StateTransitionFalse ||
            edge.kind == GoolIRFlowEdgeKind::Return) continue;
        blockSuccessors[edge.sourceBlock].push_back({edge.targetInstruction, edge.kind});
    }
    for (auto& successors : blockSuccessors) {
        std::sort(successors.begin(), successors.end(), [](const auto& a, const auto& b) {
            if (a.target != b.target) return a.target < b.target;
            return a.kind < b.kind;
        });
        successors.erase(std::unique(successors.begin(), successors.end(),
            [](const auto& a, const auto& b) { return a.target == b.target && a.kind == b.kind; }),
            successors.end());
    }

    for (const auto& candidate : candidates) {
        GoolIRRoutine routine;
        routine.entryInstruction = candidate.instruction;
        routine.fromCodeHook = candidate.codeHook;
        routine.fromLocalCall = candidate.localCall;
        struct PendingInstruction {
            std::size_t instruction = 0;
            std::size_t sourceInstruction = 0;
            GoolIRFlowEdgeKind incomingKind = GoolIRFlowEdgeKind::Fallthrough;
        };
        std::vector<bool> visited(instructionCount, false);
        std::vector<PendingInstruction> pending{{candidate.instruction, 0U,
            GoolIRFlowEdgeKind::Fallthrough}};
        while (!pending.empty()) {
            const auto next = pending.back();
            pending.pop_back();
            const auto current = next.instruction;
            if (current >= instructionCount || visited[current]) continue;
            if (current != candidate.instruction &&
                std::any_of(candidates.begin(), candidates.end(), [current](const auto& other) {
                    return other.instruction == current;
                })) {
                routine.entryPointBoundaries.push_back(current);
                routine.exits.push_back({GoolIRRoutineExitKind::EntryPointBoundary,
                    next.sourceInstruction, current, true, next.incomingKind,
                    "Control flow reaches another discovered routine entry point."});
                continue;
            }
            visited[current] = true;
            routine.instructions.push_back(current);
            const auto blockIndex = current < result.instructionToBlock.size()
                ? result.instructionToBlock[current] : result.basicBlocks.size();
            if (blockIndex >= result.basicBlocks.size()) continue;
            const auto& block = result.basicBlocks[blockIndex];
            if (current + 1U < block.endInstruction) {
                pending.push_back({current + 1U, current, GoolIRFlowEdgeKind::Fallthrough});
            } else {
                for (const auto& successor : blockSuccessors[blockIndex])
                    pending.push_back({successor.target, block.endInstruction - 1U,
                        successor.kind});
            }
        }
        std::sort(routine.instructions.begin(), routine.instructions.end());
        std::sort(routine.entryPointBoundaries.begin(), routine.entryPointBoundaries.end());
        routine.entryPointBoundaries.erase(std::unique(routine.entryPointBoundaries.begin(),
            routine.entryPointBoundaries.end()), routine.entryPointBoundaries.end());

        // Record explicit return and invalid-branch exits only when their source
        // instruction is reachable from this routine's entry.
        for (const auto instruction : routine.instructions) {
            const auto blockIndex = result.instructionToBlock[instruction];
            for (const auto& edge : result.flowEdges) {
                if (edge.sourceBlock != blockIndex) continue;
                const auto sourceInstruction = result.basicBlocks[blockIndex].endInstruction - 1U;
                if (sourceInstruction != instruction) continue;
                if (edge.kind == GoolIRFlowEdgeKind::Return) {
                    routine.exits.push_back({GoolIRRoutineExitKind::Return,
                        sourceInstruction, 0U, false, edge.kind,
                        "Routine returns to its caller; the return destination is dynamic."});
                } else if (!edge.targetValid &&
                    (edge.kind == GoolIRFlowEdgeKind::Branch ||
                     edge.kind == GoolIRFlowEdgeKind::BranchTrue ||
                     edge.kind == GoolIRFlowEdgeKind::BranchFalse)) {
                    routine.exits.push_back({GoolIRRoutineExitKind::InvalidBranchTarget,
                        sourceInstruction, 0U, false, edge.kind, edge.diagnostic});
                }
            }
        }

        // A sequential or conditional path that reaches the end of the decoded
        // instruction stream is an incomplete boundary, not an implicit RET.
        if (!routine.instructions.empty() &&
            routine.instructions.back() + 1U == instructionCount) {
            const auto last = routine.instructions.back();
            const auto opcode = result.instructions[last].opcode;
            if (opcode != 49U && opcode != 50U) {
                routine.exits.push_back({GoolIRRoutineExitKind::EndOfProgram,
                    last, 0U, false, GoolIRFlowEdgeKind::Fallthrough,
                    "Execution can fall beyond the decoded GOOL instruction stream."});
            }
        }

        for (const auto& operation : result.controlOperations) {
            if (operation.kind != GoolIRControlOperationKind::Call ||
                !std::binary_search(routine.instructions.begin(), routine.instructions.end(),
                    operation.sourceInstruction)) continue;
            GoolIRRoutineCall call;
            call.sourceInstruction = operation.sourceInstruction;
            call.targetInstruction = operation.targetInstruction;
            call.targetInstructionValid = operation.targetInstructionValid;
            call.external = operation.external;
            if (!call.external && call.targetInstructionValid) {
                const auto target = std::find_if(candidates.begin(), candidates.end(),
                    [&call](const auto& entry) { return entry.instruction == call.targetInstruction; });
                if (target != candidates.end()) {
                    call.targetRoutineIndex = static_cast<std::size_t>(std::distance(candidates.begin(), target));
                    call.targetRoutineFound = true;
                }
            }
            routine.calls.push_back(call);
        }

        for (const auto instruction : routine.instructions) {
            ++result.instructionRoutineMembership[instruction];
            if (result.instructionRoutineMembership[instruction] > 1U)
                routine.hasSharedInstructions = true;
        }
        result.routines.push_back(std::move(routine));
    }
    for (std::size_t instruction = 0; instruction < instructionCount; ++instruction) {
        if (result.instructionRoutineMembership[instruction] == 0U)
            result.unassignedInstructions.push_back(instruction);
    }
    for (auto& routine : result.routines) {
        for (const auto instruction : routine.instructions) {
            if (result.instructionRoutineMembership[instruction] > 1U)
                routine.sharedInstructions.push_back(instruction);
        }
        routine.hasSharedInstructions = !routine.sharedInstructions.empty();
        if (routine.hasSharedInstructions)
            routine.diagnostics.push_back("Some reachable instructions are shared with another discovered entry point.");
        if (!routine.entryPointBoundaries.empty())
            routine.diagnostics.push_back("Control flow reaches another discovered entry point; traversal stops there to preserve separate routine ownership.");
        if (std::any_of(routine.exits.begin(), routine.exits.end(), [](const auto& exit) {
                return exit.kind == GoolIRRoutineExitKind::InvalidBranchTarget ||
                       exit.kind == GoolIRRoutineExitKind::EndOfProgram;
            }))
            routine.diagnostics.push_back("At least one execution path has an unresolved or incomplete routine boundary.");
    }

    const auto dataFlow = analyzeGoolDataFlow(source);
    result.valueUses.reserve(dataFlow.uses.size());
    for (const auto& use : dataFlow.uses) {
        GoolIRValueUse irUse;
        irUse.instructionIndex = use.instructionIndex;
        irUse.location = use.location;
        irUse.kind = use.kind == NsfGoolDataFlowUseKind::SingleReachingDefinition
            ? GoolIRValueOriginKind::SingleReachingDefinition
            : use.kind == NsfGoolDataFlowUseKind::MultipleReachingDefinitions
                ? GoolIRValueOriginKind::MultipleReachingDefinitions
                : GoolIRValueOriginKind::NoInStreamDefinition;
        irUse.mayHaveUnmodeledValue = use.mayHaveUnmodeledValue;
        for (const auto definitionId : use.reachingDefinitions) {
            if (definitionId < dataFlow.definitions.size())
                irUse.reachingDefinitionInstructions.push_back(dataFlow.definitions[definitionId].instructionIndex);
        }
        result.valueUses.push_back(std::move(irUse));
    }
    result.frameGroups.reserve(source.frameGroups.size());
    for (const auto& group : source.frameGroups) {
        result.frameGroups.push_back({group.type, group.frameCount, group.eid,
            group.byteOffset, group.byteSize, group.partCount, group.font, group.interpolated});
    }
    return result;
}

}
