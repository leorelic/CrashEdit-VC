#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace CrashEdit {
bool saveFilePair(const std::filesystem::path& firstPath,
                  const std::vector<std::uint8_t>& firstBytes,
                  const std::filesystem::path& secondPath,
                  const std::vector<std::uint8_t>& secondBytes,
                  bool createBackups,
                  const std::string& dateStamp,
                  std::string& error);
}
