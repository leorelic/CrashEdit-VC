#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/eid.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
static void p16(std::vector<std::uint8_t>&b,std::size_t p,std::uint16_t v){b[p]=std::uint8_t(v);b[p+1]=std::uint8_t(v>>8);}
static void p32(std::vector<std::uint8_t>&b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b[p+i]=std::uint8_t(v>>(8*i));}
int main(){using namespace CrashEdit::Crash;assert(eidToEName(0x0200007BU)=="1000Z");assert(eNameToEid("1000Z")==0x0200007BU);assert(isValidEid(0x0200007BU));assert(eidToEName(0x6396347FU)=="NONE!");std::vector<std::uint8_t>b(65536);p16(b,0,0x1234);p16(b,2,0x42);p32(b,4,0x12345678);NsfArchive a;std::string e;assert(a.loadBytes(b,e));assert(a.chunks().size()==1);assert(a.chunks()[0].type==0x42);assert(a.chunks()[0].chunkId==0x12345678);std::vector<std::uint8_t>bad={0x34,0x12,0};assert(!a.loadBytes(bad,e));// Normal entry chunk: one entry containing two items. The item offset table has three offsets.
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
 constexpr std::size_t modelEntry=24,modelItemCount=6,modelTable=16+(modelItemCount+1)*4;
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
 constexpr std::size_t animationEntry=24,animationItemCount=1,animationTable=16+(animationItemCount+1)*4,animationFrameSize=24;
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
 constexpr std::size_t triEntry=24,triItems=5,triTable=16+(triItems+1)*4;const std::size_t triSizes[]={0x50,12,52,12,4};std::size_t triSize=triTable;for(auto n:triSizes)triSize+=n;
 p32(triangleChunk,16,triEntry);p32(triangleChunk,20,triEntry+triSize);p32(triangleChunk,triEntry,0x0100ffff);p32(triangleChunk,triEntry+4,0x0200007b);p32(triangleChunk,triEntry+8,2);p32(triangleChunk,triEntry+12,triItems);
 std::size_t triOffset=triTable;for(std::size_t i=0;i<triItems;++i){p32(triangleChunk,triEntry+16+i*4,triOffset);triOffset+=triSizes[i];}p32(triangleChunk,triEntry+16+triItems*4,static_cast<std::uint32_t>(triSize));
 const std::size_t triInfo=triEntry+triTable,triPoly=triInfo+0x50,triColors=triPoly+12,triTextures=triColors+52;
 p32(triangleChunk,triInfo+0x2c,3);p32(triangleChunk,triInfo+0x34,1);p32(triangleChunk,triInfo+0x38,3);p32(triangleChunk,triInfo+0x3c,13);p32(triangleChunk,triInfo+0x40,1);p32(triangleChunk,triInfo+0x44,1);p32(triangleChunk,triInfo+0x48,1);p32(triangleChunk,triInfo+0x0c,0x4cd8206fU);
 for(std::uint32_t i=0;i<3;++i)p32(triangleChunk,triPoly+i*4,0x80000000U|((i+1U)<<16U)|((10U+i)<<9U)|1U);
 for(std::uint32_t i=0;i<13;++i){triangleChunk[triColors+i*4]=static_cast<std::uint8_t>(i);triangleChunk[triColors+i*4+1]=static_cast<std::uint8_t>(i+1);triangleChunk[triColors+i*4+2]=static_cast<std::uint8_t>(i+2);}triangleChunk[triTextures+6]=0x10U;triangleChunk[triTextures+7]=0x02U;
 assert(a.loadBytes(triangleChunk,e));a.decodeAnimationModels(false);const auto&triModel=a.chunks()[0].entries[0].model;assert(triModel.decoded&&triModel.polygonsDecoded&&triModel.triangles.size()==1);
 assert(triModel.triangles[0].a==0&&triModel.triangles[0].b==1&&triModel.triangles[0].c==2);
 assert(triModel.triangles[0].colorA==10&&triModel.triangles[0].colorB==11&&triModel.triangles[0].colorC==12);
 assert(triModel.extendedTextures.size()==1&&triModel.texturePageEids.size()==1);
 NsfSceneryColor editedColor{210,120,35,7};assert(a.setModelColor(0,0,10,editedColor));
 NsfModelTexture editedTexture{};editedTexture.u1=8;editedTexture.v1=9;editedTexture.u2=10;editedTexture.v2=11;editedTexture.colorMode=1;editedTexture.page=0;
 assert(a.setModelTexture(0,0,0,editedTexture));assert(a.setModelScale(0,0,0,777));assert(a.setModelTexturePageEid(0,0,0,0x4cd8206fU));
 assert(a.setModelExtendedTextureWord(0,0,0,0x00001001U));assert(a.setModelTriangleTexture(0,0,0,0,true));assert(a.setModelTriangleColorIndex(0,0,0,1,12));
 bool textureValid=false;assert(resolveNsfModelTextureIndex(a.chunks()[0].entries[0].model,0,true,0,textureValid)==0&&textureValid);
 NsfModel lodModel;lodModel.textures.resize(4);lodModel.extendedTextureWords.push_back(0x80000001U|(2U<<29U)|(1U<<27U));
 bool lodValid=false;assert(resolveNsfModelTextureIndex(lodModel,0,true,0,lodValid,0)==2&&lodValid);
 lodValid=false;assert(resolveNsfModelTextureIndex(lodModel,0,true,0,lodValid,1)==1&&lodValid);

 std::vector<std::uint8_t> editedNsf;assert(a.saveUncompressedBytes(editedNsf,e));NsfArchive roundTrip;assert(roundTrip.loadBytes(editedNsf,e));roundTrip.decodeAnimationModels(false);
 const auto&roundModel=roundTrip.chunks()[0].entries[0].model;assert(roundModel.scaleX==777);assert(roundModel.colors[10].r==210&&roundModel.colors[10].g==120&&roundModel.colors[10].b==35);
 assert(roundModel.textures[0].u1==8&&roundModel.textures[0].v1==9&&roundModel.textures[0].page==0);assert((roundTrip.chunks()[0].bytes[triTextures+6]&0x10U)!=0&&(roundTrip.chunks()[0].bytes[triTextures+7]&0x02U)!=0);
 assert(roundModel.texturePageEids[0]==0x4cd8206fU&&roundModel.extendedTextureWords[0]==0x00001001U);
 assert(roundModel.triangles.size()==1&&roundModel.triangles[0].animated&&roundModel.triangles[0].textureIndex==0&&roundModel.triangles[0].colorB==12);
 // Crash 3 animation stores the linked Model EID at +8, with a reserved word at +12.
 std::vector<std::uint8_t> c3Animation(65536);p16(c3Animation,0,0x1234);p16(c3Animation,2,0);p32(c3Animation,4,5);p32(c3Animation,8,1);
 constexpr std::size_t c3Entry=24,c3Frame=48,c3EntrySize=56;p32(c3Animation,16,c3Entry);p32(c3Animation,20,c3Entry+c3EntrySize);
 p32(c3Animation,c3Entry,0x0100ffff);p32(c3Animation,c3Entry+4,0x0200007b);p32(c3Animation,c3Entry+8,1);p32(c3Animation,c3Entry+12,1);
 p32(c3Animation,c3Entry+16,24);p32(c3Animation,c3Entry+20,c3EntrySize);
 p16(c3Animation,c3Frame,1);p16(c3Animation,c3Frame+2,2);p16(c3Animation,c3Frame+4,3);p16(c3Animation,c3Frame+6,4);
 p32(c3Animation,c3Frame+8,0x0200007b);p32(c3Animation,c3Frame+12,0);p32(c3Animation,c3Frame+16,0);p32(c3Animation,c3Frame+20,0);p32(c3Animation,c3Frame+24,28);
 assert(a.loadBytes(c3Animation,e));a.decodeAnimationModels(true);const auto&c3=a.chunks()[0].entries[0].animation;assert(c3.decoded&&c3.frames[0].modelEid==0x0200007bU);

 // Uncompressed animation frame expands its 24-bit vertex from the temporal stream.
 std::vector<std::uint8_t> vertexAnimation(65536);p16(vertexAnimation,0,0x1234);p16(vertexAnimation,2,0);p32(vertexAnimation,4,5);p32(vertexAnimation,8,1);
 constexpr std::size_t vaEntry=24,vaFrame=48,vaEntrySize=56;p32(vertexAnimation,16,vaEntry);p32(vertexAnimation,20,vaEntry+vaEntrySize);
 p32(vertexAnimation,vaEntry,0x0100ffff);p32(vertexAnimation,vaEntry+4,0x0200007b);p32(vertexAnimation,vaEntry+8,1);p32(vertexAnimation,vaEntry+12,1);p32(vertexAnimation,vaEntry+16,24);p32(vertexAnimation,vaEntry+20,vaEntrySize);
 p32(vertexAnimation,vaFrame+8,0x0200007b);p32(vertexAnimation,vaFrame+16,1);p32(vertexAnimation,vaFrame+20,0);p32(vertexAnimation,vaFrame+24,28);
 // Bytes are reversed within the 32-bit temporal word by the original format decoder.
 p32(vertexAnimation,vaFrame+28,0x03020100);
 assert(a.loadBytes(vertexAnimation,e));a.decodeAnimationModels(true);const auto&va=a.chunks()[0].entries[0].animation;
 assert(va.decoded&&va.frames[0].verticesDecoded&&va.frames[0].vertices.size()==1);
 assert(va.frames[0].vertices[0].x==0&&va.frames[0].vertices[0].y==8&&va.frames[0].vertices[0].z==16);
 assert(a.setAnimationFrameVertex(0,0,0,0,1,80));assert(a.chunks()[0].entries[0].animation.frames[0].vertices[0].y==80);
 const std::vector<std::size_t> batchVertexIndices{0};
 const std::vector<NsfAnimationVertex> unrestrictedBatchTargets{{0,300,16}};
 assert(a.setAnimationFrameVertices(0,0,0,batchVertexIndices,unrestrictedBatchTargets));
 assert(a.chunks()[0].entries[0].animation.frames[0].vertices[0].y==300);
 const std::vector<NsfAnimationVertex> validBatchTargets{{0,88,16}};
 assert(a.setAnimationFrameVertices(0,0,0,batchVertexIndices,validBatchTargets));
 assert(a.chunks()[0].entries[0].animation.frames[0].vertices[0].y==88);
 const std::vector<std::size_t> duplicateVertexIndices{0,0};
 const std::vector<NsfAnimationVertex> duplicateVertexTargets{{0,96,16},{0,104,16}};
 assert(!a.setAnimationFrameVertices(0,0,0,duplicateVertexIndices,duplicateVertexTargets));
 assert(a.chunks()[0].entries[0].animation.frames[0].vertices[0].y==88);
 assert(a.setAnimationFrameVerticesRebased(0,0,0,batchVertexIndices,validBatchTargets,12,-8,4));
 assert(a.chunks()[0].entries[0].animation.frames[0].xOffset==12);
 assert(a.chunks()[0].entries[0].animation.frames[0].yOffset==-8);
 assert(a.chunks()[0].entries[0].animation.frames[0].zOffset==4);
 assert(!a.setAnimationFrameVerticesRebased(0,0,0,batchVertexIndices,validBatchTargets,40000,-8,4));
 assert(a.chunks()[0].entries[0].animation.frames[0].xOffset==12);
 std::vector<std::uint8_t> editedVertexNsf;assert(a.saveUncompressedBytes(editedVertexNsf,e));NsfArchive vertexRoundTrip;assert(vertexRoundTrip.loadBytes(editedVertexNsf,e));vertexRoundTrip.decodeAnimationModels(true);
 assert(vertexRoundTrip.chunks()[0].entries[0].animation.frames[0].vertices[0].y==88);
 assert(vertexRoundTrip.chunks()[0].entries[0].animation.frames[0].xOffset==12);
 assert(vertexRoundTrip.chunks()[0].entries[0].animation.frames[0].yOffset==-8);
 assert(vertexRoundTrip.chunks()[0].entries[0].animation.frames[0].zOffset==4);

 // Regression: uncompressed Crash 2 saves must preserve edits to later
 // vertices. Each component encoder decodes the frame, so the commit routine
 // must restore the full desired pose between component writes.
 std::vector<std::uint8_t> uncompressedModelChunk(65536);p16(uncompressedModelChunk,0,0x1234);p16(uncompressedModelChunk,2,0);p32(uncompressedModelChunk,4,6);p32(uncompressedModelChunk,8,1);
 constexpr std::size_t umEntry=24,umItems=5,umTable=16+(umItems+1)*4,umEntrySize=umTable+0x50;
 p32(uncompressedModelChunk,16,umEntry);p32(uncompressedModelChunk,20,umEntry+umEntrySize);
 p32(uncompressedModelChunk,umEntry,0x0100ffff);p32(uncompressedModelChunk,umEntry+4,0x0200007b);p32(uncompressedModelChunk,umEntry+8,2);p32(uncompressedModelChunk,umEntry+12,umItems);
 p32(uncompressedModelChunk,umEntry+16,umTable);for(std::size_t i=1;i<=umItems;++i)p32(uncompressedModelChunk,umEntry+16+i*4,umTable+0x50);
 p32(uncompressedModelChunk,umEntry+umTable+0x38,3);
 std::vector<std::uint8_t> uncompressedAnimationChunk(65536);p16(uncompressedAnimationChunk,0,0x1234);p16(uncompressedAnimationChunk,2,0);p32(uncompressedAnimationChunk,4,7);p32(uncompressedAnimationChunk,8,1);
 constexpr std::size_t uaEntry=24,uaFrame=48,uaFrameSize=36,uaEntrySize=24+uaFrameSize;
 p32(uncompressedAnimationChunk,16,uaEntry);p32(uncompressedAnimationChunk,20,uaEntry+uaEntrySize);
 p32(uncompressedAnimationChunk,uaEntry,0x0100ffff);p32(uncompressedAnimationChunk,uaEntry+4,0x0200007c);p32(uncompressedAnimationChunk,uaEntry+8,1);p32(uncompressedAnimationChunk,uaEntry+12,1);
 p32(uncompressedAnimationChunk,uaEntry+16,24);p32(uncompressedAnimationChunk,uaEntry+20,uaEntrySize);
 p32(uncompressedAnimationChunk,uaFrame+8,3);p32(uncompressedAnimationChunk,uaFrame+12,0);p32(uncompressedAnimationChunk,uaFrame+16,0x0200007b);p32(uncompressedAnimationChunk,uaFrame+20,24);
 const std::vector<NsfAnimationVertex> uncompressedBasePose{{10,20,30},{40,50,60},{70,80,90}};
 std::vector<std::uint8_t> logicalBits(96U,0U),temporalBits(96U,0U);std::size_t logicalBit=0;
 for(const auto&vertex:uncompressedBasePose)for(const auto value:{vertex.x,vertex.y,vertex.z})
  for(unsigned bit=0;bit<8U;++bit)logicalBits[logicalBit++]=static_cast<std::uint8_t>((static_cast<std::uint32_t>(value)>>(7U-bit))&1U);
 for(std::size_t base=0;base<temporalBits.size();base+=32U)for(std::size_t byte=0;byte<4U;++byte)for(std::size_t bit=0;bit<8U;++bit)
  temporalBits[base+byte*8U+bit]=logicalBits[base+24U-byte*8U+bit];
 for(std::size_t word=0;word<3U;++word){std::uint32_t raw=0;for(std::size_t bit=0;bit<32U;++bit)raw=(raw<<1U)|temporalBits[word*32U+bit];p32(uncompressedAnimationChunk,uaFrame+24U+word*4U,raw);}
 std::vector<std::uint8_t> uncompressedArchiveBytes;uncompressedArchiveBytes.reserve(131072U);
 uncompressedArchiveBytes.insert(uncompressedArchiveBytes.end(),uncompressedModelChunk.begin(),uncompressedModelChunk.end());
 uncompressedArchiveBytes.insert(uncompressedArchiveBytes.end(),uncompressedAnimationChunk.begin(),uncompressedAnimationChunk.end());
 NsfArchive uncompressedEditArchive;assert(uncompressedEditArchive.loadBytes(uncompressedArchiveBytes,e));uncompressedEditArchive.decodeAnimationModels(false);
 assert(uncompressedEditArchive.chunks()[0].entries[0].model.decoded&&!uncompressedEditArchive.chunks()[0].entries[0].model.compressed);
 assert(uncompressedEditArchive.chunks()[1].entries[0].animation.frames[0].vertices==uncompressedBasePose);
 std::vector<NsfAnimationVertex> uncompressedExpected=uncompressedBasePose;
 uncompressedExpected[1].y=81;uncompressedExpected[2].x=95;uncompressedExpected[2].z=101;
 assert(uncompressedEditArchive.setAnimationFrameVertex(1,0,0,1,1,81));
 assert(uncompressedEditArchive.setAnimationFrameVertex(1,0,0,2,0,95));
 assert(uncompressedEditArchive.setAnimationFrameVertex(1,0,0,2,2,101));
 std::vector<std::uint8_t> uncompressedSaved;assert(uncompressedEditArchive.saveUncompressedBytes(uncompressedSaved,e));
 NsfArchive uncompressedReload;assert(uncompressedReload.loadBytes(uncompressedSaved,e));uncompressedReload.decodeAnimationModels(false);
 const auto&uncompressedSavedVertices=uncompressedReload.chunks()[1].entries[0].animation.frames[0].vertices;
 assert(uncompressedSavedVertices.size()==uncompressedExpected.size());
 for(std::size_t vi=0;vi<uncompressedExpected.size();++vi){
  assert(uncompressedSavedVertices[vi].x==uncompressedExpected[vi].x);
  assert(uncompressedSavedVertices[vi].y==uncompressedExpected[vi].y);
  assert(uncompressedSavedVertices[vi].z==uncompressedExpected[vi].z);
 }

 // Compressed Model positions use signed, variable-width X-Z-Y deltas.
 std::vector<std::uint8_t> compressedModelChunk(65536);p16(compressedModelChunk,0,0x1234);p16(compressedModelChunk,2,0);p32(compressedModelChunk,4,6);p32(compressedModelChunk,8,1);
 constexpr std::size_t cmEntry=24,cmItems=6,cmTable=16+(cmItems+1)*4;const std::size_t cmSizes[]={0x50,12,52,12,4,12};std::size_t cmEntrySize=cmTable;for(auto n:cmSizes)cmEntrySize+=n;
 p32(compressedModelChunk,16,cmEntry);p32(compressedModelChunk,20,cmEntry+cmEntrySize);p32(compressedModelChunk,cmEntry,0x0100ffff);p32(compressedModelChunk,cmEntry+4,0x0200007b);p32(compressedModelChunk,cmEntry+8,2);p32(compressedModelChunk,cmEntry+12,cmItems);
 std::size_t cmOffset=cmTable;for(std::size_t i=0;i<cmItems;++i){p32(compressedModelChunk,cmEntry+16+i*4,cmOffset);cmOffset+=cmSizes[i];}p32(compressedModelChunk,cmEntry+16+cmItems*4,static_cast<std::uint32_t>(cmEntrySize));
 const std::size_t cmInfo=cmEntry+cmTable,cmPoly=cmInfo+0x50,cmColors=cmPoly+12,cmTextures=cmColors+52,cmExtended=cmTextures+12,cmPositions=cmExtended+4;
 p32(compressedModelChunk,cmInfo+0x2c,3);p32(compressedModelChunk,cmInfo+0x34,1);p32(compressedModelChunk,cmInfo+0x38,3);p32(compressedModelChunk,cmInfo+0x3c,13);p32(compressedModelChunk,cmInfo+0x40,1);p32(compressedModelChunk,cmInfo+0x44,1);p32(compressedModelChunk,cmInfo+0x48,1);p32(compressedModelChunk,cmInfo+0x0c,0x4cd8206fU);
 for(std::uint32_t i=0;i<3;++i)p32(compressedModelChunk,cmPoly+i*4,0x80000000U|((i+1U)<<16U)|((10U+i)<<9U)|1U);
 for(std::uint32_t i=0;i<13;++i){compressedModelChunk[cmColors+i*4]=static_cast<std::uint8_t>(i);compressedModelChunk[cmColors+i*4+1]=static_cast<std::uint8_t>(i+1);compressedModelChunk[cmColors+i*4+2]=static_cast<std::uint8_t>(i+2);}
 p32(compressedModelChunk,cmExtended,0x00001001U);for(std::uint32_t i=0;i<3;++i)p32(compressedModelChunk,cmPositions+i*4,0x000001ffU);
 std::vector<std::uint8_t> compressedAnimationChunk(65536);p16(compressedAnimationChunk,0,0x1234);p16(compressedAnimationChunk,2,0);p32(compressedAnimationChunk,4,7);p32(compressedAnimationChunk,8,1);
 constexpr std::size_t caEntry=24,caFrame=48,caEntrySize=100;p32(compressedAnimationChunk,16,caEntry);p32(compressedAnimationChunk,20,caEntry+caEntrySize);
 p32(compressedAnimationChunk,caEntry,0x0100ffff);p32(compressedAnimationChunk,caEntry+4,0x0200007b);p32(compressedAnimationChunk,caEntry+8,1);p32(compressedAnimationChunk,caEntry+12,1);p32(compressedAnimationChunk,caEntry+16,24);p32(compressedAnimationChunk,caEntry+20,caEntrySize);
 p32(compressedAnimationChunk,caFrame+8,3);p32(compressedAnimationChunk,caFrame+12,1);p32(compressedAnimationChunk,caFrame+16,0x0200007b);p32(compressedAnimationChunk,caFrame+20,64);for(std::size_t i=0;i<40U;++i)compressedAnimationChunk[caFrame+24U+i]=static_cast<std::uint8_t>(0xa0U+i);
 std::vector<std::uint8_t> compressedArchiveBytes;compressedArchiveBytes.reserve(131072);compressedArchiveBytes.insert(compressedArchiveBytes.end(),compressedModelChunk.begin(),compressedModelChunk.end());compressedArchiveBytes.insert(compressedArchiveBytes.end(),compressedAnimationChunk.begin(),compressedAnimationChunk.end());
 NsfArchive compressedArchive;assert(compressedArchive.loadBytes(compressedArchiveBytes,e));compressedArchive.decodeAnimationModels(false);
 assert(compressedArchive.chunks().size()==2&&compressedArchive.chunks()[0].entries[0].model.compressed);
 assert(compressedArchive.chunks()[1].entries[0].animation.frames[0].verticesDecoded);
 assert(compressedArchive.setAnimationFrameVertex(1,0,0,1,1,10));
 assert(compressedArchive.chunks()[1].entries[0].animation.frames[0].vertices[1].y==10);
 const std::vector<std::size_t> compressedBatchIndices{0,2};
 const std::vector<NsfAnimationVertex> compressedBatchTargets{{20,30,40},{50,60,70}};
 assert(compressedArchive.setAnimationFrameVertices(1,0,0,compressedBatchIndices,compressedBatchTargets));
 const auto&compressedFrames=compressedArchive.chunks()[1].entries[0].animation.frames[0].vertices;
 assert(compressedFrames[0].x==20&&compressedFrames[0].y==30&&compressedFrames[0].z==40);
 assert(compressedFrames[1].x==0&&compressedFrames[1].y==10&&compressedFrames[1].z==0);
 assert(compressedFrames[2].x==50&&compressedFrames[2].y==60&&compressedFrames[2].z==70);
 for(std::size_t i=0;i<40U;++i)assert(compressedArchive.chunks()[1].bytes[caFrame+24U+i]==static_cast<std::uint8_t>(0xa0U+i));
 for(std::size_t i=0;i<12U;++i)assert(compressedArchive.chunks()[1].bytes[caFrame+64U+i]==compressedAnimationChunk[caFrame+64U+i]);
 // Crash 2 frame coordinates use the full unsigned-byte range, including 255.
 assert(compressedArchive.setAnimationFrameVertex(1,0,0,0,0,255));
 assert(compressedArchive.chunks()[1].entries[0].animation.frames[0].vertices[0].x==255);
 std::vector<std::uint8_t> compressedEdited;assert(compressedArchive.saveUncompressedBytes(compressedEdited,e));NsfArchive compressedRoundTrip;assert(compressedRoundTrip.loadBytes(compressedEdited,e));compressedRoundTrip.decodeAnimationModels(false); for(std::size_t i=0;i<40U;++i)assert(compressedRoundTrip.chunks()[1].bytes[caFrame+24U+i]==static_cast<std::uint8_t>(0xa0U+i));
 // A pose that cannot fit the source delta widths remains editable and survives history serialization.
 const std::vector<NsfAnimationVertex> outOfRangePose{{-20,300,40},{50,60,70}};
 assert(compressedArchive.setAnimationFrameVertices(1,0,0,compressedBatchIndices,outOfRangePose));
 std::string encodingError;assert(!compressedArchive.commitAnimationEdits(encodingError));
 std::vector<std::uint8_t> rejectedSave;assert(!compressedArchive.saveUncompressedBytes(rejectedSave,encodingError));assert(rejectedSave.empty());
 assert(compressedArchive.chunks()[1].entries[0].animation.frames[0].vertices[0].x==-20);
 assert(compressedArchive.chunks()[1].entries[0].animation.frames[0].verticesEdited);
 std::vector<std::uint8_t> sourceBytes,poseOverlay;
 assert(compressedArchive.saveUncompressedSourceBytes(sourceBytes,e));
 assert(compressedArchive.saveAnimationEditOverlay(poseOverlay,e));
 NsfArchive overlayRoundTrip;assert(overlayRoundTrip.loadBytes(sourceBytes,e));overlayRoundTrip.decodeAnimationModels(false);
 assert(overlayRoundTrip.loadAnimationEditOverlay(poseOverlay,e));
 const auto&restoredPose=overlayRoundTrip.chunks()[1].entries[0].animation.frames[0].vertices;
 assert(restoredPose[0].x==-20&&restoredPose[0].y==300&&restoredPose[0].z==40);

 const auto&compressedReloaded=compressedRoundTrip.chunks()[1].entries[0].animation.frames[0].vertices;
 assert(compressedReloaded[0].x==255&&compressedReloaded[0].y==30&&compressedReloaded[0].z==40);
 assert(compressedReloaded[1].x==0&&compressedReloaded[1].y==10&&compressedReloaded[1].z==0);
 assert(compressedReloaded[2].x==50&&compressedReloaded[2].y==60&&compressedReloaded[2].z==70);
 for(std::size_t i=0;i<40U;++i)assert(compressedRoundTrip.chunks()[1].bytes[caFrame+24U+i]==static_cast<std::uint8_t>(0xa0U+i));

 // Crash 2 compressed frames keep special vertices in the frame header and
 // decode the model's compressed positions after that special-vertex prefix.
 std::vector<std::uint8_t> specialCompressedModel(65536);p16(specialCompressedModel,0,0x1234);p16(specialCompressedModel,2,0);p32(specialCompressedModel,4,6);p32(specialCompressedModel,8,1);
 constexpr std::size_t scmEntry=24,scmItems=6,scmTable=16+(scmItems+1)*4;const std::size_t scmSizes[]={0x50,4,4,0,0,8};std::size_t scmEntrySize=scmTable;for(auto n:scmSizes)scmEntrySize+=n;
 p32(specialCompressedModel,16,scmEntry);p32(specialCompressedModel,20,scmEntry+scmEntrySize);p32(specialCompressedModel,scmEntry,0x0100ffff);p32(specialCompressedModel,scmEntry+4,0x0200007b);p32(specialCompressedModel,scmEntry+8,2);p32(specialCompressedModel,scmEntry+12,scmItems);
 std::size_t scmOffset=scmTable;for(std::size_t i=0;i<scmItems;++i){p32(specialCompressedModel,scmEntry+16+i*4,static_cast<std::uint32_t>(scmOffset));scmOffset+=scmSizes[i];}p32(specialCompressedModel,scmEntry+16+scmItems*4,static_cast<std::uint32_t>(scmEntrySize));
 const std::size_t scmInfo=scmEntry+scmTable,scmPoly=scmInfo+0x50,scmColor=scmPoly+4,scmPositions=scmColor+4;
 p32(specialCompressedModel,scmInfo+0x2c,1);p32(specialCompressedModel,scmInfo+0x38,3);p32(specialCompressedModel,scmInfo+0x3c,1);p32(specialCompressedModel,scmInfo+0x4c,1);
 p32(specialCompressedModel,scmPoly,0xffffffffU);p32(specialCompressedModel,scmColor,0x00010203U);p32(specialCompressedModel,scmPositions,0x000001ffU);p32(specialCompressedModel,scmPositions+4,0x000001ffU);
 std::vector<std::uint8_t> specialCompressedAnimation(65536);p16(specialCompressedAnimation,0,0x1234);p16(specialCompressedAnimation,2,0);p32(specialCompressedAnimation,4,7);p32(specialCompressedAnimation,8,1);
 constexpr std::size_t scaEntry=24,scaFrame=48,scaEntrySize=60;p32(specialCompressedAnimation,16,scaEntry);p32(specialCompressedAnimation,20,scaEntry+scaEntrySize);
 p32(specialCompressedAnimation,scaEntry,0x0100ffff);p32(specialCompressedAnimation,scaEntry+4,0x0200007b);p32(specialCompressedAnimation,scaEntry+8,1);p32(specialCompressedAnimation,scaEntry+12,1);p32(specialCompressedAnimation,scaEntry+16,24);p32(specialCompressedAnimation,scaEntry+20,scaEntrySize);
 p32(specialCompressedAnimation,scaFrame+8,3);p32(specialCompressedAnimation,scaFrame+12,0);p32(specialCompressedAnimation,scaFrame+16,0x0200007b);p32(specialCompressedAnimation,scaFrame+20,27);
 p32(specialCompressedAnimation,scaFrame+24,0x001e140aU);p32(specialCompressedAnimation,scaFrame+28,0);p32(specialCompressedAnimation,scaFrame+32,0);
 std::vector<std::uint8_t> specialCompressedBytes;specialCompressedBytes.insert(specialCompressedBytes.end(),specialCompressedModel.begin(),specialCompressedModel.end());specialCompressedBytes.insert(specialCompressedBytes.end(),specialCompressedAnimation.begin(),specialCompressedAnimation.end());
 NsfArchive specialCompressedArchive;assert(specialCompressedArchive.loadBytes(specialCompressedBytes,e));specialCompressedArchive.decodeAnimationModels(false);
 const auto&specialFrame=specialCompressedArchive.chunks()[1].entries[0].animation.frames[0];
 assert(specialFrame.verticesDecoded&&specialFrame.specialVertexCount==1&&specialFrame.vertices.size()==3);
 // CrashEdit's Crash 2 reference decoder starts compressed decoding after
 // the 24-bit special-vertex prefix, then writes compressed positions from
 // vertex index 0 (overwriting the initially read special vertex). In this
 // fixture the remaining temporal bits encode X=10, Z=0, Y=0.
 assert(specialFrame.vertices[0].x==10&&specialFrame.vertices[0].y==0&&specialFrame.vertices[0].z==0);
 assert(specialFrame.vertices[1].x==0&&specialFrame.vertices[1].y==0&&specialFrame.vertices[1].z==0);
 const std::vector<std::size_t> specialEditIndices{0,1};
 const std::vector<NsfAnimationVertex> specialEditTargets{{11,21,31},{44,55,66}};
 assert(specialCompressedArchive.setAnimationFrameVertices(1,0,0,specialEditIndices,specialEditTargets));
 std::vector<std::uint8_t> specialSaved;if(!specialCompressedArchive.saveUncompressedBytes(specialSaved,e)){std::fprintf(stderr,"special compressed save failed: %s\\n",e.c_str());std::abort();}
 NsfArchive specialReload;assert(specialReload.loadBytes(specialSaved,e));specialReload.decodeAnimationModels(false);
 const auto&specialReloaded=specialReload.chunks()[1].entries[0].animation.frames[0].vertices;
 assert(specialReloaded[0].x==11&&specialReloaded[0].y==21&&specialReloaded[0].z==31);
 assert(specialReloaded[1].x==44&&specialReloaded[1].y==55&&specialReloaded[1].z==66);

 // Save-time rebasing preserves the displayed pose while bringing byte coordinates into range.
 std::vector<std::uint8_t> crash2RebaseAnimation=vertexAnimation;
 p32(crash2RebaseAnimation,vaFrame+8,1);p32(crash2RebaseAnimation,vaFrame+12,0);p32(crash2RebaseAnimation,vaFrame+16,0);p32(crash2RebaseAnimation,vaFrame+20,24);p32(crash2RebaseAnimation,vaFrame+24,0x03020100);
 NsfArchive rebaseArchive;assert(rebaseArchive.loadBytes(crash2RebaseAnimation,e));rebaseArchive.decodeAnimationModels(false);
 assert(rebaseArchive.setAnimationFrameVertex(0,0,0,0,0,-20));
 std::vector<std::uint8_t> rebasedBytes;assert(rebaseArchive.saveUncompressedBytes(rebasedBytes,e));
 NsfArchive rebasedRoundTrip;assert(rebasedRoundTrip.loadBytes(rebasedBytes,e));rebasedRoundTrip.decodeAnimationModels(false);
 assert(rebasedRoundTrip.chunks()[0].entries[0].animation.frames[0].vertices[0].x==0);
 assert(rebasedRoundTrip.chunks()[0].entries[0].animation.frames[0].xOffset==-80);

 // Widening one model position's delta width expands the shared table and re-encodes linked frames.
 std::vector<std::uint8_t> narrowModelChunk=compressedModelChunk;p32(narrowModelChunk,cmPositions,0x0000007fU);
 std::vector<std::uint8_t> narrowArchiveBytes;narrowArchiveBytes.reserve(131072);narrowArchiveBytes.insert(narrowArchiveBytes.end(),narrowModelChunk.begin(),narrowModelChunk.end());narrowArchiveBytes.insert(narrowArchiveBytes.end(),compressedAnimationChunk.begin(),compressedAnimationChunk.end());
 NsfArchive narrowArchive;assert(narrowArchive.loadBytes(narrowArchiveBytes,e));narrowArchive.decodeAnimationModels(false);
 assert(narrowArchive.chunks()[0].entries[0].model.positions[0].xBits==1U);
 assert(narrowArchive.setAnimationFrameVertex(1,0,0,0,0,200));
 std::vector<std::uint8_t> expandedBytes;assert(narrowArchive.saveUncompressedBytes(expandedBytes,e));
 NsfArchive expandedRoundTrip;assert(expandedRoundTrip.loadBytes(expandedBytes,e));expandedRoundTrip.decodeAnimationModels(false);
 assert(expandedRoundTrip.chunks()[0].entries[0].model.positions[0].xBits==7U);
 assert(expandedRoundTrip.chunks()[1].entries[0].animation.frames[0].vertices[0].x==200);

 // Animation timeline frame operations duplicate the exact raw frame payload,
 // update entry/item offsets, and survive NSF save/reload.
 std::vector<std::uint8_t> timelineChunk(65536);p16(timelineChunk,0,0x1234);p16(timelineChunk,2,0);p32(timelineChunk,4,8);p32(timelineChunk,8,2);
 constexpr std::size_t tlEntry=28,tlHeader=28,tlFrameSize=24,tlEntrySize=tlHeader+tlFrameSize*2+4,tlSecondEntry=tlEntry+tlEntrySize,tlSecondEntrySize=28;
 p32(timelineChunk,16,tlEntry);p32(timelineChunk,20,tlSecondEntry);p32(timelineChunk,24,tlSecondEntry+tlSecondEntrySize);
 p32(timelineChunk,tlEntry,0x0100ffff);p32(timelineChunk,tlEntry+4,0x0200007b);p32(timelineChunk,tlEntry+8,1);p32(timelineChunk,tlEntry+12,2);
 p32(timelineChunk,tlEntry+16,tlHeader);p32(timelineChunk,tlEntry+20,tlHeader+tlFrameSize);p32(timelineChunk,tlEntry+24,tlEntrySize-4);
 p16(timelineChunk,tlEntry+tlHeader,11);p32(timelineChunk,tlEntry+tlHeader+20,24);
 p16(timelineChunk,tlEntry+tlHeader+tlFrameSize,22);p32(timelineChunk,tlEntry+tlHeader+tlFrameSize+20,24);
 p32(timelineChunk,tlEntry+tlEntrySize-4,0xaabbccddU);
 p32(timelineChunk,tlSecondEntry,0x0100ffff);p32(timelineChunk,tlSecondEntry+4,0x0200007c);p32(timelineChunk,tlSecondEntry+8,99);p32(timelineChunk,tlSecondEntry+12,1);
 p32(timelineChunk,tlSecondEntry+16,24);p32(timelineChunk,tlSecondEntry+20,28);p32(timelineChunk,tlSecondEntry+24,0xdeadbeefU);
 NsfArchive timelineArchive;assert(timelineArchive.loadBytes(timelineChunk,e));timelineArchive.decodeAnimationModels(false);
 assert(timelineArchive.chunks()[0].entries[0].animation.decoded&&timelineArchive.chunks()[0].entries[0].animation.frames.size()==2);
 assert(timelineArchive.appendAnimationFrame(0,0));
 const auto&timelineFrames=timelineArchive.chunks()[0].entries[0].animation.frames;
 assert(timelineFrames.size()==3&&timelineFrames[2].xOffset==22&&timelineFrames[2].rawSize==tlFrameSize);
 assert(std::equal(timelineArchive.chunks()[0].bytes.begin()+static_cast<std::ptrdiff_t>(timelineFrames[1].rawOffset),timelineArchive.chunks()[0].bytes.begin()+static_cast<std::ptrdiff_t>(timelineFrames[1].rawOffset+tlFrameSize),timelineArchive.chunks()[0].bytes.begin()+static_cast<std::ptrdiff_t>(timelineFrames[2].rawOffset)));
 assert(timelineArchive.insertAnimationFrameAfter(0,0,0));
 assert(timelineFrames.size()==4&&timelineFrames[1].xOffset==11&&timelineFrames[2].xOffset==22&&timelineFrames[3].xOffset==22);
 assert(timelineArchive.deleteAnimationFrame(0,0,2));
 assert(timelineFrames.size()==3&&timelineFrames[0].xOffset==11&&timelineFrames[1].xOffset==11&&timelineFrames[2].xOffset==22);
 assert(!timelineArchive.deleteAnimationFrame(0,0,3));
 std::vector<std::uint8_t> timelineSaved;assert(timelineArchive.saveUncompressedBytes(timelineSaved,e));
 NsfArchive timelineReloaded;assert(timelineReloaded.loadBytes(timelineSaved,e));timelineReloaded.decodeAnimationModels(false);
 const auto&reloadedFrames=timelineReloaded.chunks()[0].entries[0].animation.frames;
 assert(reloadedFrames.size()==3&&reloadedFrames[0].xOffset==11&&reloadedFrames[1].xOffset==11&&reloadedFrames[2].xOffset==22);
 assert(timelineReloaded.chunks()[0].entries.size()==2&&timelineReloaded.chunks()[0].entries[1].valid);
 const auto&followingEntry=timelineReloaded.chunks()[0].entries[1];
 assert(followingEntry.type==99&&followingEntry.items.size()==1&&timelineReloaded.chunks()[0].bytes[followingEntry.items[0].offset]==0xef);
 assert(timelineReloaded.chunks()[0].bytes[followingEntry.offset-4U]==0xdd);
 // A full source chunk must relocate the whole Animation entry before retrying
 // Append. The following opaque entry remains valid and the moved EID is unchanged.
 std::vector<std::uint8_t> expansionChunk(65536);p16(expansionChunk,0,0x1234);p16(expansionChunk,2,0);p32(expansionChunk,4,9);p32(expansionChunk,8,2);
 constexpr std::size_t exAnimationEntry=28,exAnimationHeader=28,exFrameSize=24,exAnimationSize=exAnimationHeader+exFrameSize*2,exFollowingEntry=exAnimationEntry+exAnimationSize;
 constexpr std::size_t exUsedEnd=65520;
 p32(expansionChunk,16,exAnimationEntry);p32(expansionChunk,20,exFollowingEntry);p32(expansionChunk,24,exUsedEnd);
 p32(expansionChunk,exAnimationEntry,0x0100ffff);p32(expansionChunk,exAnimationEntry+4,0x0200007d);p32(expansionChunk,exAnimationEntry+8,1);p32(expansionChunk,exAnimationEntry+12,2);
 p32(expansionChunk,exAnimationEntry+16,exAnimationHeader);p32(expansionChunk,exAnimationEntry+20,exAnimationHeader+exFrameSize);p32(expansionChunk,exAnimationEntry+24,exAnimationSize);
 p16(expansionChunk,exAnimationEntry+exAnimationHeader,31);p32(expansionChunk,exAnimationEntry+exAnimationHeader+20,24);
 p16(expansionChunk,exAnimationEntry+exAnimationHeader+exFrameSize,47);p32(expansionChunk,exAnimationEntry+exAnimationHeader+exFrameSize+20,24);
 const std::size_t exFollowingSize=exUsedEnd-exFollowingEntry;
 p32(expansionChunk,exFollowingEntry,0x0100ffff);p32(expansionChunk,exFollowingEntry+4,0x0200007e);p32(expansionChunk,exFollowingEntry+8,99);p32(expansionChunk,exFollowingEntry+12,1);
 p32(expansionChunk,exFollowingEntry+16,24);p32(expansionChunk,exFollowingEntry+20,static_cast<std::uint32_t>(exFollowingSize));
 expansionChunk[exFollowingEntry+24]=0x5a;std::fill(expansionChunk.begin()+exUsedEnd,expansionChunk.end(),0xa5U);
 NsfArchive expansionArchive;assert(expansionArchive.loadBytes(expansionChunk,e));expansionArchive.decodeAnimationModels(false);
 assert(expansionArchive.animationFrameEditNeedsNewChunk(0,0,1));
 std::vector<std::uint8_t> unchangedBeforeCancel;assert(expansionArchive.saveUncompressedBytes(unchangedBeforeCancel,e));
 assert(expansionArchive.animationFrameEditNeedsNewChunk(0,0,1));
 std::vector<std::uint8_t> unchangedAfterCancel;assert(expansionArchive.saveUncompressedBytes(unchangedAfterCancel,e));assert(unchangedBeforeCancel==unchangedAfterCancel);
 std::size_t relocatedChunk=0;assert(expansionArchive.moveEntryToNewChunk(0,0,relocatedChunk,e));
 assert(relocatedChunk==1&&expansionArchive.chunks().size()==2);
 assert(expansionArchive.chunks()[0].entries.size()==1&&expansionArchive.chunks()[0].entries[0].eid==0x0200007eU);
 assert(expansionArchive.chunks()[1].entries.size()==1&&expansionArchive.chunks()[1].entries[0].eid==0x0200007dU);
 assert(expansionArchive.chunks()[1].entries[0].type==1&&expansionArchive.chunks()[1].entries[0].animation.frames.size()==2);
 assert(expansionArchive.chunks()[0].entries[0].items.size()==1&&expansionArchive.chunks()[0].bytes[expansionArchive.chunks()[0].entries[0].items[0].offset]==0x5a);
 assert(expansionArchive.chunks()[0].bytes[65455]==0xa5U);
 assert(expansionArchive.appendAnimationFrame(relocatedChunk,0));
 assert(expansionArchive.chunks()[1].entries[0].animation.frames.size()==3);
 std::vector<std::uint8_t> expansionSaved;assert(expansionArchive.saveUncompressedBytes(expansionSaved,e));
 NsfArchive expansionReloaded;assert(expansionReloaded.loadBytes(expansionSaved,e));expansionReloaded.decodeAnimationModels(false);
 assert(expansionReloaded.chunks().size()==2&&expansionReloaded.chunks()[1].entries[0].eid==0x0200007dU);
 assert(expansionReloaded.chunks()[1].entries[0].animation.frames.size()==3);
 assert(expansionReloaded.chunks()[0].entries[0].eid==0x0200007eU&&expansionReloaded.chunks()[0].entries[0].type==99);
 assert(expansionReloaded.chunks()[0].entries[0].items[0].size==exFollowingSize-24U);
 
 NsfArchive oneFrameGuard=timelineArchive;
 assert(oneFrameGuard.deleteAnimationFrame(0,0,0));
 assert(oneFrameGuard.deleteAnimationFrame(0,0,0));
 assert(!oneFrameGuard.deleteAnimationFrame(0,0,0));

 std::cout<<"NSF raw, 64 KiB chunk, entry-table, and truncation checks passed\\n";}
