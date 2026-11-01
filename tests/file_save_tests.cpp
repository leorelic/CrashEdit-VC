#include "core/file_save.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
bool check(bool value, const char* message) {
    if (!value) std::cerr << "FAIL: " << message << '\n';
    return value;
}
}
int main() {
    const auto root = std::filesystem::temp_directory_path() / "CrashEditSavePairTests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    const auto nsf = root / "S000000E.NSF";
    const auto nsd = root / "S000000E.NSD";
    const std::vector<std::uint8_t> oldNsf{1,2,3}, oldNsd{4,5,6};
    const std::vector<std::uint8_t> newNsf{7,8,9,10}, newNsd{11,12,13};
    bool ok = check(write(nsf, oldNsf) && write(nsd, oldNsd), "write original pair");
    std::string error;
    ok &= check(CrashEdit::saveFilePair(nsf,newNsf,nsd,newNsd,true,"2026-11-01",error), "save pair with backups");
    ok &= check(read(nsf)==newNsf && read(nsd)==newNsd, "replace both original files");
    ok &= check(read(root/"S000000E.NSF.bak")==oldNsf && read(root/"S000000E.NSD.bak")==oldNsd, "preserve pre-save originals");
    ok &= check(read(root/"S000000E(2026-11-01).NSF")==newNsf &&
                read(root/"S000000E(2026-11-01).NSD")==newNsd, "write dated copies of new pair");
    const std::vector<std::uint8_t> newerNsf{21,22}, newerNsd{23,24};
    ok &= check(CrashEdit::saveFilePair(nsf,newerNsf,nsd,newerNsd,true,"2026-11-01",error), "save same-day pair again");
    ok &= check(read(root/"S000000E.NSF.bak")==newNsf && read(root/"S000000E.NSD.bak")==newNsd,
                "refresh .bak files with immediately previous originals");
    ok &= check(read(root/"S000000E(2026-11-01).NSF")==newerNsf &&
                read(root/"S000000E(2026-11-01).NSD")==newerNsd, "replace same-day archive with latest pair");
    std::filesystem::remove_all(root, ec);
    std::cout << (ok ? "All paired-save tests passed.\n" : "Paired-save tests failed.\n");
    return ok ? 0 : 1;
}
