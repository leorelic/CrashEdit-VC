// Minimal language registry for CrashEdit-VC.
// The GOOL Script language is configured directly by GoolScriptEditor.
#pragma once

#include "languages/TextEditorC.h"

inline void TextEditor::registerLanguages() {
    TextEditor::RegisterLanguage("C", GetLanguageDefinitionC, {".c"});
}
