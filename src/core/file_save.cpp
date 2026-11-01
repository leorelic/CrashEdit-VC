#include "core/file_save.hpp"
#include <cstdint>
#include <fstream>
#include <system_error>

namespace CrashEdit {
namespace {
bool writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, std::string& error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) { error = "Could not create file: " + path.string(); return false; }
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    if (!stream) { error = "Could not write complete file: " + path.string(); return false; }
    return true;
}
bool replaceFile(const std::filesystem::path& temporary, const std::filesystem::path& target, std::string& error) {
    const auto displaced = std::filesystem::path(target.string() + ".replace.old.tmp");
    std::error_code ec;
    std::filesystem::remove(displaced, ec);
    ec.clear();
    std::filesystem::rename(target, displaced, ec);
    if (ec) { error = "Could not stage original file " + target.string() + ": " + ec.message(); return false; }
    std::filesystem::rename(temporary, target, ec);
    if (ec) {
        const std::string replaceError = ec.message();
        std::error_code restoreError;
        std::filesystem::rename(displaced, target, restoreError);
        error = "Could not replace file " + target.string() + ": " + replaceError;
        if (restoreError) error += "; restoring original also failed: " + restoreError.message();
        return false;
    }
    std::filesystem::remove(displaced, ec);
    return true;
}
std::filesystem::path datedPath(const std::filesystem::path& path, const std::string& date) {
    return path.parent_path() / (path.stem().string() + "(" + date + ")" + path.extension().string());
}
}
bool saveFilePair(const std::filesystem::path& firstPath,
                  const std::vector<std::uint8_t>& firstBytes,
                  const std::filesystem::path& secondPath,
                  const std::vector<std::uint8_t>& secondBytes,
                  bool createBackups,
                  const std::string& dateStamp,
                  std::string& error) {
    if (firstPath.empty() || secondPath.empty() || firstPath == secondPath) {
        error = "Save paths are empty or refer to the same file."; return false;
    }
    const auto firstTemp = std::filesystem::path(firstPath.string() + ".tmp");
    const auto secondTemp = std::filesystem::path(secondPath.string() + ".tmp");
    std::vector<std::uint8_t> oldFirst, oldSecond;
    auto readOriginal = [&](const std::filesystem::path& path, std::vector<std::uint8_t>& bytes) {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream) { error = "Could not open original file for safe saving: " + path.string(); return false; }
        const auto end = stream.tellg();
        if (end < 0) { error = "Could not determine original file size: " + path.string(); return false; }
        bytes.resize(static_cast<std::size_t>(end));
        stream.seekg(0);
        if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
            error = "Could not read original file completely: " + path.string(); return false;
        }
        return true;
    };
    if (!readOriginal(firstPath, oldFirst) || !readOriginal(secondPath, oldSecond)) return false;
    std::error_code ec;
    std::filesystem::remove(firstTemp, ec); ec.clear();
    std::filesystem::remove(secondTemp, ec); ec.clear();
    if (!writeFile(firstTemp, firstBytes, error)) return false;
    if (!writeFile(secondTemp, secondBytes, error)) { std::filesystem::remove(firstTemp, ec); return false; }
    if (createBackups) {
        if (dateStamp.size() != 10 || dateStamp[4] != '-' || dateStamp[7] != '-') {
            error = "Date stamp must use YYYY-MM-DD."; std::filesystem::remove(firstTemp, ec); std::filesystem::remove(secondTemp, ec); return false;
        }
        const auto firstBackup = std::filesystem::path(firstPath.string() + ".bak");
        const auto secondBackup = std::filesystem::path(secondPath.string() + ".bak");
        std::filesystem::copy_file(firstPath, firstBackup, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not create backup " + firstBackup.string() + ": " + ec.message(); std::filesystem::remove(firstTemp, ec); std::filesystem::remove(secondTemp, ec); return false; }
        std::filesystem::copy_file(secondPath, secondBackup, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not create backup " + secondBackup.string() + ": " + ec.message(); std::filesystem::remove(firstTemp, ec); std::filesystem::remove(secondTemp, ec); return false; }
        if (!writeFile(datedPath(firstPath, dateStamp), firstBytes, error) ||
            !writeFile(datedPath(secondPath, dateStamp), secondBytes, error)) {
            std::filesystem::remove(firstTemp, ec); std::filesystem::remove(secondTemp, ec); return false;
        }
    }
    if (!replaceFile(firstTemp, firstPath, error)) {
        std::filesystem::remove(firstTemp, ec); std::filesystem::remove(secondTemp, ec); return false;
    }
    if (!replaceFile(secondTemp, secondPath, error)) {
        std::string restoreError;
        const auto restoreTemp = std::filesystem::path(firstPath.string() + ".restore.tmp");
        if (writeFile(restoreTemp, oldFirst, restoreError)) replaceFile(restoreTemp, firstPath, restoreError);
        std::filesystem::remove(secondTemp, ec);
        error += restoreError.empty() ? " First file was restored from its original contents." : " Rollback also failed: " + restoreError;
        return false;
    }
    error.clear();
    return true;
}
}
