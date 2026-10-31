#include "crash/formats/nsf/nsf_archive.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
namespace CrashEdit::Crash {
static std::uint16_t u16(const std::vector<std::uint8_t>&d,std::size_t p){return std::uint16_t(d[p])|std::uint16_t(std::uint16_t(d[p+1])<<8);}
static std::uint32_t u32(const std::vector<std::uint8_t>&d,std::size_t p){return std::uint32_t(d[p])|(std::uint32_t(d[p+1])<<8)|(std::uint32_t(d[p+2])<<16)|(std::uint32_t(d[p+3])<<24);}
static bool has(const std::vector<std::uint8_t>&d,std::size_t p,std::size_t n){return p<=d.size()&&n<=d.size()-p;}
static void checksum(std::vector<std::uint8_t>&d){std::uint32_t c=0x12345678;for(std::size_t i=0;i<NsfChunkSize;++i){if(i<12||i>=16)c+=d[i];c=(c<<3)|(c>>29);}for(int i=0;i<4;++i)d[12+i]=std::uint8_t(c>>(8*i));}
bool NsfArchive::load(const std::filesystem::path&f,std::string&e){
 std::ifstream s(f,std::ios::binary|std::ios::ate);if(!s){e="Could not open NSF file: "+f.string();return false;}
 auto end=s.tellg();if(end<0){e="Could not determine NSF file size.";return false;}
 auto z=static_cast<std::uintmax_t>(end);if(z>std::numeric_limits<std::size_t>::max()){e="NSF file too large.";return false;}
 std::vector<std::uint8_t>b(static_cast<std::size_t>(z));s.seekg(0);if(!b.empty()&&!s.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()))){e="Could not read NSF file.";return false;}return loadBytes(b,e);
}
bool NsfArchive::loadBytes(const std::vector<std::uint8_t>&in,std::string&e){std::vector<NsfChunk> parsed;std::size_t p=0;while(p<in.size()){NsfChunk c;if(!decodeChunk(in,p,c,e))return false;parsed.push_back(std::move(c));}chunks_=std::move(parsed);return true;}
bool NsfArchive::decodeChunk(const std::vector<std::uint8_t>&in,std::size_t&p,NsfChunk&o,std::string&e){
 if(!has(in,p,2)){e="NSF truncated: missing chunk magic.";return false;}auto magic=u16(in,p);o.bytes.assign(NsfChunkSize,0);
 if(magic==0x1234||magic==0){if(!has(in,p,NsfChunkSize)){e="NSF truncated in raw 64 KiB chunk.";return false;}std::copy_n(in.begin()+static_cast<std::ptrdiff_t>(p),NsfChunkSize,o.bytes.begin());p+=NsfChunkSize;}
 else if(magic==0x1235){
  if(!has(in,p,12)){e="NSF truncated in compressed header.";return false;}auto length=u32(in,p+4),skip=u32(in,p+8);if(length>NsfChunkSize||skip>in.size()){e="Invalid compressed chunk length/skip.";return false;}p+=12;std::size_t w=0;
  while(w<length){if(!has(in,p,1)){e="Truncated compressed data.";return false;}auto a=in[p++];if(a&0x80){if(!has(in,p,1)){e="Truncated back-reference.";return false;}auto b=in[p++];std::size_t span=(b&7)==7?64:(b&7)+3;std::size_t dist=(std::size_t(b>>3)|(std::size_t(a&0x7f)<<5));if(!dist||dist>w||span>NsfChunkSize-w){e="Invalid back-reference.";return false;}for(std::size_t i=0;i<span;++i)o.bytes[w+i]=o.bytes[w-dist+i];w+=span;}
  else{std::size_t n=a;if(!n||!has(in,p,n)||n>NsfChunkSize-w){e="Invalid literal run.";return false;}std::copy_n(in.begin()+static_cast<std::ptrdiff_t>(p),n,o.bytes.begin()+static_cast<std::ptrdiff_t>(w));p+=n;w+=n;}}
  if(!has(in,p,skip)){e="Truncated skip region.";return false;}p+=skip;auto tail=NsfChunkSize-length;if(w>NsfChunkSize||tail>NsfChunkSize-w||!has(in,p,tail)){e="Truncated raw tail.";return false;}std::copy_n(in.begin()+static_cast<std::ptrdiff_t>(p),tail,o.bytes.begin()+static_cast<std::ptrdiff_t>(w));p+=tail;o.wasCompressed=true;
 }else{e="Unknown NSF chunk magic.";return false;}
 o.type=u16(o.bytes,2);o.chunkId=static_cast<std::int32_t>(u32(o.bytes,4));
 if(o.type==0){
  const std::uint32_t count=u32(o.bytes,8);
  const std::uint64_t tableEnd=16ULL+(static_cast<std::uint64_t>(count)+1ULL)*4ULL;
  if(tableEnd>NsfChunkSize){o.entryTableError="Entry offset table exceeds the 64 KiB chunk.";return true;}
  std::vector<std::uint32_t> offsets;offsets.reserve(static_cast<std::size_t>(count)+1U);
  for(std::uint32_t i=0;i<=count;++i)offsets.push_back(u32(o.bytes,16U+static_cast<std::size_t>(i)*4U));
  bool valid=true;
  for(std::size_t i=0;i<offsets.size();++i)if(offsets[i]<tableEnd||offsets[i]>NsfChunkSize||(i&&offsets[i]<offsets[i-1])){valid=false;break;}
  if(!valid){o.entryTableError="Entry chunk contains invalid entry offsets.";return true;}
  o.entries.reserve(count);
  for(std::uint32_t i=0;i<count;++i){
   NsfEntry entry;entry.offset=offsets[i];entry.size=offsets[i+1]-offsets[i];
   const std::size_t p=entry.offset;
   if(entry.size<16){entry.error="Entry header is truncated.";o.entries.push_back(std::move(entry));continue;}
   if(u32(o.bytes,p)!=0x0100FFFFU){entry.error="Entry magic mismatch.";o.entries.push_back(std::move(entry));continue;}
   entry.eid=u32(o.bytes,p+4);entry.type=u32(o.bytes,p+8);entry.itemCount=u32(o.bytes,p+12);
   const std::uint64_t headerEnd=16ULL+static_cast<std::uint64_t>(entry.itemCount)*8ULL;
   if(headerEnd>entry.size){entry.error="Item offset table exceeds entry boundary.";o.entries.push_back(std::move(entry));continue;}
   bool itemsValid=true;
   for(std::uint32_t j=0;j<entry.itemCount;++j){
    const std::size_t q=p+16U+static_cast<std::size_t>(j)*8U;
    const std::uint32_t begin=u32(o.bytes,q),end=u32(o.bytes,q+4);
    if(begin<headerEnd||end<begin||end>entry.size){itemsValid=false;break;}
   }
   if(!itemsValid){entry.error="Entry contains an invalid item range.";o.entries.push_back(std::move(entry));continue;}
   entry.valid=true;o.entries.push_back(std::move(entry));
  }
  o.entryTableValid=true;
 }
 return true;
}
bool NsfArchive::saveUncompressed(const std::filesystem::path&f,std::string&e)const{
 std::ofstream s(f,std::ios::binary|std::ios::trunc);if(!s){e="Could not create NSF output.";return false;}
 for(const auto&c:chunks_){if(c.bytes.size()!=NsfChunkSize){e="Invalid in-memory NSF chunk size.";return false;}auto b=c.bytes;checksum(b);s.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!s){e="Failed writing NSF output.";return false;}}return true;
}
std::size_t NsfArchive::compressedChunkCount()const noexcept{return std::count_if(chunks_.begin(),chunks_.end(),[](const auto&c){return c.wasCompressed;});}
}
