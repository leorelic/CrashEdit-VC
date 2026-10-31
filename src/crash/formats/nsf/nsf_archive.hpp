#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace CrashEdit::Crash {
inline constexpr std::size_t NsfChunkSize=65536;
static_assert(NsfChunkSize == 64U * 1024U, "NSF chunks are exactly 64 KiB.");
struct NsfItem { std::size_t offset=0, size=0; };
struct NsfEntityProperty {
 std::int16_t id=0; std::uint8_t rawType=0, type=0, elementSize=0; std::int16_t rowCount=0;
 bool last=false, sparse=false, hasMetaValues=false, valid=false;
 std::size_t dataOffset=0, dataSize=0;
 std::vector<std::int32_t> intValues; std::vector<std::size_t> valueOffsets;
 struct Position { std::int16_t x=0,y=0,z=0; };
 std::vector<Position> positions;
 std::string error;
};
struct NsfEntitySetting { std::uint8_t valueA=0; std::int32_t valueB=0; std::uint32_t raw=0; std::size_t dataOffset=0; };
struct NsfEntity {
 std::size_t itemIndex=0; bool valid=false; std::uint32_t declaredLength=0;
 bool hasCameraIndex=false,hasCameraSubIndex=false; std::int32_t cameraIndex=0,cameraSubIndex=0;
 bool hasId=false,hasAlternateId=false,hasGoolType=false,hasSubtype=false;
 std::int32_t id=0,alternateId=0,goolType=0,subtype=0;
 std::string name;
 std::vector<NsfEntityProperty::Position> positions;
 std::vector<NsfEntitySetting> settings;
 std::vector<std::int32_t> victims;
 bool hasDdaSettings=false,hasDdaSection=false,hasBoxCount=false;
 std::int32_t ddaDeathCount=0,ddaSection=0,boxCount=0;
 std::vector<NsfEntityProperty> properties; std::string error;
};
struct NsfZone {
 bool decoded=false, isCrash3Retail=false; std::size_t headerSize=0;
 std::int32_t worldCount=0, infoCount=0, cameraCount=0, entityCount=0, zoneCount=0, zoneFlags=0, music=0;
 std::vector<std::int32_t> worlds, zones, zoneLinkTypes;
 std::int32_t x=0,y=0,z=0,width=0,height=0,depth=0;
 std::uint16_t collisionDepthX=0,collisionDepthY=0,collisionDepthZ=0;
 std::vector<NsfEntity> entities; std::string error;
};
struct NsfEntry {
 std::uint32_t eid=0, type=0, itemCount=0;
 std::size_t offset=0, size=0;
 bool valid=false; std::string error;
 std::vector<NsfItem> items; NsfZone zone;
};
struct NsfChunk {
 std::vector<std::uint8_t> bytes;
 std::int32_t chunkId=0; std::uint16_t type=0; bool wasCompressed=false;
 std::vector<NsfEntry> entries; bool entryTableValid=false; std::string entryTableError;
};
class NsfArchive {
public:
 bool load(const std::filesystem::path&,std::string&); bool loadBytes(const std::vector<std::uint8_t>&,std::string&);
 bool saveUncompressed(const std::filesystem::path&,std::string&) const;
 const std::vector<NsfChunk>& chunks()const noexcept{return chunks_;}
 std::size_t compressedChunkCount()const noexcept;
 void decodeZones(bool crash3Retail);
 bool setEntityPropertyInt(std::size_t chunkIndex,std::size_t entryIndex,std::size_t entityIndex,std::uint16_t propertyId,std::size_t valueIndex,std::int32_t value);
private:
 static bool decodeChunk(const std::vector<std::uint8_t>&,std::size_t&,NsfChunk&,std::string&);
 std::vector<NsfChunk> chunks_;
};
}
