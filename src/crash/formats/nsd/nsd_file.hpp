#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include <vector>

namespace CrashEdit::Crash {

enum class NsdVariant { Proto, Old, Standard, Crash3 };

struct NsdLink {
    std::int32_t chunkId = 0;
    std::int32_t entryId = 0;
};

struct NsdSpawnPoint {
    std::int32_t zoneEid = 0;
    std::int32_t camera = 0;
    std::int32_t point = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

// Parsed NSD metadata. originalData retains the exact source bytes so fields
// not yet understood by the C++ port are not lost when the model is extended.
struct NsdFile {
    NsdVariant variant = NsdVariant::Standard;
    std::int32_t chunkCount = 0;
    std::vector<std::int32_t> hashKeyMap;
    std::vector<NsdLink> index;
    std::vector<std::int32_t> levelData;
    std::int32_t uncompressedChunkSec = 0;
    std::int32_t preludeCount = 0;
    std::vector<std::int32_t> compressedChunkInfo;
    std::int32_t blank = 0;
    std::int32_t magic = 0;
    std::int32_t unknown = 0;
    std::vector<std::int32_t> goolMap;
    std::vector<std::uint8_t> extraData;
    std::vector<std::uint8_t> imageData;
    std::vector<NsdSpawnPoint> spawns;
    std::int32_t id = 0;
    std::int32_t entityCount = 0;
    std::int32_t startZone = 0;
    std::int32_t camera = 0;
    std::vector<std::uint8_t> originalData;
    std::size_t trailingDataBytes = 0;

    bool load(const std::filesystem::path& file, NsdVariant format, std::string& error);
    bool loadBytes(const std::vector<std::uint8_t>& bytes, NsdVariant format, std::string& error);
    bool save(const std::filesystem::path& file, std::string& error) const;
    bool saveBytes(std::vector<std::uint8_t>& bytes, std::string& error) const;
    std::vector<std::string> validationWarnings() const;
};

} // namespace CrashEdit::Crash
