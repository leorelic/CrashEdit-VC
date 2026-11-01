#include "crash/formats/gool/gool_opcodes.hpp"
#include <array>
#include <cstdio>

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
 f[33]=lr;f[34]=lr;f[35]="000001111101 LLL IIIIII 000";f[36]="[CCCCCCCCCCCC] LLL IIIIII 000";f[37]=lr;f[38]=ab;f[39]=ds;f[40]=ab;f[41]=ab;f[42]=lr;f[43]=ds;f[44]=ds;f[45]=ab;
 f[47]="101111100000101111100000";f[48]=ab;f[49]="0000000000 0000 100110 00 01";
 f[50]="<IIIIIIIIII> VVVV (RRRRRR) 00 00";f[51]="<IIIIIIIIII> VVVV (RRRRRR) 10 00";f[52]="<IIIIIIIIII> VVVV (RRRRRR) 01 00";
 f[53]="SSSSSSSSSS VVVV (RRRRRR) 00 10";f[54]="SSSSSSSSSS VVVV (RRRRRR) 10 10";f[55]="SSSSSSSSSS VVVV (RRRRRR) 01 10";
 f[56]="FFFFFFF SSSSSSSSS TTTTTT HH";f[57]="[FFFFFFFFFFFF] 0000 TTTTTT HH";f[58]="[VVVVVVVVVVVV] AAA BBB TTT (LLL)";f[59]="IIIIIIIIIIIIII E 00000 VVVV";
 f[60]="[EEEEEEEEEEEE] (RRRRRR) AAA (LLL)";f[61]="IIIIIIIIII VVVV (RRRRRR) CC TT";f[62]=f[61];f[63]="CCCCCC SSSSSS TTTTTTTT AAAA";f[64]="[EEEEEEEEEEEE] [TTTTTTTTTTTT]";f[65]=ab;f[66]="[SSSSSSSSSSSS] (RRRRRR) FF TTTT";f[67]="[VVVVVVVVVVVV] AAA BBB TTT (LLL)";f[68]="[EEEEEEEEEEEE] (RRRRRR) AAA LLL";f[69]="[EEEEEEEEEEEE] (RRRRRR) AAA (LLL)";f[70]=f[63];f[71]=ab;f[73]="101111100000101111100000";f[78]=lr;
 return f;
}();
std::int32_t signExtend(std::uint32_t v,unsigned bits){if(bits==0||bits>=32)return static_cast<std::int32_t>(v);const std::uint32_t mask=1U<<(bits-1U);return static_cast<std::int32_t>((v^mask)-mask);}
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
std::string formatGoolOperands(const std::vector<NsfGoolOperand>& operands){
 std::string out;char buf[64];
 for(const auto&a:operands){if(!out.empty())out+=", ";std::snprintf(buf,sizeof(buf),"%c=",a.name);out+=buf;
  if(a.kind==NsfGoolOperandKind::Signed)std::snprintf(buf,sizeof(buf),"%d",a.value);
  else std::snprintf(buf,sizeof(buf),"0x%X",a.rawValue);
  out+=buf;
  switch(a.kind){case NsfGoolOperandKind::Reference:out+=" [ref]";break;case NsfGoolOperandKind::DestinationReference:out+=" [dest]";break;case NsfGoolOperandKind::ProcessField:out+=" [field]";break;default:break;}
 }
 return out;
}
}
