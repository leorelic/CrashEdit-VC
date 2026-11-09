#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/eid.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <unordered_map>
namespace CrashEdit::Crash {
static std::uint16_t u16(const std::vector<std::uint8_t>&d,std::size_t p){return std::uint16_t(d[p])|std::uint16_t(std::uint16_t(d[p+1])<<8);}
static std::uint32_t u32(const std::vector<std::uint8_t>&d,std::size_t p){return std::uint32_t(d[p])|(std::uint32_t(d[p+1])<<8)|(std::uint32_t(d[p+2])<<16)|(std::uint32_t(d[p+3])<<24);}
static std::int32_t i32(const std::vector<std::uint8_t>&d,std::size_t p){return static_cast<std::int32_t>(u32(d,p));}
static bool has(const std::vector<std::uint8_t>&d,std::size_t p,std::size_t n){return p<=d.size()&&n<=d.size()-p;}
static void checksum(std::vector<std::uint8_t>&d){std::uint32_t c=0x12345678;for(std::size_t i=0;i<NsfChunkSize;++i){if(i<12||i>=16)c+=d[i];c=(c<<3)|(c>>29);}for(int i=0;i<4;++i)d[12+i]=std::uint8_t(c>>(8*i));}

static std::size_t align4(std::size_t n){return (n+3U)&~std::size_t(3U);}
static bool parseGoolFrameGroups(const std::vector<std::uint8_t>&bytes,const NsfItem&item,NsfGool&gool,NsfGoolVersion version){
 const std::size_t end=item.offset+item.size;
 if(!has(bytes,item.offset,item.size)){gool.frameGroupError="Frame-group item exceeds its chunk.";return false;}
 std::size_t p=item.offset;
 while(p<end){
  if(end-p<4U){gool.frameGroupError="Truncated frame-group header.";return false;}
  const auto type=u16(bytes,p);
  const auto count=static_cast<std::int16_t>(u16(bytes,p+2));
  if(count<0){gool.frameGroupError="Negative frame count in frame group.";return false;}
  NsfGoolFrameGroup group;group.type=type;group.frameCount=count;group.byteOffset=p-item.offset;
  std::size_t size=0;
  if(type==1U){
   if(version==NsfGoolVersion::Version3){
    if(end-p<20U){gool.frameGroupError="Truncated Crash 3 vertex-group header.";return false;}
    group.interpolated=u32(bytes,p+4U)!=0;group.partCount=u32(bytes,p+8U);
    group.modelEid=u32(bytes,p+12U);group.eid=u32(bytes,p+16U);
    group.eidFieldOffset=16U;group.interpolatedFieldOffset=4U;size=20U;
   }else{
    if(end-p<12U){gool.frameGroupError="Truncated Crash 2 vertex-group header.";return false;}
    group.eid=u32(bytes,p+4U);group.interpolated=u32(bytes,p+8U)!=0;size=12U;
   }
  }else if(type==2U||type==3U){
   if(end-p<8U){gool.frameGroupError="Truncated sprite/font-group header.";return false;}
   group.eid=u32(bytes,p+4);const std::size_t stride=type==2U?16U:20U;
   if(static_cast<std::size_t>(count)>(end-p-8U)/stride){gool.frameGroupError="Sprite/font frame records exceed the frame-group item.";return false;}
   size=8U+static_cast<std::size_t>(count)*stride;
  }else if(type==4U){
   if(end-p<12U){gool.frameGroupError="Truncated text-group header.";return false;}
   group.eid=u32(bytes,p+4);group.font=static_cast<std::int32_t>(u32(bytes,p+8));
   std::size_t q=p+12U;
   for(std::int16_t i=0;i<count;++i){
    const auto zero=std::find(bytes.begin()+static_cast<std::ptrdiff_t>(q),bytes.begin()+static_cast<std::ptrdiff_t>(end),std::uint8_t{0});
    if(zero==bytes.begin()+static_cast<std::ptrdiff_t>(end)){gool.frameGroupError="Unterminated text string in text frame group.";return false;}
    q=static_cast<std::size_t>(zero-bytes.begin())+1U;
   }
   size=((q-item.offset+3U)&~std::size_t(3U))-(p-item.offset);
   if(size>end-p){gool.frameGroupError="Text-group alignment exceeds the frame-group item.";return false;}
  }else if(type==5U){
   if(end-p<12U){gool.frameGroupError="Truncated image-group header.";return false;}
   group.eid=u32(bytes,p+4);group.partCount=u32(bytes,p+8);
   const std::size_t available=end-p-12U;
   if(group.partCount!=0U&&static_cast<std::size_t>(count)>available/(24U*static_cast<std::size_t>(group.partCount))){gool.frameGroupError="Image-group frame records exceed the frame-group item.";return false;}
   const std::size_t records=static_cast<std::size_t>(count)*static_cast<std::size_t>(group.partCount);
   if(records>available/24U){gool.frameGroupError="Image-group part records exceed the frame-group item.";return false;}
   size=12U+records*24U;
  }else{
   char message[128];std::snprintf(message,sizeof(message),"Unknown frame-group type %u at item offset 0x%zX.",static_cast<unsigned>(type),p-item.offset);
   gool.frameGroupError=message;return false;
  }
  if(size==0U||size>end-p){gool.frameGroupError="Invalid frame-group record size.";return false;}
  group.byteSize=size;gool.frameGroups.push_back(group);p+=size;
 }
 gool.frameGroupsDecoded=true;return true;
}
static bool parseScenery(const std::vector<std::uint8_t>&bytes,const NsfEntry&entry,bool crash3,NsfScenery&out){
 out.isCrash3=crash3;
 auto fail=[&](const char*message){out.error=message;return false;};
 if(entry.items.size()!=7)return fail("Scenery entry must contain exactly seven items.");
 const auto&info=entry.items[0];
 if(info.size!=76||!has(bytes,info.offset,info.size))return fail("Scenery header must be exactly 76 bytes.");
 const auto count=[&](std::size_t offset){return u32(bytes,info.offset+offset);};
 const std::uint32_t vertexCount=count(16),triangleCount=count(20),quadCount=count(24),textureCount=count(28),colorCount=count(32),animatedCount=count(36),pageCount=count(40);
 constexpr std::uint32_t maxElements=16384;
 if(vertexCount>maxElements||triangleCount>maxElements||quadCount>maxElements||textureCount>maxElements||colorCount>maxElements||animatedCount>maxElements||pageCount>8U)
  return fail("Scenery header contains unreasonable element counts.");
 const std::size_t expected[]={76,align4(static_cast<std::size_t>(vertexCount)*6U),align4(static_cast<std::size_t>(triangleCount)*6U),static_cast<std::size_t>(quadCount)*8U,static_cast<std::size_t>(textureCount)*12U,static_cast<std::size_t>(colorCount)*4U,static_cast<std::size_t>(animatedCount)*4U};
 for(std::size_t i=0;i<7;++i)if(entry.items[i].size!=expected[i]||!has(bytes,entry.items[i].offset,entry.items[i].size))return fail("Scenery item size does not match its header counts.");
 out.xOffset=i32(bytes,info.offset);out.yOffset=i32(bytes,info.offset+4);out.zOffset=i32(bytes,info.offset+8);out.isSky=i32(bytes,info.offset+12)!=0;
 for(std::uint32_t i=0;i<pageCount;++i)out.texturePageEids.push_back(u32(bytes,info.offset+0x2c+static_cast<std::size_t>(i)*4U));
 const auto&vitem=entry.items[1];out.vertices.reserve(vertexCount);
 for(std::uint32_t i=0;i<vertexCount;++i){
  const std::size_t xyOffset=vitem.offset+(static_cast<std::size_t>(vertexCount)-1U-i)*4U;
  const std::size_t zOffset=vitem.offset+static_cast<std::size_t>(vertexCount)*4U+static_cast<std::size_t>(i)*2U;
  const std::uint16_t xraw=u16(bytes,xyOffset),yraw=u16(bytes,xyOffset+2),zraw=u16(bytes,zOffset);
  const std::uint16_t ux=xraw&0xfU,uy=yraw&0xfU,uz=zraw&0xfU;
  const auto decodeCoord=[&](std::uint16_t raw){return crash3?static_cast<std::int32_t>(raw>>4U):static_cast<std::int32_t>(static_cast<std::int16_t>(raw)>>4);};
  out.vertices.push_back({decodeCoord(xraw),decodeCoord(yraw),decodeCoord(zraw),static_cast<std::uint16_t>(((uy&3U)<<8U)|(ux<<4U)|uz)});
 }
 const auto&titem=entry.items[2];
 for(std::uint32_t i=0;i<triangleCount;++i){
  const std::size_t aOffset=titem.offset+(static_cast<std::size_t>(triangleCount)-1U-i)*4U;
  const std::size_t bOffset=titem.offset+static_cast<std::size_t>(triangleCount)*4U+static_cast<std::size_t>(i)*2U;
  const std::uint32_t a=u32(bytes,aOffset);const std::uint16_t b=u16(bytes,bOffset);
  out.triangles.push_back({static_cast<std::uint16_t>((a>>8U)&0xfffU),static_cast<std::uint16_t>((a>>20U)&0xfffU),static_cast<std::uint16_t>((b>>4U)&0xfffU),static_cast<std::uint16_t>((b&0xfU)|((a&0x7fU)<<4U)),(a&0x80U)!=0});
 }
 const auto&qitem=entry.items[3];
 for(std::uint32_t i=0;i<quadCount;++i){
  const std::size_t p=qitem.offset+static_cast<std::size_t>(i)*8U;const std::uint32_t a=u32(bytes,p),b=u32(bytes,p+4);
  out.quads.push_back({static_cast<std::uint16_t>((a>>8U)&0xfffU),static_cast<std::uint16_t>((a>>20U)&0xfffU),static_cast<std::uint16_t>((b>>20U)&0xfffU),static_cast<std::uint16_t>((b>>8U)&0xfffU),static_cast<std::uint16_t>((b&0xfU)|((a&0x7fU)<<4U)),static_cast<std::uint8_t>((b>>4U)&0xfU),(a&0x80U)!=0});
 }
 const auto&texitem=entry.items[4];
 for(std::uint32_t i=0;i<textureCount;++i){
  const std::size_t p=texitem.offset+static_cast<std::size_t>(i)*12U;const std::uint8_t b2=bytes[p+2],b6=bytes[p+6],b7=bytes[p+7];
  out.textures.push_back({bytes[p],bytes[p+1],bytes[p+4],bytes[p+5],bytes[p+8],bytes[p+9],bytes[p+10],bytes[p+11],static_cast<std::uint8_t>(b2&0xfU),static_cast<std::uint8_t>((b2>>4U)&0xfU),bytes[p+3],static_cast<std::uint8_t>(((b6>>7U)|((b7&1U)<<1U))&3U),static_cast<std::uint8_t>((b6>>5U)&3U),static_cast<std::uint8_t>(b6&0xfU),static_cast<std::uint8_t>((b7>>2U)&0x3fU)});
 }
 const auto&coloritem=entry.items[5];
 for(std::uint32_t i=0;i<colorCount;++i){const std::size_t p=coloritem.offset+static_cast<std::size_t>(i)*4U;out.colors.push_back({bytes[p],bytes[p+1],bytes[p+2],bytes[p+3]});}
 const auto&animitem=entry.items[6];
 for(std::uint32_t i=0;i<animatedCount;++i)out.animatedTextures.push_back(u32(bytes,animitem.offset+static_cast<std::size_t>(i)*4U));
 out.decoded=true;return true;
}
static bool parseEntity(const std::vector<std::uint8_t>&bytes,const NsfItem&item,std::size_t itemIndex,NsfEntity&out){
 const std::size_t base=item.offset,end=base+item.size;out.itemIndex=itemIndex;
 if(item.size<16){out.error="Entity header is truncated.";return false;}
 out.declaredLength=u32(bytes,base);const std::uint32_t count=u32(bytes,base+12);
 if(count>8192||16ULL+static_cast<std::uint64_t>(count)*8ULL>item.size){out.error="Entity property descriptor table is invalid.";return false;}
 if(out.declaredLength!=item.size)out.error="Entity length field does not match item size.";
 for(std::uint32_t n=0;n<count;++n){
  const std::size_t q=base+16U+static_cast<std::size_t>(n)*8U;
  NsfEntityProperty p;p.id=static_cast<std::int16_t>(u16(bytes,q));p.rawType=bytes[q+4];p.type=static_cast<std::uint8_t>(p.rawType&31U);
  p.last=(p.rawType&0x80U)!=0;p.sparse=(p.rawType&0x40U)!=0;p.hasMetaValues=(p.rawType&0x20U)!=0;
  if(static_cast<std::uint16_t>(p.id)==0x0103U&&p.type==0x13U)p.type=4;
  p.elementSize=bytes[q+5];p.rowCount=static_cast<std::int16_t>(u16(bytes,q+6));
  const std::size_t dataStart=base+static_cast<std::size_t>(u16(bytes,q+2))+12U;
  const std::size_t dataEnd=(n+1U==count)?end:base+static_cast<std::size_t>(u16(bytes,q+10))+12U;
  if(dataStart<base+16U+static_cast<std::size_t>(count)*8U||dataEnd<dataStart||dataEnd>end){p.error="Property data range is invalid.";out.properties.push_back(std::move(p));continue;}
  p.dataOffset=dataStart;p.dataSize=dataEnd-dataStart;p.valid=true;
  if(p.rowCount<0){p.valid=false;p.error="Property row count is negative.";out.properties.push_back(std::move(p));continue;}
  if((p.type==19U&&p.elementSize==4U)||(p.type==5U&&p.elementSize==4U)||(p.type==4U&&p.elementSize==4U)||(p.type==3U&&p.elementSize==4U)||(p.type==1U&&p.elementSize==1U)||(p.type==6U&&p.elementSize==6U)||(p.type==18U&&p.elementSize==2U)){
   std::size_t pos=dataStart;std::vector<std::uint16_t> rowCounts;
   if(p.sparse){if(!has(bytes,pos,static_cast<std::size_t>(p.rowCount)*2U)||pos+static_cast<std::size_t>(p.rowCount)*2U>dataEnd){p.valid=false;p.error="Sparse property row counts are truncated.";}}
   if(p.valid&&p.sparse){for(int r=0;r<p.rowCount;++r){rowCounts.push_back(u16(bytes,pos));pos+=2;}}
   else if(p.valid){if(!has(bytes,pos,2)||pos+2>dataEnd){p.valid=false;p.error="Property value count is truncated.";}else{auto nvalues=u16(bytes,pos);pos+=2;rowCounts.assign(static_cast<std::size_t>(p.rowCount),nvalues);}}
   if(p.valid&&p.hasMetaValues){if(pos+static_cast<std::size_t>(p.rowCount)*2U>dataEnd){p.valid=false;p.error="Property metadata is truncated.";}else pos+=static_cast<std::size_t>(p.rowCount)*2U;}
   if(p.valid){pos=(pos+3U)&~std::size_t(3U);for(auto nvalues:rowCounts){const std::size_t bytesNeeded=static_cast<std::size_t>(nvalues)*p.elementSize;if(pos+bytesNeeded>dataEnd){p.valid=false;p.error="Property values exceed their data range.";break;}
    for(std::uint16_t k=0;k<nvalues;++k){if(p.type==6U){NsfEntityProperty::Position v{static_cast<std::int16_t>(u16(bytes,pos)),static_cast<std::int16_t>(u16(bytes,pos+2)),static_cast<std::int16_t>(u16(bytes,pos+4))};p.positions.push_back(v);}else if(p.type==1U)p.intValues.push_back(bytes[pos]);else if(p.type==18U)p.intValues.push_back(static_cast<std::int16_t>(u16(bytes,pos)));else p.intValues.push_back(i32(bytes,pos));p.valueOffsets.push_back(pos);pos+=p.elementSize;}
   }}
  }
  out.properties.push_back(std::move(p));
 }
 for(const auto& property:out.properties){
  if(!property.valid)continue;const auto id=static_cast<std::uint16_t>(property.id);
  if(id==0x002cU&&property.type==1U){for(auto ch:property.intValues){if(ch==0)break;out.name.push_back(static_cast<char>(static_cast<unsigned char>(ch)));}}
  if(id==0x004bU)out.positions=property.positions;
  if(id==0x009fU&& !property.intValues.empty()){out.hasId=true;out.id=property.intValues[0];if(property.intValues.size()>1){out.hasAlternateId=true;out.alternateId=property.intValues[1];}}
  if(id==0x00a9U&&!property.intValues.empty()){out.hasGoolType=true;out.goolType=property.intValues.front();}
  if(id==0x00aaU&&!property.intValues.empty()){out.hasSubtype=true;out.subtype=property.intValues.front();}
  if(id==0x00a4U&&property.type==5U){for(std::size_t n=0;n<property.intValues.size();++n){const auto raw=static_cast<std::uint32_t>(property.intValues[n]);out.settings.push_back({static_cast<std::uint8_t>(raw&0xffU),static_cast<std::int32_t>(property.intValues[n]>>8),raw,0});}}
  if(id==0x0173U&&!property.intValues.empty()){out.hasCameraIndex=true;out.cameraIndex=property.intValues.front();}
  if(id==0x0174U&&!property.intValues.empty()){out.hasCameraSubIndex=true;out.cameraSubIndex=property.intValues.front();}
  if(id==0x0277U&&!property.intValues.empty()){out.hasDdaSettings=true;out.ddaDeathCount=property.intValues.front()>>8;}
  if(id==0x0287U&&property.type==18U)out.victims=property.intValues;
  if(id==0x0288U&&!property.intValues.empty()){out.hasDdaSection=true;out.ddaSection=property.intValues.front();}
  if(id==0x028bU&&property.type==5U&&!property.intValues.empty()){out.hasBoxCount=true;out.boxCount=property.intValues.front()>>8;}
 }
 out.valid=true;return true;
}
bool NsfArchive::load(const std::filesystem::path&f,std::string&e){
 std::ifstream s(f,std::ios::binary|std::ios::ate);if(!s){e="Could not open NSF file: "+f.string();return false;}
 auto end=s.tellg();if(end<0){e="Could not determine NSF file size.";return false;}
 auto z=static_cast<std::uintmax_t>(end);if(z>std::numeric_limits<std::size_t>::max()){e="NSF file too large.";return false;}
 std::vector<std::uint8_t>b(static_cast<std::size_t>(z));s.seekg(0);if(!b.empty()&&!s.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()))){e="Could not read NSF file." ;return false;}return loadBytes(b,e);
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
  const std::uint32_t count=u32(o.bytes,8);const std::uint64_t tableEnd=20ULL+static_cast<std::uint64_t>(count)*4ULL;
  if(tableEnd>NsfChunkSize){o.entryTableError="Entry offset table exceeds the 64 KiB chunk.";return true;}
  std::vector<std::uint32_t> offsets;offsets.reserve(static_cast<std::size_t>(count)+1U);
  for(std::uint32_t i=0;i<=count;++i)offsets.push_back(u32(o.bytes,16U+static_cast<std::size_t>(i)*4U));
  bool valid=true;for(std::size_t i=0;i<offsets.size();++i)if(offsets[i]<tableEnd||offsets[i]>NsfChunkSize||(i&&offsets[i]<offsets[i-1])){valid=false;break;}
  if(!valid){o.entryTableError="Entry chunk contains invalid entry offsets.";return true;}
  o.entries.reserve(count);
  for(std::uint32_t i=0;i<count;++i){
   NsfEntry entry;entry.offset=offsets[i];entry.size=offsets[i+1]-offsets[i];const std::size_t q=entry.offset;
   if(entry.size<16){entry.error="Entry header is truncated.";o.entries.push_back(std::move(entry));continue;}
   if(u32(o.bytes,q)!=0x0100FFFFU){entry.error="Entry magic mismatch.";o.entries.push_back(std::move(entry));continue;}
   entry.eid=u32(o.bytes,q+4);entry.type=u32(o.bytes,q+8);entry.itemCount=u32(o.bytes,q+12);
   const std::uint64_t headerEnd=16ULL+(static_cast<std::uint64_t>(entry.itemCount)+1ULL)*4ULL;
   if(headerEnd>entry.size){entry.error="Item offset table exceeds entry boundary.";o.entries.push_back(std::move(entry));continue;}
   std::vector<std::uint32_t> itemOffsets;itemOffsets.reserve(static_cast<std::size_t>(entry.itemCount)+1U);
   for(std::uint32_t j=0;j<=entry.itemCount;++j)itemOffsets.push_back(u32(o.bytes,q+16U+static_cast<std::size_t>(j)*4U));
   bool itemsValid=true;for(std::size_t j=0;j<itemOffsets.size();++j)if(itemOffsets[j]<headerEnd||itemOffsets[j]>entry.size||(j&&itemOffsets[j]<itemOffsets[j-1])){itemsValid=false;break;}
   if(!itemsValid){entry.error="Entry contains an invalid item offset table.";o.entries.push_back(std::move(entry));continue;}
   for(std::uint32_t j=0;j<entry.itemCount;++j)entry.items.push_back({q+itemOffsets[j],itemOffsets[j+1]-itemOffsets[j]});
   entry.valid=true;o.entries.push_back(std::move(entry));
  }
  o.entryTableValid=true;
 }
 return true;
}

static bool parseAnimationFrame(const std::vector<std::uint8_t>&bytes,const NsfItem&item,bool crash3,NsfAnimationFrame&out){
 out.rawOffset=item.offset;out.rawSize=item.size;
 const std::size_t fixedHeader=crash3?28U:24U;
 auto fail=[&](const char*message){out.error=message;return false;};
 if(!has(bytes,item.offset,item.size)||item.size<fixedHeader)return fail("Animation frame header is truncated.");
 const std::size_t p=item.offset;
 out.xOffset=static_cast<std::int16_t>(u16(bytes,p));
 out.yOffset=static_cast<std::int16_t>(u16(bytes,p+2U));
 out.zOffset=static_cast<std::int16_t>(u16(bytes,p+4U));
 out.unknown=static_cast<std::int16_t>(u16(bytes,p+6U));
 if(crash3){
  out.modelEid=u32(bytes,p+8U);out.reserved=u32(bytes,p+12U);
  out.vertexCount=u32(bytes,p+16U);out.collisionCount=u32(bytes,p+20U);out.headerSize=u32(bytes,p+24U);
 }else{
  out.vertexCount=u32(bytes,p+8U);out.collisionCount=u32(bytes,p+12U);out.modelEid=u32(bytes,p+16U);out.headerSize=u32(bytes,p+20U);
 }
 constexpr std::uint32_t maxVertices=16384U,maxCollisions=4096U;
 if(out.vertexCount>maxVertices||out.collisionCount>maxCollisions)return fail("Animation frame contains unreasonable vertex or collision counts.");
 if(out.collisionCount>(item.size-fixedHeader)/0x28U)return fail("Animation collision records exceed the frame item.");
 const std::size_t fakeHeader=fixedHeader+static_cast<std::size_t>(out.collisionCount)*0x28U;
 if(out.headerSize<fakeHeader||out.headerSize>item.size)return fail("Animation frame header-size boundary is invalid.");
 const std::size_t specialBytes=static_cast<std::size_t>(out.headerSize)-fakeHeader;
 if(specialBytes%3U!=0U)return fail("Animation special-vertex data is not a whole number of vertices.");
 out.specialVertexCount=specialBytes/3U;
 if(out.specialVertexCount>out.vertexCount)return fail("Animation frame has more special vertices than total vertices.");
 const std::size_t temporalBytes=item.size-fakeHeader;
 if(temporalBytes%4U!=0U)return fail("Animation temporal bitstream is not 32-bit aligned.");
 out.collisions.reserve(out.collisionCount);
 for(std::uint32_t i=0;i<out.collisionCount;++i){
  NsfAnimationCollision collision{};
  for(std::size_t n=0;n<10U;++n)collision.values[n]=i32(bytes,p+fixedHeader+static_cast<std::size_t>(i)*0x28U+n*4U);
  out.collisions.push_back(collision);
 }
 out.specialVertices.reserve(out.specialVertexCount);
 for(std::size_t i=0;i<out.specialVertexCount;++i){
  const std::size_t q=p+fakeHeader+i*3U;
  out.specialVertices.push_back(std::array<std::uint8_t,3>{bytes[q],bytes[q+1U],bytes[q+2U]});
 }
 out.temporalWords.reserve(temporalBytes/4U);
 for(std::size_t q=p+fakeHeader;q<p+item.size;q+=4U)out.temporalWords.push_back(u32(bytes,q));
 return true;
}
struct ModelPolyCommand {bool triangle=false;std::uint8_t color1=0,color2=0,texture=0,key=0,type=0,triType=0,triSubtype=0;bool animated=false,flag=false;};
struct ModelColorRef {std::size_t wordIndex=0;std::uint8_t bitOffset=9;bool valid=false;};
static bool decodeModelPolygons(NsfModel&model){
 model.triangles.clear();model.polygonError.clear();
 auto fail=[&](const char*m){model.polygonError=m;return false;};
 const std::size_t count=model.polygonWords.size();std::vector<ModelPolyCommand>cmd(count);std::vector<std::int32_t>vtx;vtx.reserve(count);
 std::unordered_map<std::uint8_t,std::int32_t>keys;
 const std::int32_t special=model.compressed?static_cast<std::int32_t>(model.vertexCount-model.positions.size()):0;
 std::int32_t v=model.compressed?-special:0;
 for(std::size_t i=0;i<count;++i){
  const auto raw=model.polygonWords[i];if(raw==0xffffffffU)break;
  if((raw&0xffff0000U)==0){cmd[i].color1=static_cast<std::uint8_t>((raw>>2U)&0x7fU);cmd[i].color2=static_cast<std::uint8_t>((raw>>9U)&0x7fU);continue;}
  auto&x=cmd[i];x.triangle=true;x.texture=static_cast<std::uint8_t>(raw);x.animated=((raw>>8U)&1U)!=0;x.color1=static_cast<std::uint8_t>((raw>>9U)&0x7fU);x.key=static_cast<std::uint8_t>(raw>>16U);x.type=static_cast<std::uint8_t>((raw>>26U)&1U);x.flag=((raw>>27U)&1U)!=0;const auto tri=static_cast<std::uint8_t>(raw>>28U);x.triSubtype=tri&3U;x.triType=(tri>>2U)&3U;
  if(x.type==0){if(x.key!=0x57U)keys[x.key]=v;vtx.push_back(v++);}else{auto it=keys.find(x.key);if(it==keys.end())return fail("Model polygon stream duplicates an undefined vertex key.");vtx.push_back(it->second);}
 }
 auto triangleColorRef=[&](std::size_t i)->ModelColorRef{return i<count&&cmd[i].triangle?ModelColorRef{i,9U,true}:ModelColorRef{};};
 auto colorValue=[&](const ModelColorRef&r)->int{if(!r.valid||r.wordIndex>=count)return -1;const auto raw=model.polygonWords[r.wordIndex];return static_cast<int>((raw>>r.bitOffset)&0x7fU);};
 std::int32_t lastCC=-1,lastAA=-1,lastValid=-3,lastNonBB=-1,lastColor=-1;std::size_t cur=0;
 auto emit=[&](int a,int b,int c,const ModelColorRef&ra,const ModelColorRef&rb,const ModelColorRef&rc,const ModelPolyCommand&x,std::size_t wordIndex){
  const int ca=colorValue(ra),cb=colorValue(rb),cc=colorValue(rc);const int maxIndex=static_cast<int>(model.vertexCount)-special;
  if(a< -special||b< -special||c< -special||a>=maxIndex||b>=maxIndex||c>=maxIndex||ca<0||cb<0||cc<0||static_cast<std::size_t>(ca)>=model.colors.size()||static_cast<std::size_t>(cb)>=model.colors.size()||static_cast<std::size_t>(cc)>=model.colors.size())return false;
  NsfModelTriangle triangle;triangle.a=a;triangle.b=b;triangle.c=c;triangle.colorA=static_cast<std::uint8_t>(ca);triangle.colorB=static_cast<std::uint8_t>(cb);triangle.colorC=static_cast<std::uint8_t>(cc);
  triangle.textureIndex=x.texture;triangle.triangleType=x.triType;triangle.triangleSubtype=x.triSubtype;triangle.animated=x.animated;triangle.flag=x.flag;triangle.commandWordIndex=wordIndex;
  triangle.colorWordIndexA=ra.wordIndex;triangle.colorWordIndexB=rb.wordIndex;triangle.colorWordIndexC=rc.wordIndex;triangle.colorBitOffsetA=ra.bitOffset;triangle.colorBitOffsetB=rb.bitOffset;triangle.colorBitOffsetC=rc.bitOffset;
  model.triangles.push_back(triangle);return true;
 };
 for(std::size_t i=0;i<count;++i){
  if(model.polygonWords[i]==0xffffffffU)break;const auto&x=cmd[i];if(!x.triangle){lastColor=static_cast<std::int32_t>(i);continue;}if(cur>=vtx.size())return fail("Model polygon stream is inconsistent.");
  const ModelColorRef current{i,9U,true};
  if(x.triType==0){
   if(cur<2)return fail("Model strip starts before three vertices.");
   const ModelColorRef c1=lastColor+1==static_cast<int>(i)?ModelColorRef{static_cast<std::size_t>(lastColor),2U,true}:triangleColorRef(i-1U);
   const ModelColorRef c2=lastColor+1==static_cast<int>(i)?ModelColorRef{static_cast<std::size_t>(lastColor),9U,true}:lastColor+2==static_cast<int>(i)?ModelColorRef{static_cast<std::size_t>(lastColor),2U,true}:triangleColorRef(i-2U);
   if(!emit(vtx[cur],vtx[cur-1U],vtx[cur-2U],current,c1,c2,x,i))return fail("Model strip triangle references invalid geometry.");
   lastAA=static_cast<int>(cur);lastNonBB=static_cast<int>(i);
  }else if(x.triType==1){
   ModelColorRef ci{};
   if(lastColor<lastNonBB-2&&lastNonBB>=2)ci=triangleColorRef(static_cast<std::size_t>(lastNonBB-2));
   else if(lastColor==lastNonBB-2&&lastColor>=0)ci={static_cast<std::size_t>(lastColor),2U,true};
   else if(lastColor==lastNonBB-1&&lastColor>=0)ci={static_cast<std::size_t>(lastColor),9U,true};
   else if(lastColor>lastNonBB-2&&lastColor>=0)ci={static_cast<std::size_t>(lastColor),9U,true};
   const ModelColorRef c1=lastColor+1==static_cast<int>(i)?ModelColorRef{static_cast<std::size_t>(lastColor),2U,true}:triangleColorRef(i-1U);
   if(cur<1)return fail("Connected triangle lacks preceding vertex.");
   const int third=vtx[static_cast<std::size_t>(lastCC>lastAA?lastCC:std::max(0,lastAA-2))];
   if(!emit(vtx[cur],vtx[cur-1U],third,current,c1,ci,x,i))return fail("Connected triangle references invalid geometry.");
  }else if(x.triType==2){
   if(i+2U<count&&lastValid+2<static_cast<int>(i)){
    if(cur+2U>=vtx.size())return fail("Fan triangle exceeds vertex stream.");
    if(!emit(vtx[cur],vtx[cur+1U],vtx[cur+2U],current,triangleColorRef(i+1U),triangleColorRef(i+2U),x,i))return fail("Model fan triangle references invalid geometry.");
    lastValid=static_cast<int>(i);
   }
   lastCC=static_cast<int>(cur);lastNonBB=static_cast<int>(i);
  }
  ++cur;
 }
 if(model.polygonCount&&model.triangles.size()!=model.polygonCount)model.polygonError="Decoded polygon count differs from model header count.";
 return true;
}

static bool parseModel(const std::vector<std::uint8_t>&bytes,const NsfEntry&entry,NsfModel&out){
 auto fail=[&](const char*message){out.error=message;return false;};
 if(entry.items.size()!=5U&&entry.items.size()!=6U)return fail("Model entry must contain five or six items.");
 const auto&info=entry.items[0];
 if(info.size<0x50U||!has(bytes,info.offset,info.size))return fail("Model info item must contain at least 0x50 bytes.");
 const std::size_t h=info.offset;
 const std::uint32_t polygonWordCount=u32(bytes,h+0x2cU),textureCount=u32(bytes,h+0x34U);
 out.vertexCount=u32(bytes,h+0x38U);
 const std::uint32_t colorCount=u32(bytes,h+0x3cU),pageCount=u32(bytes,h+0x40U);
 out.polygonWordCount=polygonWordCount;out.polygonCount=u32(bytes,h+0x44U);out.specialVertexCount=u32(bytes,h+0x4cU);
 const std::uint32_t extendedCount=u32(bytes,h+0x48U),positionBase=u32(bytes,h+0x4cU);
 constexpr std::uint32_t maxElements=16384U;
 if(out.polygonWordCount>maxElements||out.polygonCount>maxElements||textureCount>maxElements||out.vertexCount>maxElements||colorCount>maxElements||extendedCount>maxElements||pageCount>8U)
  return fail("Model info contains unreasonable element counts.");
 if(positionBase>out.vertexCount)return fail("Model compressed-position base exceeds the model vertex count.");
 const std::uint32_t positionCount=out.vertexCount-positionBase;
 if(entry.items.size()==6U&&positionCount>maxElements)return fail("Model compressed-position count is unreasonable.");
 const std::size_t expected[]={info.size,static_cast<std::size_t>(out.polygonWordCount)*4U,static_cast<std::size_t>(colorCount)*4U,static_cast<std::size_t>(textureCount)*12U,static_cast<std::size_t>(extendedCount)*4U,static_cast<std::size_t>(positionCount)*4U};
 for(std::size_t i=0;i<5U;++i)if(entry.items[i].size!=expected[i]||!has(bytes,entry.items[i].offset,entry.items[i].size))return fail("Model item size does not match its header counts.");
 if(entry.items.size()==6U&&(entry.items[5].size!=expected[5]||!has(bytes,entry.items[5].offset,entry.items[5].size)))return fail("Model compressed-position item size does not match its header count.");
 out.header.assign(bytes.begin()+static_cast<std::ptrdiff_t>(info.offset),bytes.begin()+static_cast<std::ptrdiff_t>(info.offset+info.size));
 out.scaleX=i32(bytes,h);out.scaleY=i32(bytes,h+4U);out.scaleZ=i32(bytes,h+8U);
 for(std::uint32_t i=0;i<pageCount;++i)out.texturePageEids.push_back(u32(bytes,h+0x0cU+static_cast<std::size_t>(i)*4U));
 const auto&polygons=entry.items[1];
 for(std::size_t p=polygons.offset;p<polygons.offset+polygons.size;p+=4U)out.polygonWords.push_back(u32(bytes,p));
 const auto&colors=entry.items[2];
 for(std::uint32_t i=0;i<colorCount;++i){const std::size_t p=colors.offset+static_cast<std::size_t>(i)*4U;out.colors.push_back({bytes[p],bytes[p+1U],bytes[p+2U],bytes[p+3U]});}
 const auto&textures=entry.items[3];
 for(std::uint32_t i=0;i<textureCount;++i){
  const std::size_t p=textures.offset+static_cast<std::size_t>(i)*12U;const std::uint8_t b2=bytes[p+2U],b6=bytes[p+6U],b7=bytes[p+7U];
  out.textures.push_back({bytes[p],bytes[p+1U],bytes[p+4U],bytes[p+5U],bytes[p+8U],bytes[p+9U],bytes[p+10U],bytes[p+11U],
   static_cast<std::uint8_t>(b2&0xfU),static_cast<std::uint8_t>((b2>>4U)&0xfU),bytes[p+3U],
   static_cast<std::uint8_t>(((b6>>7U)|((b7&1U)<<1U))&3U),static_cast<std::uint8_t>((b6>>5U)&3U),
   static_cast<std::uint8_t>(b6&0xfU),static_cast<std::uint8_t>((b7>>2U)&0x3fU)});
 }
 const auto&extended=entry.items[4];
 for(std::uint32_t i=0;i<extendedCount;++i){const std::uint32_t raw=u32(bytes,extended.offset+static_cast<std::size_t>(i)*4U);out.extendedTextureWords.push_back(raw);NsfModelExtendedTexture record;record.raw=raw;record.offset=static_cast<std::uint16_t>(raw&0x7ffU);record.isLod=(raw&0x80000000U)!=0;record.leap=(raw&(1U<<11U))!=0;record.mask=static_cast<std::uint8_t>((raw>>12U)&0x7fU);record.delay=static_cast<std::uint8_t>((raw>>19U)&0x7fU);record.latency=static_cast<std::uint8_t>((raw>>26U)&0x1fU);for(std::size_t level=0;level<8U;++level)record.lod[level]=static_cast<std::uint8_t>((raw>>(29U-level*2U))&3U);out.extendedTextures.push_back(record);}
 if(entry.items.size()==6U){
  const auto&positions=entry.items[5];out.positions.reserve(positionCount);out.compressed=true;
  for(std::uint32_t i=0;i<positionCount;++i){
   const std::uint32_t raw=u32(bytes,positions.offset+static_cast<std::size_t>(i)*4U);
   out.positions.push_back({static_cast<std::uint8_t>((raw>>25U)&0x7fU),static_cast<std::uint8_t>(raw>>17U),static_cast<std::uint8_t>(raw>>9U),
    static_cast<std::uint8_t>((raw>>6U)&7U),static_cast<std::uint8_t>((raw>>3U)&7U),static_cast<std::uint8_t>(raw&7U),raw});
  }
 }
 out.polygonsDecoded=decodeModelPolygons(out);
 out.decoded=true;return true;
}

void NsfArchive::decodeZones(bool crash3Retail){
 for(auto&chunk:chunks_)if(chunk.type==0&&chunk.entryTableValid)for(auto&entry:chunk.entries)if(entry.valid&&entry.type==3){NsfScenery scenery;if(!parseScenery(chunk.bytes,entry,crash3Retail,scenery)&&scenery.error.empty())scenery.error="Scenery could not be decoded.";entry.scenery=std::move(scenery);}
 for(auto&chunk:chunks_)if(chunk.type==0&&chunk.entryTableValid)for(auto&entry:chunk.entries)if(entry.valid&&entry.type==7){
  NsfZone z;z.isCrash3Retail=crash3Retail;
  if(entry.items.size()<2){z.error="Zone entry requires a header and layout item.";entry.zone=std::move(z);continue;}
  z.headerSize=crash3Retail?0x358U:0x318U;const auto&header=entry.items[0];const auto&layout=entry.items[1];
  if(header.size!=z.headerSize){z.error="Zone header size does not match selected game version (expected "+std::to_string(z.headerSize)+" bytes).";entry.zone=std::move(z);continue;}
  if(layout.size<0x24){z.error="Zone layout item is shorter than 0x24 bytes.";entry.zone=std::move(z);continue;}
  const auto&b=chunk.bytes;const std::size_t h=header.offset,l=layout.offset;
  z.worldCount=i32(b,h);z.infoCount=i32(b,h+0x184);z.cameraCount=i32(b,h+0x188);z.entityCount=i32(b,h+0x18c);z.zoneCount=i32(b,h+0x190);
  if(z.worldCount<0||z.worldCount>8||z.cameraCount<0||z.entityCount<0||z.zoneCount<0||z.zoneCount>(crash3Retail?16:8)){z.error="Zone header contains invalid counts.";entry.zone=std::move(z);continue;}
  for(int i=0;i<8;++i)z.worlds.push_back(i32(b,h+4+static_cast<std::size_t>(i)*0x30));
  const int links=crash3Retail?16:8;for(int i=0;i<links;++i){z.zones.push_back(i32(b,h+0x194+static_cast<std::size_t>(i)*4));z.zoneLinkTypes.push_back(i32(b,h+(crash3Retail?0x1d4:0x1b4)+static_cast<std::size_t>(i)*4));}
  z.zoneFlags=i32(b,h+(crash3Retail?0x2dc:0x29c));z.music=i32(b,h+(crash3Retail?0x2e4:0x2a4));
  z.x=i32(b,l);z.y=i32(b,l+4);z.z=i32(b,l+8);z.width=i32(b,l+12);z.height=i32(b,l+16);z.depth=i32(b,l+20);
  z.collisionDepthX=u16(b,l+0x1e);z.collisionDepthY=u16(b,l+0x20);z.collisionDepthZ=u16(b,l+0x22);
  for(std::size_t i=2;i<entry.items.size();++i){NsfEntity entity;if(!parseEntity(b,entry.items[i],i-2,entity)&&entity.error.empty())entity.error="Entity could not be decoded.";z.entities.push_back(std::move(entity));}
  if(static_cast<std::size_t>(z.cameraCount)>z.entities.size()){z.error="Zone camera count exceeds the number of entity items.";entry.zone=std::move(z);continue;}
  z.decoded=true;entry.zone=std::move(z);
 }
}



static bool decodeAnimationVertices(NsfAnimationFrame&frame,const NsfModel*model,bool crash3){
 auto fail=[&](const char*message){frame.vertexError=message;return false;};
 if(frame.vertexCount>16384U||frame.specialVertexCount>frame.vertexCount)return fail("Animation vertex count is invalid.");
 frame.vertices.assign(frame.vertexCount,{});
 for(std::size_t i=0;i<frame.specialVertexCount;++i){
  const auto&v=frame.specialVertices[i];frame.vertices[i]={v[0],v[1],v[2]};
 }
 std::vector<std::uint8_t> bits;bits.reserve(frame.temporalWords.size()*32U);
 for(const auto word:frame.temporalWords)for(unsigned bit=0;bit<32U;++bit)bits.push_back(static_cast<std::uint8_t>((word>>(31U-bit))&1U));
 const std::size_t start=frame.specialVertexCount*24U;
 if(model&&model->decoded&&model->compressed){
  if(model->positions.size()>frame.vertices.size())return fail("Model compressed-position count exceeds animation frame vertices.");
  std::size_t bit=start;std::int32_t xAcc=0,yAcc=0,zAcc=0;
  auto readSigned=[&](std::uint8_t width,std::int32_t&value)->bool{
   if(bit>=bits.size())return false;
   const bool negative=bits[bit++]!=0;value=negative?-(1<<width):0;
   for(unsigned b=0;b<width;++b){if(bit>=bits.size())return false;value|=static_cast<std::int32_t>(bits[bit++])<<(width-1U-b);}
   return true;
  };
  for(std::size_t i=0;i<model->positions.size();++i){
   const auto&p=model->positions[i];if(p.xBits==7U)xAcc=0;if(p.yBits==7U)yAcc=0;if(p.zBits==7U)zAcc=0;
   std::int32_t dx=0,dz=0,dy=0;
   if(!readSigned(p.xBits,dx)||!readSigned(p.zBits,dz)||!readSigned(p.yBits,dy))return fail("Animation compressed vertex bitstream is truncated.");
   xAcc=(xAcc+dx+(static_cast<std::int32_t>(p.x)<<1U))&0xff;
   yAcc=(yAcc+dy+p.y)&0xff;
   zAcc=(zAcc+dz+p.z)&0xff;
   // Match the established CrashEdit frame decoder's position ordering.
   if(i<frame.vertices.size())frame.vertices[i]={xAcc,yAcc,zAcc};
  }
 }else{
  const std::size_t required=(static_cast<std::size_t>(frame.vertexCount)-frame.specialVertexCount)*24U;
  if(bits.size()<required+start)return fail("Animation uncompressed vertex bitstream is truncated.");
  std::vector<std::uint8_t> reordered(bits.size());
  for(std::size_t base=0;base+32U<=bits.size();base+=32U)
   for(std::size_t byte=0;byte<4U;++byte)
    for(std::size_t bit=0;bit<8U;++bit)reordered[base+24U-byte*8U+bit]=bits[base+byte*8U+bit];
  std::size_t bit=start;
  auto readByte=[&](std::int32_t&value){value=0;for(unsigned b=0;b<8U;++b)value=(value<<1U)|reordered[bit++];};
  for(std::size_t i=frame.specialVertexCount;i<frame.vertexCount;++i){
   NsfAnimationVertex v{};readByte(v.x);readByte(v.y);readByte(v.z);frame.vertices[i]=v;
  }
 }
 if(crash3)for(auto&v:frame.vertices){v.x*=8;v.y*=8;v.z*=8;}
 frame.verticesDecoded=true;return true;
}

void NsfArchive::decodeAnimationModels(bool crash3){
 auto findAnimationModel=[&](std::uint32_t animationEid,std::size_t frameIndex,std::size_t frameCount)->std::uint32_t{
  std::vector<std::uint32_t> models;
  for(const auto&goolChunk:chunks_)if(goolChunk.type==0&&goolChunk.entryTableValid)
   for(const auto&goolEntry:goolChunk.entries)if(goolEntry.valid&&goolEntry.type==11&&goolEntry.gool.decoded&&goolEntry.gool.frameGroupsDecoded)
    for(const auto&group:goolEntry.gool.frameGroups)if(group.type==1U&&group.eid==animationEid&&isValidEid(group.modelEid)&&std::find(models.begin(),models.end(),group.modelEid)==models.end())models.push_back(group.modelEid);
  if(models.empty())return 0U;
  return models.size()==frameCount?models[frameIndex%models.size()]:models.front();
 };
 for(auto&chunk:chunks_){
  if(chunk.type!=0||!chunk.entryTableValid)continue;
  for(auto&entry:chunk.entries){
   if(!entry.valid)continue;
   if(entry.type==2U){
    NsfModel model;
    if(!parseModel(chunk.bytes,entry,model)&&model.error.empty())model.error="Model could not be decoded.";
    entry.model=std::move(model);
   }else if(entry.type==1U){
    NsfAnimation animation;animation.isCrash3=crash3;
    bool valid=true;
    for(const auto&item:entry.items){
     NsfAnimationFrame frame;
     if(!parseAnimationFrame(chunk.bytes,item,crash3,frame)){
      animation.error="Frame "+std::to_string(animation.frames.size())+": "+frame.error;
      valid=false;animation.frames.push_back(std::move(frame));break;
     }
     animation.frames.push_back(std::move(frame));
    }
    animation.decoded=valid;
    entry.animation=std::move(animation);
   }
  }
 }
 for(auto&chunk:chunks_)if(chunk.type==0&&chunk.entryTableValid)for(auto&entry:chunk.entries)if(entry.valid&&entry.type==1){
  for(std::size_t frameIndex=0;frameIndex<entry.animation.frames.size();++frameIndex){
   auto&frame=entry.animation.frames[frameIndex];
   frame.linkedModelEid=crash3?findAnimationModel(entry.eid,frameIndex,entry.animation.frames.size()):frame.modelEid;
   const NsfModel*linked=nullptr;
   for(const auto&modelChunk:chunks_)if(modelChunk.type==0&&modelChunk.entryTableValid)
    for(const auto&modelEntry:modelChunk.entries)
     if(modelEntry.valid&&modelEntry.type==2&&modelEntry.eid==frame.linkedModelEid&&modelEntry.model.decoded){linked=&modelEntry.model;break;}
   frame.modelResolved=linked!=nullptr;
   if(!decodeAnimationVertices(frame,linked,crash3)&&entry.animation.error.empty())entry.animation.error=frame.vertexError;
  }
 }
}

void NsfArchive::decodeGools(NsfGoolVersion version){
 for(auto&chunk:chunks_){
  if(chunk.type!=0||!chunk.entryTableValid)continue;
  for(auto&entry:chunk.entries){
   if(!entry.valid||entry.type!=11)continue;
   NsfGool gool;gool.version=version;
   auto fail=[&](const std::string&message){gool.error=message;entry.gool=std::move(gool);};
   if(entry.items.size()!=3&&entry.items.size()!=5&&entry.items.size()!=6){fail("GOOL entry must contain 3, 5, or 6 items.");continue;}
   const auto&header=entry.items[0];
   if(header.size!=24||!has(chunk.bytes,header.offset,header.size)){fail("GOOL header must be exactly 24 bytes.");continue;}
   const auto&code=entry.items[1];const auto&data=entry.items[2];
   if(code.size%4U!=0||!has(chunk.bytes,code.offset,code.size)){fail("GOOL instruction item is not a valid sequence of 32-bit words.");continue;}
   if(data.size%4U!=0||!has(chunk.bytes,data.offset,data.size)){fail("GOOL data pool is not a valid sequence of 32-bit words.");continue;}
   gool.id=i32(chunk.bytes,header.offset);
   gool.classId=i32(chunk.bytes,header.offset+4);
   gool.format=i32(chunk.bytes,header.offset+8);
   gool.heapBase=i32(chunk.bytes,header.offset+12);
   gool.eventCount=i32(chunk.bytes,header.offset+16);
   gool.entryCount=i32(chunk.bytes,header.offset+20);
   if(gool.format==1){
    if(entry.items.size()!=5&&entry.items.size()!=6){fail("GOOL format 1 requires state map and state descriptor items.");continue;}
    const auto&stateMap=entry.items[3];const auto&descriptors=entry.items[4];
    if(stateMap.size%2U!=0||!has(chunk.bytes,stateMap.offset,stateMap.size)){fail("GOOL state map has an invalid byte length.");continue;}
    if(descriptors.size%16U!=0||!has(chunk.bytes,descriptors.offset,descriptors.size)){fail("GOOL state descriptor item has an invalid byte length.");continue;}
    if(entry.items.size()==6&&!has(chunk.bytes,entry.items[5].offset,entry.items[5].size)){fail("GOOL frame-group item exceeds its chunk.");continue;}
    for(std::size_t p=0;p<stateMap.size;p+=2)gool.stateMap.push_back(static_cast<std::int16_t>(u16(chunk.bytes,stateMap.offset+p)));
    for(std::size_t p=0;p<descriptors.size;p+=16){
     const std::size_t q=descriptors.offset+p;
     gool.stateDescriptors.push_back({i32(chunk.bytes,q),i32(chunk.bytes,q+4),static_cast<std::int16_t>(u16(chunk.bytes,q+8)),static_cast<std::int16_t>(u16(chunk.bytes,q+10)),static_cast<std::int16_t>(u16(chunk.bytes,q+12)),static_cast<std::int16_t>(u16(chunk.bytes,q+14))});
    }
    if(entry.items.size()==6){gool.frameGroupBytes=entry.items[5].size;parseGoolFrameGroups(chunk.bytes,entry.items[5],gool,version);}
   }else if(gool.format==0){
    if(entry.items.size()!=3){fail("GOOL format 0 must contain exactly 3 items.");continue;}
   }else{fail("Unsupported GOOL format value "+std::to_string(gool.format)+".");continue;}
   for(std::size_t p=0;p<data.size;p+=4)gool.dataPool.push_back(i32(chunk.bytes,data.offset+p));
   bool mipsMode=false;
   for(std::size_t p=0;p<code.size;p+=4){
    const std::uint32_t word=u32(chunk.bytes,code.offset+p);
    NsfGoolInstruction instruction;instruction.byteOffset=p;instruction.word=word;instruction.isMips=mipsMode;
    if(mipsMode)decodeMipsInstruction(word,instruction.mnemonic,instruction.arguments,instruction.mipsKnown,p,code.size);
    else{
     instruction.opcode=static_cast<std::uint8_t>(word>>24U);instruction.operandBits=word&0x00FFFFFFU;
     const auto&info=goolOpcodeInfo(version,instruction.opcode);instruction.mnemonic=std::string(info.mnemonic);
     auto operands=decodeGoolOperands(version,instruction.opcode,instruction.operandBits);resolveGoolOperands(operands,gool);
     instruction.arguments=formatGoolOperands(operands);
     const auto controlFlow=formatGoolControlFlow(instruction.opcode,operands,gool,gool.instructions.size()-1U);
     if(!controlFlow.empty())instruction.arguments=controlFlow;
    }
    gool.instructions.push_back(std::move(instruction));
    if(mipsMode){
     if(gool.instructions.size()>=2U){const auto&previous=gool.instructions[gool.instructions.size()-2U];if(previous.isMips&&(previous.word==0x03E0A809U||previous.word==0x03E00008U))mipsMode=false;}
    }else if(version==NsfGoolVersion::Version2||version==NsfGoolVersion::Version3)mipsMode=gool.instructions.back().opcode==73U;
   }
   gool.decoded=true;entry.gool=std::move(gool);
  }
 }
 for(std::size_t sourceChunkIndex=0;sourceChunkIndex<chunks_.size();++sourceChunkIndex){
  auto& sourceChunk=chunks_[sourceChunkIndex];
  if(sourceChunk.type!=0||!sourceChunk.entryTableValid)continue;
  for(std::size_t sourceEntryIndex=0;sourceEntryIndex<sourceChunk.entries.size();++sourceEntryIndex){
   auto& sourceEntry=sourceChunk.entries[sourceEntryIndex];
   if(!sourceEntry.valid||sourceEntry.type!=11||!sourceEntry.gool.decoded)continue;
   auto& gool=sourceEntry.gool;
   gool.stateReferences.clear();
   for(std::size_t descriptorIndex=0;descriptorIndex<gool.stateDescriptors.size();++descriptorIndex){
    const auto dataPoolIndex=gool.stateDescriptors[descriptorIndex].goolIndex;
    NsfGoolStateReference reference;reference.descriptorIndex=descriptorIndex;reference.dataPoolIndex=dataPoolIndex;
    if(dataPoolIndex<0||static_cast<std::size_t>(dataPoolIndex)>=gool.dataPool.size()){
     reference.status=NsfGoolStateReferenceStatus::InvalidDataIndex;
     gool.stateReferences.push_back(reference);continue;
    }
    reference.eid=static_cast<std::uint32_t>(gool.dataPool[static_cast<std::size_t>(dataPoolIndex)]);
    // CrashEdit treats a Data Pool word equal to this entry's EID as local;
    // every other value is an external GOOL identity, whether or not its
    // resource is present in the currently loaded NSF.
    if(reference.eid==sourceEntry.eid){
     reference.status=NsfGoolStateReferenceStatus::Local;
     reference.targetChunkIndex=sourceChunkIndex;
     reference.targetEntryIndex=sourceEntryIndex;
     reference.targetFound=true;
    }else{
     reference.status=NsfGoolStateReferenceStatus::External;
     const auto allMatches=findEntriesByEid(reference.eid);
     std::vector<NsfEntryLocation> goolMatches;
     for(const auto& match:allMatches){
      const auto& targetChunk=chunks_[match.chunkIndex];
      if(match.entryIndex<targetChunk.entries.size()){
       const auto& targetEntry=targetChunk.entries[match.entryIndex];
       if(targetEntry.valid&&targetEntry.type==11)goolMatches.push_back(match);
      }
     }
     if(goolMatches.size()>1)reference.status=NsfGoolStateReferenceStatus::Ambiguous;
     else if(goolMatches.size()==1){
      reference.targetChunkIndex=goolMatches[0].chunkIndex;
      reference.targetEntryIndex=goolMatches[0].entryIndex;
      reference.targetFound=true;
     }
    }
    gool.stateReferences.push_back(reference);
   }
   gool.stateHookReferences.clear();
   for(std::size_t descriptorIndex=0;descriptorIndex<gool.stateDescriptors.size();++descriptorIndex){
    const auto& descriptor=gool.stateDescriptors[descriptorIndex];
    auto addHook=[&](NsfGoolStateHookKind kind,std::int16_t encodedHook){
     NsfGoolStateHookReference hook;hook.descriptorIndex=descriptorIndex;hook.kind=kind;
     hook.rawValue=static_cast<std::uint16_t>(encodedHook);
     hook.instructionOffset=static_cast<std::uint16_t>(hook.rawValue&0x3FFFU);
     hook.external=(hook.rawValue&0x4000U)!=0;
     if(hook.instructionOffset==0x3FFFU){hook.status=NsfGoolStateHookStatus::Absent;gool.stateHookReferences.push_back(hook);return;}
     const NsfGool* targetGool=nullptr;
     if(!hook.external){
      hook.targetChunkIndex=sourceChunkIndex;hook.targetEntryIndex=sourceEntryIndex;hook.targetFound=true;targetGool=&gool;
     }else{
      if(descriptorIndex>=gool.stateReferences.size()){
       hook.status=NsfGoolStateHookStatus::InvalidDataIndex;gool.stateHookReferences.push_back(hook);return;
      }
      const auto& stateReference=gool.stateReferences[descriptorIndex];
      if(stateReference.status==NsfGoolStateReferenceStatus::InvalidDataIndex){
       hook.status=NsfGoolStateHookStatus::InvalidDataIndex;gool.stateHookReferences.push_back(hook);return;
      }
      if(stateReference.status==NsfGoolStateReferenceStatus::Ambiguous){
       hook.status=NsfGoolStateHookStatus::AmbiguousTarget;gool.stateHookReferences.push_back(hook);return;
      }
      if(!stateReference.targetFound){
       hook.status=NsfGoolStateHookStatus::MissingTarget;gool.stateHookReferences.push_back(hook);return;
      }
      hook.targetChunkIndex=stateReference.targetChunkIndex;hook.targetEntryIndex=stateReference.targetEntryIndex;hook.targetFound=true;
      if(hook.targetChunkIndex>=chunks_.size()||hook.targetEntryIndex>=chunks_[hook.targetChunkIndex].entries.size()){
       hook.targetFound=false;hook.status=NsfGoolStateHookStatus::MissingTarget;gool.stateHookReferences.push_back(hook);return;
      }
      targetGool=&chunks_[hook.targetChunkIndex].entries[hook.targetEntryIndex].gool;
     }
     if(!targetGool||!targetGool->decoded)hook.status=NsfGoolStateHookStatus::TargetUndecoded;
     else if(static_cast<std::size_t>(hook.instructionOffset)>=targetGool->instructions.size())hook.status=NsfGoolStateHookStatus::OffsetOutOfRange;
     else hook.status=hook.external?NsfGoolStateHookStatus::External:NsfGoolStateHookStatus::Local;
     gool.stateHookReferences.push_back(hook);
    };
    addHook(NsfGoolStateHookKind::Event,descriptor.eventHook);
    addHook(NsfGoolStateHookKind::Transition,descriptor.transHook);
    addHook(NsfGoolStateHookKind::Code,descriptor.codeHook);
   }
  }
 }
}
bool NsfArchive::setEntityPropertyInt(std::size_t chunkIndex,std::size_t entryIndex,std::size_t entityIndex,std::uint16_t propertyId,std::size_t valueIndex,std::int32_t value){
 if(chunkIndex>=chunks_.size())return false;auto&chunk=chunks_[chunkIndex];if(entryIndex>=chunk.entries.size())return false;auto&entry=chunk.entries[entryIndex];if(!entry.zone.decoded||entityIndex>=entry.zone.entities.size())return false;auto&entity=entry.zone.entities[entityIndex];
 for(auto&property:entity.properties){if(static_cast<std::uint16_t>(property.id)!=propertyId||!property.valid||valueIndex>=property.valueOffsets.size()||valueIndex>=property.intValues.size())continue;
  const std::size_t offset=property.valueOffsets[valueIndex];if(property.type==18U){if(value<-32768||value>32767||!has(chunk.bytes,offset,2))return false;chunk.bytes[offset]=static_cast<std::uint8_t>(value);chunk.bytes[offset+1]=static_cast<std::uint8_t>(static_cast<std::uint16_t>(value)>>8);}
  else if((property.type==5U||property.type==19U||property.type==3U||property.type==4U)&&has(chunk.bytes,offset,4)){const auto raw=static_cast<std::uint32_t>(value);for(int n=0;n<4;++n)chunk.bytes[offset+static_cast<std::size_t>(n)]=static_cast<std::uint8_t>(raw>>(8*n));}
  else return false;
  property.intValues[valueIndex]=value;return true;
 }
 return false;
}
bool NsfArchive::saveUncompressedBytes(std::vector<std::uint8_t>&out,std::string&e)const{
 out.clear();out.reserve(chunks_.size()*NsfChunkSize);
 for(const auto&c:chunks_){if(c.bytes.size()!=NsfChunkSize){e="Invalid in-memory NSF chunk size.";return false;}auto b=c.bytes;checksum(b);out.insert(out.end(),b.begin(),b.end());}
 e.clear();return true;
}
bool NsfArchive::saveUncompressed(const std::filesystem::path&f,std::string&e)const{
 std::vector<std::uint8_t> bytes;if(!saveUncompressedBytes(bytes,e))return false;
 std::ofstream s(f,std::ios::binary|std::ios::trunc);if(!s){e="Could not create NSF output.";return false;}
 if(!bytes.empty())s.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
 if(!s){e="Failed writing NSF output.";return false;}e.clear();return true;
}
bool NsfArchive::setGoolStateDescriptor(std::size_t chunkIndex,std::size_t entryIndex,std::size_t descriptorIndex,const NsfGoolStateDescriptor& descriptor){
 if(chunkIndex>=chunks_.size())return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=11||!entry.gool.decoded||entry.gool.format!=1||entry.items.size()<5||descriptorIndex>=entry.gool.stateDescriptors.size())return false;
 const auto& item=entry.items[4];const std::size_t offset=item.offset+descriptorIndex*16U;
 if(!has(chunk.bytes,offset,16U))return false;
 const auto write32=[&](std::size_t p,std::uint32_t value){for(unsigned i=0;i<4;++i)chunk.bytes[p+i]=static_cast<std::uint8_t>(value>>(i*8U));};
 const auto write16=[&](std::size_t p,std::uint16_t value){chunk.bytes[p]=static_cast<std::uint8_t>(value);chunk.bytes[p+1]=static_cast<std::uint8_t>(value>>8U);};
 write32(offset,static_cast<std::uint32_t>(descriptor.stateFlags));
 write32(offset+4U,static_cast<std::uint32_t>(descriptor.blockFlags));
 write16(offset+8U,static_cast<std::uint16_t>(descriptor.goolIndex));
 write16(offset+10U,static_cast<std::uint16_t>(descriptor.eventHook));
 write16(offset+12U,static_cast<std::uint16_t>(descriptor.transHook));
 write16(offset+14U,static_cast<std::uint16_t>(descriptor.codeHook));
 entry.gool.stateDescriptors[descriptorIndex]=descriptor;
 return true;
}
bool NsfArchive::setGoolDataPoolWord(std::size_t chunkIndex,std::size_t entryIndex,std::size_t wordIndex,std::uint32_t value){
 if(chunkIndex>=chunks_.size())return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=11||!entry.gool.decoded||entry.items.size()<3||wordIndex>=entry.gool.dataPool.size())return false;
 const auto& item=entry.items[2];const std::size_t offset=item.offset+wordIndex*4U;
 if(!has(chunk.bytes,offset,4U))return false;
 for(unsigned i=0;i<4;++i)chunk.bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8U));
 entry.gool.dataPool[wordIndex]=static_cast<std::int32_t>(value);
 return true;
}

bool NsfArchive::setGoolFrameGroupEid(std::size_t chunkIndex,std::size_t entryIndex,std::size_t groupIndex,std::uint32_t eid){
 if(!isValidEid(eid)||chunkIndex>=chunks_.size())return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=11||!entry.gool.decoded||!entry.gool.frameGroupsDecoded||entry.items.size()<6||groupIndex>=entry.gool.frameGroups.size())return false;
 const auto& item=entry.items[5];const auto& group=entry.gool.frameGroups[groupIndex];
 if(group.byteOffset>item.size||group.byteSize>item.size-group.byteOffset||
    group.eidFieldOffset>group.byteSize||4U>group.byteSize-group.eidFieldOffset)return false;
 const std::size_t offset=item.offset+group.byteOffset+group.eidFieldOffset;
 if(!has(chunk.bytes,offset,4U))return false;
 for(unsigned i=0;i<4;++i)chunk.bytes[offset+i]=static_cast<std::uint8_t>(eid>>(8U*i));
 entry.gool.frameGroups[groupIndex].eid=eid;
 return true;
}
bool NsfArchive::setGoolFrameGroupFrameCount(std::size_t chunkIndex,std::size_t entryIndex,std::size_t groupIndex,std::uint16_t frameCount){
 if(frameCount>255U||chunkIndex>=chunks_.size())return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=11||!entry.gool.decoded||!entry.gool.frameGroupsDecoded||entry.items.size()<6||groupIndex>=entry.gool.frameGroups.size())return false;
 const auto& item=entry.items[5];const auto& group=entry.gool.frameGroups[groupIndex];
 if(group.type!=1U||group.byteOffset>item.size||group.byteSize<12U||12U>item.size-group.byteOffset)return false;
 const std::size_t offset=item.offset+group.byteOffset+2U;
 if(!has(chunk.bytes,offset,2U))return false;
 chunk.bytes[offset]=static_cast<std::uint8_t>(frameCount);
 chunk.bytes[offset+1U]=static_cast<std::uint8_t>(frameCount>>8U);
 entry.gool.frameGroups[groupIndex].frameCount=static_cast<std::int16_t>(frameCount);
 return true;
}
bool NsfArchive::setGoolFrameGroupInterpolated(std::size_t chunkIndex,std::size_t entryIndex,std::size_t groupIndex,bool interpolated){
 if(chunkIndex>=chunks_.size())return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=11||!entry.gool.decoded||!entry.gool.frameGroupsDecoded||entry.items.size()<6||groupIndex>=entry.gool.frameGroups.size())return false;
 const auto& item=entry.items[5];const auto& group=entry.gool.frameGroups[groupIndex];
 if(group.type!=1U||group.byteOffset>item.size||group.byteSize>item.size-group.byteOffset||
    group.interpolatedFieldOffset>group.byteSize||4U>group.byteSize-group.interpolatedFieldOffset)return false;
 const std::size_t offset=item.offset+group.byteOffset+group.interpolatedFieldOffset;
 if(!has(chunk.bytes,offset,4U))return false;
 const std::uint32_t value=interpolated?1U:0U;
 for(unsigned i=0;i<4;++i)chunk.bytes[offset+i]=static_cast<std::uint8_t>(value>>(8U*i));
 entry.gool.frameGroups[groupIndex].interpolated=interpolated;
 return true;
}
bool NsfArchive::setAnimationFrameHeaderValue(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,std::uint32_t field,std::int32_t value){
 if(chunkIndex>=chunks_.size())return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=1||!entry.animation.decoded||frameIndex>=entry.animation.frames.size())return false;
 auto& animation=entry.animation;
 auto& frame=animation.frames[frameIndex];
 const std::size_t fixedHeader=animation.isCrash3?28U:24U;
 const std::size_t offset=frame.rawOffset;
 if(frame.rawSize<fixedHeader||!has(chunk.bytes,offset,frame.rawSize))return false;
 std::size_t fieldOffset=0;
 switch(field){
  case 0: fieldOffset=0U; if(value<INT16_MIN||value>INT16_MAX)return false; frame.xOffset=static_cast<std::int16_t>(value); break;
  case 1: fieldOffset=2U; if(value<INT16_MIN||value>INT16_MAX)return false; frame.yOffset=static_cast<std::int16_t>(value); break;
  case 2: fieldOffset=4U; if(value<INT16_MIN||value>INT16_MAX)return false; frame.zOffset=static_cast<std::int16_t>(value); break;
  case 3: fieldOffset=6U; if(value<INT16_MIN||value>INT16_MAX)return false; frame.unknown=static_cast<std::int16_t>(value); break;
  case 4:
   if(animation.isCrash3||value<0)return false;
   fieldOffset=16U;frame.modelEid=static_cast<std::uint32_t>(value);break;
  default:return false;
 }
 if(fieldOffset+2U>fixedHeader)return false;
 if(field==4U){
  for(unsigned i=0;i<4U;++i)chunk.bytes[offset+fieldOffset+i]=static_cast<std::uint8_t>(static_cast<std::uint32_t>(value)>>(i*8U));
 }else{
  const auto v=static_cast<std::uint16_t>(static_cast<std::int16_t>(value));
  chunk.bytes[offset+fieldOffset]=static_cast<std::uint8_t>(v);
  chunk.bytes[offset+fieldOffset+1U]=static_cast<std::uint8_t>(v>>8U);
 }
 return true;
}
bool NsfArchive::setAnimationFrameCollisionValue(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,std::size_t collisionIndex,std::size_t valueIndex,std::int32_t value){
 if(chunkIndex>=chunks_.size()||valueIndex>=10U)return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=1||!entry.animation.decoded||frameIndex>=entry.animation.frames.size())return false;
 auto& frame=entry.animation.frames[frameIndex];
 if(collisionIndex>=frame.collisions.size())return false;
 const std::size_t fixedHeader=entry.animation.isCrash3?28U:24U;
 const std::size_t relative=fixedHeader+collisionIndex*0x28U+valueIndex*4U;
 if(relative>frame.rawSize||4U>frame.rawSize-relative||!has(chunk.bytes,frame.rawOffset+relative,4U))return false;
 const std::uint32_t raw=static_cast<std::uint32_t>(value);
 for(unsigned i=0;i<4U;++i)chunk.bytes[frame.rawOffset+relative+i]=static_cast<std::uint8_t>(raw>>(i*8U));
 frame.collisions[collisionIndex].values[valueIndex]=value;
 return true;
}
bool NsfArchive::setAnimationFrameSpecialVertex(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,std::size_t vertexIndex,std::size_t axis,std::uint8_t value){
 if(chunkIndex>=chunks_.size()||axis>=3U)return false;
 auto& chunk=chunks_[chunkIndex];
 if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto& entry=chunk.entries[entryIndex];
 if(!entry.valid||entry.type!=1||!entry.animation.decoded||frameIndex>=entry.animation.frames.size())return false;
 auto& frame=entry.animation.frames[frameIndex];
 if(vertexIndex>=frame.specialVertices.size()||vertexIndex>=frame.specialVertexCount)return false;
 const std::size_t fixedHeader=entry.animation.isCrash3?28U:24U;
 const std::size_t relative=fixedHeader+static_cast<std::size_t>(frame.collisionCount)*0x28U+vertexIndex*3U+axis;
 if(relative>=frame.rawSize||!has(chunk.bytes,frame.rawOffset+relative,1U))return false;
 chunk.bytes[frame.rawOffset+relative]=value;
 frame.specialVertices[vertexIndex][axis]=value;
 if(frame.verticesDecoded){
  const NsfModel*linkedModel=nullptr;
  for(const auto&modelChunk:chunks_){if(modelChunk.type!=0||!modelChunk.entryTableValid)continue;for(const auto&modelEntry:modelChunk.entries)if(modelEntry.valid&&modelEntry.type==2&&modelEntry.eid==frame.linkedModelEid&&modelEntry.model.decoded){linkedModel=&modelEntry.model;break;}if(linkedModel)break;}
  decodeAnimationVertices(frame,linkedModel,entry.animation.isCrash3);
 }
 return true;
}
bool NsfArchive::setAnimationFrameVertex(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,std::size_t vertexIndex,std::size_t axis,std::int32_t value){
 if(chunkIndex>=chunks_.size()||axis>=3U)return false;
 auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=1||!entry.animation.decoded||frameIndex>=entry.animation.frames.size())return false;
 auto&frame=entry.animation.frames[frameIndex];if(!frame.verticesDecoded||vertexIndex>=frame.vertexCount)return false;
 const bool crash3=entry.animation.isCrash3;
 if(crash3){if(value%8!=0)return false;value/=8;}
 if(value<0||value>255)return false;
 const std::size_t fixedHeader=crash3?28U:24U;
 const std::size_t fakeHeader=fixedHeader+static_cast<std::size_t>(frame.collisionCount)*0x28U;
 if(frame.rawSize<fakeHeader||!has(chunk.bytes,frame.rawOffset,frame.rawSize))return false;
 const NsfModel*model=nullptr;
 for(const auto&modelChunk:chunks_)if(modelChunk.type==0&&modelChunk.entryTableValid)
  for(const auto&modelEntry:modelChunk.entries)
   if(modelEntry.valid&&modelEntry.type==2&&modelEntry.eid==frame.linkedModelEid&&modelEntry.model.decoded){model=&modelEntry.model;break;}
 if(!(model&&model->compressed)&&vertexIndex<frame.specialVertexCount){
  if(value>255)return false;
  if(!setAnimationFrameSpecialVertex(chunkIndex,entryIndex,frameIndex,vertexIndex,axis,static_cast<std::uint8_t>(value)))return false;
  return decodeAnimationVertices(frame,model,crash3);
 }
 const std::size_t temporalOffset=frame.rawOffset+fakeHeader;
 if(!has(chunk.bytes,temporalOffset,frame.temporalWords.size()*4U))return false;
 std::vector<std::uint8_t>bits;bits.reserve(frame.temporalWords.size()*32U);
 for(std::size_t wordIndex=0;wordIndex<frame.temporalWords.size();++wordIndex){const auto word=u32(chunk.bytes,temporalOffset+wordIndex*4U);for(unsigned bit=0;bit<32U;++bit)bits.push_back(static_cast<std::uint8_t>((word>>(31U-bit))&1U));}
 const std::size_t start=frame.specialVertexCount*24U;
 if(model&&model->compressed){
  if(vertexIndex>=model->positions.size()||model->positions.size()>frame.vertices.size())return false;
  std::size_t bit=start;std::int32_t xAcc=0,yAcc=0,zAcc=0;
  auto toByte=[&](std::int32_t source,std::int32_t&target){if(crash3){if(source%8!=0)return false;source/=8;}if(source<0||source>255)return false;target=source;return true;};
  auto encodeSigned=[&](std::uint8_t width,std::int32_t delta)->bool{
   if(bit>=bits.size())return false;const std::int32_t minValue=-(1<<width),maxValue=(1<<width)-1;
   if(delta<minValue||delta>maxValue)return false;
   bits[bit++]=delta<0?1U:0U;const std::uint32_t encoded=static_cast<std::uint32_t>(delta<0?delta+(1<<width):delta);
   for(unsigned b=0;b<width;++b){if(bit>=bits.size())return false;bits[bit++]=static_cast<std::uint8_t>((encoded>>(width-1U-b))&1U);}
   return true;
  };
  for(std::size_t i=0;i<model->positions.size();++i){
   const auto&p=model->positions[i];if(p.xBits==7U)xAcc=0;if(p.yBits==7U)yAcc=0;if(p.zBits==7U)zAcc=0;
   std::int32_t targetX=0,targetY=0,targetZ=0;
   const auto&source=frame.vertices[i];
   if(!toByte(i==vertexIndex&&axis==0U?value:source.x,targetX)||!toByte(i==vertexIndex&&axis==1U?value:source.y,targetY)||!toByte(i==vertexIndex&&axis==2U?value:source.z,targetZ))return false;
   const std::int32_t baseX=(xAcc+(static_cast<std::int32_t>(p.x)<<1U))&0xff;
   const std::int32_t baseZ=(zAcc+p.z)&0xff;
   const std::int32_t baseY=(yAcc+p.y)&0xff;
   auto deltaFor=[](std::int32_t target,std::int32_t base){std::int32_t delta=(target-base)%256;if(delta<0)delta+=256;return delta>127?delta-256:delta;};
   const std::int32_t dx=deltaFor(targetX,baseX),dz=deltaFor(targetZ,baseZ),dy=deltaFor(targetY,baseY);
   if(!encodeSigned(p.xBits,dx)||!encodeSigned(p.zBits,dz)||!encodeSigned(p.yBits,dy))return false;
   xAcc=targetX;yAcc=targetY;zAcc=targetZ;
  }
 }else{
  const std::size_t required=start+(static_cast<std::size_t>(frame.vertexCount)-frame.specialVertexCount)*24U;
  if(bits.size()<required||vertexIndex<frame.specialVertexCount)return false;
  std::vector<std::uint8_t>reordered(bits.size());
  for(std::size_t base=0;base+32U<=bits.size();base+=32U)
   for(std::size_t byte=0;byte<4U;++byte)
    for(std::size_t b=0;b<8U;++b)reordered[base+24U-byte*8U+b]=bits[base+byte*8U+b];
  std::size_t bit=start+(vertexIndex-frame.specialVertexCount)*24U;
  const std::int32_t current[3]={frame.vertices[vertexIndex].x,frame.vertices[vertexIndex].y,frame.vertices[vertexIndex].z};
  for(std::size_t component=0;component<3U;++component){
   const std::int32_t v=component==axis?value:(crash3?current[component]/8:current[component]);
   if(v<0||v>255)return false;
   for(unsigned b=0;b<8U;++b)reordered[bit++]=static_cast<std::uint8_t>((static_cast<std::uint32_t>(v)>>(7U-b))&1U);
  }
  for(std::size_t base=0;base+32U<=bits.size();base+=32U)
   for(std::size_t byte=0;byte<4U;++byte)
    for(std::size_t b=0;b<8U;++b)bits[base+byte*8U+b]=reordered[base+24U-byte*8U+b];
 }
 for(std::size_t wordIndex=0;wordIndex<frame.temporalWords.size();++wordIndex){
  std::uint32_t word=0;for(unsigned bit=0;bit<32U;++bit)word=(word<<1U)|bits[wordIndex*32U+bit];
  frame.temporalWords[wordIndex]=word;
  const std::size_t offset=temporalOffset+wordIndex*4U;if(!has(chunk.bytes,offset,4U))return false;
  for(unsigned byte=0;byte<4U;++byte)chunk.bytes[offset+byte]=static_cast<std::uint8_t>(word>>(byte*8U));
 }
 return decodeAnimationVertices(frame,model,crash3);
}

bool NsfArchive::setModelScale(std::size_t chunkIndex,std::size_t entryIndex,std::size_t axis,std::int32_t value){
 if(chunkIndex>=chunks_.size()||axis>=3U)return false;
 auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=2||!entry.model.decoded||entry.items.size()<1U)return false;
 const auto&item=entry.items[0];const std::size_t offset=item.offset+axis*4U;if(axis*4U+4U>item.size||!has(chunk.bytes,offset,4U))return false;
 const std::uint32_t raw=static_cast<std::uint32_t>(value);for(unsigned i=0;i<4U;++i)chunk.bytes[offset+i]=static_cast<std::uint8_t>(raw>>(i*8U));
 if(axis==0U)entry.model.scaleX=value;else if(axis==1U)entry.model.scaleY=value;else entry.model.scaleZ=value;
 for(unsigned i=0;i<4U;++i)entry.model.header[axis*4U+i]=static_cast<std::uint8_t>(raw>>(i*8U));
 return true;
}
bool NsfArchive::setModelTexturePageEid(std::size_t chunkIndex,std::size_t entryIndex,std::size_t pageIndex,std::uint32_t eid){
 if(!isValidEid(eid)||chunkIndex>=chunks_.size())return false;
 auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=2||!entry.model.decoded||pageIndex>=entry.model.texturePageEids.size()||entry.items.empty())return false;
 const std::size_t relative=0x0cU+pageIndex*4U;if(relative+4U>entry.items[0].size)return false;
 const std::size_t offset=entry.items[0].offset+relative;if(!has(chunk.bytes,offset,4U))return false;
 for(unsigned i=0;i<4U;++i){const auto b=static_cast<std::uint8_t>(eid>>(i*8U));chunk.bytes[offset+i]=b;entry.model.header[relative+i]=b;}
 entry.model.texturePageEids[pageIndex]=eid;return true;
}
bool NsfArchive::setModelColor(std::size_t chunkIndex,std::size_t entryIndex,std::size_t colorIndex,const NsfSceneryColor&color){
 if(chunkIndex>=chunks_.size())return false;auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=2||!entry.model.decoded||colorIndex>=entry.model.colors.size()||entry.items.size()<3U)return false;
 const auto&item=entry.items[2];const std::size_t relative=colorIndex*4U;if(relative+4U>item.size)return false;const std::size_t p=item.offset+relative;if(!has(chunk.bytes,p,4U))return false;
 chunk.bytes[p]=color.r;chunk.bytes[p+1U]=color.g;chunk.bytes[p+2U]=color.b;chunk.bytes[p+3U]=color.extra;entry.model.colors[colorIndex]=color;return true;
}
bool NsfArchive::setModelTexture(std::size_t chunkIndex,std::size_t entryIndex,std::size_t textureIndex,const NsfModelTexture&texture){
 if(chunkIndex>=chunks_.size())return false;auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=2||!entry.model.decoded||textureIndex>=entry.model.textures.size()||entry.items.size()<4U)return false;
 if(texture.clutX>15U||texture.clutY2>15U||texture.colorMode>3U||texture.blendMode>3U||texture.segment>15U||texture.page>=entry.model.texturePageEids.size()||texture.page>63U)return false;
 const auto&item=entry.items[3];const std::size_t relative=textureIndex*12U;if(relative+12U>item.size)return false;const std::size_t p=item.offset+relative;if(!has(chunk.bytes,p,12U))return false;
 const std::uint8_t old6=chunk.bytes[p+6U],old7=chunk.bytes[p+7U];
 chunk.bytes[p]=texture.u1;chunk.bytes[p+1U]=texture.v1;
 chunk.bytes[p+2U]=static_cast<std::uint8_t>((texture.clutX&0x0fU)|((texture.clutY2&0x0fU)<<4U));
 chunk.bytes[p+3U]=texture.clutY1;chunk.bytes[p+4U]=texture.u2;chunk.bytes[p+5U]=texture.v2;
 chunk.bytes[p+6U]=static_cast<std::uint8_t>((old6&0x10U)|((texture.colorMode&1U)<<7U)|((texture.blendMode&3U)<<5U)|(texture.segment&0x0fU));
 chunk.bytes[p+7U]=static_cast<std::uint8_t>((old7&0x02U)|((texture.colorMode>>1U)&1U)|((texture.page&0x3fU)<<2U));
 chunk.bytes[p+8U]=texture.u3;chunk.bytes[p+9U]=texture.v3;chunk.bytes[p+10U]=texture.u4;chunk.bytes[p+11U]=texture.v4;
 entry.model.textures[textureIndex]=texture;return true;
}
bool NsfArchive::setModelExtendedTextureWord(std::size_t chunkIndex,std::size_t entryIndex,std::size_t textureIndex,std::uint32_t raw){
 if(chunkIndex>=chunks_.size())return false;auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=2||!entry.model.decoded||textureIndex>=entry.model.extendedTextureWords.size()||entry.items.size()<5U)return false;
 const auto&item=entry.items[4];const std::size_t relative=textureIndex*4U;if(relative+4U>item.size)return false;const std::size_t p=item.offset+relative;if(!has(chunk.bytes,p,4U))return false;
 for(unsigned i=0;i<4U;++i)chunk.bytes[p+i]=static_cast<std::uint8_t>(raw>>(i*8U));
 entry.model.extendedTextureWords[textureIndex]=raw;NsfModelExtendedTexture record;record.raw=raw;record.offset=static_cast<std::uint16_t>(raw&0x7ffU);record.isLod=(raw&0x80000000U)!=0;record.leap=(raw&(1U<<11U))!=0;record.mask=static_cast<std::uint8_t>((raw>>12U)&0x7fU);record.delay=static_cast<std::uint8_t>((raw>>19U)&0x7fU);record.latency=static_cast<std::uint8_t>((raw>>26U)&0x1fU);for(std::size_t level=0;level<8U;++level)record.lod[level]=static_cast<std::uint8_t>((raw>>(29U-level*2U))&3U);entry.model.extendedTextures[textureIndex]=record;return true;
}
bool NsfArchive::setModelTriangleTexture(std::size_t chunkIndex,std::size_t entryIndex,std::size_t triangleIndex,std::uint8_t textureIndex,bool animated){
 if(chunkIndex>=chunks_.size())return false;auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=2||!entry.model.decoded||!entry.model.polygonsDecoded||triangleIndex>=entry.model.triangles.size()||entry.items.size()<2U)return false;
 if(animated?(textureIndex>=entry.model.extendedTextureWords.size()):(textureIndex!=0U&&static_cast<std::size_t>(textureIndex-1U)>=entry.model.textures.size()))return false;
 const auto&triangle=entry.model.triangles[triangleIndex];if(triangle.commandWordIndex>=entry.model.polygonWords.size())return false;
 const auto&item=entry.items[1];const std::size_t relative=triangle.commandWordIndex*4U;if(relative+4U>item.size)return false;const std::size_t p=item.offset+relative;if(!has(chunk.bytes,p,4U))return false;
 std::uint32_t raw=u32(chunk.bytes,p);raw=(raw&~0x1ffU)|textureIndex|(animated?0x100U:0U);
 for(unsigned i=0;i<4U;++i)chunk.bytes[p+i]=static_cast<std::uint8_t>(raw>>(i*8U));
 entry.model.polygonWords[triangle.commandWordIndex]=raw;entry.model.polygonsDecoded=decodeModelPolygons(entry.model);return entry.model.polygonsDecoded;
}
bool NsfArchive::setModelTriangleColorIndex(std::size_t chunkIndex,std::size_t entryIndex,std::size_t triangleIndex,std::size_t corner,std::uint8_t colorIndex){
 if(chunkIndex>=chunks_.size()||corner>=3U)return false;auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=2||!entry.model.decoded||triangleIndex>=entry.model.triangles.size()||colorIndex>=entry.model.colors.size()||entry.items.size()<2U)return false;
 const auto&triangle=entry.model.triangles[triangleIndex];const std::size_t wordIndex=corner==0U?triangle.colorWordIndexA:corner==1U?triangle.colorWordIndexB:triangle.colorWordIndexC;
 const std::uint8_t bitOffset=corner==0U?triangle.colorBitOffsetA:corner==1U?triangle.colorBitOffsetB:triangle.colorBitOffsetC;
 if(wordIndex>=entry.model.polygonWords.size()||bitOffset>25U||colorIndex>127U)return false;
 const auto&item=entry.items[1];const std::size_t relative=wordIndex*4U;if(relative+4U>item.size)return false;const std::size_t p=item.offset+relative;if(!has(chunk.bytes,p,4U))return false;
 std::uint32_t raw=u32(chunk.bytes,p);const std::uint32_t mask=0x7fU<<bitOffset;raw=(raw&~mask)|(static_cast<std::uint32_t>(colorIndex)<<bitOffset);
 for(unsigned i=0;i<4U;++i)chunk.bytes[p+i]=static_cast<std::uint8_t>(raw>>(i*8U));
 entry.model.polygonWords[wordIndex]=raw;entry.model.polygonsDecoded=decodeModelPolygons(entry.model);return entry.model.polygonsDecoded;
}

int resolveNsfModelTextureIndex(const NsfModel&model,std::uint8_t textureIndex,bool animated,std::uint32_t realFrame,bool&valid,std::uint8_t lodLevel){
 valid=true;int textureId=static_cast<int>(textureIndex)-1;
 if(animated){
  ++textureId;if(textureId<0||static_cast<std::size_t>(textureId)>=model.extendedTextureWords.size()){valid=false;return -1;}
  std::uint32_t anim=model.extendedTextureWords[static_cast<std::size_t>(textureId)];const int offset=static_cast<int>(anim&0x7ffU);
  if(offset==0)return -1;
  textureId=offset-1;
  if((anim&0x80000000U)!=0){const unsigned shift=29U-static_cast<unsigned>(std::min<std::uint8_t>(lodLevel,7U))*2U;textureId+=static_cast<int>((anim>>shift)&3U);}
  else{
   const int latency=static_cast<int>((anim>>26U)&0x1fU),delay=static_cast<int>((anim>>19U)&0x7fU),mask=static_cast<int>((anim>>12U)&0x7fU);
   textureId+=(static_cast<int>(realFrame)/(1+latency)+delay)&mask;
   if((anim&(1U<<11U))!=0){
    ++textureId;if(textureId<0||static_cast<std::size_t>(textureId)>=model.extendedTextureWords.size()){valid=false;return -1;}
    anim=model.extendedTextureWords[static_cast<std::size_t>(textureId)];
    const unsigned shift=29U-static_cast<unsigned>(std::min<std::uint8_t>(lodLevel,7U))*2U;
    textureId=static_cast<int>(anim&0x7ffU)-1+static_cast<int>((anim>>shift)&3U);
   }
  }
 }
 if(textureId>=0&&static_cast<std::size_t>(textureId)>=model.textures.size())valid=false;
 return textureId;
}

std::vector<NsfEntryLocation> NsfArchive::findEntriesByEid(std::uint32_t eid) const {
 std::vector<NsfEntryLocation> matches;
 for(std::size_t chunkIndex=0;chunkIndex<chunks_.size();++chunkIndex){
  const auto& chunk=chunks_[chunkIndex];
  if(chunk.type!=0||!chunk.entryTableValid)continue;
  for(std::size_t entryIndex=0;entryIndex<chunk.entries.size();++entryIndex){
   const auto& entry=chunk.entries[entryIndex];
   if(entry.valid&&entry.eid==eid)matches.push_back({chunkIndex,entryIndex});
  }
 }
 return matches;
}
std::size_t NsfArchive::compressedChunkCount()const noexcept{return std::count_if(chunks_.begin(),chunks_.end(),[](const auto&c){return c.wasCompressed;});}
}
