#include "ui/gool_script_editor.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>
namespace CrashEdit { namespace {
enum class Kind { Plain,Keyword,Builtin,Number,String,Comment,Punctuation };
struct Token { std::size_t begin,end; Kind kind; };
constexpr ImU32 Text=IM_COL32(222,226,234,255),Keyword=IM_COL32(198,157,242,255),
 Builtin=IM_COL32(126,190,239,255),Number=IM_COL32(177,211,143,255),
 String=IM_COL32(231,184,132,255),Comment=IM_COL32(132,148,168,255),
 Punctuation=IM_COL32(185,195,211,255),LineNumber=IM_COL32(103,116,135,255),
 ActiveLineNumber=IM_COL32(188,199,215,255),CurrentLine=IM_COL32(37,42,52,255),
 Selection=IM_COL32(62,91,128,180),Caret=IM_COL32(225,231,241,255);
bool identStart(unsigned char c){return std::isalpha(c)!=0||c=='_';}
bool identPart(unsigned char c){return std::isalnum(c)!=0||c=='_';}
std::string lower(std::string_view s){std::string r(s);std::transform(r.begin(),r.end(),r.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return r;}
Kind classify(std::string_view s){
 static const std::vector<std::string_view> kw={"event","state","routine","if","else","while","for","in","break","continue","return","call","transition","true","false","null","and","or","not","native","unknown","let","var","const"};
 static const std::vector<std::string_view> bi={"health","position","velocity","rotation","scale","frame","object_id","camera_id","state_id","eid","random","sin","cos","distance","timer","speed","target","player"};
 const auto v=lower(s);
 if(std::find(kw.begin(),kw.end(),v)!=kw.end())return Kind::Keyword;
 if(std::find(bi.begin(),bi.end(),v)!=bi.end())return Kind::Builtin;
 return Kind::Plain;
}
ImU32 color(Kind k){switch(k){case Kind::Keyword:return Keyword;case Kind::Builtin:return Builtin;case Kind::Number:return Number;case Kind::String:return String;case Kind::Comment:return Comment;case Kind::Punctuation:return Punctuation;default:return Text;}}
std::vector<Token> lex(std::string_view s){
 std::vector<Token> out;std::size_t i=0;
 while(i<s.size()){
  const auto c=static_cast<unsigned char>(s[i]);const auto a=i;Kind k=Kind::Plain;
  if(c=='#'){out.push_back({i,s.size(),Kind::Comment});break;}
  if(c=='"'||c=='\''){const char q=static_cast<char>(c);++i;while(i<s.size()){if(s[i]=='\\'&&i+1<s.size()){i+=2;continue;}if(s[i++]==q)break;}k=Kind::String;}
  else if(std::isdigit(c)){if(c=='0'&&i+1<s.size()&&(s[i+1]=='x'||s[i+1]=='X')){i+=2;while(i<s.size()&&(std::isxdigit(static_cast<unsigned char>(s[i]))||s[i]=='_'))++i;}else{while(i<s.size()&&std::isdigit(static_cast<unsigned char>(s[i])))++i;if(i<s.size()&&s[i]=='.'){++i;while(i<s.size()&&std::isdigit(static_cast<unsigned char>(s[i])))++i;}}k=Kind::Number;}
  else if(identStart(c)){++i;while(i<s.size()&&identPart(static_cast<unsigned char>(s[i])))++i;k=classify(s.substr(a,i-a));}
  else if(std::ispunct(c)){++i;if(i<s.size()&&std::strchr("=<>!&|+-*/%",static_cast<char>(c))&&s[i]=='=')++i;k=Kind::Punctuation;}
  else ++i;
  if(i>a)out.push_back({a,i,k});
 }
 return out;
}
std::pair<std::size_t,std::size_t> lineColumn(std::string_view s,std::size_t pos){
 pos=std::min(pos,s.size());std::size_t line=0,col=0;
 for(std::size_t i=0;i<pos;++i){if(s[i]=='\n'){++line;col=0;}else if(s[i]!='\r')++col;}
 return {line,col};
}
std::vector<std::string_view> linesOf(std::string_view s){
 std::vector<std::string_view> v;std::size_t a=0;
 while(a<=s.size()){auto e=s.find('\n',a);auto b=e==std::string_view::npos?s.size():e;if(b>a&&s[b-1]=='\r')--b;v.emplace_back(s.data()+a,b-a);if(e==std::string_view::npos)break;a=e+1;}
 return v;
}
}
void GoolScriptEditor::SetText(const std::string& s){if(initialized_&&text_==s)return;text_=s;inputState_.cursor=static_cast<int>(text_.size());inputState_.selectionStart=inputState_.selectionEnd=inputState_.cursor;initialized_=true;}
int GoolScriptEditor::inputCallback(ImGuiInputTextCallbackData* d){
 auto* st=static_cast<InputState*>(d->UserData);if(!st||!st->editor)return 0;
 if(d->EventFlag==ImGuiInputTextFlags_CallbackResize){auto& s=st->editor->text_;s.resize(static_cast<std::size_t>(d->BufTextLen));d->Buf=s.data();d->BufSize=static_cast<int>(s.capacity()+1);return 0;}
 st->cursor=d->CursorPos;st->selectionStart=d->SelectionStart;st->selectionEnd=d->SelectionEnd;return 0;
}
bool GoolScriptEditor::Render(const char* id,ImVec2 size,bool readOnly){
 inputState_.editor=this;
 const std::string child=std::string(id?id:"GoolEditor")+"Scroll";
 if(size.x<=0)size.x=ImGui::GetContentRegionAvail().x;if(size.y<=0)size.y=ImGui::GetContentRegionAvail().y;
 const bool visible=ImGui::BeginChild(child.c_str(),size,true);bool changed=false;
 if(visible){
  ImGui::SetWindowFontScale(fontScale_);
  const ImGuiStyle& style=ImGui::GetStyle();const float lh=ImGui::GetTextLineHeightWithSpacing(),gutter=48.0f;
  const auto lines=linesOf(text_);
  const ImVec2 contentAvail=ImGui::GetContentRegionAvail();
  // Keep the input surface taller than its text so it never creates a second,
  // independent vertical scrollbar. The surrounding editor child owns scrolling.
  const float textHeight=static_cast<float>(lines.size())*lh+style.FramePadding.y*2+lh*2.0f;
  const float h=std::max(contentAvail.y,textHeight);
  const ImVec2 origin=ImGui::GetCursorScreenPos();const float w=std::max(80.0f,contentAvail.x-gutter);
  ImGui::SetCursorScreenPos(ImVec2(origin.x+gutter,origin.y));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(6,style.FramePadding.y));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
  ImGui::PushStyleColor(ImGuiCol_FrameBg,IM_COL32(24,27,33,0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,IM_COL32(24,27,33,0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive,IM_COL32(24,27,33,0));
  ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(0,0,0,0));
  ImGui::PushStyleColor(ImGuiCol_Border,IM_COL32(0,0,0,0));
  ImGuiInputTextFlags flags=ImGuiInputTextFlags_CallbackResize|ImGuiInputTextFlags_CallbackAlways|ImGuiInputTextFlags_AllowTabInput|ImGuiInputTextFlags_NoHorizontalScroll;
  if(readOnly)flags|=ImGuiInputTextFlags_ReadOnly;
  changed=ImGui::InputTextMultiline(id?id:"##GoolCode",text_.data(),text_.capacity()+1,ImVec2(w,std::max(1.0f,h-style.FramePadding.y*2)),flags,inputCallback,&inputState_);
  const ImVec2 mn=ImGui::GetItemRectMin(),mx=ImGui::GetItemRectMax();const bool active=ImGui::IsItemActive();
  if(active&&ImGui::GetIO().KeyCtrl){
   const bool zoomIn=ImGui::IsKeyPressed(ImGuiKey_Equal,false)&&ImGui::GetIO().KeyShift;
   const bool zoomOut=ImGui::IsKeyPressed(ImGuiKey_Minus,false);
   const bool keypadIn=ImGui::IsKeyPressed(ImGuiKey_KeypadAdd,false);
   const bool keypadOut=ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract,false);
   if(zoomIn||keypadIn)fontScale_=std::min(2.0f,fontScale_+0.1f);
   else if(zoomOut||keypadOut)fontScale_=std::max(0.6f,fontScale_-0.1f);
  }
  ImGui::PopStyleColor(5);ImGui::PopStyleVar(2);
  drawSource(mn,mx,lh,style.FramePadding.y+1,active);
  ImDrawList* dl=ImGui::GetWindowDrawList();const ImVec2 g0(origin.x,mn.y),g1(origin.x+gutter,mx.y);dl->PushClipRect(g0,g1,true);dl->AddRectFilled(g0,g1,IM_COL32(27,30,37,255));
  const auto activeLine=lineColumn(text_,static_cast<std::size_t>(std::max(0,inputState_.cursor))).first;
  for(std::size_t i=0;i<lines.size();++i){char n[24];std::snprintf(n,sizeof(n),"%zu",i+1);const ImVec2 m=ImGui::CalcTextSize(n);dl->AddText(ImVec2(g1.x-m.x-9,mn.y+style.FramePadding.y+1+static_cast<float>(i)*lh),i==activeLine?ActiveLineNumber:LineNumber,n);}
  dl->PopClipRect();
 }
 ImGui::EndChild();return changed;
}
void GoolScriptEditor::drawSource(const ImVec2& mn,const ImVec2& mx,float lh,float off,bool focused){
 ImDrawList* dl=ImGui::GetWindowDrawList();dl->PushClipRect(mn,mx,true);ImFont* font=ImGui::GetFont();const float fs=ImGui::GetFontSize();
 const ImVec2 base(mn.x+6,mn.y+off);const auto lines=linesOf(text_);const auto cur=lineColumn(text_,static_cast<std::size_t>(std::max(0,inputState_.cursor)));const auto activeLine=cur.first;
 const int a=std::clamp(inputState_.selectionStart,0,static_cast<int>(text_.size())),b=std::clamp(inputState_.selectionEnd,0,static_cast<int>(text_.size()));const int sa=std::min(a,b),sb=std::max(a,b);
 std::size_t offset=0;
 for(std::size_t li=0;li<lines.size();++li){auto line=lines[li];const float y=base.y+static_cast<float>(li)*lh;if(li==activeLine)dl->AddRectFilled(ImVec2(mn.x,y-1),ImVec2(mx.x,y+lh),CurrentLine);
  const std::size_t end=offset+line.size();
  if(sb>sa&&static_cast<std::size_t>(sb)>offset&&static_cast<std::size_t>(sa)<end){const auto x0=std::max(offset,static_cast<std::size_t>(sa))-offset,x1=std::min(end,static_cast<std::size_t>(sb))-offset;const float left=font->CalcTextSizeA(fs,FLT_MAX,0,line.data(),line.data()+x0).x;const float width=font->CalcTextSizeA(fs,FLT_MAX,0,line.data()+x0,line.data()+x1).x;dl->AddRectFilled(ImVec2(base.x+left,y),ImVec2(base.x+left+width,y+lh),Selection);}
  for(const auto& t:lex(line)){const float x=font->CalcTextSizeA(fs,FLT_MAX,0,line.data(),line.data()+t.begin).x;dl->AddText(font,fs,ImVec2(base.x+x,y),color(t.kind),line.data()+t.begin,line.data()+t.end);}
  if(li==activeLine&&focused&&std::fmod(ImGui::GetTime(),1.0)<0.55){const auto col=std::min(cur.second,line.size());const float x=font->CalcTextSizeA(fs,FLT_MAX,0,line.data(),line.data()+col).x;dl->AddLine(ImVec2(base.x+x,y-1),ImVec2(base.x+x,y+lh-2),Caret,1.4f);}
  offset=end+1;
 }
 dl->PopClipRect();
}
}
