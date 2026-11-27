#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/eid.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>
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
bool NsfArchive::loadBytes(const std::vector<std::uint8_t>&in,std::string&e){crash3Retail_=false;goolDecoded_=false;animationModelsDecoded_=false;animationModelsCrash3_=false;goolVersion_=NsfGoolVersion::Version2;std::vector<NsfChunk> parsed;std::size_t p=0;while(p<in.size()){NsfChunk c;if(!decodeChunk(in,p,c,e))return false;parsed.push_back(std::move(c));}chunks_=std::move(parsed);return true;}
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

void NsfArchive::decodeZones(bool crash3Retail){crash3Retail_=crash3Retail;
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
 // Compressed deltas are read in the original temporal-word bit order.
 // Only uncompressed byte coordinates use the per-word byte permutation below.
 const auto&compressedBits=bits;
 const std::size_t start=frame.specialVertexCount*24U;
 if(model&&model->decoded&&model->compressed){
  if(model->positions.size()+frame.specialVertexCount!=frame.vertices.size())return fail("Model compressed-position count and frame special-vertex count do not match the frame vertex count.");
  std::size_t bit=start;std::int32_t xAcc=0,yAcc=0,zAcc=0;
  auto readSigned=[&](std::uint8_t width,std::int32_t&value)->bool{
   if(bit>=compressedBits.size())return false;
   const bool negative=compressedBits[bit++]!=0;value=negative?-(1<<width):0;
   for(unsigned b=0;b<width;++b){if(bit>=compressedBits.size())return false;value|=static_cast<std::int32_t>(compressedBits[bit++])<<(width-1U-b);}
   return true;
  };
  for(std::size_t i=0;i<model->positions.size();++i){
   const auto&p=model->positions[i];if(p.xBits==7U)xAcc=0;if(p.yBits==7U)yAcc=0;if(p.zBits==7U)zAcc=0;
   std::int32_t dx=0,dz=0,dy=0;
   if(!readSigned(p.xBits,dx)||!readSigned(p.zBits,dz)||!readSigned(p.yBits,dy))return fail("Animation compressed vertex bitstream is truncated.");
   xAcc=(xAcc+dx+(static_cast<std::int32_t>(p.x)<<1U))&0xff;
   yAcc=(yAcc+dy+p.y)&0xff;
   zAcc=(zAcc+dz+p.z)&0xff;
   // Special vertices occupy the leading frame slots and are stored in the
   // frame header; compressed positions begin immediately after that prefix.
   frame.vertices[i]={xAcc,yAcc,zAcc};
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
 animationModelsDecoded_=true;animationModelsCrash3_=crash3;
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
 goolVersion_=version;goolDecoded_=true;
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
bool NsfArchive::saveUncompressedSourceBytes(std::vector<std::uint8_t>&out,std::string&e)const{
 out.clear();out.reserve(chunks_.size()*NsfChunkSize);
 for(const auto&c:chunks_){if(c.bytes.size()!=NsfChunkSize){e="Invalid in-memory NSF chunk size.";return false;}auto b=c.bytes;checksum(b);out.insert(out.end(),b.begin(),b.end());}
 e.clear();return true;
}
bool NsfArchive::saveUncompressedBytes(std::vector<std::uint8_t>&out,std::string&e)const{
 NsfArchive candidate=*this;
 if(!candidate.commitAnimationEdits(e)){out.clear();return false;}
 return candidate.saveUncompressedSourceBytes(out,e);
}
bool NsfArchive::commitAnimationEdits(std::string&e){
 NsfArchive candidate=*this;
 // A compressed ModelPosition table is shared by every Animation that
 // references its EID. Only isolate an Animation when a non-mutating save probe
 // proves its current per-vertex widths cannot encode the edited pose.
 struct ModelCloneTarget { std::size_t chunkIndex,entryIndex; std::uint32_t modelEid; };
 std::vector<ModelCloneTarget> cloneTargets;
 auto requiresModelExpansion=[&](std::size_t ci,std::size_t ei,std::size_t fi)->bool{
  if(ci>=candidate.chunks_.size()||ei>=candidate.chunks_[ci].entries.size())return false;
  const auto&sourceEntry=candidate.chunks_[ci].entries[ei];
  if(!sourceEntry.valid||sourceEntry.type!=1U||fi>=sourceEntry.animation.frames.size())return false;
  const auto&sourceFrame=sourceEntry.animation.frames[fi];
  if(!sourceFrame.verticesEdited||!sourceFrame.verticesDecoded||sourceFrame.vertices.empty())return false;
  NsfArchive probe=candidate;
  auto&probeFrame=probe.chunks_[ci].entries[ei].animation.frames[fi];
  std::int64_t minimum[3]={INT64_MAX,INT64_MAX,INT64_MAX},maximum[3]={INT64_MIN,INT64_MIN,INT64_MIN};
  for(const auto&v:probeFrame.vertices){const std::int32_t values[3]={v.x,v.y,v.z};for(unsigned axis=0;axis<3U;++axis){minimum[axis]=std::min<std::int64_t>(minimum[axis],values[axis]);maximum[axis]=std::max<std::int64_t>(maximum[axis],values[axis]);}}
  std::int64_t shift[3]={0,0,0};
  for(unsigned axis=0;axis<3U;++axis){
   if(maximum[axis]-minimum[axis]>255)return false;
   if(minimum[axis]<0)shift[axis]=minimum[axis];
   else if(maximum[axis]>255)shift[axis]=maximum[axis]-255;
  }
  const std::int64_t offsets[3]={static_cast<std::int64_t>(probeFrame.xOffset)+shift[0]*4,
                                  static_cast<std::int64_t>(probeFrame.yOffset)+shift[2]*4,
                                  static_cast<std::int64_t>(probeFrame.zOffset)+shift[1]*4};
  for(const auto offset:offsets)if(offset<INT16_MIN||offset>INT16_MAX)return false;
  for(auto&v:probeFrame.vertices){v.x=static_cast<std::int32_t>(static_cast<std::int64_t>(v.x)-shift[0]);v.y=static_cast<std::int32_t>(static_cast<std::int64_t>(v.y)-shift[1]);v.z=static_cast<std::int32_t>(static_cast<std::int64_t>(v.z)-shift[2]);}
  if(shift[0]||shift[1]||shift[2]){
   if(!probe.setAnimationFrameHeaderValue(ci,ei,fi,0U,static_cast<std::int32_t>(offsets[0]))||
      !probe.setAnimationFrameHeaderValue(ci,ei,fi,1U,static_cast<std::int32_t>(offsets[1]))||
      !probe.setAnimationFrameHeaderValue(ci,ei,fi,2U,static_cast<std::int32_t>(offsets[2])))return false;
  }
  const auto desired=probeFrame.vertices;
  for(std::size_t vi=0;vi<probeFrame.specialVertexCount;++vi){
   const auto v=desired[vi];
   if(!probe.encodeAnimationFrameVertex(ci,ei,fi,vi,0U,v.x)||
      !probe.encodeAnimationFrameVertex(ci,ei,fi,vi,1U,v.y)||
      !probe.encodeAnimationFrameVertex(ci,ei,fi,vi,2U,v.z))return false;
  }
  probeFrame.vertices=desired;
  const auto modelIndex=probeFrame.specialVertexCount;
  if(modelIndex>=probeFrame.vertices.size())return false;
  if(!probe.encodeAnimationFrameVertex(ci,ei,fi,modelIndex,0U,probeFrame.vertices[modelIndex].x))return true;
  for(std::size_t vi=0;vi<desired.size();++vi){
   const auto&actual=probeFrame.vertices[vi];const auto&expected=desired[vi];
   if(actual.x!=expected.x||actual.y!=expected.y||actual.z!=expected.z)return true;
  }
  return false;
 };
 const std::size_t originalChunkCount=candidate.chunks_.size();
 for(std::size_t ci=0;ci<originalChunkCount;++ci){
  const auto&chunk=candidate.chunks_[ci];
  if(chunk.type!=0U||!chunk.entryTableValid)continue;
  for(std::size_t ei=0;ei<chunk.entries.size();++ei){
   const auto&entry=chunk.entries[ei];
   if(!entry.valid||entry.type!=1U||!entry.animation.decoded)continue;
   for(std::size_t fi=0;fi<entry.animation.frames.size();++fi){
    const auto&frame=entry.animation.frames[fi];
    if(!frame.verticesEdited||frame.linkedModelEid==0U||!requiresModelExpansion(ci,ei,fi))continue;
    bool compressedModel=false;
    for(const auto&modelChunk:candidate.chunks_)if(modelChunk.type==0U&&modelChunk.entryTableValid)
     for(const auto&modelEntry:modelChunk.entries)
      if(modelEntry.valid&&modelEntry.type==2U&&modelEntry.eid==frame.linkedModelEid&&modelEntry.model.decoded&&modelEntry.model.compressed){compressedModel=true;break;}
    if(!compressedModel)continue;
    const bool alreadyListed=std::any_of(cloneTargets.begin(),cloneTargets.end(),[&](const ModelCloneTarget&target){return target.chunkIndex==ci&&target.entryIndex==ei&&target.modelEid==frame.linkedModelEid;});
    if(!alreadyListed)cloneTargets.push_back({ci,ei,frame.linkedModelEid});
   }
  }
 }
 auto write32=[](std::vector<std::uint8_t>&bytes,std::size_t offset,std::uint32_t value){
  for(unsigned byte=0;byte<4U;++byte)bytes[offset+byte]=static_cast<std::uint8_t>(value>>(byte*8U));
 };
 for(const auto&target:cloneTargets){
  NsfEntryLocation modelLocation{};bool modelFound=false;
  for(std::size_t ci=0;ci<candidate.chunks_.size()&&!modelFound;++ci){
   const auto&chunk=candidate.chunks_[ci];if(chunk.type!=0U||!chunk.entryTableValid)continue;
   for(std::size_t ei=0;ei<chunk.entries.size();++ei){
    const auto&entry=chunk.entries[ei];
    if(entry.valid&&entry.type==2U&&entry.eid==target.modelEid&&entry.model.decoded&&entry.model.compressed){modelLocation={ci,ei};modelFound=true;break;}
   }
  }
  if(!modelFound){e="Could not locate the compressed Model entry needed to isolate this animation edit.";return false;}
  const auto&sourceChunk=candidate.chunks_[modelLocation.chunkIndex];
  const auto&sourceEntry=sourceChunk.entries[modelLocation.entryIndex];
  if(sourceEntry.offset>sourceChunk.bytes.size()||sourceEntry.size>sourceChunk.bytes.size()-sourceEntry.offset||sourceEntry.size>NsfChunkSize-24U){
   e="The compressed Model entry cannot be safely cloned into a fresh NSF chunk.";return false;
  }
  std::uint32_t cloneEid=0x02000001U;
  for(;;){
   bool used=false;
   for(const auto&chunk:candidate.chunks_)for(const auto&entry:chunk.entries)if(entry.valid&&entry.eid==cloneEid){used=true;break;}
   if(!used)break;
   if(cloneEid>=0x7ffffffdU){e="No unused valid EID is available for an isolated animation Model.";return false;}
   cloneEid+=2U;
  }
  std::vector<std::uint8_t>cloneBytes(NsfChunkSize,0U);
  cloneBytes[0]=0x34U;cloneBytes[1]=0x12U;
  write32(cloneBytes,8U,1U);write32(cloneBytes,16U,24U);
  write32(cloneBytes,20U,static_cast<std::uint32_t>(24U+sourceEntry.size));
  std::copy_n(sourceChunk.bytes.begin()+static_cast<std::ptrdiff_t>(sourceEntry.offset),sourceEntry.size,cloneBytes.begin()+24);
  write32(cloneBytes,28U,cloneEid);
  std::size_t parseOffset=0;NsfChunk clonedChunk;std::string parseError;
  if(!candidate.decodeChunk(cloneBytes,parseOffset,clonedChunk,parseError)||parseOffset!=NsfChunkSize||
     !clonedChunk.entryTableValid||clonedChunk.entries.size()!=1U||!clonedChunk.entries[0].valid||clonedChunk.entries[0].type!=2U){
   e="Could not parse the isolated Model clone: "+parseError;return false;
  }
  NsfModel clonedModel;
  if(!parseModel(clonedChunk.bytes,clonedChunk.entries[0],clonedModel)||!clonedModel.decoded||!clonedModel.compressed){
   e="Could not decode the isolated compressed Model clone: "+clonedModel.error;return false;
  }
  clonedChunk.entries[0].model=std::move(clonedModel);
  candidate.chunks_.push_back(std::move(clonedChunk));
  auto&animation=candidate.chunks_[target.chunkIndex].entries[target.entryIndex].animation;
  for(std::size_t fi=0;fi<animation.frames.size();++fi){
   if(animation.frames[fi].linkedModelEid!=target.modelEid)continue;
   if(!candidate.setAnimationFrameHeaderValue(target.chunkIndex,target.entryIndex,fi,4U,static_cast<std::int32_t>(cloneEid))){
    e="Could not redirect an edited Animation frame to its isolated Model clone.";return false;
   }
   animation.frames[fi].linkedModelEid=cloneEid;
   animation.frames[fi].modelResolved=true;
  }
 }
 for(std::size_t ci=0;ci<candidate.chunks_.size();++ci){
  auto&chunk=candidate.chunks_[ci];
  for(std::size_t ei=0;ei<chunk.entries.size();++ei){
   auto&entry=chunk.entries[ei];
   if(!entry.valid||entry.type!=1U)continue;
   for(std::size_t fi=0;fi<entry.animation.frames.size();++fi){
    auto&frame=entry.animation.frames[fi];
    if(!frame.verticesEdited)continue;
    if(!frame.verticesDecoded||frame.vertices.size()!=frame.vertexCount){e="Animation frame "+std::to_string(fi)+" has an incomplete editable pose.";return false;}
    // Crash 2 frame coordinates are bytes, but frame offsets can rebase a
    // whole pose without changing its world-space placement. Do this only at
    // save time; never clamp the working pose during editing.
    if(!entry.animation.isCrash3&&!frame.vertices.empty()){
     std::int64_t minimum[3]={INT64_MAX,INT64_MAX,INT64_MAX},maximum[3]={INT64_MIN,INT64_MIN,INT64_MIN};
     for(const auto&v:frame.vertices){const std::int32_t values[3]={v.x,v.y,v.z};for(unsigned axis=0;axis<3U;++axis){minimum[axis]=std::min<std::int64_t>(minimum[axis],values[axis]);maximum[axis]=std::max<std::int64_t>(maximum[axis],values[axis]);}}
     std::int64_t shift[3]={0,0,0};bool requiresRebase=false;
     for(unsigned axis=0;axis<3U;++axis){
      if(maximum[axis]-minimum[axis]>255){e="Animation frame "+std::to_string(fi)+" spans more than 255 local units on an axis. The pose remains editable, but the original Crash 2 frame cannot represent that geometry.";return false;}
      if(minimum[axis]<0){shift[axis]=minimum[axis];requiresRebase=true;}
      else if(maximum[axis]>255){shift[axis]=maximum[axis]-255;requiresRebase=true;}
     }
     if(requiresRebase){
      const std::int64_t offsets[3]={
       static_cast<std::int64_t>(frame.xOffset)+shift[0]*4,
       static_cast<std::int64_t>(frame.yOffset)+shift[2]*4,
       static_cast<std::int64_t>(frame.zOffset)+shift[1]*4
      };
      for(const auto offset:offsets)if(offset<INT16_MIN||offset>INT16_MAX){e="Animation frame "+std::to_string(fi)+" needs an offset outside the signed 16-bit NSF range to preserve its edited pose. The pose remains editable.";return false;}
      for(auto&v:frame.vertices){v.x=static_cast<std::int32_t>(static_cast<std::int64_t>(v.x)-shift[0]);v.y=static_cast<std::int32_t>(static_cast<std::int64_t>(v.y)-shift[1]);v.z=static_cast<std::int32_t>(static_cast<std::int64_t>(v.z)-shift[2]);}
      if(!candidate.setAnimationFrameHeaderValue(ci,ei,fi,0U,static_cast<std::int32_t>(offsets[0]))||
         !candidate.setAnimationFrameHeaderValue(ci,ei,fi,1U,static_cast<std::int32_t>(offsets[1]))||
         !candidate.setAnimationFrameHeaderValue(ci,ei,fi,2U,static_cast<std::int32_t>(offsets[2]))){
       e="Could not rebase animation frame "+std::to_string(fi)+" into the Crash 2 byte-coordinate space. The pose remains editable.";return false;
      }
     }
    }
    const NsfModel*model=nullptr;
    for(const auto&modelChunk:candidate.chunks_)if(modelChunk.type==0&&modelChunk.entryTableValid)
     for(const auto&modelEntry:modelChunk.entries)
      if(modelEntry.valid&&modelEntry.type==2U&&modelEntry.eid==frame.linkedModelEid&&modelEntry.model.decoded){model=&modelEntry.model;break;}
    if(!entry.animation.isCrash3&&frame.linkedModelEid!=0U&&!model){e="Animation frame "+std::to_string(fi)+" references a model that is not available for Crash 2 save validation. The editable pose is preserved.";return false;}
    if(model&&model->compressed){
     // Complex pose slots correspond directly to ModelPosition records.
     // Special vertices are stored independently in frame.specialVertices.
     const auto desiredVertices=frame.vertices;
     if(model->positions.size()+frame.specialVertexCount!=frame.vertices.size()){
      e="Animation frame "+std::to_string(fi)+" has a compressed-position count plus special-vertex count that does not match its frame vertices. The editable pose is preserved.";
      return false;
     }
     if(!model->positions.empty()){
      if(!candidate.encodeAnimationFrameVertex(ci,ei,fi,0U,0U,frame.vertices[0].x)){
       if(!candidate.expandCompressedModelPositions(frame.linkedModelEid,ci,ei,e))return false;
       continue;
      }
      for(std::size_t vi=0;vi<model->positions.size();++vi){
       const auto&actual=frame.vertices[vi];const auto&desired=desiredVertices[vi];
       if(actual.x!=desired.x||actual.y!=desired.y||actual.z!=desired.z){
        e="Compressed animation frame "+std::to_string(fi)+" did not reproduce edited ModelPosition "+std::to_string(vi)+" exactly. The editable pose is preserved.";
        return false;
       }
      }
     }
    }else{
     // The uncompressed encoder decodes the frame after each component write.
     // Keep the complete desired pose separate so that decoding an early
     // vertex cannot erase edits to vertices that have not been written yet.
     const auto desiredVertices=frame.vertices;
     for(std::size_t vi=0;vi<desiredVertices.size();++vi)for(std::size_t axis=0;axis<3U;++axis){
      const NsfAnimationVertex v=desiredVertices[vi];const std::int32_t value=axis==0U?v.x:axis==1U?v.y:v.z;
      if(!candidate.encodeAnimationFrameVertex(ci,ei,fi,vi,axis,value)){
       e="Animation frame "+std::to_string(fi)+" vertex "+std::to_string(vi)+" has coordinates that cannot be represented by the current NSF frame encoding. The editable pose is preserved; no source data was changed.";
       return false;
      }
      frame.vertices=desiredVertices;
     }
     if(!decodeAnimationVertices(frame,model,entry.animation.isCrash3)){
      e="Could not verify the saved coordinates for animation frame "+std::to_string(fi)+": "+frame.vertexError;
      return false;
     }
     for(std::size_t vi=0;vi<desiredVertices.size();++vi){
      const auto&actual=frame.vertices[vi];const auto&desired=desiredVertices[vi];
      if(actual.x!=desired.x||actual.y!=desired.y||actual.z!=desired.z){
       e="Saved coordinates for animation frame "+std::to_string(fi)+" vertex "+std::to_string(vi)+" did not reproduce the editable pose exactly.";
       return false;
      }
     }
    }
    frame.verticesEdited=false;
   }
  }
 }
 chunks_=std::move(candidate.chunks_);
 e.clear();return true;
}
bool NsfArchive::saveAnimationEditOverlay(std::vector<std::uint8_t>&out,std::string&e)const{
 out.clear();
 auto put32=[&](std::uint32_t v){for(unsigned i=0;i<4U;++i)out.push_back(static_cast<std::uint8_t>(v>>(i*8U)));};
 std::uint32_t count=0;
 for(const auto&chunk:chunks_)for(const auto&entry:chunk.entries)for(const auto&frame:entry.animation.frames)if(frame.verticesEdited)++count;
 put32(0x45534f50U);put32(1U);put32(count);
 for(std::size_t ci=0;ci<chunks_.size();++ci)for(std::size_t ei=0;ei<chunks_[ci].entries.size();++ei){
  const auto&entry=chunks_[ci].entries[ei];
  for(std::size_t fi=0;fi<entry.animation.frames.size();++fi){
   const auto&frame=entry.animation.frames[fi];if(!frame.verticesEdited)continue;
   if(ci>UINT32_MAX||ei>UINT32_MAX||fi>UINT32_MAX||frame.vertices.size()>UINT32_MAX){e="Editable animation pose overlay exceeds its supported index range.";out.clear();return false;}
   put32(static_cast<std::uint32_t>(ci));put32(static_cast<std::uint32_t>(ei));put32(static_cast<std::uint32_t>(fi));put32(static_cast<std::uint32_t>(frame.vertices.size()));
   for(const auto&v:frame.vertices){put32(static_cast<std::uint32_t>(v.x));put32(static_cast<std::uint32_t>(v.y));put32(static_cast<std::uint32_t>(v.z));}
  }
 }
 e.clear();return true;
}
bool NsfArchive::loadAnimationEditOverlay(const std::vector<std::uint8_t>&bytes,std::string&e){
 auto get32=[&](std::size_t p){return std::uint32_t(bytes[p])|(std::uint32_t(bytes[p+1U])<<8U)|(std::uint32_t(bytes[p+2U])<<16U)|(std::uint32_t(bytes[p+3U])<<24U);};
 if(bytes.size()<12U||get32(0)!=0x45534f50U||get32(4)!=1U){e="Editable animation pose overlay has an invalid header.";return false;}
 const std::uint32_t count=get32(8);if(static_cast<std::size_t>(count)>(bytes.size()-12U)/16U){e="Editable animation pose overlay declares more frames than its data can contain.";return false;}
 std::size_t p=12U;
 NsfArchive candidate=*this;
 for(auto&chunk:candidate.chunks_)for(auto&entry:chunk.entries)for(auto&frame:entry.animation.frames)frame.verticesEdited=false;
 for(std::uint32_t n=0;n<count;++n){
  if(p>bytes.size()||bytes.size()-p<16U){e="Editable animation pose overlay is truncated.";return false;}
  const std::size_t ci=get32(p),ei=get32(p+4U),fi=get32(p+8U),vc=get32(p+12U);p+=16U;
  if(vc>16384U||p>bytes.size()||static_cast<std::size_t>(vc)>(bytes.size()-p)/12U){e="Editable animation pose overlay contains an invalid vertex count.";return false;}
  std::vector<std::size_t>indices;std::vector<NsfAnimationVertex>vertices;indices.reserve(vc);vertices.reserve(vc);
  for(std::size_t vi=0;vi<vc;++vi){indices.push_back(vi);vertices.push_back({static_cast<std::int32_t>(get32(p)),static_cast<std::int32_t>(get32(p+4U)),static_cast<std::int32_t>(get32(p+8U))});p+=12U;}
  if(!candidate.setAnimationFrameVertices(ci,ei,fi,indices,vertices)){e="Editable animation pose overlay does not match the loaded NSF frame structure.";return false;}
 }
 if(p!=bytes.size()){e="Editable animation pose overlay contains trailing data.";return false;}
 chunks_=std::move(candidate.chunks_);e.clear();return true;
}
static bool replaceAnimationFramePayloads(NsfChunk&,NsfEntry&,const std::vector<std::vector<std::uint8_t>>&);

bool NsfArchive::expandCompressedModelPositions(std::uint32_t modelEid,std::size_t preferredChunk,std::size_t preferredEntry,std::string&e){
 NsfEntry*modelEntry=nullptr;NsfChunk*modelChunk=nullptr;
 for(auto&chunk:chunks_)if(chunk.type==0&&chunk.entryTableValid)for(auto&entry:chunk.entries)
  if(entry.valid&&entry.type==2U&&entry.eid==modelEid&&entry.model.decoded){modelEntry=&entry;modelChunk=&chunk;break;}
 if(!modelEntry||!modelChunk||!modelEntry->model.compressed||modelEntry->items.size()!=6U){e="The linked model has no expandable compressed-position table.";return false;}
 auto&model=modelEntry->model;
 if(model.positions.empty()){e="The linked model has no compressed positions to expand.";return false;}
 const auto&positionItem=modelEntry->items[5];
 if(positionItem.size!=model.positions.size()*4U||!has(modelChunk->bytes,positionItem.offset,positionItem.size)){e="The linked model's compressed-position table is malformed.";return false;}
 // Width 7 is the format's full signed-byte delta width. It also resets the
 // corresponding accumulator, so every byte-coordinate delta is representable.
 for(std::size_t i=0;i<model.positions.size();++i){
  auto&position=model.positions[i];position.xBits=7U;position.zBits=7U;position.yBits=7U;
  position.raw=(position.raw&~0x1ffU)|0x1ffU;
  const std::size_t offset=positionItem.offset+i*4U;
  for(unsigned byte=0;byte<4U;++byte)modelChunk->bytes[offset+byte]=static_cast<std::uint8_t>(position.raw>>(byte*8U));
 }
 // The model position table is shared by every animation that references it,
 // so all of those entries must be rebuilt. Process the animation whose edited
 // pose triggered this expansion first: if the chunk cannot hold its enlarged
 // payload, the relocation prompt must move the animation the user is editing,
 // not an unrelated earlier entry that happens to share the model.
 std::vector<NsfEntryLocation> animationOrder;
 for(std::size_t ci=0;ci<chunks_.size();++ci){
  const auto&sourceChunk=chunks_[ci];
  if(sourceChunk.type!=0||!sourceChunk.entryTableValid)continue;
  for(std::size_t ei=0;ei<sourceChunk.entries.size();++ei){
   const auto&sourceEntry=sourceChunk.entries[ei];
   if(sourceEntry.valid&&sourceEntry.type==1U)animationOrder.push_back({ci,ei});
  }
 }
 std::stable_sort(animationOrder.begin(),animationOrder.end(),[&](const NsfEntryLocation&a,const NsfEntryLocation&b){
  const bool aPreferred=a.chunkIndex==preferredChunk&&a.entryIndex==preferredEntry;
  const bool bPreferred=b.chunkIndex==preferredChunk&&b.entryIndex==preferredEntry;
  return aPreferred&&!bPreferred;
 });
 for(const auto&location:animationOrder){
  auto&chunk=chunks_[location.chunkIndex];
  auto&entry=chunk.entries[location.entryIndex];
   if(!entry.animation.decoded){e="Cannot expand a shared model position table while an animation entry is undecoded. The editable pose is preserved.";return false;}
   if(entry.animation.frames.empty())continue;
   std::vector<std::vector<std::uint8_t>>payloads;payloads.reserve(entry.animation.frames.size());
   std::vector<bool>affected(entry.animation.frames.size(),false);
   bool entryAffected=false;
   for(std::size_t fi=0;fi<entry.animation.frames.size();++fi){
    auto&frame=entry.animation.frames[fi];
    if(frame.linkedModelEid!=modelEid){
     if(!has(chunk.bytes,frame.rawOffset,frame.rawSize)){e="An animation frame payload is outside its NSF chunk.";return false;}
     payloads.emplace_back(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(frame.rawOffset),chunk.bytes.begin()+static_cast<std::ptrdiff_t>(frame.rawOffset+frame.rawSize));
     continue;
    }
    entryAffected=true;affected[fi]=true;
    if(entry.animation.isCrash3){e="Compressed model expansion is only enabled for the active Crash 2 animation format.";return false;}
    if(!frame.verticesDecoded||frame.vertices.size()!=frame.vertexCount||frame.vertexCount!=model.vertexCount||
       model.positions.size()+frame.specialVertexCount!=frame.vertexCount||model.specialVertexCount!=frame.specialVertexCount){
     e="An animation frame linked to this model has an unsupported special-vertex or compressed-position layout.";return false;
    }
    const std::size_t fakeHeader=24U+static_cast<std::size_t>(frame.collisionCount)*0x28U;
    if(frame.rawSize<fakeHeader||!has(chunk.bytes,frame.rawOffset,frame.rawSize)||frame.headerSize!=fakeHeader+frame.specialVertexCount*3U){
     e="An animation frame linked to this model has an invalid header boundary.";return false;
    }
    std::int64_t minimum[3]={INT64_MAX,INT64_MAX,INT64_MAX},maximum[3]={INT64_MIN,INT64_MIN,INT64_MIN};
    for(const auto&v:frame.vertices){const std::int32_t values[3]={v.x,v.y,v.z};for(unsigned axis=0;axis<3U;++axis){minimum[axis]=std::min<std::int64_t>(minimum[axis],values[axis]);maximum[axis]=std::max<std::int64_t>(maximum[axis],values[axis]);}}
    std::int64_t shift[3]={0,0,0};bool needsRebase=false;
    for(unsigned axis=0;axis<3U;++axis){
     if(maximum[axis]-minimum[axis]>255){e="An animation frame linked to this model spans more than 255 local units on an axis; its pose remains editable but cannot be stored in the Crash 2 byte-coordinate format.";return false;}
     if(minimum[axis]<0){shift[axis]=minimum[axis];needsRebase=true;}
     else if(maximum[axis]>255){shift[axis]=maximum[axis]-255;needsRebase=true;}
    }
    if(needsRebase){
     const std::int64_t offsets[3]={
      static_cast<std::int64_t>(frame.xOffset)+shift[0]*4,
      static_cast<std::int64_t>(frame.yOffset)+shift[2]*4,
      static_cast<std::int64_t>(frame.zOffset)+shift[1]*4
     };
     for(const auto offset:offsets)if(offset<INT16_MIN||offset>INT16_MAX){e="An animation frame linked to this model needs an offset outside the signed 16-bit NSF range; its pose remains editable.";return false;}
     for(auto&v:frame.vertices){v.x=static_cast<std::int32_t>(static_cast<std::int64_t>(v.x)-shift[0]);v.y=static_cast<std::int32_t>(static_cast<std::int64_t>(v.y)-shift[1]);v.z=static_cast<std::int32_t>(static_cast<std::int64_t>(v.z)-shift[2]);}
     const std::size_t chunkIndex=static_cast<std::size_t>(&chunk-chunks_.data());
     const std::size_t entryIndex=static_cast<std::size_t>(&entry-chunk.entries.data());
     if(!setAnimationFrameHeaderValue(chunkIndex,entryIndex,fi,0U,static_cast<std::int32_t>(offsets[0]))||
        !setAnimationFrameHeaderValue(chunkIndex,entryIndex,fi,1U,static_cast<std::int32_t>(offsets[1]))||
        !setAnimationFrameHeaderValue(chunkIndex,entryIndex,fi,2U,static_cast<std::int32_t>(offsets[2]))){
      e="Could not rebase an animation frame linked to the expanded model.";return false;
     }
    }
    const std::size_t start=frame.specialVertexCount*24U;
    const std::size_t requiredBits=start+model.positions.size()*24U;
    const std::size_t wordCount=(requiredBits+31U)/32U;
    std::vector<std::uint8_t>payload(fakeHeader+wordCount*4U,0U);
    std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(frame.rawOffset),fakeHeader,payload.begin());
    for(std::size_t vi=0;vi<frame.specialVertexCount;++vi){
     const auto&v=frame.vertices[vi];const std::size_t offset=fakeHeader+vi*3U;
     payload[offset]=static_cast<std::uint8_t>(v.x);payload[offset+1U]=static_cast<std::uint8_t>(v.y);payload[offset+2U]=static_cast<std::uint8_t>(v.z);
    }
    std::vector<std::uint8_t>bits(wordCount*32U,0U);
    for(std::size_t wi=0;wi<wordCount;++wi){
     const std::uint32_t word=u32(payload,fakeHeader+wi*4U);
     for(unsigned bit=0;bit<32U;++bit)bits[wi*32U+bit]=static_cast<std::uint8_t>((word>>(31U-bit))&1U);
    }
      std::size_t bit=start;std::int32_t xAcc=0,yAcc=0,zAcc=0;
    auto encodeSigned=[&](std::int32_t delta){
     bits[bit++]=delta<0?1U:0U;const std::uint32_t encoded=static_cast<std::uint32_t>(delta<0?delta+128:delta);
     for(unsigned b=0;b<7U;++b)bits[bit++]=static_cast<std::uint8_t>((encoded>>(6U-b))&1U);
    };
    auto deltaFor=[](std::int32_t target,std::int32_t base){std::int32_t delta=(target-base)%256;if(delta<0)delta+=256;return delta>127?delta-256:delta;};
    for(std::size_t pi=0;pi<model.positions.size();++pi){
     const auto&position=model.positions[pi];if(position.xBits==7U)xAcc=0;if(position.yBits==7U)yAcc=0;if(position.zBits==7U)zAcc=0;
     // Compressed Model vertices follow the special-vertex prefix in the editor's unified pose.
     const std::size_t frameVertexIndex=pi+frame.specialVertexCount;
     if(frameVertexIndex>=frame.vertices.size()){e="Compressed model position index exceeds the linked animation frame.";return false;}
     const auto&target=frame.vertices[frameVertexIndex];
     const std::int32_t baseX=(xAcc+(static_cast<std::int32_t>(position.x)<<1U))&0xff;
     const std::int32_t baseZ=(zAcc+position.z)&0xff;
     const std::int32_t baseY=(yAcc+position.y)&0xff;
     encodeSigned(deltaFor(target.x,baseX));encodeSigned(deltaFor(target.z,baseZ));encodeSigned(deltaFor(target.y,baseY));
     xAcc=target.x;yAcc=target.y;zAcc=target.z;
    }
    for(std::size_t wi=0;wi<wordCount;++wi){
     std::uint32_t word=0;
     for(unsigned bitIndex=0;bitIndex<32U;++bitIndex)word=(word<<1U)|bits[wi*32U+bitIndex];
     for(unsigned byte=0;byte<4U;++byte)payload[fakeHeader+wi*4U+byte]=static_cast<std::uint8_t>(word>>(byte*8U));
    }
    // Special XYZ bytes alias the temporal bitstream. The raw prefix seeds
    // its unused prefix bits above; after encoding, serialized temporal words
    // are authoritative. Restoring raw XYZ here can overwrite compressed
    // deltas that share a physical byte with the prefix.
    payloads.push_back(std::move(payload));
   }
   if(!entryAffected)continue;
   if(!replaceAnimationFramePayloads(chunk,entry,payloads)){const std::size_t failedChunk=static_cast<std::size_t>(&chunk-chunks_.data());const std::size_t failedEntry=static_cast<std::size_t>(&entry-chunk.entries.data());e="The expanded animation frame payloads do not fit safely in their NSF chunk. The editable pose is preserved. [chunk="+std::to_string(failedChunk)+",entry="+std::to_string(failedEntry)+"]";return false;}
   for(std::size_t fi=0;fi<entry.animation.frames.size();++fi)if(affected[fi]){
    auto&previous=entry.animation.frames[fi];const auto desired=previous.vertices;
    NsfAnimationFrame decoded;
    if(!parseAnimationFrame(chunk.bytes,entry.items[fi],false,decoded)){e="Could not parse a rebuilt compressed animation frame: "+decoded.error;return false;}
    decoded.linkedModelEid=modelEid;decoded.modelResolved=true;
    if(!decodeAnimationVertices(decoded,&model,false)){e="Could not decode a rebuilt compressed animation frame: "+decoded.vertexError;return false;}
    if(decoded.vertices.size()!=desired.size()){e="Rebuilt compressed animation frame changed its vertex count.";return false;}
    for(std::size_t vi=0;vi<desired.size();++vi)if(decoded.vertices[vi].x!=desired[vi].x||decoded.vertices[vi].y!=desired[vi].y||decoded.vertices[vi].z!=desired[vi].z){
     e="Rebuilt compressed animation frame did not reproduce its editable pose exactly.";return false;
    }
    decoded.verticesEdited=false;previous=std::move(decoded);
   }
  }
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

static bool rebuildAnimationFrameItems(NsfChunk&chunk,NsfEntry&entry,std::size_t frameIndex,bool insert,std::size_t insertAt){
 auto&animation=entry.animation;
 const std::size_t oldCount=entry.items.size();
 if(!entry.valid||entry.type!=1U||!animation.decoded||oldCount==0U||oldCount!=animation.frames.size())return false;
 if(insert){
  if(frameIndex>=oldCount||insertAt>oldCount)return false;
 }else if(oldCount<=1U||frameIndex>=oldCount)return false;
 const std::uint32_t entryCount=u32(chunk.bytes,8U);
 if(entryCount!=chunk.entries.size()||entryCount==0U)return false;
 const std::size_t entryIndex=static_cast<std::size_t>(&entry-chunk.entries.data());
 if(entryIndex>=chunk.entries.size())return false;
 const std::size_t oldEntryEnd=entry.offset+entry.size;
 const std::size_t offsetTableEnd=16U+(static_cast<std::size_t>(entryCount)+1U)*4U;
 if(entry.offset<offsetTableEnd||oldEntryEnd>NsfChunkSize)return false;
 const std::size_t sentinelOffset=16U+static_cast<std::size_t>(entryCount)*4U;
 const std::size_t usedEnd=u32(chunk.bytes,sentinelOffset);
 if(usedEnd<oldEntryEnd||usedEnd>NsfChunkSize)return false;
 const std::size_t oldHeaderEnd=16U+(oldCount+1U)*4U;
 if(entry.size<oldHeaderEnd)return false;
 const std::size_t firstItemOffset=entry.items.front().offset-entry.offset;
 if(firstItemOffset<oldHeaderEnd||firstItemOffset>entry.size)return false;
 const std::size_t prefixPadding=firstItemOffset-oldHeaderEnd;
 std::vector<std::vector<std::uint8_t>>payloads;
 payloads.reserve(insert?oldCount+1U:oldCount-1U);
 for(std::size_t i=0;i<oldCount;++i){
  const auto&item=entry.items[i];
  if(item.offset<entry.offset||item.offset+item.size>oldEntryEnd||!has(chunk.bytes,item.offset,item.size))return false;
  payloads.emplace_back(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(item.offset),chunk.bytes.begin()+static_cast<std::ptrdiff_t>(item.offset+item.size));
 }
 std::vector<NsfAnimationFrame>newFrames=animation.frames;
 if(insert){
  auto duplicatePayload=payloads[frameIndex];
  auto duplicateFrame=newFrames[frameIndex];
  payloads.insert(payloads.begin()+static_cast<std::ptrdiff_t>(insertAt),std::move(duplicatePayload));
  newFrames.insert(newFrames.begin()+static_cast<std::ptrdiff_t>(insertAt),std::move(duplicateFrame));
 }else{
  payloads.erase(payloads.begin()+static_cast<std::ptrdiff_t>(frameIndex));
  newFrames.erase(newFrames.begin()+static_cast<std::ptrdiff_t>(frameIndex));
 }
 const std::size_t newCount=payloads.size();
 if(newCount==0U||newCount>std::numeric_limits<std::uint32_t>::max())return false;
 const std::size_t newHeaderEnd=16U+(newCount+1U)*4U;
 const std::size_t newFirstItemOffset=newHeaderEnd+prefixPadding;
 std::size_t newEntrySize=newFirstItemOffset;
 for(const auto&payload:payloads){
  if(payload.size()>NsfChunkSize-newEntrySize)return false;
  newEntrySize+=payload.size();
 }
 const std::size_t oldPayloadEnd=entry.items.back().offset+entry.items.back().size;
 if(oldPayloadEnd>oldEntryEnd)return false;
 const std::size_t trailingPadding=oldEntryEnd-oldPayloadEnd;
 const std::size_t newPayloadEnd=newEntrySize;
 if(trailingPadding>NsfChunkSize-newEntrySize)return false;
 newEntrySize+=trailingPadding;
 if(entry.offset>NsfChunkSize||newEntrySize>NsfChunkSize-entry.offset)return false;
 const std::ptrdiff_t delta=static_cast<std::ptrdiff_t>(newEntrySize)-static_cast<std::ptrdiff_t>(entry.size);
 const std::ptrdiff_t newUsedEndSigned=static_cast<std::ptrdiff_t>(usedEnd)+delta;
 if(newUsedEndSigned<0||newUsedEndSigned>static_cast<std::ptrdiff_t>(NsfChunkSize))return false;
 std::vector<std::uint8_t>newEntry(newEntrySize,0U);
 std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset),16U,newEntry.begin());
 auto write32=[](std::vector<std::uint8_t>&bytes,std::size_t offset,std::uint32_t value){
  for(unsigned i=0;i<4U;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8U));
 };
 write32(newEntry,12U,static_cast<std::uint32_t>(newCount));
 std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset+oldHeaderEnd),prefixPadding,newEntry.begin()+static_cast<std::ptrdiff_t>(newHeaderEnd));
 std::size_t cursor=newFirstItemOffset;
 for(std::size_t i=0;i<newCount;++i){
  write32(newEntry,16U+i*4U,static_cast<std::uint32_t>(cursor));
  if(!payloads[i].empty())std::copy(payloads[i].begin(),payloads[i].end(),newEntry.begin()+static_cast<std::ptrdiff_t>(cursor));
  cursor+=payloads[i].size();
 }
 write32(newEntry,16U+newCount*4U,static_cast<std::uint32_t>(cursor));
 if(trailingPadding>0U)
  std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(oldPayloadEnd),trailingPadding,newEntry.begin()+static_cast<std::ptrdiff_t>(newPayloadEnd));
 const std::size_t suffixSize=usedEnd-oldEntryEnd;
 const std::size_t newEntryEnd=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(oldEntryEnd)+delta);
 if(suffixSize>0U)std::memmove(chunk.bytes.data()+static_cast<std::ptrdiff_t>(newEntryEnd),chunk.bytes.data()+static_cast<std::ptrdiff_t>(oldEntryEnd),suffixSize);
 std::copy(newEntry.begin(),newEntry.end(),chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset));
 entry.size=newEntrySize;
 entry.itemCount=static_cast<std::uint32_t>(newCount);
 entry.items.clear();
 entry.items.reserve(newCount);
 for(std::size_t i=0;i<newCount;++i){
  const std::size_t start=u32(chunk.bytes,entry.offset+16U+i*4U);
  const std::size_t end=u32(chunk.bytes,entry.offset+16U+(i+1U)*4U);
  entry.items.push_back({entry.offset+start,end-start});
 }
 for(std::size_t i=entryIndex+1U;i<chunk.entries.size();++i){
  auto&following=chunk.entries[i];
  following.offset=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(following.offset)+delta);
  for(auto&item:following.items)item.offset=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(item.offset)+delta);
  if(following.type==1U&&following.animation.frames.size()==following.items.size()){
   for(std::size_t frame=0;frame<following.animation.frames.size();++frame){
    following.animation.frames[frame].rawOffset=following.items[frame].offset;
    following.animation.frames[frame].rawSize=following.items[frame].size;
   }
  }
 }
 for(std::size_t i=0;i<chunk.entries.size();++i)
  write32(chunk.bytes,16U+i*4U,static_cast<std::uint32_t>(chunk.entries[i].offset));
 write32(chunk.bytes,sentinelOffset,static_cast<std::uint32_t>(newUsedEndSigned));
 animation.frames=std::move(newFrames);
 for(std::size_t i=0;i<animation.frames.size();++i){
  animation.frames[i].rawOffset=entry.items[i].offset;
  animation.frames[i].rawSize=entry.items[i].size;
 }
 return true;
}

static bool replaceAnimationFramePayloads(NsfChunk&chunk,NsfEntry&entry,const std::vector<std::vector<std::uint8_t>>&payloads){
 const std::size_t oldCount=entry.items.size();
 if(!entry.valid||entry.type!=1U||!entry.animation.decoded||oldCount==0U||oldCount!=entry.animation.frames.size()||payloads.size()!=oldCount)return false;
 const std::uint32_t entryCount=u32(chunk.bytes,8U);
 if(entryCount!=chunk.entries.size()||entryCount==0U)return false;
 const std::size_t entryIndex=static_cast<std::size_t>(&entry-chunk.entries.data());
 if(entryIndex>=chunk.entries.size())return false;
 const std::size_t oldEntryEnd=entry.offset+entry.size;
 const std::size_t offsetTableEnd=16U+(static_cast<std::size_t>(entryCount)+1U)*4U;
 if(entry.offset<offsetTableEnd||oldEntryEnd>NsfChunkSize)return false;
 const std::size_t sentinelOffset=16U+static_cast<std::size_t>(entryCount)*4U;
 const std::size_t usedEnd=u32(chunk.bytes,sentinelOffset);
 if(usedEnd<oldEntryEnd||usedEnd>NsfChunkSize)return false;
 const std::size_t headerEnd=16U+(oldCount+1U)*4U;
 if(entry.size<headerEnd||entry.items.empty())return false;
 const std::size_t firstItemOffset=entry.items.front().offset-entry.offset;
 if(firstItemOffset<headerEnd||firstItemOffset>entry.size)return false;
 const std::size_t prefixPadding=firstItemOffset-headerEnd;
 const std::size_t oldPayloadEnd=entry.items.back().offset+entry.items.back().size;
 if(oldPayloadEnd>oldEntryEnd)return false;
 const std::size_t trailingPadding=oldEntryEnd-oldPayloadEnd;
 std::size_t newEntrySize=headerEnd+prefixPadding;
 for(const auto&payload:payloads){if(payload.size()>NsfChunkSize-newEntrySize)return false;newEntrySize+=payload.size();}
 if(trailingPadding>NsfChunkSize-newEntrySize)return false;
 const std::size_t newPayloadEnd=newEntrySize;newEntrySize+=trailingPadding;
 if(entry.offset>NsfChunkSize||newEntrySize>NsfChunkSize-entry.offset)return false;
 const std::ptrdiff_t delta=static_cast<std::ptrdiff_t>(newEntrySize)-static_cast<std::ptrdiff_t>(entry.size);
 const std::ptrdiff_t newUsedEndSigned=static_cast<std::ptrdiff_t>(usedEnd)+delta;
 if(newUsedEndSigned<0||newUsedEndSigned>static_cast<std::ptrdiff_t>(NsfChunkSize))return false;
 std::vector<std::uint8_t>newEntry(newEntrySize,0U);
 std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset),16U,newEntry.begin());
 auto write32=[](std::vector<std::uint8_t>&bytes,std::size_t offset,std::uint32_t value){for(unsigned i=0;i<4U;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8U));};
 write32(newEntry,12U,static_cast<std::uint32_t>(oldCount));
 std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset+headerEnd),prefixPadding,newEntry.begin()+static_cast<std::ptrdiff_t>(headerEnd));
 std::size_t cursor=headerEnd+prefixPadding;
 for(std::size_t i=0;i<oldCount;++i){
  write32(newEntry,16U+i*4U,static_cast<std::uint32_t>(cursor));
  if(!payloads[i].empty())std::copy(payloads[i].begin(),payloads[i].end(),newEntry.begin()+static_cast<std::ptrdiff_t>(cursor));
  cursor+=payloads[i].size();
 }
 write32(newEntry,16U+oldCount*4U,static_cast<std::uint32_t>(cursor));
 if(trailingPadding>0U)std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(oldPayloadEnd),trailingPadding,newEntry.begin()+static_cast<std::ptrdiff_t>(newPayloadEnd));
 const std::size_t suffixSize=usedEnd-oldEntryEnd;
 const std::size_t newEntryEnd=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(oldEntryEnd)+delta);
 if(suffixSize>0U)std::memmove(chunk.bytes.data()+static_cast<std::ptrdiff_t>(newEntryEnd),chunk.bytes.data()+static_cast<std::ptrdiff_t>(oldEntryEnd),suffixSize);
 std::copy(newEntry.begin(),newEntry.end(),chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset));
 entry.size=newEntrySize;entry.itemCount=static_cast<std::uint32_t>(oldCount);entry.items.clear();entry.items.reserve(oldCount);
 for(std::size_t i=0;i<oldCount;++i){const std::size_t start=u32(chunk.bytes,entry.offset+16U+i*4U),end=u32(chunk.bytes,entry.offset+16U+(i+1U)*4U);entry.items.push_back({entry.offset+start,end-start});}
 for(std::size_t i=entryIndex+1U;i<chunk.entries.size();++i){
  auto&following=chunk.entries[i];following.offset=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(following.offset)+delta);
  for(auto&item:following.items)item.offset=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(item.offset)+delta);
  if(following.type==1U&&following.animation.frames.size()==following.items.size())for(std::size_t frame=0;frame<following.animation.frames.size();++frame){following.animation.frames[frame].rawOffset=following.items[frame].offset;following.animation.frames[frame].rawSize=following.items[frame].size;}
 }
 for(std::size_t i=0;i<chunk.entries.size();++i)write32(chunk.bytes,16U+i*4U,static_cast<std::uint32_t>(chunk.entries[i].offset));
 write32(chunk.bytes,sentinelOffset,static_cast<std::uint32_t>(newUsedEndSigned));
 for(std::size_t i=0;i<entry.animation.frames.size();++i){entry.animation.frames[i].rawOffset=entry.items[i].offset;entry.animation.frames[i].rawSize=entry.items[i].size;}
 return true;
}

bool NsfArchive::animationFrameEditNeedsNewChunk(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex)const{
 if(chunkIndex>=chunks_.size())return false;
 const auto&chunk=chunks_[chunkIndex];
 if(chunk.type!=0U||!chunk.entryTableValid||entryIndex>=chunk.entries.size())return false;
 const auto&entry=chunk.entries[entryIndex];
 const auto&animation=entry.animation;
 const std::size_t oldCount=entry.items.size();
 if(!entry.valid||entry.type!=1U||!animation.decoded||oldCount==0U||oldCount!=animation.frames.size()||frameIndex>=oldCount)return false;
 const std::uint32_t entryCount=u32(chunk.bytes,8U);
 if(entryCount!=chunk.entries.size()||entryCount==0U)return false;
 const std::size_t oldEntryEnd=entry.offset+entry.size;
 const std::size_t tableEnd=16U+(static_cast<std::size_t>(entryCount)+1U)*4U;
 if(entry.offset<tableEnd||oldEntryEnd>NsfChunkSize)return false;
 const std::size_t usedEnd=u32(chunk.bytes,16U+static_cast<std::size_t>(entryCount)*4U);
 if(usedEnd<oldEntryEnd||usedEnd>NsfChunkSize)return false;
 const std::size_t oldHeaderEnd=16U+(oldCount+1U)*4U;
 if(entry.size<oldHeaderEnd||entry.items.empty())return false;
 const std::size_t firstItemOffset=entry.items.front().offset-entry.offset;
 if(firstItemOffset<oldHeaderEnd||firstItemOffset>entry.size)return false;
 const std::size_t prefixPadding=firstItemOffset-oldHeaderEnd;
 const std::size_t oldPayloadEnd=entry.items.back().offset+entry.items.back().size;
 if(oldPayloadEnd>oldEntryEnd)return false;
 const std::size_t trailingPadding=oldEntryEnd-oldPayloadEnd;
 std::size_t payloadBytes=0;
 for(const auto&item:entry.items){if(item.size>NsfChunkSize-payloadBytes)return false;payloadBytes+=item.size;}
 if(entry.items[frameIndex].size>NsfChunkSize-payloadBytes)return false;
 payloadBytes+=entry.items[frameIndex].size;
 const std::size_t newCount=oldCount+1U;
 if(newCount>std::numeric_limits<std::uint32_t>::max())return false;
 const std::size_t newHeaderEnd=16U+(newCount+1U)*4U;
 if(newHeaderEnd>NsfChunkSize||prefixPadding>NsfChunkSize-newHeaderEnd)return true;
 std::size_t newEntrySize=newHeaderEnd+prefixPadding;
 if(payloadBytes>NsfChunkSize-newEntrySize)return true;
 newEntrySize+=payloadBytes;
 if(trailingPadding>NsfChunkSize-newEntrySize)return true;
 newEntrySize+=trailingPadding;
 if(entry.offset>NsfChunkSize||newEntrySize>NsfChunkSize-entry.offset)return true;
 const std::ptrdiff_t delta=static_cast<std::ptrdiff_t>(newEntrySize)-static_cast<std::ptrdiff_t>(entry.size);
 const std::ptrdiff_t newUsedEnd=static_cast<std::ptrdiff_t>(usedEnd)+delta;
 return newUsedEnd<0||newUsedEnd>static_cast<std::ptrdiff_t>(NsfChunkSize);
}

bool NsfArchive::moveEntryToNewChunk(std::size_t chunkIndex,std::size_t entryIndex,std::size_t&newChunkIndex,std::string&error){
 auto fail=[&](const char*message){error=message;return false;};
 if(chunkIndex>=chunks_.size())return fail("The source chunk no longer exists.");
 const auto&source=chunks_[chunkIndex];
 if(source.type!=0U||!source.entryTableValid||entryIndex>=source.entries.size())return fail("The selected resource is not a valid entry in a normal NSF chunk.");
 const std::size_t count=source.entries.size();
 if(count==0U||u32(source.bytes,8U)!=count)return fail("The source chunk entry table is inconsistent.");
 const std::size_t tableEnd=20U+count*4U;
 const std::size_t usedEnd=u32(source.bytes,16U+count*4U);
 if(usedEnd>NsfChunkSize||usedEnd<tableEnd)return fail("The source chunk's used-data boundary is invalid.");
 const auto&selected=source.entries[entryIndex];
 if(!selected.valid||selected.offset<tableEnd||selected.offset>NsfChunkSize||selected.size>NsfChunkSize-selected.offset||
    selected.offset+selected.size>usedEnd)return fail("The selected entry has invalid bounds and cannot be relocated.");
 if(selected.size>NsfChunkSize-24U)return fail("This entry is too large to fit in a new 64 KiB chunk.");
 if(source.entries.front().offset<tableEnd||source.entries.front().offset>usedEnd)return fail("The source chunk's first-entry boundary is invalid.");
 const std::size_t prefixPadding=source.entries.front().offset-tableEnd;
 const std::size_t remainingCount=count-1U;
 const std::size_t newTableEnd=20U+remainingCount*4U;

 struct PendingAnimationPose { std::size_t chunkIndex; std::size_t entryIndex; std::size_t frameIndex; std::vector<NsfAnimationVertex> vertices; };
 std::vector<PendingAnimationPose> pendingPoses;
 for(std::size_t sourceChunkIndex=0;sourceChunkIndex<chunks_.size();++sourceChunkIndex){
  const auto&sourceChunk=chunks_[sourceChunkIndex];
  if(sourceChunk.type!=0U||!sourceChunk.entryTableValid)continue;
  for(std::size_t sourceEntryIndex=0;sourceEntryIndex<sourceChunk.entries.size();++sourceEntryIndex){
   const auto&sourceEntry=sourceChunk.entries[sourceEntryIndex];
   if(!sourceEntry.valid||sourceEntry.type!=1U)continue;
   for(std::size_t fi=0;fi<sourceEntry.animation.frames.size();++fi){
    const auto&frame=sourceEntry.animation.frames[fi];
    if(frame.verticesEdited)pendingPoses.push_back({sourceChunkIndex,sourceEntryIndex,fi,frame.vertices});
   }
  }
 }
 NsfArchive candidate=*this;
 std::size_t packedDestination=std::numeric_limits<std::size_t>::max();NsfChunk packedChunk;
 for(std::size_t di=candidate.chunks_.size();di>0;--di){
  const std::size_t index=di-1U;const auto&dest=candidate.chunks_[index];
  if(index==chunkIndex||!dest.conversionPackingChunk||dest.type!=0U||!dest.entryTableValid||dest.entries.empty())continue;
  const std::size_t count=dest.entries.size(),oldTableEnd=20U+count*4U,usedEnd=u32(dest.bytes,16U+count*4U);
  if(usedEnd>NsfChunkSize||usedEnd<oldTableEnd||dest.entries.front().offset<oldTableEnd||dest.entries.front().offset>usedEnd)continue;
  const std::size_t prefix=dest.entries.front().offset-oldTableEnd,newTableEnd=20U+(count+1U)*4U;
  std::size_t payload=0;bool valid=true;
  for(const auto&entry:dest.entries){if(!entry.valid||entry.offset>usedEnd||entry.size>usedEnd-entry.offset||entry.size>NsfChunkSize-payload){valid=false;break;}payload+=entry.size;}
  const std::size_t trailing=NsfChunkSize-usedEnd;if(!valid||newTableEnd>NsfChunkSize||prefix>NsfChunkSize-newTableEnd)continue;
  std::size_t required=newTableEnd+prefix;if(payload>NsfChunkSize-required)continue;required+=payload;
  if(selected.size>NsfChunkSize-required)continue;required+=selected.size;if(trailing>NsfChunkSize-required)continue;
  std::vector<std::uint8_t> bytes(NsfChunkSize,0U);std::copy_n(dest.bytes.begin(),16U,bytes.begin());
  if(prefix)std::copy_n(dest.bytes.begin()+static_cast<std::ptrdiff_t>(oldTableEnd),prefix,bytes.begin()+static_cast<std::ptrdiff_t>(newTableEnd));
  std::size_t cursor=newTableEnd+prefix;
  for(std::size_t ei=0;ei<count;++ei){const auto&entry=dest.entries[ei];std::copy_n(dest.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset),entry.size,bytes.begin()+static_cast<std::ptrdiff_t>(cursor));write32(bytes,16U+ei*4U,static_cast<std::uint32_t>(cursor));cursor+=entry.size;}
  if(selected.size)std::copy_n(source.bytes.begin()+static_cast<std::ptrdiff_t>(selected.offset),selected.size,bytes.begin()+static_cast<std::ptrdiff_t>(cursor));
  write32(bytes,16U+count*4U,static_cast<std::uint32_t>(cursor));cursor+=selected.size;
  write32(bytes,16U+(count+1U)*4U,static_cast<std::uint32_t>(cursor));write32(bytes,8U,static_cast<std::uint32_t>(count+1U));
  if(trailing)std::copy_n(dest.bytes.begin()+static_cast<std::ptrdiff_t>(usedEnd),trailing,bytes.begin()+static_cast<std::ptrdiff_t>(cursor));
  std::size_t po=0;NsfChunk parsed;std::string pe;if(!decodeChunk(bytes,po,parsed,pe)||po!=NsfChunkSize||!parsed.entryTableValid||parsed.entries.size()!=count+1U)continue;
  parsed.wasCompressed=dest.wasCompressed;parsed.conversionPackingChunk=true;packedDestination=index;packedChunk=std::move(parsed);break;
 }
 std::vector<std::uint8_t> rebuiltSource(NsfChunkSize,0U);
 std::copy_n(source.bytes.begin(),16U,rebuiltSource.begin());
 if(prefixPadding)std::copy_n(source.bytes.begin()+static_cast<std::ptrdiff_t>(tableEnd),prefixPadding,
                              rebuiltSource.begin()+static_cast<std::ptrdiff_t>(newTableEnd));
 std::size_t cursor=newTableEnd+prefixPadding;
 if(cursor>NsfChunkSize)return fail("The source chunk header and padding exceed its capacity.");
 for(std::size_t i=0;i<count;++i){
  if(i==entryIndex)continue;
  const auto&entry=source.entries[i];
  if(!entry.valid||entry.offset>usedEnd||entry.size>usedEnd-entry.offset||entry.size>NsfChunkSize-cursor)
   return fail("Another entry in the source chunk has invalid bounds.");
  cursor+=entry.size;
 }
 // Repack all remaining entries while retaining each complete raw entry block and the
 // original prefix padding. Entry-internal offsets remain relative to their own block.
 cursor=newTableEnd+prefixPadding;
 std::size_t outputEntry=0;
 for(std::size_t i=0;i<count;++i){
  if(i==entryIndex)continue;
  const auto&entry=source.entries[i];
  const std::size_t oldOffset=entry.offset;
  if(entry.size)std::copy_n(source.bytes.begin()+static_cast<std::ptrdiff_t>(oldOffset),entry.size,
                            rebuiltSource.begin()+static_cast<std::ptrdiff_t>(cursor));
  for(unsigned b=0;b<4U;++b)rebuiltSource[16U+outputEntry*4U+b]=static_cast<std::uint8_t>(cursor>>(b*8U));
  cursor+=entry.size;++outputEntry;
 }
 for(unsigned b=0;b<4U;++b)rebuiltSource[16U+remainingCount*4U+b]=static_cast<std::uint8_t>(cursor>>(b*8U));
 const std::size_t unusedTail=NsfChunkSize-usedEnd;
 if(unusedTail>NsfChunkSize-cursor)return fail("The rebuilt source chunk has no room for its original trailing padding.");
 if(unusedTail)std::copy_n(source.bytes.begin()+static_cast<std::ptrdiff_t>(usedEnd),unusedTail,
                            rebuiltSource.begin()+static_cast<std::ptrdiff_t>(cursor));
 for(unsigned b=0;b<4U;++b)rebuiltSource[8U+b]=static_cast<std::uint8_t>(remainingCount>>(b*8U));

 std::string parseError;std::size_t parseOffset=0;NsfChunk parsedSource;
 if(!decodeChunk(rebuiltSource,parseOffset,parsedSource,parseError)||parseOffset!=NsfChunkSize||!parsedSource.entryTableValid)return fail("Could not rebuild the source chunk's entry table safely.");
 parsedSource.wasCompressed=source.wasCompressed;std::size_t relocatedEntryIndex=0;
 if(packedDestination==std::numeric_limits<std::size_t>::max()){
  std::vector<std::uint8_t> newBytes(NsfChunkSize,0U);newBytes[0]=0x34U;newBytes[1]=0x12U;newBytes[8]=1U;newBytes[16]=24U;
  const std::size_t newUsedEnd=24U+selected.size;write32(newBytes,20U,static_cast<std::uint32_t>(newUsedEnd));
  if(selected.size)std::copy_n(source.bytes.begin()+static_cast<std::ptrdiff_t>(selected.offset),selected.size,newBytes.begin()+24);
  parseOffset=0;NsfChunk parsedDestination;
  if(!decodeChunk(newBytes,parseOffset,parsedDestination,parseError)||parseOffset!=NsfChunkSize||!parsedDestination.entryTableValid||parsedDestination.entries.size()!=1U||!parsedDestination.entries[0].valid)return fail("Could not validate the new chunk after relocating the entry.");
  parsedDestination.conversionPackingChunk=true;candidate.chunks_.push_back(std::move(parsedDestination));newChunkIndex=candidate.chunks_.size()-1U;
 }else{candidate.chunks_[packedDestination]=std::move(packedChunk);newChunkIndex=packedDestination;relocatedEntryIndex=candidate.chunks_[packedDestination].entries.size()-1U;}
 candidate.chunks_[chunkIndex]=std::move(parsedSource);
 if(candidate.goolDecoded_)candidate.decodeGools(candidate.goolVersion_);
 candidate.decodeZones(candidate.crash3Retail_);
 if(candidate.animationModelsDecoded_)candidate.decodeAnimationModels(candidate.animationModelsCrash3_);
 // Chunk reparsing reconstructs frames from raw bytes; pending poses are not
 // serialized yet. Restore by original chunk/entry/frame identity, not EID:
 // archives may contain duplicate EIDs, and EID-only matching can apply one
 // animation's pose to another entry.
 for(const auto&pending:pendingPoses){
  std::size_t targetChunkIndex=pending.chunkIndex;
  std::size_t targetEntryIndex=pending.entryIndex;
  if(pending.chunkIndex==chunkIndex){
   if(pending.entryIndex==entryIndex){
    targetChunkIndex=newChunkIndex;
    targetEntryIndex=relocatedEntryIndex;
   }else if(pending.entryIndex>entryIndex){
    targetEntryIndex=pending.entryIndex-1U;
   }
  }
  if(targetChunkIndex>=candidate.chunks_.size()){
   error="Could not locate a pending animation pose after NSF chunk relocation.";return false;
  }
  auto&targetChunk=candidate.chunks_[targetChunkIndex];
  if(targetEntryIndex>=targetChunk.entries.size()){
   error="Could not locate a pending animation entry after NSF chunk relocation.";return false;
  }
  auto&targetEntry=targetChunk.entries[targetEntryIndex];
  if(!targetEntry.valid||targetEntry.type!=1U||pending.frameIndex>=targetEntry.animation.frames.size()){
   error="Could not restore a pending animation pose after NSF chunk relocation.";return false;
  }
  auto&targetFrame=targetEntry.animation.frames[pending.frameIndex];
  if(!targetFrame.verticesDecoded||targetFrame.vertices.size()!=pending.vertices.size()){
   error="Could not restore a pending animation pose after NSF chunk relocation.";return false;
  }
  std::vector<std::size_t>indices;indices.reserve(pending.vertices.size());
  for(std::size_t vi=0;vi<pending.vertices.size();++vi)indices.push_back(vi);
  if(!candidate.setAnimationFrameVertices(targetChunkIndex,targetEntryIndex,pending.frameIndex,indices,pending.vertices)){
   error="Could not reapply pending animation vertex edits after NSF chunk relocation.";return false;
  }
 }
 *this=std::move(candidate);
 error.clear();return true;
}

bool NsfArchive::appendAnimationFrame(std::size_t chunkIndex,std::size_t entryIndex){
 if(chunkIndex>=chunks_.size())return false;
 auto&chunk=chunks_[chunkIndex];
 if(chunk.type!=0U||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];
 if(entry.animation.frames.empty())return false;
 const std::size_t last=entry.animation.frames.size()-1U;
 const bool changed=rebuildAnimationFrameItems(chunk,entry,last,true,entry.animation.frames.size());
 if(changed)decodeZones(crash3Retail_);
 return changed;
}
bool NsfArchive::insertAnimationFrameAfter(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex){
 if(chunkIndex>=chunks_.size())return false;
 auto&chunk=chunks_[chunkIndex];
 if(chunk.type!=0U||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];
 if(frameIndex>=entry.animation.frames.size())return false;
 const bool changed=rebuildAnimationFrameItems(chunk,entry,frameIndex,true,frameIndex+1U);
 if(changed)decodeZones(crash3Retail_);
 return changed;
}
bool NsfArchive::deleteAnimationFrame(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex){
 if(chunkIndex>=chunks_.size())return false;
 auto&chunk=chunks_[chunkIndex];
 if(chunk.type!=0U||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];
 if(entry.animation.frames.size()<=1U||frameIndex>=entry.animation.frames.size())return false;
 const bool changed=rebuildAnimationFrameItems(chunk,entry,frameIndex,false,0U);
 if(changed)decodeZones(crash3Retail_);
 return changed;
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
bool NsfArchive::setAnimationFrameCollisions(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,const std::vector<std::size_t>&indices,const std::vector<NsfAnimationCollision>&values){
 if(indices.empty()||indices.size()!=values.size()||chunkIndex>=chunks_.size())return false;
 for(std::size_t i=0;i<indices.size();++i)if(std::find(indices.begin(),indices.begin()+static_cast<std::ptrdiff_t>(i),indices[i])!=indices.begin()+static_cast<std::ptrdiff_t>(i))return false;
 NsfArchive candidate=*this;
 for(std::size_t i=0;i<indices.size();++i)for(std::size_t field=0;field<10U;++field)
  if(!candidate.setAnimationFrameCollisionValue(chunkIndex,entryIndex,frameIndex,indices[i],field,values[i].values[field]))return false;
 if(entryIndex>=chunks_[chunkIndex].entries.size()||entryIndex>=candidate.chunks_[chunkIndex].entries.size())return false;
 auto&dst=chunks_[chunkIndex].entries[entryIndex];const auto&src=candidate.chunks_[chunkIndex].entries[entryIndex];
 if(frameIndex>=dst.animation.frames.size()||frameIndex>=src.animation.frames.size())return false;
 chunks_[chunkIndex].bytes=candidate.chunks_[chunkIndex].bytes;
 dst.animation.frames[frameIndex].collisions=src.animation.frames[frameIndex].collisions;return true;
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
  // Special XYZ bytes overlap the temporal-word storage. Refresh the cached
  // words from the bytes just edited before decoding, otherwise the working
  // pose keeps stale compressed deltas and save can overwrite the special edit.
  const std::size_t fakeHeader=fixedHeader+static_cast<std::size_t>(frame.collisionCount)*0x28U;
  const std::size_t temporalOffset=frame.rawOffset+fakeHeader;
  for(std::size_t wi=0;wi<frame.temporalWords.size();++wi){
   const std::size_t offset=temporalOffset+wi*4U;
   if(!has(chunk.bytes,offset,4U))return false;
   frame.temporalWords[wi]=u32(chunk.bytes,offset);
  }
  const NsfModel*linkedModel=nullptr;
  for(const auto&modelChunk:chunks_){if(modelChunk.type!=0||!modelChunk.entryTableValid)continue;for(const auto&modelEntry:modelChunk.entries)if(modelEntry.valid&&modelEntry.type==2&&modelEntry.eid==frame.linkedModelEid&&modelEntry.model.decoded){linkedModel=&modelEntry.model;break;}if(linkedModel)break;}
  decodeAnimationVertices(frame,linkedModel,entry.animation.isCrash3);
 }
 return true;
}
bool NsfArchive::encodeAnimationFrameVertex(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,std::size_t vertexIndex,std::size_t axis,std::int32_t value){
 if(chunkIndex>=chunks_.size()||axis>=3U)return false;
 auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=1||!entry.animation.decoded||frameIndex>=entry.animation.frames.size())return false;
 auto&frame=entry.animation.frames[frameIndex];if(!frame.verticesDecoded||vertexIndex>=frame.vertexCount)return false;
 const bool crash3=entry.animation.isCrash3;
 if(crash3){if(value%8!=0)return false;value/=8;}
 // Animation vertex components are stored/decoded as unsigned bytes.
 // The full representable range is 0-255 (0-254 was an off-by-one cap).
 const std::int32_t maximumCoordinate=255;
 if(value<0||value>maximumCoordinate)return false;
 const std::size_t fixedHeader=crash3?28U:24U;
 const std::size_t fakeHeader=fixedHeader+static_cast<std::size_t>(frame.collisionCount)*0x28U;
 if(frame.rawSize<fakeHeader||!has(chunk.bytes,frame.rawOffset,frame.rawSize))return false;
 const NsfModel*model=nullptr;
 for(const auto&modelChunk:chunks_)if(modelChunk.type==0&&modelChunk.entryTableValid)
  for(const auto&modelEntry:modelChunk.entries)
   if(modelEntry.valid&&modelEntry.type==2&&modelEntry.eid==frame.linkedModelEid&&modelEntry.model.decoded){model=&modelEntry.model;break;}
 if(vertexIndex<frame.specialVertexCount&&!(model&&model->compressed)){
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
  if(vertexIndex>=model->positions.size()||model->positions.size()+frame.specialVertexCount!=frame.vertices.size())return false;
  std::size_t bit=start;std::int32_t xAcc=0,yAcc=0,zAcc=0;
  auto toByte=[&](std::int32_t source,std::int32_t&target){if(crash3){if(source%8!=0)return false;source/=8;}if(source<0||source>maximumCoordinate)return false;target=source;return true;};
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
   // Complex ModelPosition i is decoded into frame.vertices[i].
   // specialVertexCount advances the compressed bit cursor and the renderer's
   // polygon index bias; it does not shift the decoded/editable pose slots.
   const std::size_t frameVertexIndex=i;
   if(frameVertexIndex>=frame.vertices.size())return false;
   const auto&source=frame.vertices[frameVertexIndex];
   if(!toByte(frameVertexIndex==vertexIndex&&axis==0U?value:source.x,targetX)||!toByte(frameVertexIndex==vertexIndex&&axis==1U?value:source.y,targetY)||!toByte(frameVertexIndex==vertexIndex&&axis==2U?value:source.z,targetZ))return false;
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
   if(v<0||v>maximumCoordinate)return false;
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
 // The special-vertex bytes are a raw view into the temporal-word payload.
 // Refresh their cache from the bytes we actually serialized; do not restore
 // the old XYZ values, which would overwrite compressed delta bits.
 if(frame.specialVertices.size()<frame.specialVertexCount)return false;
 for(std::size_t vi=0;vi<frame.specialVertexCount;++vi)
  for(std::size_t axisIndex=0;axisIndex<3U;++axisIndex){
   const std::size_t offset=temporalOffset+vi*3U+axisIndex;
   if(!has(chunk.bytes,offset,1U))return false;
   frame.specialVertices[vi][axisIndex]=chunk.bytes[offset];
  }
 return decodeAnimationVertices(frame,model,crash3);
}


bool NsfArchive::setAnimationFrameVertex(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,std::size_t vertexIndex,std::size_t axis,std::int32_t value){
 if(chunkIndex>=chunks_.size()||axis>=3U)return false;
 auto&chunk=chunks_[chunkIndex];if(chunk.type!=0||entryIndex>=chunk.entries.size())return false;
 auto&entry=chunk.entries[entryIndex];if(!entry.valid||entry.type!=1U||!entry.animation.decoded||frameIndex>=entry.animation.frames.size())return false;
 auto&frame=entry.animation.frames[frameIndex];if(!frame.verticesDecoded||vertexIndex>=frame.vertices.size())return false;
 auto&vertex=frame.vertices[vertexIndex];if(axis==0U)vertex.x=value;else if(axis==1U)vertex.y=value;else vertex.z=value;
 frame.verticesEdited=true;return true;
}
bool NsfArchive::setAnimationFrameVertices(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,const std::vector<std::size_t>&vertexIndices,const std::vector<NsfAnimationVertex>&vertices){
 if(vertexIndices.empty()||vertexIndices.size()!=vertices.size()||chunkIndex>=chunks_.size())return false;
 if(chunks_[chunkIndex].type!=0||entryIndex>=chunks_[chunkIndex].entries.size())return false;
 const auto&entry=chunks_[chunkIndex].entries[entryIndex];
 if(!entry.valid||entry.type!=1U||!entry.animation.decoded||frameIndex>=entry.animation.frames.size())return false;
 const auto&frame=entry.animation.frames[frameIndex];if(!frame.verticesDecoded)return false;
 for(std::size_t i=0;i<vertexIndices.size();++i){
  if(vertexIndices[i]>=frame.vertices.size()||std::find(vertexIndices.begin(),vertexIndices.begin()+static_cast<std::ptrdiff_t>(i),vertexIndices[i])!=vertexIndices.begin()+static_cast<std::ptrdiff_t>(i))return false;
 }
 auto&destination=chunks_[chunkIndex].entries[entryIndex].animation.frames[frameIndex];
 for(std::size_t i=0;i<vertexIndices.size();++i)destination.vertices[vertexIndices[i]]=vertices[i];
 destination.verticesEdited=true;return true;
}

bool NsfArchive::setAnimationFrameVerticesRebased(std::size_t chunkIndex,std::size_t entryIndex,std::size_t frameIndex,const std::vector<std::size_t>&vertexIndices,const std::vector<NsfAnimationVertex>&vertices,std::int32_t xOffset,std::int32_t yOffset,std::int32_t zOffset){
 if(xOffset<std::numeric_limits<std::int16_t>::min()||xOffset>std::numeric_limits<std::int16_t>::max()||
    yOffset<std::numeric_limits<std::int16_t>::min()||yOffset>std::numeric_limits<std::int16_t>::max()||
    zOffset<std::numeric_limits<std::int16_t>::min()||zOffset>std::numeric_limits<std::int16_t>::max()||chunkIndex>=chunks_.size())return false;
 NsfArchive candidate=*this;
 if(!candidate.setAnimationFrameVertices(chunkIndex,entryIndex,frameIndex,vertexIndices,vertices)||
    !candidate.setAnimationFrameHeaderValue(chunkIndex,entryIndex,frameIndex,0U,xOffset)||
    !candidate.setAnimationFrameHeaderValue(chunkIndex,entryIndex,frameIndex,1U,yOffset)||
    !candidate.setAnimationFrameHeaderValue(chunkIndex,entryIndex,frameIndex,2U,zOffset))return false;
 chunks_=std::move(candidate.chunks_);return true;
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

static bool makeNormalFramePayload(const NsfChunk&chunk,const NsfAnimationFrame&frame,
                                  const NsfModel&model,std::vector<std::uint8_t>&payload,
                                  std::string&error){
 const std::size_t fixedHeader=24U;
 const std::size_t fakeHeader=fixedHeader+static_cast<std::size_t>(frame.collisionCount)*0x28U;
 const std::size_t prefixBytes=frame.specialVertexCount*3U;
 if(frame.vertexCount!=model.vertexCount||frame.specialVertexCount!=model.specialVertexCount||
    model.positions.size()+frame.specialVertexCount!=frame.vertexCount||
    frame.vertices.size()!=frame.vertexCount||!frame.verticesDecoded||
    frame.specialVertices.size()!=frame.specialVertexCount){
  error="A linked frame's vertex/special-prefix layout does not match the selected Complex Model.";return false;
 }
 // The Normal temporal byte-lane permutation reverses four-byte lanes. A
 // prefix not aligned to a full word aliases special header bytes with the
 // first ordinary coordinate bytes. Until that layout is independently proven,
 // reject it rather than silently corrupt the special vertices.
 if((prefixBytes&3U)!=0U){
  error="This frame's special-vertex prefix is not 32-bit aligned; conversion is stopped to protect overlapping special/temporal bytes.";return false;
 }
 if(frame.headerSize!=fakeHeader+prefixBytes||frame.rawSize<fakeHeader+prefixBytes||
    !has(chunk.bytes,frame.rawOffset,frame.rawSize)){
  error="A linked frame has an invalid header/payload boundary.";return false;
 }
 for(std::size_t i=0;i<frame.specialVertexCount;++i){
  const std::size_t p=frame.rawOffset+fakeHeader+i*3U;
  for(std::size_t axis=0;axis<3U;++axis)if(chunk.bytes[p+axis]!=frame.specialVertices[i][axis]){
   error="A special vertex cache does not match the source frame bytes.";return false;
  }
 }
 const std::size_t ordinaryCount=model.positions.size();
 const std::size_t requiredBits=(prefixBytes+ordinaryCount*3U)*8U;
 const std::size_t wordCount=std::max(frame.temporalWords.size(),(requiredBits+31U)/32U);
 if(wordCount>(NsfChunkSize-fakeHeader)/4U){error="The converted Normal frame exceeds one NSF chunk.";return false;}
 payload.assign(fakeHeader+wordCount*4U,0U);
 std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(frame.rawOffset),fakeHeader+prefixBytes,payload.begin());
 std::vector<std::uint8_t>rawBits(wordCount*32U,0U),reordered(wordCount*32U,0U);
 for(std::size_t wi=0;wi<wordCount;++wi){
  const std::uint32_t word=wi<frame.temporalWords.size()?frame.temporalWords[wi]:0U;
  for(unsigned bit=0;bit<32U;++bit)rawBits[wi*32U+bit]=static_cast<std::uint8_t>((word>>(31U-bit))&1U);
 }
 for(std::size_t base=0;base+32U<=rawBits.size();base+=32U)
  for(std::size_t byte=0;byte<4U;++byte)
   for(std::size_t bit=0;bit<8U;++bit)
    reordered[base+24U-byte*8U+bit]=rawBits[base+byte*8U+bit];
 std::size_t cursor=prefixBytes*8U;
 for(std::size_t i=0;i<ordinaryCount;++i){
  const auto&v=frame.vertices[i];
  const std::int32_t values[3]={v.x,v.y,v.z};
  for(const auto value:values){
   if(value<0||value>255){error="A Complex ModelPosition is outside the Normal frame's byte coordinate range.";return false;}
   for(unsigned bit=0;bit<8U;++bit)reordered[cursor++]=static_cast<std::uint8_t>((static_cast<std::uint32_t>(value)>>(7U-bit))&1U);
  }
 }
 for(std::size_t base=0;base+32U<=rawBits.size();base+=32U)
  for(std::size_t byte=0;byte<4U;++byte)
   for(std::size_t bit=0;bit<8U;++bit)
    rawBits[base+byte*8U+bit]=reordered[base+24U-byte*8U+bit];
 for(std::size_t wi=0;wi<wordCount;++wi){
  std::uint32_t word=0;
  for(unsigned bit=0;bit<32U;++bit)word=(word<<1U)|rawBits[wi*32U+bit];
  for(unsigned byte=0;byte<4U;++byte)payload[fakeHeader+wi*4U+byte]=static_cast<std::uint8_t>(word>>(byte*8U));
 }
 error.clear();return true;
}

static bool removeCompressedPositionItem(NsfChunk&chunk,NsfEntry&entry,std::string&error){
 if(!entry.valid||entry.type!=2U||entry.items.size()!=6U||entry.itemCount!=6U){
  error="Selected Model does not have the expected six-item Complex layout.";return false;
 }
 const std::size_t entryIndex=static_cast<std::size_t>(&entry-chunk.entries.data());
 const std::size_t entryCount=chunk.entries.size();
 if(entryIndex>=entryCount||u32(chunk.bytes,8U)!=entryCount){error="Model entry table is inconsistent.";return false;}
 const std::size_t oldEntryEnd=entry.offset+entry.size;
 const std::size_t tableEnd=16U+(entryCount+1U)*4U;
 if(entry.offset<tableEnd||oldEntryEnd>NsfChunkSize){error="Model entry lies outside its chunk's valid entry region.";return false;}
 const std::size_t sentinelOffset=16U+entryCount*4U;
 const std::size_t usedEnd=u32(chunk.bytes,sentinelOffset);
 if(usedEnd<oldEntryEnd||usedEnd>NsfChunkSize){error="Model chunk's entry-table sentinel is invalid.";return false;}
 const std::size_t oldHeaderEnd=16U+(entry.items.size()+1U)*4U;
 const std::size_t newItemCount=5U,newHeaderEnd=16U+(newItemCount+1U)*4U;
 const std::size_t firstItemOffset=entry.items.front().offset-entry.offset;
 if(entry.size<oldHeaderEnd||firstItemOffset<oldHeaderEnd||firstItemOffset>entry.size){error="Model item table has invalid padding/offsets.";return false;}
 const std::size_t prefixPadding=firstItemOffset-oldHeaderEnd;
 const std::size_t oldPayloadEnd=entry.items.back().offset+entry.items.back().size;
 if(oldPayloadEnd>oldEntryEnd){error="Model's compressed-position item exceeds its entry.";return false;}
 const std::size_t trailingPadding=oldEntryEnd-oldPayloadEnd;
 std::size_t newEntrySize=newHeaderEnd+prefixPadding;
 for(std::size_t i=0;i<newItemCount;++i){
  if(entry.items[i].offset<entry.offset||entry.items[i].size>NsfChunkSize-newEntrySize){error="Model item sizes exceed the NSF chunk.";return false;}
  newEntrySize+=entry.items[i].size;
 }
 if(trailingPadding>NsfChunkSize-newEntrySize){error="Model entry padding exceeds the NSF chunk.";return false;}
 const std::size_t newPayloadEnd=newEntrySize;newEntrySize+=trailingPadding;
 if(entry.offset>NsfChunkSize||newEntrySize>NsfChunkSize-entry.offset){error="Converted Model entry does not fit its NSF chunk.";return false;}
 const std::ptrdiff_t delta=static_cast<std::ptrdiff_t>(newEntrySize)-static_cast<std::ptrdiff_t>(entry.size);
 const std::ptrdiff_t newUsedEnd=static_cast<std::ptrdiff_t>(usedEnd)+delta;
 if(newUsedEnd<0||newUsedEnd>static_cast<std::ptrdiff_t>(NsfChunkSize)){error="Converted Model would overflow its NSF chunk.";return false;}
 std::vector<std::uint8_t>newEntry(newEntrySize,0U);
 std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset),16U,newEntry.begin());
 auto write32=[](std::vector<std::uint8_t>&bytes,std::size_t offset,std::uint32_t value){for(unsigned b=0;b<4U;++b)bytes[offset+b]=static_cast<std::uint8_t>(value>>(b*8U));};
 write32(newEntry,12U,static_cast<std::uint32_t>(newItemCount));
 std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset+oldHeaderEnd),prefixPadding,newEntry.begin()+static_cast<std::ptrdiff_t>(newHeaderEnd));
 std::size_t cursor=newHeaderEnd+prefixPadding;
 for(std::size_t i=0;i<newItemCount;++i){
  write32(newEntry,16U+i*4U,static_cast<std::uint32_t>(cursor));
  const auto&item=entry.items[i];
  std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(item.offset),item.size,newEntry.begin()+static_cast<std::ptrdiff_t>(cursor));
  cursor+=item.size;
 }
 write32(newEntry,16U+newItemCount*4U,static_cast<std::uint32_t>(cursor));
 if(trailingPadding)std::copy_n(chunk.bytes.begin()+static_cast<std::ptrdiff_t>(oldPayloadEnd),trailingPadding,newEntry.begin()+static_cast<std::ptrdiff_t>(newPayloadEnd));
 const std::size_t newEntryEnd=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(oldEntryEnd)+delta);
 const std::size_t suffixSize=usedEnd-oldEntryEnd;
 if(suffixSize)std::memmove(chunk.bytes.data()+newEntryEnd,chunk.bytes.data()+oldEntryEnd,suffixSize);
 std::copy(newEntry.begin(),newEntry.end(),chunk.bytes.begin()+static_cast<std::ptrdiff_t>(entry.offset));
 entry.size=newEntrySize;entry.itemCount=static_cast<std::uint32_t>(newItemCount);entry.items.clear();entry.items.reserve(newItemCount);
 for(std::size_t i=0;i<newItemCount;++i){
  const std::size_t begin=u32(chunk.bytes,entry.offset+16U+i*4U),end=u32(chunk.bytes,entry.offset+16U+(i+1U)*4U);
  entry.items.push_back({entry.offset+begin,end-begin});
 }
 for(std::size_t i=entryIndex+1U;i<chunk.entries.size();++i){
  auto&following=chunk.entries[i];following.offset=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(following.offset)+delta);
  for(auto&item:following.items)item.offset=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(item.offset)+delta);
  if(following.type==1U&&following.animation.frames.size()==following.items.size())
   for(std::size_t fi=0;fi<following.animation.frames.size();++fi){following.animation.frames[fi].rawOffset=following.items[fi].offset;following.animation.frames[fi].rawSize=following.items[fi].size;}
 }
 for(std::size_t i=0;i<chunk.entries.size();++i)write32(chunk.bytes,16U+i*4U,static_cast<std::uint32_t>(chunk.entries[i].offset));
 write32(chunk.bytes,sentinelOffset,static_cast<std::uint32_t>(newUsedEnd));
 error.clear();return true;
}

bool NsfArchive::convertComplexModelToNormal(std::uint32_t modelEid,std::string&error){
 NsfArchive candidate=*this;
 if(!candidate.commitAnimationEdits(error))return false;
 NsfEntryLocation modelLocation{};bool modelFound=false;
 for(std::size_t ci=0;ci<candidate.chunks_.size()&&!modelFound;++ci){
  const auto&chunk=candidate.chunks_[ci];if(chunk.type!=0U||!chunk.entryTableValid)continue;
  for(std::size_t ei=0;ei<chunk.entries.size();++ei){
   const auto&entry=chunk.entries[ei];
   if(entry.valid&&entry.type==2U&&entry.eid==modelEid&&entry.model.decoded&&entry.model.compressed){modelLocation={ci,ei};modelFound=true;break;}
  }
 }
 if(!modelFound){error="Selected EID is not a decoded Complex Model.";return false;}
 const NsfModel sourceModel=candidate.chunks_[modelLocation.chunkIndex].entries[modelLocation.entryIndex].model;
 if(sourceModel.specialVertexCount%4U!=0U){
  error="This Model's special-vertex count is not 32-bit aligned in the Normal temporal layout. Conversion is safely refused until that byte overlap is resolved.";return false;
 }
 struct ExpectedPose {std::size_t chunkIndex,entryIndex,frameIndex;std::uint32_t animationEid;std::vector<NsfAnimationVertex>positions;std::vector<std::array<std::uint8_t,3>>special;};
 std::vector<ExpectedPose>expected;std::size_t linkedFrameCount=0;
 std::vector<NsfEntryLocation>animationLocations;
 for(std::size_t ci=0;ci<candidate.chunks_.size();++ci){
  const auto&chunk=candidate.chunks_[ci];if(chunk.type!=0U||!chunk.entryTableValid)continue;
  for(std::size_t ei=0;ei<chunk.entries.size();++ei)
   if(chunk.entries[ei].valid&&chunk.entries[ei].type==1U)animationLocations.push_back({ci,ei});
 }
 // Descending entry order keeps not-yet-processed source indices stable when
 // a too-large Animation is removed and relocated out of its chunk.
 std::sort(animationLocations.begin(),animationLocations.end(),[](const NsfEntryLocation&a,const NsfEntryLocation&b){
  return a.chunkIndex!=b.chunkIndex?a.chunkIndex>b.chunkIndex:a.entryIndex>b.entryIndex;
 });
 for(const auto&location:animationLocations){
  const std::size_t ci=location.chunkIndex,ei=location.entryIndex;
  if(ci>=candidate.chunks_.size()||ei>=candidate.chunks_[ci].entries.size()){
   error="An Animation entry moved unexpectedly during batch conversion.";return false;
  }
  auto&entry=candidate.chunks_[ci].entries[ei];
  if(!entry.valid||entry.type!=1U)continue;
  if(!entry.animation.decoded){error="Batch conversion cannot proceed because an Animation entry is undecoded; linked references cannot be exhaustively checked.";return false;}
  auto buildPayloads=[&](std::size_t targetChunk,std::size_t targetEntry,
                         std::vector<std::vector<std::uint8_t>>&payloads,
                         std::vector<ExpectedPose>&poses,std::size_t&linkedFrames)->bool{
   if(targetChunk>=candidate.chunks_.size()||targetEntry>=candidate.chunks_[targetChunk].entries.size()){
    error="Relocated Animation entry could not be located.";return false;
   }
   const auto&target=candidate.chunks_[targetChunk];
   const auto&anim=target.entries[targetEntry];
   if(!anim.valid||anim.type!=1U||!anim.animation.decoded){
    error="Relocated Animation entry did not decode.";return false;
   }
   payloads.clear();payloads.reserve(anim.animation.frames.size());poses.clear();linkedFrames=0;
   for(std::size_t fi=0;fi<anim.animation.frames.size();++fi){
    const auto&frame=anim.animation.frames[fi];
    if(frame.linkedModelEid!=modelEid){
     if(!has(target.bytes,frame.rawOffset,frame.rawSize)){
      error="An unrelated frame payload is outside its chunk.";return false;
     }
     payloads.emplace_back(target.bytes.begin()+static_cast<std::ptrdiff_t>(frame.rawOffset),
                          target.bytes.begin()+static_cast<std::ptrdiff_t>(frame.rawOffset+frame.rawSize));
     continue;
    }
    if(!frame.modelResolved||!frame.verticesDecoded||anim.animation.isCrash3){
     error="A linked frame is unresolved, undecoded, or uses an unsupported Crash 3 layout.";return false;
    }
    std::vector<std::uint8_t>converted;
    if(!makeNormalFramePayload(target,frame,sourceModel,converted,error))return false;
    payloads.push_back(std::move(converted));++linkedFrames;
    std::vector<NsfAnimationVertex>ordinaryPose(frame.vertices.begin(),
     frame.vertices.begin()+static_cast<std::ptrdiff_t>(sourceModel.positions.size()));
    poses.push_back({targetChunk,targetEntry,fi,anim.eid,std::move(ordinaryPose),frame.specialVertices});
   }
   return true;
  };
  std::vector<std::vector<std::uint8_t>>payloads;
  std::vector<ExpectedPose>poses;
  std::size_t linkedFrames=0;
  if(!buildPayloads(ci,ei,payloads,poses,linkedFrames))return false;
  if(linkedFrames==0U)continue;
  if(replaceAnimationFramePayloads(candidate.chunks_[ci],candidate.chunks_[ci].entries[ei],payloads)){
   linkedFrameCount+=linkedFrames;
   expected.insert(expected.end(),std::make_move_iterator(poses.begin()),std::make_move_iterator(poses.end()));
   continue;
  }
  // Entry-level capacity was exceeded. Relocate the original untouched entry
  // into its own type-0 chunk, then regenerate converted frames against the
  // reparsed destination entry before attempting the replacement again.
  for(auto&pose:expected)
   if(pose.chunkIndex==ci&&pose.entryIndex>ei)--pose.entryIndex;
  std::size_t newChunkIndex=0;
  if(!candidate.moveEntryToNewChunk(ci,ei,newChunkIndex,error))return false;
  if(newChunkIndex>=candidate.chunks_.size()||candidate.chunks_[newChunkIndex].entries.size()!=1U){
   error="Animation relocation did not produce a single-entry destination chunk.";return false;
  }
  poses.clear();payloads.clear();linkedFrames=0;
  if(!buildPayloads(newChunkIndex,0U,payloads,poses,linkedFrames))return false;
  if(linkedFrames==0U){error="Relocated Animation no longer references the selected Model.";return false;}
  if(!replaceAnimationFramePayloads(candidate.chunks_[newChunkIndex],candidate.chunks_[newChunkIndex].entries[0],payloads)){
   error="The converted Animation entry itself exceeds the 64 KiB capacity of a new NSF chunk; conversion was aborted without changing the archive.";return false;
  }
  linkedFrameCount+=linkedFrames;
  expected.insert(expected.end(),std::make_move_iterator(poses.begin()),std::make_move_iterator(poses.end()));
 }
 if(linkedFrameCount==0U){error="No decoded Animation frames reference the selected Model EID.";return false;}
 // Animation relocation can shift the selected Model's entry index when it
 // shares a source chunk. Resolve it again by EID before rebuilding its item table.
 modelFound=false;
 for(std::size_t ci=0;ci<candidate.chunks_.size()&&!modelFound;++ci){
  const auto&chunk=candidate.chunks_[ci];if(chunk.type!=0U||!chunk.entryTableValid)continue;
  for(std::size_t ei=0;ei<chunk.entries.size();++ei){
   const auto&entry=chunk.entries[ei];
   if(entry.valid&&entry.type==2U&&entry.eid==modelEid&&entry.model.decoded&&entry.model.compressed){modelLocation={ci,ei};modelFound=true;break;}
  }
 }
 if(!modelFound){error="Selected Complex Model could not be located after Animation relocation.";return false;}
 auto&modelChunk=candidate.chunks_[modelLocation.chunkIndex];
 auto&modelEntry=modelChunk.entries[modelLocation.entryIndex];
 if(!removeCompressedPositionItem(modelChunk,modelEntry,error))return false;
 candidate.decodeAnimationModels(false);
 const auto&convertedModel=candidate.chunks_[modelLocation.chunkIndex].entries[modelLocation.entryIndex].model;
 if(!convertedModel.decoded||convertedModel.compressed||!convertedModel.polygonsDecoded||!convertedModel.positions.empty()){
  error="Converted Model did not reparse as a Normal Model.";return false;
 }
 if(convertedModel.triangles.size()!=sourceModel.triangles.size()){
  error="Converted Model polygon count changed during reparse.";return false;
 }
 for(std::size_t ti=0;ti<sourceModel.triangles.size();++ti){
  const auto&a=sourceModel.triangles[ti];const auto&b=convertedModel.triangles[ti];
  const auto bias=static_cast<std::int32_t>(sourceModel.specialVertexCount);
  if(b.a!=a.a+bias||b.b!=a.b+bias||b.c!=a.c+bias){
   error="Converted Model polygon vertex bias changed unexpectedly; archive transaction was discarded.";return false;
  }
 }
 for(const auto&pose:expected){
  if(pose.chunkIndex>=candidate.chunks_.size()||pose.entryIndex>=candidate.chunks_[pose.chunkIndex].entries.size()){
   error="Converted Animation location became invalid during archive reparse.";return false;
  }
  const auto&entry=candidate.chunks_[pose.chunkIndex].entries[pose.entryIndex];
  if(!entry.valid||entry.eid!=pose.animationEid||!entry.animation.decoded||pose.frameIndex>=entry.animation.frames.size()){
   error="A linked Animation did not reparse after conversion.";return false;
  }
  const auto&frame=entry.animation.frames[pose.frameIndex];
  if(frame.linkedModelEid!=modelEid||frame.specialVertexCount!=pose.special.size()||frame.vertices.size()!=frame.vertexCount){
   error="A converted frame's linked Model or vertex layout changed.";return false;
  }
  for(std::size_t i=0;i<pose.special.size();++i)
   if(frame.specialVertices[i]!=pose.special[i]){error="A converted frame's special vertices changed.";return false;}
  for(std::size_t i=0;i<pose.positions.size();++i){
   const auto&before=pose.positions[i];const auto&after=frame.vertices[frame.specialVertexCount+i];
   if(before.x!=after.x||before.y!=after.y||before.z!=after.z){
    error="A converted Normal frame did not reproduce its Complex pose exactly.";return false;
   }
  }
 }
 chunks_=std::move(candidate.chunks_);error.clear();return true;
}

}
