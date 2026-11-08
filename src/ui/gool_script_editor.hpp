#pragma once

#include <string>
#include <imgui.h>
#include "TextEditor.h"

namespace CrashEdit {

class GoolScriptEditor {
public:
    GoolScriptEditor();

    void SetText(const std::string& text);
    std::string GetText() const;
    bool Render(const char* id, ImVec2 size, bool readOnly = false);

private:
    TextEditor editor_;
    bool readOnly_ = false;
    float fontScale_ = 1.0f;
};

} // namespace CrashEdit
