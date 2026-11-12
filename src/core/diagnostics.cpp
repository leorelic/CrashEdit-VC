#include "core/diagnostics.hpp"
#include <algorithm>
#include <utility>

namespace CrashEdit {
namespace {
std::vector<DiagnosticMessage> entries;
constexpr std::size_t maxEntries = 1000;
}
void Diagnostics::log(DiagnosticLevel level, std::string message) {
    entries.push_back({level, std::move(message)});
    if (entries.size() > maxEntries) entries.erase(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(entries.size() - maxEntries));
}
const std::vector<DiagnosticMessage>& Diagnostics::messages() { return entries; }
std::size_t Diagnostics::count(DiagnosticLevel level) {
    return static_cast<std::size_t>(std::count_if(entries.begin(), entries.end(), [level](const auto& e) { return e.level == level; }));
}
void Diagnostics::clear() { entries.clear(); }
}
