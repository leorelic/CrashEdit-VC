#include "crash/formats/nsf/nsf_archive.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace CrashEdit::Crash;

#define REQUIRE(condition) do { if(!(condition)){ std::cerr << "Sample compatibility check failed at " << __FILE__ << ":" << __LINE__ << ": " << #condition << "\\n"; std::abort(); } } while(false)

static void put16(std::vector<std::uint8_t>&bytes,std::size_t offset,std::uint16_t value){
 bytes[offset]=static_cast<std::uint8_t>(value);bytes[offset+1U]=static_cast<std::uint8_t>(value>>8U);
}
static void put32(std::vector<std::uint8_t>&bytes,std::size_t offset,std::uint32_t value){
 for(unsigned i=0;i<4U;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8U));
}
static std::vector<std::uint8_t>readFile(const std::filesystem::path&path){
 std::ifstream input(path,std::ios::binary);REQUIRE(static_cast<bool>(input));
 return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),{});
}
static std::vector<std::uint8_t>makeArchive(const std::vector<std::uint8_t>&model,const std::vector<std::uint8_t>&animation){
 constexpr std::size_t chunkSize=65536U,entryTableEnd=28U;
 REQUIRE(entryTableEnd+model.size()+animation.size()<=chunkSize);
 std::vector<std::uint8_t>bytes(chunkSize,0);
 put16(bytes,0,0x1234U);put16(bytes,2,0U);put32(bytes,4,0U);put32(bytes,8,2U);
 const std::size_t animationOffset=entryTableEnd+model.size(),endOffset=animationOffset+animation.size();
 put32(bytes,16,static_cast<std::uint32_t>(entryTableEnd));
 put32(bytes,20,static_cast<std::uint32_t>(animationOffset));
 put32(bytes,24,static_cast<std::uint32_t>(endOffset));
 std::copy(model.begin(),model.end(),bytes.begin()+static_cast<std::ptrdiff_t>(entryTableEnd));
 std::copy(animation.begin(),animation.end(),bytes.begin()+static_cast<std::ptrdiff_t>(animationOffset));
 return bytes;
}
static const NsfEntry*findEntry(const NsfArchive&archive,std::uint32_t eid,std::uint32_t type){
 for(const auto&chunk:archive.chunks())for(const auto&entry:chunk.entries)if(entry.valid&&entry.eid==eid&&entry.type==type)return &entry;
 return nullptr;
}
static std::uint64_t hashByte(std::uint64_t hash,std::uint8_t value){
 hash^=value;return hash*1099511628211ULL;
}
static std::uint64_t hashInt32(std::uint64_t hash,std::int32_t value){
 const auto raw=static_cast<std::uint32_t>(value);
 for(unsigned i=0;i<4U;++i)hash=hashByte(hash,static_cast<std::uint8_t>(raw>>(i*8U)));
 return hash;
}
static std::uint64_t triangleHash(const NsfModel&model){
 std::uint64_t hash=14695981039346656037ULL;
 for(const auto&triangle:model.triangles){
  hash=hashInt32(hash,triangle.a);hash=hashInt32(hash,triangle.b);hash=hashInt32(hash,triangle.c);
  hash=hashByte(hash,triangle.colorA);hash=hashByte(hash,triangle.colorB);hash=hashByte(hash,triangle.colorC);
  hash=hashByte(hash,triangle.textureIndex);hash=hashByte(hash,triangle.triangleType);hash=hashByte(hash,triangle.triangleSubtype);
  hash=hashByte(hash,triangle.animated?1U:0U);
 }
 return hash;
}
static std::uint64_t frameVertexHash(const NsfAnimationFrame&frame){
 std::uint64_t hash=14695981039346656037ULL;
 for(const auto&vertex:frame.vertices){
  REQUIRE(vertex.x>=0&&vertex.x<=255&&vertex.y>=0&&vertex.y<=255&&vertex.z>=0&&vertex.z<=255);
  hash=hashByte(hash,static_cast<std::uint8_t>(vertex.x));
  hash=hashByte(hash,static_cast<std::uint8_t>(vertex.y));
  hash=hashByte(hash,static_cast<std::uint8_t>(vertex.z));
 }
 return hash;
}
static std::vector<std::uint8_t>rawEntryBytes(const NsfArchive&archive,const NsfEntry&entry){
 const auto&bytes=archive.chunks().front().bytes;
 REQUIRE(entry.offset<=bytes.size()&&entry.size<=bytes.size()-entry.offset);
 return std::vector<std::uint8_t>(bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset),bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset+entry.size));
}
static void validatePair(const std::filesystem::path&directory,
 const char*modelFile,const char*animationFile,std::uint32_t modelEid,std::uint32_t animationEid,
 std::uint32_t vertexCount,std::uint32_t polygonCount,std::size_t frameCount,
 std::uint32_t textureCount,std::uint32_t colorCount,std::uint32_t extendedCount,std::size_t pageCount,
 std::uint64_t expectedTriangleHash,const std::vector<std::uint64_t>&expectedFrameHashes){
 const auto modelBytes=readFile(directory/modelFile),animationBytes=readFile(directory/animationFile);
 NsfArchive archive;std::string error;REQUIRE(archive.loadBytes(makeArchive(modelBytes,animationBytes),error));
 archive.decodeAnimationModels(false);
 const NsfEntry*modelEntry=findEntry(archive,modelEid,2U);const NsfEntry*animationEntry=findEntry(archive,animationEid,1U);
 REQUIRE(modelEntry&&animationEntry);
 const NsfModel&model=modelEntry->model;const NsfAnimation&animation=animationEntry->animation;
 REQUIRE(model.decoded&&model.polygonsDecoded&&model.error.empty()&&model.polygonError.empty());
 REQUIRE(model.compressed&&model.vertexCount==vertexCount&&model.polygonCount==polygonCount);
 const std::uint64_t actualTriangleHash=triangleHash(model);
 if(model.triangles.size()!=polygonCount||actualTriangleHash!=expectedTriangleHash){
  std::cerr<<"Model "<<std::hex<<modelEid<<std::dec<<" triangle count "<<model.triangles.size()<<"/"<<polygonCount
           <<" hash 0x"<<std::hex<<actualTriangleHash<<" expected 0x"<<expectedTriangleHash<<std::dec<<"\\n";
  for(std::size_t i=0;i<std::min<std::size_t>(12,model.triangles.size());++i){
   const auto&t=model.triangles[i];
   std::cerr<<i<<": "<<t.a<<","<<t.b<<","<<t.c<<" colors "<<static_cast<int>(t.colorA)<<","<<static_cast<int>(t.colorB)<<","<<static_cast<int>(t.colorC)
            <<" tex "<<static_cast<int>(t.textureIndex)<<" type "<<static_cast<int>(t.triangleType)<<" sub "<<static_cast<int>(t.triangleSubtype)<<" anim "<<t.animated<<"\\n";
  }
 }
 REQUIRE(model.triangles.size()==polygonCount&&actualTriangleHash==expectedTriangleHash);
 REQUIRE(model.textures.size()==textureCount&&model.colors.size()==colorCount);
 REQUIRE(model.extendedTextures.size()==extendedCount&&model.texturePageEids.size()==pageCount);
 REQUIRE(animation.decoded&&animation.error.empty()&&animation.frames.size()==frameCount);
 REQUIRE(expectedFrameHashes.size()==frameCount);
 for(std::size_t i=0;i<frameCount;++i){
  const auto&frame=animation.frames[i];
  REQUIRE(frame.verticesDecoded&&frame.modelResolved&&frame.modelEid==modelEid);
  REQUIRE(frame.vertexCount==vertexCount&&frame.vertices.size()==vertexCount);
  REQUIRE(frameVertexHash(frame)==expectedFrameHashes[i]);
 }
 const auto originalModelBytes=rawEntryBytes(archive,*modelEntry);
 const auto originalAnimationBytes=rawEntryBytes(archive,*animationEntry);
 std::vector<std::uint8_t>saved;REQUIRE(archive.saveUncompressedBytes(saved,error));
 NsfArchive roundTrip;REQUIRE(roundTrip.loadBytes(saved,error));roundTrip.decodeAnimationModels(false);
 const NsfEntry*roundModel=findEntry(roundTrip,modelEid,2U);const NsfEntry*roundAnimation=findEntry(roundTrip,animationEid,1U);
 REQUIRE(roundModel&&roundAnimation);
 REQUIRE(rawEntryBytes(roundTrip,*roundModel)==originalModelBytes);
 REQUIRE(rawEntryBytes(roundTrip,*roundAnimation)==originalAnimationBytes);
 REQUIRE(triangleHash(roundModel->model)==expectedTriangleHash);
 REQUIRE(roundAnimation->animation.frames.size()==expectedFrameHashes.size());
 for(std::size_t i=0;i<expectedFrameHashes.size();++i)REQUIRE(frameVertexHash(roundAnimation->animation.frames[i])==expectedFrameHashes[i]);
}

int main(int argc,char**argv){
 REQUIRE(argc==2);
 const std::filesystem::path directory=argv[1];
 validatePair(directory,"entry_8_2_El1eG.bin","entry_8_3_El1eV.bin",
  0x50A82755U,0x50A82773U,38U,64U,7U,25U,15U,0U,3U,
  0xF5261495EC15B3E0ULL,
  {0xEBBEED295F11D0F9ULL,0xC98BC31ED73CB88AULL,0xD09BD3CC51FFEFF6ULL,0xD96DB2B13C90D8C5ULL,
   0x072FAC1C9BFE36B0ULL,0x1B9AC4FB740946CEULL,0x91047EE7D89DC84CULL});
 validatePair(directory,"entry_7_3_Cr10G.bin","entry_14_3_Cr10V.bin",
  0x4CD82055U,0x4CD82073U,309U,574U,11U,917U,90U,14U,1U,
  0xC8501FC3FFDB579FULL,
  {0x09E91ACC26AD9A04ULL,0x04095291B09E2A5EULL,0x4A7C0F7C22E0AA45ULL,0xAECABEFF2933AFECULL,
   0x1598E289F09248E5ULL,0xC1B030A121AE9001ULL,0x9BED2F3675DF9EACULL,0x1954AA9B4087B1AEULL,
   0x6C9754A7F294418AULL,0x2E32745505EB07C6ULL,0xECE3F6CA9011A1FBULL});
 return 0;
}
