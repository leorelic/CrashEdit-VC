#include "crash/formats/nsd/nsd_file.hpp"

#include <fstream>
#include <limits>

namespace CrashEdit::Crash {
namespace {

bool rangeFits(std::size_t size, std::size_t offset, std::size_t count) {
    return offset <= size && count <= size - offset;
}

std::int32_t readI32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    const std::uint32_t value = static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    return static_cast<std::int32_t>(value);
}

bool checkedArrayBytes(std::int32_t count, std::size_t elementSize, std::size_t& result) {
    if (count < 0 || static_cast<std::uint64_t>(count) >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / elementSize)) return false;
    result = static_cast<std::size_t>(count) * elementSize;
    return true;
}

} // namespace

bool NsdFile::load(const std::filesystem::path& file, NsdVariant format, std::string& error) {
    std::ifstream stream(file, std::ios::binary | std::ios::ate);
    if (!stream) { error = "Could not open NSD file: " + file.string(); return false; }
    const auto end = stream.tellg();
    if (end < 0) { error = "Could not determine NSD file size."; return false; }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        error = "Could not read the complete NSD file."; return false;
    }
    return loadBytes(bytes, format, error);
}

bool NsdFile::loadBytes(const std::vector<std::uint8_t>& bytes, NsdVariant format, std::string& error) {
    const std::size_t base = format == NsdVariant::Proto ? 0x408U :
        (format == NsdVariant::Old ? 0x688U : (format == NsdVariant::Crash3 ? 0x7FCU : 0x6FCU));
    if (bytes.size() < base) { error = "NSD is too short for the selected format."; return false; }

    const std::int32_t chunks = readI32(bytes, 0x400);
    const std::int32_t entries = readI32(bytes, 0x404);
    std::size_t indexBytes = 0;
    if (chunks < 0 || !checkedArrayBytes(entries, 8, indexBytes)) {
        error = "NSD contains a negative or invalid chunk/entry count."; return false;
    }
    const std::size_t indexOffset = format == NsdVariant::Proto ? 0x408U : 0x520U;
    if (!rangeFits(bytes.size(), indexOffset, indexBytes)) { error = "NSD index is truncated."; return false; }

    NsdFile parsed;
    parsed.variant = format;
    parsed.chunkCount = chunks;
    parsed.hashKeyMap.reserve(256);
    for (std::size_t i = 0; i < 256; ++i) parsed.hashKeyMap.push_back(readI32(bytes, i * 4));
    parsed.index.reserve(static_cast<std::size_t>(entries));
    for (std::int32_t i = 0; i < entries; ++i) {
        const std::size_t offset = indexOffset + static_cast<std::size_t>(i) * 8;
        parsed.index.push_back({readI32(bytes, offset), readI32(bytes, offset + 4)});
    }

    if (format != NsdVariant::Proto) {
        for (std::size_t i = 0; i < 4; ++i) parsed.levelData.push_back(readI32(bytes, 0x408 + i * 4));
        const std::size_t tail = 0x520U + indexBytes;
        const std::size_t requiredMetadata = format == NsdVariant::Old ? 0x114U :
            (format == NsdVariant::Crash3 ? 0x2DCU : 0x1DCU);
        if (!rangeFits(bytes.size(), tail, requiredMetadata)) {
            error = "NSD metadata after the index is truncated."; return false;
        }
        if (format == NsdVariant::Old) {
            parsed.id = readI32(bytes, tail + 4);
            parsed.startZone = readI32(bytes, tail + 8);
            parsed.camera = readI32(bytes, tail + 12);
            for (std::size_t i = 0; i < 64; ++i) parsed.goolMap.push_back(readI32(bytes, tail + 20 + i * 4));
            const std::size_t end = 0x634U + indexBytes;
            parsed.trailingDataBytes = bytes.size() - end;
        } else {
            const std::int32_t spawnCount = readI32(bytes, tail);
            if (spawnCount < 0) { error = "NSD spawn-point count is negative."; return false; }
            const std::size_t goolCount = format == NsdVariant::Crash3 ? 128U : 64U;
            const std::size_t goolOffset = tail + 16;
            const std::size_t spawnOffset = base + indexBytes;
            std::size_t spawnBytes = 0;
            if (!checkedArrayBytes(spawnCount, 24, spawnBytes) || !rangeFits(bytes.size(), goolOffset, goolCount * 4) ||
                !rangeFits(bytes.size(), spawnOffset, spawnBytes)) {
                error = "NSD spawn points or GOOL map are truncated."; return false;
            }
            parsed.id = readI32(bytes, tail + 8);
            parsed.entityCount = readI32(bytes, tail + 12);
            for (std::size_t i = 0; i < goolCount; ++i) parsed.goolMap.push_back(readI32(bytes, goolOffset + i * 4));
            for (std::int32_t i = 0; i < spawnCount; ++i) {
                const std::size_t offset = spawnOffset + static_cast<std::size_t>(i) * 24;
                parsed.spawns.push_back({readI32(bytes, offset), readI32(bytes, offset + 4), readI32(bytes, offset + 8),
                    readI32(bytes, offset + 12), readI32(bytes, offset + 16), readI32(bytes, offset + 20)});
            }
            parsed.trailingDataBytes = bytes.size() - (spawnOffset + spawnBytes);
        }
    }

    *this = std::move(parsed);
    return true;
}

} // namespace CrashEdit::Crash
