#include "crash/formats/nsf/nsf_archive.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
static void p16(std::vector<std::uint8_t>&b,std::size_t p,std::uint16_t v){b[p]=std::uint8_t(v);b[p+1]=std::uint8_t(v>>8);}
static void p32(std::vector<std::uint8_t>&b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b[p+i]=std::uint8_t(v>>(8*i));}
int main(){std::vector<std::uint8_t>b(65536);p16(b,0,0x1234);p16(b,2,0x42);p32(b,4,0x12345678);CrashEdit::Crash::NsfArchive a;std::string e;assert(a.loadBytes(b,e));assert(a.chunks().size()==1);assert(a.chunks()[0].type==0x42);assert(a.chunks()[0].chunkId==0x12345678);std::vector<std::uint8_t>bad={0x34,0x12,0};assert(!a.loadBytes(bad,e));// Normal entry chunk: one entry, one item, all offsets relative to the entry.
 std::vector<std::uint8_t> normal(65536);p16(normal,0,0x1234);p16(normal,2,0);p32(normal,4,3);p32(normal,8,1);
 p32(normal,16,24);p32(normal,20,52);
 p32(normal,24,0x0100FFFF);p32(normal,28,0xAABBCCDD);p32(normal,32,7);p32(normal,36,1);
 p32(normal,40,24);p32(normal,44,28);
 assert(a.loadBytes(normal,e));assert(a.chunks()[0].entryTableValid);assert(a.chunks()[0].entries.size()==1);
 assert(a.chunks()[0].entries[0].valid);assert(a.chunks()[0].entries[0].eid==0xAABBCCDD);assert(a.chunks()[0].entries[0].itemCount==1);
 std::cout<<"NSF raw, 64 KiB chunk, entry-table, and truncation checks passed\\n";}
