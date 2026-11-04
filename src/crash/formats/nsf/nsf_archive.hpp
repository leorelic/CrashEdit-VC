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
struct NsfSceneryVertex { std::int32_t x=0,y=0,z=0; std::uint16_t colorIndex=0; };
struct NsfSceneryTriangle { std::uint16_t a=0,b=0,c=0; std::uint16_t textureIndex=0; bool animated=false; };
struct NsfSceneryQuad { std::uint16_t a=0,b=0,c=0,d=0; std::uint16_t textureIndex=0; std::uint8_t unknown=0; bool animated=false; };
struct NsfSceneryTexture {
 std::uint8_t u1=0,v1=0,u2=0,v2=0,u3=0,v3=0,u4=0,v4=0;
 std::uint8_t clutX=0,clutY1=0,clutY2=0,colorMode=0,blendMode=0,segment=0,page=0;
};
struct NsfSceneryColor { std::uint8_t r=0,g=0,b=0,extra=0; };
struct NsfScenery {
 bool decoded=false,isCrash3=false,isSky=false;
 std::int32_t xOffset=0,yOffset=0,zOffset=0;
 std::vector<std::uint32_t> texturePageEids;
 std::vector<NsfSceneryVertex> vertices;
 std::vector<NsfSceneryTriangle> triangles;
 std::vector<NsfSceneryQuad> quads;
 std::vector<NsfSceneryTexture> textures;
 std::vector<NsfSceneryColor> colors;
 std::vector<std::uint32_t> animatedTextures;
 std::string error;
};
enum class NsfGoolVersion : std::uint8_t { Version0, Version1, Version2, Version3 };
struct NsfGoolInstruction {
 std::size_t byteOffset=0;
 std::uint32_t word=0;
 std::uint8_t opcode=0;
 std::uint32_t operandBits=0;
 bool isMips=false, mipsKnown=false;
 std::string mnemonic, arguments;
};
struct NsfGoolStateDescriptor {
 std::int32_t stateFlags=0, blockFlags=0;
 std::int16_t goolIndex=0, eventHook=0, transHook=0, codeHook=0;
};
enum class NsfGoolStateReferenceStatus : std::uint8_t { InvalidDataIndex, Local, External, Ambiguous };
enum class NsfGoolStateHookKind : std::uint8_t { Event, Transition, Code };
enum class NsfGoolStateHookStatus : std::uint8_t { Absent, InvalidDataIndex, MissingTarget, AmbiguousTarget, TargetUndecoded, OffsetOutOfRange, Local, External };
struct NsfGoolStateHookReference {
 std::size_t descriptorIndex=0, targetChunkIndex=0, targetEntryIndex=0;
 NsfGoolStateHookKind kind=NsfGoolStateHookKind::Event;
 std::uint16_t rawValue=0, instructionOffset=0;
 bool external=false, targetFound=false;
 NsfGoolStateHookStatus status=NsfGoolStateHookStatus::Absent;
};
struct NsfGoolStateReference {
 std::size_t descriptorIndex=0, targetChunkIndex=0, targetEntryIndex=0;
 std::int32_t dataPoolIndex=0;
 std::uint32_t eid=0;
 bool targetFound=false;
 NsfGoolStateReferenceStatus status=NsfGoolStateReferenceStatus::InvalidDataIndex;
};
struct NsfGoolFrameGroup {
 std::uint16_t type=0;
 std::int16_t frameCount=0;
 std::uint32_t eid=0;
 std::size_t byteOffset=0, byteSize=0;
 std::uint32_t partCount=0;
 std::int32_t font=0;
 bool interpolated=false;
};
struct NsfGool {
 bool decoded=false;
 NsfGoolVersion version=NsfGoolVersion::Version2;
 std::int32_t id=0, classId=0, format=0, heapBase=0, eventCount=0, entryCount=0;
 std::vector<NsfGoolInstruction> instructions;
 std::vector<std::int32_t> dataPool;
 std::vector<std::int16_t> stateMap;
 std::vector<NsfGoolStateDescriptor> stateDescriptors;
 std::vector<NsfGoolStateReference> stateReferences;
 std::vector<NsfGoolStateHookReference> stateHookReferences;
 std::size_t frameGroupBytes=0;
 bool frameGroupsDecoded=false;
 std::vector<NsfGoolFrameGroup> frameGroups;
 std::string frameGroupError;
 std::string error;
};
struct NsfEntry {
 std::uint32_t eid=0, type=0, itemCount=0;
 std::size_t offset=0, size=0;
 bool valid=false; std::string error;
 std::vector<NsfItem> items; NsfZone zone; NsfScenery scenery; NsfGool gool;
};
struct NsfEntryLocation { std::size_t chunkIndex=0, entryIndex=0; };
struct NsfChunk {
 std::vector<std::uint8_t> bytes;
 std::int32_t chunkId=0; std::uint16_t type=0; bool wasCompressed=false;
 std::vector<NsfEntry> entries; bool entryTableValid=false; std::string entryTableError;
};
class NsfArchive {
public:
 bool load(const std::filesystem::path&,std::string&); bool loadBytes(const std::vector<std::uint8_t>&,std::string&);
 bool saveUncompressed(const std::filesystem::path&,std::string&) const;
 bool saveUncompressedBytes(std::vector<std::uint8_t>&,std::string&) const;
 const std::vector<NsfChunk>& chunks()const noexcept{return chunks_;}
 std::vector<NsfEntryLocation> findEntriesByEid(std::uint32_t eid) const;
 std::size_t compressedChunkCount()const noexcept;
 void decodeZones(bool crash3Retail); void decodeGools(NsfGoolVersion version);
 bool setEntityPropertyInt(std::size_t chunkIndex,std::size_t entryIndex,std::size_t entityIndex,std::uint16_t propertyId,std::size_t valueIndex,std::int32_t value);
 bool setGoolStateDescriptor(std::size_t chunkIndex,std::size_t entryIndex,std::size_t descriptorIndex,const NsfGoolStateDescriptor& descriptor);
 bool setGoolDataPoolWord(std::size_t chunkIndex,std::size_t entryIndex,std::size_t wordIndex,std::uint32_t value);
 bool setGoolFrameGroupEid(std::size_t chunkIndex,std::size_t entryIndex,std::size_t groupIndex,std::uint32_t eid);
 bool setGoolFrameGroupFrameCount(std::size_t chunkIndex,std::size_t entryIndex,std::size_t groupIndex,std::uint16_t frameCount);
 bool setGoolFrameGroupInterpolated(std::size_t chunkIndex,std::size_t entryIndex,std::size_t groupIndex,bool interpolated);
private:
 static bool decodeChunk(const std::vector<std::uint8_t>&,std::size_t&,NsfChunk&,std::string&);
 std::vector<NsfChunk> chunks_;
};
}
