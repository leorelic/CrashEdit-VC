#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace CrashEdit {
enum class DiagnosticLevel { Info, Warning, Error };
struct DiagnosticMessage {
    DiagnosticLevel level = DiagnosticLevel::Info;
    std::string message;
};
class Diagnostics {
public:
    static void log(DiagnosticLevel level, std::string message);
    static const std::vector<DiagnosticMessage>& messages();
    static std::size_t count(DiagnosticLevel level);
    static void clear();
};
}
