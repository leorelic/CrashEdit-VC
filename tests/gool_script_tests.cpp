#ifdef NDEBUG
#undef NDEBUG
#endif

#include "crash/formats/gool/gool_script.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace CrashEdit::Crash;

int main() {
    const auto valid = analyzeGoolScriptIndentation(
        "# illustrative draft\n"
        "event Update:\n"
        "    if health <= 0:\n"
        "        transition Dead\n"
        "    else:\n"
        "        call UpdateMovement\n");
    assert(valid.valid());
    assert(valid.lineCount == 7U);

    const auto unexpected = analyzeGoolScriptIndentation(
        "call Start\n"
        "    call Move\n");
    assert(!unexpected.valid());
    assert(unexpected.diagnostics[0].code == "GS104");
    assert(unexpected.diagnostics[0].line == 2U);

    const auto missingBody = analyzeGoolScriptIndentation(
        "event Update:\n"
        "call Start\n");
    bool foundMissingBody = false;
    for (const auto& diagnostic : missingBody.diagnostics)
        if (diagnostic.code == "GS103") foundMissingBody = true;
    assert(foundMissingBody);

    const auto badDedent = analyzeGoolScriptIndentation(
        "event Update:\n"
        "    if ready:\n"
        "        call Move\n"
        "      return\n");
    bool foundBadDedent = false;
    bool foundWidthError = false;
    for (const auto& diagnostic : badDedent.diagnostics) {
        if (diagnostic.code == "GS105") foundBadDedent = true;
        if (diagnostic.code == "GS102") foundWidthError = true;
    }
    assert(foundBadDedent);
    assert(foundWidthError);

    const auto tabs = analyzeGoolScriptIndentation(
        "event Update:\n"
        "\tcall Move\n");
    bool foundTabError = false;
    for (const auto& diagnostic : tabs.diagnostics)
        if (diagnostic.code == "GS100") foundTabError = true;
    assert(foundTabError);

    const auto mixed = analyzeGoolScriptIndentation(
        "event Update:\n"
        " \tcall Move\n");
    bool foundMixedError = false;
    for (const auto& diagnostic : mixed.diagnostics)
        if (diagnostic.code == "GS101") foundMixedError = true;
    assert(foundMixedError);

    const auto commentAndString = analyzeGoolScriptIndentation(
        "event Update: # block comment\n"
        "    call Print(\"not a block:\")\n");
    assert(commentAndString.valid());

    const auto empty = analyzeGoolScriptIndentation("");
    assert(empty.valid());
    assert(empty.lineCount == 1U);

    const auto validLexical = analyzeGoolScript(
        "event Update:\n"
        "    call Print(\"text with ) and # inside\") # comment [\n");
    assert(validLexical.valid());

    const auto unterminatedString = analyzeGoolScript("call Print(\"unfinished\n");
    bool foundStringError = false;
    for (const auto& diagnostic : unterminatedString.diagnostics)
        if (diagnostic.code == "GS200") foundStringError = true;
    assert(foundStringError);

    const auto mismatchedDelimiter = analyzeGoolScript("call Print(value]\n");
    bool foundMismatch = false;
    for (const auto& diagnostic : mismatchedDelimiter.diagnostics)
        if (diagnostic.code == "GS201") foundMismatch = true;
    assert(foundMismatch);

    const auto unclosedDelimiter = analyzeGoolScript("call Print(value\n");
    bool foundUnclosed = false;
    for (const auto& diagnostic : unclosedDelimiter.diagnostics)
        if (diagnostic.code == "GS202") foundUnclosed = true;
    assert(foundUnclosed);

    std::cout << "GOOL Script indentation tests passed\n";
    return 0;
}
