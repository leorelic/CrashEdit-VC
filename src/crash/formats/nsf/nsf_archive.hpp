#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace CrashEdit::Crash {
inline constexpr std::size_t NsfChunkSize=65536;
static_assert(NsfChunkSize == 64U * 1024U, "NSF chunks are exactly 64 KiB.");
struct NsfEntry {
 std::uint32_t eid=0, type=0, itemCount=0;
 std::size_t offset=0, size=0;
 bool valid=false;
 std::string error;
};
struct NsfChunk {
 std::vector<std::uint8_t> bytes;
 std::int32_t chunkId=0;
 std::uint16_t type=0;
 bool wasCompressed=false;
 std::vector<NsfEntry> entries;
 bool entryTableValid=false;
 std::string entryTableError;
};
class NsfArchive {
public:
 bool load(const std::filesystem::path&,std::string&); bool loadBytes(const std::vector<std::uint8_t>&,std::string&);
 bool saveUncompressed(const std::filesystem::path&,std::string&) const;
 const std::vector<NsfChunk>& chunks()const noexcept{return chunks_;}
 std::size_t compressedChunkCount()const noexcept;
private:
 static bool decodeChunk(const std::vector<std::uint8_t>&,std::size_t&,NsfChunk&,std::string&);
 std::vector<NsfChunk> chunks_;
};
}
