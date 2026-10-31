#include "crash/formats/nsd/nsd_file.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <limits>
#include <system_error>

namespace CrashEdit::Crash {
namespace {

bool rangeFits(std::size_t size, std::size_t offset, std::size_t count) {
    return offset <= size && count <= size - offset;
}

void writeI32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::int32_t value) {
    const auto v = static_cast<std::uint32_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(v & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((v >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((v >> 24U) & 0xFFU);
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

bool NsdFile::saveBytes(std::vector<std::uint8_t>& bytes, std::string& error) const {
    if (originalData.empty()) { error = "Cannot save an NSD that has not been loaded."; return false; }
    if (hashKeyMap.size() != 256) { error = "NSD hash-key map must contain 256 entries."; return false; }

    const std::size_t indexOffset = variant == NsdVariant::Proto ? 0x408U : 0x520U;
    if (index.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        error = "NSD index contains too many entries."; return false;
    }
    const std::size_t newIndexBytes = index.size() * 8U;
    const std::size_t oldEntryCount = static_cast<std::size_t>(readI32(originalData, 0x404));
    const std::size_t oldIndexBytes = oldEntryCount * 8U;
    if (!rangeFits(originalData.size(), indexOffset, oldIndexBytes)) {
        error = "Original NSD index is invalid; refusing to save."; return false;
    }

    std::vector<std::uint8_t> output;
    output.insert(output.end(), originalData.begin(), originalData.begin() + static_cast<std::ptrdiff_t>(indexOffset));
    output.resize(indexOffset + newIndexBytes);
    for (std::size_t i = 0; i < hashKeyMap.size(); ++i) writeI32(output, i * 4, hashKeyMap[i]);
    writeI32(output, 0x400, chunkCount);
    writeI32(output, 0x404, static_cast<std::int32_t>(index.size()));
    for (std::size_t i = 0; i < index.size(); ++i) {
        writeI32(output, indexOffset + i * 8, index[i].chunkId);
        writeI32(output, indexOffset + i * 8 + 4, index[i].entryId);
    }

    const std::size_t oldTailOffset = indexOffset + oldIndexBytes;
    const std::size_t newTailOffset = indexOffset + newIndexBytes;
    if (variant == NsdVariant::Proto) {
        output.insert(output.end(), originalData.begin() + static_cast<std::ptrdiff_t>(oldTailOffset), originalData.end());
        bytes = std::move(output);
        error.clear();
        return true;
    }

    if (levelData.size() != 4 || compressedChunkInfo.size() != 64) {
        error = "NSD level/compression metadata has an invalid size."; return false;
    }
    const std::size_t fixedTailSize = variant == NsdVariant::Old ? 0x114U :
        (variant == NsdVariant::Crash3 ? 0x2DCU : 0x1DCU);
    if (!rangeFits(originalData.size(), oldTailOffset, fixedTailSize)) {
        error = "Original NSD metadata is truncated; refusing to save."; return false;
    }
    std::vector<std::uint8_t> tail(originalData.begin() + static_cast<std::ptrdiff_t>(oldTailOffset),
        originalData.begin() + static_cast<std::ptrdiff_t>(oldTailOffset + fixedTailSize));

    if (variant == NsdVariant::Old) {
        if (goolMap.size() != 64) { error = "Old NSD GOOL map must contain 64 entries."; return false; }
        writeI32(tail, 0, magic);
        writeI32(tail, 4, id);
        writeI32(tail, 8, startZone);
        writeI32(tail, 12, camera);
        writeI32(tail, 16, unknown);
        for (std::size_t i = 0; i < goolMap.size(); ++i) writeI32(tail, 20 + i * 4, goolMap[i]);
        output.insert(output.end(), tail.begin(), tail.end());
        const std::size_t oldExtraOffset = 0x634U + oldIndexBytes;
        if (!rangeFits(originalData.size(), oldExtraOffset, originalData.size() - oldExtraOffset)) {
            error = "Old NSD trailing data is invalid."; return false;
        }
        output.insert(output.end(), originalData.begin() + static_cast<std::ptrdiff_t>(oldExtraOffset), originalData.end());
    } else {
        const std::size_t goolCount = variant == NsdVariant::Crash3 ? 128U : 64U;
        if (goolMap.size() != goolCount) { error = "NSD GOOL map has an invalid size for this variant."; return false; }
        if (spawns.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
            error = "NSD contains too many spawn points."; return false;
        }
        const std::size_t oldSpawnOffset = variant == NsdVariant::Crash3 ? 0x7FCU + oldIndexBytes : 0x6FCU + oldIndexBytes;
        if (!rangeFits(originalData.size(), oldSpawnOffset, 0)) {
            error = "Original NSD spawn offset is invalid."; return false;
        }
        writeI32(tail, 0, static_cast<std::int32_t>(spawns.size()));
        writeI32(tail, 4, blank);
        writeI32(tail, 8, id);
        writeI32(tail, 12, entityCount);
        for (std::size_t i = 0; i < goolMap.size(); ++i) writeI32(tail, 16 + i * 4, goolMap[i]);
        output.insert(output.end(), tail.begin(), tail.end());

        for (const auto& spawn : spawns) {
            const std::size_t offset = output.size();
            output.resize(offset + 24);
            writeI32(output, offset, spawn.zoneEid);
            writeI32(output, offset + 4, spawn.camera);
            writeI32(output, offset + 8, spawn.point);
            writeI32(output, offset + 12, spawn.x);
            writeI32(output, offset + 16, spawn.y);
            writeI32(output, offset + 20, spawn.z);
        }
        output.insert(output.end(), imageData.begin(), imageData.end());
    }

    bytes = std::move(output);
    error.clear();
    return true;
}

bool NsdFile::save(const std::filesystem::path& file, std::string& error) const {
    std::vector<std::uint8_t> bytes;
    if (!saveBytes(bytes, error)) return false;
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream) { error = "Could not open NSD for writing: " + file.string(); return false; }
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) { error = "Could not write the complete NSD file: " + file.string(); return false; }
    error.clear();
    return true;
}

} // namespace CrashEdit::Crash
