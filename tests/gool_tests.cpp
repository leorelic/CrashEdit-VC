#include "crash/formats/nsf/nsf_archive.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace CrashEdit::Crash;
static void p16(std::vector<std::uint8_t>&b,std::size_t p,std::uint16_t v){b[p]=static_cast<std::uint8_t>(v);b[p+1]=static_cast<std::uint8_t>(v>>8U);}
static void p32(std::vector<std::uint8_t>&b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b[p+static_cast<std::size_t>(i)]=static_cast<std::uint8_t>(v>>(8U*static_cast<unsigned>(i)));}

static std::vector<std::uint8_t> makeGoolChunk(bool extended){
 std::vector<std::uint8_t>b(NsfChunkSize);
 p16(b,0,0x1234);p16(b,2,0);p32(b,4,0x12345678);p32(b,8,1);
 const std::size_t entry=24;
 const std::size_t count=extended?6:3;
 const std::size_t tableSize=16+(count+1)*4;
 const std::size_t sizesExtended[]={24,8,8,4,16,4};
 const std::size_t sizesBasic[]={24,8,8};
 std::size_t offsets[7]{};offsets[0]=tableSize;
 for(std::size_t i=0;i<count;++i)offsets[i+1]=offsets[i]+(extended?sizesExtended[i]:sizesBasic[i]);
 p32(b,16,static_cast<std::uint32_t>(entry));p32(b,20,static_cast<std::uint32_t>(entry+offsets[count]));
 p32(b,entry,0x0100ffff);p32(b,entry+4,0x0200007b);p32(b,entry+8,11);p32(b,entry+12,static_cast<std::uint32_t>(count));
 for(std::size_t i=0;i<=count;++i)p32(b,entry+16+i*4,static_cast<std::uint32_t>(offsets[i]));
 const std::size_t header=entry+offsets[0];
 p32(b,header,0x10203040);p32(b,header+4,0x55667788);p32(b,header+8,extended?1:0);
 p32(b,header+12,0x100);p32(b,header+16,4);p32(b,header+20,2);
 const std::size_t code=entry+offsets[1];p32(b,code,0x12345678);p32(b,code+4,0xABCDEF01);
 const std::size_t data=entry+offsets[2];p32(b,data,0x11112222);p32(b,data+4,0x33334444);
 if(extended){
  const std::size_t map=entry+offsets[3];p16(b,map,0xffff);p16(b,map+2,3);
  const std::size_t desc=entry+offsets[4];p32(b,desc,0x01020304);p32(b,desc+4,0x05060708);
  p16(b,desc+8,9);p16(b,desc+10,10);p16(b,desc+12,11);p16(b,desc+14,12);
  p32(b,entry+offsets[5],0xDEADBEEF);
 }
 return b;
}

int main(){
 std::string error;
 NsfArchive archive;
 assert(archive.loadBytes(makeGoolChunk(true),error));
 archive.decodeGools(NsfGoolVersion::Version2);
 const auto&entry=archive.chunks()[0].entries[0];
 assert(entry.valid&&entry.type==11&&entry.gool.decoded);
 const auto&g=entry.gool;
 assert(g.version==NsfGoolVersion::Version2);
 assert(static_cast<std::uint32_t>(g.id)==0x10203040U);
 assert(static_cast<std::uint32_t>(g.classId)==0x55667788U);
 assert(g.format==1&&g.heapBase==0x100&&g.eventCount==4&&g.entryCount==2);
 assert(g.instructions.size()==2);
 assert(g.instructions[0].byteOffset==0&&g.instructions[0].word==0x12345678U);
 assert(g.instructions[0].opcode==0x12&&g.instructions[0].operandBits==0x345678U);
 assert(g.instructions[1].opcode==0xAB&&g.instructions[1].operandBits==0xCDEF01U);
 assert(g.dataPool.size()==2&&static_cast<std::uint32_t>(g.dataPool[1])==0x33334444U);
 assert(g.stateMap.size()==2&&g.stateMap[0]==-1&&g.stateMap[1]==3);
 assert(g.stateDescriptors.size()==1&&g.stateDescriptors[0].stateFlags==0x01020304);
 assert(g.stateDescriptors[0].blockFlags==0x05060708&&g.stateDescriptors[0].goolIndex==9);
 assert(g.stateDescriptors[0].eventHook==10&&g.stateDescriptors[0].transHook==11&&g.stateDescriptors[0].codeHook==12);
 assert(g.frameGroupBytes==4);

 assert(archive.loadBytes(makeGoolChunk(false),error));
 archive.decodeGools(NsfGoolVersion::Version2);
 const auto&basic=archive.chunks()[0].entries[0].gool;
 assert(basic.decoded&&basic.format==0&&basic.instructions.size()==2&&basic.dataPool.size()==2);
 std::cout<<"GOOL entry and instruction-word decoding checks passed\\n";
}
