#pragma once

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

// Parsed metadata/index layer. This does not yet decode NSF entries or render zones.
struct NsdFile {
    NsdVariant variant = NsdVariant::Standard;
    std::int32_t chunkCount = 0;
    std::vector<std::int32_t> hashKeyMap;
    std::vector<NsdLink> index;
    std::vector<std::int32_t> levelData;
    std::vector<std::int32_t> goolMap;
    std::vector<NsdSpawnPoint> spawns;
    std::int32_t id = 0;
    std::int32_t entityCount = 0;
    std::int32_t startZone = 0;
    std::int32_t camera = 0;
    std::size_t trailingDataBytes = 0;

    bool load(const std::filesystem::path& file, NsdVariant format, std::string& error);
    bool loadBytes(const std::vector<std::uint8_t>& bytes, NsdVariant format, std::string& error);
};

} // namespace CrashEdit::Crash
