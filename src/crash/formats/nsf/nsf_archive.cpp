#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
namespace CrashEdit::Crash {
static std::uint16_t u16(const std::vector<std::uint8_t>&d,std::size_t p){return std::uint16_t(d[p])|std::uint16_t(std::uint16_t(d[p+1])<<8);}
static std::uint32_t u32(const std::vector<std::uint8_t>&d,std::size_t p){return std::uint32_t(d[p])|(std::uint32_t(d[p+1])<<8)|(std::uint32_t(d[p+2])<<16)|(std::uint32_t(d[p+3])<<24);}
static std::int32_t i32(const std::vector<std::uint8_t>&d,std::size_t p){return static_cast<std::int32_t>(u32(d,p));}
static bool has(const std::vector<std::uint8_t>&d,std::size_t p,std::size_t n){return p<=d.size()&&n<=d.size()-p;}
static void checksum(std::vector<std::uint8_t>&d){std::uint32_t c=0x12345678;for(std::size_t i=0;i<NsfChunkSize;++i){if(i<12||i>=16)c+=d[i];c=(c<<3)|(c>>29);}for(int i=0;i<4;++i)d[12+i]=std::uint8_t(c>>(8*i));}

static std::size_t align4(std::size_t n){return (n+3U)&~std::size_t(3U);}
static bool parseGoolFrameGroups(const std::vector<std::uint8_t>&bytes,const NsfItem&item,NsfGool&gool){
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
   if(end-p<12U){gool.frameGroupError="Truncated Crash 2 vertex-group header.";return false;}
   group.eid=u32(bytes,p+4);group.interpolated=u32(bytes,p+8)!=0;size=12U;
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
    if(entry.items.size()==6){gool.frameGroupBytes=entry.items[5].size;parseGoolFrameGroups(chunk.bytes,entry.items[5],gool);}
   }else if(gool.format==0){
    if(entry.items.size()!=3){fail("GOOL format 0 must contain exactly 3 items.");continue;}
   }else{fail("Unsupported GOOL format value "+std::to_string(gool.format)+".");continue;}
   for(std::size_t p=0;p<data.size;p+=4)gool.dataPool.push_back(i32(chunk.bytes,data.offset+p));
   bool mipsMode=false;
   for(std::size_t p=0;p<code.size;p+=4){
    const std::uint32_t word=u32(chunk.bytes,code.offset+p);
    NsfGoolInstruction instruction;instruction.byteOffset=p;instruction.word=word;instruction.isMips=mipsMode;
    if(mipsMode)decodeMipsInstruction(word,instruction.mnemonic,instruction.arguments,instruction.mipsKnown);
    else{
     instruction.opcode=static_cast<std::uint8_t>(word>>24U);instruction.operandBits=word&0x00FFFFFFU;
     const auto&info=goolOpcodeInfo(version,instruction.opcode);instruction.mnemonic=std::string(info.mnemonic);
     auto operands=decodeGoolOperands(version,instruction.opcode,instruction.operandBits);resolveGoolOperands(operands,gool);
     instruction.arguments=formatGoolOperands(operands);
    }
    gool.instructions.push_back(std::move(instruction));
    if(mipsMode){
     if(gool.instructions.size()>=2U){const auto&previous=gool.instructions[gool.instructions.size()-2U];if(previous.isMips&&(previous.word==0x03E0A809U||previous.word==0x03E00008U))mipsMode=false;}
    }else if(version==NsfGoolVersion::Version2||version==NsfGoolVersion::Version3)mipsMode=gool.instructions.back().opcode==73U;
   }
   gool.decoded=true;entry.gool=std::move(gool);
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
std::size_t NsfArchive::compressedChunkCount()const noexcept{return std::count_if(chunks_.begin(),chunks_.end(),[](const auto&c){return c.wasCompressed;});}
}
