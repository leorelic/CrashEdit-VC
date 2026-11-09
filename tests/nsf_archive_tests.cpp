#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/eid.hpp"
#include <algorithm>
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

 // A Crash 2 zone with a named object, GOOL arguments, victims and DDA metadata.
 std::vector<std::uint8_t> objectChunk(65536);p16(objectChunk,0,0x1234);p16(objectChunk,2,0);p32(objectChunk,4,5);p32(objectChunk,8,1);
 constexpr std::size_t objectEntry=24, objectHeaderSize=0x318, objectLayoutSize=0x24;
 struct TestProperty{std::uint16_t id;std::uint8_t type,size;std::vector<std::uint8_t> data;};
 auto numeric=[](std::uint32_t value){std::vector<std::uint8_t>d(8);p16(d,0,1);p32(d,4,value);return d;};
 auto victimData=[](){std::vector<std::uint8_t>d(8);p16(d,0,2);p16(d,4,7);p16(d,6,8);return d;};
 std::vector<TestProperty> props;
 props.push_back({0x002c,1,1,{6,0,0,0,'W','i','l','l','y',0,0,0}});
 props.push_back({0x004b,6,6,{1,0,0,0,10,0,20,0,30,0,0,0}});
 props.push_back({0x009f,19,4,numeric(77)});
 props.push_back({0x00a4,5,4,numeric(0x00001234)});
 props.push_back({0x00a9,19,4,numeric(0x0200007b)});
 props.push_back({0x00aa,19,4,numeric(3)});
 props.push_back({0x0287,18,2,victimData()});
 props.push_back({0x0277,19,4,numeric(5U<<8)});
 props.push_back({0x0288,19,4,numeric(2)});
 props.push_back({0x028b,5,4,numeric(9U<<8)});
 const std::size_t entityHeader=16+props.size()*8;std::vector<std::uint8_t> entity(entityHeader);
 p32(entity,12,static_cast<std::uint32_t>(props.size()));
 std::size_t dataAt=entityHeader;
 for(std::size_t n=0;n<props.size();++n){const auto&pr=props[n];p16(entity,16+n*8,pr.id);p16(entity,18+n*8,static_cast<std::uint16_t>(dataAt-12));entity[20+n*8]=pr.type;entity[21+n*8]=pr.size;p16(entity,22+n*8,1);entity.insert(entity.end(),pr.data.begin(),pr.data.end());dataAt+=pr.data.size();}
 p32(entity,0,static_cast<std::uint32_t>(entity.size()));
 const std::size_t objectEntrySize=32+objectHeaderSize+objectLayoutSize+entity.size();
 p32(objectChunk,16,objectEntry);p32(objectChunk,20,objectEntry+objectEntrySize);
 p32(objectChunk,objectEntry,0x0100ffff);p32(objectChunk,objectEntry+4,0x0200007b);p32(objectChunk,objectEntry+8,7);p32(objectChunk,objectEntry+12,3);
 p32(objectChunk,objectEntry+16,32);p32(objectChunk,objectEntry+20,32+objectHeaderSize);p32(objectChunk,objectEntry+24,32+objectHeaderSize+objectLayoutSize);p32(objectChunk,objectEntry+28,static_cast<std::uint32_t>(objectEntrySize));
 const std::size_t objectHeader=objectEntry+32,objectLayout=objectHeader+objectHeaderSize,objectData=objectLayout+objectLayoutSize;
 p32(objectChunk,objectHeader+0x18c,1);std::copy(entity.begin(),entity.end(),objectChunk.begin()+static_cast<std::ptrdiff_t>(objectData));
 assert(a.loadBytes(objectChunk,e));a.decodeZones(false);const auto&object=a.chunks()[0].entries[0].zone.entities[0];
 assert(object.valid&&object.name=="Willy"&&object.hasId&&object.id==77);
 assert(object.hasGoolType&&static_cast<std::uint32_t>(object.goolType)==0x0200007bU&&object.hasSubtype&&object.subtype==3);
 assert(object.positions.size()==1&&object.positions[0].x==10&&object.positions[0].y==20&&object.positions[0].z==30);
 assert(object.settings.size()==1&&object.settings[0].valueA==0x34&&object.settings[0].valueB==0x12);
 assert(object.victims.size()==2&&object.victims[0]==7&&object.victims[1]==8);
 assert(object.hasDdaSettings&&object.ddaDeathCount==5&&object.hasDdaSection&&object.ddaSection==2&&object.hasBoxCount&&object.boxCount==9);

 // A Crash 2 scenery entry with one vertex, triangle, quad, texture, color, and animated texture.
 std::vector<std::uint8_t> sceneryChunk(65536);p16(sceneryChunk,0,0x1234);p16(sceneryChunk,2,0);p32(sceneryChunk,4,6);p32(sceneryChunk,8,1);
 constexpr std::size_t sceneryEntry=24;
 constexpr std::size_t sceneryEntrySize=48+76+8+8+8+12+4+4;
 p32(sceneryChunk,16,sceneryEntry);p32(sceneryChunk,20,sceneryEntry+sceneryEntrySize);
 p32(sceneryChunk,sceneryEntry,0x0100ffff);p32(sceneryChunk,sceneryEntry+4,0x0200007b);p32(sceneryChunk,sceneryEntry+8,3);p32(sceneryChunk,sceneryEntry+12,7);
 std::size_t itemAt=48;
 const std::size_t itemSizes[]={76,8,8,8,12,4,4};
 for(std::size_t i=0;i<7;++i){p32(sceneryChunk,sceneryEntry+16+i*4,itemAt);itemAt+=itemSizes[i];}
 p32(sceneryChunk,sceneryEntry+16+7*4,sceneryEntrySize);
 const std::size_t sceneryInfo=sceneryEntry+48,vertexData=sceneryInfo+76,triangleData=vertexData+8,quadData=triangleData+8,textureData=quadData+8,colorData=textureData+12,animatedData=colorData+4;
 p32(sceneryChunk,sceneryInfo,10);p32(sceneryChunk,sceneryInfo+4,20);p32(sceneryChunk,sceneryInfo+8,30);p32(sceneryChunk,sceneryInfo+12,0);
 p32(sceneryChunk,sceneryInfo+16,1);p32(sceneryChunk,sceneryInfo+20,1);p32(sceneryChunk,sceneryInfo+24,1);p32(sceneryChunk,sceneryInfo+28,1);p32(sceneryChunk,sceneryInfo+32,1);p32(sceneryChunk,sceneryInfo+36,1);p32(sceneryChunk,sceneryInfo+40,1);p32(sceneryChunk,sceneryInfo+44,0x12345678);
 p32(sceneryChunk,vertexData,0x0032ffe1);p16(sceneryChunk,vertexData+4,0x0043);
 const std::uint32_t triangleA=0x80U|((0x123U>>4U)&0x7fU);p32(sceneryChunk,triangleData,triangleA);p16(sceneryChunk,triangleData+4,0x3U);
 const std::uint32_t quadA=0x80U|((0x234U>>4U)&0x7fU);const std::uint32_t quadB=(5U<<4U)|(0x234U&0xfU);p32(sceneryChunk,quadData,quadA);p32(sceneryChunk,quadData+4,quadB);
 sceneryChunk[textureData]=1;sceneryChunk[textureData+1]=2;sceneryChunk[textureData+2]=0x43;sceneryChunk[textureData+3]=4;sceneryChunk[textureData+4]=5;sceneryChunk[textureData+5]=6;sceneryChunk[textureData+6]=0xa7;sceneryChunk[textureData+7]=0x05;sceneryChunk[textureData+8]=7;sceneryChunk[textureData+9]=8;sceneryChunk[textureData+10]=9;sceneryChunk[textureData+11]=10;
 sceneryChunk[colorData]=11;sceneryChunk[colorData+1]=22;sceneryChunk[colorData+2]=33;sceneryChunk[colorData+3]=44;p32(sceneryChunk,animatedData,0xabcdef01);
 assert(a.loadBytes(sceneryChunk,e));a.decodeZones(false);const auto&scenery=a.chunks()[0].entries[0].scenery;
 assert(scenery.decoded&&scenery.xOffset==10&&scenery.yOffset==20&&scenery.zOffset==30&&!scenery.isSky);
 assert(scenery.texturePageEids.size()==1&&scenery.texturePageEids[0]==0x12345678);
 assert(scenery.vertices.size()==1&&scenery.vertices[0].x==-2&&scenery.vertices[0].y==3&&scenery.vertices[0].z==4&&scenery.vertices[0].colorIndex==0x213);
 assert(scenery.triangles.size()==1&&scenery.triangles[0].textureIndex==0x123&&scenery.triangles[0].animated);
 assert(scenery.quads.size()==1&&scenery.quads[0].textureIndex==0x234&&scenery.quads[0].unknown==5&&scenery.quads[0].animated);
 assert(scenery.textures.size()==1&&scenery.textures[0].u1==1&&scenery.textures[0].v4==10&&scenery.textures[0].clutX==3&&scenery.textures[0].clutY1==4&&scenery.textures[0].clutY2==4);
 assert(scenery.colors.size()==1&&scenery.colors[0].r==11&&scenery.colors[0].g==22&&scenery.colors[0].b==33&&scenery.colors[0].extra==44);
 assert(scenery.animatedTextures.size()==1&&scenery.animatedTextures[0]==0xabcdef01);

 // Crash 2 Type 2 model: all five base items plus the optional compressed-position item.
 std::vector<std::uint8_t> modelChunk(65536);p16(modelChunk,0,0x1234);p16(modelChunk,2,0);p32(modelChunk,4,4);p32(modelChunk,8,1);
 constexpr std::size_t modelEntry=24,modelItemCount=6,modelTable=16+modelItemCount*4;
 const std::size_t modelItemSizes[]={0x50,4,4,12,4,4};
 std::size_t modelEntrySize=modelTable;for(auto n:modelItemSizes)modelEntrySize+=n;
 p32(modelChunk,16,modelEntry);p32(modelChunk,20,modelEntry+modelEntrySize);
 p32(modelChunk,modelEntry,0x0100ffff);p32(modelChunk,modelEntry+4,0x0200007b);p32(modelChunk,modelEntry+8,2);p32(modelChunk,modelEntry+12,modelItemCount);
 std::size_t modelOffset=modelTable;
 for(std::size_t i=0;i<modelItemCount;++i){p32(modelChunk,modelEntry+16+i*4,static_cast<std::uint32_t>(modelOffset));modelOffset+=modelItemSizes[i];}
 p32(modelChunk,modelEntry+16+modelItemCount*4,static_cast<std::uint32_t>(modelEntrySize));
 const std::size_t modelInfo=modelEntry+modelTable;
 p32(modelChunk,modelInfo,100);p32(modelChunk,modelInfo+4,200);p32(modelChunk,modelInfo+8,300);p32(modelChunk,modelInfo+12,0x0200007b);
 p32(modelChunk,modelInfo+0x2c,1);p32(modelChunk,modelInfo+0x34,1);p32(modelChunk,modelInfo+0x38,1);p32(modelChunk,modelInfo+0x3c,1);p32(modelChunk,modelInfo+0x40,1);p32(modelChunk,modelInfo+0x44,1);p32(modelChunk,modelInfo+0x48,1);p32(modelChunk,modelInfo+0x4c,0);
 const std::size_t modelPoly=modelInfo+0x50,modelColor=modelPoly+4,modelTexture=modelColor+4,modelExtended=modelTexture+12,modelPosition=modelExtended+4;
 p32(modelChunk,modelPoly,0x12345678);modelChunk[modelColor]=10;modelChunk[modelColor+1]=20;modelChunk[modelColor+2]=30;modelChunk[modelColor+3]=40;
 modelChunk[modelTexture]=1;modelChunk[modelTexture+1]=2;modelChunk[modelTexture+2]=0x43;modelChunk[modelTexture+3]=4;modelChunk[modelTexture+4]=5;modelChunk[modelTexture+5]=6;modelChunk[modelTexture+6]=0xa7;modelChunk[modelTexture+7]=0x05;modelChunk[modelTexture+8]=7;modelChunk[modelTexture+9]=8;modelChunk[modelTexture+10]=9;modelChunk[modelTexture+11]=10;
 p32(modelChunk,modelExtended,0xabcdef01);p32(modelChunk,modelPosition,0x12345678);
 assert(a.loadBytes(modelChunk,e));a.decodeAnimationModels(false);const auto&model=a.chunks()[0].entries[0].model;
 assert(model.decoded&&model.compressed&&model.scaleX==100&&model.scaleY==200&&model.scaleZ==300);
 assert(model.texturePageEids.size()==1&&model.texturePageEids[0]==0x0200007bU);
 assert(model.polygonWords.size()==1&&model.polygonWords[0]==0x12345678U);
 assert(model.colors.size()==1&&model.colors[0].r==10&&model.colors[0].extra==40);
 assert(model.textures.size()==1&&model.textures[0].u1==1&&model.textures[0].v4==10&&model.textures[0].clutX==3);
 assert(model.extendedTextureWords.size()==1&&model.extendedTextureWords[0]==0xabcdef01U);
 assert(model.positions.size()==1&&model.positions[0].raw==0x12345678U);
 // Crash 2 Type 1 animation: one bounded frame with a direct Model EID.
 std::vector<std::uint8_t> animationChunk(65536);p16(animationChunk,0,0x1234);p16(animationChunk,2,0);p32(animationChunk,4,5);p32(animationChunk,8,1);
 constexpr std::size_t animationEntry=24,animationItemCount=1,animationTable=16+animationItemCount*4,animationFrameSize=24;
 const std::size_t animationEntrySize=animationTable+animationFrameSize;
 p32(animationChunk,16,animationEntry);p32(animationChunk,20,animationEntry+animationEntrySize);
 p32(animationChunk,animationEntry,0x0100ffff);p32(animationChunk,animationEntry+4,0x0200007b);p32(animationChunk,animationEntry+8,1);p32(animationChunk,animationEntry+12,animationItemCount);
 p32(animationChunk,animationEntry+16,static_cast<std::uint32_t>(animationTable));p32(animationChunk,animationEntry+20,static_cast<std::uint32_t>(animationEntrySize));
 const std::size_t animationFrame=animationEntry+animationTable;
 p16(animationChunk,animationFrame,1);p16(animationChunk,animationFrame+2,2);p16(animationChunk,animationFrame+4,3);p16(animationChunk,animationFrame+6,4);
 p32(animationChunk,animationFrame+8,0);p32(animationChunk,animationFrame+12,0);p32(animationChunk,animationFrame+16,0x0200007b);p32(animationChunk,animationFrame+20,24);
 assert(a.loadBytes(animationChunk,e));a.decodeAnimationModels(false);const auto&animation=a.chunks()[0].entries[0].animation;
 assert(animation.decoded&&animation.frames.size()==1&&animation.frames[0].modelEid==0x0200007bU);
 assert(animation.frames[0].xOffset==1&&animation.frames[0].yOffset==2&&animation.frames[0].zOffset==3);
 // Malformed frame header-size is rejected without reading beyond its item.
 p32(animationChunk,animationFrame+20,0xffffffffU);
 assert(a.loadBytes(animationChunk,e));a.decodeAnimationModels(false);assert(!a.chunks()[0].entries[0].animation.decoded);

 // Three-vertex fan command reconstructs one editable Model triangle.
 std::vector<std::uint8_t> triangleChunk(65536);p16(triangleChunk,0,0x1234);p16(triangleChunk,2,0);p32(triangleChunk,4,4);p32(triangleChunk,8,1);
 constexpr std::size_t triEntry=24,triItems=5,triTable=16+triItems*4;const std::size_t triSizes[]={0x50,12,52,12,4};std::size_t triSize=triTable;for(auto n:triSizes)triSize+=n;
 p32(triangleChunk,16,triEntry);p32(triangleChunk,20,triEntry+triSize);p32(triangleChunk,triEntry,0x0100ffff);p32(triangleChunk,triEntry+4,0x0200007b);p32(triangleChunk,triEntry+8,2);p32(triangleChunk,triEntry+12,triItems);
 std::size_t triOffset=triTable;for(std::size_t i=0;i<triItems;++i){p32(triangleChunk,triEntry+16+i*4,triOffset);triOffset+=triSizes[i];}p32(triangleChunk,triEntry+16+triItems*4,static_cast<std::uint32_t>(triSize));
 const std::size_t triInfo=triEntry+triTable,triPoly=triInfo+0x50,triColors=triPoly+12,triTextures=triColors+52,triExtended=triTextures+12;
 p32(triangleChunk,triInfo+0x2c,3);p32(triangleChunk,triInfo+0x34,1);p32(triangleChunk,triInfo+0x38,3);p32(triangleChunk,triInfo+0x3c,13);p32(triangleChunk,triInfo+0x40,1);p32(triangleChunk,triInfo+0x44,1);p32(triangleChunk,triInfo+0x48,1);p32(triangleChunk,triInfo+0x0c,0x4cd8206fU);
 for(std::uint32_t i=0;i<3;++i)p32(triangleChunk,triPoly+i*4,0x80000000U|((i+1U)<<16U)|((10U+i)<<9U)|1U);
 for(std::uint32_t i=0;i<13;++i){triangleChunk[triColors+i*4]=static_cast<std::uint8_t>(i);triangleChunk[triColors+i*4+1]=static_cast<std::uint8_t>(i+1);triangleChunk[triColors+i*4+2]=static_cast<std::uint8_t>(i+2);}
 assert(a.loadBytes(triangleChunk,e));a.decodeAnimationModels(false);const auto&triModel=a.chunks()[0].entries[0].model;assert(triModel.decoded&&triModel.polygonsDecoded&&triModel.triangles.size()==1);
 assert(triModel.triangles[0].a==0&&triModel.triangles[0].b==1&&triModel.triangles[0].c==2);
 assert(triModel.triangles[0].colorA==10&&triModel.triangles[0].colorB==11&&triModel.triangles[0].colorC==12);
 assert(triModel.extendedTextures.size()==1&&triModel.texturePageEids.size()==1);
 Crash::NsfSceneryColor editedColor{210,120,35,7};assert(a.setModelColor(0,0,10,editedColor));
 Crash::NsfModelTexture editedTexture{};editedTexture.u1=8;editedTexture.v1=9;editedTexture.u2=10;editedTexture.v2=11;editedTexture.colorMode=1;editedTexture.page=0;
 assert(a.setModelTexture(0,0,0,editedTexture));assert(a.setModelScale(0,0,0,777));assert(a.setModelTexturePageEid(0,0,0,0x4cd8206fU));
 assert(a.setModelExtendedTextureWord(0,0,0,0x00001001U));assert(a.setModelTriangleTexture(0,0,0,0,true));
 bool textureValid=false;assert(Crash::resolveNsfModelTextureIndex(a.chunks()[0].entries[0].model,0,true,0,textureValid)==0&&textureValid);
 std::vector<std::uint8_t> editedNsf;assert(a.saveUncompressedBytes(editedNsf,e));Crash::NsfArchive roundTrip;assert(roundTrip.loadBytes(editedNsf,e));roundTrip.decodeAnimationModels(false);
 const auto&roundModel=roundTrip.chunks()[0].entries[0].model;assert(roundModel.scaleX==777);assert(roundModel.colors[10].r==210&&roundModel.colors[10].g==120&&roundModel.colors[10].b==35);
 assert(roundModel.textures[0].u1==8&&roundModel.textures[0].v1==9&&roundModel.textures[0].page==0);
 assert(roundModel.texturePageEids[0]==0x4cd8206fU&&roundModel.extendedTextureWords[0]==0x00001001U);
 assert(roundModel.triangles.size()==1&&roundModel.triangles[0].animated&&roundModel.triangles[0].textureIndex==0);
 // Crash 3 animation stores the linked Model EID at +8, with a reserved word at +12.
 std::vector<std::uint8_t> c3Animation(65536);p16(c3Animation,0,0x1234);p16(c3Animation,2,0);p32(c3Animation,4,5);p32(c3Animation,8,1);
 constexpr std::size_t c3Entry=24,c3Frame=44,c3EntrySize=56;p32(c3Animation,16,c3Entry);p32(c3Animation,20,c3Entry+c3EntrySize);
 p32(c3Animation,c3Entry,0x0100ffff);p32(c3Animation,c3Entry+4,0x0200007b);p32(c3Animation,c3Entry+8,1);p32(c3Animation,c3Entry+12,1);
 p32(c3Animation,c3Entry+16,20);p32(c3Animation,c3Entry+20,c3EntrySize);
 p16(c3Animation,c3Frame,1);p16(c3Animation,c3Frame+2,2);p16(c3Animation,c3Frame+4,3);p16(c3Animation,c3Frame+6,4);
 p32(c3Animation,c3Frame+8,0x0200007b);p32(c3Animation,c3Frame+12,0);p32(c3Animation,c3Frame+16,0);p32(c3Animation,c3Frame+20,0);p32(c3Animation,c3Frame+24,28);
 assert(a.loadBytes(c3Animation,e));a.decodeAnimationModels(true);const auto&c3=a.chunks()[0].entries[0].animation;assert(c3.decoded&&c3.frames[0].modelEid==0x0200007bU);

 // Uncompressed animation frame expands its 24-bit vertex from the temporal stream.
 std::vector<std::uint8_t> vertexAnimation(65536);p16(vertexAnimation,0,0x1234);p16(vertexAnimation,2,0);p32(vertexAnimation,4,5);p32(vertexAnimation,8,1);
 constexpr std::size_t vaEntry=24,vaFrame=44,vaEntrySize=56;p32(vertexAnimation,16,vaEntry);p32(vertexAnimation,20,vaEntry+vaEntrySize);
 p32(vertexAnimation,vaEntry,0x0100ffff);p32(vertexAnimation,vaEntry+4,0x0200007b);p32(vertexAnimation,vaEntry+8,1);p32(vertexAnimation,vaEntry+12,1);p32(vertexAnimation,vaEntry+16,20);p32(vertexAnimation,vaEntry+20,vaEntrySize);
 p32(vertexAnimation,vaFrame+8,0x0200007b);p32(vertexAnimation,vaFrame+16,1);p32(vertexAnimation,vaFrame+20,0);p32(vertexAnimation,vaFrame+24,28);
 // Bytes are reversed within the 32-bit temporal word by the original format decoder.
 p32(vertexAnimation,vaFrame+28,0x03020100);
 assert(a.loadBytes(vertexAnimation,e));a.decodeAnimationModels(true);const auto&va=a.chunks()[0].entries[0].animation;
 assert(va.decoded&&va.frames[0].verticesDecoded&&va.frames[0].vertices.size()==1);
 std::cout<<"NSF raw, 64 KiB chunk, entry-table, and truncation checks passed\\n";}
