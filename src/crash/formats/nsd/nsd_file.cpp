#include "crash/formats/nsd/nsd_file.hpp"

#include <algorithm>
#include <cstddef>
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

std::vector<std::uint8_t> copyRange(const std::vector<std::uint8_t>& bytes,
                                    std::size_t offset, std::size_t count) {
    return {bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + count)};
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
    const std::size_t minimumSize = format == NsdVariant::Proto ? 0x408U :
        (format == NsdVariant::Old ? 0x688U : (format == NsdVariant::Crash3 ? 0x7FCU : 0x6FCU));
    if (bytes.size() < minimumSize) { error = "NSD is too short for the selected format."; return false; }

    const std::int32_t chunks = readI32(bytes, 0x400);
    const std::int32_t entries = readI32(bytes, 0x404);
    std::size_t indexBytes = 0;
    if (chunks < 0 || !checkedArrayBytes(entries, 8, indexBytes)) {
        error = "NSD contains a negative or invalid chunk/entry count."; return false;
    }

    const std::size_t indexOffset = format == NsdVariant::Proto ? 0x408U : 0x520U;
    if (!rangeFits(bytes.size(), indexOffset, indexBytes)) {
        error = "NSD index is truncated."; return false;
    }

    NsdFile parsed;
    parsed.variant = format;
    parsed.chunkCount = chunks;
    parsed.originalData = bytes;
    parsed.hashKeyMap.reserve(256);
    for (std::size_t i = 0; i < 256; ++i) parsed.hashKeyMap.push_back(readI32(bytes, i * 4));
    parsed.index.reserve(static_cast<std::size_t>(entries));
    for (std::int32_t i = 0; i < entries; ++i) {
        const std::size_t offset = indexOffset + static_cast<std::size_t>(i) * 8;
        parsed.index.push_back({readI32(bytes, offset), readI32(bytes, offset + 4)});
    }

    if (format == NsdVariant::Proto) {
        *this = std::move(parsed);
        error.clear();
        return true;
    }

    // Shared chunk-compression metadata used by Old, Standard and Crash 3 NSDs.
    for (std::size_t i = 0; i < 4; ++i) parsed.levelData.push_back(readI32(bytes, 0x408 + i * 4));
    parsed.uncompressedChunkSec = readI32(bytes, 0x418);
    parsed.preludeCount = readI32(bytes, 0x41C);
    parsed.compressedChunkInfo.reserve(64);
    for (std::size_t i = 0; i < 64; ++i)
        parsed.compressedChunkInfo.push_back(readI32(bytes, 0x420 + i * 4));

    const std::size_t tail = 0x520U + indexBytes;
    if (format == NsdVariant::Old) {
        constexpr std::size_t oldMetadataSize = 0x114U;
        if (!rangeFits(bytes.size(), tail, oldMetadataSize)) {
            error = "Old NSD metadata after the index is truncated."; return false;
        }
        parsed.magic = readI32(bytes, tail);
        parsed.id = readI32(bytes, tail + 4);
        parsed.startZone = readI32(bytes, tail + 8);
        parsed.camera = readI32(bytes, tail + 12);
        parsed.unknown = readI32(bytes, tail + 16);
        parsed.goolMap.reserve(64);
        for (std::size_t i = 0; i < 64; ++i)
            parsed.goolMap.push_back(readI32(bytes, tail + 20 + i * 4));

        const std::size_t extraOffset = 0x634U + indexBytes;
        if (!rangeFits(bytes.size(), extraOffset, 0)) {
            error = "Old NSD extra data offset is invalid."; return false;
        }
        parsed.extraData = copyRange(bytes, extraOffset, bytes.size() - extraOffset);
        parsed.trailingDataBytes = parsed.extraData.size();
    } else {
        const std::int32_t spawnCount = readI32(bytes, tail);
        if (spawnCount < 0) { error = "NSD spawn-point count is negative."; return false; }
        parsed.blank = readI32(bytes, tail + 4);
        parsed.id = readI32(bytes, tail + 8);
        parsed.entityCount = readI32(bytes, tail + 12);

        const std::size_t goolCount = format == NsdVariant::Crash3 ? 128U : 64U;
        const std::size_t goolOffset = tail + 16;
        const std::size_t extraOffset = format == NsdVariant::Crash3 ? 0x730U + indexBytes : 0x630U + indexBytes;
        const std::size_t spawnOffset = format == NsdVariant::Crash3 ? 0x7FCU + indexBytes : 0x6FCU + indexBytes;
        std::size_t spawnBytes = 0;
        if (!checkedArrayBytes(spawnCount, 24, spawnBytes) ||
            !rangeFits(bytes.size(), goolOffset, goolCount * 4) ||
            !rangeFits(bytes.size(), extraOffset, 0xCCU) ||
            !rangeFits(bytes.size(), spawnOffset, spawnBytes)) {
            error = "NSD GOOL map, extra data or spawn records are truncated."; return false;
        }

        parsed.goolMap.reserve(goolCount);
        for (std::size_t i = 0; i < goolCount; ++i)
            parsed.goolMap.push_back(readI32(bytes, goolOffset + i * 4));
        parsed.extraData = copyRange(bytes, extraOffset, 0xCCU);

        parsed.spawns.reserve(static_cast<std::size_t>(spawnCount));
        for (std::int32_t i = 0; i < spawnCount; ++i) {
            const std::size_t offset = spawnOffset + static_cast<std::size_t>(i) * 24;
            parsed.spawns.push_back({readI32(bytes, offset), readI32(bytes, offset + 4), readI32(bytes, offset + 8),
                readI32(bytes, offset + 12), readI32(bytes, offset + 16), readI32(bytes, offset + 20)});
        }
        const std::size_t imageOffset = spawnOffset + spawnBytes;
        parsed.imageData = copyRange(bytes, imageOffset, bytes.size() - imageOffset);
        parsed.trailingDataBytes = parsed.imageData.size();
    }

    *this = std::move(parsed);
    error.clear();
    return true;
}

} // namespace CrashEdit::Crash
