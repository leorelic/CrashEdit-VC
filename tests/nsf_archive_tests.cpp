#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/eid.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
static void p16(std::vector<std::uint8_t>&b,std::size_t p,std::uint16_t v){b[p]=std::uint8_t(v);b[p+1]=std::uint8_t(v>>8);}
static void p32(std::vector<std::uint8_t>&b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b[p+i]=std::uint8_t(v>>(8*i));}
int main(){using namespace CrashEdit::Crash;assert(eidToEName(0x0200007BU)=="1000Z");assert(eNameToEid("1000Z")==0x0200007BU);assert(isValidEid(0x0200007BU));assert(eidToEName(0x6396347FU)=="NONE!");std::vector<std::uint8_t>b(65536);p16(b,0,0x1234);p16(b,2,0x42);p32(b,4,0x12345678);CrashEdit::Crash::NsfArchive a;std::string e;assert(a.loadBytes(b,e));assert(a.chunks().size()==1);assert(a.chunks()[0].type==0x42);assert(a.chunks()[0].chunkId==0x12345678);std::vector<std::uint8_t>bad={0x34,0x12,0};assert(!a.loadBytes(bad,e));// Normal entry chunk: one entry containing two items. The item offset table has three offsets.
 std::vector<std::uint8_t> normal(65536);p16(normal,0,0x1234);p16(normal,2,0);p32(normal,4,3);p32(normal,8,1);
 p32(normal,16,24);p32(normal,20,60);
 p32(normal,24,0x0100FFFF);p32(normal,28,0xAABBCCDD);p32(normal,32,7);p32(normal,36,2);
 p32(normal,40,28);p32(normal,44,32);p32(normal,48,36);
 p32(normal,52,0x11223344);p32(normal,56,0x55667788);
 assert(a.loadBytes(normal,e));assert(a.chunks()[0].entryTableValid);assert(a.chunks()[0].entries.size()==1);
 assert(a.chunks()[0].entries[0].valid);assert(a.chunks()[0].entries[0].eid==0xAABBCCDD);assert(a.chunks()[0].entries[0].itemCount==2);
 assert(a.chunks()[0].entries[0].items.size()==2);assert(a.chunks()[0].entries[0].items[0].size==4);assert(a.chunks()[0].entries[0].items[1].size==4);
 // Crash 2 zone entry: header, layout, and no entity items.
 std::vector<std::uint8_t> zoneChunk(65536);p16(zoneChunk,0,0x1234);p16(zoneChunk,2,0);p32(zoneChunk,4,4);p32(zoneChunk,8,1);
 constexpr std::size_t zoneEntryStart=24, zoneHeaderSize=0x318, zoneLayoutSize=0x24;
 constexpr std::size_t zoneEntrySize=16+3*4+zoneHeaderSize+zoneLayoutSize;
 p32(zoneChunk,16,zoneEntryStart);p32(zoneChunk,20,zoneEntryStart+zoneEntrySize);
 p32(zoneChunk,zoneEntryStart,0x0100FFFF);p32(zoneChunk,zoneEntryStart+4,0x0200007B);p32(zoneChunk,zoneEntryStart+8,7);p32(zoneChunk,zoneEntryStart+12,2);
 p32(zoneChunk,zoneEntryStart+16,28);p32(zoneChunk,zoneEntryStart+20,28+zoneHeaderSize);p32(zoneChunk,zoneEntryStart+24,zoneEntrySize);
 const std::size_t zoneHeader=zoneEntryStart+28,zoneLayout=zoneHeader+zoneHeaderSize;
 p32(zoneChunk,zoneHeader+0x188,0);p32(zoneChunk,zoneHeader+0x18c,0);p32(zoneChunk,zoneHeader+0x190,0);
 p32(zoneChunk,zoneLayout,100);p32(zoneChunk,zoneLayout+4,200);p32(zoneChunk,zoneLayout+8,300);
 p32(zoneChunk,zoneLayout+12,400);p32(zoneChunk,zoneLayout+16,500);p32(zoneChunk,zoneLayout+20,600);
 assert(a.loadBytes(zoneChunk,e));a.decodeZones(false);
 const auto&zone=a.chunks()[0].entries[0].zone;
 assert(zone.decoded);assert(zone.headerSize==0x318);assert(zone.x==100&&zone.y==200&&zone.z==300);
 assert(zone.width==400&&zone.height==500&&zone.depth==600);assert(zone.entities.empty());
 std::cout<<"NSF raw, 64 KiB chunk, entry-table, and truncation checks passed\\n";}
