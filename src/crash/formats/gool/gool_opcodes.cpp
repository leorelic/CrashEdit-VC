#include "crash/formats/gool/gool_opcodes.hpp"
#include <array>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace CrashEdit::Crash {
namespace {
constexpr NsfGoolOpcodeInfo unknown(std::uint8_t opcode) { return {opcode, "UNK", false}; }
constexpr std::array<std::string_view, 256> names = [] {
 std::array<std::string_view,256> n{};
 n[0]="ADD";n[1]="SUB";n[2]="MUL";n[3]="DIV";n[4]="EQL";n[5]="AND";n[6]="OR";n[7]="ANDL";n[8]="ORL";n[9]="LT";n[10]="LTE";n[11]="GT";n[12]="GTE";n[13]="MOD";n[14]="XORL";n[15]="TSTA";n[16]="RAND";n[17]="SETF";n[18]="NOT";n[19]="LOOP";n[20]="LEA";n[21]="ASH";n[22]="PUSH";n[23]="NOTL";n[24]="SETC";n[25]="ABS";n[26]="CPAD";n[27]="VEL";n[28]="MISC";n[29]="PSIN";n[30]="SYNC";n[31]="GVAR";n[32]="GVAW";n[33]="DEGD";n[34]="SEEK";n[35]="COLR";n[36]="COLW";n[37]="DSEK";n[38]="PSHP";n[39]="ANIS";n[40]="EFLR";n[41]="EFLV";n[42]="ARRL";n[43]="SIN";n[44]="COS";n[45]="ATAN";n[46]="UNK46";n[47]="NOP";n[48]="DBG";n[49]="RET";n[50]="BRA";n[51]="BNEZ";n[52]="BEQZ";n[53]="GO";n[54]="GNEZ";n[55]="GEQZ";n[56]="ANIM";n[57]="ANIF";n[58]="VEC";n[59]="CALL";n[60]="SEVT";n[61]="RJEV";n[62]="ACEV";n[63]="SPWN";n[64]="CHNK";n[65]="SNDP";n[66]="SNDA";n[67]="COLL";n[68]="BEVT";n[69]="CEVT";n[70]="SPWF";n[71]="CALR";n[73]="MIPS";n[78]="ARRS";
 return n;
}();
constexpr std::array<std::string_view,256> formats=[] {
 std::array<std::string_view,256> f{};
 constexpr std::string_view lr="[LLLLLLLLLLLL] [RRRRRRRRRRRR]";
 constexpr std::string_view ds="{DDDDDDDDDDDD} [SSSSSSSSSSSS]";
 constexpr std::string_view ab="[AAAAAAAAAAAA] [BBBBBBBBBBBB]";
 for(int i=0;i<=15;++i)f[static_cast<std::size_t>(i)]=lr;
 f[16]="[BBBBBBBBBBBB] [AAAAAAAAAAAA]";
 f[17]=ds;f[18]=ds;f[19]=lr;f[20]=ds;f[21]=lr;f[22]=ab;f[23]=ds;
 f[24]="IIIIIIIIIIIIII E 000 (RRRRRR)";f[25]=ds;f[26]="BBBBBBBBBBBB PP SS DDDD T 000";f[27]=ab;
 f[28]="[XXXXXXXXXXXX] (LLL) SSSSS PPPP";f[29]=ab;f[30]=lr;f[31]="[IIIIIIIIIIII] 000001111101";f[32]="[IIIIIIIIIIII] [SSSSSSSSSSSS]";
 f[33]=lr;f[34]=lr;f[35]="000001111101 LLL IIIIII 000";f[36]="[CCCCCCCCCCCC] LLL IIIIII 000";f[37]=lr;f[38]=ab;f[39]=ds;f[40]=ab;f[41]=ab;f[42]=lr;f[43]=ds;f[44]=ds;f[45]=ab;f[46]=ds;
 f[47]="000001111101000001111101";f[48]=ab;f[49]="0000000000 0000 100110 00 01";
 f[50]="<IIIIIIIIII> VVVV (RRRRRR) 00 00";f[51]="<IIIIIIIIII> VVVV (RRRRRR) 10 00";f[52]="<IIIIIIIIII> VVVV (RRRRRR) 01 00";
 f[53]="SSSSSSSSSS VVVV (RRRRRR) 00 10";f[54]="SSSSSSSSSS VVVV (RRRRRR) 10 10";f[55]="SSSSSSSSSS VVVV (RRRRRR) 01 10";
 f[56]="FFFFFFF SSSSSSSSS TTTTTT HH";f[57]="[FFFFFFFFFFFF] 0000 TTTTTT HH";f[58]="[VVVVVVVVVVVV] AAA BBB TTT (LLL)";f[59]="IIIIIIIIIIIIII E 00000 VVVV";
 f[60]="[EEEEEEEEEEEE] (RRRRRR) AAA (LLL)";f[61]="IIIIIIIIII VVVV (RRRRRR) CC TT";f[62]=f[61];f[63]="CCCCCC SSSSSS TTTTTTTT AAAA";f[64]="[EEEEEEEEEEEE] [TTTTTTTTTTTT]";f[65]=ab;f[66]="[SSSSSSSSSSSS] (RRRRRR) FF TTTT";f[67]="[VVVVVVVVVVVV] AAA BBB TTT (LLL)";f[68]="[EEEEEEEEEEEE] (RRRRRR) AAA LLL";f[69]="[EEEEEEEEEEEE] (RRRRRR) AAA (LLL)";f[70]=f[63];f[71]=ab;f[73]="000001111101000001111101";f[72]=ab;for(int i=74;i<=77;++i)f[static_cast<std::size_t>(i)]=ab;f[78]=lr;
 return f;
}();
const char* gteCommandName(std::uint32_t function) {
 switch(function) {
 case 0x01:return "rtps"; case 0x06:return "nclip"; case 0x0C:return "op";
 case 0x10:return "dpcs"; case 0x11:return "intpl"; case 0x12:return "mvmva";
 case 0x13:return "ncds"; case 0x14:return "cdp"; case 0x16:return "ncdt";
 case 0x1B:return "nccs"; case 0x1C:return "cc"; case 0x1E:return "ncs";
 case 0x20:return "nct"; case 0x28:return "sqr"; case 0x29:return "dcpl";
 case 0x2A:return "dpct"; case 0x2D:return "avsz3"; case 0x2E:return "avsz4";
 case 0x30:return "rtpt"; case 0x3D:return "gpf"; case 0x3E:return "gpl";
 case 0x3F:return "ncct"; default:return nullptr;
 }
}
void decodeGteCommand(std::uint32_t word,std::string& mnemonic,std::string& arguments,bool& known) {
 const auto function=word&0x3FU;
 const char* name=gteCommandName(function);
 if(!name) { known=false; return; }
 mnemonic=name;
 arguments="sf="+std::to_string((word>>19U)&1U)+",lm="+std::to_string((word>>10U)&1U);
 if(function==0x12U) {
  static constexpr const char* matrices[]={"rotation","light","color","reserved"};
  static constexpr const char* vectors[]={"V0","V1","V2","IR"};
  static constexpr const char* translations[]={"TR","BK","FC","none"};
  arguments+=",mx=";arguments+=matrices[(word>>17U)&3U];
  arguments+=",v=";arguments+=vectors[(word>>15U)&3U];
  arguments+=",cv=";arguments+=translations[(word>>13U)&3U];
 }
 known=true;
}
std::int32_t signExtend(std::uint32_t v,unsigned bits){if(bits==0||bits>=32)return static_cast<std::int32_t>(v);const std::uint32_t mask=1U<<(bits-1U);return static_cast<std::int32_t>((v^mask)-mask);}
}
std::string goolObjectFieldName(std::uint32_t field) {
 static constexpr std::array<std::string_view,64> names = {
  "self","parent","brother","child","creator","player","collider","interrupter",
  "x","y","z","rotx","roty","rotz","scax","scay","scaz","velx","vely","velz",
  "trotx","troty","trotz","vecx","vecy","vecz","statusa","statusb","blockflag",
  "subtype","id","sp","pc","fp","tpc","epc","hpc","misc","var","frametime",
  "statetime","stalltime","framegroup","framenum","entity","pathprog","pathlen",
  "groundy","stateflag","speed","displaymode","field_51","groundtime","groundvel",
  "zindex","eventreceived","vfx","yzapproach","density","voice","field_60",
  "field_61","field_62","field_63"
 };
 if(field<names.size()) return std::string(names[field]);
 if(field<=126U) return "mem"+std::to_string(field-63U);
 return "field #"+std::to_string(field);
}

const NsfGoolOpcodeInfo& goolOpcodeInfo(NsfGoolVersion version,std::uint8_t opcode) noexcept {
 static const auto table=[] {std::array<NsfGoolOpcodeInfo,256> t{};for(std::size_t i=0;i<t.size();++i)t[i]=unknown(static_cast<std::uint8_t>(i));for(std::size_t i=0;i<t.size();++i)if(!names[i].empty())t[i]={static_cast<std::uint8_t>(i),names[i],i!=46};return t;}();
 static const NsfGoolOpcodeInfo unsupported{0,"UNSUPPORTED",false};
 if(version!=NsfGoolVersion::Version2)return unsupported;return table[opcode];
}
std::string_view goolOpcodeFormat(NsfGoolVersion version,std::uint8_t opcode) noexcept {
 if(version!=NsfGoolVersion::Version2)return {};return formats[opcode];
}
std::vector<NsfGoolOperand> decodeGoolOperands(NsfGoolVersion version,std::uint8_t opcode,std::uint32_t bits) {
 std::vector<NsfGoolOperand> out;const auto fmt=goolOpcodeFormat(version,opcode);if(fmt.empty())return out;
 char active='\0', opener='\0';unsigned width=0;std::uint32_t value=0;unsigned bit=0;
 auto flush=[&]{if(active!='\0'&&width){NsfGoolOperand a;a.name=active;a.bitWidth=static_cast<std::uint8_t>(width);a.rawValue=value;a.value=opener=='<'?signExtend(value,width):static_cast<std::int32_t>(value);a.kind=opener=='['?NsfGoolOperandKind::Reference:opener=='{'?NsfGoolOperandKind::DestinationReference:opener=='('?NsfGoolOperandKind::ProcessField:opener=='<'?NsfGoolOperandKind::Signed:NsfGoolOperandKind::Value;out.push_back(a);}active='\0';opener='\0';width=0;value=0;};
 for(char c:fmt){if(c==' '||c=='\t')continue;if(bit>=24)break;
  if(c=='['||c=='{'||c=='('||c=='<'){flush();opener=c;continue;}
  if(c==']'||c=='}'||c==')'||c=='>'){flush();continue;}
  if(c=='0'||c=='1'||c=='-'){flush();++bit;continue;}
  if(active!='\0'&&active!=c)flush();
  active=c;value|=((bits>>bit)&1U)<<width;++width;++bit;
 }
 flush();return out;
}

void resolveGoolOperands(std::vector<NsfGoolOperand>& operands, const NsfGool& gool, const std::vector<std::int32_t>* parentData) {
 for(auto& operand:operands) {
  if(operand.kind!=NsfGoolOperandKind::Reference && operand.kind!=NsfGoolOperandKind::DestinationReference) continue;
  const auto raw=operand.rawValue;
  if(operand.kind==NsfGoolOperandKind::DestinationReference) {
   if((raw&0x400U)==0) {
    const auto index=signExtend(raw&0x7FU,7);
    operand.resolvedText=index>=0?"stack["+std::to_string(index)+"]":"arg["+std::to_string(-index-1)+"]";
   } else if((raw&0x200U)==0) {
    const auto link=(raw>>6U)&0x7U;
    const auto field=raw&0x3FU;
    const auto fieldName=goolObjectFieldName(field);
    operand.resolvedText=link==0?fieldName:goolObjectFieldName(link)+"->"+fieldName;
   } else if((raw&0x1FFU)==0x1FU) {
    operand.resolvedText="[sp]";
   } else {
    operand.resolvedText=goolObjectFieldName(raw&0x1FFU);
   }
   continue;
  }
  if((raw&0x800U)==0) {
   const std::size_t offset=raw&0x3FFU;
   const bool external=(raw&0x400U)!=0;
   const std::vector<std::int32_t>* pool=nullptr;
   if(gool.format==1 && !external) pool=&gool.dataPool;
   else if(gool.format==0 && external) pool=&gool.dataPool;
   else if(gool.format==0 && !external && parentData) pool=parentData;
   if(pool) {
    if(offset>=pool->size()) operand.resolvedText="unresolved pool offset "+std::to_string(offset);
    else {
     const auto value=static_cast<std::uint32_t>((*pool)[offset]);
     char text[64];
     if(offset<static_cast<std::size_t>(std::max(0,gool.entryCount)))
      std::snprintf(text,sizeof(text),"EID 0x%08X",value);
     else std::snprintf(text,sizeof(text),"value 0x%08X (%d)",value,static_cast<std::int32_t>(value));
     operand.resolvedText=text;
    }
   } else operand.resolvedText=(external?"external":"local")+std::string(" reference @")+std::to_string(offset)+" (pool unavailable)";
  } else {
   if(operand.kind==NsfGoolOperandKind::DestinationReference) {
    operand.resolvedText="special writable reference (raw 0x"+[] (std::uint32_t v){char b[16];std::snprintf(b,sizeof(b),"%03X",v);return std::string(b);}(raw)+")";
   } else if((raw&0x400U)==0) {
    if((raw&0x200U)==0) {
     operand.resolvedText="immediate "+std::to_string(signExtend(raw&0x1FFU,9));
    } else if((raw&0x100U)==0) {
     operand.resolvedText="immediate "+std::to_string(signExtend(raw&0xFFU,8));
    } else if((raw&0x80U)==0) {
     const auto index=signExtend(raw&0x7FU,7);
     operand.resolvedText=index>=0?"stack["+std::to_string(index)+"]":"arg["+std::to_string(-index-1)+"]";
    } else if(raw==0xBE0U) {
     operand.resolvedText="[null]";
    } else if(raw==0xBF0U) {
     operand.resolvedText="[sp2]";
    } else {
     char text[64];std::snprintf(text,sizeof(text),"reserved special reference (raw 0x%03X)",raw);operand.resolvedText=text;
    }
   } else if((raw&0x200U)==0) {
    const auto link=(raw>>6U)&0x7U;
    const auto field=raw&0x3FU;
    operand.resolvedText=goolObjectFieldName(link)+"->"+goolObjectFieldName(field);
   } else if((raw&0x1FFU)==0x1FU) {
    operand.resolvedText="[sp]";
   } else {
    operand.resolvedText=goolObjectFieldName(raw&0x1FFU);
   }
  }
 }
}
std::string formatGoolControlFlow(std::uint8_t opcode, const std::vector<NsfGoolOperand>& operands, const NsfGool& gool, std::size_t instructionIndex) {
 auto find=[&](char name)->const NsfGoolOperand* { for(const auto& operand:operands) if(operand.name==name) return &operand; return nullptr; };
 auto number=[&](char name)->std::string { const auto* operand=find(name); return operand?std::to_string(operand->value):"?"; };
 auto field=[&]()->std::string { const auto* operand=find('R'); return operand?goolObjectFieldName(operand->rawValue):"unknown"; };
 auto relativeTarget=[&](std::int32_t displacement)->std::string {
  const auto target=static_cast<std::int64_t>(instructionIndex)+1+displacement;
  if(target<0||static_cast<std::uint64_t>(target)>=gool.instructions.size()) return " (target out of range: "+std::to_string(target)+")";
  return " (target instruction "+std::to_string(target)+")";
 };
 if(opcode>=50U&&opcode<=52U) {
  const auto* offset=find('I');
  const auto displacement=offset?offset->value:0;
  std::string result="relative "+number('I')+" instructions"+relativeTarget(displacement);
  result+=", pop "+number('V');
  if(opcode==51U) result+=", if "+field()+" is true";
  else if(opcode==52U) result+=", if "+field()+" is false";
  return result;
 }
 if(opcode>=53U&&opcode<=55U) {
  const auto* state=find('S');
  const auto index=state?state->rawValue:0U;
  std::string result="state "+number('S')+", arguments "+number('V');
  if(index<gool.stateDescriptors.size()) {
   const auto& descriptor=gool.stateDescriptors[index];
   result+=" (descriptor "+std::to_string(index);
   if(descriptor.goolIndex>=0&&static_cast<std::size_t>(descriptor.goolIndex)<gool.dataPool.size()) {
    std::ostringstream eid;
    eid<<"0x"<<std::uppercase<<std::hex<<std::setw(8)<<std::setfill('0')<<static_cast<std::uint32_t>(gool.dataPool[static_cast<std::size_t>(descriptor.goolIndex)]);
    result+=", GOOL "+eid.str();
   } else result+=", invalid GOOL data index "+std::to_string(descriptor.goolIndex);
   result+=")";
  } else result+=" (state descriptor out of range)";
  if(opcode==54U) result+=", if "+field()+" is true";
  else if(opcode==55U) result+=", if "+field()+" is false";
  return result;
 }
 if(opcode==59U) {
  const auto* external=find('E');
  const auto* target=find('I');
  const bool isExternal=external&&external->rawValue!=0;
  std::string result="subroutine "+number('I')+(isExternal?" (external)":" (local)");
  if(!isExternal&&target) { const auto offset=target->value; if(offset>=0&&static_cast<std::uint64_t>(offset)<gool.instructions.size()) result+=" (target instruction "+std::to_string(offset)+")"; else result+=" (target out of range: "+std::to_string(offset)+")"; }
  result+=", arguments "+number('V');
  return result;
 }
 return {};
}

std::string formatGoolOperands(const std::vector<NsfGoolOperand>& operands){
 std::string out;char buf[64];
 for(const auto&a:operands){if(!out.empty())out+=", ";std::snprintf(buf,sizeof(buf),"%c=",a.name);out+=buf;
  if(a.kind==NsfGoolOperandKind::Signed)std::snprintf(buf,sizeof(buf),"%d",a.value);
  else std::snprintf(buf,sizeof(buf),"0x%X",a.rawValue);
  out+=buf;
  switch(a.kind){case NsfGoolOperandKind::Reference:out+=" [ref]";break;case NsfGoolOperandKind::DestinationReference:out+=" [dest]";break;case NsfGoolOperandKind::ProcessField:out+=" [field]";break;default:break;}
  if(!a.resolvedText.empty()) out+=" -> "+a.resolvedText;
 }
 return out;
}

void decodeMipsInstruction(std::uint32_t word,std::string& mnemonic,std::string& arguments,bool& known,
 std::size_t instructionByteOffset,std::size_t instructionStreamSize){
 static constexpr const char* regs[]={"zr","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7","s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","s8","ra"};
 const auto op=(word>>26U)&63U,rs=(word>>21U)&31U,rt=(word>>16U)&31U,rd=(word>>11U)&31U,sh=(word>>6U)&31U,fn=word&63U;
 const auto imm=static_cast<std::int16_t>(word&0xFFFFU);
 auto reg=[](unsigned n){return std::string("$")+regs[n&31U];};
 mnemonic.clear();arguments.clear();known=true;
 auto args=[&](std::string a){arguments=std::move(a);};
 auto branchTargetSuffix=[&]()->std::string {
  if(instructionStreamSize==0U) return {};
  const auto target=static_cast<std::int64_t>(instructionByteOffset)+4+
   static_cast<std::int64_t>(imm)*4;
  std::ostringstream text;
  text<<" -> stream+0x"<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(4)
      <<static_cast<std::uint64_t>(target);
  if(target<0||static_cast<std::uint64_t>(target)>=instructionStreamSize)
   text<<" (outside instruction stream)";
  return text.str();
 };
 auto jumpTargetSuffix=[&]()->std::string {
  if(instructionStreamSize==0U) return {};
  std::ostringstream text;
  text<<" -> index=0x"<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(7)
      <<(word&0x03FFFFFFU)<<" (upper address bits inherited from PC+4)";
  return text.str();
 };
 switch(op){
 case 0:
  switch(fn){
  case 0:mnemonic="sll";args(reg(rd)+","+reg(rt)+","+std::to_string(sh));break;
  case 2:mnemonic="srl";args(reg(rd)+","+reg(rt)+","+std::to_string(sh));break;
  case 3:mnemonic="sra";args(reg(rd)+","+reg(rt)+","+std::to_string(sh));break;
  case 4:mnemonic="sllv";args(reg(rd)+","+reg(rt)+","+reg(rs));break;
  case 6:mnemonic="srlv";args(reg(rd)+","+reg(rt)+","+reg(rs));break;
  case 7:mnemonic="srav";args(reg(rd)+","+reg(rt)+","+reg(rs));break;
  case 8:mnemonic="jr";args(reg(rs));break;
  case 9:mnemonic="jalr";args(reg(rd)+","+reg(rs));break;
  case 12:mnemonic="syscall";break;case 13:mnemonic="break";break;
  case 16:mnemonic="mfhi";args(reg(rd));break;case 17:mnemonic="mthi";args(reg(rs));break;
  case 18:mnemonic="mflo";args(reg(rd));break;case 19:mnemonic="mtlo";args(reg(rs));break;
  case 24:mnemonic="mult";args(reg(rs)+","+reg(rt));break;case 25:mnemonic="multu";args(reg(rs)+","+reg(rt));break;
  case 26:mnemonic="div";args(reg(rs)+","+reg(rt));break;case 27:mnemonic="divu";args(reg(rs)+","+reg(rt));break;
  case 32:mnemonic="add";args(reg(rd)+","+reg(rs)+","+reg(rt));break;case 33:mnemonic="addu";args(reg(rd)+","+reg(rs)+","+reg(rt));break;
  case 34:mnemonic="sub";args(reg(rd)+","+reg(rs)+","+reg(rt));break;case 35:mnemonic="subu";args(reg(rd)+","+reg(rs)+","+reg(rt));break;
  case 36:mnemonic="and";args(reg(rd)+","+reg(rs)+","+reg(rt));break;case 37:mnemonic="or";args(reg(rd)+","+reg(rs)+","+reg(rt));break;
  case 38:mnemonic="xor";args(reg(rd)+","+reg(rs)+","+reg(rt));break;case 39:mnemonic="nor";args(reg(rd)+","+reg(rs)+","+reg(rt));break;
  case 42:mnemonic="slt";args(reg(rd)+","+reg(rs)+","+reg(rt));break;case 43:mnemonic="sltu";args(reg(rd)+","+reg(rs)+","+reg(rt));break;
  default:known=false;break;
  }break;
 case 1:
  if(rt==0)mnemonic="bltz";else if(rt==1)mnemonic="bgez";else if(rt==16)mnemonic="bltzal";else if(rt==17)mnemonic="bgezal";else{known=false;break;}
  args(reg(rs)+","+std::to_string(imm)+branchTargetSuffix());break;
 case 2:case 3:mnemonic=op==2?"j":"jal";args(std::to_string(word&0x03FFFFFFU)+jumpTargetSuffix());break;
 case 4:case 5:mnemonic=op==4?"beq":"bne";args(reg(rs)+","+reg(rt)+","+std::to_string(imm)+branchTargetSuffix());break;
 case 6:case 7:mnemonic=op==6?"blez":"bgtz";args(reg(rs)+","+std::to_string(imm)+branchTargetSuffix());break;
 case 8:case 9:case 10:case 11:case 12:case 13:case 14:
  {static constexpr const char* names[]={"addi","addiu","slti","sltiu","andi","ori","xori"};mnemonic=names[op-8];args(reg(rt)+","+reg(rs)+","+std::to_string((op>=12)?(word&0xFFFFU):static_cast<std::uint32_t>(static_cast<std::int32_t>(imm))));}break;
 case 15:mnemonic="lui";args(reg(rt)+","+std::to_string(word&0xFFFFU));break;
 case 16:
  // R3000A COP0 register transfers (MFC0/MTC0). Keep the PS1 debug
  // registers named where their hardware meanings are known.
  if((rs==0U || rs==4U) && (word&0x7FFU)==0U) {
   static constexpr const char* systemRegisters[]={
    "Index","Random","EntryLo","BPC","BDA","JUMPDEST","DCIC","Reserved7",
    "BadVaddr","BDAM","BPCM","Reserved11","Status","Cause","EPC","PRId",
    "Reserved16","Reserved17","Reserved18","Reserved19","Reserved20","Reserved21","Reserved22","Reserved23",
    "Reserved24","Reserved25","Reserved26","Reserved27","Reserved28","Reserved29","Reserved30","Reserved31"
   };
   mnemonic=rs==0U?"mfc0":"mtc0";
   args(reg(rt)+","+systemRegisters[rd]);
  } else if(rs==16U && (word&0x001FFFC0U)==0U && (word&63U)==0x10U) {
   // RFE is the R3000A's return-from-exception instruction.
   mnemonic="rfe";
  } else {
   known=false;
  }
  break;
 case 18:
  if(rs==8U) {
   if(rt==0U) mnemonic="bc2f";
   else if(rt==1U) mnemonic="bc2t";
   else {known=false;break;}
   args(std::to_string(imm)+branchTargetSuffix());
  } else if((rs&0x10U)!=0) {
   decodeGteCommand(word,mnemonic,arguments,known);
  } else {
   static constexpr const char* dataRegisters[]={
    "VXY0","VZ0","VXY1","VZ1","VXY2","VZ2","RGBC","OTZ",
    "IR0","IR1","IR2","IR3","SXY0","SXY1","SXY2","SXYP",
    "SZ0","SZ1","SZ2","SZ3","RGB0","RGB1","RGB2","RES1",
    "MAC0","MAC1","MAC2","MAC3","IRGB","ORGB","LZCS","LZCR"
   };
   static constexpr const char* controlRegisters[]={
    "RT11RT12","RT13RT21","RT22RT23","RT31RT32","RT33","TRX","TRY","TRZ",
    "L11L12","L13L21","L22L23","L31L32","L33","RBK","GBK","BBK",
    "LR1LR2","LR3LG1","LG2LG3","LB1LB2","LB3","RFC","GFC","BFC",
    "OFX","OFY","H","DQA","DQB","ZSF3","ZSF4","FLAG"
   };
   const char* cop2Register=nullptr;
   if((word&0x7FFU)!=0U) { known=false; break; }
   switch(rs) {
   case 0:mnemonic="mfc2";cop2Register=dataRegisters[rd];break;
   case 2:mnemonic="cfc2";cop2Register=controlRegisters[rd];break;
   case 4:mnemonic="mtc2";cop2Register=dataRegisters[rd];break;
   case 6:mnemonic="ctc2";cop2Register=controlRegisters[rd];break;
   default:known=false;break;
   }
   if(cop2Register) args(reg(rt)+","+cop2Register);
  }
  break;
 case 32:case 33:case 34:case 35:case 36:case 37:case 38:case 40:case 41:case 42:case 43:case 46:case 50:case 58:
  {static constexpr const char* cop2DataRegisters[]={
    "VXY0","VZ0","VXY1","VZ1","VXY2","VZ2","RGBC","OTZ",
    "IR0","IR1","IR2","IR3","SXY0","SXY1","SXY2","SXYP",
    "SZ0","SZ1","SZ2","SZ3","RGB0","RGB1","RGB2","RES1",
    "MAC0","MAC1","MAC2","MAC3","IRGB","ORGB","LZCS","LZCR"
   };static constexpr const char* names[]={"lb","lh","lwl","lw","lbu","lhu","lwr","","sb","sh","swl","sw","swr"};const char* name=nullptr;
   switch(op){case 32:name="lb";break;case 33:name="lh";break;case 34:name="lwl";break;case 35:name="lw";break;case 36:name="lbu";break;case 37:name="lhu";break;case 38:name="lwr";break;case 40:name="sb";break;case 41:name="sh";break;case 42:name="swl";break;case 43:name="sw";break;case 46:name="swr";break;case 50:name="lwc2";break;case 58:name="swc2";break;default:break;}
   if(op==50||op==58)args(std::string(cop2DataRegisters[rt])+","+std::to_string(imm)+"("+reg(rs)+")");
   else args(reg(rt)+","+std::to_string(imm)+"("+reg(rs)+")");mnemonic=name;}break;
 default:known=false;break;
 }
 if(word==0){mnemonic="nop";arguments.clear();known=true;}
 if(!known){mnemonic="???";arguments="0x";char hex[9];std::snprintf(hex,sizeof(hex),"%08X",word);arguments+=hex;}
}

}
