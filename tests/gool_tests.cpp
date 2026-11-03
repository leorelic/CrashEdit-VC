#ifdef NDEBUG
#undef NDEBUG
#endif

#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace CrashEdit::Crash;
static void p16(std::vector<std::uint8_t>&b,std::size_t p,std::uint16_t v){b[p]=static_cast<std::uint8_t>(v);b[p+1]=static_cast<std::uint8_t>(v>>8U);}
static void p32(std::vector<std::uint8_t>&b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b[p+static_cast<std::size_t>(i)]=static_cast<std::uint8_t>(v>>(8U*static_cast<unsigned>(i)));}

static std::vector<std::uint8_t> makeGoolChunk(bool extended,bool alternateReturn=false){
 std::vector<std::uint8_t>b(NsfChunkSize);
 p16(b,0,0x1234);p16(b,2,0);p32(b,4,0x12345678);p32(b,8,1);
 const std::size_t entry=24;
 const std::size_t count=extended?6:3;
 const std::size_t tableSize=16+(count+1)*4;
 const std::size_t sizesExtended[]={24,20,8,4,16,116};
 const std::size_t sizesBasic[]={24,8,8};
 std::size_t offsets[7]{};offsets[0]=tableSize;
 for(std::size_t i=0;i<count;++i)offsets[i+1]=offsets[i]+(extended?sizesExtended[i]:sizesBasic[i]);
 p32(b,16,static_cast<std::uint32_t>(entry));p32(b,20,static_cast<std::uint32_t>(entry+offsets[count]));
 p32(b,entry,0x0100ffff);p32(b,entry+4,0x0200007b);p32(b,entry+8,11);p32(b,entry+12,static_cast<std::uint32_t>(count));
 for(std::size_t i=0;i<=count;++i)p32(b,entry+16+i*4,static_cast<std::uint32_t>(offsets[i]));
 const std::size_t header=entry+offsets[0];
 p32(b,header,0x10203040);p32(b,header+4,0x55667788);p32(b,header+8,extended?1:0);
 p32(b,header+12,0x100);p32(b,header+16,4);p32(b,header+20,2);
 const std::size_t code=entry+offsets[1];if(extended){p32(b,code,0x49000000);p32(b,code+4,0x012A4021);p32(b,code+8,alternateReturn?0x03E0A809U:0x03E00008U);p32(b,code+12,0);p32(b,code+16,0x12000000);}else{p32(b,code,0x12345678);p32(b,code+4,0xABCDEF01);}
 const std::size_t data=entry+offsets[2];p32(b,data,0x11112222);p32(b,data+4,0x33334444);
 if(extended){
  const std::size_t map=entry+offsets[3];p16(b,map,0xffff);p16(b,map+2,3);
  const std::size_t desc=entry+offsets[4];p32(b,desc,0x01020304);p32(b,desc+4,0x05060708);
  p16(b,desc+8,9);p16(b,desc+10,10);p16(b,desc+12,11);p16(b,desc+14,12);
  const std::size_t fg=entry+offsets[5];
  p16(b,fg,1);p16(b,fg+2,2);p32(b,fg+4,0x02001234);p32(b,fg+8,1);
  p16(b,fg+12,2);p16(b,fg+14,1);p32(b,fg+16,0x02005678);
  p16(b,fg+36,3);p16(b,fg+38,1);p32(b,fg+40,0x02009ABC);
  p16(b,fg+64,4);p16(b,fg+66,1);p32(b,fg+68,0x0200DEF0);p32(b,fg+72,0x02001111);b[fg+76]='x';b[fg+77]=0;
  p16(b,fg+80,5);p16(b,fg+82,1);p32(b,fg+84,0x02002222);p32(b,fg+88,1);
 }
 return b;
}

int main(){
 const auto& add=goolOpcodeInfo(NsfGoolVersion::Version2,0);
 assert(add.known&&add.mnemonic=="ADD");
 const auto& setf=goolOpcodeInfo(NsfGoolVersion::Version2,17);
 assert(setf.known&&setf.mnemonic=="SETF");
 const auto& branch=goolOpcodeInfo(NsfGoolVersion::Version2,50);
 assert(branch.known&&branch.mnemonic=="BRA");
 const auto& mips=goolOpcodeInfo(NsfGoolVersion::Version2,73);
 assert(mips.known&&mips.mnemonic=="MIPS");
 const auto& reserved=goolOpcodeInfo(NsfGoolVersion::Version2,72);
 assert(!reserved.known&&reserved.mnemonic=="UNK");
 std::string mipsName,mipsArgs;bool mipsKnown=false;
 decodeMipsInstruction(0x012A4021U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="addu"&&mipsArgs=="$t0,$t1,$t2");
 decodeMipsInstruction(0x8C880010U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="lw"&&mipsArgs=="$t0,16($a0)");
 decodeMipsInstruction(0xC8890010U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="lwc2"&&mipsArgs=="IR1,16($a0)");
 decodeMipsInstruction(0xE8AAFFFCU,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="swc2"&&mipsArgs=="IR2,-4($a1)");
 decodeMipsInstruction(0x49000004U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="bc2f"&&mipsArgs=="4");
 decodeMipsInstruction(0x49010004U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="bc2t"&&mipsArgs=="4");
 decodeMipsInstruction(0x49020004U,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x49020004");
 decodeMipsInstruction(0x03E0F809U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="jalr"&&mipsArgs=="$ra,$ra");
 decodeMipsInstruction(0x03E0A809U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="jalr"&&mipsArgs=="$s5,$ra");
 decodeMipsInstruction(0x01200011U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mthi"&&mipsArgs=="$t1");
 decodeMipsInstruction(0x01200013U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mtlo"&&mipsArgs=="$t1");
 decodeMipsInstruction(0x40086000U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mfc0"&&mipsArgs=="$t0,Status");
 decodeMipsInstruction(0x40886000U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mtc0"&&mipsArgs=="$t0,Status");
 decodeMipsInstruction(0x40084000U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mfc0"&&mipsArgs=="$t0,BadVaddr");
 decodeMipsInstruction(0x42000010U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="rfe"&&mipsArgs.empty());
 decodeMipsInstruction(0x42000001U,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x42000001");
 // COP0 transfer and RFE encodings reserve their low/unused fields.
 decodeMipsInstruction(0x40084001U,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x40084001");
 decodeMipsInstruction(0x42000810U,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x42000810");
 // COP2 register transfers likewise require zero in the low 11 bits.
 decodeMipsInstruction(0x48084801U,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x48084801");
 decodeMipsInstruction(0x48084800U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mfc2"&&mipsArgs=="$t0,IR1");
 decodeMipsInstruction(0x4849F800U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="cfc2"&&mipsArgs=="$t1,FLAG");
 decodeMipsInstruction(0x488A5000U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mtc2"&&mipsArgs=="$t2,IR2");
 decodeMipsInstruction(0x48CBF800U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="ctc2"&&mipsArgs=="$t3,FLAG");
 decodeMipsInstruction(0x48E04800U,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x48E04800");
 decodeMipsInstruction(0x4A180001U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="rtps"&&mipsArgs=="sf=1,lm=0");
 decodeMipsInstruction(0x4BBE0BE0U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="nct"&&mipsArgs=="sf=1,lm=0");
 decodeMipsInstruction(0x4A000012U,mipsName,mipsArgs,mipsKnown);
 assert(mipsKnown&&mipsName=="mvmva"&&mipsArgs=="sf=0,lm=0,mx=rotation,v=V0,cv=TR");
 decodeMipsInstruction(0x49BE0BE0U,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x49BE0BE0");
 decodeMipsInstruction(0x4A00003CU,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0x4A00003C");
 decodeMipsInstruction(0xFFFFFFFFU,mipsName,mipsArgs,mipsKnown);
 assert(!mipsKnown&&mipsName=="???"&&mipsArgs=="0xFFFFFFFF");
 const auto& invalid=goolOpcodeInfo(NsfGoolVersion::Version2,250);
 assert(!invalid.known&&invalid.mnemonic=="UNK");
 const auto& unsupported=goolOpcodeInfo(NsfGoolVersion::Version1,0);
 assert(!unsupported.known&&unsupported.mnemonic=="UNSUPPORTED");
 const auto addFields=decodeGoolOperands(NsfGoolVersion::Version2,0,0x345678U);
 assert(addFields.size()==2);
 assert(addFields[0].name=='L'&&addFields[0].bitWidth==12&&addFields[0].rawValue==0x678U);
 assert(addFields[1].name=='R'&&addFields[1].bitWidth==12&&addFields[1].rawValue==0x345U);
 assert(addFields[0].kind==NsfGoolOperandKind::Reference);
 const auto setfFields=decodeGoolOperands(NsfGoolVersion::Version2,17,0xABC123U);
 assert(setfFields.size()==2);
 assert(setfFields[0].name=='D'&&setfFields[0].kind==NsfGoolOperandKind::DestinationReference);
 assert(setfFields[0].rawValue==0x123U&&setfFields[1].rawValue==0xABCU);
 const auto branchFields=decodeGoolOperands(NsfGoolVersion::Version2,50,0x3FFU);
 assert(branchFields.size()==3);
 assert(branchFields[0].name=='I'&&branchFields[0].kind==NsfGoolOperandKind::Signed&&branchFields[0].value==-1);
 assert(branchFields[1].name=='V'&&branchFields[1].bitWidth==4);
 assert(branchFields[2].name=='R'&&branchFields[2].kind==NsfGoolOperandKind::ProcessField);

 assert(goolOpcodeFormat(NsfGoolVersion::Version2,50).find("<IIII")!=std::string_view::npos);
 const auto branchArgs=decodeGoolOperands(NsfGoolVersion::Version2,51,0x3FFU);
 assert(formatGoolControlFlow(51,branchArgs)=="relative -1 instructions, pop 0, if self is true");
 const auto equalBranchArgs=decodeGoolOperands(NsfGoolVersion::Version2,52,0x000000U);
 assert(formatGoolControlFlow(52,equalBranchArgs)=="relative 0 instructions, pop 0, if self is false");
 const auto goArgs=decodeGoolOperands(NsfGoolVersion::Version2,53,0x000123U);
 assert(formatGoolControlFlow(53,goArgs)=="state 291, arguments 0");
 const auto callArgs=decodeGoolOperands(NsfGoolVersion::Version2,59,0x000000U);
 assert(formatGoolControlFlow(59,callArgs)=="subroutine 0 (local), arguments 0");
 const auto externalCallArgs=decodeGoolOperands(NsfGoolVersion::Version2,59,0x000010U);
 assert(formatGoolControlFlow(59,externalCallArgs)=="subroutine 0 (external), arguments 0");
 // Destination references use their own encoding, not the source-pool format.
 NsfGool destinationContext;
 auto destination=decodeGoolOperands(NsfGoolVersion::Version2,17,0x00000000U);
 destination[0].rawValue=5; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="stack[5]");
 destination[0].rawValue=0x07FU; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="arg[0]");
 destination[0].rawValue=0x37FU; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="arg[0]");
 destination[0].rawValue=0x380U; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="stack[0]");
 destination[0].rawValue=0x408U; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="x");
 destination[0].rawValue=0x489U; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="brother->y");
 destination[0].rawValue=0x61FU; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="[sp]");
 destination[0].rawValue=0x640U; resolveGoolOperands(destination,destinationContext);
 assert(destination[0].resolvedText=="mem1");
 assert(formatGoolOperands(destination).find("-> mem1")!=std::string::npos);

 // Source-reference special encodings follow GOOLInstruction.GetRefVal.
 auto sourceRef=decodeGoolOperands(NsfGoolVersion::Version2,0,0U);
 sourceRef[0].rawValue=0x900U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="immediate -256");
 sourceRef[0].rawValue=0xA80U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="immediate -128");
 sourceRef[0].rawValue=0xB05U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="stack[5]");
 sourceRef[0].rawValue=0xB7FU; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="arg[0]");
 sourceRef[0].rawValue=0xBE0U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="[null]");
 sourceRef[0].rawValue=0xBF0U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="[sp2]");
 sourceRef[0].rawValue=0xC08U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="self->x");
 sourceRef[0].rawValue=0xC89U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="brother->y");
 sourceRef[0].rawValue=0xE1FU; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="[sp]");
 sourceRef[0].rawValue=0xE40U; resolveGoolOperands(sourceRef,destinationContext);
 assert(sourceRef[0].resolvedText=="mem1");

 assert(goolOpcodeFormat(NsfGoolVersion::Version1,0).empty());
 // CrashEdit's Crash 2 instruction definitions retain operand layouts even
 // for the explicitly unidentified opcodes 46, 72, and 74-77.
 const auto unknown46=decodeGoolOperands(NsfGoolVersion::Version2,46,0xABC123U);
 assert(unknown46.size()==2);
 assert(unknown46[0].name=='D'&&unknown46[0].kind==NsfGoolOperandKind::DestinationReference);
 assert(unknown46[1].name=='S'&&unknown46[1].kind==NsfGoolOperandKind::Reference);
 for(const std::uint8_t opcode : {std::uint8_t{72},std::uint8_t{74},std::uint8_t{75},std::uint8_t{76},std::uint8_t{77}}){
  const auto& info=goolOpcodeInfo(NsfGoolVersion::Version2,opcode);
  assert(!info.known);
  const auto fields=decodeGoolOperands(NsfGoolVersion::Version2,opcode,0x123456U);
  assert(fields.size()==2);
  assert(fields[0].name=='A'&&fields[0].kind==NsfGoolOperandKind::Reference);
  assert(fields[1].name=='B'&&fields[1].kind==NsfGoolOperandKind::Reference);
 }
 assert(goolOpcodeInfo(NsfGoolVersion::Version2,73).known);
 assert(goolOpcodeInfo(NsfGoolVersion::Version2,73).mnemonic=="MIPS");
 assert(decodeGoolOperands(NsfGoolVersion::Version2,73,0x000000U).empty());
 // Every declared Crash 2 format must account for exactly 24 operand bits.
 for(unsigned opcode=0;opcode<256;++opcode){
  const auto format=goolOpcodeFormat(NsfGoolVersion::Version2,static_cast<std::uint8_t>(opcode));
  if(format.empty())continue;
  unsigned bitCount=0;
  for(char ch:format)if((ch>='A'&&ch<='Z')||ch=='0'||ch=='1'||ch=='-')++bitCount;
  assert(bitCount==24);
 }

 NsfGool refGool;refGool.format=1;refGool.entryCount=1;refGool.dataPool={static_cast<std::int32_t>(0x12345678U),42};
 auto resolved=decodeGoolOperands(NsfGoolVersion::Version2,0,0x00000000U);
 assert(resolved.size()==2);resolveGoolOperands(resolved,refGool);
 assert(resolved[0].resolvedText=="EID 0x12345678");
 assert(resolved[1].resolvedText=="EID 0x12345678");
 auto externalRef=decodeGoolOperands(NsfGoolVersion::Version2,0,0x00000400U);
 resolveGoolOperands(externalRef,refGool);
 assert(externalRef[0].resolvedText.find("pool unavailable")!=std::string::npos);
 NsfGool localGool;localGool.format=0;localGool.entryCount=0;localGool.dataPool={42};
 auto localRef=decodeGoolOperands(NsfGoolVersion::Version2,0,0x00000400U);
 resolveGoolOperands(localRef,localGool);
 assert(localRef[0].resolvedText=="value 0x0000002A (42)");
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
 assert(g.instructions.size()==5);
 assert(g.instructions[0].opcode==73&&g.instructions[0].mnemonic=="MIPS"&&!g.instructions[0].isMips);
 assert(g.instructions[1].isMips&&g.instructions[1].mipsKnown&&g.instructions[1].mnemonic=="addu"&&g.instructions[1].arguments=="$t0,$t1,$t2");
 assert(g.instructions[2].isMips&&g.instructions[2].mnemonic=="jr"&&g.instructions[2].arguments=="$ra");
 assert(g.instructions[3].isMips&&g.instructions[3].mnemonic=="nop");
 assert(!g.instructions[4].isMips&&g.instructions[4].opcode==0x12&&g.instructions[4].mnemonic=="NOT");
 assert(g.dataPool.size()==2&&static_cast<std::uint32_t>(g.dataPool[1])==0x33334444U);
 assert(g.stateMap.size()==2&&g.stateMap[0]==-1&&g.stateMap[1]==3);
 assert(g.stateDescriptors.size()==1&&g.stateDescriptors[0].stateFlags==0x01020304);
 assert(g.stateDescriptors[0].blockFlags==0x05060708&&g.stateDescriptors[0].goolIndex==9);
 assert(g.stateDescriptors[0].eventHook==10&&g.stateDescriptors[0].transHook==11&&g.stateDescriptors[0].codeHook==12);
 assert(g.frameGroupBytes==116);
 assert(g.frameGroupsDecoded&&g.frameGroupError.empty()&&g.frameGroups.size()==5);
 assert(g.frameGroups[0].type==1&&g.frameGroups[0].frameCount==2&&g.frameGroups[0].interpolated);
 assert(g.frameGroups[0].eid==0x02001234U&&g.frameGroups[0].byteOffset==0&&g.frameGroups[0].byteSize==12);
 assert(g.frameGroups[1].type==2&&g.frameGroups[1].byteSize==24);
 assert(g.frameGroups[2].type==3&&g.frameGroups[2].byteSize==28);
 assert(g.frameGroups[3].type==4&&g.frameGroups[3].font==0x02001111&&g.frameGroups[3].byteSize==16);
 assert(g.frameGroups[4].type==5&&g.frameGroups[4].partCount==1&&g.frameGroups[4].byteSize==36);

 // The original parser also treats 0x03E0A809 as a return sentinel,
 // even though the instruction itself disassembles as jalr $s5,$ra.
 assert(archive.loadBytes(makeGoolChunk(true,true),error));
 archive.decodeGools(NsfGoolVersion::Version2);
 const auto&sentinel=archive.chunks()[0].entries[0].gool;
 assert(sentinel.decoded&&sentinel.instructions.size()==5);
 assert(sentinel.instructions[0].opcode==73&&!sentinel.instructions[0].isMips);
 assert(sentinel.instructions[1].isMips&&sentinel.instructions[1].mnemonic=="addu");
 assert(sentinel.instructions[2].isMips&&sentinel.instructions[2].word==0x03E0A809U);
 assert(sentinel.instructions[2].mnemonic=="jalr"&&sentinel.instructions[2].arguments=="$s5,$ra");
 assert(sentinel.instructions[3].isMips&&sentinel.instructions[3].mnemonic=="nop");
 assert(!sentinel.instructions[4].isMips&&sentinel.instructions[4].opcode==0x12);

 assert(archive.loadBytes(makeGoolChunk(false),error));
 archive.decodeGools(NsfGoolVersion::Version2);
 const auto&basic=archive.chunks()[0].entries[0].gool;
 assert(basic.decoded&&basic.format==0&&basic.instructions.size()==2&&basic.dataPool.size()==2);
 std::cout<<"GOOL entry parsing, Crash 2 opcode recognition, and operand decoding checks passed\\n";
}
