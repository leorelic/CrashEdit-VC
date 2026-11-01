#include "crash/formats/gool/gool_opcodes.hpp"
#include <array>

namespace CrashEdit::Crash {
namespace {
constexpr NsfGoolOpcodeInfo unknown(std::uint8_t opcode) { return {opcode, "UNK", false}; }
constexpr std::array<std::string_view, 256> crash2Names = [] {
    std::array<std::string_view, 256> names{};
    names[0]="ADD"; names[1]="SUB"; names[2]="MUL"; names[3]="DIV";
    names[4]="EQL"; names[5]="AND"; names[6]="OR"; names[7]="ANDL";
    names[8]="ORL"; names[9]="LT"; names[10]="LTE"; names[11]="GT";
    names[12]="GTE"; names[13]="MOD"; names[14]="XORL"; names[15]="TSTA";
    names[16]="RAND"; names[17]="SETF"; names[18]="NOT"; names[19]="LOOP";
    names[20]="LEA"; names[21]="ASH"; names[22]="PUSH"; names[23]="NOTL";
    names[24]="SETC"; names[25]="ABS"; names[26]="CPAD"; names[27]="VEL";
    names[28]="MISC"; names[29]="PSIN"; names[30]="SYNC"; names[31]="GVAR";
    names[32]="GVAW"; names[33]="DEGD"; names[34]="SEEK"; names[35]="COLR";
    names[36]="COLW"; names[37]="DSEK"; names[38]="PSHP"; names[39]="ANIS";
    names[40]="EFLR"; names[41]="EFLV"; names[42]="ARRL"; names[43]="SIN";
    names[44]="COS"; names[45]="ATAN"; names[46]="UNK46"; names[47]="NOP";
    names[48]="DBG"; names[49]="RET"; names[50]="BRA"; names[51]="BNEZ";
    names[52]="BEQZ"; names[53]="GO"; names[54]="GNEZ"; names[55]="GEQZ";
    names[56]="ANIM"; names[57]="ANIF"; names[58]="VEC"; names[59]="CALL";
    names[60]="SEVT"; names[61]="RJEV"; names[62]="ACEV"; names[63]="SPWN";
    names[64]="CHNK"; names[65]="SNDP"; names[66]="SNDA"; names[67]="COLL";
    names[68]="BEVT"; names[69]="CEVT"; names[70]="SPWF"; names[71]="CALR";
    names[73]="MIPS"; names[78]="ARRS";
    return names;
}();
}
const NsfGoolOpcodeInfo& goolOpcodeInfo(NsfGoolVersion version, std::uint8_t opcode) noexcept {
    static const auto crash2 = [] {
        std::array<NsfGoolOpcodeInfo, 256> table{};
        for (std::size_t i=0;i<table.size();++i) table[i]=unknown(static_cast<std::uint8_t>(i));
        for (std::size_t i=0;i<table.size();++i) {
            if (!crash2Names[i].empty()) table[i]={static_cast<std::uint8_t>(i),crash2Names[i],i!=46 && !(i>=72 && i<=77)};
        }
        return table;
    }();
    static const NsfGoolOpcodeInfo unsupported{0, "UNSUPPORTED", false};
    if (version != NsfGoolVersion::Version2) return unsupported;
    return crash2[opcode];
}
}
