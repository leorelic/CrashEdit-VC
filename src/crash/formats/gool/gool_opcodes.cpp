#include "crash/formats/gool/gool_opcodes.hpp"
#include <array>
#include <cstdio>
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
std::int32_t signExtend(std::uint32_t v,unsigned bits){if(bits==0||bits>=32)return static_cast<std::int32_t>(v);const std::uint32_t mask=1U<<(bits-1U);return static_cast<std::int32_t>((v^mask)-mask);}
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

}
const NsfGoolOpcodeInfo& goolOpcodeInfo(NsfGoolVersion version,std::uint8_t opcode) noexcept {
 static const auto table=[] {std::array<NsfGoolOpcodeInfo,256> t{};for(std::size_t i=0;i<t.size();++i)t[i]=unknown(static_cast<std::uint8_t>(i));for(std::size_t i=0;i<t.size();++i)if(!names[i].empty())t[i]={static_cast<std::uint8_t>(i),names[i],i!=46&&!(i>=72&&i<=77)};return t;}();
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
    const auto index=signExtend(raw,7);
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
   if(operand.kind==NsfGoolOperandKind::DestinationReference) operand.resolvedText="special writable reference (raw 0x"+[] (std::uint32_t v){char b[16];std::snprintf(b,sizeof(b),"%03X",v);return std::string(b);}(raw)+")";
   else operand.resolvedText="special reference (raw 0x"+[] (std::uint32_t v){char b[16];std::snprintf(b,sizeof(b),"%03X",v);return std::string(b);}(raw)+")";
  }
 }
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
}
