#include "ui/gool_script_editor.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace CrashEdit {
namespace {

const TextEditor::Language* goolScriptLanguage() {
    static const TextEditor::Language language = [] {
        TextEditor::Language value;
        value.name = "GOOL Script";
        value.singleLineComment = "#";
        value.hasSingleQuotedStrings = true;
        value.hasDoubleQuotedStrings = true;
        value.stringEscape = '\\';
        value.indentationForBlocks = true;

        value.keywords = {
            "event", "state", "routine", "if", "else", "while", "for", "in",
            "break", "continue", "return", "call", "transition", "true",
            "false", "null", "and", "or", "not", "native", "unknown",
            "let", "var", "const"
        };
        value.identifiers = {
            "health", "position", "velocity", "rotation", "scale", "frame",
            "object_id", "camera_id", "state_id", "eid", "random", "sin",
            "cos", "distance", "timer", "speed", "target", "player"
        };

        value.getIdentifier = [](TextEditor::Iterator start, TextEditor::Iterator end) {
            auto it = start;
            if (it == end || !TextEditor::CodePoint::isXidStart(*it)) return start;
            ++it;
            while (it < end && TextEditor::CodePoint::isXidContinue(*it)) ++it;
            return it;
        };
        value.getNumber = [](TextEditor::Iterator start, TextEditor::Iterator end) {
            auto it = start;
            if (it == end || *it < '0' || *it > '9') return start;
            if (*it == '0') {
                ++it;
                if (it < end && (*it == 'x' || *it == 'X')) {
                    ++it;
                    const auto digits = it;
                    while (it < end && ((*it >= '0' && *it <= '9') ||
                           (*it >= 'a' && *it <= 'f') || (*it >= 'A' && *it <= 'F') ||
                           *it == '_')) ++it;
                    return it == digits ? start : it;
                }
            }
            while (it < end && ((*it >= '0' && *it <= '9') || *it == '_')) ++it;
            if (it < end && *it == '.') {
                ++it;
                while (it < end && *it >= '0' && *it <= '9') ++it;
            }
            if (it < end && (*it == 'e' || *it == 'E')) {
                auto exponent = it++;
                if (it < end && (*it == '+' || *it == '-')) ++it;
                const auto digits = it;
                while (it < end && *it >= '0' && *it <= '9') ++it;
                if (it == digits) it = exponent;
            }
            return it;
        };
        value.isPunctuation = [](ImWchar c) {
            return c < 128 && std::ispunct(static_cast<unsigned char>(c)) != 0;
        };
        return value;
    }();
    return &language;
}

void setPaletteColor(TextEditor::Palette& palette, TextEditor::Color color, ImU32 value) {
    palette[static_cast<std::size_t>(color)] = value;
}

} // namespace

GoolScriptEditor::GoolScriptEditor() {
    editor_.SetLanguage(goolScriptLanguage());

    editor_.SetTextContextMenuCallback([this](TextEditor::PopupData&) {
        const bool hasSelection = editor_.CurrentCursorHasSelection();
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, hasSelection && !readOnly_)) {
            editor_.Cut();
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection)) {
            editor_.Copy();
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !readOnly_)) {
            editor_.Paste();
        }
    });

    auto palette = TextEditor::GetDarkPalette();
    setPaletteColor(palette, TextEditor::Color::text, IM_COL32(222, 226, 234, 255));
    setPaletteColor(palette, TextEditor::Color::keyword, IM_COL32(198, 157, 242, 255));
    setPaletteColor(palette, TextEditor::Color::declaration, IM_COL32(126, 190, 239, 255));
    setPaletteColor(palette, TextEditor::Color::knownIdentifier, IM_COL32(126, 190, 239, 255));
    setPaletteColor(palette, TextEditor::Color::number, IM_COL32(177, 211, 143, 255));
    setPaletteColor(palette, TextEditor::Color::string, IM_COL32(231, 184, 132, 255));
    setPaletteColor(palette, TextEditor::Color::punctuation, IM_COL32(185, 195, 211, 255));
    setPaletteColor(palette, TextEditor::Color::comment, IM_COL32(132, 148, 168, 255));
    setPaletteColor(palette, TextEditor::Color::background, IM_COL32(24, 27, 33, 255));
    setPaletteColor(palette, TextEditor::Color::cursor, IM_COL32(220, 226, 236, 255));
    setPaletteColor(palette, TextEditor::Color::selection, IM_COL32(62, 91, 128, 180));
    setPaletteColor(palette, TextEditor::Color::lineNumber, IM_COL32(103, 116, 135, 255));
    setPaletteColor(palette, TextEditor::Color::currentLineNumber, IM_COL32(188, 199, 215, 255));
    setPaletteColor(palette, TextEditor::Color::currentLineHighlight, IM_COL32(22, 43, 72, 105));
    setPaletteColor(palette, TextEditor::Color::currentLineHighlightBorder, IM_COL32(22, 43, 72, 0));
    editor_.SetPalette(palette);

    editor_.SetShowLineNumbersEnabled(true);
    editor_.SetShowCurrentLineHighlightEnabled(true);
    editor_.SetShowMiniMapEnabled(false);
    editor_.SetShowScrollbarMiniMapEnabled(false);
    editor_.SetLineFoldingEnabled(false);
    editor_.SetAutoIndentEnabled(true);
    editor_.SetInsertSpacesOnTabs(true);
    editor_.SetTabSize(4);
    editor_.SetCompletePairedGlyphs(true);
    editor_.SetShowMatchingBrackets(true);
    editor_.SetWordWrapEnabled(false);
    editor_.SetFindReplaceEnabled(false);
}

void GoolScriptEditor::SetText(const std::string& text) {
    editor_.SetText(text);
}

std::string GoolScriptEditor::GetText() const {
    return editor_.GetText();
}

bool GoolScriptEditor::Render(const char* id, ImVec2 size, bool readOnly) {
    readOnly_ = readOnly;
    editor_.SetReadOnlyEnabled(readOnly);

    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && io.KeyCtrl) {
        const bool zoomIn = ImGui::IsKeyPressed(ImGuiKey_Equal, false) && io.KeyShift;
        const bool zoomOut = ImGui::IsKeyPressed(ImGuiKey_Minus, false);
        const bool keypadIn = ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false);
        const bool keypadOut = ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false);
        if (zoomIn || keypadIn) fontScale_ = std::min(2.0f, fontScale_ + 0.1f);
        else if (zoomOut || keypadOut) fontScale_ = std::max(0.6f, fontScale_ - 0.1f);
    }

    const float baseFontSize = ImGui::GetFontSize();
    ImGui::PushFont(nullptr, baseFontSize * fontScale_);
    const bool changed = editor_.Render(id, size);
    ImGui::PopFont();
    return changed;
}

} // namespace CrashEdit
