#include "ui/editor_ui.hpp"
#include "ui/gool_script_editor.hpp"
#include "core/diagnostics.hpp"
#include "core/file_save.hpp"
#include "crash/formats/eid.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"
#include "crash/formats/gool/gool_control_flow.hpp"
#include "crash/formats/gool/gool_ir.hpp"
#include "crash/formats/gool/gool_decompiler.hpp"
#include "crash/formats/gool/gool_script.hpp"
#include "render/gl_api.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <map>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <string_view>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <tuple>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif
namespace CrashEdit {
static void usePremultipliedAnimationImageBlend(const ImDrawList*,const ImDrawCmd*){GL::BlendFunc(1U,0x0303U);}
struct RendererDiagnostics { double sceneBuildMs=0.0,gpuSubmitCpuMs=0.0; std::size_t gpuDrawCalls=0,gpuUploadBytes=0,cachedTextureCount=0,cachedTextureBytes=0; };
static RendererDiagnostics rendererDiagnostics;
struct EditorPreferences {
 int uiScaling=100;
 bool useModernFont=false,softwareRendering=false,depthBufferEnabled=true,rendererDiagnosticsVisible=false;
 bool showProperties=true,showConsole=true,createBakBackup=false,createDatedBackup=false;
 bool traditionalGoolInspection=false,palRegion=false,animationRefreshRateMode=false;
 int dockLayoutVersion=0;
 int sceneMoveSpeed=100,animationMoveSpeed=100;
 std::string lastNsfPath;
};
static EditorPreferences editorPreferences;

struct BlueprintInstructionCacheRow {
 Crash::NsfGoolInstructionAnalysis analysis;
 std::string reads,writes,uncertain,from,usedBy;
 std::vector<std::string> fromLinks,usedByLinks;
};
struct BlueprintAnalysisCache {
 const Crash::NsfGool* source=nullptr;
 std::size_t instructionCount=0;
 Crash::NsfGoolControlFlowGraph graph;
 Crash::NsfGoolDataFlowResult dataFlow;
 std::vector<BlueprintInstructionCacheRow> rows;
 bool dataFlowReady=false;
};
static BlueprintAnalysisCache blueprintAnalysisCache;
static BlueprintAnalysisCache& blueprintCacheFor(const Crash::NsfGool& gool) {
 auto& cache=blueprintAnalysisCache;
 if(cache.source!=&gool||cache.instructionCount!=gool.instructions.size()) {
  cache=BlueprintAnalysisCache{};
  cache.source=&gool;
  cache.instructionCount=gool.instructions.size();
  cache.graph=Crash::analyzeGoolControlFlow(gool);
 }
 return cache;
}
static void prepareBlueprintDataFlow(BlueprintAnalysisCache& cache,const Crash::NsfGool& gool) {
 if(cache.dataFlowReady)return;
 cache.dataFlow=Crash::analyzeGoolDataFlow(gool);
 cache.rows.resize(gool.instructions.size());
 for(std::size_t i=0;i<gool.instructions.size();++i) {
  auto& row=cache.rows[i];
  row.analysis=Crash::analyzeGoolInstruction(gool,i);
  for(const auto& effect:row.analysis.operandEffects) {
   std::string& target=effect.access==Crash::NsfGoolOperandAccess::Read?row.reads:
    effect.access==Crash::NsfGoolOperandAccess::Write?row.writes:row.uncertain;
   if(!target.empty())target+=", ";
   target+=effect.reference;
  }
 }
 for(const auto& use:cache.dataFlow.uses) {
  if(use.instructionIndex>=cache.rows.size()||use.reachingDefinitions.empty())continue;
  std::string relation=use.location+" <- ";
  for(std::size_t n=0;n<use.reachingDefinitions.size();++n) {
   if(n)relation+="/";
   const auto definitionId=use.reachingDefinitions[n];
   if(definitionId<cache.dataFlow.definitions.size())
    relation+="#"+std::to_string(cache.dataFlow.definitions[definitionId].instructionIndex);
  }
  auto& fromLinks=cache.rows[use.instructionIndex].fromLinks;
  if(std::find(fromLinks.begin(),fromLinks.end(),relation)==fromLinks.end())
   fromLinks.push_back(relation);
  const std::string reverseRelation=use.location+" -> #"+std::to_string(use.instructionIndex);
  for(const auto definitionId:use.reachingDefinitions) {
   if(definitionId>=cache.dataFlow.definitions.size())continue;
   const auto definitionInstruction=cache.dataFlow.definitions[definitionId].instructionIndex;
   if(definitionInstruction>=cache.rows.size())continue;
   auto& links=cache.rows[definitionInstruction].usedByLinks;
   if(std::find(links.begin(),links.end(),reverseRelation)==links.end())
    links.push_back(reverseRelation);
  }
 }
 for(auto& row:cache.rows) {
  for(const auto& link:row.fromLinks) {
   if(!row.from.empty())row.from+=", ";
   row.from+=link;
  }
  for(const auto& link:row.usedByLinks) {
   if(!row.usedBy.empty())row.usedBy+=", ";
   row.usedBy+=link;
  }
 }
 cache.dataFlowReady=true;
}
static void invalidateBlueprintAnalysisCache() {
 blueprintAnalysisCache=BlueprintAnalysisCache{};
}
static void* readEditorSettingsOpen(ImGuiContext*,ImGuiSettingsHandler* handler,const char*){return handler->UserData;}
static void readEditorSettingsLine(ImGuiContext*,ImGuiSettingsHandler*,void*,const char* line){
 int value=0;
 if(std::sscanf(line,"UIScaling=%d",&value)==1)editorPreferences.uiScaling=std::clamp(value,100,200);
 else if(std::sscanf(line,"UseModernFont=%d",&value)==1)editorPreferences.useModernFont=value!=0;
 else if(std::sscanf(line,"SoftwareRendering=%d",&value)==1)editorPreferences.softwareRendering=value!=0;
 else if(std::sscanf(line,"DepthBufferEnabled=%d",&value)==1)editorPreferences.depthBufferEnabled=value!=0;
 else if(std::sscanf(line,"ShowRendererDiagnostics=%d",&value)==1)editorPreferences.rendererDiagnosticsVisible=value!=0;
 else if(std::sscanf(line,"SceneMoveSpeed=%d",&value)==1)editorPreferences.sceneMoveSpeed=std::clamp(value,100,300);
  else if(std::sscanf(line,"AnimationMoveSpeed=%d",&value)==1)editorPreferences.animationMoveSpeed=std::clamp(value,25,400);
 else if(std::sscanf(line,"ShowProperties=%d",&value)==1)editorPreferences.showProperties=value!=0;
 else if(std::sscanf(line,"ShowConsole=%d",&value)==1)editorPreferences.showConsole=value!=0;
 else if(std::sscanf(line,"TraditionalGoolInspection=%d",&value)==1)editorPreferences.traditionalGoolInspection=value!=0;
 else if(std::sscanf(line,"PALRegion=%d",&value)==1)editorPreferences.palRegion=value!=0;
 else if(std::sscanf(line,"AnimationPlaybackMode=%d",&value)==1)editorPreferences.animationRefreshRateMode=value!=0;
 else if(std::sscanf(line,"CreateSaveBackups=%d",&value)==1){editorPreferences.createBakBackup=value!=0;editorPreferences.createDatedBackup=value!=0;}
 else if(std::sscanf(line,"CreateBakBackup=%d",&value)==1)editorPreferences.createBakBackup=value!=0;
 else if(std::sscanf(line,"CreateDatedBackup=%d",&value)==1)editorPreferences.createDatedBackup=value!=0;
 else if(std::sscanf(line,"DockLayoutVersion=%d",&value)==1)editorPreferences.dockLayoutVersion=value;
 else if(std::strncmp(line,"LastNSF=",8)==0)editorPreferences.lastNsfPath=line+8;
}
static void writeEditorSettings(ImGuiContext*,ImGuiSettingsHandler* handler,ImGuiTextBuffer* buffer){
 const auto& p=*static_cast<EditorPreferences*>(handler->UserData);
 buffer->appendf("[CrashEdit][Preferences]\nUIScaling=%d\nUseModernFont=%d\nSoftwareRendering=%d\nDepthBufferEnabled=%d\nShowRendererDiagnostics=%d\nSceneMoveSpeed=%d\nAnimationMoveSpeed=%d\nShowProperties=%d\nShowConsole=%d\nCreateBakBackup=%d\nCreateDatedBackup=%d\nTraditionalGoolInspection=%d\nPALRegion=%d\nAnimationPlaybackMode=%d\nDockLayoutVersion=%d\nLastNSF=%s\n\n",
 p.uiScaling,p.useModernFont?1:0,p.softwareRendering?1:0,p.depthBufferEnabled?1:0,p.rendererDiagnosticsVisible?1:0,p.sceneMoveSpeed,p.animationMoveSpeed,p.showProperties?1:0,p.showConsole?1:0,p.createBakBackup?1:0,p.createDatedBackup?1:0,p.traditionalGoolInspection?1:0,p.palRegion?1:0,p.animationRefreshRateMode?1:0,p.dockLayoutVersion,p.lastNsfPath.c_str());
}
static void registerEditorSettingsHandler(){
 ImGuiSettingsHandler handler{};
 handler.TypeName="CrashEdit";handler.TypeHash=ImHashStr("CrashEdit");
 handler.ReadOpenFn=readEditorSettingsOpen;handler.ReadLineFn=readEditorSettingsLine;
 handler.WriteAllFn=writeEditorSettings;handler.UserData=&editorPreferences;
 ImGui::AddSettingsHandler(&handler);
}
static void grid(ImDrawList*d,ImVec2 a,ImVec2 b){ImVec2 c((a.x+b.x)/2,a.y+(b.y-a.y)*.58f);float h=a.y+(b.y-a.y)*.3f;d->AddRectFilled(a,b,IM_COL32(25,30,38,255));d->AddRectFilled(a,ImVec2(b.x,h),IM_COL32(29,36,46,255));for(int i=-12;i<=12;++i)d->AddLine(ImVec2(c.x+i*42,h),ImVec2(c.x+i*115,b.y),IM_COL32(86,105,126,95));for(int i=1;i<=12;++i){float t=float(i)/12;float y=h+(b.y-h)*t*t;d->AddLine(ImVec2(a.x,y),ImVec2(b.x,y),IM_COL32(86,105,126,95));}}
static const char* versionName(GameVersion v){switch(v){case GameVersion::Crash1Beta1995:return "Crash 1 — 1995 Beta";case GameVersion::Crash1BetaMAR08:return "Crash 1 — March 8 Beta";case GameVersion::Crash1BetaMAY11:return "Crash 1 — May 11 Beta";case GameVersion::Crash1:return "Crash 1";case GameVersion::Crash2:return "Crash 2";case GameVersion::Crash3BetaMAY14:return "Crash 3 — May 14 Beta";case GameVersion::Crash3:return "Crash 3";}return "Unknown";}
static Crash::NsdVariant nsdVariant(GameVersion v){if(v==GameVersion::Crash3||v==GameVersion::Crash3BetaMAY14)return Crash::NsdVariant::Crash3;if(v==GameVersion::Crash1Beta1995||v==GameVersion::Crash1BetaMAR08||v==GameVersion::Crash1BetaMAY11)return Crash::NsdVariant::Proto;if(v==GameVersion::Crash1)return Crash::NsdVariant::Old;return Crash::NsdVariant::Standard;}
static std::string lowercase(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});return value;}
static bool hasExtension(const std::filesystem::path& file,const char* extension){return lowercase(file.extension().string())==extension;}
static std::filesystem::path findNsdSidecar(const std::filesystem::path& nsf){
 auto expected=nsf;expected.replace_extension(".nsd");
 std::error_code ec;if(std::filesystem::is_regular_file(expected,ec))return expected;
 const std::string wantedStem=lowercase(nsf.stem().string());
 const auto directory=nsf.has_parent_path()?nsf.parent_path():std::filesystem::path(".");
 for(std::filesystem::directory_iterator it(directory,ec),end;!ec&&it!=end;it.increment(ec)){
  const auto& candidate=it->path();
  if(hasExtension(candidate,".nsd")&&lowercase(candidate.stem().string())==wantedStem){
   std::error_code fileError;if(std::filesystem::is_regular_file(candidate,fileError))return candidate;
  }
 }
 return expected;
}
#ifndef _WIN32
static std::string browseNsfPath(){
#ifdef __APPLE__
 const char* command="osascript -e 'POSIX path of (choose file with prompt \"Select a Crash NSF\")' 2>/dev/null";
#else
 const char* command="zenity --file-selection --title='Open CrashEdit NSF' --file-filter='Crash NSF | *.nsf *.NSF' 2>/dev/null";
#endif
 std::FILE* pipe=::popen(command,"r");if(!pipe)return {};
 char buffer[4096]{};std::string result;
 while(std::fgets(buffer,sizeof(buffer),pipe))result+=buffer;
 ::pclose(pipe);
 while(!result.empty()&&(result.back()=='\n'||result.back()=='\r'))result.pop_back();
 return result;
}
#endif
static Crash::NsfGoolVersion nsfGoolVersion(GameVersion v){
 switch(v){
  case GameVersion::Crash1Beta1995:
  case GameVersion::Crash1BetaMAR08:return Crash::NsfGoolVersion::Version0;
  case GameVersion::Crash1:
  case GameVersion::Crash1BetaMAY11:return Crash::NsfGoolVersion::Version1;
  case GameVersion::Crash2:return Crash::NsfGoolVersion::Version2;
  case GameVersion::Crash3BetaMAY14:
  case GameVersion::Crash3:return Crash::NsfGoolVersion::Version3;
 }
 return Crash::NsfGoolVersion::Version2;
}
static const char* chunkTypeName(std::uint16_t t){switch(t){case 0:return "Entry Chunk";case 1:return "Texture Chunk";case 2:return "Old Sound Chunk";case 3:return "Sound Chunk";case 4:return "Wavebank Chunk";case 5:return "Speech Chunk";default:return "Unknown Chunk";}}
static void pixelIcon(ImDrawList*d,ImVec2 p,float s,const char*key){
 auto r=[&](int x,int y,int w,int h,ImU32 c){d->AddRectFilled(ImVec2(p.x+x*s,p.y+y*s),ImVec2(p.x+(x+w)*s,p.y+(y+h)*s),c);};
 const ImU32 ink=IM_COL32(25,30,39,255),white=IM_COL32(245,240,220,255);
 const std::string k=key?key:"";
 if(k=="ThingViolet"){r(2,1,12,14,IM_COL32(82,42,151,255));r(4,3,8,10,IM_COL32(229,151,43,255));r(5,5,6,6,IM_COL32(62,157,70,255));r(7,6,3,4,IM_COL32(65,177,224,255));r(3,2,2,2,IM_COL32(255,211,90,255));}
 else if(k=="ThingBlue"){r(1,2,14,12,IM_COL32(24,80,139,255));r(2,4,12,8,IM_COL32(68,178,230,255));r(2,10,12,4,IM_COL32(39,126,54,255));r(4,7,4,4,white);r(9,8,4,3,IM_COL32(222,241,255,255));}
 else if(k=="ThingLime"){r(1,1,14,14,IM_COL32(38,76,45,255));r(3,3,10,10,IM_COL32(169,210,128,255));r(4,4,2,2,IM_COL32(225,244,187,255));r(8,4,2,2,IM_COL32(225,244,187,255));r(5,7,6,4,IM_COL32(46,103,57,255));r(3,6,1,6,IM_COL32(211,232,164,255));r(12,6,1,6,IM_COL32(211,232,164,255));}
 else if(k=="Arrow"){r(1,6,8,4,IM_COL32(104,112,122,255));r(7,3,5,10,IM_COL32(104,112,122,255));r(10,1,5,14,IM_COL32(104,112,122,255));r(11,2,3,12,IM_COL32(205,212,220,255));}
 else if(k=="Painting"){r(1,1,14,14,IM_COL32(142,68,10,255));r(3,3,10,10,IM_COL32(246,173,45,255));r(4,4,8,7,IM_COL32(76,178,223,255));r(4,8,3,3,IM_COL32(48,133,56,255));r(8,7,4,4,IM_COL32(44,121,52,255));r(6,12,4,1,IM_COL32(255,224,120,255));}
 else if(k.find("MusicNote")==0){ImU32 c=k=="MusicNoteBlue"?IM_COL32(49,111,230,255):k=="MusicNoteYellow"?IM_COL32(238,188,48,255):IM_COL32(225,73,58,255);r(10,1,3,11,c);r(6,3,7,2,c);r(4,11,5,3,c);r(2,12,4,3,c);r(10,1,2,2,white);}
 else if(k.find("Speaker")==0){ImU32 c=k=="SpeakerBlue"?IM_COL32(54,126,211,255):IM_COL32(222,232,242,255);r(1,6,4,6,c);r(5,4,3,10,c);r(8,3,2,12,c);r(11,5,1,8,white);r(13,7,1,4,white);}
 else if(k.find("Journal")==0||k=="File"){ImU32 c=k=="JournalOrange"?IM_COL32(222,132,39,255):k=="JournalBlue"?IM_COL32(59,133,196,255):k=="JournalGray"?IM_COL32(145,155,166,255):IM_COL32(220,232,243,255);r(3,1,10,14,c);r(2,2,2,12,IM_COL32(75,87,102,255));r(5,4,6,2,white);r(5,8,6,1,IM_COL32(91,107,123,255));r(5,10,6,1,IM_COL32(91,107,123,255));}
 else if(k=="ThingCode"){r(2,1,12,14,IM_COL32(218,233,247,255));r(4,4,8,1,IM_COL32(69,129,187,255));r(4,7,2,3,IM_COL32(49,121,206,255));r(10,7,2,3,IM_COL32(49,121,206,255));r(7,6,2,5,ink);}
 else if(k=="ThingCrimson"){r(8,1,6,4,IM_COL32(224,105,118,255));r(2,5,6,7,IM_COL32(194,73,87,255));r(8,5,6,7,IM_COL32(144,55,69,255));r(5,12,6,3,IM_COL32(117,42,56,255));r(8,5,1,10,white);}
 else if(k=="ThingYellow"){r(2,3,12,11,IM_COL32(214,164,36,255));r(4,5,3,3,IM_COL32(224,75,56,255));r(9,4,3,3,IM_COL32(58,147,214,255));r(8,9,3,3,IM_COL32(62,169,82,255));}
 else if(k=="ThingStar"){r(7,1,3,5,IM_COL32(255,205,44,255));r(5,5,7,4,IM_COL32(255,205,44,255));r(1,6,14,3,IM_COL32(255,205,44,255));r(4,9,8,3,IM_COL32(255,205,44,255));r(5,11,3,4,IM_COL32(255,205,44,255));r(9,11,3,4,IM_COL32(255,205,44,255));}
 else if(k=="ThingDoor"){r(3,1,11,14,IM_COL32(171,101,21,255));r(5,3,8,12,IM_COL32(72,45,25,255));r(10,8,1,2,IM_COL32(255,212,88,255));}
 else if(k=="Folder"||k=="FolderOpen"){r(1,4,7,3,IM_COL32(225,164,44,255));r(1,6,14,9,IM_COL32(177,111,25,255));r(2,8,12,6,IM_COL32(232,180,73,255));if(k=="FolderOpen")r(1,9,14,5,IM_COL32(245,194,87,255));}
 else {r(2,2,12,12,IM_COL32(211,137,42,255));r(4,4,8,8,IM_COL32(255,211,102,255));}
}
static bool treeNodeWithIcon(const char*label,ImGuiTreeNodeFlags flags,const char*icon){
 const ImVec2 row=ImGui::GetCursorScreenPos();
 const std::string padded=std::string("  ")+label;
 const bool open=ImGui::TreeNodeEx(padded.c_str(),flags);
 const float scale=ImGui::GetIO().FontGlobalScale;
 pixelIcon(ImGui::GetWindowDrawList(),ImVec2(row.x+17.0f*scale,row.y+1.0f*scale),scale,icon);
 return open;
}

static bool hierarchyIconRow(const char* label,bool selected,const char* icon){
 const ImVec2 row=ImGui::GetCursorScreenPos();
 const float scale=ImGui::GetIO().FontGlobalScale;
 const float iconSize=16.0f*scale;
 const float textOffset=22.0f*scale;
 const bool clicked=ImGui::Selectable("##HierarchyIconRow",selected,ImGuiSelectableFlags_SpanAvailWidth);
 const ImVec2 rowMax=ImGui::GetItemRectMax();
 const float rowHeight=rowMax.y-row.y;
 const float fontSize=ImGui::GetFontSize();
 const float iconY=row.y+std::max(0.0f,(rowHeight-iconSize)*0.5f);
 const float textY=row.y+std::max(0.0f,(rowHeight-fontSize)*0.5f);
 ImDrawList* draw=ImGui::GetWindowDrawList();
 pixelIcon(draw,ImVec2(row.x+2.0f*scale,iconY),scale,icon);
 draw->AddText(ImVec2(row.x+textOffset,textY),ImGui::GetColorU32(ImGuiCol_Text),label);
 return clicked;
}
static const char* chunkIconName(std::uint16_t type){
 switch(type){case 0:return "JournalOrange";case 1:return "Painting";case 2:case 3:return "JournalBlue";case 4:return "MusicNoteRed";case 5:return "JournalWhite";default:return "JournalGray";}
}
static const char* entryIconName(std::uint32_t type){
 switch(type){case 1:return "ThingLime";case 3:return "ThingBlue";case 7:return "ThingViolet";case 21:return "Painting";case 6:case 15:case 17:return "ThingOrange";case 11:return "ThingCode";default:return "ThingGray";}
}
static const char* entityPropertyName(std::uint16_t id){switch(id){case 0x2C:return "Name";case 0x32:return "Z modifier";case 0x4B:return "Positions";case 0x9F:return "Entity ID";case 0xA4:return "GOOL arguments / Settings";case 0xA9:return "GOOL Type";case 0xAA:return "GOOL Subtype";case 0x277:return "DDA Death / 1-Up Count";case 0x287:return "Victim Object IDs";case 0x288:return "DDA Section";case 0x28B:return "Box Count (Willy)";case 0x119:return "Camera panning";case 0x130:return "Field of view";case 0x142:return "Camera distance";case 0x162:return "Camera field 0x162";case 0x16D:return "Camera setting 0x16D";case 0x173:return "Camera index";case 0x174:return "Camera sub-index";case 0x176:return "Camera path links";case 0x185:return "Camera flags";case 0x186:return "Camera water settings";case 0x198:return "Camera event sender";case 0x1A8:return "Camera transitions";case 0x1B5:return "Camera particles";case 0x1B8:return "Camera FX control";case 0x1DE:return "Camera fog distance";case 0x1FA:return "Camera backgrounds";case 0x27F:return "Camera scenery updates";case 0x297:return "Camera mirrors";case 0x2AA:return "Camera stars";default:return "Unknown / game-specific";}}
EditorUI::EditorUI(Workspace& w):workspace_(w){registerEditorSettingsHandler();}
void EditorUI::handleDroppedFiles(int count,const char** paths){
 for(int i=0;i<count;++i){
  if(!paths||!paths[i])continue;
  const std::string droppedPathText(paths[i]);
  std::filesystem::path droppedPath=std::filesystem::u8path(droppedPathText);
  std::string extension=droppedPath.extension().string();
  std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
  if(extension!=".nsf")continue;
  if(droppedPathText.size()>=sizeof(path_)){
   status_="Dropped NSF path is too long to fit in the file path field.";
   return;
  }
  std::snprintf(path_,sizeof(path_),"%s",droppedPathText.c_str());
  openDialog_=false;
  versionDialog_=true;
  centerVersionDialog_=true;
  status_="NSF dropped: choose its Crash game version to continue.";
  return;
 }
 status_="No NSF file found in the dropped files.";
}
bool EditorUI::loadLevel(const std::filesystem::path& file,std::string& error){
 Crash::NsfArchive archive;if(!archive.load(file,error)){Diagnostics::log(DiagnosticLevel::Error,"Failed to load NSF: "+error);return false;}if(pendingVersion_==GameVersion::Crash2||pendingVersion_==GameVersion::Crash3BetaMAY14||pendingVersion_==GameVersion::Crash3)archive.decodeZones(pendingVersion_==GameVersion::Crash3);archive.decodeGools(nsfGoolVersion(pendingVersion_));if(pendingVersion_==GameVersion::Crash2||pendingVersion_==GameVersion::Crash3)archive.decodeAnimationModels(pendingVersion_==GameVersion::Crash3);
 auto sidecar=findNsdSidecar(file);Crash::NsdFile level;std::string nsdError;
 bool haveNsd=level.load(sidecar,nsdVariant(pendingVersion_),nsdError);
 if(haveNsd)for(const auto&warning:level.validationWarnings())Diagnostics::log(DiagnosticLevel::Warning,warning);
 if(!workspace_.open(file.parent_path(),error)){Diagnostics::log(DiagnosticLevel::Error,"Failed to open level workspace: "+error);return false;}
 invalidateBlueprintAnalysisCache();nsf_=std::move(archive);sceneCameraInitialized_=false;sceneCameraChunk_=-1;sceneCameraEntry_=-1;viewerChunk_=-1;viewerEntry_=-1;nsfPath_=file;nsdPath_=sidecar;nsdValid_=haveNsd;if(haveNsd)nsd_=std::move(level);
 nsfValid_=true;selectedChunk_=-1;selectedSpawn_=-1;selectedEntry_=-1;selectedEntity_=-1;gameVersion_=pendingVersion_;workspace_.selectResource(file.string());
 status_=std::string("Loaded ")+versionName(gameVersion_)+" NSF ("+std::to_string(nsf_.chunks().size())+" chunks).";
 if(!haveNsd){status_+=" Matching NSD unavailable: "+nsdError;Diagnostics::log(DiagnosticLevel::Warning,"Matching NSD unavailable: "+nsdError);}
 editorPreferences.lastNsfPath=file.string();
 std::snprintf(path_,sizeof(path_),"%s",editorPreferences.lastNsfPath.c_str());
 ImGui::MarkIniSettingsDirty();
 Diagnostics::log(DiagnosticLevel::Info,std::string("Loaded ")+versionName(gameVersion_)+" NSF with "+std::to_string(nsf_.chunks().size())+" chunks.");
 return true;
}
bool EditorUI::saveFiles(){
 if(!nsfValid_||!nsdValid_){status_="Save requires both the NSF and its matching NSD.";Diagnostics::log(DiagnosticLevel::Warning,status_);return false;}
 std::string error;
 std::vector<std::uint8_t> nsdBytes,nsfBytes;
 if(!nsd_.saveBytes(nsdBytes,error)){status_="Save failed (NSD serialization): "+error;Diagnostics::log(DiagnosticLevel::Error,status_);return false;}
 if(!nsf_.saveUncompressedBytes(nsfBytes,error)){status_="Save failed (NSF serialization): "+error;Diagnostics::log(DiagnosticLevel::Error,status_);return false;}
 std::string dateStamp;
 if(editorPreferences.createDatedBackup){
  const std::time_t now=std::time(nullptr);std::tm localTime{};
#ifdef _WIN32
  localtime_s(&localTime,&now);
#else
  localtime_r(&now,&localTime);
#endif
  char date[11];if(std::strftime(date,sizeof(date),"%Y-%m-%d",&localTime)==0){status_="Could not format the save archive date.";Diagnostics::log(DiagnosticLevel::Error,status_);return false;}dateStamp=date;
 }
 if(!saveFilePair(nsfPath_,nsfBytes,nsdPath_,nsdBytes,editorPreferences.createBakBackup,editorPreferences.createDatedBackup,dateStamp,error)){
  status_="Save failed: "+error;Diagnostics::log(DiagnosticLevel::Error,status_);return false;
 }
 status_="Saved NSF and NSD.";
 if(editorPreferences.createBakBackup&&editorPreferences.createDatedBackup)status_="Saved NSF and NSD with .bak backups and dated archives.";
 else if(editorPreferences.createBakBackup)status_="Saved NSF and NSD with .bak backups.";
 else if(editorPreferences.createDatedBackup)status_="Saved NSF and NSD with dated archives.";
 Diagnostics::log(DiagnosticLevel::Info,status_);
 return true;
}
void EditorUI::draw(){
 if(nsfValid_&&ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F,false))searchWindow_=true;
 auto*v=ImGui::GetMainViewport();ImGui::SetNextWindowPos(v->WorkPos);ImGui::SetNextWindowSize(v->WorkSize);ImGui::SetNextWindowViewport(v->ID);
 ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
 ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::Begin("CrashEditHost",nullptr,f);ImGui::PopStyleVar(3);drawMenu();drawToolbar();
 auto id=ImGui::GetID("CrashEditDockSpace");
 if(ImGui::DockBuilderGetNode(id)==nullptr||editorPreferences.dockLayoutVersion<3){if(ImGui::DockBuilderGetNode(id)!=nullptr)ImGui::DockBuilderRemoveNode(id);ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace|ImGuiDockNodeFlags_PassthruCentralNode);ImGui::DockBuilderSetNodeSize(id,v->WorkSize);ImGuiID center=id;auto right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.28f,nullptr,&center);auto rightBottom=ImGui::DockBuilderSplitNode(right,ImGuiDir_Down,.42f,nullptr,&right);auto bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.27f,nullptr,&center);ImGui::DockBuilderDockWindow("3D Zone Editor",center);ImGui::DockBuilderDockWindow("3D Animation Editor",center);ImGui::DockBuilderDockWindow("GOOL Inspector",center);ImGui::DockBuilderDockWindow("GOOL Script Editor",center);ImGui::DockBuilderDockWindow("GOOL Blueprint",center);ImGui::DockBuilderDockWindow("Resource Hierarchy",right);ImGui::DockBuilderDockWindow("Properties",rightBottom);ImGui::DockBuilderDockWindow("Console / Diagnostics",bottom);ImGui::DockBuilderDockWindow("GOOL Data Pool",bottom);ImGui::DockBuilderDockWindow("GOOL State Map",bottom);ImGui::DockBuilderDockWindow("GOOL State Descriptions",bottom);ImGui::DockBuilderDockWindow("GOOL Frame Groups",bottom);ImGui::DockBuilderDockWindow("Search Entries",bottom);ImGui::DockBuilderDockWindow("Texture Viewer",bottom);ImGui::DockBuilderDockWindow("CLUT",bottom);ImGui::DockBuilderDockWindow("Spawn Points",bottom);ImGui::DockBuilderDockWindow("Animation Frame Editor",bottom);ImGui::DockBuilderDockWindow("Animation Timeline",bottom);ImGui::DockBuilderFinish(id);editorPreferences.dockLayoutVersion=3;ImGui::MarkIniSettingsDirty();}
 ImGui::DockSpace(id,ImVec2(0,-(ImGui::GetTextLineHeightWithSpacing()+ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().ItemSpacing.y)),ImGuiDockNodeFlags_PassthruCentralNode);ImGui::Separator();ImGui::TextUnformatted(status_.c_str());
 drawHierarchy();
 bool showAnimationWorkspace=false;
 const Crash::NsfEntry*selectedAnimationResource=nullptr;
 const Crash::NsfEntry*selectedModelResource=nullptr;
 if(nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto&selectedChunk=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(selectedChunk.type==0&&selectedEntry_>=0&&static_cast<std::size_t>(selectedEntry_)<selectedChunk.entries.size()){
   const auto&selected=selectedChunk.entries[static_cast<std::size_t>(selectedEntry_)];
   if(selected.valid&&selected.type==1&&selected.animation.decoded)selectedAnimationResource=&selected;
   else if(selected.valid&&selected.type==2&&selected.model.decoded)selectedModelResource=&selected;
  }
 }
 showAnimationWorkspace=selectedAnimationResource||selectedModelResource;
 if(!showAnimationWorkspace)animationWorkspaceWasVisible_=false;
 if(showAnimationWorkspace){
  const bool modelMode=selectedModelResource!=nullptr;
  const std::uint32_t modelEid=modelMode?selectedModelResource->eid:0U;
  const bool selectionChanged=!animationWorkspaceWasVisible_||animationSelectionChunk_!=selectedChunk_||animationSelectionEntry_!=selectedEntry_;
  animationWorkspaceWasVisible_=true;
  if(selectionChanged||animationSelectionModel_!=modelMode||animationSelectedModelEid_!=modelEid){
   animationSelectionChunk_=selectedChunk_;animationSelectionEntry_=selectedEntry_;
   animationSelectionModel_=modelMode;animationSelectedModelEid_=modelEid;
   animationEditorFocus_=true;
  }
  auto animationUsesModel=[](const Crash::NsfEntry&entry,std::uint32_t eid){
   if(!entry.valid||entry.type!=1||!entry.animation.decoded)return false;
   for(const auto&frame:entry.animation.frames){
    const std::uint32_t linked=entry.animation.isCrash3?frame.linkedModelEid:frame.modelEid;
    if(linked==eid)return true;
   }
   return false;
  };
  bool currentAnimationValid=false;
  if(animationViewerChunk_>=0&&static_cast<std::size_t>(animationViewerChunk_)<nsf_.chunks().size()){
   const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(animationViewerChunk_)];
   if(chunk.type==0&&animationViewerEntry_>=0&&static_cast<std::size_t>(animationViewerEntry_)<chunk.entries.size()){
    const auto&entry=chunk.entries[static_cast<std::size_t>(animationViewerEntry_)];
    currentAnimationValid=entry.valid&&entry.type==1&&entry.animation.decoded&&
     (!modelMode||animationUsesModel(entry,modelEid));
   }
  }
  if(!modelMode){
   if(animationViewerChunk_!=selectedChunk_||animationViewerEntry_!=selectedEntry_){
    animationViewerChunk_=selectedChunk_;animationViewerEntry_=selectedEntry_;
    animationFrame_=0;animationPlaybackFrame_=0.0;animationFrameTimer_=0.0f;
    animationPlaying_=false;animationHalfSpeed_=false;animationCameraInitialized_=false;
   }
  }else if(!currentAnimationValid){
   int firstChunk=-1,firstEntry=-1;
   for(std::size_t ci=0;ci<nsf_.chunks().size()&&firstChunk<0;++ci){
    const auto&chunk=nsf_.chunks()[ci];if(chunk.type!=0||!chunk.entryTableValid)continue;
    for(std::size_t ei=0;ei<chunk.entries.size();++ei){
     if(animationUsesModel(chunk.entries[ei],modelEid)){firstChunk=static_cast<int>(ci);firstEntry=static_cast<int>(ei);break;}
    }
   }
   animationViewerChunk_=firstChunk;animationViewerEntry_=firstEntry;
   animationFrame_=0;animationPlaybackFrame_=0.0;animationFrameTimer_=0.0f;
   animationPlaying_=false;animationHalfSpeed_=false;animationCameraInitialized_=false;
  }
 }
 drawViewport();
 if(showAnimationWorkspace)drawAnimationViewer();
 if(animationFrameEditorOpen_&&showAnimationWorkspace)drawAnimationFrameEditor();
 drawBottom();
 if(editorPreferences.traditionalGoolInspection){drawGoolDataWindows();drawGoolInspector();drawGoolGraph();}
 else{drawGoolBlueprintGraph();drawGoolBlueprintInstructions();drawGoolScriptEditor();}
 if(selectedChunk_==-2&&nsdValid_)drawSpawnPointsEditor();drawNsdGoolMap();
 const bool textureChunkSelected=nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()&&nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)].type==1;
 if(textureChunkSelected){drawTextureViewer();drawClutEditor();}
 drawResourceActionDialog();
 // Keep Animation Timeline last in the dock submission order, then explicitly
 // restore focus to the main animation editor (new bottom-dock windows can
 // otherwise steal focus when they are first submitted).
 if(showAnimationWorkspace)drawAnimationTimeline();
 if(showAnimationWorkspace&&animationEditorFocus_){
  // Select the editor's tab in its central dock node after every other
  // window (especially the bottom Animation Timeline) has been submitted.
  // The generic focus request alone can leave the bottom dock tab selected
  // when the animation workspace is created for the first time.
  if(ImGuiWindow* animationEditor=ImGui::FindWindowByName("3D Animation Editor")){
   if(animationEditor->DockNode)
    animationEditor->DockNode->SelectedTabId=animationEditor->ID;
   ImGui::FocusWindow(animationEditor);
  }
  animationEditorFocus_=false;
 }
 ImGui::End();
 drawSearchWindow();
 if(!restoredLastNsfPath_){
  if(path_[0]=='\0'&&!editorPreferences.lastNsfPath.empty())
   std::snprintf(path_,sizeof(path_),"%s",editorPreferences.lastNsfPath.c_str());
  restoredLastNsfPath_=true;
 }
 const ImGuiViewport* dialogViewport=ImGui::GetMainViewport();
 const ImVec2 dialogCenter=dialogViewport->GetCenter();
 const ImGuiWindowFlags dialogFlags=ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings;
 if(openDialog_){
  const bool centerThisFrame=centerOpenDialog_;
  if(centerThisFrame)ImGui::SetNextWindowFocus();
  const bool openDialogVisible=ImGui::Begin("Open NSF",&openDialog_,dialogFlags);
  if(centerThisFrame){
   const ImVec2 windowSize=ImGui::GetWindowSize();
   ImGui::SetWindowPos(ImVec2(dialogCenter.x-windowSize.x*0.5f,dialogCenter.y-windowSize.y*0.5f),ImGuiCond_Always);
   centerOpenDialog_=false;
  }
  if(openDialogVisible){
   ImGui::TextWrapped("Select a level NSF. The game version is requested before the matching NSD is interpreted.");
   ImGui::SetNextItemWidth(420.0f);ImGui::InputText("NSF file path",path_,sizeof(path_));
   ImGui::SameLine();
   if(ImGui::Button("Browse NSF...")){
#ifdef _WIN32
    OPENFILENAMEA ofn{};char selected[MAX_PATH]{};ofn.lStructSize=sizeof(ofn);ofn.lpstrFile=selected;ofn.nMaxFile=MAX_PATH;ofn.lpstrFilter="Crash Bandicoot level archives (*.nsf)\0*.nsf;*.NSF\0All files (*.*)\0*.*\0";ofn.nFilterIndex=1;ofn.Flags=OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_EXPLORER;if(GetOpenFileNameA(&ofn))std::snprintf(path_,sizeof(path_),"%s",selected);
#else
    const std::string selected=browseNsfPath();if(!selected.empty()){if(selected.size()<sizeof(path_)){std::snprintf(path_,sizeof(path_),"%s",selected.c_str());status_="Selected NSF: "+selected;}else status_="Selected NSF path is too long.";}else status_="No file selected. If the file picker is unavailable, enter the NSF path manually.";
#endif
   }
   if(ImGui::Button("Continue")){
    std::filesystem::path p=std::filesystem::u8path(path_);
    if(hasExtension(p,".nsf")){versionDialog_=true;centerVersionDialog_=true;openDialog_=false;}
    else status_="Please select an .nsf file.";
   }
   ImGui::SameLine();
   if(ImGui::Button("Cancel"))openDialog_=false;
  }
  ImGui::End();
 }
 if(versionDialog_){
  const bool centerThisFrame=centerVersionDialog_;
  if(centerThisFrame)ImGui::SetNextWindowFocus();
  const bool versionDialogVisible=ImGui::Begin("Choose the game version",&versionDialog_,dialogFlags);
  if(centerThisFrame){
   const ImVec2 windowSize=ImGui::GetWindowSize();
   ImGui::SetWindowPos(ImVec2(dialogCenter.x-windowSize.x*0.5f,dialogCenter.y-windowSize.y*0.5f),ImGuiCond_Always);
   centerVersionDialog_=false;
  }
  if(versionDialogVisible){
   ImGui::TextWrapped("Which Crash game/version does this NSF belong to? This controls the format loaders used for its resources and NSD.");
   const char* names[]={"Crash 1 — 1995 Beta","Crash 1 — March 8 Beta","Crash 1 — May 11 Beta","Crash 1","Crash 2","Crash 3 — May 14 Beta","Crash 3"};
   int choice=static_cast<int>(pendingVersion_);
   if(ImGui::Combo("Game version",&choice,names,7))pendingVersion_=static_cast<GameVersion>(choice);
   if(ImGui::Button("Open Level")){
    std::string e;
    if(loadLevel(std::filesystem::u8path(path_),e))versionDialog_=false;
    else status_="Could not open level: "+e;
   }
   ImGui::SameLine();
   if(ImGui::Button("Back")){versionDialog_=false;openDialog_=true;centerOpenDialog_=true;}
  }
  ImGui::End();
 }
 if(about_){ImGui::OpenPopup("About CrashEdit");about_=false;}if(ImGui::BeginPopupModal("About CrashEdit",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::Text("CrashEdit — cross-platform rewrite");ImGui::Text("C++20 / Dear ImGui / GLFW / OpenGL");if(ImGui::Button("Close"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 static EditorPreferences previousPreferences=editorPreferences;
 if(previousPreferences.uiScaling!=editorPreferences.uiScaling||previousPreferences.useModernFont!=editorPreferences.useModernFont||previousPreferences.softwareRendering!=editorPreferences.softwareRendering||previousPreferences.depthBufferEnabled!=editorPreferences.depthBufferEnabled||previousPreferences.rendererDiagnosticsVisible!=editorPreferences.rendererDiagnosticsVisible||previousPreferences.sceneMoveSpeed!=editorPreferences.sceneMoveSpeed||previousPreferences.animationMoveSpeed!=editorPreferences.animationMoveSpeed||previousPreferences.palRegion!=editorPreferences.palRegion||previousPreferences.animationRefreshRateMode!=editorPreferences.animationRefreshRateMode||previousPreferences.showProperties!=editorPreferences.showProperties||previousPreferences.showConsole!=editorPreferences.showConsole){ImGui::MarkIniSettingsDirty();previousPreferences=editorPreferences;}
}


// Present the conservative IR decompilation using the chosen indentation-based
// source layout. Provenance/debug comments remain available through the Inspector.
static std::string formatGoolIRForScriptView(const std::string& source,std::size_t& hiddenAnnotations){
 std::string output;
 std::size_t indent=0,start=0;
 const auto trim=[](std::string value){
  const auto first=value.find_first_not_of(" \t\r");
  if(first==std::string::npos)return std::string{};
  const auto last=value.find_last_not_of(" \t\r");
  return value.substr(first,last-first+1U);
 };
 while(start<=source.size()){
  const auto newline=source.find('\n',start);
  const auto end=newline==std::string::npos?source.size():newline;
  std::string line=trim(source.substr(start,end-start));
  const auto comment=line.find("//");
  if(comment!=std::string::npos){
   line=trim(line.substr(0,comment));
   ++hiddenAnnotations;
  }
  bool label=false;
  if(!line.empty()){
   bool closes=line.front()=='}';
   if(closes){
    if(indent>0)--indent;
    if(line.rfind("} else if (",0)==0){
     line=trim(line.substr(11U));
     if(line.size()>=2U&&line.compare(line.size()-2U,2U," {")==0)
      line=trim(line.substr(0,line.size()-2U));
     if(!line.empty()&&line.back()==')')line.pop_back();
     line="else if "+trim(line)+":";
    }else if(line.rfind("} else",0)==0){
     line="else:";
    }else if(line=="}"){
     line.clear();
    }else{
     line.erase(line.begin());
     line=trim(line);
    }
   }else if(line.size()>=2U&&line.compare(0,2,"if")==0&&line.back()=='{'){
    line=trim(line.substr(0,line.size()-1U));
    if(line.rfind("if (",0)==0&&line.back()==')'){
     line="if "+line.substr(4U,line.size()-5U)+":";
    }else line+=":";
   }else if(line.size()>=2U&&line.back()=='{' ){
    line=trim(line.substr(0,line.size()-1U));
    if(line=="else")line="else:";
    else if(line.rfind("else if (",0)==0&&line.back()==')')
     line="else if "+line.substr(9U,line.size()-10U)+":";
    else line+=":";
   }
   if(!line.empty()){
    // Keep routine labels flush-left; all executable statements follow the
    // reconstructed block indentation.
    label=line.size()>1U&&line.back()==':'&&line.front()=='L'&&
      std::all_of(line.begin()+1,line.end()-1,[](unsigned char c){return std::isdigit(c)!=0;});
    if(label)indent=0;
    output.append(label?0U:indent*4U,' ');
    output+=line;
    output+='\n';
   }
   if(!closes&&!label&&line.size()>1U&&line.back()==':')++indent;
  }
  if(newline==std::string::npos)break;
  start=newline+1U;
 }
 return output;
}


namespace {
struct GoolDocumentationTopic { const char* title; const char* summary; const char* details; const char* example; };
static const std::array<GoolDocumentationTopic,9> goolDocumentationTopics={{
 {"GOOL Script overview","A readable, best-effort view of the selected GOOL entry.",
  "This reference explains the notation used by the decompiler. The Selected GOOL tab is read-only: it does not edit original GOOL data or recompile source. An operation is shown as source-like syntax only when its meaning is supported by current analysis. Use the Inspector to verify low-level data.",
  "Selected GOOL  →  routine listing  →  source-like preview"},
 {"Routines and labels","Routines are discovered entry points; labels identify instruction destinations.",
  "A routine is a region reached through supported entry sources such as local Code hooks or local calls. Lxxxxx labels identify instruction positions and branch destinations. Routine boundaries and shared-instruction counts are analysis results, not proof that every runtime entry has been discovered.",
  "L00012:\n    if condition:\n        goto L00020"},
 {"Conditions and loops","Structured blocks appear only when the control-flow shape can be proven.",
  "The decompiler may reconstruct if/else, while, do/while, and proven loop exits. If branches are ambiguous, cross routine boundaries, or form unsupported control flow, it keeps explicit labels and branches instead of inventing a high-level structure.",
  "if condition:\n    action()\nelse:\n    fallback()"},
 {"Calls, returns and transitions","Known control operations are rendered with readable control-flow notation.",
  "Calls and returns describe routine control flow. State transitions describe a change to a GOOL state descriptor when the descriptor can be resolved. Unresolved destinations remain explicit diagnostics or references; readable spelling does not mean every runtime side effect is understood.",
  "call RoutineName\nreturn\ntransition StateName"},
 {"Native MIPS","A mips line is a PlayStation CPU instruction embedded in the GOOL stream.",
  "Native MIPS is not a GOOL Script command. The decompiler preserves assembly where supported and records branch destinations when they resolve to exact instruction boundaries. Unsupported or malformed encodings remain represented as raw words.",
  "mips: addu $v0, $a0, $a1"},
 {"Unknown instructions","Unknown operations are retained rather than assigned speculative meanings.",
  "A .gool_word line preserves the original 32-bit GOOL instruction when its semantics are not currently modeled. A .mips_word line preserves a native instruction that could not be decoded. These markers are intentional. Consult the Inspector and evidence-backed opcode audit; do not replace them with guessed behavior.",
  ".gool_word 0x........\n.mips_word 0x........"},
 {"Offsets and raw words","The readable view hides low-level annotations to keep code uncluttered.",
  "The underlying IR/decompiler retains original instruction positions, byte offsets, and raw 32-bit words, along with source relationships. The compact script view suppresses these comments. Use the GOOL Inspector to compare a readable statement with its exact encoded instruction and operands.",
  "@ byte offset  ·  raw= original 32-bit word"},
 {"Diagnostics","Diagnostics describe gaps or uncertainty in analysis, not necessarily bad game data.",
  "A diagnostic can report an unresolved target, unsupported instruction, invalid reference, or control-flow edge that cannot safely be structured. It means the tool could not prove an interpretation. It does not automatically mean the original NSF is corrupt, and an empty diagnostic list does not prove complete semantic understanding.",
  "IR diagnostics: 2  ·  low-level annotations hidden: 14"},
 {"Operations and semantics","An opcode gets source-like notation only after its behavior is established.",
  "The project is building an evidence-backed inventory of GOOL opcodes, operand roles, object/process fields, state and hook references, and native MIPS/GTE behavior. Names and examples in the current preview are not a finalized language specification. Unknown and partially understood operations stay low-level until verified.",
  "Known operation  →  verified IR meaning  →  readable form"}
}};
static int activeGoolDocumentationTopic=0;
static bool documentationTabActive=false;
static int documentationTopicForToken(std::string token){
 std::transform(token.begin(),token.end(),token.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 if(token=="if"||token=="else"||token=="while"||token=="do"||token=="break"||token=="goto"||token=="continue")return 2;
 if(token=="call"||token=="return"||token=="transition"||token=="state")return 3;
 if(token=="mips"||token=="mips_word")return 4;
 if(token=="gool_word"||token=="unknown")return 5;
 if(token=="routine"||token=="event"||(!token.empty()&&token.front()=='l'&&std::all_of(token.begin()+1,token.end(),[](unsigned char ch){return std::isdigit(ch)!=0;})))return 1;
 if(token=="raw"||token=="offset"||token=="byte")return 6;
 if(token=="diagnostic"||token=="diagnostics")return 7;
 return 8;
}
static void drawGoolDocumentationPage(){
 if(activeGoolDocumentationTopic<0||activeGoolDocumentationTopic>=static_cast<int>(goolDocumentationTopics.size()))activeGoolDocumentationTopic=0;
 const auto& topic=goolDocumentationTopics[static_cast<std::size_t>(activeGoolDocumentationTopic)];
 ImGui::TextDisabled("GOOL Script reference");
 ImGui::Separator();
 ImGui::Text("%s",topic.title);
 ImGui::TextWrapped("%s",topic.summary);
 ImGui::Spacing();
 ImGui::Separator();
 ImGui::TextWrapped("%s",topic.details);
 ImGui::Spacing();
 ImGui::TextDisabled("Example / notation");
 ImGui::BeginChild("GoolDocumentationExample",ImVec2(0.0f,72.0f),true);
 ImGui::TextWrapped("%s",topic.example);
 ImGui::EndChild();
 ImGui::Spacing();
 ImGui::TextDisabled("Tip: Ctrl+Click a supported term in the Selected GOOL view to open its documentation topic.");
}
} // namespace

void EditorUI::drawGoolScriptEditor(){

 const Crash::NsfGool* selectedGool=nullptr;
 std::uint32_t selectedEid=0;
 std::string selectedName;
 if(nsfValid_&&selectedChunk_>=0&&selectedEntry_>=0&&
    static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto& chunk=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(chunk.type==0&&static_cast<std::size_t>(selectedEntry_)<chunk.entries.size()){
   const auto& entry=chunk.entries[static_cast<std::size_t>(selectedEntry_)];
   if(entry.valid&&entry.type==11&&entry.gool.decoded){
    selectedGool=&entry.gool;
    selectedEid=entry.eid;
    selectedName=Crash::eidToEName(entry.eid);
   }
  }
 }

 static const Crash::NsfGool* cachedSource=nullptr;
 static int cachedChunk=-1,cachedEntry=-1;
 static Crash::GoolIRProgram program;
 static std::size_t activeRoutine=0;
 static bool programReady=false;
 if(selectedGool&&(cachedSource!=selectedGool||cachedChunk!=selectedChunk_||
    cachedEntry!=selectedEntry_||!programReady)){
  program=Crash::buildGoolIR(*selectedGool);
  cachedSource=selectedGool;cachedChunk=selectedChunk_;cachedEntry=selectedEntry_;
  activeRoutine=0;programReady=true;
 }else if(!selectedGool){
  cachedSource=nullptr;cachedChunk=cachedEntry=-1;program={};
  activeRoutine=0;programReady=false;
 }

 if(!selectedGool)return;
 const ImGuiViewport* viewport=ImGui::GetMainViewport();
 const ImVec2 workSize=viewport->WorkSize;
 ImGui::SetNextWindowSizeConstraints(ImVec2(700.0f,480.0f),workSize);
 ImGui::SetNextWindowSize(ImVec2(std::min(1120.0f,workSize.x*0.92f),
   std::min(760.0f,workSize.y*0.88f)),ImGuiCond_FirstUseEver);
 ImGui::SetNextWindowPos(viewport->GetCenter(),ImGuiCond_FirstUseEver,ImVec2(0.5f,0.5f));
 static bool scriptDockInitialized=false;
 if(!scriptDockInitialized){
  if(ImGuiWindow* viewer=ImGui::FindWindowByName("3D Zone Editor"))
   if(viewer->DockNode){ImGui::SetNextWindowDockID(viewer->DockNode->ID,ImGuiCond_Always);scriptDockInitialized=true;}
 }
 static GoolScriptEditor selectedEditor;
 static GoolScriptEditor scratchEditor;
 static std::string selectedEditorText;
 static std::string lastScratchAnalysisText;
 static Crash::GoolScriptIndentationResult analysis;
 static std::size_t hiddenAnnotations=0;
 static int displayedChunk=-1,displayedEntry=-1;
 static std::size_t displayedRoutine=static_cast<std::size_t>(-1);
 static bool scratchEditorInitialized=false;
 if(!scratchEditorInitialized){scratchEditor.SetText(
  "event Update:\n"
  "    if health <= 0:\n"
  "        transition Dead\n"
  "    else:\n"
  "        call UpdateMovement\n");scratchEditorInitialized=true;}

 if(ImGui::Begin("GOOL Script Editor")){
  ImGui::TextDisabled("GOOL authoring workspace");
  ImGui::SameLine();
  if(ImGui::SmallButton(showGoolScriptNavigator_?"Hide Navigator":"Show Navigator"))
   showGoolScriptNavigator_=!showGoolScriptNavigator_;
  ImGui::Separator();

  const ImVec2 available=ImGui::GetContentRegionAvail();
  if(showGoolScriptNavigator_){
   ImGui::BeginChild("GoolScriptNavigator",ImVec2(190.0f,available.y),true);
   if(documentationTabActive){
    ImGui::TextUnformatted("Documentation");
    ImGui::Separator();
    for(std::size_t i=0;i<goolDocumentationTopics.size();++i)
     if(ImGui::Selectable(goolDocumentationTopics[i].title,activeGoolDocumentationTopic==static_cast<int>(i)))
      activeGoolDocumentationTopic=static_cast<int>(i);
   }else{
    ImGui::TextUnformatted("Document");
    ImGui::Separator();
    if(selectedGool)ImGui::Text("%s",selectedName.c_str());
    else ImGui::TextDisabled("No GOOL selected");
    if(selectedGool){
     ImGui::TextDisabled("EID  %08X",selectedEid);
     ImGui::TextDisabled("%zu instructions",program.instructions.size());
     ImGui::TextDisabled("%zu routines",program.routines.size());
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Outline");
    ImGui::TextDisabled("Events");
    ImGui::TextDisabled("States");
    ImGui::TextDisabled("Routines");
    if(selectedGool){
     for(std::size_t i=0;i<program.routines.size();++i){
      char label[64];std::snprintf(label,sizeof(label),"Routine %zu  ·  L%05zu",i,program.routines[i].entryInstruction);
      if(ImGui::Selectable(label,activeRoutine==i))activeRoutine=i;
     }
    }
   }
   ImGui::EndChild();
   ImGui::SameLine();
  }

  ImGui::BeginChild("GoolScriptEditorSurface",ImVec2(0.0f,available.y),false);
  if(ImGui::BeginTabBar("GoolScriptDocuments")){
   if(ImGui::BeginTabItem("Selected GOOL")){
    documentationTabActive=false;
    ImGui::Text("%s",selectedGool?selectedName.c_str():"No GOOL selected");
    ImGui::SameLine();
    ImGui::TextDisabled("EID %08X",selectedEid);
    ImGui::SameLine();
    ImGui::TextDisabled("IR preview · read-only");
    ImGui::Separator();
    if(selectedGool){
     if(!program.routines.empty()){
      if(activeRoutine>=program.routines.size())activeRoutine=0;
      const auto& routine=program.routines[activeRoutine];
      ImGui::Text("Routine %zu",activeRoutine);
      ImGui::SameLine();
      ImGui::TextDisabled("Entry L%05zu  ·  %zu instructions  ·  %zu shared",
       routine.entryInstruction,routine.instructions.size(),routine.sharedInstructions.size());
      ImGui::SameLine();
      if(ImGui::ArrowButton("##PreviousRoutine",ImGuiDir_Left)&&activeRoutine>0)--activeRoutine;
      ImGui::SameLine();
      if(ImGui::ArrowButton("##NextRoutine",ImGuiDir_Right)&&activeRoutine+1<program.routines.size())++activeRoutine;
      auto decomp=Crash::decompileGoolIRRoutine(program,activeRoutine);
      std::size_t hidden=0;
      const std::string formatted=formatGoolIRForScriptView(decomp.text,hidden);
      if(displayedChunk!=selectedChunk_||displayedEntry!=selectedEntry_||
         displayedRoutine!=activeRoutine||selectedEditorText!=formatted){
       selectedEditor.SetText(formatted);
       selectedEditorText=formatted;
       displayedChunk=selectedChunk_;displayedEntry=selectedEntry_;
       displayedRoutine=activeRoutine;hiddenAnnotations=hidden;
      }
      const float fontCount=static_cast<float>(ImGui::GetIO().Fonts->Fonts.Size);
      if(fontCount>1.0f)ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
      const ImVec2 selectedEditorAvail=ImGui::GetContentRegionAvail();
      const float selectedEditorHeight=std::max(120.0f,selectedEditorAvail.y-42.0f);
      selectedEditor.Render("##SelectedGoolCode",ImVec2(0.0f,selectedEditorHeight),true);
      if(const std::string clickedWord=selectedEditor.TakeCtrlClickedWord();!clickedWord.empty()){
       activeGoolDocumentationTopic=documentationTopicForToken(clickedWord);
       documentationTabActive=true;
      }
      if(fontCount>1.0f)ImGui::PopFont();
      if(!decomp.diagnostics.empty()||hiddenAnnotations>0){
       ImGui::TextDisabled("%zu IR diagnostics  ·  %zu low-level annotations hidden from source view",
        decomp.diagnostics.size(),hiddenAnnotations);
      }
     }else{
      ImGui::TextDisabled("No routine entry points were discovered for this object.");
      auto decomp=Crash::decompileGoolIR(program);
      std::size_t hidden=0;
      const std::string formatted=formatGoolIRForScriptView(decomp.text,hidden);
      if(selectedEditorText!=formatted){selectedEditor.SetText(formatted);selectedEditorText=formatted;}
      const float fontCount=static_cast<float>(ImGui::GetIO().Fonts->Fonts.Size);
      if(fontCount>1.0f)ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
      const ImVec2 selectedEditorAvail=ImGui::GetContentRegionAvail();
      const float selectedEditorHeight=std::max(120.0f,selectedEditorAvail.y-42.0f);
      selectedEditor.Render("##SelectedGoolCode",ImVec2(0.0f,selectedEditorHeight),true);
      if(const std::string clickedWord=selectedEditor.TakeCtrlClickedWord();!clickedWord.empty()){
       activeGoolDocumentationTopic=documentationTopicForToken(clickedWord);
       documentationTabActive=true;
      }
      if(fontCount>1.0f)ImGui::PopFont();
     }
    }else{
     ImGui::TextWrapped("Select a decoded GOOL entry in the Resource Hierarchy to inspect its IR-backed decompilation.");
    }
    ImGui::EndTabItem();
   }
   if(ImGui::BeginTabItem("Scratch Script")){
    documentationTabActive=false;
    ImGui::TextDisabled("Draft source · indentation checked as you type");
    const float diagnosticsHeight=112.0f;
    const ImVec2 editorAvail=ImGui::GetContentRegionAvail();
    const float editorHeight=std::max(140.0f,editorAvail.y-diagnosticsHeight);
    const float fontCount=static_cast<float>(ImGui::GetIO().Fonts->Fonts.Size);
    if(fontCount>1.0f)ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
    scratchEditor.Render("##ScratchGoolCode",ImVec2(0.0f,editorHeight),false);
    if(fontCount>1.0f)ImGui::PopFont();
    const std::string scratchText=scratchEditor.GetText();
    if(scratchText!=lastScratchAnalysisText){
     analysis=Crash::analyzeGoolScript(scratchText);
     lastScratchAnalysisText=scratchText;
         }
    ImGui::Separator();
    ImGui::Text("Script diagnostics  ·  %zu error(s)",analysis.diagnostics.size());
    ImGui::BeginChild("GoolScriptProblems",ImVec2(0.0f,0.0f),false,
      ImGuiWindowFlags_HorizontalScrollbar);
    if(analysis.diagnostics.empty())ImGui::TextDisabled("No script errors detected.");
    else for(const auto& diagnostic:analysis.diagnostics){
     ImGui::PushID(static_cast<int>(diagnostic.line));
     ImGui::TextColored(ImVec4(1.0f,0.42f,0.36f,1.0f),
       "Error %s  ·  line %zu, column %zu",diagnostic.code.c_str(),
       diagnostic.line,diagnostic.column);
     ImGui::SameLine();ImGui::TextWrapped("%s",diagnostic.message.c_str());
     ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
   }
   if(ImGui::BeginTabItem("Documentation",nullptr,documentationTabActive?ImGuiTabItemFlags_SetSelected:0)){
    documentationTabActive=true;
    drawGoolDocumentationPage();
    ImGui::EndTabItem();
   }
   ImGui::EndTabBar();
  }
  ImGui::EndChild();
 }
 ImGui::End();
}

void EditorUI::drawMenu(){
 int& uiScaling=editorPreferences.uiScaling;
 static int appliedScaling=100;
 static ImGuiStyle baseStyle=ImGui::GetStyle();
 auto&io=ImGui::GetIO();
 if(uiScaling!=appliedScaling){
  ImGui::GetStyle()=baseStyle;
  ImGui::GetStyle().ScaleAllSizes(static_cast<float>(uiScaling)/100.0f);
  appliedScaling=uiScaling;
 }
 io.FontGlobalScale=static_cast<float>(uiScaling)/100.0f;
 io.FontDefault=(editorPreferences.useModernFont&&io.Fonts->Fonts.Size>1)?io.Fonts->Fonts[1]:nullptr;
 if(!ImGui::BeginMenuBar())return;
 if(ImGui::BeginMenu("File")){
  if(ImGui::MenuItem("Open NSF...","Ctrl+O")){openDialog_=true;centerOpenDialog_=true;}
  if(ImGui::MenuItem("Save","Ctrl+S",false,nsfValid_&&nsdValid_))saveFiles();
  if(ImGui::MenuItem("Close Workspace",nullptr,false,workspace_.isOpen())){workspace_.close();nsfValid_=nsdValid_=false;status_="Workspace closed.";}
  ImGui::Separator();if(ImGui::MenuItem("Exit"))std::exit(0);ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Edit")){ImGui::MenuItem("Undo","Ctrl+Z",false,false);ImGui::MenuItem("Redo","Ctrl+Y",false,false);ImGui::EndMenu();}
 if(ImGui::BeginMenu("Search")){if(ImGui::MenuItem("Find Entry...","Ctrl+F",false,nsfValid_))searchWindow_=true;ImGui::EndMenu();}
 if(ImGui::BeginMenu("View")){ImGui::MenuItem("Properties",nullptr,&editorPreferences.showProperties);ImGui::MenuItem("Console / Diagnostics",nullptr,&editorPreferences.showConsole);ImGui::EndMenu();}
 if(ImGui::BeginMenu("Experimental")){
  if(ImGui::MenuItem("Traditional GOOL Inspection",nullptr,&editorPreferences.traditionalGoolInspection)){activeGoolChunk_=-1;activeGoolEntry_=-1;ImGui::MarkIniSettingsDirty();}
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Use the original GOOL inspection windows instead of the Script Editor and Blueprint workspace.");
  if(ImGui::MenuItem("Create .bak backups during save",nullptr,&editorPreferences.createBakBackup))ImGui::MarkIniSettingsDirty();
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Before saving, copy the previous NSF and NSD to matching .bak files.");
  if(ImGui::MenuItem("Create dated backups during save",nullptr,&editorPreferences.createDatedBackup))ImGui::MarkIniSettingsDirty();
  if(ImGui::IsItemHovered())ImGui::SetTooltip("After preparing the new save, archive the new NSF and NSD using today’s date.");
  bool& useModernFont=editorPreferences.useModernFont;
  if(ImGui::MenuItem("Use modern UI font",nullptr,&useModernFont))io.FontDefault=(useModernFont&&io.Fonts->Fonts.Size>1)?io.Fonts->Fonts[1]:nullptr;
  ImGui::SetNextItemWidth(190.0f*static_cast<float>(uiScaling)/100.0f);
  ImGui::SliderInt("UI Scaling",&uiScaling,100,200,"%d%%");
  ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Help")){if(ImGui::MenuItem("About"))about_=true;ImGui::EndMenu();}
 ImGui::EndMenuBar();
}
void EditorUI::drawToolbar(){const auto&style=ImGui::GetStyle();const float toolbarHeight=ImGui::GetFrameHeight()+style.WindowPadding.y*2.0f+2.0f;ImGui::BeginChild("Toolbar",ImVec2(0,toolbarHeight),ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar);if(ImGui::Button("Open NSF")){openDialog_=true;centerOpenDialog_=true;}ImGui::SameLine();ImGui::BeginDisabled(!nsfValid_||!nsdValid_);if(ImGui::Button("Save"))saveFiles();ImGui::EndDisabled();ImGui::SameLine();ImGui::SameLine();ImGui::BeginDisabled(!nsfValid_);if(ImGui::Button("Search"))searchWindow_=true;ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("| Game: %s",nsfValid_?versionName(gameVersion_):"None");ImGui::SameLine();ImGui::SetNextItemWidth(82.0f);int regionChoice=editorPreferences.palRegion?1:0;const char*regionNames[]={"NTSC","PAL"};if(ImGui::Combo("##NSFRegion",&regionChoice,regionNames,2)){editorPreferences.palRegion=regionChoice==1;animationFrameTimer_=0.0f;}ImGui::EndChild();}
void EditorUI::drawHierarchy(){ImGui::Begin("Resource Hierarchy");
 auto resourceDefaultFilename=[&](int kind,int chunkIndex,int entryIndex,int itemIndex){
  std::string name=kind==0?"chunk_"+std::to_string(chunkIndex):kind==1?"entry_"+std::to_string(chunkIndex)+"_"+std::to_string(entryIndex):"item_"+std::to_string(chunkIndex)+"_"+std::to_string(entryIndex)+"_"+std::to_string(itemIndex);
  if(kind!=0&&chunkIndex>=0&&static_cast<std::size_t>(chunkIndex)<nsf_.chunks().size()){
   const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(chunkIndex)];
   if(entryIndex>=0&&static_cast<std::size_t>(entryIndex)<chunk.entries.size()){
    const auto eid=chunk.entries[static_cast<std::size_t>(entryIndex)].eid;
    name+="_"+(Crash::isValidEid(eid)?Crash::eidToEName(eid):std::string("UnknownEID"));
   }
  }
  return name+".bin";
 };
 auto drawResourceContext=[&](int kind,int chunkIndex,int entryIndex,int itemIndex,const char* title){
  if(ImGui::IsItemClicked(ImGuiMouseButton_Right)){
   selectedChunk_=chunkIndex;selectedEntry_=entryIndex;selectedItem_=itemIndex;selectedEntity_=-1;selectedSpawn_=-1;
   if(kind==0){selectedEntry_=-1;selectedItem_=-1;}
   if(kind==1)selectedItem_=-1;
   resourceTargetKind_=kind;resourceTargetChunk_=chunkIndex;resourceTargetEntry_=entryIndex;resourceTargetItem_=itemIndex;
  }
  char popupId[96];std::snprintf(popupId,sizeof(popupId),"HierarchyResourceContext_%d_%d_%d_%d",kind,chunkIndex,entryIndex,itemIndex);
  if(ImGui::BeginPopupContextItem(popupId)){
   ImGui::TextUnformatted(title);ImGui::Separator();
   if(ImGui::MenuItem("Export raw bytes...")){
    resourceAction_=1;resourceTargetKind_=kind;resourceTargetChunk_=chunkIndex;resourceTargetEntry_=entryIndex;resourceTargetItem_=itemIndex;
    const std::string name=resourceDefaultFilename(kind,chunkIndex,entryIndex,itemIndex);
    std::snprintf(resourceActionPath_,sizeof(resourceActionPath_),"%s",name.c_str());
   }
   if(ImGui::MenuItem("Import / replace raw bytes...")){
    resourceAction_=2;resourceTargetKind_=kind;resourceTargetChunk_=chunkIndex;resourceTargetEntry_=entryIndex;resourceTargetItem_=itemIndex;
    const std::string name=kind==0?"chunk_"+std::to_string(chunkIndex)+".bin":kind==1?"entry_"+std::to_string(chunkIndex)+"_"+std::to_string(entryIndex)+".bin":"item_"+std::to_string(chunkIndex)+"_"+std::to_string(entryIndex)+"_"+std::to_string(itemIndex)+".bin";
    std::snprintf(resourceActionPath_,sizeof(resourceActionPath_),"%s",name.c_str());
   }
   ImGui::EndPopup();
  }
 };
 auto drawEntryItems=[&](const Crash::NsfEntry& entry,int chunkIndex,int entryIndex){
  static const char* goolNames[]={"GOOL Header","GOOL Instructions","GOOL Data Pool","GOOL State Map","GOOL State Descriptors","GOOL Frame Group"};
  for(std::size_t itemIndex=0;itemIndex<entry.items.size();++itemIndex){
   char itemLabel[128];const char* itemName=entry.type==11&&itemIndex<6?goolNames[itemIndex]:"Item";
   std::snprintf(itemLabel,sizeof(itemLabel),"%s %zu | %zu bytes",itemName,itemIndex,entry.items[itemIndex].size);
   ImGui::PushID(static_cast<int>(itemIndex));
   const bool itemSelected=selectedChunk_==chunkIndex&&selectedEntry_==entryIndex&&selectedItem_==static_cast<int>(itemIndex);
   if(hierarchyIconRow(itemLabel,itemSelected,"Arrow")){selectedChunk_=chunkIndex;selectedEntry_=entryIndex;selectedItem_=static_cast<int>(itemIndex);selectedEntity_=-1;selectedSpawn_=-1;}
   drawResourceContext(2,chunkIndex,entryIndex,static_cast<int>(itemIndex),"Entry Item");
   ImGui::PopID();
  }
 };

if(ImGui::Button("Open NSF...")){openDialog_=true;centerOpenDialog_=true;}ImGui::Separator();ImGui::BeginChild("HierarchyScrollRegion",ImVec2(0,0),false,ImGuiWindowFlags_HorizontalScrollbar);if(nsfValid_){char nsdLabel[96];std::snprintf(nsdLabel,sizeof(nsdLabel),"NSD - Level ID : %02X",static_cast<unsigned>(nsd_.id)&0xFFU);if(nsdValid_){if(ImGui::Selectable(nsdLabel,selectedChunk_==-2,ImGuiSelectableFlags_SpanAvailWidth)){selectedChunk_=-2;selectedSpawn_=-1;selectedEntry_=-1;selectedEntity_=-1;spawnPointsEditor_=true;}}else ImGui::TextDisabled("NSD - Level Details (unavailable)");if(openSelectedHierarchy_)ImGui::SetNextItemOpen(true,ImGuiCond_Always);if(ImGui::TreeNode("NSF - Resources##nsf")){if(ImGui::IsItemClicked()){selectedChunk_=-1;selectedEntry_=-1;selectedEntity_=-1;selectedSpawn_=-1;}for(std::size_t i=0;i<nsf_.chunks().size();++i){const auto&c=nsf_.chunks()[i];char label[192];if(c.type==1){const auto textureEid=static_cast<std::uint32_t>(c.chunkId);const auto textureName=Crash::eidToEName(textureEid);std::snprintf(label,sizeof(label),"Chunk %03zu | EID %s | %s",i,textureName.c_str(),chunkTypeName(c.type));}else{std::snprintf(label,sizeof(label),"Chunk %03zu | %s",i,chunkTypeName(c.type));}if(openSelectedHierarchy_&&selectedChunk_==static_cast<int>(i))ImGui::SetNextItemOpen(true,ImGuiCond_Always);bool opened=treeNodeWithIcon(label,ImGuiTreeNodeFlags_SpanAvailWidth,chunkIconName(c.type));if(ImGui::IsItemClicked()){selectedChunk_=static_cast<int>(i);selectedEntry_=-1;selectedItem_=-1;selectedSpawn_=-1;}drawResourceContext(0,static_cast<int>(i),-1,-1,"Chunk");if(opened){if(c.type==0){if(!c.entryTableValid)ImGui::TextDisabled("%s",c.entryTableError.empty()?"Entry table unavailable":c.entryTableError.c_str());else for(std::size_t j=0;j<c.entries.size();++j){const auto&e=c.entries[j];char entryLabel[192];if(e.valid&&e.type==11)std::snprintf(entryLabel,sizeof(entryLabel),"Entry %03zu | GOOL | %s",j,Crash::eidToEName(e.eid).c_str());else if(e.valid)std::snprintf(entryLabel,sizeof(entryLabel),"Entry %03zu | %s | Type %u | Items %u",j,Crash::eidToEName(e.eid).c_str(),e.type,e.itemCount);else std::snprintf(entryLabel,sizeof(entryLabel),"Entry %03zu | Invalid",j);if(openSelectedHierarchy_&&selectedChunk_==static_cast<int>(i)&&selectedEntry_==static_cast<int>(j))ImGui::SetNextItemOpen(true,ImGuiCond_Always);bool entryOpen=treeNodeWithIcon(entryLabel,ImGuiTreeNodeFlags_SpanAvailWidth,entryIconName(e.type));if(openSelectedHierarchy_&&selectedChunk_==static_cast<int>(i)&&selectedEntry_==static_cast<int>(j))ImGui::SetScrollHereY(0.5f);if(ImGui::IsItemClicked()){selectedChunk_=static_cast<int>(i);selectedEntry_=static_cast<int>(j);selectedItem_=-1;selectedEntity_=-1;selectedSpawn_=-1;if(e.valid&&((e.type==7&&e.zone.decoded)||(e.type==3&&e.scenery.decoded))){viewerChunk_=static_cast<int>(i);viewerEntry_=static_cast<int>(j);}}drawResourceContext(1,static_cast<int>(i),static_cast<int>(j),-1,e.type==11?"GOOL Entry":"Entry");if(entryOpen){if(e.type==11){if(e.gool.decoded)ImGui::BulletText("GOOL | %zu instructions | %zu data words",e.gool.instructions.size(),e.gool.dataPool.size());else ImGui::TextDisabled("GOOL not decoded: %s",e.gool.error.c_str());drawEntryItems(e,static_cast<int>(i),static_cast<int>(j));}else if(e.type==7){if(e.zone.decoded){ImGui::BulletText("Zone Header | %d entities | %d cameras",e.zone.entityCount,e.zone.cameraCount);ImGui::BulletText("Layout | %d x %d x %d",e.zone.width,e.zone.height,e.zone.depth);for(std::size_t k=0;k<e.zone.entities.size();++k){const auto&ent=e.zone.entities[k];char entLabel[128];if(ent.hasCameraIndex&&ent.hasCameraSubIndex)std::snprintf(entLabel,sizeof(entLabel),"Camera [%d]-%d",ent.cameraIndex,ent.cameraSubIndex);else if(!ent.name.empty()&&ent.hasId)std::snprintf(entLabel,sizeof(entLabel),"%s [ID %d]",ent.name.c_str(),ent.id);else if(ent.hasId)std::snprintf(entLabel,sizeof(entLabel),"Entity [ID %d]",ent.id);else std::snprintf(entLabel,sizeof(entLabel),"Entity %03zu",k);{ImGui::PushID(static_cast<int>(k));const bool entitySelected=selectedChunk_==static_cast<int>(i)&&selectedEntry_==static_cast<int>(j)&&selectedEntity_==static_cast<int>(k);if(hierarchyIconRow(entLabel,entitySelected,"Arrow")){selectedChunk_=static_cast<int>(i);selectedEntry_=static_cast<int>(j);selectedEntity_=static_cast<int>(k);selectedSpawn_=-1;}ImGui::PopID();}}}else ImGui::TextDisabled("Zone not decoded: %s",e.zone.error.c_str());}else{for(std::size_t k=0;k<e.items.size();++k){char itemLabel[96];std::snprintf(itemLabel,sizeof(itemLabel),"Item %zu | %zu bytes",k,e.items[k].size);ImGui::PushID(static_cast<int>(k));const bool itemSelected=selectedChunk_==static_cast<int>(i)&&selectedEntry_==static_cast<int>(j)&&selectedItem_==static_cast<int>(k);if(hierarchyIconRow(itemLabel,itemSelected,"Arrow")){selectedChunk_=static_cast<int>(i);selectedEntry_=static_cast<int>(j);selectedItem_=static_cast<int>(k);selectedEntity_=-1;selectedSpawn_=-1;}drawResourceContext(2,static_cast<int>(i),static_cast<int>(j),static_cast<int>(k),"Entry Item");ImGui::PopID();}}ImGui::TreePop();}}}ImGui::TreePop();}}ImGui::TreePop();}}else if(!workspace_.isOpen())ImGui::TextDisabled("No NSF archive loaded.");else for(const auto&n:workspace_.resources())drawNode(n);openSelectedHierarchy_=false;ImGui::EndChild();ImGui::End();}
void EditorUI::drawResourceActionDialog(){
 if(resourceAction_==0)return;
 const bool importing=resourceAction_==2;
 const char* title=importing?"Import / Replace Resource":"Export Resource";
 const ImGuiWindowFlags flags=ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings;
 ImGui::OpenPopup(title);
 if(ImGui::BeginPopupModal(title,nullptr,flags)){
  ImGui::TextWrapped(importing?"The replacement must have exactly the same byte length as the selected resource.":"Exports the selected resource's original bytes without decoding or rebuilding it.");
  ImGui::SetNextItemWidth(420.0f);ImGui::InputText("File path",resourceActionPath_,sizeof(resourceActionPath_));
  ImGui::SameLine();
  if(ImGui::Button("Browse...")){
#ifdef _WIN32
   char selected[MAX_PATH]{};OPENFILENAMEA ofn{};ofn.lStructSize=sizeof(ofn);ofn.lpstrFile=selected;ofn.nMaxFile=MAX_PATH;
   ofn.lpstrFilter="Binary files (*.bin)\0*.bin\0All files (*.*)\0*.*\0";ofn.Flags=OFN_PATHMUSTEXIST|OFN_EXPLORER|(importing?OFN_FILEMUSTEXIST:0);
   const BOOL ok=importing?GetOpenFileNameA(&ofn):GetSaveFileNameA(&ofn);
   if(ok)std::snprintf(resourceActionPath_,sizeof(resourceActionPath_),"%s",selected);
#else
   status_="Enter the file path manually; native file browsing is not yet available on this platform.";
#endif
  }
  if(ImGui::Button(importing?"Replace":"Export")){
   const auto&chunks=nsf_.chunks();
   if(resourceTargetChunk_<0||static_cast<std::size_t>(resourceTargetChunk_)>=chunks.size()){status_="Resource export/import failed: chunk selection is no longer valid.";resourceAction_=0;}
   else{
    const auto&chunk=chunks[static_cast<std::size_t>(resourceTargetChunk_)];
    std::size_t offset=0,size=0;bool valid=true;
    if(resourceTargetKind_==0){size=chunk.bytes.size();}
    else if(resourceTargetEntry_<0||static_cast<std::size_t>(resourceTargetEntry_)>=chunk.entries.size()){valid=false;}
    else if(resourceTargetKind_==1){const auto&e=chunk.entries[static_cast<std::size_t>(resourceTargetEntry_)];valid=e.valid;offset=e.offset;size=e.size;}
    else{const auto&e=chunk.entries[static_cast<std::size_t>(resourceTargetEntry_)];if(resourceTargetItem_<0||static_cast<std::size_t>(resourceTargetItem_)>=e.items.size())valid=false;else{const auto&item=e.items[static_cast<std::size_t>(resourceTargetItem_)];offset=item.offset;size=item.size;}}
    if(!valid||offset>chunk.bytes.size()||size>chunk.bytes.size()-offset){status_="Resource export/import failed: selected byte range is invalid.";resourceAction_=0;}
    else if(resourceActionPath_[0]=='\0')status_="Enter a file path first.";
    else if(!importing){
     std::ofstream out(std::filesystem::u8path(resourceActionPath_),std::ios::binary|std::ios::trunc);
     if(!out)status_="Could not create export file.";
     else{out.write(reinterpret_cast<const char*>(chunk.bytes.data()+offset),static_cast<std::streamsize>(size));if(!out)status_="Failed writing export file.";else status_="Exported "+std::to_string(size)+" raw bytes to "+resourceActionPath_;}
     if(out)resourceAction_=0;
    }else{
     std::ifstream in(std::filesystem::u8path(resourceActionPath_),std::ios::binary|std::ios::ate);
     if(!in)status_="Could not open import file.";
     else{
      const auto end=in.tellg();
      if(end<0||static_cast<std::uintmax_t>(end)!=size)status_="Import rejected: replacement size must exactly match the selected resource ("+std::to_string(size)+" bytes).";
      else{
       std::vector<std::uint8_t> replacement(size);in.seekg(0);
       if(size&&!in.read(reinterpret_cast<char*>(replacement.data()),static_cast<std::streamsize>(size)))status_="Could not read the complete import file.";
       else{
        std::vector<std::uint8_t> archiveBytes;std::string error;
        if(!nsf_.saveUncompressedBytes(archiveBytes,error))status_="Import failed while preparing NSF: "+error;
        else{
         const std::size_t absolute=static_cast<std::size_t>(resourceTargetChunk_)*Crash::NsfChunkSize+offset;
         if(absolute>archiveBytes.size()||size>archiveBytes.size()-absolute)status_="Import rejected: target range is outside the serialized archive.";
         else{
          std::copy(replacement.begin(),replacement.end(),archiveBytes.begin()+static_cast<std::ptrdiff_t>(absolute));
          if(!nsf_.loadBytes(archiveBytes,error))status_="Import failed while reparsing NSF: "+error;
          else{if(gameVersion_==GameVersion::Crash2||gameVersion_==GameVersion::Crash3BetaMAY14||gameVersion_==GameVersion::Crash3)nsf_.decodeZones(gameVersion_==GameVersion::Crash3);nsf_.decodeGools(nsfGoolVersion(gameVersion_));status_="Replaced resource bytes successfully. Review the entry before saving.";}
         }
        }
       }
      }
      if(status_.find("Import rejected")!=0&&status_.find("Import failed")!=0)resourceAction_=0;
     }
    }
   }
  }
  ImGui::SameLine();if(ImGui::Button("Cancel"))resourceAction_=0;
  ImGui::EndPopup();
 }
}
void EditorUI::drawNode(const ResourceNode&n){std::string label=(n.isDirectory?"[+] ":"    ")+n.name;if(n.isDirectory){bool open=ImGui::TreeNodeEx(label.c_str(),ImGuiTreeNodeFlags_SpanAvailWidth|(n.children.empty()?ImGuiTreeNodeFlags_Leaf:0));if(ImGui::IsItemClicked())workspace_.selectResource(n.path.string());if(open){for(const auto&c:n.children)drawNode(c);ImGui::TreePop();}}else if(ImGui::Selectable(label.c_str(),workspace_.selectedResource()==n.path.string(),ImGuiSelectableFlags_SpanAvailWidth)){workspace_.selectResource(n.path.string());if(n.path.extension()==".nsf"||n.path.extension()==".NSF"){std::snprintf(path_,sizeof(path_),"%s",n.path.string().c_str());openDialog_=true;centerOpenDialog_=true;}else status_="Selected: "+n.name+" ("+n.kind+")";}}
void EditorUI::drawViewport(){
 ImGui::Begin("3D Zone Editor");
 animationDockId_=ImGui::GetWindowDockID();
 ImGui::TextDisabled("Scenery / Zone viewer | Left drag: orbit | Wheel: zoom | WASD/QE: move | Shift: world-aligned | Viewport options: top-left button");
 const ImVec2 canvasMin=ImGui::GetCursorScreenPos();
 ImVec2 canvasSize=ImGui::GetContentRegionAvail();
 // Never make the interaction canvas larger than the space actually available.
 canvasSize.x=std::max(canvasSize.x,1.0f);canvasSize.y=std::max(canvasSize.y,1.0f);
 // Submit the toolbar hit target before the full-canvas input item. The canvas
 // explicitly allows the preceding toolbar item to overlap it, giving the
 // small control priority without changing camera gesture handling.
 const ImVec2 toolbarMin(canvasMin.x+8.0f,canvasMin.y+8.0f);
 ImGui::SetCursorScreenPos(toolbarMin);
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(6.0f,4.0f));
 ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,3.0f);
 const bool toolbarButton=ImGui::InvisibleButton("##ViewportToolbarMenu",ImVec2(28.0f,26.0f));
 const ImVec2 toolbarMax=ImGui::GetItemRectMax();
 const bool toolbarHovered=ImGui::IsItemHovered();
 if(toolbarHovered)ImGui::SetTooltip("Viewport settings");
 if(toolbarButton)ImGui::OpenPopup("ViewportOptions");
 if(ImGui::BeginPopup("ViewportOptions")){
  ImGui::TextUnformatted("Renderer");
  ImGui::Checkbox("Software rendering",&editorPreferences.softwareRendering);
  ImGui::BeginDisabled(editorPreferences.softwareRendering);
  ImGui::Checkbox("Enable depth buffer",&editorPreferences.depthBufferEnabled);
  ImGui::EndDisabled();
  ImGui::Checkbox("Show performance diagnostics",&editorPreferences.rendererDiagnosticsVisible);
  ImGui::Separator();
  ImGui::TextUnformatted("Camera movement speed");
  ImGui::SetNextItemWidth(180.0f);
  ImGui::SliderInt("##MovementSpeed",&editorPreferences.sceneMoveSpeed,100,300,"%d%%");
  ImGui::EndPopup();
 }
 const bool viewportToolbarOwnsMouse=toolbarHovered||ImGui::IsPopupOpen("ViewportOptions");
 ImGui::PopStyleVar(2);
 ImGui::SetCursorScreenPos(canvasMin);
 ImGui::SetNextItemAllowOverlap();
 ImGui::InvisibleButton("ZoneViewportCanvas",canvasSize,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
 const bool canvasHovered=ImGui::IsItemHovered();
 const bool canvasActive=ImGui::IsItemActive();
 ImDrawList*dl=ImGui::GetWindowDrawList();
 const ImVec2 canvasMax(canvasMin.x+canvasSize.x,canvasMin.y+canvasSize.y);
 dl->AddRectFilled(canvasMin,canvasMax,IM_COL32(20,25,33,255));
 // All projected geometry, markers, and labels must stay inside this panel.
 dl->PushClipRect(canvasMin,canvasMax,true);
 const Crash::NsfZone*zone=nullptr;const Crash::NsfEntry*zoneEntry=nullptr;
 const Crash::NsfScenery*standaloneScenery=nullptr;const Crash::NsfEntry*sceneryEntry=nullptr;
 // The 3D preview has its own resource target. Hierarchy selection may move
 // to sounds, textures, or other resources without closing the active preview.
 if(nsfValid_&&viewerChunk_>=0&&static_cast<std::size_t>(viewerChunk_)<nsf_.chunks().size()){
  const auto&ch=nsf_.chunks()[static_cast<std::size_t>(viewerChunk_)];
  if(ch.type==0&&viewerEntry_>=0&&static_cast<std::size_t>(viewerEntry_)<ch.entries.size()){
   const auto&e=ch.entries[static_cast<std::size_t>(viewerEntry_)];
   if(e.valid&&e.type==7&&e.zone.decoded){zone=&e.zone;zoneEntry=&e;}
   else if(e.valid&&e.type==3&&e.scenery.decoded){standaloneScenery=&e.scenery;sceneryEntry=&e;}
  }
 }
 const auto sceneBuildStart=std::chrono::steady_clock::now();
 std::vector<const Crash::NsfEntry*> sceneryEntries;
 std::vector<const Crash::NsfEntry*> neighborZoneEntries;
 if(standaloneScenery&&sceneryEntry)sceneryEntries.push_back(sceneryEntry);
 if(zone){
  // The original ZoneEntryViewer renders scenery from linked zones, not from
  // the selected zone's own world list. Each linked zone contributes its worlds.
  const auto&chunks=nsf_.chunks();
  for(std::size_t zi=0;zi<static_cast<std::size_t>(zone->zoneCount)&&zi<zone->zones.size();++zi){
   const std::uint32_t linkedZoneEid=static_cast<std::uint32_t>(zone->zones[zi]);
   for(const auto&zoneChunk:chunks){
    if(zoneChunk.type!=0)continue;
    for(const auto&linkedZoneEntry:zoneChunk.entries){
     if(!linkedZoneEntry.valid||linkedZoneEntry.type!=7||linkedZoneEntry.eid!=linkedZoneEid||!linkedZoneEntry.zone.decoded)continue;
     const auto&linkedZone=linkedZoneEntry.zone;
     if(std::find(neighborZoneEntries.begin(),neighborZoneEntries.end(),&linkedZoneEntry)==neighborZoneEntries.end())
      neighborZoneEntries.push_back(&linkedZoneEntry);
     for(std::size_t wi=0;wi<static_cast<std::size_t>(linkedZone.worldCount)&&wi<linkedZone.worlds.size();++wi){
      const std::uint32_t worldEid=static_cast<std::uint32_t>(linkedZone.worlds[wi]);
      for(const auto&worldChunk:chunks){
       if(worldChunk.type!=0)continue;
       for(const auto&worldEntry:worldChunk.entries)
        if(worldEntry.valid&&worldEntry.type==3&&worldEntry.eid==worldEid&&worldEntry.scenery.decoded&&
           std::find(sceneryEntries.begin(),sceneryEntries.end(),&worldEntry)==sceneryEntries.end())
         sceneryEntries.push_back(&worldEntry);
      }
     }
    }
   }
  }
 }
 struct V3{float x=0,y=0,z=0;};
 struct Projected{float x=0,y=0,depth=0;};
 struct Face{Projected p[3];ImU32 colors[3]{};ImVec2 uv[3]{};const std::vector<std::uint8_t>*page=nullptr;int colorMode=0,blendMode=0,clutX=0,clutY=0;float depth=0;bool sky=false;};
 std::vector<Face>faces;
 std::size_t sceneryCount=0,vertexCount=0,faceCount=0,texturedFaceCount=0,resolvedTexturePageCount=0,missingTexturePageCount=0;
 V3 boundsMin{FLT_MAX,FLT_MAX,FLT_MAX},boundsMax{-FLT_MAX,-FLT_MAX,-FLT_MAX};
 V3 frameMin{FLT_MAX,FLT_MAX,FLT_MAX},frameMax{-FLT_MAX,-FLT_MAX,-FLT_MAX};
 auto includePoint=[&](V3 p){boundsMin.x=std::min(boundsMin.x,p.x);boundsMin.y=std::min(boundsMin.y,p.y);boundsMin.z=std::min(boundsMin.z,p.z);boundsMax.x=std::max(boundsMax.x,p.x);boundsMax.y=std::max(boundsMax.y,p.y);boundsMax.z=std::max(boundsMax.z,p.z);};
 auto includeFramePoint=[&](V3 p){frameMin.x=std::min(frameMin.x,p.x);frameMin.y=std::min(frameMin.y,p.y);frameMin.z=std::min(frameMin.z,p.z);frameMax.x=std::max(frameMax.x,p.x);frameMax.y=std::max(frameMax.y,p.y);frameMax.z=std::max(frameMax.z,p.z);};
 for(const auto*entry:sceneryEntries){
  const auto&sc=entry->scenery;++sceneryCount;vertexCount+=sc.vertices.size();
  // Scenery offsets are already world-space offsets; do not add zone XYZ.
  if(!sc.isSky)for(const auto&v:sc.vertices)includePoint({(float(v.x)*16.0f+float(sc.xOffset))/400.0f,(float(v.y)*16.0f+float(sc.yOffset))/400.0f,(float(v.z)*16.0f+float(sc.zOffset))/400.0f});
 }
 if(zone){
  // Match ZoneEntryViewer.CorePositions: frame the selected zone, its cameras,
  // and its entities. Linked scenery can span neighboring worlds and must not
  // pull the camera target away from the selected zone.
  const V3 origin{float(zone->x)/400.0f,float(zone->y)/400.0f,float(zone->z)/400.0f};
  includeFramePoint(origin);
  includeFramePoint({origin.x+float(zone->width)/400.0f,origin.y+float(zone->height)/400.0f,origin.z+float(zone->depth)/400.0f});
  for(std::size_t i=0;i<zone->entities.size();++i){
   const auto&entity=zone->entities[i];
   if(i<static_cast<std::size_t>(zone->cameraCount)&&i%3!=0)continue;
   const float scale=i<static_cast<std::size_t>(zone->cameraCount)?400.0f:100.0f;
   for(const auto&p:entity.positions)includeFramePoint({origin.x+float(p.x)/scale,origin.y+float(p.y)/scale,origin.z+float(p.z)/scale});
  }
 }else{
  frameMin=boundsMin;frameMax=boundsMax;
 }
 const bool cameraTargetChanged=viewerChunk_!=sceneCameraChunk_||viewerEntry_!=sceneCameraEntry_;
 if(!sceneryEntries.empty()&&sceneryCount>0){
  if(cameraTargetChanged){sceneCameraInitialized_=false;sceneYaw_=0.0f;scenePitch_=0.261799f;}
  if(!sceneCameraInitialized_&&frameMin.x!=FLT_MAX){
   sceneTargetX_=(frameMin.x+frameMax.x)*0.5f;sceneTargetY_=(frameMin.y+frameMax.y)*0.5f;sceneTargetZ_=(frameMin.z+frameMax.z)*0.5f;
   const float dx=frameMax.x-frameMin.x,dy=frameMax.y-frameMin.y,dz=frameMax.z-frameMin.z;
   const float radius=std::sqrt(dx*dx+dy*dy+dz*dz)*0.5f;
   sceneDistance_=std::clamp(radius*1.65f+0.75f,2.0f,80.0f);sceneCameraChunk_=viewerChunk_;sceneCameraEntry_=viewerEntry_;sceneCameraInitialized_=true;
  }
 }
 const ImGuiIO&io=ImGui::GetIO();
 if(canvasActive&&!viewportToolbarOwnsMouse&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
  sceneYaw_-=io.MouseDelta.x*0.005f;
  scenePitch_=std::clamp(scenePitch_+io.MouseDelta.y*0.005f,-1.45f,1.45f);
 }
 if(canvasActive&&!viewportToolbarOwnsMouse&&ImGui::IsMouseDragging(ImGuiMouseButton_Right)){
  const float pan=sceneDistance_*0.0018f;
  const float cy=std::cos(sceneYaw_),sy=std::sin(sceneYaw_);
  sceneTargetX_-=(-cy*io.MouseDelta.x+sy*io.MouseDelta.y)*pan;
  sceneTargetZ_-=(-sy*io.MouseDelta.x-cy*io.MouseDelta.y)*pan;
 }
 if(canvasHovered&&!viewportToolbarOwnsMouse&&io.MouseWheel!=0.0f){
  const float delta=io.MouseWheel*(0.3f+1.7f*(sceneDistance_/100.0f));
  sceneDistance_=std::clamp(sceneDistance_-delta,0.2f,200.0f);
 }
 if(canvasHovered&&!viewportToolbarOwnsMouse&&ImGui::IsKeyDown(ImGuiKey_R)){sceneCameraInitialized_=false;}
 if(canvasHovered&&!viewportToolbarOwnsMouse){
  const float moveSpeed=(2.0f*float(editorPreferences.sceneMoveSpeed)/100.0f)*(0.2f+0.8f*(sceneDistance_/20.0f))*io.DeltaTime;
  const bool shift=ImGui::IsKeyDown(ImGuiKey_LeftShift)||ImGui::IsKeyDown(ImGuiKey_RightShift);
  const float cy=std::cos(sceneYaw_),sy=std::sin(sceneYaw_);
  if(shift){
   if(ImGui::IsKeyDown(ImGuiKey_W))sceneTargetZ_+=moveSpeed;
   if(ImGui::IsKeyDown(ImGuiKey_S))sceneTargetZ_-=moveSpeed;
   if(ImGui::IsKeyDown(ImGuiKey_A))sceneTargetX_+=moveSpeed;
   if(ImGui::IsKeyDown(ImGuiKey_D))sceneTargetX_-=moveSpeed;
  }else{
   if(ImGui::IsKeyDown(ImGuiKey_W)){sceneTargetX_-=sy*moveSpeed;sceneTargetZ_-=cy*moveSpeed;}
   if(ImGui::IsKeyDown(ImGuiKey_S)){sceneTargetX_+=sy*moveSpeed;sceneTargetZ_+=cy*moveSpeed;}
   if(ImGui::IsKeyDown(ImGuiKey_A)){sceneTargetX_-=cy*moveSpeed;sceneTargetZ_+=sy*moveSpeed;}
   if(ImGui::IsKeyDown(ImGuiKey_D)){sceneTargetX_+=cy*moveSpeed;sceneTargetZ_-=sy*moveSpeed;}
  }
  if(ImGui::IsKeyDown(ImGuiKey_E))sceneTargetY_+=moveSpeed;
  if(ImGui::IsKeyDown(ImGuiKey_Q))sceneTargetY_-=moveSpeed;
 }
 auto project=[&](V3 p){
  const float x=p.x-sceneTargetX_,y=p.y-sceneTargetY_,z=p.z-sceneTargetZ_;
  const float cy=std::cos(sceneYaw_),sy=std::sin(sceneYaw_),cp=std::cos(scenePitch_),sp=std::sin(scenePitch_);
  const float rx=x*cy-z*sy,rz=x*sy+z*cy;
  const float py=y*cp-rz*sp,pz=y*sp+rz*cp;
  const float depth=sceneDistance_-pz;
  const float focal=canvasSize.y*0.5f/std::tan(0.785398163f*0.5f);
  if(depth<=0.05f)return Projected{-100000.0f,-100000.0f,depth};
  return Projected{canvasMin.x+canvasSize.x*0.5f+rx*focal/depth,canvasMin.y+canvasSize.y*0.5f-py*focal/depth,depth};
 };
 if(!sceneryEntries.empty()){
  for(const auto*entry:sceneryEntries){
     const auto&sc=entry->scenery;
     auto vertexPosition=[&](std::uint16_t index)->V3{const auto&v=sc.vertices[index];if(sc.isSky){const float cp=std::cos(scenePitch_),sp=std::sin(scenePitch_),cy=std::cos(sceneYaw_),sy=std::sin(sceneYaw_);const float cameraX=sceneDistance_*cp*sy,cameraY=sceneDistance_*sp,cameraZ=sceneDistance_*cp*cy;return {float(v.x)*16.0f/400.0f+sceneTargetX_+cameraX,float(v.y)*16.0f/400.0f+sceneTargetY_+cameraY,float(v.z)*16.0f/400.0f+sceneTargetZ_+cameraZ};}return {(float(v.x)*16.0f+float(sc.xOffset))/400.0f,(float(v.y)*16.0f+float(sc.yOffset))/400.0f,(float(v.z)*16.0f+float(sc.zOffset))/400.0f};};
     auto vertexColor=[&](std::uint16_t index)->ImU32{
      const auto&v=sc.vertices[index];
      if(v.colorIndex<sc.colors.size()){const auto&c=sc.colors[v.colorIndex];return IM_COL32(c.r,c.g,c.b,255);}
      return IM_COL32(180,0,180,255);
     };
     auto addFace=[&](std::uint16_t ia,std::uint16_t ib,std::uint16_t ic,const Crash::NsfSceneryTexture*tex,int uvA,int uvB,int uvC){
      if(ia>=sc.vertices.size()||ib>=sc.vertices.size()||ic>=sc.vertices.size())return;
      const auto pa=project(vertexPosition(ia)),pb=project(vertexPosition(ib)),pc=project(vertexPosition(ic));
      if(pa.depth<=0.05f||pb.depth<=0.05f||pc.depth<=0.05f)return;
      const float minX=std::min({pa.x,pb.x,pc.x}),maxX=std::max({pa.x,pb.x,pc.x});
      const float minY=std::min({pa.y,pb.y,pc.y}),maxY=std::max({pa.y,pb.y,pc.y});
      if(maxX<canvasMin.x||minX>canvasMax.x||maxY<canvasMin.y||minY>canvasMax.y)return;
      Face f{};f.sky=sc.isSky;f.p[0]=pa;f.p[1]=pb;f.p[2]=pc;f.colors[0]=vertexColor(ia);f.colors[1]=vertexColor(ib);f.colors[2]=vertexColor(ic);f.depth=(pa.depth+pb.depth+pc.depth)/3.0f;
      if(tex){f.colorMode=tex->colorMode;f.blendMode=tex->blendMode;f.clutX=tex->clutX;f.clutY=(tex->clutY2<<2)|((tex->clutY1>>2)&3);const std::uint32_t pageEid=tex->page<sc.texturePageEids.size()?sc.texturePageEids[tex->page]:0U;for(const auto&chunk:nsf_.chunks())if(chunk.type==1&&chunk.bytes.size()>=65536&&chunk.bytes.size()>=8){const std::uint32_t pageId=std::uint32_t(chunk.bytes[4])|(std::uint32_t(chunk.bytes[5])<<8)|(std::uint32_t(chunk.bytes[6])<<16)|(std::uint32_t(chunk.bytes[7])<<24);if(pageId==pageEid){f.page=&chunk.bytes;break;}}const int xoff=(1<<(2-std::min<int>(tex->colorMode,2)))*64*tex->segment;float us[4]={float(tex->u1)+xoff,float(tex->u2)+xoff,float(tex->u3)+xoff,float(tex->u4)+xoff};float vs[4]={float(tex->v1),float(tex->v2),float(tex->v3),float(tex->v4)};auto adjustCoordinates=[](float* values){if(values[0]>values[1]||values[0]>values[2])values[0]+=1.0f;if(values[1]>values[0]||values[1]>values[2])values[1]+=1.0f;if(values[2]>values[1]||values[2]>values[0])values[2]+=1.0f;if(values[3]>values[1]||values[3]>values[2]||values[3]>values[0])values[3]+=1.0f;};adjustCoordinates(us);adjustCoordinates(vs);f.uv[0]=ImVec2(us[uvA],vs[uvA]);f.uv[1]=ImVec2(us[uvB],vs[uvB]);f.uv[2]=ImVec2(us[uvC],vs[uvC]);}
      faces.push_back(f);
     };
     // Resolve the polygon's texture index exactly as ProcessTextureInfoC2 does
     // in the original CrashEdit. Animated indices address the extended-texture
     // table first; that record then selects the actual texture-info entry.
     const int textureFrame=static_cast<int>(ImGui::GetTime()*30.0);
     auto resolveTexture=[&](std::uint16_t textureIndex,bool animated)->const Crash::NsfSceneryTexture*{
      int textureId=static_cast<int>(textureIndex)-1;
      if(animated){
       if(++textureId<0||static_cast<std::size_t>(textureId)>=sc.animatedTextures.size())return nullptr;
       std::uint32_t anim=sc.animatedTextures[static_cast<std::size_t>(textureId)];
       const bool isLod=(anim&0x80000000U)!=0;
       const int offset=static_cast<int>(anim&0x7ffU);
       if(offset==0)return nullptr;
       textureId=offset-1;
       if(isLod){
        textureId+=static_cast<int>((anim>>29U)&3U);
       }else{
        const int latency=static_cast<int>((anim>>26U)&0x1fU);
        const int delay=static_cast<int>((anim>>19U)&0x7fU);
        const int mask=static_cast<int>((anim>>12U)&0x7fU);
        textureId+=(textureFrame/(1+latency)+delay)&mask;
        if((anim&(1U<<11U))!=0){
         ++textureId;
         if(textureId<0||static_cast<std::size_t>(textureId)>=sc.animatedTextures.size())return nullptr;
         anim=sc.animatedTextures[static_cast<std::size_t>(textureId)];
         textureId=static_cast<int>(anim&0x7ffU)-1+static_cast<int>((anim>>29U)&3U);
        }
       }
      }else if(textureIndex==0){
       return nullptr;
      }
      if(textureId<0||static_cast<std::size_t>(textureId)>=sc.textures.size())return nullptr;
      return &sc.textures[static_cast<std::size_t>(textureId)];
     };
     for(const auto&t:sc.triangles){const auto*tex=resolveTexture(t.textureIndex,t.animated);if(tex)++texturedFaceCount;const auto before=faces.size();addFace(t.a,t.b,t.c,tex,1,0,2);if(tex&&faces.size()>before){if(faces.back().page)++resolvedTexturePageCount;else ++missingTexturePageCount;}}
     for(const auto&q:sc.quads){const auto*tex=resolveTexture(q.textureIndex,q.animated);if(tex)texturedFaceCount+=2;auto before=faces.size();addFace(q.a,q.b,q.c,tex,1,0,2);if(tex&&faces.size()>before){if(faces.back().page)++resolvedTexturePageCount;else ++missingTexturePageCount;}before=faces.size();addFace(q.a,q.d,q.c,tex,1,3,2);if(tex&&faces.size()>before){if(faces.back().page)++resolvedTexturePageCount;else ++missingTexturePageCount;}}
  }
  rendererDiagnostics.sceneBuildMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-sceneBuildStart).count();
  if(editorPreferences.softwareRendering)std::stable_sort(faces.begin(),faces.end(),[](const Face&a,const Face&b){if(a.sky!=b.sky)return a.sky;return a.depth>b.depth;});
  else std::stable_partition(faces.begin(),faces.end(),[](const Face&f){return f.sky;});
  auto channel=[](ImU32 c,int shift){return float((c>>shift)&255U);};
  // OpenGL's smooth vertex shading is perspective-correct. The CPU fallback
  // approximates it with a finer screen-space mesh and perspective-correct
  // barycentric color weights at each small triangle's centroid.
  auto sampleTexture=[&](const Face&f,float u,float v)->ImU32{if(!f.page)return IM_COL32(255,255,255,255);const auto&bytes=*f.page;const int pixelWidth=1024>>std::clamp(f.colorMode,0,2);const int x=std::clamp(static_cast<int>(std::floor(u)),0,pixelWidth-1),y=std::clamp(static_cast<int>(std::floor(v)),0,127);auto read16=[&](std::size_t off){return std::uint16_t(bytes[off])|(std::uint16_t(bytes[off+1])<<8);};auto expand=[](unsigned n){return (n*255U+15U)/31U;};std::uint16_t raw=0;if(f.colorMode==0){const unsigned packed=bytes[static_cast<std::size_t>(y)*512U+static_cast<std::size_t>(x/2)];const unsigned index=(packed>>((x&1)?4:0))&15U;const std::size_t off=static_cast<std::size_t>(f.clutY)*512U+static_cast<std::size_t>(f.clutX*16+index)*2U;if(off+1>=bytes.size())return IM_COL32(255,0,255,255);raw=read16(off);}else if(f.colorMode==1){const unsigned index=bytes[static_cast<std::size_t>(y)*512U+static_cast<std::size_t>(x)];const std::size_t off=static_cast<std::size_t>(f.clutY)*512U+static_cast<std::size_t>(f.clutX*16+index)*2U;if(off+1>=bytes.size())return IM_COL32(255,0,255,255);raw=read16(off);}else{const std::size_t off=static_cast<std::size_t>(y)*512U+static_cast<std::size_t>(x)*2U;if(off+1>=bytes.size())return IM_COL32(255,0,255,255);raw=read16(off);}const unsigned r=expand(raw&31U),g=expand((raw>>5)&31U),b=expand((raw>>10)&31U);if((raw&0x7fffU)==0&&(raw&0x8000U)==0)return IM_COL32(0,0,0,0);return IM_COL32(r,g,b,(raw&0x8000U)&&f.blendMode==0?128:255);};
  auto lerpColor=[&](const Face&f,float wa,float wb,float wc){
   const float qa=wa/f.p[0].depth,qb=wb/f.p[1].depth,qc=wc/f.p[2].depth;
   const float sum=qa+qb+qc;
   if(sum>0.0f){wa=qa/sum;wb=qb/sum;wc=qc/sum;}
   const auto mix=[&](int shift){return static_cast<unsigned char>(std::clamp(channel(f.colors[0],shift)*wa+channel(f.colors[1],shift)*wb+channel(f.colors[2],shift)*wc,0.0f,255.0f));};
   ImU32 base=IM_COL32(mix(IM_COL32_R_SHIFT),mix(IM_COL32_G_SHIFT),mix(IM_COL32_B_SHIFT),255);if(!f.page)return base;const ImU32 tex=sampleTexture(f,f.uv[0].x*wa+f.uv[1].x*wb+f.uv[2].x*wc,f.uv[0].y*wa+f.uv[1].y*wb+f.uv[2].y*wc);if((tex>>24)==0)return IM_COL32(0,0,0,0);const auto mul=[&](int sh){return static_cast<unsigned char>(std::min(255U,(((base>>sh)&255U)*((tex>>sh)&255U)*2U)/255U));};return IM_COL32(mul(IM_COL32_R_SHIFT),mul(IM_COL32_G_SHIFT),mul(IM_COL32_B_SHIFT),(tex>>24)&255U);
  };
  // Zone guides are collected in camera space and submitted to the same
  // depth buffer as scenery. ImGui draw-list primitives are screen-space
  // overlays and therefore cannot be occluded by walls.
  struct OverlaySegment{Projected a,b;ImU32 color;float width;};
  struct OverlayMarker{Projected p;ImU32 color;float radius;};
  std::vector<OverlaySegment> overlaySegments;
  std::vector<OverlayMarker> overlayMarkers;
  bool gpuOverlayRendered=false;
  auto addOverlaySegment=[&](Projected a,Projected b,ImU32 color,float width){
   if(a.depth>0.05f&&b.depth>0.05f)overlaySegments.push_back({a,b,color,width});
  };
  auto addOverlayMarker=[&](Projected p,ImU32 color,float radius){
   if(p.depth>0.05f)overlayMarkers.push_back({p,color,radius});
  };
  if(zone){
   const V3 origin{float(zone->x)/400.0f,float(zone->y)/400.0f,float(zone->z)/400.0f};
   const V3 corners[8]={{origin.x,origin.y,origin.z},{origin.x+float(zone->width)/400.0f,origin.y,origin.z},{origin.x,origin.y,origin.z+float(zone->depth)/400.0f},{origin.x+float(zone->width)/400.0f,origin.y,origin.z+float(zone->depth)/400.0f},{origin.x,origin.y+float(zone->height)/400.0f,origin.z},{origin.x+float(zone->width)/400.0f,origin.y+float(zone->height)/400.0f,origin.z},{origin.x,origin.y+float(zone->height)/400.0f,origin.z+float(zone->depth)/400.0f},{origin.x+float(zone->width)/400.0f,origin.y+float(zone->height)/400.0f,origin.z+float(zone->depth)/400.0f}};
   const int edges[12][2]={{0,1},{0,2},{1,3},{2,3},{4,5},{4,6},{5,7},{6,7},{0,4},{1,5},{2,6},{3,7}};
   for(const auto&e:edges)addOverlaySegment(project(corners[e[0]]),project(corners[e[1]]),IM_COL32(110,155,190,180),1.2f);
   for(std::size_t i=static_cast<std::size_t>(zone->cameraCount);i<zone->entities.size();++i){
    const auto&entity=zone->entities[i];if(entity.positions.empty())continue;
    std::vector<Projected> path;path.reserve(entity.positions.size());
    for(const auto&p:entity.positions)path.push_back(project({origin.x+float(p.x)/100.0f,origin.y+float(p.y)/100.0f,origin.z+float(p.z)/100.0f}));
    for(std::size_t pi=1;pi<path.size();++pi)addOverlaySegment(path[pi-1],path[pi],IM_COL32(245,190,75,220),1.8f);
    for(const auto&q:path)addOverlayMarker(q,IM_COL32(245,190,75,255),4.0f);
   }
   for(int i=0;i+2<zone->cameraCount&&static_cast<std::size_t>(i+2)<zone->entities.size();i+=3){
    const auto&camera=zone->entities[static_cast<std::size_t>(i)];std::vector<Projected> path;path.reserve(camera.positions.size());
    for(const auto&p:camera.positions)path.push_back(project({origin.x+float(p.x)/400.0f,origin.y+float(p.y)/400.0f,origin.z+float(p.z)/400.0f}));
    for(std::size_t pi=1;pi<path.size();++pi)addOverlaySegment(path[pi-1],path[pi],IM_COL32(80,210,130,255),2.0f);
    for(const auto&q:path)addOverlayMarker(q,IM_COL32(235,85,85,255),3.5f);
   }
   for(const auto*neighborEntry:neighborZoneEntries){
    const auto&neighbor=neighborEntry->zone;
    const V3 neighborOrigin{float(neighbor.x)/400.0f,float(neighbor.y)/400.0f,float(neighbor.z)/400.0f};
    const V3 neighborCorners[8]={{neighborOrigin.x,neighborOrigin.y,neighborOrigin.z},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y,neighborOrigin.z},{neighborOrigin.x,neighborOrigin.y,neighborOrigin.z+float(neighbor.depth)/400.0f},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y,neighborOrigin.z+float(neighbor.depth)/400.0f},{neighborOrigin.x,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z},{neighborOrigin.x,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z+float(neighbor.depth)/400.0f},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z+float(neighbor.depth)/400.0f}};
    for(const auto&e:edges)addOverlaySegment(project(neighborCorners[e[0]]),project(neighborCorners[e[1]]),IM_COL32(110,155,190,128),1.0f);
    for(std::size_t i=static_cast<std::size_t>(neighbor.cameraCount);i<neighbor.entities.size();++i){
     const auto&entity=neighbor.entities[i];if(entity.positions.empty())continue;
     std::vector<Projected> path;path.reserve(entity.positions.size());
     for(const auto&p:entity.positions)path.push_back(project({neighborOrigin.x+float(p.x)/100.0f,neighborOrigin.y+float(p.y)/100.0f,neighborOrigin.z+float(p.z)/100.0f}));
     for(std::size_t pi=1;pi<path.size();++pi)addOverlaySegment(path[pi-1],path[pi],IM_COL32(245,190,75,128),1.2f);
     for(const auto&q:path)addOverlayMarker(q,IM_COL32(245,190,75,128),3.0f);
    }
    for(int i=0;i+2<neighbor.cameraCount&&static_cast<std::size_t>(i+2)<neighbor.entities.size();i+=3){
     const auto&camera=neighbor.entities[static_cast<std::size_t>(i)];std::vector<Projected> path;path.reserve(camera.positions.size());
     for(const auto&p:camera.positions)path.push_back(project({neighborOrigin.x+float(p.x)/400.0f,neighborOrigin.y+float(p.y)/400.0f,neighborOrigin.z+float(p.z)/400.0f}));
     for(std::size_t pi=1;pi<path.size();++pi)addOverlaySegment(path[pi-1],path[pi],IM_COL32(80,210,130,128),1.4f);
     for(const auto&q:path)addOverlayMarker(q,IM_COL32(235,85,85,128),2.5f);
    }
   }
  }
  if(editorPreferences.softwareRendering){
   for(const auto&f:faces){
    const int subdivisions=2;
    auto point=[&](float a,float b,float c){return ImVec2(f.p[0].x*a+f.p[1].x*b+f.p[2].x*c,f.p[0].y*a+f.p[1].y*b+f.p[2].y*c);};
    for(int i=0;i<subdivisions;++i)for(int j=0;j<subdivisions-i;++j){
     const float n=float(subdivisions),a=float(i)/n,b=float(j)/n,c=1.0f-a-b;
     const float a1=float(i+1)/n,b1=float(j)/n,c1=1.0f-a1-b1;
     const float a2=float(i)/n,b2=float(j+1)/n,c2=1.0f-a2-b2;
     {const float wa=(c+c1+c2)/3.0f,wb=(a+a1+a2)/3.0f,wc=(b+b1+b2)/3.0f;const ImU32 color=lerpColor(f,wa,wb,wc);if((color>>24)!=0)dl->AddTriangleFilled(point(c,a,b),point(c1,a1,b1),point(c2,a2,b2),color);}
     if(i+j<subdivisions-1){const float a3=float(i+1)/n,b3=float(j+1)/n,c3=1.0f-a3-b3;{const float wa=(c1+c3+c2)/3.0f,wb=(a1+a3+a2)/3.0f,wc=(b1+b3+b2)/3.0f;const ImU32 color=lerpColor(f,wa,wb,wc);if((color>>24)!=0)dl->AddTriangleFilled(point(c1,a1,b1),point(c3,a3,b3),point(c2,a2,b2),color);}}
    }
    ++faceCount;
   }
  }else{
   const auto gpuCpuStart=std::chrono::steady_clock::now();
   std::size_t gpuDrawCalls=0,gpuUploadBytes=0;
   struct GpuVertex{float x,y,z,w,r,g,b,a,u,v;};
   using TextureKey=std::tuple<const std::vector<std::uint8_t>*,int,int,int,int>;
   using BatchKey=std::tuple<bool,TextureKey>;
   struct GpuState{unsigned int program=0,vao=0,vbo=0,framebuffer=0,colorTexture=0,depthBuffer=0;int width=0,height=0;bool ready=false;std::map<TextureKey,unsigned int> textureCache;std::string textureCachePath;};
   static GpuState gpu;
   auto compileShader=[](unsigned int type,const char*source)->unsigned int{
    const unsigned int shader=GL::CreateShader(type);if(!shader)return 0;
    GL::ShaderSource(shader,1,&source,nullptr);GL::CompileShader(shader);int ok=0;GL::GetShaderiv(shader,0x8B81U,&ok);
    if(!ok){GL::DeleteShader(shader);return 0;}return shader;
   };
   if(!gpu.ready){
    static constexpr const char* vertexSource=
     "#version 150\n"
     "in vec4 aPosition;\n"
     "in vec4 aColor;\n"
     "in vec2 aUV;\n"
     "out vec4 vColor;\n"
     "out vec2 vUV;\n"
     "void main(){gl_Position=aPosition;vColor=aColor;vUV=aUV;}\n";
    static constexpr const char* fragmentSource=
     "#version 150\n"
     "in vec4 vColor;in vec2 vUV;out vec4 fragColor;\n"
     "uniform sampler2D uTexture;uniform int uHasTexture;uniform int uBlendMask;uniform int uBlendMode;\n"
     "void main(){if(uHasTexture==0){if(uBlendMask!=3)discard;fragColor=vColor;return;}vec4 texel=texture(uTexture,vUV);if(texel.a<=0.0)discard;bool stp=texel.a<0.999;if(uBlendMask==3){if(stp&&uBlendMode!=3)discard;}else if(uBlendMode!=uBlendMask)discard;float alpha=(uBlendMask==0&&uBlendMode==0&&stp)?0.5:1.0;fragColor=vec4(min(vColor.rgb*texel.rgb*2.0,vec3(1.0)),alpha);}\n";
    const unsigned int vs=compileShader(0x8B31U,vertexSource),fs=compileShader(0x8B30U,fragmentSource);
    if(vs&&fs){
     gpu.program=GL::CreateProgram();GL::AttachShader(gpu.program,vs);GL::AttachShader(gpu.program,fs);GL::BindAttribLocation(gpu.program,0,"aPosition");GL::BindAttribLocation(gpu.program,1,"aColor");GL::BindAttribLocation(gpu.program,2,"aUV");GL::LinkProgram(gpu.program);
     int linked=0;GL::GetProgramiv(gpu.program,0x8B82U,&linked);gpu.ready=linked!=0;
    }
    if(vs)GL::DeleteShader(vs);if(fs)GL::DeleteShader(fs);
    if(gpu.ready){
     GL::GenVertexArrays(1,&gpu.vao);GL::GenBuffers(1,&gpu.vbo);
     GL::BindVertexArray(gpu.vao);GL::BindBuffer(0x8892U,gpu.vbo);
     GL::EnableVertexAttribArray(0);GL::VertexAttribPointer(0,4,0x1406U,0,sizeof(GpuVertex),reinterpret_cast<const void*>(0));
     GL::EnableVertexAttribArray(1);GL::VertexAttribPointer(1,4,0x1406U,0,sizeof(GpuVertex),reinterpret_cast<const void*>(4*sizeof(float)));
     GL::EnableVertexAttribArray(2);GL::VertexAttribPointer(2,2,0x1406U,0,sizeof(GpuVertex),reinterpret_cast<const void*>(8*sizeof(float)));
     GL::BindVertexArray(0);GL::BindBuffer(0x8892U,0);
    }
   }
   bool gpuRendered=false;
   if(gpu.ready){
    const ImGuiIO&renderIo=ImGui::GetIO();
    const int targetWidth=std::max(1,static_cast<int>(std::round(canvasSize.x*renderIo.DisplayFramebufferScale.x)));
    const int targetHeight=std::max(1,static_cast<int>(std::round(canvasSize.y*renderIo.DisplayFramebufferScale.y)));
    int previousFramebuffer=0,previousViewport[4]{},previousTexture=0,previousRenderbuffer=0,previousActiveTexture=0,previousDepthMask=1;
    GL::GetIntegerv(0x8CA6U,&previousFramebuffer);GL::GetIntegerv(0x0BA2U,previousViewport);GL::GetIntegerv(0x0B72U,&previousDepthMask);
    GL::GetIntegerv(0x84E0U,&previousActiveTexture);GL::ActiveTexture(0x84C0U);GL::GetIntegerv(0x8069U,&previousTexture);
    GL::GetIntegerv(0x8CA7U,&previousRenderbuffer);
    const std::string currentTextureSource=nsfPath_.string();
    if(gpu.textureCachePath!=currentTextureSource){
     for(const auto&entry:gpu.textureCache)GL::DeleteTextures(1,&entry.second);
     gpu.textureCache.clear();gpu.textureCachePath=currentTextureSource;
    }
    if(!gpu.framebuffer){GL::GenFramebuffers(1,&gpu.framebuffer);GL::GenTextures(1,&gpu.colorTexture);GL::GenRenderbuffers(1,&gpu.depthBuffer);}
    if(gpu.width!=targetWidth||gpu.height!=targetHeight){
     gpu.width=targetWidth;gpu.height=targetHeight;
     GL::BindTexture(0x0DE1U,gpu.colorTexture);
     GL::TexParameteri(0x0DE1U,0x2801U,0x2601);GL::TexParameteri(0x0DE1U,0x2800U,0x2601);
     GL::TexParameteri(0x0DE1U,0x2802U,0x812F);GL::TexParameteri(0x0DE1U,0x2803U,0x812F);
     GL::TexImage2D(0x0DE1U,0,0x8058,targetWidth,targetHeight,0,0x1908U,0x1401U,nullptr);
     GL::BindRenderbuffer(0x8D41U,gpu.depthBuffer);GL::RenderbufferStorage(0x8D41U,0x81A6U,targetWidth,targetHeight);
     GL::BindFramebuffer(0x8D40U,gpu.framebuffer);
     GL::FramebufferTexture2D(0x8D40U,0x8CE0U,0x0DE1U,gpu.colorTexture,0);
     GL::FramebufferRenderbuffer(0x8D40U,0x8D00U,0x8D41U,gpu.depthBuffer);
    }
    GL::BindFramebuffer(0x8D40U,gpu.framebuffer);
    const bool framebufferReady=GL::CheckFramebufferStatus(0x8D40U)==0x8CD5U;
    if(framebufferReady){
     std::map<BatchKey,std::vector<GpuVertex>> batches;
     auto makeVertices=[&](const Face&face){
      std::vector<GpuVertex> vertices;vertices.reserve(3);
      auto colorChannel=[](ImU32 color,int shift){return float((color>>shift)&255U)/255.0f;};
      for(int i=0;i<3;++i){
       const float x=2.0f*(face.p[i].x-canvasMin.x)/canvasSize.x-1.0f;
       const float y=1.0f-2.0f*(face.p[i].y-canvasMin.y)/canvasSize.y;
       constexpr float nearPlane=0.05f,farPlane=200.0f;
       const float depth=face.p[i].depth;
       const float clipZ=((farPlane+nearPlane)/(farPlane-nearPlane))*depth-(2.0f*farPlane*nearPlane)/(farPlane-nearPlane);
       const ImU32 color=face.colors[i];
       const int textureWidth=1024>>std::clamp(face.colorMode,0,2);
       const float u=face.page?face.uv[i].x/float(textureWidth):0.0f;
       const float v=face.page?face.uv[i].y/128.0f:0.0f;
       vertices.push_back({x*depth,y*depth,clipZ,depth,colorChannel(color,IM_COL32_R_SHIFT),colorChannel(color,IM_COL32_G_SHIFT),colorChannel(color,IM_COL32_B_SHIFT),1.0f,u,v});
      }
      return vertices;
     };
     for(const auto&face:faces){
      const TextureKey textureKey{face.page,face.colorMode,face.clutX,face.clutY,face.blendMode};
      auto&batch=batches[BatchKey{face.sky,textureKey}];auto vertices=makeVertices(face);
      batch.insert(batch.end(),vertices.begin(),vertices.end());
      ++faceCount;
     }
     GL::Viewport(0,0,targetWidth,targetHeight);if(editorPreferences.depthBufferEnabled)GL::Enable(0x0B71U);else GL::Disable(0x0B71U);GL::DepthFunc(0x0201U);
     GL::Enable(0x0BE2U);GL::BlendFuncSeparate(0x0302U,0x0303U,0U,1U);
     GL::ClearColor(0.0f,0.0f,0.0f,1.0f);GL::Clear(0x00004000U|0x00000100U);
     GL::UseProgram(gpu.program);GL::BindVertexArray(gpu.vao);GL::BindBuffer(0x8892U,gpu.vbo);
     GL::Uniform1i(GL::GetUniformLocation(gpu.program,"uTexture"),0);
     const int hasTextureLocation=GL::GetUniformLocation(gpu.program,"uHasTexture");
     const int blendMaskLocation=GL::GetUniformLocation(gpu.program,"uBlendMask");
     const int blendModeLocation=GL::GetUniformLocation(gpu.program,"uBlendMode");
     auto getTexture=[&](const TextureKey&key)->unsigned int{
      const auto*page=std::get<0>(key);if(!page)return 0;
      auto found=gpu.textureCache.find(key);
      if(found!=gpu.textureCache.end())return found->second;
      const int mode=std::clamp(std::get<1>(key),0,2);
      const int clutX=std::clamp(std::get<2>(key),0,15);
      const int clutY=std::clamp(std::get<3>(key),0,127);
      const int blendMode=std::get<4>(key);
      const int width=1024>>mode;
      std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width)*128U*4U,0);
      const auto&bytes=*page;
      auto expand5=[](unsigned value){return (value*255U+15U)/31U;};
      for(int py=0;py<128;++py)for(int px=0;px<width;++px){
       const std::size_t row=static_cast<std::size_t>(py)*512U;
       std::uint16_t raw=0;bool valid=true;
       if(mode==0||mode==1){
        const unsigned index=mode==0?((bytes[row+static_cast<std::size_t>(px/2)]>>((px&1)?4:0))&15U):bytes[row+static_cast<std::size_t>(px)];
        const std::size_t offset=static_cast<std::size_t>(clutY)*512U+static_cast<std::size_t>(clutX*16+static_cast<int>(index))*2U;
        if(offset+1U>=bytes.size())valid=false;else raw=std::uint16_t(bytes[offset])|(std::uint16_t(bytes[offset+1U])<<8);
       }else{
        const std::size_t offset=row+static_cast<std::size_t>(px)*2U;
        if(offset+1U>=bytes.size())valid=false;else raw=std::uint16_t(bytes[offset])|(std::uint16_t(bytes[offset+1U])<<8);
       }
       const std::size_t out=(static_cast<std::size_t>(py)*static_cast<std::size_t>(width)+static_cast<std::size_t>(px))*4U;
       if(!valid){rgba[out]=255;rgba[out+1U]=0;rgba[out+2U]=255;rgba[out+3U]=255;continue;}
       const unsigned red=expand5(raw&31U),green=expand5((raw>>5U)&31U),blue=expand5((raw>>10U)&31U);
       rgba[out]=static_cast<std::uint8_t>(red);rgba[out+1U]=static_cast<std::uint8_t>(green);rgba[out+2U]=static_cast<std::uint8_t>(blue);
       rgba[out+3U]=static_cast<std::uint8_t>((raw&0x7fffU)==0&&(raw&0x8000U)==0?0U:((raw&0x8000U)?128U:255U));
      }
      unsigned int texture=0;GL::GenTextures(1,&texture);GL::BindTexture(0x0DE1U,texture);
      GL::TexParameteri(0x0DE1U,0x2801U,0x2600);GL::TexParameteri(0x0DE1U,0x2800U,0x2600);
      GL::TexParameteri(0x0DE1U,0x2802U,0x812F);GL::TexParameteri(0x0DE1U,0x2803U,0x812F);
      GL::TexImage2D(0x0DE1U,0,0x8058,width,128,0,0x1908U,0x1401U,rgba.data());
      gpu.textureCache.emplace(key,texture);return texture;
     };
     // Match CrashEdit's four PS1 scenery blend passes: solid, trans,
     // subtractive, then additive. Non-solid passes preserve source polygon order.
     GL::BlendEquation(0x8006U);
     GL::BlendFuncSeparate(0x0302U,0x0303U,0U,1U);
     GL::Uniform1i(blendMaskLocation,3);
     for(bool skyPass:{true,false}){
      GL::DepthMask(skyPass?0:1);
      for(const auto&batch:batches){
      if(std::get<0>(batch.first)!=skyPass)continue;
      const auto&textureKey=std::get<1>(batch.first);
      const auto&page=std::get<0>(textureKey);
      const int mode=std::get<4>(textureKey);
      GL::BindTexture(0x0DE1U,getTexture(textureKey));
      GL::Uniform1i(hasTextureLocation,page?1:0);
      GL::Uniform1i(blendModeLocation,mode);
      const auto&vertices=batch.second;
      const std::size_t uploadSize=vertices.size()*sizeof(GpuVertex);gpuUploadBytes+=uploadSize;
      GL::BufferData(0x8892U,static_cast<GL::SizePtr>(uploadSize),vertices.empty()?nullptr:vertices.data(),0x88E8U);
      if(!vertices.empty()){GL::DrawArrays(0x0004U,0,static_cast<int>(vertices.size()));++gpuDrawCalls;}
      }
     }
     auto drawBlendPass=[&](int blendMode,unsigned int equation,unsigned int sourceFactor,unsigned int destinationFactor){
      GL::DepthMask(0);
      GL::BlendEquation(equation);
      GL::BlendFuncSeparate(sourceFactor,destinationFactor,0U,1U);
      GL::Uniform1i(blendMaskLocation,blendMode);
      for(bool skyPass:{true,false})for(const auto&face:faces){
       if(face.sky!=skyPass||!face.page||face.blendMode!=blendMode)continue;
       const TextureKey key{face.page,face.colorMode,face.clutX,face.clutY,face.blendMode};
       GL::BindTexture(0x0DE1U,getTexture(key));
       GL::Uniform1i(hasTextureLocation,1);
       GL::Uniform1i(blendModeLocation,face.blendMode);
       const auto vertices=makeVertices(face);
       const std::size_t uploadSize=vertices.size()*sizeof(GpuVertex);gpuUploadBytes+=uploadSize;
       GL::BufferData(0x8892U,static_cast<GL::SizePtr>(uploadSize),vertices.data(),0x88E8U);
       GL::DrawArrays(0x0004U,0,static_cast<int>(vertices.size()));++gpuDrawCalls;
      }
     };
     drawBlendPass(0,0x8006U,0x0302U,0x0303U); // standard alpha
     drawBlendPass(2,0x800BU,1U,1U);           // reverse subtract
     drawBlendPass(1,0x8006U,1U,1U);           // additive
     // Submit zone guides as camera-space billboards into the existing
     // scenery framebuffer. Their clip-space depth is tested against scenery.
     std::vector<GpuVertex> guideVertices;
     guideVertices.reserve(overlaySegments.size()*6U+overlayMarkers.size()*36U);
     auto guideVertex=[&](float sx,float sy,float depth,ImU32 color){
      const float x=2.0f*(sx-canvasMin.x)/canvasSize.x-1.0f;
      const float y=1.0f-2.0f*(sy-canvasMin.y)/canvasSize.y;
      constexpr float nearPlane=0.05f,farPlane=200.0f;
      const float clipZ=((farPlane+nearPlane)/(farPlane-nearPlane))*depth-(2.0f*farPlane*nearPlane)/(farPlane-nearPlane);
      auto channel=[](ImU32 c,int shift){return float((c>>shift)&255U)/255.0f;};
      return GpuVertex{x*depth,y*depth,clipZ,depth,channel(color,IM_COL32_R_SHIFT),channel(color,IM_COL32_G_SHIFT),channel(color,IM_COL32_B_SHIFT),channel(color,IM_COL32_A_SHIFT),0.0f,0.0f};
     };
     auto addGuideTriangle=[&](const GpuVertex&a,const GpuVertex&b,const GpuVertex&c){guideVertices.push_back(a);guideVertices.push_back(b);guideVertices.push_back(c);};
     for(const auto&line:overlaySegments){
      const float dx=line.b.x-line.a.x,dy=line.b.y-line.a.y,length=std::sqrt(dx*dx+dy*dy);
      if(length<=0.001f)continue;
      const float ox=-dy/length*(line.width*0.5f),oy=dx/length*(line.width*0.5f);
      const auto a=guideVertex(line.a.x+ox,line.a.y+oy,line.a.depth,line.color);
      const auto b=guideVertex(line.a.x-ox,line.a.y-oy,line.a.depth,line.color);
      const auto c=guideVertex(line.b.x+ox,line.b.y+oy,line.b.depth,line.color);
      const auto d=guideVertex(line.b.x-ox,line.b.y-oy,line.b.depth,line.color);
      addGuideTriangle(a,b,c);addGuideTriangle(c,b,d);
     }
     constexpr int markerSteps=12;
     for(const auto&marker:overlayMarkers){
      const auto center=guideVertex(marker.p.x,marker.p.y,marker.p.depth,marker.color);
      for(int step=0;step<markerSteps;++step){
       const float a=float(step)*6.28318530718f/float(markerSteps);
       const float b=float(step+1)*6.28318530718f/float(markerSteps);
       const auto va=guideVertex(marker.p.x+std::cos(a)*marker.radius,marker.p.y+std::sin(a)*marker.radius,marker.p.depth,marker.color);
       const auto vb=guideVertex(marker.p.x+std::cos(b)*marker.radius,marker.p.y+std::sin(b)*marker.radius,marker.p.depth,marker.color);
       addGuideTriangle(center,va,vb);
      }
     }
     if(!guideVertices.empty()){
      GL::DepthFunc(0x0203U);
      GL::DepthMask(0);
      GL::BlendEquation(0x8006U);
      GL::BlendFuncSeparate(0x0302U,0x0303U,0U,1U);
      GL::Uniform1i(blendMaskLocation,3);GL::Uniform1i(hasTextureLocation,0);GL::Uniform1i(blendModeLocation,3);
      const std::size_t guideBytes=guideVertices.size()*sizeof(GpuVertex);gpuUploadBytes+=guideBytes;
      GL::BufferData(0x8892U,static_cast<GL::SizePtr>(guideBytes),guideVertices.data(),0x88E8U);
      GL::DrawArrays(0x0004U,0,static_cast<int>(guideVertices.size()));++gpuDrawCalls;
      GL::DepthMask(1);GL::DepthFunc(0x0201U);
      gpuOverlayRendered=true;
     }
     GL::DepthMask(1);
     GL::BindTexture(0x0DE1U,0);
     GL::BindVertexArray(0);GL::UseProgram(0);GL::Disable(0x0B71U);GL::Enable(0x0BE2U);
     gpuRendered=true;
    }
    GL::DepthMask(static_cast<unsigned char>(previousDepthMask!=0));
    GL::BindFramebuffer(0x8D40U,static_cast<unsigned int>(previousFramebuffer));
    GL::Viewport(previousViewport[0],previousViewport[1],previousViewport[2],previousViewport[3]);
    GL::ActiveTexture(0x84C0U);GL::BindTexture(0x0DE1U,static_cast<unsigned int>(previousTexture));GL::ActiveTexture(static_cast<unsigned int>(previousActiveTexture));
    GL::BindRenderbuffer(0x8D41U,static_cast<unsigned int>(previousRenderbuffer));
    if(gpuRendered)dl->AddImage((ImTextureID)(std::intptr_t)gpu.colorTexture,canvasMin,canvasMax,ImVec2(0,1),ImVec2(1,0));
   }
   if(!gpuRendered){
    for(const auto&f:faces){
     const ImU32 color=lerpColor(f,1.0f/3.0f,1.0f/3.0f,1.0f/3.0f);
     if((color>>24)!=0)dl->AddTriangleFilled(ImVec2(f.p[0].x,f.p[0].y),ImVec2(f.p[1].x,f.p[1].y),ImVec2(f.p[2].x,f.p[2].y),color);
     ++faceCount;
    }
   }
   rendererDiagnostics.gpuSubmitCpuMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-gpuCpuStart).count();
   rendererDiagnostics.gpuDrawCalls=gpuDrawCalls;rendererDiagnostics.gpuUploadBytes=gpuUploadBytes;
   rendererDiagnostics.cachedTextureCount=gpu.textureCache.size();rendererDiagnostics.cachedTextureBytes=0;
   for(const auto&entry:gpu.textureCache){const int mode=std::clamp(std::get<1>(entry.first),0,2);rendererDiagnostics.cachedTextureBytes+=static_cast<std::size_t>(1024>>mode)*128U*4U;}
  }
  // The software renderer (or a failed GPU setup) has no usable depth
  // attachment, so retain a visible 2D guide fallback in those cases.
  if(editorPreferences.softwareRendering||!gpuOverlayRendered){
   for(const auto&line:overlaySegments)dl->AddLine(ImVec2(line.a.x,line.a.y),ImVec2(line.b.x,line.b.y),line.color,line.width);
   for(const auto&marker:overlayMarkers)dl->AddCircleFilled(ImVec2(marker.p.x,marker.p.y),marker.radius,marker.color);
  }
  char stats[384];std::snprintf(stats,sizeof(stats),"Scenery: %zu | Vertices: %zu | Faces: %zu | Textured: %zu | TPAG OK: %zu | Missing: %zu | Distance: %.2f",sceneryCount,vertexCount,faceCount,texturedFaceCount,resolvedTexturePageCount,missingTexturePageCount,sceneDistance_);
  char performanceStats[384];std::snprintf(performanceStats,sizeof(performanceStats),"CPU scene build: %.2f ms | GPU submit (CPU): %.2f ms | Draw calls: %zu | Upload: %.1f KiB | Texture cache: %zu (%.2f MiB)",rendererDiagnostics.sceneBuildMs,rendererDiagnostics.gpuSubmitCpuMs,rendererDiagnostics.gpuDrawCalls,double(rendererDiagnostics.gpuUploadBytes)/1024.0,rendererDiagnostics.cachedTextureCount,double(rendererDiagnostics.cachedTextureBytes)/(1024.0*1024.0));
  const char* resourceLabel=zoneEntry?Crash::eidToEName(zoneEntry->eid).c_str():(sceneryEntry?Crash::eidToEName(sceneryEntry->eid).c_str():"No scenery selected");
  dl->AddText(ImVec2(canvasMin.x+44,canvasMin.y+14),IM_COL32(230,235,242,255),resourceLabel);
  dl->AddText(ImVec2(canvasMin.x+12,canvasMax.y-(editorPreferences.rendererDiagnosticsVisible?40.0f:22.0f)),IM_COL32(180,194,210,255),stats);
  if(editorPreferences.rendererDiagnosticsVisible)dl->AddText(ImVec2(canvasMin.x+12,canvasMax.y-22),IM_COL32(145,190,210,255),performanceStats);
  if(faceCount==0)dl->AddText(ImVec2(canvasMin.x+16,canvasMin.y+34),IM_COL32(245,190,105,255),"No visible scenery faces at this camera position");
 }else{
  dl->AddText(ImVec2(canvasMin.x+16,canvasMin.y+16),IM_COL32(205,211,220,255),nsfValid_?"Select a decoded zone entry to view its scenery":"No zone loaded — open an NSF");
 }
 dl->PopClipRect();
 // Draw the toolbar last so scene geometry and status labels cannot cover it.
 ImDrawList* toolbarDraw=ImGui::GetWindowDrawList();
 toolbarDraw->AddRectFilled(toolbarMin,toolbarMax,ImGui::GetColorU32(toolbarHovered?ImGuiCol_ButtonHovered:ImGuiCol_Button),4.0f);
 const ImVec2 iconCenter((toolbarMin.x+toolbarMax.x)*0.5f,(toolbarMin.y+toolbarMax.y)*0.5f);
 const ImU32 iconColor=ImGui::GetColorU32(ImGuiCol_Text);
 for(int dot=-1;dot<=1;++dot)toolbarDraw->AddCircleFilled(ImVec2(iconCenter.x+float(dot)*5.0f,iconCenter.y),1.6f,iconColor);
 ImGui::End();
}
void EditorUI::drawTextureViewer(){
 if(!textureViewer_)return;
 const float uiScale=ImGui::GetIO().FontGlobalScale;
 ImGui::Begin("Texture Viewer",&textureViewer_);
 const Crash::NsfChunk* page=nullptr;
 if(nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto&candidate=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(candidate.type==1&&candidate.bytes.size()>=65536)page=&candidate;
 }
 if(!page){ImGui::TextDisabled("Select a type-1 Texture Chunk in Resource Hierarchy to inspect its 512 x 128 texture page.");ImGui::End();return;}
 const std::uint32_t eid=page->bytes.size()>=8?std::uint32_t(page->bytes[4])|(std::uint32_t(page->bytes[5])<<8)|(std::uint32_t(page->bytes[6])<<16)|(std::uint32_t(page->bytes[7])<<24):0U;
 ImGui::Text("Texture Page %s | Chunk %d | 64 KiB",Crash::eidToEName(eid).c_str(),selectedChunk_);
 if(ImGui::BeginTabBar("TextureFormatTabs")){
  if(ImGui::BeginTabItem("Crash 1")){textureFormat_=0;ImGui::EndTabItem();}
  if(ImGui::BeginTabItem("Crash 2 / 3")){textureFormat_=1;ImGui::EndTabItem();}
  ImGui::EndTabBar();
 }
 const char*modeNames[]={"4-bit indexed","8-bit indexed","16-bit direct"};
 ImGui::TextUnformatted("Color mode");ImGui::SameLine();ImGui::SetNextItemWidth(150.0f*uiScale);ImGui::Combo("##ColorMode",&textureColorMode_,modeNames,3);
 const char*blendNames[]={"Semi-transparent bit","Opaque"};
 ImGui::SameLine();ImGui::TextUnformatted("Blend mode");ImGui::SameLine();ImGui::SetNextItemWidth(150.0f*uiScale);ImGui::Combo("##BlendMode",&textureBlendMode_,blendNames,2);
 auto labeledInt=[uiScale](const char*label,const char*id,int*value){ImGui::TextUnformatted(label);ImGui::SameLine();ImGui::SetNextItemWidth(80.0f*uiScale);ImGui::InputInt(id,value);};
 labeledInt("X","##TextureX",&textureX_);ImGui::SameLine();labeledInt("Y","##TextureY",&textureY_);ImGui::SameLine();labeledInt("Width","##TextureWidth",&textureW_);ImGui::SameLine();labeledInt("Height","##TextureHeight",&textureH_);
 labeledInt("CLUT X","##ClutX",&textureClutX_);ImGui::SameLine();labeledInt("CLUT Y","##ClutY",&textureClutY_);ImGui::SameLine();ImGui::TextUnformatted("Zoom");ImGui::SameLine();ImGui::SetNextItemWidth(120.0f*uiScale);ImGui::SliderFloat("##TextureZoom",&textureZoom_,0.5f,3.0f,"%.1fx");
 const int imageWidth=textureColorMode_==0?1024:(textureColorMode_==1?512:256);
 constexpr int imageHeight=128;
 textureX_=std::clamp(textureX_,0,textureFormat_==0?127:imageWidth-1);
 textureY_=std::clamp(textureY_,0,textureFormat_==0?31:imageHeight-1);
 textureClutX_=std::clamp(textureClutX_,0,15);textureClutY_=std::clamp(textureClutY_,0,127);
 const int scaleX=textureFormat_==0?(8>>textureColorMode_):1;
 // Size is independent of position. Moving the rectangle must never
 // silently reduce its dimensions near a page edge.
 textureW_=std::clamp(textureW_,1,imageWidth);
 textureH_=std::clamp(textureH_,1,imageHeight);
 ImGui::BeginChild("TexturePageCanvas",ImVec2(0,0),true,ImGuiWindowFlags_HorizontalScrollbar);
 const ImVec2 origin=ImGui::GetCursorScreenPos();
 const ImVec2 imageSize(float(imageWidth)*textureZoom_,float(imageHeight)*textureZoom_);
 // The invisible item owns mouse capture and reserves the full image area.
 // Render the image and selection after it so later widgets cannot cover the overlay.
 ImGui::InvisibleButton("TextureSelection",imageSize,ImGuiButtonFlags_MouseButtonLeft);
 const bool imageHovered=ImGui::IsItemHovered();
 auto expand5=[](unsigned v){return static_cast<unsigned char>((v*255U+15U)/31U);};
 auto read16=[&](std::size_t offset){return std::uint16_t(page->bytes[offset])|(std::uint16_t(page->bytes[offset+1])<<8);};
 auto convert5551=[&](std::uint16_t raw){const unsigned r=expand5(raw&31U),g=expand5((raw>>5)&31U),b=expand5((raw>>10)&31U);const bool bit=(raw&0x8000U)!=0;unsigned a=255U;if(textureBlendMode_==0&&bit)a=127U;else if(textureBlendMode_==0&&r+g+b==0)a=0U;return IM_COL32(r,g,b,a);};
 std::vector<ImU32> palette;
 if(textureColorMode_==0||textureColorMode_==1){
  const int count=textureColorMode_==0?16:256;palette.resize(static_cast<std::size_t>(count));
  for(int i=0;i<count;++i){const std::size_t offset=static_cast<std::size_t>(textureClutY_)*512U+static_cast<std::size_t>((textureColorMode_==0?textureClutX_*16:0)+i)*2U;if(offset+1U<page->bytes.size())palette[static_cast<std::size_t>(i)]=convert5551(read16(offset));}
 }
 ImDrawList*draw=ImGui::GetWindowDrawList();
 draw->PushClipRect(origin,ImVec2(origin.x+imageSize.x,origin.y+imageSize.y),true);
 for(int y=0;y<imageHeight;++y){
  for(int x=0;x<imageWidth;++x){
   ImU32 color=IM_COL32(255,0,255,255);const std::size_t row=static_cast<std::size_t>(y)*512U;
   if(textureColorMode_==0){const unsigned packed=page->bytes[row+static_cast<std::size_t>(x/2)];const unsigned index=(packed>>((x&1)?4:0))&15U;color=palette[index];}
   else if(textureColorMode_==1){const unsigned index=page->bytes[row+static_cast<std::size_t>(x)];color=palette[index];}
   else{const std::size_t offset=row+static_cast<std::size_t>(x)*2U;if(offset+1U<page->bytes.size())color=convert5551(read16(offset));}
   const ImVec2 p0(origin.x+float(x)*textureZoom_,origin.y+float(y)*textureZoom_);
   draw->AddRectFilled(p0,ImVec2(p0.x+textureZoom_,p0.y+textureZoom_),color);
  }
 }
 // A single click moves the existing selection rectangle. Width and height
 // remain controlled by the numeric fields; dragging does not resize it.
 if(imageHovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
  const ImVec2 mouse=ImGui::GetIO().MousePos;
  const int px=std::clamp(static_cast<int>((mouse.x-origin.x)/textureZoom_),0,imageWidth-1);
  const int py=std::clamp(static_cast<int>((mouse.y-origin.y)/textureZoom_),0,imageHeight-1);
  // Place the selection center under the cursor, then constrain its
  // top-left corner so the full rectangle stays within the texture page.
  const int left=std::clamp(px-textureW_/2,0,imageWidth-textureW_);
  const int top=std::clamp(py-textureH_/2,0,imageHeight-textureH_);
  if(textureFormat_==0){textureX_=left/scaleX;textureY_=top/4;}
  else{textureX_=left;textureY_=top;}
 }
 // Recalculate from the final controls every frame. This keeps the dim mask
 // and border persistent after any unrelated UI interaction.
 const int finalX=textureFormat_==0?textureX_*scaleX:textureX_;
 const int finalY=textureFormat_==0?textureY_*4:textureY_;
 const int finalW=textureW_;
 const int finalH=textureH_;
 const ImVec2 r0(origin.x+float(finalX)*textureZoom_,origin.y+float(finalY)*textureZoom_);
 const ImVec2 r1(r0.x+float(finalW)*textureZoom_,r0.y+float(finalH)*textureZoom_);
 const ImU32 dim=IM_COL32(0,0,0,127);
 draw->AddRectFilled(origin,ImVec2(origin.x+imageSize.x,r0.y),dim);
 draw->AddRectFilled(ImVec2(origin.x,r0.y),ImVec2(r0.x,r1.y),dim);
 draw->AddRectFilled(ImVec2(r1.x,r0.y),ImVec2(origin.x+imageSize.x,r1.y),dim);
 draw->AddRectFilled(ImVec2(origin.x,r1.y),ImVec2(origin.x+imageSize.x,origin.y+imageSize.y),dim);
 draw->AddRect(r0,r1,IM_COL32(0,0,0,255),0.0f,0,3.0f);
 draw->AddRect(ImVec2(r0.x+2,r0.y+2),ImVec2(r1.x-2,r1.y-2),IM_COL32(255,255,255,255),0.0f,0,1.0f);
 draw->PopClipRect();
 ImGui::EndChild();ImGui::End();
}
void EditorUI::drawClutEditor(){
 if(!clutEditor_)return;
 const float uiScale=ImGui::GetIO().FontGlobalScale;
 ImGui::Begin("CLUT",&clutEditor_);
 const Crash::NsfChunk* page=nullptr;
 if(nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto&candidate=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(candidate.type==1&&candidate.bytes.size()>=65536)page=&candidate;
 }
 if(!page){ImGui::TextDisabled("Select a type-1 Texture Chunk to inspect its CLUT palette.");ImGui::End();return;}
 ImGui::Text("Palette from texture page chunk %d",selectedChunk_);
 ImGui::TextUnformatted("CLUT Y");ImGui::SameLine();ImGui::SetNextItemWidth(120.0f*uiScale);ImGui::SliderInt("##ClutEditorY",&textureClutY_,0,127);
 const int count=textureColorMode_==0?16:(textureColorMode_==1?256:0);
 if(count==0){ImGui::TextDisabled("16-bit direct color has no CLUT.");ImGui::End();return;}
 textureClutY_=std::clamp(textureClutY_,0,127);
 auto expand5=[](unsigned v){return static_cast<unsigned char>((v*255U+15U)/31U);};
 ImGui::TextDisabled(textureColorMode_==0?"4-bit: each column is a separate CLUT X (16 colors per CLUT).":"8-bit: CLUT X is unused; the 256-entry palette is shown as a 16 x 16 grid.");
 if(ImGui::BeginTable("ClutPaletteGrid",16,ImGuiTableFlags_Borders|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX,ImVec2(0,0),1200.0f)){
  for(int x=0;x<16;++x){ImGui::TableSetupColumn(("X "+std::to_string(x)).c_str(),ImGuiTableColumnFlags_WidthFixed,22.0f*uiScale);}
  ImGui::TableHeadersRow();
  for(int row=0;row<16;++row){
   ImGui::TableNextRow();
   for(int col=0;col<16;++col){
    ImGui::TableSetColumnIndex(col);
    const int paletteIndex=textureColorMode_==0?row:(row*16+col);
    const int clutX=textureColorMode_==0?col:0;
    const int entry=textureColorMode_==0?(clutX*16+row):paletteIndex;
    const std::size_t offset=static_cast<std::size_t>(textureClutY_)*512U+static_cast<std::size_t>(entry*2);
    if(offset+1U>=page->bytes.size())continue;
    const std::uint16_t raw=std::uint16_t(page->bytes[offset])|(std::uint16_t(page->bytes[offset+1])<<8);
    const ImU32 color=IM_COL32(expand5(raw&31U),expand5((raw>>5)&31U),expand5((raw>>10)&31U),255);
    ImGui::PushID(row*16+col);
    ImGui::ColorButton("##Color",ImGui::ColorConvertU32ToFloat4(color),ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoDragDrop,ImVec2(20.0f*uiScale,20.0f*uiScale));
    ImGui::PopID();
   }
  }
  ImGui::EndTable();
 }
 ImGui::TextDisabled("Palette entries are displayed read-only.");
 ImGui::End();
}

void EditorUI::drawSpawnPointsEditor(){
 if(!spawnPointsEditor_||!nsdValid_)return;
 // Do not force a dock ID here: users must be free to move this tab anywhere.
 ImGui::Begin("Spawn Points",&spawnPointsEditor_);
 ImGui::Text("Level ID");
 ImGui::SameLine();
 ImGui::SetNextItemWidth(90.0f);
 ImGui::InputScalar("##SpawnLevelID",ImGuiDataType_S32,&nsd_.id,nullptr,nullptr,"%02X",ImGuiInputTextFlags_CharsHexadecimal);
 ImGui::SameLine();ImGui::Text("Entity Count: %d",nsd_.entityCount);
 ImGui::Separator();
 const bool hasSelection=selectedSpawn_>=0&&static_cast<std::size_t>(selectedSpawn_)<nsd_.spawns.size();
 if(ImGui::Button("Append")){
  nsd_.spawns.push_back(Crash::NsdSpawnPoint{});
  selectedSpawn_=static_cast<int>(nsd_.spawns.size()-1);
 }
 ImGui::SameLine();
 ImGui::BeginDisabled(!hasSelection);
 if(ImGui::Button("Delete")){
  if(nsd_.spawns.size()<=1){status_="At least one spawn point must remain.";Diagnostics::log(DiagnosticLevel::Warning,status_);}else{
  nsd_.spawns.erase(nsd_.spawns.begin()+selectedSpawn_);
  selectedSpawn_=nsd_.spawns.empty()?-1:std::min(selectedSpawn_,static_cast<int>(nsd_.spawns.size())-1);
  }
 }
 ImGui::SameLine();
 if(ImGui::Button("Copy")){
  const auto&p=nsd_.spawns[static_cast<std::size_t>(selectedSpawn_)];char hex[49];
  std::snprintf(hex,sizeof(hex),"%08X%08X%08X%08X%08X%08X",static_cast<std::uint32_t>(p.zoneEid),static_cast<std::uint32_t>(p.camera),static_cast<std::uint32_t>(p.point),static_cast<std::uint32_t>(p.x),static_cast<std::uint32_t>(p.y),static_cast<std::uint32_t>(p.z));
  ImGui::SetClipboardText(hex);
 }
 ImGui::SameLine();
 if(ImGui::Button("Paste")){
  const char*clip=ImGui::GetClipboardText();unsigned z=0,cam=0,pt=0,x=0,y=0,zz=0;
  if(clip&&std::sscanf(clip,"%8x%8x%8x%8x%8x%8x",&z,&cam,&pt,&x,&y,&zz)==6){
   auto&p=nsd_.spawns[static_cast<std::size_t>(selectedSpawn_)];
   p={static_cast<std::int32_t>(z),static_cast<std::int32_t>(cam),static_cast<std::int32_t>(pt),static_cast<std::int32_t>(x),static_cast<std::int32_t>(y),static_cast<std::int32_t>(zz)};
  }else status_="Clipboard does not contain a 24-byte spawn-point structure.";
 }
 ImGui::EndDisabled();
 ImGui::SameLine();
 static int targetSpawnEntityId=0;
 ImGui::TextUnformatted("Object ID");ImGui::SameLine();ImGui::SetNextItemWidth(100.0f);ImGui::InputInt("##SpawnEntityID",&targetSpawnEntityId);
 ImGui::SameLine();
 ImGui::BeginDisabled(!hasSelection);
 if(ImGui::Button("Get Spawn")){
  bool found=false;
  for(const auto&chunk:nsf_.chunks())for(const auto&entry:chunk.entries)if(entry.valid&&entry.type==7&&entry.zone.decoded)for(const auto&entity:entry.zone.entities)if(entity.hasId&&entity.id==targetSpawnEntityId&&!entity.positions.empty()){
   auto&p=nsd_.spawns[static_cast<std::size_t>(selectedSpawn_)];p.zoneEid=static_cast<std::int32_t>(entry.eid);p.point=0;p.x=(entry.zone.x+4*entity.positions[0].x)<<8;p.y=(entry.zone.y+4*entity.positions[0].y)<<8;p.z=(entry.zone.z+4*entity.positions[0].z)<<8;found=true;break;
  }
  if(!found)status_="Entity ID was not found in a decoded NSF zone.";
 }
 ImGui::EndDisabled();
 ImGui::SameLine();
 ImGui::BeginDisabled(nsd_.goolMap.empty());if(ImGui::Button("GOOL Map"))nsdGoolMapWindow_=true;ImGui::EndDisabled();ImGui::SameLine();
 if(ImGui::Button("Reload")){
  Crash::NsdFile reloaded;std::string error;
  if(reloaded.load(nsdPath_,nsdVariant(gameVersion_),error)){nsd_=std::move(reloaded);selectedSpawn_=-1;status_="NSD reloaded from disk.";}
  else status_="NSD reload failed: "+error;
 }
 if(ImGui::BeginTable("SpawnPointTableV2",6,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX,ImVec2(0,0))){
  ImGui::TableSetupColumn("Zone EID",ImGuiTableColumnFlags_WidthFixed,190.0f);ImGui::TableSetupColumn("Camera",ImGuiTableColumnFlags_WidthFixed,75.0f);ImGui::TableSetupColumn("Point",ImGuiTableColumnFlags_WidthFixed,75.0f);ImGui::TableSetupColumn("X",ImGuiTableColumnFlags_WidthFixed,130.0f);ImGui::TableSetupColumn("Y",ImGuiTableColumnFlags_WidthFixed,130.0f);ImGui::TableSetupColumn("Z",ImGuiTableColumnFlags_WidthFixed,130.0f);ImGui::TableHeadersRow();
  for(std::size_t i=0;i<nsd_.spawns.size();++i){
   auto&p=nsd_.spawns[i];ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();
   ImGui::TableSetColumnIndex(0);
    const auto currentZoneEid=static_cast<std::uint32_t>(p.zoneEid);
   const auto eidName=Crash::eidToEName(currentZoneEid);
   std::string zonePreview=Crash::isValidEid(currentZoneEid)?eidName:"Unknown EID";
   if(!Crash::isValidEid(currentZoneEid)){char unknown[16];std::snprintf(unknown,sizeof(unknown)," (%08X)",currentZoneEid);zonePreview+=unknown;}
   ImGui::SetNextItemWidth(-FLT_MIN);
   if(ImGui::BeginCombo("##ZoneEID",zonePreview.c_str())){
    selectedSpawn_=static_cast<int>(i);
    for(const auto&zoneChunk:nsf_.chunks())if(zoneChunk.type==0)for(const auto&zoneEntry:zoneChunk.entries)if(zoneEntry.valid&&zoneEntry.type==7){
     char zoneHex[9];std::snprintf(zoneHex,sizeof(zoneHex),"%08X",zoneEntry.eid);
     const std::string option=Crash::eidToEName(zoneEntry.eid)+" ("+zoneHex+")";
     const bool isCurrent=currentZoneEid==zoneEntry.eid;
     if(ImGui::Selectable(option.c_str(),isCurrent))p.zoneEid=static_cast<std::int32_t>(zoneEntry.eid);
     if(isCurrent)ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
   }
   if(ImGui::BeginDragDropSource()){
    const int from=static_cast<int>(i);ImGui::SetDragDropPayload("NSD_SPAWN_ROW",&from,sizeof(from));ImGui::Text("Move spawn point %zu",i);ImGui::EndDragDropSource();
   }
   if(ImGui::BeginDragDropTarget()){
    if(const ImGuiPayload*payload=ImGui::AcceptDragDropPayload("NSD_SPAWN_ROW")){
     const int from=*static_cast<const int*>(payload->Data);
     if(from>=0&&static_cast<std::size_t>(from)<nsd_.spawns.size()&&from!=static_cast<int>(i)){
      auto first=nsd_.spawns.begin();
      if(from<static_cast<int>(i))std::rotate(first+from,first+from+1,first+static_cast<std::ptrdiff_t>(i+1));
      else std::rotate(first+static_cast<std::ptrdiff_t>(i),first+from,first+from+1);
      selectedSpawn_=static_cast<int>(i);
     }
    }
    ImGui::EndDragDropTarget();
   }
   ImGui::TableSetColumnIndex(1);ImGui::SetNextItemWidth(-FLT_MIN);ImGui::InputInt("##Camera",&p.camera);
   ImGui::TableSetColumnIndex(2);ImGui::SetNextItemWidth(-FLT_MIN);ImGui::InputInt("##Point",&p.point);
   std::int32_t*coords[]={&p.x,&p.y,&p.z};
   for(int col=0;col<3;++col){ImGui::TableSetColumnIndex(3+col);ImGui::SetNextItemWidth(-FLT_MIN);char id[16];std::snprintf(id,sizeof(id),"##Coord%d",col);ImGui::InputScalar(id,ImGuiDataType_S32,coords[col],nullptr,nullptr,"%08X",ImGuiInputTextFlags_CharsHexadecimal);}
   ImGui::PopID();
  }
  ImGui::EndTable();
 }
 ImGui::TextDisabled("Spawn point changes are included when you save the level (Ctrl+S).");
 ImGui::End();
}


void EditorUI::drawNsdGoolMap(){
 if(!nsdGoolMapWindow_||!nsdValid_)return;
 const ImGuiViewport*viewport=ImGui::GetMainViewport();
 const ImVec2 maxWindowSize(viewport->WorkSize.x*0.95f,viewport->WorkSize.y*0.90f);
 ImGui::SetNextWindowSize(ImVec2(900.0f,560.0f),ImGuiCond_FirstUseEver);
 ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f,280.0f),maxWindowSize);
 if(!ImGui::Begin("NSD GOOL Map",&nsdGoolMapWindow_)){ImGui::End();return;}
 ImGui::Text("GOOL map entries: %zu",nsd_.goolMap.size());
 ImGui::Separator();
 if(ImGui::BeginTable("NsdGoolMapTableV2",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX,ImVec2(0,0),900.0f)){
  ImGui::TableSetupColumn("Index",ImGuiTableColumnFlags_WidthFixed,55.0f);
  ImGui::TableSetupColumn("GOOL EID",ImGuiTableColumnFlags_WidthFixed,230.0f);
  ImGui::TableSetupColumn("NSF entry",ImGuiTableColumnFlags_WidthFixed,115.0f);
  ImGui::TableHeadersRow();
  for(std::size_t i=0;i<nsd_.goolMap.size();++i){
   const auto eid=static_cast<std::uint32_t>(nsd_.goolMap[i]);
   const auto name=Crash::eidToEName(eid);
   int matchChunk=-1,matchEntry=-1;
   for(std::size_t ci=0;ci<nsf_.chunks().size()&&matchChunk<0;++ci){
    const auto&chunk=nsf_.chunks()[ci];if(chunk.type!=0)continue;
    for(std::size_t ei=0;ei<chunk.entries.size();++ei){const auto&entry=chunk.entries[ei];if(entry.valid&&entry.type==11&&entry.eid==eid){matchChunk=static_cast<int>(ci);matchEntry=static_cast<int>(ei);break;}}
   }
   ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%02zu",i);
   ImGui::TableSetColumnIndex(1);ImGui::Text("%s (%08X)",name.c_str(),eid);
   ImGui::TableSetColumnIndex(2);
   if(matchChunk>=0){
    ImGui::PushID(static_cast<int>(i));
    if(ImGui::SmallButton("Open GOOL")){
     selectedChunk_=matchChunk;selectedEntry_=matchEntry;selectedEntity_=-1;selectedSpawn_=-1;openSelectedHierarchy_=true;
     status_="Opened GOOL map entry "+name+".";
    }
    ImGui::PopID();
   }else ImGui::TextDisabled("Not in NSF");
  }
  ImGui::EndTable();
 }
 ImGui::End();
}


void EditorUI::drawSearchWindow(){
 if(!searchWindow_)return;
 if(!nsfValid_){searchWindow_=false;return;}
 ImGui::SetNextWindowSize(ImVec2(680.0f,520.0f),ImGuiCond_FirstUseEver);
 if(!ImGui::Begin("Search Entries",&searchWindow_)){ImGui::End();return;}
 ImGui::TextDisabled("Find an NSF entry and jump directly to it in the Resource Hierarchy.");
 auto entryTypeLabel=[](std::uint32_t type){
  if(type==1)return std::string("Animation (1)");
  if(type==2)return std::string("Model (2)");
  if(type==7)return std::string("Zone (7)");
  if(type==3)return std::string("Scenery (3)");
  if(type==11)return std::string("GOOL (11)");
  return std::string("Entry type ")+std::to_string(type);
 };
 std::vector<std::uint32_t> availableTypes;
 for(const auto&chunk:nsf_.chunks())if(chunk.type==0&&chunk.entryTableValid)
  for(const auto&entry:chunk.entries)if(entry.valid&&std::find(availableTypes.begin(),availableTypes.end(),entry.type)==availableTypes.end())availableTypes.push_back(entry.type);
 std::sort(availableTypes.begin(),availableTypes.end());
 bool hasTextureChunks=false;
 for(const auto&chunk:nsf_.chunks())if(chunk.type==1){hasTextureChunks=true;break;}
 const std::string currentType=searchTypeFilter_==-2?"Texture chunks (chunk type 1)":searchTypeFilter_<0?"All resource categories":entryTypeLabel(static_cast<std::uint32_t>(searchTypeFilter_));
 ImGui::SetNextItemWidth(230.0f);
 if(ImGui::BeginCombo("Entry type",currentType.c_str())){
  if(ImGui::Selectable("All resource categories",searchTypeFilter_==-1))searchTypeFilter_=-1;
  if(hasTextureChunks&&ImGui::Selectable("Texture chunks (chunk type 1)",searchTypeFilter_==-2))searchTypeFilter_=-2;
  for(const auto type:availableTypes){
   const std::string label=entryTypeLabel(type);
   if(ImGui::Selectable(label.c_str(),searchTypeFilter_==static_cast<int>(type)))searchTypeFilter_=static_cast<int>(type);
  }
  ImGui::EndCombo();
 }
 ImGui::SameLine();
 ImGui::SetNextItemWidth(-1.0f);
 ImGui::InputTextWithHint("##EntrySearchQuery","Search by EID, entry name, type, or index...",searchQuery_,sizeof(searchQuery_));
 ImGui::Separator();
 std::size_t matchCount=0,totalCount=0;
 const std::string query=searchQuery_;
 auto containsInsensitive=[](const std::string&value,const std::string&needle){
  if(needle.empty())return true;
  return std::search(value.begin(),value.end(),needle.begin(),needle.end(),[](unsigned char a,unsigned char b){return std::tolower(a)==std::tolower(b);})!=value.end();
 };
 if(ImGui::BeginTable("EntrySearchResults",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX,ImVec2(0,-ImGui::GetFrameHeightWithSpacing()*1.8f),1000.0f)){
  ImGui::TableSetupColumn("Chunk",ImGuiTableColumnFlags_WidthFixed,62.0f);
  ImGui::TableSetupColumn("Entry",ImGuiTableColumnFlags_WidthFixed,62.0f);
  ImGui::TableSetupColumn("Name / EID");
  ImGui::TableSetupColumn("Type",ImGuiTableColumnFlags_WidthFixed,105.0f);
  ImGui::TableSetupColumn("Items",ImGuiTableColumnFlags_WidthFixed,55.0f);
  ImGui::TableHeadersRow();
  for(std::size_t ci=0;ci<nsf_.chunks().size();++ci){
   const auto&chunk=nsf_.chunks()[ci];
   if(chunk.type!=0||!chunk.entryTableValid)continue;
   for(std::size_t ei=0;ei<chunk.entries.size();++ei){
    const auto&entry=chunk.entries[ei];
    ++totalCount;
    if(!entry.valid)continue;
    const bool typeMatch=searchTypeFilter_!=-2&&(searchTypeFilter_<0||entry.type==static_cast<std::uint32_t>(searchTypeFilter_));
    if(!typeMatch)continue;
    const std::string eidName=Crash::eidToEName(entry.eid);
    const std::string typeName=entry.type==1?"Animation":entry.type==2?"Model":entry.type==7?"Zone":entry.type==3?"Scenery":entry.type==11?"GOOL":"Type "+std::to_string(entry.type);
    char eidHex[16];std::snprintf(eidHex,sizeof(eidHex),"%08X",entry.eid);
    const std::string searchable=eidName+" "+eidHex+" 0x"+eidHex+" "+typeName+" "+std::to_string(entry.type)+" "+std::to_string(ei)+" "+std::to_string(ci);
    if(!containsInsensitive(searchable,query))continue;
    ++matchCount;
    ImGui::TableNextRow();
    ImGui::PushID(static_cast<int>(ci));ImGui::PushID(static_cast<int>(ei));
    ImGui::TableSetColumnIndex(0);ImGui::Text("%03zu",ci);
    ImGui::TableSetColumnIndex(1);ImGui::Text("%03zu",ei);
    ImGui::TableSetColumnIndex(2);
    const bool selected=selectedChunk_==static_cast<int>(ci)&&selectedEntry_==static_cast<int>(ei);
    const std::string label=typeName+" ("+eidName+") ["+eidHex+"]";
    if(ImGui::Selectable(label.c_str(),selected,ImGuiSelectableFlags_SpanAllColumns)){
     selectedChunk_=static_cast<int>(ci);selectedEntry_=static_cast<int>(ei);selectedEntity_=-1;selectedSpawn_=-1;
     openSelectedHierarchy_=true;
     if((entry.type==7&&entry.zone.decoded)||(entry.type==3&&entry.scenery.decoded)){viewerChunk_=static_cast<int>(ci);viewerEntry_=static_cast<int>(ei);}
     status_="Located "+typeName+" entry "+eidName+" in chunk "+std::to_string(ci)+".";
    }
    ImGui::TableSetColumnIndex(3);ImGui::TextUnformatted(typeName.c_str());
    ImGui::TableSetColumnIndex(4);ImGui::Text("%u",entry.itemCount);
    ImGui::PopID();ImGui::PopID();
   }
  }
  for(std::size_t ci=0;ci<nsf_.chunks().size();++ci){
   const auto&chunk=nsf_.chunks()[ci];
   if(chunk.type!=1||(searchTypeFilter_!=-1&&searchTypeFilter_!=-2))continue;
   ++totalCount;
   char chunkHex[16];std::snprintf(chunkHex,sizeof(chunkHex),"%08X",static_cast<unsigned>(chunk.chunkId));
   const std::string searchable="Texture chunk "+std::string(chunkHex)+" "+std::to_string(ci);
   if(!containsInsensitive(searchable,query))continue;
   ++matchCount;
   ImGui::TableNextRow();
   ImGui::PushID(static_cast<int>(ci));ImGui::PushID(-2);
   ImGui::TableSetColumnIndex(0);ImGui::Text("%03zu",ci);
   ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted("—");
   ImGui::TableSetColumnIndex(2);
   const bool selected=selectedChunk_==static_cast<int>(ci)&&selectedEntry_==-1;
   const std::string label="Texture chunk ("+std::string(chunkHex)+")";
   if(ImGui::Selectable(label.c_str(),selected,ImGuiSelectableFlags_SpanAllColumns)){
    selectedChunk_=static_cast<int>(ci);selectedEntry_=-1;selectedEntity_=-1;selectedSpawn_=-1;
    openSelectedHierarchy_=true;
    status_="Located texture chunk "+std::string(chunkHex)+".";
   }
   ImGui::TableSetColumnIndex(3);ImGui::TextUnformatted("Texture");
   ImGui::TableSetColumnIndex(4);ImGui::TextUnformatted("Chunk");
   ImGui::PopID();ImGui::PopID();
  }
  ImGui::EndTable();
 }
 ImGui::Text("%zu matching entries | %zu total entries",matchCount,totalCount);
 ImGui::SameLine();
 if(ImGui::Button("Close"))searchWindow_=false;
 ImGui::End();
}
void EditorUI::drawGoolInspector(){
 if(!nsfValid_||selectedChunk_<0||selectedEntry_<0||static_cast<std::size_t>(selectedChunk_)>=nsf_.chunks().size())return;
 const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
 if(chunk.type!=0||static_cast<std::size_t>(selectedEntry_)>=chunk.entries.size())return;
 const auto&entry=chunk.entries[static_cast<std::size_t>(selectedEntry_)];
 if(!entry.valid||entry.type!=11)return;
 ImGui::Begin("GOOL Inspector",nullptr,ImGuiWindowFlags_HorizontalScrollbar);
 const auto&g=entry.gool;
 if(!g.decoded){
  ImGui::TextDisabled("GOOL entry could not be decoded.");
  if(!g.error.empty())ImGui::TextWrapped("%s",g.error.c_str());
  ImGui::End();return;
 }
 const char*version=g.version==Crash::NsfGoolVersion::Version0?"Prototype":g.version==Crash::NsfGoolVersion::Version1?"Crash 1":g.version==Crash::NsfGoolVersion::Version2?"Crash 2":"Crash 3";
 ImGui::Text("%s GOOL | %s",version,Crash::eidToEName(entry.eid).c_str());
 ImGui::Text("EID: %08X | ID: %d | Class: %d",entry.eid,g.id,g.classId);
 ImGui::Text("Format: %d | Heap Base: 0x%X | Events: %d | Entries: %d",g.format,static_cast<unsigned>(g.heapBase),g.eventCount,g.entryCount);
 ImGui::Text("Instructions: %zu | Data words: %zu | State map: %zu | State descriptors: %zu | Frame-group bytes: %zu",g.instructions.size(),g.dataPool.size(),g.stateMap.size(),g.stateDescriptors.size(),g.frameGroupBytes);
 ImGui::Separator();
 if(ImGui::BeginTabBar("GOOLInspectorTabs")){
  if(ImGui::BeginTabItem("Instructions")){
   ImGui::TextDisabled("GOOL and embedded R3000A instructions are decoded separately, following the original CrashEdit native-mode markers and return delay slots.");
   if(ImGui::BeginTable("GOOLInstructionListing",6,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX,ImVec2(0,280),1800.0f)){
    ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,48);
    ImGui::TableSetupColumn("Offset",ImGuiTableColumnFlags_WidthFixed,72);
    ImGui::TableSetupColumn("Instruction",ImGuiTableColumnFlags_WidthFixed,100);
    ImGui::TableSetupColumn("Mode / Opcode",ImGuiTableColumnFlags_WidthFixed,100);
    ImGui::TableSetupColumn("Word",ImGuiTableColumnFlags_WidthFixed,108);
    ImGui::TableSetupColumn("Operands");
    ImGui::TableHeadersRow();
    for(std::size_t i=0;i<g.instructions.size();++i){
     const auto&ins=g.instructions[i];const auto&info=Crash::goolOpcodeInfo(g.version,ins.opcode);ImGui::TableNextRow();
     ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",i);
     ImGui::TableSetColumnIndex(1);ImGui::Text("0x%04zX",ins.byteOffset);
     ImGui::TableSetColumnIndex(2);if(ins.mipsKnown||!ins.isMips)ImGui::TextUnformatted(ins.mnemonic.c_str());else ImGui::TextDisabled("%s",ins.mnemonic.c_str());
     ImGui::TableSetColumnIndex(3);if(ins.isMips)ImGui::Text("MIPS");else ImGui::Text("GOOL 0x%02X",static_cast<unsigned>(ins.opcode));
     ImGui::TableSetColumnIndex(4);ImGui::Text("0x%08X",ins.word);
     ImGui::TableSetColumnIndex(5);
     if(ins.isMips){if(!ins.arguments.empty())ImGui::TextUnformatted(ins.arguments.c_str());else ImGui::TextDisabled("No operands");}
     else if(!ins.arguments.empty())ImGui::TextUnformatted(ins.arguments.c_str());else ImGui::TextDisabled("%s",info.known?"No operands":"Unknown format");
    }
    ImGui::EndTable();
   }
   ImGui::EndTabItem();
  }
  if(ImGui::BeginTabItem("Control Flow")){
   const auto graph=Crash::analyzeGoolControlFlow(g);
   ImGui::Text("%zu basic blocks | %zu control-flow edges | %zu diagnostics",graph.blocks.size(),graph.edges.size(),graph.diagnostics.size());
   ImGui::TextDisabled("Instruction ranges are inclusive in this view. State-transition destinations are dynamic and are resolved separately through the State Map and hooks.");
   if(ImGui::BeginTable("GOOLControlFlowBlocks",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingFixedFit,ImVec2(0,150))){
    ImGui::TableSetupColumn("Block",ImGuiTableColumnFlags_WidthFixed,72);
    ImGui::TableSetupColumn("Instructions",ImGuiTableColumnFlags_WidthFixed,150);
    ImGui::TableSetupColumn("Count",ImGuiTableColumnFlags_WidthFixed,70);
    ImGui::TableHeadersRow();
    for(std::size_t i=0;i<graph.blocks.size();++i){const auto&block=graph.blocks[i];ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("BB%zu",i);ImGui::TableSetColumnIndex(1);ImGui::Text("%zu - %zu",block.startInstruction,block.endInstruction-1U);ImGui::TableSetColumnIndex(2);ImGui::Text("%zu",block.endInstruction-block.startInstruction);}
    ImGui::EndTable();
   }
   auto edgeKindName=[](Crash::NsfGoolFlowEdgeKind kind)->const char*{
    switch(kind){
     case Crash::NsfGoolFlowEdgeKind::Fallthrough:return "Fallthrough";
     case Crash::NsfGoolFlowEdgeKind::Branch:return "Branch";
     case Crash::NsfGoolFlowEdgeKind::BranchTrue:return "Branch (true)";
     case Crash::NsfGoolFlowEdgeKind::BranchFalse:return "Branch (false)";
     case Crash::NsfGoolFlowEdgeKind::Call:return "Local call";
     case Crash::NsfGoolFlowEdgeKind::ExternalCall:return "External call";
     case Crash::NsfGoolFlowEdgeKind::StateTransition:return "State transition";
     case Crash::NsfGoolFlowEdgeKind::StateTransitionTrue:return "State transition (true)";
     case Crash::NsfGoolFlowEdgeKind::StateTransitionFalse:return "State transition (false)";
     case Crash::NsfGoolFlowEdgeKind::Return:return "Return";
    }return "Unknown";
   };
   if(ImGui::BeginTable("GOOLControlFlowEdges",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX,ImVec2(0,190))){
    ImGui::TableSetupColumn("From",ImGuiTableColumnFlags_WidthFixed,65);
    ImGui::TableSetupColumn("Flow",ImGuiTableColumnFlags_WidthFixed,150);
    ImGui::TableSetupColumn("Destination",ImGuiTableColumnFlags_WidthFixed,115);
    ImGui::TableSetupColumn("Details");
    ImGui::TableHeadersRow();
    auto transitionHookDetails=[&](const Crash::NsfGoolFlowEdge&edge)->std::string{
     if(edge.kind!=Crash::NsfGoolFlowEdgeKind::StateTransition&&edge.kind!=Crash::NsfGoolFlowEdgeKind::StateTransitionTrue&&edge.kind!=Crash::NsfGoolFlowEdgeKind::StateTransitionFalse)return {};
     if(edge.sourceBlock>=graph.blocks.size())return "Transition source block is unavailable.";
     const auto&sourceBlock=graph.blocks[edge.sourceBlock];
     if(sourceBlock.endInstruction==0||sourceBlock.endInstruction>g.instructions.size())return "Transition instruction is unavailable.";
     const std::size_t instructionIndex=sourceBlock.endInstruction-1U;
     const auto&instruction=g.instructions[instructionIndex];
     const auto operands=Crash::decodeGoolOperands(g.version,instruction.opcode,instruction.operandBits);
     const auto stateOperand=std::find_if(operands.begin(),operands.end(),[](const auto&operand){return operand.name=='S';});
     if(stateOperand==operands.end())return "State descriptor operand is unavailable.";
     const std::size_t descriptorIndex=stateOperand->rawValue;
     if(descriptorIndex>=g.stateDescriptors.size())return "State descriptor "+std::to_string(descriptorIndex)+" is out of range.";
     const auto hook=std::find_if(g.stateHookReferences.begin(),g.stateHookReferences.end(),[descriptorIndex](const auto&reference){return reference.descriptorIndex==descriptorIndex&&reference.kind==Crash::NsfGoolStateHookKind::Transition;});
     if(hook==g.stateHookReferences.end())return "Descriptor "+std::to_string(descriptorIndex)+" has no resolved transition-hook record.";
     const char*status="Unknown";
     switch(hook->status){
      case Crash::NsfGoolStateHookStatus::Absent:status="Absent";break;
      case Crash::NsfGoolStateHookStatus::InvalidDataIndex:status="Invalid data index";break;
      case Crash::NsfGoolStateHookStatus::MissingTarget:status="Missing target";break;
      case Crash::NsfGoolStateHookStatus::AmbiguousTarget:status="Ambiguous target";break;
      case Crash::NsfGoolStateHookStatus::TargetUndecoded:status="Target not decoded";break;
      case Crash::NsfGoolStateHookStatus::OffsetOutOfRange:status="Offset out of range";break;
      case Crash::NsfGoolStateHookStatus::Local:status="Local";break;
      case Crash::NsfGoolStateHookStatus::External:status="External";break;
     }
     const std::string prefix="Descriptor "+std::to_string(descriptorIndex)+" → transition hook: ";
     if(hook->status!=Crash::NsfGoolStateHookStatus::Local&&hook->status!=Crash::NsfGoolStateHookStatus::External)return prefix+status+".";
     if(hook->targetChunkIndex>=nsf_.chunks().size())return prefix+"resolved target chunk is unavailable.";
     const auto&targetChunk=nsf_.chunks()[hook->targetChunkIndex];
     if(hook->targetEntryIndex>=targetChunk.entries.size())return prefix+"resolved target entry is unavailable.";
     const auto&targetEntry=targetChunk.entries[hook->targetEntryIndex];
     if(!targetEntry.valid||targetEntry.type!=11)return prefix+"resolved target is not a valid GOOL entry.";
     char target[256];
     std::snprintf(target,sizeof(target),"%s GOOL 0x%08X, instruction %u",Crash::eidToEName(targetEntry.eid).c_str(),targetEntry.eid,static_cast<unsigned>(hook->instructionOffset));
     return prefix+status+" → "+target+".";
    };
    for(const auto&edge:graph.edges){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("BB%zu",edge.sourceBlock);ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(edgeKindName(edge.kind));ImGui::TableSetColumnIndex(2);if(edge.targetValid)ImGui::Text("Instruction %zu",edge.targetInstruction);else ImGui::TextDisabled("Dynamic / unresolved");ImGui::TableSetColumnIndex(3);const std::string hookDetails=transitionHookDetails(edge);if(!hookDetails.empty())ImGui::TextWrapped("%s",hookDetails.c_str());else if(!edge.diagnostic.empty())ImGui::TextWrapped("%s",edge.diagnostic.c_str());else ImGui::TextDisabled("—");}
    ImGui::EndTable();
   }
   if(!graph.diagnostics.empty()){ImGui::Separator();ImGui::Text("Analysis diagnostics");for(const auto&diagnostic:graph.diagnostics)ImGui::BulletText("%s",diagnostic.c_str());}
   if(graph.blocks.empty())ImGui::TextDisabled("No instructions are available to analyze.");
   ImGui::EndTabItem();
  }
  ImGui::EndTabBar();
 }
 ImGui::End();
}

void EditorUI::drawGoolGraph(){
 const Crash::NsfEntry* selectedEntry=nullptr;
 const Crash::NsfGool* gool=nullptr;
 if(nsfValid_&&selectedChunk_>=0&&selectedEntry_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(chunk.type==0&&static_cast<std::size_t>(selectedEntry_)<chunk.entries.size()){
   const auto&entry=chunk.entries[static_cast<std::size_t>(selectedEntry_)];
   if(entry.valid&&entry.type==11){selectedEntry=&entry;gool=&entry.gool;}
  }
 }
 if(!selectedEntry||!gool)return;
 static bool graphDockInitialized=false;
 if(!graphDockInitialized){
  if(ImGuiWindow* hierarchy=ImGui::FindWindowByName("Resource Hierarchy"))
   if(hierarchy->DockNode){ImGui::SetNextWindowDockID(hierarchy->DockNode->ID,ImGuiCond_Always);graphDockInitialized=true;}
 }
 ImGui::Begin("GOOL Graph");
 if(!gool->decoded){
  ImGui::Text("%s | 0x%08X",Crash::eidToEName(selectedEntry->eid).c_str(),selectedEntry->eid);
  ImGui::TextDisabled("This GOOL entry could not be decoded, so its dependency graph is unavailable.");
  if(!gool->error.empty())ImGui::TextWrapped("%s",gool->error.c_str());
  ImGui::End();return;
 }
 const auto statusColor=[](Crash::NsfGoolStateReferenceStatus status){
  switch(status){case Crash::NsfGoolStateReferenceStatus::Local:return ImVec4(.35f,.78f,.52f,1);case Crash::NsfGoolStateReferenceStatus::External:return ImVec4(.35f,.68f,.98f,1);case Crash::NsfGoolStateReferenceStatus::Ambiguous:return ImVec4(1,.68f,.25f,1);default:return ImVec4(.95f,.38f,.36f,1);}
 };
 const auto hookStatusName=[](Crash::NsfGoolStateHookStatus status)->const char*{
  switch(status){
   case Crash::NsfGoolStateHookStatus::Absent:return "Absent";
   case Crash::NsfGoolStateHookStatus::InvalidDataIndex:return "Invalid data index";
   case Crash::NsfGoolStateHookStatus::MissingTarget:return "Missing target";
   case Crash::NsfGoolStateHookStatus::AmbiguousTarget:return "Ambiguous target";
   case Crash::NsfGoolStateHookStatus::TargetUndecoded:return "Target not decoded";
   case Crash::NsfGoolStateHookStatus::OffsetOutOfRange:return "Offset out of range";
   case Crash::NsfGoolStateHookStatus::Local:return "Local destination";
   case Crash::NsfGoolStateHookStatus::External:return "External destination";
  }return "Unknown";
 };
 const auto hookKindName=[](Crash::NsfGoolStateHookKind kind)->const char*{
  switch(kind){case Crash::NsfGoolStateHookKind::Event:return "Event";case Crash::NsfGoolStateHookKind::Transition:return "Transition";case Crash::NsfGoolStateHookKind::Code:return "Code";}return "Hook";
 };
 std::vector<std::pair<std::size_t,std::size_t>> dependencies;
 std::size_t unresolved=0,externalCount=0,localCount=0;
 for(const auto&reference:gool->stateReferences){
  if(reference.status==Crash::NsfGoolStateReferenceStatus::InvalidDataIndex||!reference.targetFound){++unresolved;continue;}
  if(reference.status==Crash::NsfGoolStateReferenceStatus::External||reference.status==Crash::NsfGoolStateReferenceStatus::Ambiguous){
   ++externalCount;
   if(reference.status==Crash::NsfGoolStateReferenceStatus::External){
    const auto location=std::make_pair(reference.targetChunkIndex,reference.targetEntryIndex);
    if(std::find(dependencies.begin(),dependencies.end(),location)==dependencies.end())dependencies.push_back(location);
   }
  }else ++localCount;
 }
 std::size_t validHooks=0,problemHooks=0,absentHooks=0;
 for(const auto&hook:gool->stateHookReferences){
  if(hook.status==Crash::NsfGoolStateHookStatus::Absent)++absentHooks;
  else if(hook.status==Crash::NsfGoolStateHookStatus::Local||hook.status==Crash::NsfGoolStateHookStatus::External)++validHooks;
  else ++problemHooks;
 }
 ImGui::Separator();
 if(ImGui::BeginChild("GOOLGraphCanvas",ImVec2(0,0),false,ImGuiWindowFlags_AlwaysVerticalScrollbar)){
  const float available=ImGui::GetContentRegionAvail().x;
  auto centeredText=[&](const char*text,ImVec4 color,bool disabled=false,float scale=1.0f){
   const float contentWidth=ImGui::GetContentRegionAvail().x;
   const float size=ImGui::GetFontSize()*scale;
   const ImVec2 extent=ImGui::GetFont()->CalcTextSizeA(size,contentWidth,contentWidth,text);
   const ImVec2 pos(ImGui::GetCursorScreenPos().x+std::max(0.0f,(contentWidth-extent.x)*.5f),ImGui::GetCursorScreenPos().y);
   ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),size,pos,disabled?ImGui::GetColorU32(ImGuiCol_TextDisabled):ImGui::GetColorU32(color),text,nullptr,contentWidth);
   ImGui::Dummy(ImVec2(1.0f,extent.y+ImGui::GetStyle().ItemSpacing.y));
  };
  const float cardWidth=std::min(available,std::max(available*.76f,300.0f));
  auto centerCard=[&](const char*id,const char*title,const char*subtitle,ImVec4 tint){
   ImGui::SetCursorPosX(std::max(0.0f,(available-cardWidth)*.5f));
   ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(tint.x*.20f,tint.y*.20f,tint.z*.20f,.95f));
   ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(tint.x,tint.y,tint.z,.75f));
   if(ImGui::BeginChild(id,ImVec2(cardWidth,124.0f),true,ImGuiWindowFlags_NoScrollbar)){
    const float inner=ImGui::GetContentRegionAvail().x;
    const ImVec2 titleSize=ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize()*1.8f,inner,inner,title);
    ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,(inner-titleSize.x)*.5f+ImGui::GetStyle().WindowPadding.x));
    ImGui::SetWindowFontScale(1.8f);ImGui::TextColored(tint,"%s",title);ImGui::SetWindowFontScale(1.0f);
    const ImVec2 nameSize=ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize()*1.5f,inner,inner,subtitle);
    ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,(inner-nameSize.x)*.5f+ImGui::GetStyle().WindowPadding.x));
    ImGui::SetWindowFontScale(1.5f);ImGui::TextUnformatted(subtitle);ImGui::SetWindowFontScale(1.0f);
   }
   ImGui::EndChild();ImGui::PopStyleColor(2);
  };
  auto connector=[&](){
   const float x=ImGui::GetCursorScreenPos().x+available*.5f;
   const float y=ImGui::GetCursorScreenPos().y;
   ImGui::GetWindowDrawList()->AddLine(ImVec2(x,y),ImVec2(x,y+22.0f),IM_COL32(105,130,157,190),2.0f);
   ImGui::Dummy(ImVec2(1,22));
  };
  centerCard("GOOLGraphRoot","GOOL ENTRY",Crash::eidToEName(selectedEntry->eid).c_str(),ImVec4(.42f,.72f,1,1));
  connector();
  ImGui::SetCursorPosX(std::max(0.0f,(available-cardWidth)*.5f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.12f,.17f,.23f,.96f));
  ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.30f,.48f,.68f,.8f));
  if(ImGui::BeginChild("GOOLGraphStates",ImVec2(cardWidth,116),true)){
   centeredText("STATE LOGIC",ImVec4(.48f,.72f,1,1));
   centeredText("This entry is organized into state descriptors. Each descriptor selects the GOOL code responsible for a part of the object's behaviour.",ImGui::GetStyleColorVec4(ImGuiCol_Text));
   ImGui::Separator();
   char stateSummary[128];std::snprintf(stateSummary,sizeof(stateSummary),"%zu states · %zu resolved references",gool->stateDescriptors.size(),gool->stateReferences.size()-std::min(gool->stateReferences.size(),unresolved));
   centeredText(stateSummary,ImGui::GetStyleColorVec4(ImGuiCol_Text));
  }
  ImGui::EndChild();ImGui::PopStyleColor(2);
  connector();
  connector();
  if(ImGui::BeginTable("GOOLGraphMiddleCards",2,ImGuiTableFlags_SizingStretchSame)){
   ImGui::TableNextRow();
   ImGui::TableSetColumnIndex(0);
   ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.12f,.18f,.25f,.96f));ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.30f,.55f,.82f,.8f));
   if(ImGui::BeginChild("GOOLGraphPoolCard",ImVec2(0,150),true)){
    centeredText("DATA POOL",ImVec4(.48f,.72f,1,1));
    centeredText("A descriptor's GOOL index selects a word here. That word supplies the EID used to resolve the target GOOL.",ImGui::GetStyleColorVec4(ImGuiCol_Text));
    ImGui::Separator();
    char poolSummary[128];std::snprintf(poolSummary,sizeof(poolSummary),"%zu local · %zu external references",localCount,externalCount);
    centeredText(poolSummary,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),true);
    const float buttonWidth=ImGui::CalcTextSize("Open Data Pool").x+ImGui::GetStyle().FramePadding.x*2;
    ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,(ImGui::GetContentRegionAvail().x-buttonWidth)*.5f+ImGui::GetStyle().WindowPadding.x));
    if(ImGui::SmallButton("Open Data Pool"))showGoolDataPool_=true;
   }ImGui::EndChild();ImGui::PopStyleColor(2);
   ImGui::TableSetColumnIndex(1);
   ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.19f,.15f,.25f,.96f));ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.62f,.43f,.83f,.8f));
   if(ImGui::BeginChild("GOOLGraphHooksCard",ImVec2(0,150),true)){
    centeredText("HOOKS",ImVec4(.76f,.59f,1,1));
    centeredText("Event, Transition, and Code hooks route execution to instruction offsets, locally or in another GOOL.",ImGui::GetStyleColorVec4(ImGuiCol_Text));
    ImGui::Separator();
    char hookSummary[96];std::snprintf(hookSummary,sizeof(hookSummary),"%zu resolved destinations",validHooks);
    centeredText(hookSummary,ImVec4(.38f,.82f,.58f,1));
    if(problemHooks){char problemSummary[96];std::snprintf(problemSummary,sizeof(problemSummary),"%zu unresolved destinations",problemHooks);centeredText(problemSummary,ImVec4(1,.68f,.30f,1));}
    const float buttonWidth=ImGui::CalcTextSize("Open State Descriptions").x+ImGui::GetStyle().FramePadding.x*2;
    ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,(ImGui::GetContentRegionAvail().x-buttonWidth)*.5f+ImGui::GetStyle().WindowPadding.x));
    if(ImGui::SmallButton("Open State Descriptions"))showGoolStateDescriptions_=true;
   }ImGui::EndChild();ImGui::PopStyleColor(2);
   ImGui::EndTable();
  }
  connector();
  ImGui::SetCursorPosX(std::max(0.0f,(available-cardWidth)*.5f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.13f,.21f,.17f,.96f));ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.32f,.70f,.48f,.8f));
  const float dependencyHeight=std::max(100.0f,70.0f+static_cast<float>(dependencies.size())*48.0f);
  if(ImGui::BeginChild("GOOLGraphDependencies",ImVec2(cardWidth,dependencyHeight),true)){
   centeredText("GOOL DEPENDENCIES",ImVec4(.42f,.82f,.57f,1));
   char dependencyCount[128];std::snprintf(dependencyCount,sizeof(dependencyCount),"%zu unique loaded external GOOL entries",dependencies.size());
   centeredText(dependencyCount,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),true);
   ImGui::Separator();
   if(dependencies.empty())centeredText("No resolved external GOOL dependencies were found.",ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),true);
   for(const auto&location:dependencies){
    if(location.first>=nsf_.chunks().size()||location.second>=nsf_.chunks()[location.first].entries.size())continue;
    const auto&target=nsf_.chunks()[location.first].entries[location.second];
    ImGui::PushID(static_cast<int>(location.first*65536U+location.second));
    const std::string label=Crash::eidToEName(target.eid)+"  ·  "+std::to_string(target.eid);
    if(ImGui::Button((label+"##open_dependency").c_str(),ImVec2(-1,0))){
     selectedChunk_=static_cast<int>(location.first);selectedEntry_=static_cast<int>(location.second);
     selectedEntity_=-1;selectedItem_=-1;selectedSpawn_=-1;openSelectedHierarchy_=true;
    }
    char locationText[128];std::snprintf(locationText,sizeof(locationText),"Chunk %zu · Entry %zu%s",location.first,location.second,target.gool.decoded?"":" · not decoded");
    centeredText(locationText,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),true);
    ImGui::PopID();
   }
   if(unresolved>0){char t[128];std::snprintf(t,sizeof(t),"%zu unresolved or invalid state references",unresolved);centeredText(t,ImVec4(1,.65f,.30f,1));}
   if(externalCount>dependencies.size()){char t[128];std::snprintf(t,sizeof(t),"%zu external/ambiguous descriptor references in total",externalCount);centeredText(t,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),true);}
   if(localCount>0){char t[128];std::snprintf(t,sizeof(t),"%zu local descriptor references remain within this GOOL",localCount);centeredText(t,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),true);}
  }
  ImGui::EndChild();ImGui::PopStyleColor(2);
  ImGui::Spacing();

 }
 ImGui::EndChild();
 ImGui::End();
}


void EditorUI::drawGoolBlueprintGraph(){
 const Crash::NsfEntry* selectedEntry=nullptr;
 const Crash::NsfGool* gool=nullptr;
 if(nsfValid_&&selectedChunk_>=0&&selectedEntry_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(chunk.type==0&&static_cast<std::size_t>(selectedEntry_)<chunk.entries.size()){
   const auto&entry=chunk.entries[static_cast<std::size_t>(selectedEntry_)];
   if(entry.valid&&entry.type==11){selectedEntry=&entry;gool=&entry.gool;}
  }
 }
 if(!selectedEntry||!gool)return;
 static bool dockInitialized=false;
 if(!dockInitialized){
  if(ImGuiWindow* viewer=ImGui::FindWindowByName("3D Zone Editor"))
   if(viewer->DockNode){ImGui::SetNextWindowDockID(viewer->DockNode->ID,ImGuiCond_Always);dockInitialized=true;}
 }
 ImGui::Begin("GOOL Blueprint");
 ImGui::Text("%s  ·  %zu instructions",Crash::eidToEName(selectedEntry->eid).c_str(),gool->instructions.size());
 if(!gool->decoded||gool->instructions.empty()){
  ImGui::TextDisabled("No decoded instruction flow is available for this GOOL entry.");
  ImGui::End();return;
 }
 const auto& graph=blueprintCacheFor(*gool).graph;
 if(graph.blocks.empty()){
  ImGui::TextDisabled("No basic blocks were produced by control-flow analysis.");
  ImGui::End();return;
 }
 if(blueprintSelectedEid_!=selectedEntry->eid){blueprintSelectedEid_=selectedEntry->eid;selectedGoolBlueprintBlock_=0;showGoolBlueprintInstructions_=true;}
 ImGui::TextDisabled("Read-only execution graph · click a block to inspect its instructions. Unknown and dynamic destinations are kept explicit.");
 ImGui::SameLine();
 ImGui::TextDisabled("%zu blocks  |  %zu connections",graph.blocks.size(),graph.edges.size());
 ImGui::Separator();
 const float nodeWidth=250.0f,columnGap=150.0f,rowGap=76.0f;
 const std::size_t columns=3;
 const std::size_t rows=(graph.blocks.size()+columns-1U)/columns;
 const float textWidth=nodeWidth-20.0f;
 std::vector<std::string> firstLines(graph.blocks.size()),lastLines(graph.blocks.size());
 std::vector<float> firstLineHeights(graph.blocks.size(),0.0f),nodeHeights(graph.blocks.size(),140.0f),rowHeights(rows,140.0f);
 for(std::size_t b=0;b<graph.blocks.size();++b){
  const auto&block=graph.blocks[b];
  if(block.startInstruction>=gool->instructions.size())continue;
  const auto&first=gool->instructions[block.startInstruction];
  firstLines[b]=first.mnemonic+(first.arguments.empty()?std::string{}:"  "+first.arguments);
  const float firstHeight=ImGui::CalcTextSize(firstLines[b].c_str(),nullptr,false,textWidth).y;
  firstLineHeights[b]=firstHeight;
  const std::size_t last=block.endInstruction>block.startInstruction?block.endInstruction-1U:block.startInstruction;
  float lastHeight=0.0f;
  if(last<gool->instructions.size()&&last!=block.startInstruction){
   const auto&end=gool->instructions[last];
   lastLines[b]="End: "+end.mnemonic+(end.arguments.empty()?std::string{}:"  "+end.arguments);
   lastHeight=ImGui::CalcTextSize(lastLines[b].c_str(),nullptr,false,textWidth).y;
  }
  nodeHeights[b]=std::max(140.0f,61.0f+firstHeight+(lastLines[b].empty()?0.0f:8.0f+lastHeight)+12.0f);
  rowHeights[b/columns]=std::max(rowHeights[b/columns],nodeHeights[b]);
 }
 const float gridWidth=static_cast<float>(columns)*nodeWidth+static_cast<float>(columns-1U)*columnGap;
 const float canvasWidth=std::max(ImGui::GetContentRegionAvail().x,gridWidth+40.0f);
 float contentHeight=40.0f;
 for(float height:rowHeights)contentHeight+=height+rowGap;
 const float canvasHeight=std::max(ImGui::GetContentRegionAvail().y,contentHeight);
 if(ImGui::BeginChild("GOOLBlueprintCanvas",ImVec2(0,0),true,ImGuiWindowFlags_HorizontalScrollbar)){
  const ImVec2 origin=ImGui::GetCursorScreenPos();
  const ImVec2 extent(canvasWidth,canvasHeight);
  ImGui::Dummy(extent);
  ImDrawList* draw=ImGui::GetWindowDrawList();
  const ImVec2 clipMin=ImGui::GetWindowPos();
  const ImVec2 clipMax(clipMin.x+ImGui::GetWindowSize().x,clipMin.y+ImGui::GetWindowSize().y);
  draw->PushClipRect(clipMin,clipMax,true);
  auto nodePosition=[&](std::size_t block){
   const std::size_t col=block%columns,row=block/columns;
   const float centeredOffset=(canvasWidth-gridWidth)*0.5f;
   float rowOffset=0.0f;for(std::size_t r=0;r<row;++r)rowOffset+=rowHeights[r]+rowGap;
   return ImVec2(origin.x+centeredOffset+static_cast<float>(col)*(nodeWidth+columnGap),origin.y+20.0f+rowOffset);
  };
  std::vector<std::size_t> instructionToBlock(gool->instructions.size(),0U);
  for(std::size_t b=0;b<graph.blocks.size();++b)
   for(std::size_t i=graph.blocks[b].startInstruction;i<graph.blocks[b].endInstruction&&i<instructionToBlock.size();++i)instructionToBlock[i]=b;
  auto edgeColor=[](Crash::NsfGoolFlowEdgeKind kind)->ImU32{
   switch(kind){
    case Crash::NsfGoolFlowEdgeKind::BranchTrue:return IM_COL32(80,205,125,235);
    case Crash::NsfGoolFlowEdgeKind::BranchFalse:return IM_COL32(235,170,65,235);
    case Crash::NsfGoolFlowEdgeKind::Branch:return IM_COL32(105,170,245,235);
    case Crash::NsfGoolFlowEdgeKind::Call:return IM_COL32(190,130,245,235);
    case Crash::NsfGoolFlowEdgeKind::StateTransition:
    case Crash::NsfGoolFlowEdgeKind::StateTransitionTrue:
    case Crash::NsfGoolFlowEdgeKind::StateTransitionFalse:return IM_COL32(225,115,205,235);
    case Crash::NsfGoolFlowEdgeKind::ExternalCall:return IM_COL32(145,155,170,220);
    case Crash::NsfGoolFlowEdgeKind::Return:return IM_COL32(220,105,95,220);
    default:return IM_COL32(115,155,190,220);
   }
  };
  auto edgeLabel=[](Crash::NsfGoolFlowEdgeKind kind)->const char*{
   switch(kind){
    case Crash::NsfGoolFlowEdgeKind::Fallthrough:return "Next";
    case Crash::NsfGoolFlowEdgeKind::Branch:return "Branch";
    case Crash::NsfGoolFlowEdgeKind::BranchTrue:return "True";
    case Crash::NsfGoolFlowEdgeKind::BranchFalse:return "False";
    case Crash::NsfGoolFlowEdgeKind::Call:return "Call";
    case Crash::NsfGoolFlowEdgeKind::ExternalCall:return "External call";
    case Crash::NsfGoolFlowEdgeKind::StateTransition:return "State change";
    case Crash::NsfGoolFlowEdgeKind::StateTransitionTrue:return "State if true";
    case Crash::NsfGoolFlowEdgeKind::StateTransitionFalse:return "State if false";
    case Crash::NsfGoolFlowEdgeKind::Return:return "Return";
   }return "Flow";
  };
  std::vector<std::size_t> sourcePorts(graph.blocks.size(),0U);
  for(const auto&edge:graph.edges){
   if(edge.sourceBlock>=graph.blocks.size()||!edge.targetValid||edge.targetInstruction>=instructionToBlock.size())continue;
   const std::size_t targetBlock=instructionToBlock[edge.targetInstruction];
   const ImVec2 source=nodePosition(edge.sourceBlock),target=nodePosition(targetBlock);
   const float sourceY=source.y+nodeHeights[edge.sourceBlock]*.5f+(static_cast<float>(sourcePorts[edge.sourceBlock]++)-0.5f)*13.0f;
   const float targetY=target.y+nodeHeights[targetBlock]*.5f;
   const ImVec2 p0(source.x+nodeWidth,sourceY),p3(target.x,targetY);
   const float bend=std::max(55.0f,std::abs(p3.x-p0.x)*.42f);
   draw->AddBezierCubic(p0,ImVec2(p0.x+bend,p0.y),ImVec2(p3.x-bend,p3.y),p3,edgeColor(edge.kind),2.5f);
   draw->AddCircleFilled(p0,4.5f,edgeColor(edge.kind));
   draw->AddCircleFilled(p3,4.5f,edgeColor(edge.kind));
   const ImVec2 mid((p0.x+p3.x)*.5f,(p0.y+p3.y)*.5f);
   draw->AddText(ImVec2(mid.x-15.0f,mid.y-17.0f),edgeColor(edge.kind),edgeLabel(edge.kind));
  }
  for(std::size_t b=0;b<graph.blocks.size();++b){
   const auto&block=graph.blocks[b];const ImVec2 pos=nodePosition(b);
   ImGui::SetCursorScreenPos(pos);
   ImGui::PushID(static_cast<int>(b));
   const float nodeHeight=nodeHeights[b];
   const bool clicked=ImGui::InvisibleButton("##BlueprintBlock",ImVec2(nodeWidth,nodeHeight));
   const bool hovered=ImGui::IsItemHovered();
   if(clicked)selectedGoolBlueprintBlock_=b;
   const ImVec2 min=pos,max(pos.x+nodeWidth,pos.y+nodeHeight);
   const ImU32 border=b==selectedGoolBlueprintBlock_?IM_COL32(105,190,255,255):hovered?IM_COL32(150,175,205,255):IM_COL32(75,95,120,255);
   draw->AddRectFilled(min,max,b==selectedGoolBlueprintBlock_?IM_COL32(38,63,86,250):IM_COL32(34,42,53,248),8.0f);
   draw->AddRect(min,max,border,8.0f,0, b==selectedGoolBlueprintBlock_?2.5f:1.5f);
   draw->AddRectFilled(min,ImVec2(max.x,min.y+27.0f),IM_COL32(49,78,105,255),8.0f,ImDrawFlags_RoundCornersTop);
   char title[64];std::snprintf(title,sizeof(title),"EXECUTION  ·  BB%zu",b);
   draw->AddText(ImVec2(min.x+10,min.y+7),IM_COL32(225,237,249,255),title);
   char range[96];std::snprintf(range,sizeof(range),"Instructions %zu–%zu",block.startInstruction,block.endInstruction-1U);
   draw->AddText(ImVec2(min.x+10,min.y+37),IM_COL32(170,190,210,255),range);
   float textY=min.y+61.0f;
   if(!firstLines[b].empty()){
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(min.x+10,textY),IM_COL32(225,225,215,255),firstLines[b].c_str(),nullptr,textWidth);
    textY+=firstLineHeights[b]+8.0f;
   }
   if(!lastLines[b].empty())
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(min.x+10,textY),IM_COL32(190,205,220,255),lastLines[b].c_str(),nullptr,textWidth);
   draw->AddCircleFilled(ImVec2(min.x,min.y+nodeHeight*.5f),5.0f,IM_COL32(145,165,185,255));
   draw->AddCircleFilled(ImVec2(max.x,min.y+nodeHeight*.5f),5.0f,IM_COL32(145,165,185,255));
   ImGui::PopID();
  }
  draw->PopClipRect();
 }
 ImGui::EndChild();
 ImGui::End();
 }

 void EditorUI::drawGoolBlueprintInstructions(){
  const Crash::NsfEntry* selectedEntry=nullptr;
  const Crash::NsfGool* gool=nullptr;
  if(nsfValid_&&selectedChunk_>=0&&selectedEntry_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
   const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
   if(chunk.type==0&&static_cast<std::size_t>(selectedEntry_)<chunk.entries.size()){
    const auto&entry=chunk.entries[static_cast<std::size_t>(selectedEntry_)];
    if(entry.valid&&entry.type==11){selectedEntry=&entry;gool=&entry.gool;}
   }
  }
  if(!selectedEntry||!gool){showGoolBlueprintInstructions_=false;blueprintSelectedEid_=0;return;}
  const auto& graph=blueprintCacheFor(*gool).graph;
  if(graph.blocks.empty()){showGoolBlueprintInstructions_=false;return;}
  selectedGoolBlueprintBlock_=std::min(selectedGoolBlueprintBlock_,graph.blocks.size()-1U);
  const auto&chosen=graph.blocks[selectedGoolBlueprintBlock_];
  ImGuiID bottomDockId=0;
  if(ImGuiWindow* frameGroups=ImGui::FindWindowByName("GOOL Frame Groups"))if(frameGroups->DockNode)bottomDockId=frameGroups->DockNode->ID;
  if(bottomDockId==0)if(ImGuiWindow* console=ImGui::FindWindowByName("Console / Diagnostics"))if(console->DockNode)bottomDockId=console->DockNode->ID;
  if(bottomDockId!=0)ImGui::SetNextWindowDockID(bottomDockId,ImGuiCond_FirstUseEver);
  if(ImGui::Begin("GOOL Blueprint Instructions",&showGoolBlueprintInstructions_,ImGuiWindowFlags_HorizontalScrollbar)){
   ImGui::Text("%s  |  BB%zu  |  instructions %zu–%zu",Crash::eidToEName(selectedEntry->eid).c_str(),selectedGoolBlueprintBlock_,chosen.startInstruction,chosen.endInstruction-1U);
   ImGui::TextDisabled("Data-flow links track stable stack, argument, and object references. Unknown instructions and engine calls break propagation.");
   auto& analysisCache=blueprintCacheFor(*gool);
   prepareBlueprintDataFlow(analysisCache,*gool);
   if(ImGui::BeginTable("GOOLBlueprintInstructionEffects",8,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingStretchProp,ImVec2(0,0))){
    ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,48.0f);
    ImGui::TableSetupColumn("Instruction",ImGuiTableColumnFlags_WidthFixed,105.0f);
    ImGui::TableSetupColumn("Reads",ImGuiTableColumnFlags_WidthStretch,1.15f);
    ImGui::TableSetupColumn("Writes",ImGuiTableColumnFlags_WidthStretch,1.15f);
    ImGui::TableSetupColumn("Uncertain",ImGuiTableColumnFlags_WidthStretch,1.0f);
    ImGui::TableSetupColumn("From",ImGuiTableColumnFlags_WidthStretch,1.35f);
    ImGui::TableSetupColumn("Used by",ImGuiTableColumnFlags_WidthStretch,1.35f);
    ImGui::TableSetupColumn("Execution",ImGuiTableColumnFlags_WidthStretch,1.4f);
    ImGui::TableHeadersRow();
    for(std::size_t instructionIndex=chosen.startInstruction;instructionIndex<chosen.endInstruction&&instructionIndex<gool->instructions.size();++instructionIndex){
     const auto&instruction=gool->instructions[instructionIndex];
     const auto&row=analysisCache.rows[instructionIndex];
     ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",instructionIndex);ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(instruction.mnemonic.c_str());
     ImGui::TableSetColumnIndex(2);if(!row.analysis.operandsKnown)ImGui::TextDisabled("Unknown");else if(row.reads.empty())ImGui::TextDisabled("—");else ImGui::TextWrapped("%s",row.reads.c_str());
     ImGui::TableSetColumnIndex(3);if(!row.analysis.operandsKnown)ImGui::TextDisabled("Unknown");else if(row.writes.empty())ImGui::TextDisabled("—");else ImGui::TextWrapped("%s",row.writes.c_str());
     ImGui::TableSetColumnIndex(4);if(!row.analysis.operandsKnown)ImGui::TextDisabled("Unknown");else if(row.uncertain.empty())ImGui::TextDisabled("—");else ImGui::TextWrapped("%s",row.uncertain.c_str());
     ImGui::TableSetColumnIndex(5);if(row.from.empty())ImGui::TextDisabled("—");else ImGui::TextWrapped("%s",row.from.c_str());
     ImGui::TableSetColumnIndex(6);if(row.usedBy.empty())ImGui::TextDisabled("—");else ImGui::TextWrapped("%s",row.usedBy.c_str());
     ImGui::TableSetColumnIndex(7);if(row.analysis.executionEffect.empty())ImGui::TextDisabled("—");else ImGui::TextWrapped("%s",row.analysis.executionEffect.c_str());
    }
    ImGui::EndTable();
   }
  }
  ImGui::End();
 }

 void EditorUI::drawGoolDataWindows(){
 const Crash::NsfGool* gool=nullptr; std::uint32_t goolEid=0;
 if(nsfValid_&&selectedChunk_>=0&&selectedEntry_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(chunk.type==0&&static_cast<std::size_t>(selectedEntry_)<chunk.entries.size()){
   const auto&entry=chunk.entries[static_cast<std::size_t>(selectedEntry_)];
   if(entry.valid&&entry.type==11){gool=&entry.gool;goolEid=entry.eid;}
  }
 }
 if(!gool){showGoolDataPool_=showGoolStateMap_=showGoolStateDescriptions_=showGoolFrameGroups_=false;activeGoolChunk_=-1;activeGoolEntry_=-1;return;}
 if(activeGoolChunk_!=selectedChunk_||activeGoolEntry_!=selectedEntry_){activeGoolChunk_=selectedChunk_;activeGoolEntry_=selectedEntry_;showGoolDataPool_=showGoolStateMap_=showGoolStateDescriptions_=showGoolFrameGroups_=true;focusGoolDataPool_=true;frameGroupEidBuffers_.clear();frameGroupEidBuffers_.resize(gool->frameGroups.size());for(std::size_t i=0;i<gool->frameGroups.size();++i){const auto name=Crash::eidToEName(gool->frameGroups[i].eid);std::snprintf(frameGroupEidBuffers_[i].data(),frameGroupEidBuffers_[i].size(),"%s",name.c_str());}}
 ImGuiID consoleDockId=0;
 if(ImGuiWindow* console=ImGui::FindWindowByName("Console / Diagnostics"))if(console->DockNode)consoleDockId=console->DockNode->ID;
 const ImGuiWindowFlags flags=ImGuiWindowFlags_HorizontalScrollbar;
 if(consoleDockId!=0)ImGui::SetNextWindowDockID(consoleDockId,ImGuiCond_FirstUseEver);
 if(focusGoolDataPool_)ImGui::SetNextWindowFocus();
 if(ImGui::Begin("GOOL Data Pool",&showGoolDataPool_,flags)){
  ImGui::Text("%s | Data pool: %zu words",Crash::eidToEName(goolEid).c_str(),gool->dataPool.size());
  if(!gool->dataPool.empty()&&ImGui::BeginTable("GOOLDataPoolV2",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY)){
   ImGui::TableSetupColumn("Index",ImGuiTableColumnFlags_WidthFixed,65.0f);ImGui::TableSetupColumn("Hex",ImGuiTableColumnFlags_WidthFixed,125.0f);ImGui::TableSetupColumn("EID name",ImGuiTableColumnFlags_WidthFixed,190.0f);ImGui::TableSetupColumn("Signed value",ImGuiTableColumnFlags_WidthFixed,115.0f);ImGui::TableHeadersRow();
   for(std::size_t i=0;i<gool->dataPool.size();++i){const auto raw=static_cast<std::uint32_t>(gool->dataPool[i]);ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",i);ImGui::TableSetColumnIndex(1);ImGui::Text("0x%08X",raw);ImGui::TableSetColumnIndex(2);if(Crash::isValidEid(raw))ImGui::TextUnformatted(Crash::eidToEName(raw).c_str());else ImGui::TextDisabled("-");ImGui::TableSetColumnIndex(3);ImGui::Text("%d",gool->dataPool[i]);}ImGui::EndTable();
  }else if(gool->dataPool.empty())ImGui::TextDisabled("No data-pool words in this GOOL entry.");
 }ImGui::End();
 if(consoleDockId!=0)ImGui::SetNextWindowDockID(consoleDockId,ImGuiCond_FirstUseEver);
 if(ImGui::Begin("GOOL State Map",&showGoolStateMap_,flags)){
  ImGui::Text("State map: %zu entries",gool->stateMap.size());
  if(!gool->stateMap.empty()&&ImGui::BeginTable("GOOLStateMapV2",2,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollY)){
   ImGui::TableSetupColumn("Index",ImGuiTableColumnFlags_WidthFixed,75.0f);ImGui::TableSetupColumn("Mapped value",ImGuiTableColumnFlags_WidthFixed,150.0f);ImGui::TableHeadersRow();
   for(std::size_t i=0;i<gool->stateMap.size();++i){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",i);ImGui::TableSetColumnIndex(1);ImGui::Text("%d",gool->stateMap[i]);}ImGui::EndTable();
  }else ImGui::TextDisabled("No state-map entries in this GOOL entry.");
 }ImGui::End();
 if(consoleDockId!=0)ImGui::SetNextWindowDockID(consoleDockId,ImGuiCond_FirstUseEver);
 if(ImGui::Begin("GOOL State Descriptions",&showGoolStateDescriptions_,flags)){
  ImGui::Text("State descriptors: %zu",gool->stateDescriptors.size());
  bool goolReferencesNeedRefresh=false;
  if(!gool->stateDescriptors.empty()&&ImGui::BeginTable("GOOLStateDescriptorsV2",10,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY,ImVec2(0,190))){
   ImGui::TableSetupColumn("State flags",ImGuiTableColumnFlags_WidthFixed,115.0f);ImGui::TableSetupColumn("Block flags",ImGuiTableColumnFlags_WidthFixed,115.0f);ImGui::TableSetupColumn("GOOL index",ImGuiTableColumnFlags_WidthFixed,90.0f);ImGui::TableSetupColumn("Event hook",ImGuiTableColumnFlags_WidthFixed,95.0f);ImGui::TableSetupColumn("Transition hook",ImGuiTableColumnFlags_WidthFixed,125.0f);ImGui::TableSetupColumn("Code hook",ImGuiTableColumnFlags_WidthFixed,90.0f);ImGui::TableSetupColumn("Referenced EID",ImGuiTableColumnFlags_WidthFixed,185.0f);ImGui::TableSetupColumn("GOOL reference",ImGuiTableColumnFlags_WidthFixed,125.0f);ImGui::TableSetupColumn("Target",ImGuiTableColumnFlags_WidthFixed,155.0f);ImGui::TableSetupColumn("Edit",ImGuiTableColumnFlags_WidthFixed,48.0f);ImGui::TableHeadersRow();
   static int selectedExternalDescriptor=0;
   selectedExternalDescriptor=std::clamp(selectedExternalDescriptor,0,static_cast<int>(gool->stateDescriptors.size()-1));
   for(std::size_t i=0;i<gool->stateDescriptors.size();++i){
    auto d=gool->stateDescriptors[i];const auto*reference=i<gool->stateReferences.size()?&gool->stateReferences[i]:nullptr;bool changed=false;
    ImGui::TableNextRow();ImGui::PushID(static_cast<int>(i));
    
    ImGui::TableSetColumnIndex(0);std::uint32_t stateFlags=static_cast<std::uint32_t>(d.stateFlags);if(ImGui::InputScalar("##stateFlags",ImGuiDataType_U32,&stateFlags,nullptr,nullptr,"%08X")){d.stateFlags=static_cast<std::int32_t>(stateFlags);changed=true;}
    ImGui::TableSetColumnIndex(1);std::uint32_t blockFlags=static_cast<std::uint32_t>(d.blockFlags);if(ImGui::InputScalar("##blockFlags",ImGuiDataType_U32,&blockFlags,nullptr,nullptr,"%08X")){d.blockFlags=static_cast<std::int32_t>(blockFlags);changed=true;}
    ImGui::TableSetColumnIndex(2);std::int16_t goolIndex=d.goolIndex;if(ImGui::InputScalar("##goolIndex",ImGuiDataType_S16,&goolIndex)){d.goolIndex=goolIndex;changed=true;}
    ImGui::TableSetColumnIndex(3);std::int16_t eventHook=d.eventHook;if(ImGui::InputScalar("##eventHook",ImGuiDataType_S16,&eventHook)){d.eventHook=eventHook;changed=true;}
    ImGui::TableSetColumnIndex(4);std::int16_t transHook=d.transHook;if(ImGui::InputScalar("##transHook",ImGuiDataType_S16,&transHook)){d.transHook=transHook;changed=true;}
    ImGui::TableSetColumnIndex(5);std::int16_t codeHook=d.codeHook;if(ImGui::InputScalar("##codeHook",ImGuiDataType_S16,&codeHook)){d.codeHook=codeHook;changed=true;}
    if(changed&&nsf_.setGoolStateDescriptor(static_cast<std::size_t>(activeGoolChunk_),static_cast<std::size_t>(activeGoolEntry_),i,d))goolReferencesNeedRefresh=true;
    ImGui::TableSetColumnIndex(6);reference=i<gool->stateReferences.size()?&gool->stateReferences[i]:nullptr;
    if(reference&&reference->status!=Crash::NsfGoolStateReferenceStatus::InvalidDataIndex){const bool external=reference->status==Crash::NsfGoolStateReferenceStatus::External||reference->status==Crash::NsfGoolStateReferenceStatus::Ambiguous;if(Crash::isValidEid(reference->eid)){if(external)ImGui::TextColored(ImVec4(.25f,.85f,1.0f,1.0f),"%08X (%s)",reference->eid,Crash::eidToEName(reference->eid).c_str());else ImGui::Text("%08X (%s)",reference->eid,Crash::eidToEName(reference->eid).c_str());}else ImGui::Text("0x%08X",reference->eid);}else ImGui::TextDisabled("-");
    ImGui::TableSetColumnIndex(7);if(!reference)ImGui::TextDisabled("Unavailable");else{const char*status=reference->status==Crash::NsfGoolStateReferenceStatus::InvalidDataIndex?"Invalid data index":reference->status==Crash::NsfGoolStateReferenceStatus::Local?"Local":reference->status==Crash::NsfGoolStateReferenceStatus::External?"External":"Ambiguous";ImGui::TextUnformatted(status);}
    ImGui::TableSetColumnIndex(8);if(reference&&reference->targetFound&&reference->status==Crash::NsfGoolStateReferenceStatus::Local)ImGui::TextUnformatted("This entry");else if(reference&&reference->targetFound)ImGui::Text("Chunk %03zu / Entry %03zu",reference->targetChunkIndex,reference->targetEntryIndex);else if(reference&&reference->status==Crash::NsfGoolStateReferenceStatus::External)ImGui::TextDisabled("Not in this NSF");else ImGui::TextDisabled("-");
     ImGui::TableSetColumnIndex(9);if(ImGui::Button(("Edit##descriptor_edit_"+std::to_string(i)).c_str()))selectedExternalDescriptor=static_cast<int>(i);
     ImGui::PopID();
    }ImGui::EndTable();
   ImGui::Separator();
   ImGui::Text("External GOOL target — descriptor %d",selectedExternalDescriptor);
   ImGui::TextDisabled("Use the Edit button in the table to choose which descriptor to modify.");
   const auto&selectedDescriptor=gool->stateDescriptors[static_cast<std::size_t>(selectedExternalDescriptor)];
   const bool validPoolIndex=selectedDescriptor.goolIndex>=0&&static_cast<std::size_t>(selectedDescriptor.goolIndex)<gool->dataPool.size();
   int externalIndex=selectedDescriptor.goolIndex;
   ImGui::SetNextItemWidth(180.0f);
   if(ImGui::InputInt("Data Pool index",&externalIndex)&&externalIndex>=0&&externalIndex<=32767&&static_cast<std::size_t>(externalIndex)<gool->dataPool.size()){
    auto edited=selectedDescriptor;edited.goolIndex=static_cast<std::int16_t>(externalIndex);
    if(nsf_.setGoolStateDescriptor(static_cast<std::size_t>(activeGoolChunk_),static_cast<std::size_t>(activeGoolEntry_),static_cast<std::size_t>(selectedExternalDescriptor),edited))goolReferencesNeedRefresh=true;
   }
   if(validPoolIndex){
    const std::size_t poolIndex=static_cast<std::size_t>(selectedDescriptor.goolIndex);
    std::uint32_t targetEid=static_cast<std::uint32_t>(gool->dataPool[poolIndex]);
    ImGui::TextDisabled("Choose a loaded GOOL, or enter an EID manually. Selecting this GOOL makes the reference local.");
    std::map<std::uint32_t,std::size_t> targetCounts;
    std::map<std::uint32_t,std::pair<std::size_t,std::size_t>> targetLocations;
    for(std::size_t ci=0;ci<nsf_.chunks().size();++ci){
     const auto&targetChunk=nsf_.chunks()[ci];
     if(targetChunk.type!=0||!targetChunk.entryTableValid)continue;
     for(std::size_t ei=0;ei<targetChunk.entries.size();++ei){
      const auto&targetEntry=targetChunk.entries[ei];
      if(!targetEntry.valid||targetEntry.type!=11)continue;
      ++targetCounts[targetEntry.eid];
      targetLocations.try_emplace(targetEntry.eid,ci,ei);
     }
    }
    std::vector<std::uint32_t> targetEids{goolEid};
    std::vector<std::string> targetLabels;
    char localEid[9];std::snprintf(localEid,sizeof(localEid),"%08X",goolEid);
    targetLabels.push_back("Local — "+Crash::eidToEName(goolEid)+" (0x"+std::string(localEid)+", this entry)");
    int selectedTarget=0;
    for(const auto&[eid,count]:targetCounts){
     if(eid==goolEid||count!=1)continue;
     const auto location=targetLocations.at(eid);
     char targetHex[9];std::snprintf(targetHex,sizeof(targetHex),"%08X",eid);
     char targetLabel[256];
     std::snprintf(targetLabel,sizeof(targetLabel),"%s (0x%s) — Chunk %zu, Entry %zu%s",
      Crash::eidToEName(eid).c_str(),targetHex,location.first,location.second,
      nsf_.chunks()[location.first].entries[location.second].gool.decoded?"":" (not decoded)");
     targetEids.push_back(eid);targetLabels.emplace_back(targetLabel);
     if(eid==targetEid)selectedTarget=static_cast<int>(targetEids.size()-1);
    }
    if(targetEid!=goolEid&&std::find(targetEids.begin()+1,targetEids.end(),targetEid)==targetEids.end()){
     const bool ambiguous=targetCounts.contains(targetEid)&&targetCounts[targetEid]>1;
     targetEids.push_back(targetEid);
     char unresolvedLabel[96];
     std::snprintf(unresolvedLabel,sizeof(unresolvedLabel),ambiguous?"Ambiguous target — 0x%08X":"Custom / unresolved — 0x%08X",targetEid);
     targetLabels.emplace_back(unresolvedLabel);
     selectedTarget=static_cast<int>(targetEids.size()-1);
    }
    std::vector<const char*> targetLabelPointers;
    targetLabelPointers.reserve(targetLabels.size());
    for(const auto&label:targetLabels)targetLabelPointers.push_back(label.c_str());
    ImGui::SetNextItemWidth(360.0f);
    if(ImGui::Combo("External GOOL target",&selectedTarget,targetLabelPointers.data(),static_cast<int>(targetLabelPointers.size()))){
     targetEid=targetEids[static_cast<std::size_t>(selectedTarget)];
     if(nsf_.setGoolDataPoolWord(static_cast<std::size_t>(activeGoolChunk_),static_cast<std::size_t>(activeGoolEntry_),poolIndex,targetEid))goolReferencesNeedRefresh=true;
    }
    ImGui::SetNextItemWidth(220.0f);
    if(ImGui::InputScalar("Custom EID / override",ImGuiDataType_U32,&targetEid,nullptr,nullptr,"%08X")){
     if(nsf_.setGoolDataPoolWord(static_cast<std::size_t>(activeGoolChunk_),static_cast<std::size_t>(activeGoolEntry_),poolIndex,targetEid))goolReferencesNeedRefresh=true;
    }
    ImGui::TextDisabled("Data Pool word %zu is shared by every descriptor using index %d.",poolIndex,static_cast<int>(selectedDescriptor.goolIndex));
   }else ImGui::TextDisabled("This descriptor has an invalid Data Pool index. Select a valid index above to assign a GOOL target.");
  }else ImGui::TextDisabled("No state descriptors in this GOOL entry.");
  if(goolReferencesNeedRefresh)nsf_.decodeGools(nsfGoolVersion(gameVersion_));
 }ImGui::End();
 if(consoleDockId!=0)ImGui::SetNextWindowDockID(consoleDockId,ImGuiCond_FirstUseEver);
 if(ImGui::Begin("GOOL Frame Groups",&showGoolFrameGroups_,flags)){
   ImGui::Text("%s | Frame-group item: %zu bytes",Crash::eidToEName(goolEid).c_str(),gool->frameGroupBytes);
   ImGui::TextDisabled("Index columns are item-relative offsets and are read-only. EID is editable; Frames and Interpolated are editable for supported vertex groups.");
   if(!gool->frameGroupError.empty()){
    ImGui::TextColored(ImVec4(1.0f,.45f,.35f,1.0f),"Frame-group decoding failed: %s",gool->frameGroupError.c_str());
   }else if(gool->frameGroups.empty()){
    if(gool->frameGroupsDecoded)ImGui::TextDisabled("The frame-group item is empty.");
    else ImGui::TextDisabled("This GOOL entry has no decoded frame-group item.");
   }else{
    if(frameGroupEidBuffers_.size()!=gool->frameGroups.size()){
     frameGroupEidBuffers_.resize(gool->frameGroups.size());
     for(std::size_t i=0;i<gool->frameGroups.size();++i){const auto name=Crash::eidToEName(gool->frameGroups[i].eid);std::snprintf(frameGroupEidBuffers_[i].data(),frameGroupEidBuffers_[i].size(),"%s",name.c_str());}
    }
    if(ImGui::BeginTable("GOOLFrameGroupsV2",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY)){
     ImGui::TableSetupColumn("Index",ImGuiTableColumnFlags_WidthFixed,70.0f);
     ImGui::TableSetupColumn("Index (Alt)",ImGuiTableColumnFlags_WidthFixed,100.0f);
     ImGui::TableSetupColumn("EID",ImGuiTableColumnFlags_WidthFixed,115.0f);
     ImGui::TableSetupColumn("Frames",ImGuiTableColumnFlags_WidthFixed,115.0f);
     ImGui::TableSetupColumn("Interpolated",ImGuiTableColumnFlags_WidthFixed,105.0f);
     ImGui::TableHeadersRow();
     for(std::size_t i=0;i<gool->frameGroups.size();++i){
      const auto&group=gool->frameGroups[i];
      const std::size_t index=group.byteOffset/4U;
      ImGui::TableNextRow();ImGui::PushID(static_cast<int>(i));
      ImGui::TableSetColumnIndex(0);
      if(index>64U)ImGui::Text("0x%zX",index);else ImGui::Text("%zu",index);
      ImGui::SetItemTooltip("Group type %u | item-relative offset 0x%zX",static_cast<unsigned>(group.type),group.byteOffset);
      ImGui::TableSetColumnIndex(1);ImGui::Text("0x%zX00",index);
      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(-1.0f);
      if(ImGui::InputText("##eid",frameGroupEidBuffers_[i].data(),frameGroupEidBuffers_[i].size(),ImGuiInputTextFlags_CharsNoBlank)){
       try{
        const auto eid=Crash::eNameToEid(frameGroupEidBuffers_[i].data());
        if(!nsf_.setGoolFrameGroupEid(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),i,eid))status_="Could not write the Frame Group EID.";
       }catch(const std::invalid_argument&){status_="Frame Group EID must be a valid five-character EName.";}
      }
      ImGui::TableSetColumnIndex(3);
      if(group.type==1U){
       int frames=static_cast<int>(group.frameCount);
       ImGui::SetNextItemWidth(-1.0f);
       if(ImGui::InputInt("##frames",&frames,1,10)){
        if(frames>=0&&frames<=255){
         if(!nsf_.setGoolFrameGroupFrameCount(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),i,static_cast<std::uint16_t>(frames)))status_="Could not write the Frame Group frame count.";
        }else status_="Frame Group frame count must be between 0 and 255.";
       }
      }else ImGui::TextDisabled("%d",static_cast<int>(group.frameCount));
      ImGui::TableSetColumnIndex(4);
      if(group.type==1U){
       bool interpolated=group.interpolated;
       if(ImGui::Checkbox("##interpolated",&interpolated)){
        if(!nsf_.setGoolFrameGroupInterpolated(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),i,interpolated))status_="Could not write the Frame Group interpolation flag.";
       }
      }else ImGui::TextDisabled("-");
      ImGui::PopID();
     }
     ImGui::EndTable();
    }
   }
  }ImGui::End();focusGoolDataPool_=false;
}

void EditorUI::drawAnimationTimeline(){
 const Crash::NsfEntry*animationEntry=nullptr;
 if(nsfValid_&&animationViewerChunk_>=0&&static_cast<std::size_t>(animationViewerChunk_)<nsf_.chunks().size()){
  const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(animationViewerChunk_)];
  if(chunk.type==0&&animationViewerEntry_>=0&&static_cast<std::size_t>(animationViewerEntry_)<chunk.entries.size()){
   const auto&entry=chunk.entries[static_cast<std::size_t>(animationViewerEntry_)];
   if(entry.valid&&entry.type==1&&entry.animation.decoded)animationEntry=&entry;
  }
 }
 animationHalfSpeed_=false;
 if(animationEntry){
  const auto&frames=animationEntry->animation.frames;
  if(!frames.empty()){
   const int lastFrame=static_cast<int>(frames.size()-1U);
   for(const auto&chunk:nsf_.chunks()){
    if(!chunk.entryTableValid)continue;
    for(const auto&entry:chunk.entries){
     if(!entry.valid||!entry.gool.frameGroupsDecoded)continue;
     for(const auto&group:entry.gool.frameGroups){
      if(group.type==1U&&group.eid==animationEntry->eid){animationHalfSpeed_=group.interpolated;break;}
     }
     if(animationHalfSpeed_)break;
    }
    if(animationHalfSpeed_)break;
   }
   if(!animationPlaying_)animationPlaybackFrame_=std::clamp(animationPlaybackFrame_,0.0,static_cast<double>(lastFrame));
   else if(animationPlaybackFrame_>=static_cast<double>(frames.size()))animationPlaybackFrame_=std::fmod(animationPlaybackFrame_,static_cast<double>(frames.size()));
   if(animationPlaying_&&frames.size()>1U){
    const double gameFps=editorPreferences.palRegion?25.0:30.0;
    const double fullSpeedFps=editorPreferences.palRegion?50.0:60.0;
    const double frameRate=editorPreferences.animationRefreshRateMode?fullSpeedFps:gameFps;
    const double frameInterval=1.0/frameRate;
    animationFrameTimer_+=std::max(0.0f,ImGui::GetIO().DeltaTime);
    const auto elapsedTicks=static_cast<std::size_t>(animationFrameTimer_/frameInterval);
    if(elapsedTicks>0U){
     animationFrameTimer_-=static_cast<float>(static_cast<double>(elapsedTicks)*frameInterval);
     const double frameStep=animationHalfSpeed_?0.5:1.0;
     animationPlaybackFrame_=std::fmod(animationPlaybackFrame_+static_cast<double>(elapsedTicks)*frameStep,static_cast<double>(frames.size()));
    }
   }
   animationFrame_=std::clamp(static_cast<int>(std::floor(animationPlaybackFrame_)),0,lastFrame);
  }
 }
 if(!ImGui::Begin("Animation Timeline")){ImGui::End();return;}
 if(!animationEntry||animationEntry->animation.frames.empty()){
  ImGui::TextDisabled("Select a decoded Type 1 Animation entry in the Resource Hierarchy to use the timeline.");
  ImGui::End();return;
 }
 const auto&frames=animationEntry->animation.frames;
 const int lastFrame=static_cast<int>(frames.size()-1U);
 ImGui::Text("%s",Crash::eidToEName(animationEntry->eid).c_str());
 ImGui::SameLine();ImGui::TextDisabled("%zu frames",frames.size());
 ImGui::SameLine();
 if(ImGui::SmallButton(animationPlaying_?"Pause":"Play")){animationPlaying_=!animationPlaying_;animationFrameTimer_=0.0f;}
 ImGui::SameLine();
 if(ImGui::SmallButton("|<")){animationPlaying_=false;animationFrame_=0;animationPlaybackFrame_=0.0;animationFrameTimer_=0.0f;}
 ImGui::SameLine();
 if(ImGui::SmallButton("<")){animationPlaying_=false;animationFrame_=std::max(0,animationFrame_-1);animationPlaybackFrame_=static_cast<double>(animationFrame_);animationFrameTimer_=0.0f;}
 ImGui::SameLine();
 if(ImGui::SmallButton(">")){animationPlaying_=false;animationFrame_=std::min(lastFrame,animationFrame_+1);animationPlaybackFrame_=static_cast<double>(animationFrame_);animationFrameTimer_=0.0f;}
 ImGui::SameLine();
 if(ImGui::SmallButton(">|")){animationPlaying_=false;animationFrame_=lastFrame;animationPlaybackFrame_=static_cast<double>(lastFrame);animationFrameTimer_=0.0f;}
 ImGui::SameLine();
 if(ImGui::SmallButton("Edit")){animationFrameEditorOpen_=true;animationFrameEditorFocus_=true;}
 ImGui::SameLine();
 ImGui::Text("Frame %d / %d",animationFrame_+1,lastFrame+1);
 const double fractionalFrame=animationPlaybackFrame_-std::floor(animationPlaybackFrame_);
 if(fractionalFrame>0.001){ImGui::SameLine();ImGui::TextDisabled("Subframe %.2f",fractionalFrame);}
 ImGui::TextDisabled("Select a frame by clicking or dragging its marker.");
 const float trackWidth=std::max(1.0f,ImGui::GetContentRegionAvail().x);
 const float trackHeight=48.0f;
 ImGui::InvisibleButton("##AnimationTimelineTrack",ImVec2(trackWidth,trackHeight),ImGuiButtonFlags_MouseButtonLeft);
 const ImVec2 trackMin=ImGui::GetItemRectMin(),trackMax=ImGui::GetItemRectMax();
 const float sidePadding=std::min(12.0f,trackWidth*0.5f);
 const float dotStart=trackMin.x+sidePadding,dotEnd=trackMax.x-sidePadding;
 const float dotSpan=std::max(0.0f,dotEnd-dotStart);
 if(ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Left)){
  const float t=dotSpan>0.0f?std::clamp((ImGui::GetIO().MousePos.x-dotStart)/dotSpan,0.0f,1.0f):0.0f;
  animationFrame_=frames.size()>1U?std::clamp(static_cast<int>(std::lround(t*static_cast<float>(lastFrame))),0,lastFrame):0;
  animationPlaybackFrame_=static_cast<double>(animationFrame_);animationPlaying_=false;animationFrameTimer_=0.0f;
 }
 ImDrawList*timelineDraw=ImGui::GetWindowDrawList();
 const float centerY=(trackMin.y+trackMax.y)*0.5f;
 timelineDraw->AddLine(ImVec2(dotStart,centerY),ImVec2(dotEnd,centerY),IM_COL32(75,86,103,255),2.0f);
 const int dotStride=std::max(1,static_cast<int>((frames.size()+999U)/1000U));
 for(int i=0;i<static_cast<int>(frames.size());i+=dotStride){
  const float t=lastFrame>0?static_cast<float>(i)/static_cast<float>(lastFrame):0.5f;
  const ImVec2 center(dotStart+dotSpan*t,centerY);
  const bool selected=i==animationFrame_||(animationFrame_>=i&&animationFrame_<i+dotStride);
  timelineDraw->AddCircleFilled(center,selected?6.0f:3.5f,selected?IM_COL32(105,174,245,255):IM_COL32(153,166,184,255),12);
  if(selected)timelineDraw->AddCircle(center,9.0f,IM_COL32(105,174,245,150),16,1.5f);
 }
 const float playheadX=dotStart+(lastFrame>0?dotSpan*static_cast<float>(animationFrame_)/static_cast<float>(lastFrame):dotSpan*0.5f);
 timelineDraw->AddLine(ImVec2(playheadX,trackMin.y+3.0f),ImVec2(playheadX,trackMax.y-3.0f),IM_COL32(245,210,112,220),1.5f);
 ImGui::End();
}

void EditorUI::drawAnimationFrameEditor(){
 if(!animationFrameEditorOpen_)return;
 if(animationFrameEditorFocus_){ImGui::SetNextWindowFocus();animationFrameEditorFocus_=false;}
 if(!ImGui::Begin("Animation Frame Editor",&animationFrameEditorOpen_)){ImGui::End();return;}
 const std::size_t chunkIndex=animationViewerChunk_>=0?static_cast<std::size_t>(animationViewerChunk_):nsf_.chunks().size();
 const std::size_t entryIndex=animationViewerEntry_>=0?static_cast<std::size_t>(animationViewerEntry_):0U;
 const Crash::NsfEntry*animationEntry=nullptr;
 if(nsfValid_&&chunkIndex<nsf_.chunks().size()){
  const auto&chunk=nsf_.chunks()[chunkIndex];
  if(chunk.type==0&&entryIndex<chunk.entries.size()){
   const auto&entry=chunk.entries[entryIndex];
   if(entry.valid&&entry.type==1&&entry.animation.decoded)animationEntry=&entry;
  }
 }
 if(!animationEntry||animationEntry->animation.frames.empty()){
  ImGui::TextDisabled("Select an animation and choose a frame from its timeline.");
 }else{
  const auto&frames=animationEntry->animation.frames;
  const int frameIndex=std::clamp(animationFrame_,0,static_cast<int>(frames.size()-1U));
  const auto&frame=frames[static_cast<std::size_t>(frameIndex)];
  const std::uint32_t modelEid=animationEntry->animation.isCrash3?frame.linkedModelEid:frame.modelEid;
  ImGui::Text("Animation: %s",Crash::eidToEName(animationEntry->eid).c_str());
  ImGui::SameLine();ImGui::TextDisabled("Frame %d / %zu",frameIndex+1,frames.size());
  if(Crash::isValidEid(modelEid))ImGui::Text("Linked Model: %s (%08X)",Crash::eidToEName(modelEid).c_str(),modelEid);
  else ImGui::Text("Linked Model: %08X",modelEid);
  ImGui::Separator();
  ImGui::Text("Frame Properties");
  ImGui::TextDisabled("Values are written directly to the NSF as they are changed.");
  auto editShort=[&](const char*label,std::uint32_t field,std::int16_t current){
   int value=current;
   ImGui::TextUnformatted(label);ImGui::SameLine();ImGui::SetNextItemWidth(150.0f);
   ImGui::PushID(static_cast<int>(field));
   if(ImGui::InputInt("##Value",&value,1,16)){
    if(value<std::numeric_limits<std::int16_t>::min()||value>std::numeric_limits<std::int16_t>::max())status_="Frame offsets and unknown values must fit in a signed 16-bit field.";
    else if(!nsf_.setAnimationFrameHeaderValue(chunkIndex,entryIndex,static_cast<std::size_t>(frameIndex),field,value))status_="Could not write the animation frame field.";
   }
   ImGui::PopID();
  };
  editShort("X Offset",0U,frame.xOffset);
  ImGui::SameLine();editShort("Y Offset",1U,frame.yOffset);
  ImGui::SameLine();editShort("Z Offset",2U,frame.zOffset);
  editShort("Unknown",3U,frame.unknown);
  if(!animationEntry->animation.isCrash3){
   if(!animationFrameModelEnameInitialized_||animationFrameModelEid_!=frame.modelEid||animationFrameModelEnameAnimationEid_!=animationEntry->eid||animationFrameModelEnameFrame_!=frameIndex){
    const std::string decodedModelEname=Crash::eidToEName(frame.modelEid);
    std::snprintf(animationFrameModelEname_,sizeof(animationFrameModelEname_),"%s",decodedModelEname.c_str());
    animationFrameModelEid_=frame.modelEid;animationFrameModelEnameAnimationEid_=animationEntry->eid;animationFrameModelEnameFrame_=frameIndex;animationFrameModelEnameInitialized_=true;
   }
   ImGui::TextUnformatted("Model EID");ImGui::SameLine();ImGui::SetNextItemWidth(150.0f);
   if(ImGui::InputText("##ModelEID",animationFrameModelEname_,sizeof(animationFrameModelEname_),ImGuiInputTextFlags_CharsNoBlank)){
    try{
     const std::uint32_t editableModelEid=Crash::eNameToEid(animationFrameModelEname_);
     if(!nsf_.setAnimationFrameHeaderValue(chunkIndex,entryIndex,static_cast<std::size_t>(frameIndex),4U,static_cast<std::int32_t>(editableModelEid)))status_="Could not write the animation frame Model EID.";
     else animationFrameModelEid_=editableModelEid;
    }catch(const std::exception&){status_="Model EID must be a valid five-character decoded EID name." ;}
   }
  }else{
   ImGui::TextDisabled("Crash 3 linked-model selection is supplied by GOOL frame groups; the raw frame model field is not edited here.");
  }
  ImGui::Separator();
  ImGui::Text("Frame Structure");
  ImGui::Text("Vertices: %u    Collisions: %u",frame.vertexCount,frame.collisionCount);
  ImGui::Text("Header Size: %u bytes    Special Vertices: %zu",frame.headerSize,frame.specialVertexCount);
  ImGui::TextDisabled("Header size and special-vertex count are derived from the frame layout and are read-only for now.");
  ImGui::Separator();
  ImGui::Text("Collision Records");
  if(frame.collisions.empty())ImGui::TextDisabled("This frame has no collision records.");
  for(std::size_t ci=0;ci<frame.collisions.size();++ci){
   ImGui::PushID(static_cast<int>(ci));
   const auto&collision=frame.collisions[ci];
   char title[64];std::snprintf(title,sizeof(title),"Collision %zu",ci+1U);
   if(ImGui::CollapsingHeader(title,ImGuiTreeNodeFlags_DefaultOpen)){
    int u=collision.values[0];ImGui::TextUnformatted("U");ImGui::SameLine();ImGui::SetNextItemWidth(150.0f);
    if(ImGui::InputInt("##CollisionU",&u)){
     if(!nsf_.setAnimationFrameCollisionValue(chunkIndex,entryIndex,static_cast<std::size_t>(frameIndex),ci,0U,u))status_="Could not write the collision U value.";
    }
    const char*rowNames[]={"Offset","Point 1","Point 2"};
    for(std::size_t row=0;row<3U;++row){
     ImGui::PushID(static_cast<int>(row));
     ImGui::Text("%s",rowNames[row]);
     for(std::size_t axis=0;axis<3U;++axis){
      const std::size_t valueIndex=1U+row*3U+axis;
      int value=collision.values[valueIndex];
      const char*axisName=axis==0U?"X":axis==1U?"Y":"Z";
      ImGui::TextUnformatted(axisName);ImGui::SameLine();ImGui::SetNextItemWidth(115.0f);
      char label[24];std::snprintf(label,sizeof(label),"##Collision%s",axisName);
      if(ImGui::InputInt(label,&value)){
       if(!nsf_.setAnimationFrameCollisionValue(chunkIndex,entryIndex,static_cast<std::size_t>(frameIndex),ci,valueIndex,value))status_="Could not write a collision coordinate.";
      }
     }
     ImGui::PopID();
    }
   }
   ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::Text("Decoded Vertices");
  if(!frame.verticesDecoded)ImGui::TextDisabled("Vertices are not decoded: %s",frame.vertexError.c_str());
  else{
   ImGui::TextDisabled("%zu vertices decoded. Packed temporal vertices remain read-only until their encoder is implemented.",frame.vertices.size());
   const std::size_t showCount=std::min<std::size_t>(frame.vertices.size(),static_cast<std::size_t>(std::max(0,static_cast<int>(frame.specialVertexCount))));
   if(showCount>0U&&ImGui::BeginTable("##SpecialVertices",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0.0f,220.0f))){
    ImGui::TableSetupColumn("Index");ImGui::TableSetupColumn("X");ImGui::TableSetupColumn("Y");ImGui::TableSetupColumn("Z");ImGui::TableHeadersRow();
    for(std::size_t vi=0;vi<showCount;++vi){
     ImGui::PushID(static_cast<int>(vi));ImGui::TableNextRow();
     ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",vi);
     for(std::size_t axis=0;axis<3U;++axis){
      ImGui::TableSetColumnIndex(static_cast<int>(axis+1U));
      int value=frame.specialVertices[vi][axis];
      const char*axisName=axis==0U?"X":axis==1U?"Y":"Z";
      ImGui::SetNextItemWidth(-1.0f);
      if(ImGui::InputInt(axisName,&value,1,8)){
       if(value<0||value>255)status_="Special vertex coordinates must be between 0 and 255.";
       else if(!nsf_.setAnimationFrameSpecialVertex(chunkIndex,entryIndex,static_cast<std::size_t>(frameIndex),vi,axis,static_cast<std::uint8_t>(value)))status_="Could not write the special vertex coordinate.";
      }
     }
     ImGui::PopID();
    }
    ImGui::EndTable();
   }
  }
 }
 ImGui::End();
}

void EditorUI::drawAnimationViewer(){
 if(animationDockId_!=0)ImGui::SetNextWindowDockID(animationDockId_,ImGuiCond_FirstUseEver);
 if(!ImGui::Begin("3D Animation Editor")){ImGui::End();return;}
 const Crash::NsfEntry*animationEntry=nullptr;
 if(nsfValid_&&animationViewerChunk_>=0&&static_cast<std::size_t>(animationViewerChunk_)<nsf_.chunks().size()){
  const auto&chunk=nsf_.chunks()[static_cast<std::size_t>(animationViewerChunk_)];
  if(chunk.type==0&&animationViewerEntry_>=0&&static_cast<std::size_t>(animationViewerEntry_)<chunk.entries.size()){
   const auto&entry=chunk.entries[static_cast<std::size_t>(animationViewerEntry_)];
   if(entry.valid&&entry.type==1&&entry.animation.decoded)animationEntry=&entry;
  }
 }
 if(!animationEntry){
  ImGui::TextDisabled("Select a decoded Type 1 Animation entry in the Resource Hierarchy to preview it here.");
  ImGui::End();return;
 }
 const auto&animation=animationEntry->animation;
 if(animation.frames.empty()){
  ImGui::TextDisabled("This animation contains no frames.");ImGui::End();return;
 }
 auto findModel=[&](std::uint32_t eid)->const Crash::NsfModel*{
  if(!Crash::isValidEid(eid))return nullptr;
  for(const auto&chunk:nsf_.chunks())if(chunk.type==0&&chunk.entryTableValid)
   for(const auto&entry:chunk.entries)
    if(entry.valid&&entry.type==2&&entry.eid==eid&&entry.model.decoded)return &entry.model;
  return nullptr;
 };
 struct AnimationPoint{float x,y,z;AnimationPoint(float px,float py,float pz):x(px),y(py),z(pz){}};
 auto transformVertex=[](const Crash::NsfAnimationVertex&v,const Crash::NsfAnimationFrame&frame,const Crash::NsfModel*model,bool includeFrameOffset=true){
  constexpr float fallbackScale=1.0f/128.0f;
  constexpr float modelScale=3200.0f*128.0f;
  const float sx=model?static_cast<float>(model->scaleX)/modelScale:fallbackScale;
  const float sy=model?static_cast<float>(model->scaleY)/modelScale:fallbackScale;
  const float sz=model?static_cast<float>(model->scaleZ)/modelScale:fallbackScale;
  // CrashEdit-RE's animation path swaps Y/Z, adds the frame translation,
  // then applies the linked model's per-axis scale.
  return AnimationPoint(
   (static_cast<float>(v.x)+(includeFrameOffset?static_cast<float>(frame.xOffset)/4.0f:0.0f))*sx,
   (static_cast<float>(v.z)+(includeFrameOffset?static_cast<float>(frame.yOffset)/4.0f:0.0f))*sy,
   (static_cast<float>(v.y)+(includeFrameOffset?static_cast<float>(frame.zOffset)/4.0f:0.0f))*sz
  );
 };
 const int lastFrame=static_cast<int>(animation.frames.size()-1U);
 if(!animationPlaying_)animationPlaybackFrame_=std::clamp(animationPlaybackFrame_,0.0,static_cast<double>(lastFrame));
 else if(animationPlaybackFrame_>=static_cast<double>(animation.frames.size()))animationPlaybackFrame_=std::fmod(animationPlaybackFrame_,static_cast<double>(animation.frames.size()));
 animationFrame_=static_cast<int>(std::floor(animationPlaybackFrame_));
 // Playback controls and the clock are owned by Animation Timeline.
 const bool animationHalfSpeed=animationHalfSpeed_;
 // The GOOL frame-group flag controls source-frame cadence independently of
 // this visual interpolation toggle. Blend only when the playback position is fractional.
 const double poseFramePosition=animationPlaybackFrame_+(animationInterpolate_&&animationPlaying_&&!animationHalfSpeed?0.5:0.0);
 const int poseFrameIndex=static_cast<int>(std::floor(poseFramePosition))%static_cast<int>(animation.frames.size());
 const auto&frame=animation.frames[static_cast<std::size_t>(poseFrameIndex)];
 const std::uint32_t linkedModelEid=animation.isCrash3?frame.linkedModelEid:frame.modelEid;
 const Crash::NsfModel*model=findModel(linkedModelEid);
 std::vector<AnimationPoint> displayPositions;
 if(frame.verticesDecoded){
  displayPositions.reserve(frame.vertices.size());
  for(const auto&vertex:frame.vertices)displayPositions.push_back(transformVertex(vertex,frame,model));
  const double fraction=poseFramePosition-std::floor(poseFramePosition);
  if(animationInterpolate_&&animationPlaying_&&fraction>0.0&&animation.frames.size()>1U){
   const auto&nextFrame=animation.frames[(static_cast<std::size_t>(poseFrameIndex)+1U)%animation.frames.size()];
   const std::uint32_t nextModelEid=animation.isCrash3?nextFrame.linkedModelEid:nextFrame.modelEid;
   if(nextFrame.verticesDecoded&&nextFrame.vertices.size()==frame.vertices.size()&&nextModelEid==linkedModelEid){
    for(std::size_t i=0;i<displayPositions.size();++i){
     const auto next=transformVertex(nextFrame.vertices[i],nextFrame,model);
     auto&current=displayPositions[i];
     current.x+=static_cast<float>((next.x-current.x)*fraction);
     current.y+=static_cast<float>((next.y-current.y)*fraction);
     current.z+=static_cast<float>((next.z-current.z)*fraction);
    }
   }
  }
 }
 ImGui::SameLine();
 if(Crash::isValidEid(linkedModelEid))ImGui::TextDisabled("Selected Model EID: %s",Crash::eidToEName(linkedModelEid).c_str());
 else ImGui::TextDisabled("Selected Model EID: %08X",linkedModelEid);
 {
  ImGui::SameLine();
  std::string currentAnimationLabel="No linked animation";
  if(animationViewerChunk_>=0&&static_cast<std::size_t>(animationViewerChunk_)<nsf_.chunks().size()){
   const auto&currentChunk=nsf_.chunks()[static_cast<std::size_t>(animationViewerChunk_)];
   if(currentChunk.type==0&&animationViewerEntry_>=0&&static_cast<std::size_t>(animationViewerEntry_)<currentChunk.entries.size()){
    const auto&currentEntry=currentChunk.entries[static_cast<std::size_t>(animationViewerEntry_)];
    if(currentEntry.valid&&currentEntry.type==1)currentAnimationLabel=Crash::eidToEName(currentEntry.eid);
   }
  }
  ImGui::SetNextItemWidth(230.0f);
  if(Crash::isValidEid(linkedModelEid)&&ImGui::BeginCombo("##ModelLinkedAnimation",currentAnimationLabel.c_str())){
   for(std::size_t ci=0;ci<nsf_.chunks().size();++ci){
    const auto&candidateChunk=nsf_.chunks()[ci];if(candidateChunk.type!=0||!candidateChunk.entryTableValid)continue;
    for(std::size_t ei=0;ei<candidateChunk.entries.size();++ei){
     const auto&candidate=candidateChunk.entries[ei];
     if(!candidate.valid||candidate.type!=1||!candidate.animation.decoded)continue;
     bool usesSelectedModel=false;
     for(const auto&candidateFrame:candidate.animation.frames){
      const std::uint32_t candidateModelEid=candidate.animation.isCrash3?candidateFrame.linkedModelEid:candidateFrame.modelEid;
      if(candidateModelEid==linkedModelEid){usesSelectedModel=true;break;}
     }
     if(!usesSelectedModel)continue;
     const bool selected=animationViewerChunk_==static_cast<int>(ci)&&animationViewerEntry_==static_cast<int>(ei);
     const std::string label=Crash::eidToEName(candidate.eid)+"  ["+std::to_string(ci)+":"+std::to_string(ei)+"]";
     if(ImGui::Selectable(label.c_str(),selected)){
      animationViewerChunk_=static_cast<int>(ci);animationViewerEntry_=static_cast<int>(ei);
      animationFrame_=0;animationPlaybackFrame_=0.0;animationFrameTimer_=0.0f;
      animationPlaying_=false;animationHalfSpeed_=false;animationCameraInitialized_=false;
     }
     if(selected)ImGui::SetItemDefaultFocus();
    }
   }
   ImGui::EndCombo();
  }
 }
 if(!frame.vertexError.empty())ImGui::TextColored(ImVec4(1.0f,.55f,.35f,1.0f),"%s",frame.vertexError.c_str());
 if(!frame.verticesDecoded)ImGui::TextDisabled("Frame vertices are not decoded.");
 else if(!model&&animation.isCrash3&&!Crash::isValidEid(linkedModelEid))ImGui::TextDisabled("No matching GOOL VertexGroup3 links this animation to a Model.");
 else if(!model)ImGui::TextDisabled("Linked Model resource is not present in this NSF.");
 else if(!model->polygonsDecoded)ImGui::TextDisabled("Model topology unavailable: %s",model->polygonError.c_str());
 else ImGui::TextDisabled("%zu vertices | %zu triangles | %zu collisions",frame.vertices.size(),model->triangles.size(),frame.collisions.size());
 const ImVec2 canvasMin=ImGui::GetCursorScreenPos();
 ImVec2 canvasSize=ImGui::GetContentRegionAvail();canvasSize.x=std::max(canvasSize.x,1.0f);canvasSize.y=std::max(canvasSize.y,1.0f);
 const ImVec2 toolbarMin(canvasMin.x+8.0f,canvasMin.y+8.0f);
 ImGui::SetCursorScreenPos(toolbarMin);
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(6.0f,4.0f));
 ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,3.0f);
 const bool toolbarButton=ImGui::InvisibleButton("##AnimationViewportToolbarMenu",ImVec2(28.0f,26.0f));
 const ImVec2 toolbarMax=ImGui::GetItemRectMax();
 const bool toolbarHovered=ImGui::IsItemHovered();
 if(toolbarHovered)ImGui::SetTooltip("Animation viewport settings");
 if(toolbarButton)ImGui::OpenPopup("AnimationViewportOptions");
 if(ImGui::BeginPopup("AnimationViewportOptions")){
  ImGui::TextUnformatted("Rendering");
  if(ImGui::MenuItem("Interpolate animation frames",nullptr,animationInterpolate_))animationInterpolate_=!animationInterpolate_;
  ImGui::MenuItem("Collision Viewer",nullptr,&animationCollisionViewer_);
  ImGui::MenuItem("Disable Textures",nullptr,&animationTexturesDisabled_);
  ImGui::Separator();
  ImGui::TextUnformatted("Playback timing");
  const char*gameSpeedLabel=editorPreferences.palRegion?"Game speed (25 FPS)":"Game speed (30 FPS)";
  const char*refreshSpeedLabel=editorPreferences.palRegion?"Full Speed (50 FPS)":"Full Speed (60 FPS)";
  if(ImGui::MenuItem(gameSpeedLabel,nullptr,!editorPreferences.animationRefreshRateMode))editorPreferences.animationRefreshRateMode=false;
  if(ImGui::MenuItem(refreshSpeedLabel,nullptr,editorPreferences.animationRefreshRateMode))editorPreferences.animationRefreshRateMode=true;
  if(ImGui::MenuItem("Textured (Hardware)",nullptr,!animationWireframe_))animationWireframe_=false;
  if(ImGui::MenuItem("Wireframe",nullptr,animationWireframe_))animationWireframe_=true;
  ImGui::Separator();
  ImGui::TextUnformatted("Camera movement speed");
  ImGui::SetNextItemWidth(180.0f);
  ImGui::SliderInt("##AnimationMovementSpeed",&editorPreferences.animationMoveSpeed,25,400,"%d%%");
  ImGui::EndPopup();
 }
 const bool viewportToolbarOwnsMouse=toolbarHovered||ImGui::IsPopupOpen("AnimationViewportOptions");
 ImGui::PopStyleVar(2);
 ImGui::SetCursorScreenPos(canvasMin);
 ImGui::SetNextItemAllowOverlap();
 ImGui::InvisibleButton("AnimationViewerCanvas",canvasSize,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
 const bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();
 const ImGuiIO&shortcutIo=ImGui::GetIO();
 if(hovered&&!viewportToolbarOwnsMouse&&!shortcutIo.WantTextInput&&!shortcutIo.KeyCtrl&&!shortcutIo.KeyAlt&&!shortcutIo.KeySuper){
  if(ImGui::IsKeyPressed(ImGuiKey_C,false))animationCollisionViewer_=!animationCollisionViewer_;
  if(ImGui::IsKeyPressed(ImGuiKey_X,false))animationWireframe_=!animationWireframe_;
  if(ImGui::IsKeyPressed(ImGuiKey_T,false))animationTexturesDisabled_=!animationTexturesDisabled_;
 }
 ImDrawList*dl=ImGui::GetWindowDrawList();const ImVec2 canvasMax(canvasMin.x+canvasSize.x,canvasMin.y+canvasSize.y);
 dl->AddRectFilled(canvasMin,canvasMax,IM_COL32(0,0,0,255));dl->PushClipRect(canvasMin,canvasMax,true);
 if(!frame.verticesDecoded||frame.vertices.empty()){
  const char*message=!frame.verticesDecoded?"Animation frame geometry is unavailable.":"Animation frame has no vertices.";
  dl->AddText(ImVec2(canvasMin.x+16.0f,canvasMin.y+18.0f),IM_COL32(190,198,210,255),message);
 }else{
  // Frame all animation poses, including their source frame offsets, once when
  // opening the animation. Later offset edits move the geometry without recentering.
  AnimationPoint boundsMin(FLT_MAX,FLT_MAX,FLT_MAX),boundsMax(-FLT_MAX,-FLT_MAX,-FLT_MAX);
  bool haveBounds=false;
  for(const auto&candidate:animation.frames){
   if(!candidate.verticesDecoded)continue;
   const auto*candidateModel=findModel(animation.isCrash3?candidate.linkedModelEid:candidate.modelEid);
   for(const auto&v:candidate.vertices){
    const AnimationPoint p=transformVertex(v,candidate,candidateModel);
    boundsMin.x=std::min(boundsMin.x,p.x);boundsMin.y=std::min(boundsMin.y,p.y);boundsMin.z=std::min(boundsMin.z,p.z);
    boundsMax.x=std::max(boundsMax.x,p.x);boundsMax.y=std::max(boundsMax.y,p.y);boundsMax.z=std::max(boundsMax.z,p.z);
    haveBounds=true;
   }
  }
  if(!haveBounds){boundsMin=AnimationPoint{-1,-1,-1};boundsMax=AnimationPoint{1,1,1};}
  const AnimationPoint center((boundsMin.x+boundsMax.x)*0.5f,(boundsMin.y+boundsMax.y)*0.5f,(boundsMin.z+boundsMax.z)*0.5f);
  const float dx=boundsMax.x-boundsMin.x,dy=boundsMax.y-boundsMin.y,dz=boundsMax.z-boundsMin.z;
  const float radius=std::max(0.25f,std::sqrt(dx*dx+dy*dy+dz*dz)*0.5f);
  if(!animationCameraInitialized_||animationCameraEntryEid_!=animationEntry->eid){
   animationTargetX_=center.x;animationTargetY_=center.y;animationTargetZ_=center.z;
   animationYaw_=0.0f;animationPitch_=0.261799f;
   // Match CrashEdit-RE's reset-camera behavior: frame all CorePositions,
   // center on their bounds, and keep enough distance for the full model.
   animationDistance_=std::clamp(radius*3.2f,0.75f,200000.0f);
   animationCameraEntryEid_=animationEntry->eid;
   animationCameraInitialized_=true;
  }
  const ImGuiIO&io=ImGui::GetIO();
  if(hovered&&!viewportToolbarOwnsMouse&&io.MouseWheel!=0.0f)animationDistance_=std::clamp(animationDistance_-io.MouseWheel*(0.3f+1.7f*(animationDistance_/100.0f)),0.1f,200000.0f);
  if(active&&!viewportToolbarOwnsMouse&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
   animationYaw_-=io.MouseDelta.x*0.005f;
   animationPitch_=std::clamp(animationPitch_+io.MouseDelta.y*0.005f,-1.45f,1.45f);
  }
  if(active&&!viewportToolbarOwnsMouse&&ImGui::IsMouseDragging(ImGuiMouseButton_Right)){
   const float pan=animationDistance_*0.0018f;
   const float cyaw=std::cos(animationYaw_),syaw=std::sin(animationYaw_);
   animationTargetX_-=(-cyaw*io.MouseDelta.x+syaw*io.MouseDelta.y)*pan;
   animationTargetZ_-=(-syaw*io.MouseDelta.x-cyaw*io.MouseDelta.y)*pan;
  }
  if(hovered&&!viewportToolbarOwnsMouse&&ImGui::IsKeyPressed(ImGuiKey_R))animationCameraInitialized_=false;
  if(hovered&&!viewportToolbarOwnsMouse){
   const float moveSpeed=(2.0f*float(editorPreferences.animationMoveSpeed)/100.0f)*(0.2f+0.8f*(animationDistance_/20.0f))*io.DeltaTime;
   const bool shift=ImGui::IsKeyDown(ImGuiKey_LeftShift)||ImGui::IsKeyDown(ImGuiKey_RightShift);
   const float cyaw=std::cos(animationYaw_),syaw=std::sin(animationYaw_);
   if(shift){
    if(ImGui::IsKeyDown(ImGuiKey_W))animationTargetZ_+=moveSpeed;
    if(ImGui::IsKeyDown(ImGuiKey_S))animationTargetZ_-=moveSpeed;
    if(ImGui::IsKeyDown(ImGuiKey_A))animationTargetX_+=moveSpeed;
    if(ImGui::IsKeyDown(ImGuiKey_D))animationTargetX_-=moveSpeed;
   }else{
    if(ImGui::IsKeyDown(ImGuiKey_W)){animationTargetX_-=syaw*moveSpeed;animationTargetZ_-=cyaw*moveSpeed;}
    if(ImGui::IsKeyDown(ImGuiKey_S)){animationTargetX_+=syaw*moveSpeed;animationTargetZ_+=cyaw*moveSpeed;}
    if(ImGui::IsKeyDown(ImGuiKey_A)){animationTargetX_-=cyaw*moveSpeed;animationTargetZ_+=syaw*moveSpeed;}
    if(ImGui::IsKeyDown(ImGuiKey_D)){animationTargetX_+=cyaw*moveSpeed;animationTargetZ_-=cyaw*moveSpeed;}
   }
   if(ImGui::IsKeyDown(ImGuiKey_E))animationTargetY_+=moveSpeed;
   if(ImGui::IsKeyDown(ImGuiKey_Q))animationTargetY_-=moveSpeed;
  }
  struct CameraPoint{float x,y,depth;};
  const float cyaw=std::cos(animationYaw_),syaw=std::sin(animationYaw_);
  const float cp=std::cos(animationPitch_),sp=std::sin(animationPitch_);
  const float focal=std::min(canvasSize.x,canvasSize.y)*0.5f/std::tan(0.785398163f*0.5f);
  auto toCamera=[&](float x,float y,float z){
   x-=animationTargetX_;y-=animationTargetY_;z-=animationTargetZ_;
   const float rx=x*cyaw-z*syaw,rz=x*syaw+z*cyaw;
   const float py=y*cp-rz*sp,pz=y*sp+rz*cp;
   return CameraPoint{rx,py,animationDistance_-pz};
  };
  auto projectCamera=[&](const CameraPoint&p){
   return ImVec2(canvasMin.x+canvasSize.x*0.5f+p.x*focal/p.depth,
                 canvasMin.y+canvasSize.y*0.5f-p.y*focal/p.depth);
  };
  auto project=[&](float x,float y,float z){
   const CameraPoint p=toCamera(x,y,z);
   if(p.depth<=0.1f)return ImVec2(-100000.0f,-100000.0f);
   return projectCamera(p);
  };
  // Keep the ground grid fixed at the user-selected world-space Y position.
  // Clip each segment against the camera near plane before
  // perspective division so rotating close to the floor cannot fling lines
  // across the viewport.
  constexpr float groundY=0.0f;
  const float planeExtent=std::max(2.0f,radius*2.5f);
  constexpr float planeX=0.0f,planeZ=0.0f;
  if(animationWireframe_){
  const ImVec2 floorCorners[4]={
   project(planeX-planeExtent,groundY,planeZ-planeExtent),
   project(planeX+planeExtent,groundY,planeZ-planeExtent),
   project(planeX+planeExtent,groundY,planeZ+planeExtent),
   project(planeX-planeExtent,groundY,planeZ+planeExtent)
  };
  // The ground plane and grid are a separate viewport background layer. They are
  // drawn before the transparent model framebuffer and never enter its depth buffer.
  const bool floorVisible=floorCorners[0].x>-99999.0f&&floorCorners[1].x>-99999.0f&&floorCorners[2].x>-99999.0f&&floorCorners[3].x>-99999.0f;
  if(floorVisible)dl->AddConvexPolyFilled(floorCorners,4,IM_COL32(20,23,28,255));
  auto drawGridSegment=[&](float x1,float z1,float x2,float z2,ImU32 color,float width){
   CameraPoint a=toCamera(x1,groundY,z1),b=toCamera(x2,groundY,z2);
   constexpr float nearDepth=0.1f;
   if(a.depth<=nearDepth&&b.depth<=nearDepth)return;
   if(a.depth<=nearDepth||b.depth<=nearDepth){
    const float t=(nearDepth-a.depth)/(b.depth-a.depth);
    CameraPoint clipped{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,nearDepth};
    if(a.depth<=nearDepth)a=clipped;else b=clipped;
   }
   dl->AddLine(projectCamera(a),projectCamera(b),color,width);
  };
  const int gridLines=16;
  const float gridStep=planeExtent/static_cast<float>(gridLines);
  for(int i=-gridLines;i<=gridLines;++i){
   const float offset=static_cast<float>(i)*gridStep;
   const ImU32 gridColor=i==0?IM_COL32(105,112,124,210):IM_COL32(58,65,76,175);
   const float width=i==0?1.5f:1.0f;
   drawGridSegment(planeX+offset,planeZ-planeExtent,planeX+offset,planeZ+planeExtent,gridColor,width);
   drawGridSegment(planeX-planeExtent,planeZ+offset,planeX+planeExtent,planeZ+offset,gridColor,width);
  }
  }
  if(model&&model->polygonsDecoded&&!model->triangles.empty()){
   if(animationWireframe_){
    std::vector<ImVec2> projected(frame.vertices.size());
    std::vector<bool> visible(frame.vertices.size(),false);
    for(std::size_t i=0;i<frame.vertices.size();++i){const auto p=displayPositions[i];projected[i]=project(p.x,p.y,p.z);visible[i]=projected[i].x>-99999.0f;}
    for(const auto&tri:model->triangles){
     if(tri.a<0||tri.b<0||tri.c<0||static_cast<std::size_t>(tri.a)>=projected.size()||static_cast<std::size_t>(tri.b)>=projected.size()||static_cast<std::size_t>(tri.c)>=projected.size())continue;
     const auto a=static_cast<std::size_t>(tri.a),b=static_cast<std::size_t>(tri.b),c=static_cast<std::size_t>(tri.c);
     if(!visible[a]||!visible[b]||!visible[c])continue;
     ImU32 color=IM_COL32(165,190,220,235);
     if(tri.colorA<model->colors.size()){const auto&col=model->colors[tri.colorA];color=IM_COL32(col.r,col.g,col.b,235);}
     dl->AddLine(projected[a],projected[b],color,1.25f);dl->AddLine(projected[b],projected[c],color,1.25f);dl->AddLine(projected[c],projected[a],color,1.25f);
    }
   }else{
    struct Vertex{float x,y,z,w,r,g,b,a,u,v;};
    struct Gpu{unsigned int program=0,vao=0,vbo=0,fbo=0,color=0,depth=0;int width=0,height=0;bool ready=false;std::map<std::tuple<const std::vector<std::uint8_t>*,int,int,int>,unsigned int> textures;std::string path;};
    static Gpu gpu;
    auto shader=[](unsigned int type,const char*source){unsigned int id=GL::CreateShader(type);if(!id)return 0U;GL::ShaderSource(id,1,&source,nullptr);GL::CompileShader(id);int ok=0;GL::GetShaderiv(id,0x8B81U,&ok);if(!ok){GL::DeleteShader(id);return 0U;}return id;};
    if(!gpu.ready){
     static const char*vsSource="#version 150\nin vec4 aPosition;in vec4 aColor;in vec2 aUV;out vec4 vColor;out vec2 vUV;void main(){gl_Position=aPosition;vColor=aColor;vUV=aUV;}";
     static const char*fsSource="#version 150\nin vec4 vColor;in vec2 vUV;out vec4 fragColor;uniform sampler2D uTexture;uniform int uHasTexture;uniform int uBlendMask;uniform int uBlendMode;void main(){if(uHasTexture==0){if(uBlendMask!=3)discard;fragColor=vColor;return;}vec4 t=texture(uTexture,vUV);if(t.a<=0.0)discard;bool stp=t.a<0.999;if(uBlendMask==3){if(stp&&uBlendMode!=3)discard;}else if(uBlendMode!=uBlendMask)discard;float alpha=(uBlendMask==0&&uBlendMode==0&&stp)?0.5:1.0;fragColor=vec4(min(vColor.rgb*t.rgb*2.0,vec3(1.0)),alpha);}";
     unsigned int vs=shader(0x8B31U,vsSource),fs=shader(0x8B30U,fsSource);
     if(vs&&fs){gpu.program=GL::CreateProgram();GL::AttachShader(gpu.program,vs);GL::AttachShader(gpu.program,fs);GL::BindAttribLocation(gpu.program,0,"aPosition");GL::BindAttribLocation(gpu.program,1,"aColor");GL::BindAttribLocation(gpu.program,2,"aUV");GL::LinkProgram(gpu.program);int ok=0;GL::GetProgramiv(gpu.program,0x8B82U,&ok);gpu.ready=ok!=0;}
     if(vs)GL::DeleteShader(vs);if(fs)GL::DeleteShader(fs);
     if(gpu.ready){GL::GenVertexArrays(1,&gpu.vao);GL::GenBuffers(1,&gpu.vbo);GL::BindVertexArray(gpu.vao);GL::BindBuffer(0x8892U,gpu.vbo);GL::EnableVertexAttribArray(0);GL::VertexAttribPointer(0,4,0x1406U,0,sizeof(Vertex),nullptr);GL::EnableVertexAttribArray(1);GL::VertexAttribPointer(1,4,0x1406U,0,sizeof(Vertex),reinterpret_cast<const void*>(4*sizeof(float)));GL::EnableVertexAttribArray(2);GL::VertexAttribPointer(2,2,0x1406U,0,sizeof(Vertex),reinterpret_cast<const void*>(8*sizeof(float)));GL::BindVertexArray(0);}
    }
    bool rendered=false;
    if(gpu.ready){
     const auto&io=ImGui::GetIO();const int w=std::max(1,int(std::round(canvasSize.x*io.DisplayFramebufferScale.x))),h=std::max(1,int(std::round(canvasSize.y*io.DisplayFramebufferScale.y)));
     int oldFbo=0,oldViewport[4]{},oldTexture=0,oldRenderbuffer=0,oldActive=0,oldDepthMask=1;
     GL::GetIntegerv(0x8CA6U,&oldFbo);GL::GetIntegerv(0x0BA2U,oldViewport);GL::GetIntegerv(0x84E0U,&oldActive);GL::ActiveTexture(0x84C0U);GL::GetIntegerv(0x8069U,&oldTexture);GL::GetIntegerv(0x8CA7U,&oldRenderbuffer);GL::GetIntegerv(0x0B72U,&oldDepthMask);
     if(gpu.path!=nsfPath_.string()){for(const auto&t:gpu.textures)GL::DeleteTextures(1,&t.second);gpu.textures.clear();gpu.path=nsfPath_.string();}
     if(!gpu.fbo){GL::GenFramebuffers(1,&gpu.fbo);GL::GenTextures(1,&gpu.color);GL::GenRenderbuffers(1,&gpu.depth);}
     if(gpu.width!=w||gpu.height!=h){gpu.width=w;gpu.height=h;GL::BindTexture(0x0DE1U,gpu.color);GL::TexParameteri(0x0DE1U,0x2801U,0x2601);GL::TexParameteri(0x0DE1U,0x2800U,0x2601);GL::TexImage2D(0x0DE1U,0,0x8058,w,h,0,0x1908U,0x1401U,nullptr);GL::BindRenderbuffer(0x8D41U,gpu.depth);GL::RenderbufferStorage(0x8D41U,0x81A6U,w,h);GL::BindFramebuffer(0x8D40U,gpu.fbo);GL::FramebufferTexture2D(0x8D40U,0x8CE0U,0x0DE1U,gpu.color,0);GL::FramebufferRenderbuffer(0x8D40U,0x8D00U,0x8D41U,gpu.depth);}
     GL::BindFramebuffer(0x8D40U,gpu.fbo);
     if(GL::CheckFramebufferStatus(0x8D40U)==0x8CD5U){
      using TextureKey=std::tuple<const std::vector<std::uint8_t>*,int,int,int>;
      using BatchKey=std::pair<TextureKey,int>;
      std::map<BatchKey,std::vector<Vertex>> batches;
      std::map<std::uint32_t,const std::vector<std::uint8_t>*> pageLookup;
      for(const auto&chunk:nsf_.chunks())if(chunk.type==1&&chunk.bytes.size()>=65536U){
       const std::uint32_t pageId=std::uint32_t(chunk.bytes[4])|(std::uint32_t(chunk.bytes[5])<<8U)|(std::uint32_t(chunk.bytes[6])<<16U)|(std::uint32_t(chunk.bytes[7])<<24U);
       pageLookup.emplace(pageId,&chunk.bytes);
      }
      for(const auto&tri:model->triangles){
       const bool nocull=tri.triangleSubtype==0||tri.triangleSubtype==2;
       const bool flip=((tri.triangleType==2)^(tri.triangleSubtype==3))&&!nocull;
       const int order[3]={flip?2:0,1,flip?0:2};
       const int frameVertexOffset=static_cast<int>(frame.specialVertexCount);const int baseVertices[3]={tri.a+frameVertexOffset,tri.b+frameVertexOffset,tri.c+frameVertexOffset},baseColors[3]={tri.colorA,tri.colorB,tri.colorC};
       const int vi[3]={baseVertices[order[0]],baseVertices[order[1]],baseVertices[order[2]]};
       const int ci[3]={baseColors[order[0]],baseColors[order[1]],baseColors[order[2]]};
       if(vi[0]<0||vi[1]<0||vi[2]<0||std::size_t(vi[0])>=frame.vertices.size()||std::size_t(vi[1])>=frame.vertices.size()||std::size_t(vi[2])>=frame.vertices.size())continue;
       const Crash::NsfModelTexture*tex=nullptr;
       int textureId=static_cast<int>(tri.textureIndex)-1;
       bool textureValid=true;
       if(tri.animated){
        ++textureId;
        if(textureId<0||static_cast<std::size_t>(textureId)>=model->extendedTextureWords.size())textureValid=false;
        else{
         std::uint32_t anim=model->extendedTextureWords[static_cast<std::size_t>(textureId)];
         const int offset=static_cast<int>(anim&0x7ffU);
         if(offset!=0){
          textureId=offset-1;
          if((anim&0x80000000U)!=0)textureId+=static_cast<int>((anim>>29U)&3U);
          else{
           const int latency=static_cast<int>((anim>>26U)&0x1fU),delay=static_cast<int>((anim>>19U)&0x7fU),mask=static_cast<int>((anim>>12U)&0x7fU);
           // CrashEdit uses Render.RealCurrentFrame / 2 for animated model
           // textures. That is a 30 Hz real-time clock, independent of the
           // animation's selected/playback frame index.
           const int textureFrame=static_cast<int>(std::floor(ImGui::GetTime()*30.0));
           textureId+=(textureFrame/(1+latency)+delay)&mask;
           if((anim&(1U<<11U))!=0){
            ++textureId;
            if(textureId<0||static_cast<std::size_t>(textureId)>=model->extendedTextureWords.size())textureValid=false;
            else{anim=model->extendedTextureWords[static_cast<std::size_t>(textureId)];textureId=static_cast<int>(anim&0x7ffU)-1+static_cast<int>((anim>>29U)&3U);}
           }
          }
         }else textureId=-1;
        }
       }
       if(textureValid&&textureId>=0){
        if(static_cast<std::size_t>(textureId)>=model->textures.size())textureValid=false;
        else tex=&model->textures[static_cast<std::size_t>(textureId)];
       }
       if(!textureValid)continue;
       const std::vector<std::uint8_t>*page=nullptr;
       if(tex&&tex->page<model->texturePageEids.size()){const auto found=pageLookup.find(model->texturePageEids[tex->page]);if(found!=pageLookup.end())page=found->second;}
       const int mode=tex?std::clamp<int>(tex->colorMode,0,2):0,clutX=tex?std::clamp<int>(tex->clutX,0,15):0,clutY=tex?std::clamp<int>((tex->clutY2<<2U)|((tex->clutY1>>2U)&3U),0,127):0;
       float ux[3]{},uy[3]{};const int uvOrder=(((tri.triangleType!=2&&!flip)||(tri.triangleType==2&&tri.triangleSubtype==1))?2:0),uvOrder1=1,uvOrder2=(((tri.triangleType!=2&&!flip)||(tri.triangleType==2&&tri.triangleSubtype==1))?0:2);if(tex){const int off=(1<<(2-mode))*64*tex->segment;int x1=int(tex->u1)+off,x2=int(tex->u2)+off,x3=int(tex->u3)+off;int y1=tex->v1,y2=tex->v2,y3=tex->v3;if(x1>x2||x1>x3)++x1;if(x2>x1||x2>x3)++x2;if(x3>x2||x3>x1)++x3;if(y1>y2||y1>y3)++y1;if(y2>y1||y2>y3)++y2;if(y3>y2||y3>y1)++y3;const float u[3]={float(x1),float(x2),float(x3)},v[3]={float(y1),float(y2),float(y3)};const int uv[3]={uvOrder,uvOrder1,uvOrder2};for(int k=0;k<3;++k){ux[k]=u[uv[k]];uy[k]=v[uv[k]];}}
       bool triangleVisible=true;
       for(int k=0;k<3;++k){const auto p=displayPositions[std::size_t(vi[k])];if(toCamera(p.x,p.y,p.z).depth<=0.1f)triangleVisible=false;}
       if(!triangleVisible)continue;
       const int blendMode=(!tex||!page)?3:(tex->blendMode==0?0:tex->blendMode==1?2:tex->blendMode==2?1:3);
       const TextureKey textureKey{page,mode,clutX,clutY};
       auto&out=batches[BatchKey{textureKey,blendMode}];
       for(int k=0;k<3;++k){const auto p=displayPositions[std::size_t(vi[k])];const auto cam=toCamera(p.x,p.y,p.z);const auto screen=projectCamera(cam);const float nx=2.0f*(screen.x-canvasMin.x)/canvasSize.x-1.0f,ny=1.0f-2.0f*(screen.y-canvasMin.y)/canvasSize.y;constexpr float nearP=.05f,farP=200.0f;const float cz=((farP+nearP)/(farP-nearP))*cam.depth-(2.0f*farP*nearP)/(farP-nearP);ImU32 col=IM_COL32(180,180,180,255);if(ci[k]>=0&&std::size_t(ci[k])<model->colors.size()){const auto&v=model->colors[std::size_t(ci[k])];col=IM_COL32(v.r,v.g,v.b,255);}auto channel=[](ImU32 v,int shift){return float((v>>shift)&255U)/255.0f;};const int width=1024>>mode;out.push_back({nx*cam.depth,ny*cam.depth,cz,cam.depth,channel(col,IM_COL32_R_SHIFT),channel(col,IM_COL32_G_SHIFT),channel(col,IM_COL32_B_SHIFT),1.0f,page?ux[k]/float(width):0.0f,page?uy[k]/128.0f:0.0f});}
      }
      auto getTexture=[&](const TextureKey&key){const auto*page=std::get<0>(key);if(!page)return 0U;auto it=gpu.textures.find(key);if(it!=gpu.textures.end())return it->second;const int mode=std::get<1>(key),cx=std::get<2>(key),cy=std::get<3>(key),width=1024>>mode;std::vector<std::uint8_t>rgba(std::size_t(width)*128U*4U,0);auto ex=[](unsigned x){return (x*255U+15U)/31U;};for(int y=0;y<128;++y)for(int x=0;x<width;++x){const std::size_t row=std::size_t(y)*512U;std::uint16_t raw=0;if(mode<2){const unsigned idx=mode==0?(((*page)[row+std::size_t(x/2)]>>((x&1)?4:0))&15U):(*page)[row+std::size_t(x)];const std::size_t pos=std::size_t(cy)*512U+std::size_t(cx*16+int(idx))*2U;if(pos+1U>=page->size())continue;raw=std::uint16_t((*page)[pos])|(std::uint16_t((*page)[pos+1U])<<8U);}else{const std::size_t pos=row+std::size_t(x)*2U;if(pos+1U>=page->size())continue;raw=std::uint16_t((*page)[pos])|(std::uint16_t((*page)[pos+1U])<<8U);}const std::size_t o=(std::size_t(y)*std::size_t(width)+std::size_t(x))*4U;rgba[o]=std::uint8_t(ex(raw&31U));rgba[o+1U]=std::uint8_t(ex((raw>>5U)&31U));rgba[o+2U]=std::uint8_t(ex((raw>>10U)&31U));rgba[o+3U]=std::uint8_t((raw&0x7fffU)==0&&!(raw&0x8000U)?0U:((raw&0x8000U)?128U:255U));}unsigned int id=0;GL::GenTextures(1,&id);GL::BindTexture(0x0DE1U,id);GL::TexParameteri(0x0DE1U,0x2801U,0x2600);GL::TexParameteri(0x0DE1U,0x2800U,0x2600);GL::TexImage2D(0x0DE1U,0,0x8058,width,128,0,0x1908U,0x1401U,rgba.data());gpu.textures.emplace(key,id);return id;};
      GL::Viewport(0,0,w,h);GL::Enable(0x0B71U);GL::DepthFunc(0x0201U);GL::DepthMask(1);GL::Enable(0x0BE2U);GL::BlendEquation(0x8006U);GL::BlendFuncSeparate(0x0302U,0x0303U,1U,0x0303U);GL::ClearColor(0,0,0,0);GL::Clear(0x00004000U|0x00000100U);GL::UseProgram(gpu.program);GL::BindVertexArray(gpu.vao);GL::BindBuffer(0x8892U,gpu.vbo);GL::Uniform1i(GL::GetUniformLocation(gpu.program,"uTexture"),0);const int has=GL::GetUniformLocation(gpu.program,"uHasTexture"),blendMask=GL::GetUniformLocation(gpu.program,"uBlendMask"),blendMode=GL::GetUniformLocation(gpu.program,"uBlendMode");
      auto drawPass=[&](int mask){GL::Uniform1i(blendMask,mask);for(const auto&batch:batches){const auto&textureKey=batch.first.first;const int mode=batch.first.second;if(mask!=3&&mode!=mask)continue;const auto*page=std::get<0>(textureKey);GL::BindTexture(0x0DE1U,page&&!animationTexturesDisabled_?getTexture(textureKey):0);GL::Uniform1i(has,page&&!animationTexturesDisabled_?1:0);GL::Uniform1i(blendMode,mode);const auto&v=batch.second;GL::BufferData(0x8892U,GL::SizePtr(v.size()*sizeof(Vertex)),v.data(),0x88E8U);if(!v.empty())GL::DrawArrays(0x0004U,0,int(v.size()));}};
      // Render the ground as real depth-buffer geometry. Models below the
      // selected Y plane are occluded; models above it remain visible.
      auto makePlaneVertex=[&](const CameraPoint&cam,ImU32 color,Vertex&out){
       if(cam.depth<=0.05f)return false;
       const float nx=(cam.x*focal/cam.depth)/canvasSize.x*2.0f;
       const float ny=(cam.y*focal/cam.depth)/canvasSize.y*2.0f;
       constexpr float nearP=.05f,farP=200.0f;
       const float cz=((farP+nearP)/(farP-nearP))*cam.depth-(2.0f*farP*nearP)/(farP-nearP);
       auto channel=[](ImU32 v,int shift){return float((v>>shift)&255U)/255.0f;};
       out={nx*cam.depth,ny*cam.depth,cz,cam.depth,channel(color,IM_COL32_R_SHIFT),channel(color,IM_COL32_G_SHIFT),channel(color,IM_COL32_B_SHIFT),channel(color,IM_COL32_A_SHIFT),0.0f,0.0f};
       return true;
      };
      // The ground plane is a reference grid only; no solid face is drawn.
      std::vector<Vertex> planeGrid;
      auto appendPlaneLine=[&](float x1,float z1,float x2,float z2,ImU32 color){
       CameraPoint a=toCamera(x1,groundY,z1),b=toCamera(x2,groundY,z2);
       constexpr float nearDepth=0.05f;
       if(a.depth<=nearDepth&&b.depth<=nearDepth)return;
       if(a.depth<=nearDepth||b.depth<=nearDepth){
        const float t=(nearDepth-a.depth)/(b.depth-a.depth);
        CameraPoint clipped{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,nearDepth};
        if(a.depth<=nearDepth)a=clipped;else b=clipped;
       }
       Vertex va{},vb{};if(makePlaneVertex(a,color,va)&&makePlaneVertex(b,color,vb)){planeGrid.push_back(va);planeGrid.push_back(vb);}
      };
      const int gridLines=16;const float gridStep=planeExtent/static_cast<float>(gridLines);
      for(int i=-gridLines;i<=gridLines;++i){
       const float offset=static_cast<float>(i)*gridStep;
       const ImU32 color=i==0?IM_COL32(105,112,124,210):IM_COL32(58,65,76,175);
       appendPlaneLine(planeX+offset,planeZ-planeExtent,planeX+offset,planeZ+planeExtent,color);
       appendPlaneLine(planeX-planeExtent,planeZ+offset,planeX+planeExtent,planeZ+offset,color);
      }
      GL::Uniform1i(has,0);GL::Uniform1i(blendMask,3);GL::Uniform1i(blendMode,3);
      GL::DepthFunc(0x0203U);GL::DepthMask(0);
      if(!planeGrid.empty()){GL::BufferData(0x8892U,GL::SizePtr(planeGrid.size()*sizeof(Vertex)),planeGrid.data(),0x88E8U);GL::DrawArrays(0x0001U,0,int(planeGrid.size()));}
      GL::DepthFunc(0x0201U);GL::DepthMask(1);
      drawPass(3);
      GL::DepthMask(0);GL::BlendEquation(0x8006U);GL::BlendFuncSeparate(0x0302U,0x0303U,1U,0x0303U);drawPass(0);
      // PS1 blend mode 1 is additive; mode 2 is subtractive. Keep the
      // equations aligned with the original CrashEdit blend-mode mapping.
      GL::BlendEquation(0x8006U);GL::BlendFuncSeparate(1U,1U,0U,1U);drawPass(2);
      GL::BlendEquation(0x800BU);GL::BlendFuncSeparate(1U,1U,0U,1U);drawPass(1);
      if(animationCollisionViewer_&&!frame.collisions.empty()){
       constexpr float collisionScale=102400.0f;
       const int boxFaces[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
       const int boxEdges[12][2]={{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
       std::vector<Vertex> collisionFaces,collisionLines,collisionPoints;
       auto appendCollisionVertex=[&](std::vector<Vertex>&out,const CameraPoint&cam,ImU32 color){
        if(cam.depth<=0.05f)return false;
        const float nx=(cam.x*focal/cam.depth)/canvasSize.x*2.0f;
        const float ny=(cam.y*focal/cam.depth)/canvasSize.y*2.0f;
        constexpr float nearP=.05f,farP=200.0f;
        const float cz=((farP+nearP)/(farP-nearP))*cam.depth-(2.0f*farP*nearP)/(farP-nearP);
        // Pull collision geometry a tiny amount toward the camera in depth
        // space so coincident collision/model surfaces do not z-fight.
        const float collisionDepthBias=0.00001f;
        auto channel=[](ImU32 v,int shift){return float((v>>shift)&255U)/255.0f;};
        out.push_back({nx*cam.depth,ny*cam.depth,cz-collisionDepthBias,cam.depth,channel(color,IM_COL32_R_SHIFT),channel(color,IM_COL32_G_SHIFT),channel(color,IM_COL32_B_SHIFT),channel(color,IM_COL32_A_SHIFT),0.0f,0.0f});
        return true;
       };
       for(const auto&collision:frame.collisions){
        const float x1=(float(collision.values[4])+float(collision.values[1]))/collisionScale;
        const float y1=(float(collision.values[5])+float(collision.values[2]))/collisionScale;
        const float z1=(float(collision.values[6])+float(collision.values[3]))/collisionScale;
        const float x2=(float(collision.values[7])+float(collision.values[1]))/collisionScale;
        const float y2=(float(collision.values[8])+float(collision.values[2]))/collisionScale;
        const float z2=(float(collision.values[9])+float(collision.values[3]))/collisionScale;
        const CameraPoint corners[8]={
         toCamera(x1,y1,z1),toCamera(x2,y1,z1),toCamera(x1,y2,z1),toCamera(x2,y2,z1),
         toCamera(x1,y1,z2),toCamera(x2,y1,z2),toCamera(x1,y2,z2),toCamera(x2,y2,z2)
        };
        for(const auto&face:boxFaces){
         const ImU32 fill=IM_COL32(0,255,0,42);
         bool visible=true;
         for(int i=0;i<4;++i)if(corners[face[i]].depth<=0.05f)visible=false;
         if(!visible)continue;
         for(int tri=0;tri<2;++tri){
          const int order[3]={0,tri==0?1:2,tri==0?2:3};
          for(int k:order)appendCollisionVertex(collisionFaces,corners[face[k]],fill);
         }
        }
        for(const auto&edge:boxEdges){
         const auto&a=corners[edge[0]],&b=corners[edge[1]];
         if(a.depth<=0.05f||b.depth<=0.05f)continue;
         appendCollisionVertex(collisionLines,a,IM_COL32(0,255,0,255));
         appendCollisionVertex(collisionLines,b,IM_COL32(0,255,0,255));
        }
        for(const auto&corner:corners)
         appendCollisionVertex(collisionPoints,corner,IM_COL32(0,255,0,255));
       }
       GL::Uniform1i(has,0);GL::Uniform1i(blendMask,3);GL::Uniform1i(blendMode,3);
       GL::DepthFunc(0x0203U);GL::DepthMask(0);GL::BlendEquation(0x8006U);GL::BlendFuncSeparate(0x0302U,0x0303U,1U,0x0303U);
       if(!collisionFaces.empty()){GL::BufferData(0x8892U,GL::SizePtr(collisionFaces.size()*sizeof(Vertex)),collisionFaces.data(),0x88E8U);GL::DrawArrays(0x0004U,0,int(collisionFaces.size()));}
       if(!collisionLines.empty()){
        GL::LineWidth(2.5f);
        GL::BufferData(0x8892U,GL::SizePtr(collisionLines.size()*sizeof(Vertex)),collisionLines.data(),0x88E8U);
        GL::DrawArrays(0x0001U,0,int(collisionLines.size()));
        GL::LineWidth(1.0f);
       }
       if(!collisionPoints.empty()){
        GL::PointSize(7.0f);
        GL::BufferData(0x8892U,GL::SizePtr(collisionPoints.size()*sizeof(Vertex)),collisionPoints.data(),0x88E8U);
        GL::DrawArrays(0x0000U,0,int(collisionPoints.size()));
        GL::PointSize(1.0f);
       }
       GL::DepthFunc(0x0201U);GL::DepthMask(1);
      }
      GL::BindTexture(0x0DE1U,0);GL::BindVertexArray(0);GL::UseProgram(0);GL::Disable(0x0B71U);rendered=true;
     }
     GL::DepthMask(static_cast<unsigned char>(oldDepthMask!=0));GL::BindFramebuffer(0x8D40U,unsigned(oldFbo));GL::Viewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);GL::ActiveTexture(0x84C0U);GL::BindTexture(0x0DE1U,unsigned(oldTexture));GL::ActiveTexture(unsigned(oldActive));GL::BindRenderbuffer(0x8D41U,unsigned(oldRenderbuffer));
     if(rendered){dl->AddCallback(usePremultipliedAnimationImageBlend,nullptr);dl->AddImage((ImTextureID)(std::intptr_t)gpu.color,canvasMin,canvasMax,ImVec2(0,1),ImVec2(1,0));dl->AddCallback(ImDrawCallback_ResetRenderState,nullptr);}
    }
    if(!rendered)dl->AddText(ImVec2(canvasMin.x+16.0f,canvasMin.y+18.0f),IM_COL32(255,170,100,255),"Hardware renderer unavailable; choose Wireframe in viewport settings.");
   }
  }else{
   const char*message=!model?"No linked Model was found.":!model->polygonError.empty()?model->polygonError.c_str():"No triangles to display.";
   dl->AddText(ImVec2(canvasMin.x+16.0f,canvasMin.y+18.0f),IM_COL32(190,198,210,255),message);
  }
 // Match CrashEdit-RE: collision bounds use their own 102400-unit scale and
 // are positioned from the frame collision offset, independently of model scale.
 if(animationCollisionViewer_&&animationWireframe_&&!frame.collisions.empty()){
  constexpr float collisionScale=102400.0f;
  const ImU32 collisionFill=IM_COL32(0,255,0,42);
  const ImU32 collisionLine=IM_COL32(0,255,0,255);
  const int boxFaces[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
  const int boxEdges[12][2]={{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
  auto drawCollisionLine=[&](CameraPoint a,CameraPoint b){
   constexpr float nearDepth=0.1f;
   if(a.depth<=nearDepth&&b.depth<=nearDepth)return;
   if(a.depth<=nearDepth||b.depth<=nearDepth){
    const float t=(nearDepth-a.depth)/(b.depth-a.depth);
    const CameraPoint clipped{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,nearDepth};
    if(a.depth<=nearDepth)a=clipped;else b=clipped;
   }
   dl->AddLine(projectCamera(a),projectCamera(b),collisionLine,3.0f);
  };
  for(const auto&collision:frame.collisions){
   const float x1=(static_cast<float>(collision.values[4])+static_cast<float>(collision.values[1]))/collisionScale;
   const float y1=(static_cast<float>(collision.values[5])+static_cast<float>(collision.values[2]))/collisionScale;
   const float z1=(static_cast<float>(collision.values[6])+static_cast<float>(collision.values[3]))/collisionScale;
   const float x2=(static_cast<float>(collision.values[7])+static_cast<float>(collision.values[1]))/collisionScale;
   const float y2=(static_cast<float>(collision.values[8])+static_cast<float>(collision.values[2]))/collisionScale;
   const float z2=(static_cast<float>(collision.values[9])+static_cast<float>(collision.values[3]))/collisionScale;
   const CameraPoint corners[8]={
    toCamera(x1,y1,z1),toCamera(x2,y1,z1),toCamera(x1,y2,z1),toCamera(x2,y2,z1),
    toCamera(x1,y1,z2),toCamera(x2,y1,z2),toCamera(x1,y2,z2),toCamera(x2,y2,z2)
   };
   for(const auto&face:boxFaces){
    bool visible=true;ImVec2 points[4];
    for(int i=0;i<4;++i){const auto&corner=corners[face[i]];if(corner.depth<=0.1f){visible=false;break;}points[i]=projectCamera(corner);}
    if(visible)dl->AddConvexPolyFilled(points,4,collisionFill);
   }
   for(const auto&edge:boxEdges)drawCollisionLine(corners[edge[0]],corners[edge[1]]);
   for(const auto&corner:corners){
    if(corner.depth>0.1f)dl->AddCircleFilled(projectCamera(corner),3.5f,collisionLine,12);
   }
  }
 }
 
 }
 dl->PopClipRect();
 ImDrawList*toolbarDraw=ImGui::GetWindowDrawList();
 toolbarDraw->AddRectFilled(toolbarMin,toolbarMax,ImGui::GetColorU32(toolbarHovered?ImGuiCol_ButtonHovered:ImGuiCol_Button),4.0f);
 const ImVec2 iconCenter((toolbarMin.x+toolbarMax.x)*0.5f,(toolbarMin.y+toolbarMax.y)*0.5f);
 const ImU32 iconColor=ImGui::GetColorU32(ImGuiCol_Text);
 for(int dot=-1;dot<=1;++dot)toolbarDraw->AddCircleFilled(ImVec2(iconCenter.x+float(dot)*5.0f,iconCenter.y),1.6f,iconColor);
 ImGui::TextDisabled("Left drag: orbit | Right drag: pan | Wheel: zoom | WASD: move | Q/E: down/up | C: collisions | X: wireframe | T: textures | R: reset camera");
 ImGui::End();
}

void EditorUI::drawBottom(){if(editorPreferences.showConsole){ImGui::Begin("Console / Diagnostics",&editorPreferences.showConsole);const auto&messages=Diagnostics::messages();ImGui::Text("Messages: %zu  |  Warnings: %zu  |  Errors: %zu",messages.size(),Diagnostics::count(DiagnosticLevel::Warning),Diagnostics::count(DiagnosticLevel::Error));ImGui::SameLine();if(ImGui::SmallButton("Clear"))Diagnostics::clear();ImGui::Separator();if(ImGui::BeginChild("DiagnosticsLog",ImVec2(0,0),false,ImGuiWindowFlags_HorizontalScrollbar)){for(const auto&message:Diagnostics::messages()){const char*prefix=message.level==DiagnosticLevel::Error?"[error]":message.level==DiagnosticLevel::Warning?"[warning]":"[info]";const ImVec4 color=message.level==DiagnosticLevel::Error?ImVec4(1.0f,.38f,.34f,1.0f):message.level==DiagnosticLevel::Warning?ImVec4(1.0f,.78f,.30f,1.0f):ImVec4(.78f,.82f,.88f,1.0f);ImGui::TextColored(color,"%s",prefix);ImGui::SameLine();ImGui::TextWrapped("%s",message.message.c_str());}if(ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-2.0f)ImGui::SetScrollHereY(1.0f);}ImGui::EndChild();ImGui::End();}if(editorPreferences.showProperties){ImGui::Begin("Properties",&editorPreferences.showProperties,ImGuiWindowFlags_HorizontalScrollbar);if(nsfValid_&&selectedChunk_==-2&&nsdValid_){ImGui::Text("NSD: %s",nsdPath_.filename().string().c_str());ImGui::Text("Game: %s",versionName(gameVersion_));ImGui::Text("Level ID: %08X",static_cast<unsigned>(nsd_.id));ImGui::Text("Total entities: %d",nsd_.entityCount);ImGui::Text("Chunks: %d | Index entries: %zu",nsd_.chunkCount,nsd_.index.size());ImGui::Text("Spawn points: %zu",nsd_.spawns.size());if(selectedSpawn_>=0&&static_cast<std::size_t>(selectedSpawn_)<nsd_.spawns.size()){const auto&p=nsd_.spawns[static_cast<std::size_t>(selectedSpawn_)];ImGui::Separator();ImGui::Text("Spawn %d",selectedSpawn_);ImGui::Text("Zone EID: %s (%08X)",Crash::eidToEName(static_cast<std::uint32_t>(p.zoneEid)).c_str(),static_cast<unsigned>(p.zoneEid));ImGui::Text("Camera: %d",p.camera);ImGui::Text("Point: %d",p.point);ImGui::Text("X: %08X  Y: %08X  Z: %08X",static_cast<std::uint32_t>(p.x),static_cast<std::uint32_t>(p.y),static_cast<std::uint32_t>(p.z));if(ImGui::Button("Open matching NSF zone")){bool found=false;for(std::size_t ci=0;ci<nsf_.chunks().size()&&!found;++ci){const auto&chunk=nsf_.chunks()[ci];for(std::size_t ei=0;ei<chunk.entries.size();++ei){const auto&entry=chunk.entries[ei];if(entry.valid&&entry.type==7&&entry.eid==static_cast<std::uint32_t>(p.zoneEid)){selectedChunk_=static_cast<int>(ci);selectedEntry_=static_cast<int>(ei);selectedEntity_=-1;selectedSpawn_=-1;openSelectedHierarchy_=true;viewerChunk_=static_cast<int>(ci);viewerEntry_=static_cast<int>(ei);sceneCameraInitialized_=false;found=true;break;}}}if(!found)status_="No matching zone entry was found in this NSF.";}}}else if(nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){const auto&c=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];ImGui::Text("Chunk %d",selectedChunk_);ImGui::Text("Type: %s (%u)",chunkTypeName(c.type),static_cast<unsigned>(c.type));ImGui::Text("Chunk ID: %04X",static_cast<unsigned>(c.chunkId));ImGui::Text("Size: %zu bytes",c.bytes.size());ImGui::Text("Compressed in NSF: %s",c.wasCompressed?"Yes":"No");if(c.type==0){ImGui::Text("Entry count: %zu",c.entries.size());ImGui::Text("Entry table: %s",c.entryTableValid?"Valid":"Invalid / unavailable");if(!c.entryTableError.empty())ImGui::TextWrapped("%s",c.entryTableError.c_str());if(selectedEntry_>=0&&static_cast<std::size_t>(selectedEntry_)<c.entries.size()){const auto&e=c.entries[static_cast<std::size_t>(selectedEntry_)];ImGui::Separator();ImGui::Text("Entry %d",selectedEntry_);if(e.valid){ImGui::Text("EID: %s (%08X)",Crash::eidToEName(e.eid).c_str(),e.eid);ImGui::Text("Type: %u",e.type);ImGui::Text("Items: %u",e.itemCount);ImGui::Text("Offset: 0x%zX | Size: %zu",e.offset,e.size);if(e.type==7&&e.zone.decoded){const auto&z=e.zone;ImGui::Separator();ImGui::Text("Zone Header: %zu bytes",z.headerSize);ImGui::Text("Worlds: %d | Zones: %d",z.worldCount,z.zoneCount);ImGui::Text("Cameras: %d | Entities: %d | Info: %d",z.cameraCount,z.entityCount,z.infoCount);ImGui::Text("Music: %d | Flags: %08X",z.music,static_cast<unsigned>(z.zoneFlags));ImGui::Separator();ImGui::Text("Layout position: %d, %d, %d",z.x,z.y,z.z);ImGui::Text("Dimensions: %d x %d x %d",z.width,z.height,z.depth);ImGui::Text("Collision depth: %u, %u, %u",z.collisionDepthX,z.collisionDepthY,z.collisionDepthZ);if(selectedEntity_>=0&&static_cast<std::size_t>(selectedEntity_)<z.entities.size()){const auto&ent=z.entities[static_cast<std::size_t>(selectedEntity_)];ImGui::Separator();if(ent.hasCameraIndex&&ent.hasCameraSubIndex){ImGui::Text("Camera [%d]-%d",ent.cameraIndex,ent.cameraSubIndex);ImGui::Separator();ImGui::Text("Camera Index: %s",ent.hasCameraIndex?std::to_string(ent.cameraIndex).c_str():"Not present");ImGui::Text("Camera SubIndex: %s",ent.hasCameraSubIndex?std::to_string(ent.cameraSubIndex).c_str():"Not present");ImGui::TextDisabled("Camera entities are the first CameraCount entities in the zone.");}else{if(!ent.name.empty()&&ent.hasId)ImGui::Text("%s [ID %d]",ent.name.c_str(),ent.id);else if(ent.hasId)ImGui::Text("Entity [ID %d]",ent.id);else ImGui::Text("Entity %d",selectedEntity_-z.cameraCount);ImGui::Separator();if(ent.hasGoolType&&ent.hasSubtype){const auto goolEid=static_cast<std::uint32_t>(ent.goolType);if(Crash::isValidEid(goolEid))ImGui::Text("GOOL: %s (%08X) | Subtype: %d",Crash::eidToEName(goolEid).c_str(),goolEid,ent.subtype);else ImGui::Text("GOOL: %08X | Subtype: %d",goolEid,ent.subtype);if(ImGui::Button("Open matching GOOL entry")){bool found=false;for(std::size_t gi=0;gi<nsf_.chunks().size()&&!found;++gi){const auto&gc=nsf_.chunks()[gi];if(gc.type!=0)continue;for(std::size_t ge=0;ge<gc.entries.size();++ge){const auto&gEntry=gc.entries[ge];if(gEntry.valid&&gEntry.type==11&&gEntry.eid==goolEid){selectedChunk_=static_cast<int>(gi);selectedEntry_=static_cast<int>(ge);selectedEntity_=-1;selectedSpawn_=-1;openSelectedHierarchy_=true;found=true;break;}}}if(!found)status_="No matching GOOL entry was found for this entity.";}}else ImGui::TextDisabled("GOOL Type/Subtype not defined");if(ent.hasId){int v=ent.id;if(ImGui::InputInt("Object ID",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x009fU,0,v))entityDataChanged_=true;}}if(ent.hasGoolType){int v=ent.goolType;if(ImGui::InputInt("GOOL Type / EID",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a9U,0,v))entityDataChanged_=true;}}if(ent.hasSubtype){int v=ent.subtype;if(ImGui::InputInt("GOOL Subtype",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00aaU,0,v))entityDataChanged_=true;}}if(ent.hasAlternateId)ImGui::Text("Alternate ID: %d",ent.alternateId);if(!ent.positions.empty()){ImGui::Text("Positions: %zu",ent.positions.size());for(std::size_t pi=0;pi<ent.positions.size();++pi){const auto&pos=ent.positions[pi];ImGui::BulletText("[%zu] X %d  Y %d  Z %d",pi,pos.x,pos.y,pos.z);}}if(!ent.settings.empty()){ImGui::Separator();ImGui::Text("GOOL Arguments / Settings (%zu)",ent.settings.size());if(ImGui::BeginTable("GoolSettingsV2",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX)){ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,55.0f);ImGui::TableSetupColumn("A (8-bit)",ImGuiTableColumnFlags_WidthFixed,105.0f);ImGui::TableSetupColumn("B (signed 24-bit)",ImGuiTableColumnFlags_WidthFixed,185.0f);ImGui::TableHeadersRow();for(std::size_t si=0;si<ent.settings.size();++si){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",si);ImGui::TableSetColumnIndex(1);int argA=ent.settings[si].valueA;if(ImGui::InputInt(("##argA"+std::to_string(si)).c_str(),&argA)){argA=std::clamp(argA,0,255);const auto raw=(static_cast<std::uint32_t>(ent.settings[si].valueB)<<8)|(static_cast<std::uint32_t>(argA)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a4U,si,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}ImGui::TableSetColumnIndex(2);int argB=ent.settings[si].valueB;if(ImGui::InputInt(("##argB"+std::to_string(si)).c_str(),&argB)){argB=std::clamp(argB,-8388608,8388607);const auto raw=(static_cast<std::uint32_t>(argB)<<8)|(static_cast<std::uint32_t>(ent.settings[si].valueA)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a4U,si,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}ImGui::EndTable();}}if(!ent.victims.empty()){ImGui::Separator();ImGui::Text("Victim List (%zu)",ent.victims.size());for(std::size_t vi=0;vi<ent.victims.size();++vi){int victim=ent.victims[vi];ImGui::PushID(static_cast<int>(vi));if(ImGui::InputInt("Object ID",&victim)){victim=std::clamp(victim,-32768,32767);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0287U,vi,victim))entityDataChanged_=true;}ImGui::PopID();}}if(ent.hasBoxCount){int v=ent.boxCount;if(ImGui::InputInt("Box Count (Willy)",&v)){const auto raw=(static_cast<std::uint32_t>(v)<<8)|(static_cast<std::uint32_t>(ent.properties.empty()?0:0)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x028bU,0,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}if(ent.hasDdaSettings){int v=ent.ddaDeathCount;if(ImGui::InputInt("DDA Death / 1-Up Count",&v)){const auto raw=static_cast<std::uint32_t>(v)<<8;if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0277U,0,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}if(ent.hasDdaSection){int v=ent.ddaSection;if(ImGui::InputInt("DDA Section",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0288U,0,v))entityDataChanged_=true;}}}ImGui::Text("Declared length: %u | Properties: %zu",ent.declaredLength,ent.properties.size());if(!ent.error.empty())ImGui::TextWrapped("%s",ent.error.c_str());if(ImGui::BeginTable("EntityPropertyTableV2",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX)){ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,170.0f);ImGui::TableSetupColumn("ID",ImGuiTableColumnFlags_WidthFixed,65.0f);ImGui::TableSetupColumn("Type",ImGuiTableColumnFlags_WidthFixed,85.0f);ImGui::TableSetupColumn("Rows",ImGuiTableColumnFlags_WidthFixed,65.0f);ImGui::TableSetupColumn("Decoded values",ImGuiTableColumnFlags_WidthFixed,320.0f);ImGui::TableHeadersRow();for(const auto&prop:ent.properties){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(entityPropertyName(static_cast<std::uint16_t>(prop.id)));ImGui::TableSetColumnIndex(1);ImGui::Text("0x%03X",static_cast<unsigned>(static_cast<std::uint16_t>(prop.id)));ImGui::TableSetColumnIndex(2);ImGui::Text("%u / %u B",prop.type,prop.elementSize);ImGui::TableSetColumnIndex(3);ImGui::Text("%d",prop.rowCount);ImGui::TableSetColumnIndex(4);if(!prop.error.empty())ImGui::TextDisabled("%s",prop.error.c_str());else if(!prop.positions.empty()){for(const auto&v:prop.positions)ImGui::Text("%d, %d, %d",v.x,v.y,v.z);}else if(!prop.intValues.empty()){std::string vals;for(std::size_t vi=0;vi<std::min<std::size_t>(prop.intValues.size(),8);++vi){if(vi)vals+=", ";vals+=std::to_string(prop.intValues[vi]);}if(prop.intValues.size()>8)vals+=" ...";ImGui::TextWrapped("%s",vals.c_str());}else ImGui::TextDisabled("%zu bytes (raw)",prop.dataSize);}ImGui::EndTable();}}}}else ImGui::TextWrapped("Entry could not be decoded: %s",e.error.c_str());}}else ImGui::TextDisabled("Specialized resource decoding is pending.");}else ImGui::TextDisabled("Select an NSD, spawn point, or NSF chunk.");ImGui::End();}if(entityDataChanged_){nsf_.decodeZones(gameVersion_==GameVersion::Crash3);entityDataChanged_=false;}}
}
