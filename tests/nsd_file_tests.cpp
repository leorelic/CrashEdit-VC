#include "crash/formats/nsd/nsd_file.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
void writeI32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::int32_t value) {
    const auto v = static_cast<std::uint32_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(v & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((v >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((v >> 24U) & 0xFFU);
}

bool check(bool condition, const char* message) {
    if (!condition) std::cerr << "FAIL: " << message << '\\n';
    return condition;
}
}

int main() {
    using CrashEdit::Crash::NsdFile;
    using CrashEdit::Crash::NsdVariant;
    bool ok = true;

    // ProtoNSD: 256 hash keys, counts, then 8-byte (chunk ID, entry ID) links.
    std::vector<std::uint8_t> proto(0x408 + 8, 0);
    writeI32(proto, 0x400, 3);
    writeI32(proto, 0x404, 1);
    writeI32(proto, 0x408, 7);
    writeI32(proto, 0x40C, 0x1234);
    NsdFile parsed;
    std::string error;
    ok &= check(parsed.loadBytes(proto, NsdVariant::Proto, error), "ProtoNSD should parse");
    ok &= check(parsed.index.size() == 1 && parsed.index[0].chunkId == 7 && parsed.index[0].entryId == 0x1234,
                "ProtoNSD index link should be decoded");

    // Standard NSD with one spawn record; offsets mirror NSD.Load in the original C# source.
    std::vector<std::uint8_t> standard(0x6FC + 24 + 3, 0);
    writeI32(standard, 0x400, 5);
    writeI32(standard, 0x404, 0);
    writeI32(standard, 0x520, 1); // spawn count
    writeI32(standard, 0x528, 42); // NSD ID
    writeI32(standard, 0x52C, 9); // entity count
    writeI32(standard, 0x418, 0x111); // uncompressed chunk section
    writeI32(standard, 0x41C, 0x222); // prelude count
    writeI32(standard, 0x420, 0x333); // first compressed chunk info
    writeI32(standard, 0x524, 0); // reserved blank value
    standard[0x630] = 0xA5; // opaque extra data
    standard[0x6FC + 24] = 0x5A; // trailing image data
    writeI32(standard, 0x6FC, 0x77); // spawn zone EID
    writeI32(standard, 0x700, 2);
    writeI32(standard, 0x704, 3);
    writeI32(standard, 0x708, 100);
    writeI32(standard, 0x70C, 200);
    writeI32(standard, 0x710, 300);
    ok &= check(parsed.loadBytes(standard, NsdVariant::Standard, error), "standard NSD should parse");
    ok &= check(parsed.id == 42 && parsed.entityCount == 9, "standard NSD header fields should parse");
    ok &= check(parsed.spawns.size() == 1 && parsed.spawns[0].zoneEid == 0x77 && parsed.spawns[0].z == 300,
                "standard NSD spawn record should parse");
    ok &= check(parsed.validationWarnings().empty(), "valid standard NSD should have no validation warnings");
    auto nonzeroBlank = standard;
    writeI32(nonzeroBlank, 0x524, 0x444);
    NsdFile warningFile;
    ok &= check(warningFile.loadBytes(nonzeroBlank, NsdVariant::Standard, error) &&
                warningFile.blank == 0 && !warningFile.validationWarnings().empty(),
                "nonzero standard NSD reserved value should warn and normalize to zero");

    std::vector<std::uint8_t> crash3(0x7FC + 24, 0);
    writeI32(crash3, 0x400, 8);
    writeI32(crash3, 0x404, 0);
    writeI32(crash3, 0x520, 1);
    writeI32(crash3, 0x7FC, 0x88);
    ok &= check(parsed.loadBytes(crash3, NsdVariant::Crash3, error), "Crash 3 NSD should parse");
    ok &= check(parsed.goolMap.size() == 128 && parsed.spawns.size() == 1,
                "Crash 3 NSD should use the 128-entry GOOL map and its later spawn offset");

    std::vector<std::uint8_t> oldNsd(0x688, 0);
    writeI32(oldNsd, 0x400, 2);
    writeI32(oldNsd, 0x404, 0);
    writeI32(oldNsd, 0x524, 99);
    writeI32(oldNsd, 0x528, 11);
    writeI32(oldNsd, 0x52C, 12);
    ok &= check(parsed.loadBytes(oldNsd, NsdVariant::Old, error), "OldNSD should parse");
    ok &= check(parsed.id == 99 && parsed.startZone == 11 && parsed.camera == 12,
                "OldNSD legacy header fields should parse");
    ok &= check(parsed.magic == 1, "OldNSD invalid magic should be normalized to 1 like CrashEdit");
    ok &= check(!parsed.validationWarnings().empty(), "OldNSD should report a nonstandard magic value as a warning");

    // Serialization should round-trip all NSD variants and relocate variable-length sections safely.
    std::vector<std::uint8_t> saved;
    ok &= check(parsed.saveBytes(saved, error), "OldNSD should serialize");
    NsdFile oldRoundTrip;
    ok &= check(oldRoundTrip.loadBytes(saved, NsdVariant::Old, error) &&
                oldRoundTrip.id == parsed.id && oldRoundTrip.startZone == parsed.startZone &&
                oldRoundTrip.extraData == parsed.extraData,
                "OldNSD serialization should preserve legacy fields and trailing bytes");

    ok &= check(parsed.loadBytes(proto, NsdVariant::Proto, error), "ProtoNSD should reload for serialization");
    ok &= check(parsed.saveBytes(saved, error) && saved == proto,
                "ProtoNSD serialization should preserve its complete byte sequence");

    ok &= check(parsed.loadBytes(standard, NsdVariant::Standard, error), "standard NSD should reload for serialization");
    ok &= check(parsed.saveBytes(saved, error) && saved == standard,
                "unchanged standard NSD should round-trip byte-for-byte");
    parsed.spawns.push_back({0x99, 3, 4, 500, 600, 700});
    ok &= check(parsed.saveBytes(saved, error), "standard NSD should serialize after adding a spawn");
    NsdFile standardRoundTrip;
    ok &= check(standardRoundTrip.loadBytes(saved, NsdVariant::Standard, error) &&
                standardRoundTrip.spawns.size() == 2 && standardRoundTrip.spawns[1].zoneEid == 0x99 &&
                standardRoundTrip.imageData == parsed.imageData && standardRoundTrip.extraData == parsed.extraData,
                "standard NSD serialization should resize spawn records and retain opaque sections");

    ok &= check(parsed.loadBytes(crash3, NsdVariant::Crash3, error), "Crash 3 NSD should reload for serialization");
    ok &= check(parsed.saveBytes(saved, error) && saved == crash3,
                "unchanged Crash 3 NSD should round-trip byte-for-byte");

    std::vector<std::uint8_t> truncated(0x6FB, 0);
    ok &= check(!parsed.loadBytes(truncated, NsdVariant::Standard, error), "truncated standard NSD should fail");
    std::cout << (ok ? "All NSD tests passed.\\n" : "NSD tests failed.\\n");
    return ok ? 0 : 1;
}
