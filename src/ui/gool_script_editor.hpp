#pragma once
#include <string>
#include <imgui.h>
namespace CrashEdit {
class GoolScriptEditor {
public:
 void SetText(const std::string& text);
 const std::string& GetText() const { return text_; }
 bool Render(const char* id, ImVec2 size, bool readOnly = false);
private:
 struct InputState { GoolScriptEditor* editor=nullptr; int cursor=0,selectionStart=0,selectionEnd=0; };
 static int inputCallback(ImGuiInputTextCallbackData* data);
 void drawSource(const ImVec2& inputMin,const ImVec2& inputMax,float lineHeight,float textOffsetY,bool focused);
 std::string text_;
 InputState inputState_{nullptr,0,0,0};
 bool initialized_=false;
 float fontScale_=1.0f;
};
}
