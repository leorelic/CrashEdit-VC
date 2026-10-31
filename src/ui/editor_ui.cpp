#include "ui/editor_ui.hpp"
#include "crash/formats/eid.hpp"
#include "render/gl_api.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <map>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cfloat>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <tuple>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif
namespace CrashEdit {
struct RendererDiagnostics { double sceneBuildMs=0.0,gpuSubmitCpuMs=0.0; std::size_t gpuDrawCalls=0,gpuUploadBytes=0,cachedTextureCount=0,cachedTextureBytes=0; };
static RendererDiagnostics rendererDiagnostics;
static bool showRendererDiagnostics=false;
static void grid(ImDrawList*d,ImVec2 a,ImVec2 b){ImVec2 c((a.x+b.x)/2,a.y+(b.y-a.y)*.58f);float h=a.y+(b.y-a.y)*.3f;d->AddRectFilled(a,b,IM_COL32(25,30,38,255));d->AddRectFilled(a,ImVec2(b.x,h),IM_COL32(29,36,46,255));for(int i=-12;i<=12;++i)d->AddLine(ImVec2(c.x+i*42,h),ImVec2(c.x+i*115,b.y),IM_COL32(86,105,126,95));for(int i=1;i<=12;++i){float t=float(i)/12;float y=h+(b.y-h)*t*t;d->AddLine(ImVec2(a.x,y),ImVec2(b.x,y),IM_COL32(86,105,126,95));}}
static const char* versionName(GameVersion v){switch(v){case GameVersion::Crash1Beta1995:return "Crash 1 — 1995 Beta";case GameVersion::Crash1BetaMAR08:return "Crash 1 — March 8 Beta";case GameVersion::Crash1BetaMAY11:return "Crash 1 — May 11 Beta";case GameVersion::Crash1:return "Crash 1";case GameVersion::Crash2:return "Crash 2";case GameVersion::Crash3BetaMAY14:return "Crash 3 — May 14 Beta";case GameVersion::Crash3:return "Crash 3";}return "Unknown";}
static Crash::NsdVariant nsdVariant(GameVersion v){if(v==GameVersion::Crash3||v==GameVersion::Crash3BetaMAY14)return Crash::NsdVariant::Crash3;if(v==GameVersion::Crash1Beta1995||v==GameVersion::Crash1BetaMAR08||v==GameVersion::Crash1BetaMAY11)return Crash::NsdVariant::Proto;if(v==GameVersion::Crash1)return Crash::NsdVariant::Old;return Crash::NsdVariant::Standard;}
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
 const std::string padded=std::string("   ")+label;
 const bool open=ImGui::TreeNodeEx(padded.c_str(),flags);
 const float scale=ImGui::GetIO().FontGlobalScale;
 pixelIcon(ImGui::GetWindowDrawList(),ImVec2(row.x+17.0f*scale,row.y+1.0f*scale),scale,icon);
 return open;
}
static const char* chunkIconName(std::uint16_t type){
 switch(type){case 0:return "JournalOrange";case 1:return "Painting";case 2:case 3:return "JournalBlue";case 4:return "MusicNoteRed";case 5:return "JournalWhite";default:return "JournalGray";}
}
static const char* entryIconName(std::uint32_t type){
 switch(type){case 1:return "ThingLime";case 3:return "ThingBlue";case 7:return "ThingViolet";case 21:return "Painting";case 6:case 15:case 17:return "ThingOrange";default:return "ThingGray";}
}
static const char* entityPropertyName(std::uint16_t id){switch(id){case 0x2C:return "Name";case 0x32:return "Z modifier";case 0x4B:return "Positions";case 0x9F:return "Entity ID";case 0xA4:return "GOOL arguments / Settings";case 0xA9:return "GOOL Type";case 0xAA:return "GOOL Subtype";case 0x277:return "DDA Death / 1-Up Count";case 0x287:return "Victim Object IDs";case 0x288:return "DDA Section";case 0x28B:return "Box Count (Willy)";case 0x119:return "Camera panning";case 0x130:return "Field of view";case 0x142:return "Camera distance";case 0x162:return "Camera field 0x162";case 0x16D:return "Camera setting 0x16D";case 0x173:return "Camera index";case 0x174:return "Camera sub-index";case 0x176:return "Camera path links";case 0x185:return "Camera flags";case 0x186:return "Camera water settings";case 0x198:return "Camera event sender";case 0x1A8:return "Camera transitions";case 0x1B5:return "Camera particles";case 0x1B8:return "Camera FX control";case 0x1DE:return "Camera fog distance";case 0x1FA:return "Camera backgrounds";case 0x27F:return "Camera scenery updates";case 0x297:return "Camera mirrors";case 0x2AA:return "Camera stars";default:return "Unknown / game-specific";}}
bool EditorUI::loadLevel(const std::filesystem::path& file,std::string& error){
 Crash::NsfArchive archive;if(!archive.load(file,error))return false;if(pendingVersion_==GameVersion::Crash2||pendingVersion_==GameVersion::Crash3BetaMAY14||pendingVersion_==GameVersion::Crash3)archive.decodeZones(pendingVersion_==GameVersion::Crash3);
 auto sidecar=file;sidecar.replace_extension(".nsd");Crash::NsdFile level;std::string nsdError;
 bool haveNsd=level.load(sidecar,nsdVariant(pendingVersion_),nsdError);
 if(!workspace_.open(file.parent_path(),error))return false;
 nsf_=std::move(archive);sceneCameraInitialized_=false;viewerChunk_=-1;viewerEntry_=-1;nsfPath_=file;nsdPath_=sidecar;nsdValid_=haveNsd;if(haveNsd)nsd_=std::move(level);
 nsfValid_=true;selectedChunk_=-1;selectedSpawn_=-1;selectedEntry_=-1;selectedEntity_=-1;gameVersion_=pendingVersion_;workspace_.selectResource(file.string());
 status_=std::string("Loaded ")+versionName(gameVersion_)+" NSF ("+std::to_string(nsf_.chunks().size())+" chunks).";
 if(!haveNsd)status_+=" Matching NSD unavailable: "+nsdError;
 return true;
}
void EditorUI::draw(){
 auto*v=ImGui::GetMainViewport();ImGui::SetNextWindowPos(v->WorkPos);ImGui::SetNextWindowSize(v->WorkSize);ImGui::SetNextWindowViewport(v->ID);
 ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoDocking;
 ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::Begin("CrashEditHost",nullptr,f);ImGui::PopStyleVar(3);drawMenu();drawToolbar();
 auto id=ImGui::GetID("CrashEditDockSpace");ImGui::DockSpace(id,ImVec2(0,0),ImGuiDockNodeFlags_PassthruCentralNode);ImGui::Separator();ImGui::TextUnformatted(status_.c_str());
 static bool built=false;if(!built){built=true;ImGui::DockBuilderRemoveNode(id);ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace|ImGuiDockNodeFlags_PassthruCentralNode);ImGui::DockBuilderSetNodeSize(id,v->WorkSize);ImGuiID center=id;auto right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.28f,nullptr,&center);auto rightBottom=ImGui::DockBuilderSplitNode(right,ImGuiDir_Down,.42f,nullptr,&right);auto bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.27f,nullptr,&center);ImGui::DockBuilderDockWindow("3D Zone Editor",center);ImGui::DockBuilderDockWindow("Resource Hierarchy",right);ImGui::DockBuilderDockWindow("Properties",rightBottom);ImGui::DockBuilderDockWindow("Console / Diagnostics",bottom);ImGui::DockBuilderDockWindow("Texture Viewer",bottom);ImGui::DockBuilderDockWindow("CLUT",bottom);ImGui::DockBuilderFinish(id);}
 drawHierarchy();drawViewport();drawBottom();
 const bool textureChunkSelected=nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()&&nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)].type==1;
 if(textureChunkSelected){drawTextureViewer();drawClutEditor();}
 ImGui::End();
 if(openDialog_){ImGui::OpenPopup("Open NSF");openDialog_=false;}
 if(ImGui::BeginPopupModal("Open NSF",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("Select a level NSF. The game version is requested before the matching NSD is interpreted.");ImGui::InputText("NSF file path",path_,sizeof(path_));ImGui::SameLine();if(ImGui::Button("Browse NSF...")){
#ifdef _WIN32
 OPENFILENAMEA ofn{};char selected[MAX_PATH]{};ofn.lStructSize=sizeof(ofn);ofn.lpstrFile=selected;ofn.nMaxFile=MAX_PATH;ofn.lpstrFilter="Crash Bandicoot level archives (*.nsf)\0*.nsf;*.NSF\0All files (*.*)\0*.*\0";ofn.nFilterIndex=1;ofn.Flags=OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_EXPLORER;if(GetOpenFileNameA(&ofn))std::snprintf(path_,sizeof(path_),"%s",selected);
#else
 status_="Enter the NSF path above; native file browsing is not yet available on this platform.";
#endif
 }if(ImGui::Button("Continue")){std::filesystem::path p=std::filesystem::u8path(path_);if(p.extension()==".nsf"||p.extension()==".NSF"){versionDialog_=true;ImGui::CloseCurrentPopup();}else status_="Please select an .nsf file.";}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 if(versionDialog_){ImGui::OpenPopup("Select Game Version");versionDialog_=false;}
 if(ImGui::BeginPopupModal("Select Game Version",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("Which Crash game/version does this NSF belong to? This controls the format loaders used for its resources and NSD.");const char* names[]={"Crash 1 — 1995 Beta","Crash 1 — March 8 Beta","Crash 1 — May 11 Beta","Crash 1","Crash 2","Crash 3 — May 14 Beta","Crash 3"};int choice=static_cast<int>(pendingVersion_);if(ImGui::Combo("Game version",&choice,names,7))pendingVersion_=static_cast<GameVersion>(choice);if(ImGui::Button("Open Level")){std::string e;if(loadLevel(std::filesystem::u8path(path_),e)){ImGui::CloseCurrentPopup();}else status_="Could not open level: "+e;}ImGui::SameLine();if(ImGui::Button("Back")){ImGui::CloseCurrentPopup();openDialog_=true;}ImGui::EndPopup();}
 if(about_){ImGui::OpenPopup("About CrashEdit");about_=false;}if(ImGui::BeginPopupModal("About CrashEdit",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::Text("CrashEdit — cross-platform rewrite");ImGui::Text("C++20 / Dear ImGui / GLFW / OpenGL");if(ImGui::Button("Close"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
}
void EditorUI::drawMenu(){
 static int uiScaling=100;
 static int appliedScaling=100;
 static ImGuiStyle baseStyle=ImGui::GetStyle();
 auto&io=ImGui::GetIO();
 if(uiScaling!=appliedScaling){
  ImGui::GetStyle()=baseStyle;
  ImGui::GetStyle().ScaleAllSizes(static_cast<float>(uiScaling)/100.0f);
  appliedScaling=uiScaling;
 }
 io.FontGlobalScale=static_cast<float>(uiScaling)/100.0f;
 if(!ImGui::BeginMenuBar())return;
 if(ImGui::BeginMenu("File")){
  if(ImGui::MenuItem("Open NSF...","Ctrl+O"))openDialog_=true;
  if(ImGui::MenuItem("Export Edited NSF...","Ctrl+Shift+S",false,nsfValid_)){auto out=nsfPath_.parent_path()/(nsfPath_.stem().string()+"_edited.nsf");std::string saveError;if(nsf_.saveUncompressed(out,saveError))status_="Exported edited NSF: "+out.string();else status_="NSF export failed: "+saveError;}
  if(ImGui::MenuItem("Close Workspace",nullptr,false,workspace_.isOpen())){workspace_.close();nsfValid_=nsdValid_=false;status_="Workspace closed.";}
  ImGui::Separator();if(ImGui::MenuItem("Exit"))std::exit(0);ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Edit")){ImGui::MenuItem("Undo","Ctrl+Z",false,false);ImGui::MenuItem("Redo","Ctrl+Y",false,false);ImGui::EndMenu();}
 if(ImGui::BeginMenu("View")){ImGui::MenuItem("Properties",nullptr,&properties_);ImGui::MenuItem("Console / Diagnostics",nullptr,&console_);ImGui::EndMenu();}
 if(ImGui::BeginMenu("Experimental")){
  static bool useModernFont=false;
  if(ImGui::MenuItem("Use modern UI font",nullptr,&useModernFont))io.FontDefault=(useModernFont&&io.Fonts->Fonts.Size>1)?io.Fonts->Fonts[1]:nullptr;
  ImGui::SetNextItemWidth(190.0f*static_cast<float>(uiScaling)/100.0f);
  ImGui::SliderInt("UI Scaling",&uiScaling,100,200,"%d%%");
  ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Help")){if(ImGui::MenuItem("About"))about_=true;ImGui::EndMenu();}
 ImGui::EndMenuBar();
}
void EditorUI::drawToolbar(){const auto&style=ImGui::GetStyle();const float toolbarHeight=ImGui::GetFrameHeight()+style.WindowPadding.y*2.0f+2.0f;ImGui::BeginChild("Toolbar",ImVec2(0,toolbarHeight),ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar);if(ImGui::Button("Open NSF"))openDialog_=true;ImGui::SameLine();ImGui::BeginDisabled(!nsfValid_);ImGui::Button("Save");ImGui::SameLine();ImGui::Button("Import");ImGui::SameLine();ImGui::Button("Export");ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("| Game: %s",nsfValid_?versionName(gameVersion_):"None");ImGui::EndChild();}
void EditorUI::drawHierarchy(){ImGui::Begin("Resource Hierarchy");if(ImGui::Button("Open NSF..."))openDialog_=true;ImGui::Separator();if(nsfValid_){if(nsdValid_){if(ImGui::Selectable(("NSD — "+nsdPath_.filename().string()).c_str(),selectedChunk_==-2,ImGuiSelectableFlags_SpanAvailWidth)){selectedChunk_=-2;selectedSpawn_=-1;selectedEntry_=-1;}if(ImGui::TreeNode("Spawn Points")){for(std::size_t i=0;i<nsd_.spawns.size();++i){char b[96];std::snprintf(b,sizeof(b),"Spawn %03zu | Zone %s",i,Crash::eidToEName(static_cast<std::uint32_t>(nsd_.spawns[i].zoneEid)).c_str());if(ImGui::Selectable(b,selectedSpawn_==static_cast<int>(i))){selectedChunk_=-2;selectedSpawn_=static_cast<int>(i);}}ImGui::TreePop();}}else ImGui::TextDisabled("NSD missing or unreadable");if(ImGui::TreeNode((nsfPath_.filename().string()+"##nsf").c_str())){for(std::size_t i=0;i<nsf_.chunks().size();++i){const auto&c=nsf_.chunks()[i];char label[192];if(c.type==1){const auto textureEid=static_cast<std::uint32_t>(c.chunkId);const auto textureName=Crash::eidToEName(textureEid);std::snprintf(label,sizeof(label),"Chunk %03zu | EID %s | %s",i,textureName.c_str(),chunkTypeName(c.type));}else{std::snprintf(label,sizeof(label),"Chunk %03zu | %s",i,chunkTypeName(c.type));}bool opened=treeNodeWithIcon(label,ImGuiTreeNodeFlags_SpanAvailWidth,chunkIconName(c.type));if(ImGui::IsItemClicked()){selectedChunk_=static_cast<int>(i);selectedEntry_=-1;selectedSpawn_=-1;}if(opened){if(c.type==0){if(!c.entryTableValid)ImGui::TextDisabled("%s",c.entryTableError.empty()?"Entry table unavailable":c.entryTableError.c_str());else for(std::size_t j=0;j<c.entries.size();++j){const auto&e=c.entries[j];char entryLabel[192];if(e.valid)std::snprintf(entryLabel,sizeof(entryLabel),"Entry %03zu | %s | Type %u | Items %u",j,Crash::eidToEName(e.eid).c_str(),e.type,e.itemCount);else std::snprintf(entryLabel,sizeof(entryLabel),"Entry %03zu | Invalid",j);bool entryOpen=treeNodeWithIcon(entryLabel,ImGuiTreeNodeFlags_SpanAvailWidth,entryIconName(e.type));if(ImGui::IsItemClicked()){selectedChunk_=static_cast<int>(i);selectedEntry_=static_cast<int>(j);selectedEntity_=-1;selectedSpawn_=-1;if(e.valid&&((e.type==7&&e.zone.decoded)||(e.type==3&&e.scenery.decoded))){viewerChunk_=static_cast<int>(i);viewerEntry_=static_cast<int>(j);}}if(entryOpen){if(e.type==7){if(e.zone.decoded){ImGui::BulletText("Zone Header | %d entities | %d cameras",e.zone.entityCount,e.zone.cameraCount);ImGui::BulletText("Layout | %d x %d x %d",e.zone.width,e.zone.height,e.zone.depth);for(std::size_t k=0;k<e.zone.entities.size();++k){const auto&ent=e.zone.entities[k];const bool camera=k<static_cast<std::size_t>(e.zone.cameraCount);char entLabel[128];if(camera){const int cameraIndex=static_cast<int>(k/3);const int cameraSubIndex=static_cast<int>(k%3);std::snprintf(entLabel,sizeof(entLabel),"Camera [%d]-%d",cameraIndex,cameraSubIndex);}else if(!ent.name.empty()&&ent.hasId)std::snprintf(entLabel,sizeof(entLabel),"%s [ID %d]",ent.name.c_str(),ent.id);else if(ent.hasId)std::snprintf(entLabel,sizeof(entLabel),"Entity [ID %d]",ent.id);else std::snprintf(entLabel,sizeof(entLabel),"Entity %03zu",k-static_cast<std::size_t>(e.zone.cameraCount));{const ImVec2 rowPos=ImGui::GetCursorScreenPos();const float iconScale=ImGui::GetIO().FontGlobalScale;const auto&style=ImGui::GetStyle();ImGui::PushID(static_cast<int>(k));const bool entitySelected=selectedChunk_==static_cast<int>(i)&&selectedEntry_==static_cast<int>(j)&&selectedEntity_==static_cast<int>(k);if(ImGui::Selectable("##EntityRow",entitySelected,ImGuiSelectableFlags_SpanAvailWidth)){selectedChunk_=static_cast<int>(i);selectedEntry_=static_cast<int>(j);selectedEntity_=static_cast<int>(k);selectedSpawn_=-1;}const ImVec2 textPos(rowPos.x+style.FramePadding.x+20.0f*iconScale,rowPos.y+style.FramePadding.y);pixelIcon(ImGui::GetWindowDrawList(),ImVec2(rowPos.x+style.FramePadding.x,rowPos.y+(ImGui::GetFrameHeight()-16.0f*iconScale)*0.5f),iconScale,"Arrow");ImGui::GetWindowDrawList()->AddText(textPos,ImGui::GetColorU32(ImGuiCol_Text),entLabel);ImGui::PopID();}}}else ImGui::TextDisabled("Zone not decoded: %s",e.zone.error.c_str());}else{for(std::size_t k=0;k<e.items.size();++k){const ImVec2 itemPos=ImGui::GetCursorScreenPos();const float iconScale=ImGui::GetIO().FontGlobalScale;char itemLabel[96];std::snprintf(itemLabel,sizeof(itemLabel),"   Item %zu | %zu bytes",k,e.items[k].size);ImGui::Selectable(itemLabel,false,ImGuiSelectableFlags_SpanAvailWidth);pixelIcon(ImGui::GetWindowDrawList(),ImVec2(itemPos.x+17.0f*iconScale,itemPos.y+1.0f*iconScale),iconScale,"ThingLime");}}ImGui::TreePop();}}}ImGui::TreePop();}}ImGui::TreePop();}}else if(!workspace_.isOpen())ImGui::TextDisabled("No NSF archive loaded.");else for(const auto&n:workspace_.resources())drawNode(n);ImGui::End();}
void EditorUI::drawNode(const ResourceNode&n){std::string label=(n.isDirectory?"[+] ":"    ")+n.name;if(n.isDirectory){bool open=ImGui::TreeNodeEx(label.c_str(),ImGuiTreeNodeFlags_SpanAvailWidth|(n.children.empty()?ImGuiTreeNodeFlags_Leaf:0));if(ImGui::IsItemClicked())workspace_.selectResource(n.path.string());if(open){for(const auto&c:n.children)drawNode(c);ImGui::TreePop();}}else if(ImGui::Selectable(label.c_str(),workspace_.selectedResource()==n.path.string(),ImGuiSelectableFlags_SpanAvailWidth)){workspace_.selectResource(n.path.string());if(n.path.extension()==".nsf"||n.path.extension()==".NSF"){std::snprintf(path_,sizeof(path_),"%s",n.path.string().c_str());openDialog_=true;}else status_="Selected: "+n.name+" ("+n.kind+")";}}
void EditorUI::drawViewport(){
 ImGui::Begin("3D Zone Editor");
 ImGui::TextDisabled("Scenery / Zone viewer | Left drag: orbit | Wheel: zoom | WASD/QE: move | Shift: world-aligned | Right click: renderer and movement speed");
 const ImVec2 canvasMin=ImGui::GetCursorScreenPos();
 ImVec2 canvasSize=ImGui::GetContentRegionAvail();
 // Never make the interaction canvas larger than the space actually available.
 canvasSize.x=std::max(canvasSize.x,1.0f);canvasSize.y=std::max(canvasSize.y,1.0f);
 ImGui::InvisibleButton("ZoneViewportCanvas",canvasSize,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
 const bool canvasHovered=ImGui::IsItemHovered();
 ImGui::OpenPopupOnItemClick("ViewportOptions",ImGuiPopupFlags_MouseButtonRight);
 if(ImGui::BeginPopup("ViewportOptions")){
  ImGui::TextUnformatted("Renderer");
  ImGui::Checkbox("Software rendering",&softwareRendering_);
  ImGui::Checkbox("Show performance diagnostics",&showRendererDiagnostics);
  ImGui::Separator();
  ImGui::TextUnformatted("Camera movement speed");
  ImGui::SetNextItemWidth(180.0f);
  ImGui::SliderInt("##MovementSpeed",&sceneMoveSpeed_,100,300,"%d%%");
  ImGui::EndPopup();
 }
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
 const std::uint32_t cameraResourceEid=zoneEntry?zoneEntry->eid:(sceneryEntry?sceneryEntry->eid:0U);
 if(!sceneryEntries.empty()&&sceneryCount>0){
  if(sceneCameraZoneEid_!=cameraResourceEid)sceneCameraInitialized_=false;
  if(!sceneCameraInitialized_&&frameMin.x!=FLT_MAX){
   sceneTargetX_=(frameMin.x+frameMax.x)*0.5f;sceneTargetY_=(frameMin.y+frameMax.y)*0.5f;sceneTargetZ_=(frameMin.z+frameMax.z)*0.5f;
   const float dx=frameMax.x-frameMin.x,dy=frameMax.y-frameMin.y,dz=frameMax.z-frameMin.z;
   const float radius=std::sqrt(dx*dx+dy*dy+dz*dz)*0.5f;
   sceneDistance_=std::clamp(radius*1.65f+0.75f,2.0f,80.0f);sceneCameraZoneEid_=cameraResourceEid;sceneCameraInitialized_=true;
  }
 }
 const ImGuiIO&io=ImGui::GetIO();
 if(canvasActive&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
  sceneYaw_-=io.MouseDelta.x*0.005f;
  scenePitch_=std::clamp(scenePitch_+io.MouseDelta.y*0.005f,-1.45f,1.45f);
 }
 if(canvasActive&&ImGui::IsMouseDragging(ImGuiMouseButton_Right)){
  const float pan=sceneDistance_*0.0018f;
  const float cy=std::cos(sceneYaw_),sy=std::sin(sceneYaw_);
  sceneTargetX_-=(-cy*io.MouseDelta.x+sy*io.MouseDelta.y)*pan;
  sceneTargetZ_-=(-sy*io.MouseDelta.x-cy*io.MouseDelta.y)*pan;
 }
 if(canvasHovered&&io.MouseWheel!=0.0f){
  const float delta=io.MouseWheel*(0.3f+1.7f*(sceneDistance_/100.0f));
  sceneDistance_=std::clamp(sceneDistance_-delta,0.2f,200.0f);
 }
 if(canvasHovered&&ImGui::IsKeyDown(ImGuiKey_R)){sceneCameraInitialized_=false;}
 if(canvasHovered){
  const float moveSpeed=(2.0f*float(sceneMoveSpeed_)/100.0f)*(0.2f+0.8f*(sceneDistance_/20.0f))*io.DeltaTime;
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
  if(softwareRendering_)std::stable_sort(faces.begin(),faces.end(),[](const Face&a,const Face&b){if(a.sky!=b.sky)return a.sky;return a.depth>b.depth;});
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
  if(softwareRendering_){
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
     GL::Viewport(0,0,targetWidth,targetHeight);GL::Enable(0x0B71U);GL::DepthFunc(0x0201U);
     GL::Enable(0x0BE2U);GL::BlendFuncSeparate(0x0302U,0x0303U,1U,0x0303U);
     GL::ClearColor(0.078f,0.098f,0.13f,1.0f);GL::Clear(0x00004000U|0x00000100U);
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
     GL::BlendFuncSeparate(0x0302U,0x0303U,1U,0x0303U);
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
      GL::BlendFuncSeparate(sourceFactor,destinationFactor,sourceFactor,destinationFactor);
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
  if(zone){
  const V3 origin{float(zone->x)/400.0f,float(zone->y)/400.0f,float(zone->z)/400.0f};
  const V3 corners[8]={{origin.x,origin.y,origin.z},{origin.x+float(zone->width)/400.0f,origin.y,origin.z},{origin.x,origin.y,origin.z+float(zone->depth)/400.0f},{origin.x+float(zone->width)/400.0f,origin.y,origin.z+float(zone->depth)/400.0f},{origin.x,origin.y+float(zone->height)/400.0f,origin.z},{origin.x+float(zone->width)/400.0f,origin.y+float(zone->height)/400.0f,origin.z},{origin.x,origin.y+float(zone->height)/400.0f,origin.z+float(zone->depth)/400.0f},{origin.x+float(zone->width)/400.0f,origin.y+float(zone->height)/400.0f,origin.z+float(zone->depth)/400.0f}};
  const int edges[12][2]={{0,1},{0,2},{1,3},{2,3},{4,5},{4,6},{5,7},{6,7},{0,4},{1,5},{2,6},{3,7}};
  for(const auto&e:edges){const auto a=project(corners[e[0]]),b=project(corners[e[1]]);if(a.depth>0.05f&&b.depth>0.05f)dl->AddLine(ImVec2(a.x,a.y),ImVec2(b.x,b.y),IM_COL32(110,155,190,180),1.2f);}
  // Draw each multi-position entity as a connected path, matching the
  // original viewer's waypoint/path visualization.
  for(std::size_t i=static_cast<std::size_t>(zone->cameraCount);i<zone->entities.size();++i){
   const auto&entity=zone->entities[i];if(entity.positions.empty())continue;
   std::vector<Projected> path;path.reserve(entity.positions.size());
   for(const auto&p:entity.positions)path.push_back(project({origin.x+float(p.x)/100.0f,origin.y+float(p.y)/100.0f,origin.z+float(p.z)/100.0f}));
   for(std::size_t pi=1;pi<path.size();++pi)if(path[pi-1].depth>0.05f&&path[pi].depth>0.05f)dl->AddLine(ImVec2(path[pi-1].x,path[pi-1].y),ImVec2(path[pi].x,path[pi].y),IM_COL32(245,190,75,220),1.8f);
   for(const auto&q:path)if(q.depth>0.05f)dl->AddCircleFilled(ImVec2(q.x,q.y),4.0f,IM_COL32(245,190,75,255));
  }
  // Camera path is stored on the first entity of each three-entity camera
  // group; the other two entities carry camera angle data.
  for(int i=0;i+2<zone->cameraCount&&static_cast<std::size_t>(i+2)<zone->entities.size();i+=3){
   const auto&camera=zone->entities[static_cast<std::size_t>(i)];std::vector<Projected> path;path.reserve(camera.positions.size());
   for(const auto&p:camera.positions)path.push_back(project({origin.x+float(p.x)/400.0f,origin.y+float(p.y)/400.0f,origin.z+float(p.z)/400.0f}));
   for(std::size_t pi=1;pi<path.size();++pi)if(path[pi-1].depth>0.05f&&path[pi].depth>0.05f)dl->AddLine(ImVec2(path[pi-1].x,path[pi-1].y),ImVec2(path[pi].x,path[pi].y),IM_COL32(80,210,130,255),2.0f);
   for(const auto&q:path)if(q.depth>0.05f)dl->AddCircleFilled(ImVec2(q.x,q.y),3.5f,IM_COL32(235,85,85,255));
  }
  // Render linked zones as translucent neighbors, matching OG CrashEdit's
  // master (255 alpha) / neighbor (128 alpha) distinction.
  for(const auto*neighborEntry:neighborZoneEntries){
   const auto&neighbor=neighborEntry->zone;
   const V3 neighborOrigin{float(neighbor.x)/400.0f,float(neighbor.y)/400.0f,float(neighbor.z)/400.0f};
   const V3 corners[8]={{neighborOrigin.x,neighborOrigin.y,neighborOrigin.z},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y,neighborOrigin.z},{neighborOrigin.x,neighborOrigin.y,neighborOrigin.z+float(neighbor.depth)/400.0f},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y,neighborOrigin.z+float(neighbor.depth)/400.0f},{neighborOrigin.x,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z},{neighborOrigin.x,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z+float(neighbor.depth)/400.0f},{neighborOrigin.x+float(neighbor.width)/400.0f,neighborOrigin.y+float(neighbor.height)/400.0f,neighborOrigin.z+float(neighbor.depth)/400.0f}};
   const int edges[12][2]={{0,1},{0,2},{1,3},{2,3},{4,5},{4,6},{5,7},{6,7},{0,4},{1,5},{2,6},{3,7}};
   for(const auto&e:edges){const auto a=project(corners[e[0]]),b=project(corners[e[1]]);if(a.depth>0.05f&&b.depth>0.05f)dl->AddLine(ImVec2(a.x,a.y),ImVec2(b.x,b.y),IM_COL32(110,155,190,128),1.0f);}
   for(std::size_t i=static_cast<std::size_t>(neighbor.cameraCount);i<neighbor.entities.size();++i){
    const auto&entity=neighbor.entities[i];if(entity.positions.empty())continue;
    std::vector<Projected> path;path.reserve(entity.positions.size());
    for(const auto&p:entity.positions)path.push_back(project({neighborOrigin.x+float(p.x)/100.0f,neighborOrigin.y+float(p.y)/100.0f,neighborOrigin.z+float(p.z)/100.0f}));
    for(std::size_t pi=1;pi<path.size();++pi)if(path[pi-1].depth>0.05f&&path[pi].depth>0.05f)dl->AddLine(ImVec2(path[pi-1].x,path[pi-1].y),ImVec2(path[pi].x,path[pi].y),IM_COL32(245,190,75,128),1.2f);
    for(const auto&q:path)if(q.depth>0.05f)dl->AddCircleFilled(ImVec2(q.x,q.y),3.0f,IM_COL32(245,190,75,128));
   }
   for(int i=0;i+2<neighbor.cameraCount&&static_cast<std::size_t>(i+2)<neighbor.entities.size();i+=3){
    const auto&camera=neighbor.entities[static_cast<std::size_t>(i)];std::vector<Projected> path;path.reserve(camera.positions.size());
    for(const auto&p:camera.positions)path.push_back(project({neighborOrigin.x+float(p.x)/400.0f,neighborOrigin.y+float(p.y)/400.0f,neighborOrigin.z+float(p.z)/400.0f}));
    for(std::size_t pi=1;pi<path.size();++pi)if(path[pi-1].depth>0.05f&&path[pi].depth>0.05f)dl->AddLine(ImVec2(path[pi-1].x,path[pi-1].y),ImVec2(path[pi].x,path[pi].y),IM_COL32(80,210,130,128),1.4f);
    for(const auto&q:path)if(q.depth>0.05f)dl->AddCircleFilled(ImVec2(q.x,q.y),2.5f,IM_COL32(235,85,85,128));
   }
  }
  }
  char stats[384];std::snprintf(stats,sizeof(stats),"Scenery: %zu | Vertices: %zu | Faces: %zu | Textured: %zu | TPAG OK: %zu | Missing: %zu | Distance: %.2f",sceneryCount,vertexCount,faceCount,texturedFaceCount,resolvedTexturePageCount,missingTexturePageCount,sceneDistance_);
  char performanceStats[384];std::snprintf(performanceStats,sizeof(performanceStats),"CPU scene build: %.2f ms | GPU submit (CPU): %.2f ms | Draw calls: %zu | Upload: %.1f KiB | Texture cache: %zu (%.2f MiB)",rendererDiagnostics.sceneBuildMs,rendererDiagnostics.gpuSubmitCpuMs,rendererDiagnostics.gpuDrawCalls,double(rendererDiagnostics.gpuUploadBytes)/1024.0,rendererDiagnostics.cachedTextureCount,double(rendererDiagnostics.cachedTextureBytes)/(1024.0*1024.0));
  const char* resourceLabel=zoneEntry?Crash::eidToEName(zoneEntry->eid).c_str():(sceneryEntry?Crash::eidToEName(sceneryEntry->eid).c_str():"No scenery selected");
  dl->AddText(ImVec2(canvasMin.x+12,canvasMin.y+10),IM_COL32(230,235,242,255),resourceLabel);
  dl->AddText(ImVec2(canvasMin.x+12,canvasMax.y-(showRendererDiagnostics?40.0f:22.0f)),IM_COL32(180,194,210,255),stats);
  if(showRendererDiagnostics)dl->AddText(ImVec2(canvasMin.x+12,canvasMax.y-22),IM_COL32(145,190,210,255),performanceStats);
  if(faceCount==0)dl->AddText(ImVec2(canvasMin.x+16,canvasMin.y+34),IM_COL32(245,190,105,255),"No visible scenery faces at this camera position");
 }else{
  dl->AddText(ImVec2(canvasMin.x+16,canvasMin.y+16),IM_COL32(205,211,220,255),nsfValid_?"Select a decoded zone entry to view its scenery":"No zone loaded — open an NSF");
 }
 dl->PopClipRect();
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
 if(ImGui::BeginTable("ClutPaletteGrid",16,ImGuiTableFlags_Borders|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollX)){
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
void EditorUI::drawBottom(){if(console_){ImGui::Begin("Console / Diagnostics",&console_);ImGui::TextUnformatted("[info] Native editor ready.");if(nsfValid_)ImGui::Text("NSF: %zu chunks | compressed: %zu",nsf_.chunks().size(),nsf_.compressedChunkCount());if(nsfValid_&&!nsdValid_)ImGui::TextWrapped("[warning] %s",status_.c_str());ImGui::End();}if(properties_){ImGui::Begin("Properties",&properties_);if(nsfValid_&&selectedChunk_==-2&&nsdValid_){ImGui::Text("NSD: %s",nsdPath_.filename().string().c_str());ImGui::Text("Game: %s",versionName(gameVersion_));ImGui::Text("Level ID: %08X",static_cast<unsigned>(nsd_.id));ImGui::Text("Total entities: %d",nsd_.entityCount);ImGui::Text("Chunks: %d | Index entries: %zu",nsd_.chunkCount,nsd_.index.size());ImGui::Text("Spawn points: %zu",nsd_.spawns.size());if(selectedSpawn_>=0&&static_cast<std::size_t>(selectedSpawn_)<nsd_.spawns.size()){const auto&p=nsd_.spawns[static_cast<std::size_t>(selectedSpawn_)];ImGui::Separator();ImGui::Text("Spawn %d",selectedSpawn_);ImGui::Text("Zone EID: %s (%08X)",Crash::eidToEName(static_cast<std::uint32_t>(p.zoneEid)).c_str(),static_cast<unsigned>(p.zoneEid));ImGui::Text("Camera: %d",p.camera);ImGui::Text("Point: %d",p.point);ImGui::Text("X: %08X  Y: %08X  Z: %08X",static_cast<std::uint32_t>(p.x),static_cast<std::uint32_t>(p.y),static_cast<std::uint32_t>(p.z));if(ImGui::Button("Open matching NSF zone")){bool found=false;for(std::size_t ci=0;ci<nsf_.chunks().size()&&!found;++ci){const auto&chunk=nsf_.chunks()[ci];for(std::size_t ei=0;ei<chunk.entries.size();++ei){const auto&entry=chunk.entries[ei];if(entry.valid&&entry.type==7&&entry.eid==static_cast<std::uint32_t>(p.zoneEid)){selectedChunk_=static_cast<int>(ci);selectedEntry_=static_cast<int>(ei);selectedEntity_=-1;selectedSpawn_=-1;viewerChunk_=static_cast<int>(ci);viewerEntry_=static_cast<int>(ei);sceneCameraInitialized_=false;found=true;break;}}}if(!found)status_="No matching zone entry was found in this NSF.";}}}else if(nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){const auto&c=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];ImGui::Text("Chunk %d",selectedChunk_);ImGui::Text("Type: %s (%u)",chunkTypeName(c.type),static_cast<unsigned>(c.type));ImGui::Text("Chunk ID: %04X",static_cast<unsigned>(c.chunkId));ImGui::Text("Size: %zu bytes",c.bytes.size());ImGui::Text("Compressed in NSF: %s",c.wasCompressed?"Yes":"No");if(c.type==0){ImGui::Text("Entry count: %zu",c.entries.size());ImGui::Text("Entry table: %s",c.entryTableValid?"Valid":"Invalid / unavailable");if(!c.entryTableError.empty())ImGui::TextWrapped("%s",c.entryTableError.c_str());if(selectedEntry_>=0&&static_cast<std::size_t>(selectedEntry_)<c.entries.size()){const auto&e=c.entries[static_cast<std::size_t>(selectedEntry_)];ImGui::Separator();ImGui::Text("Entry %d",selectedEntry_);if(e.valid){ImGui::Text("EID: %s (%08X)",Crash::eidToEName(e.eid).c_str(),e.eid);ImGui::Text("Type: %u",e.type);ImGui::Text("Items: %u",e.itemCount);ImGui::Text("Offset: 0x%zX | Size: %zu",e.offset,e.size);if(e.type==7&&e.zone.decoded){const auto&z=e.zone;ImGui::Separator();ImGui::Text("Zone Header: %zu bytes",z.headerSize);ImGui::Text("Worlds: %d | Zones: %d",z.worldCount,z.zoneCount);ImGui::Text("Cameras: %d | Entities: %d | Info: %d",z.cameraCount,z.entityCount,z.infoCount);ImGui::Text("Music: %d | Flags: %08X",z.music,static_cast<unsigned>(z.zoneFlags));ImGui::Separator();ImGui::Text("Layout position: %d, %d, %d",z.x,z.y,z.z);ImGui::Text("Dimensions: %d x %d x %d",z.width,z.height,z.depth);ImGui::Text("Collision depth: %u, %u, %u",z.collisionDepthX,z.collisionDepthY,z.collisionDepthZ);if(selectedEntity_>=0&&static_cast<std::size_t>(selectedEntity_)<z.entities.size()){const auto&ent=z.entities[static_cast<std::size_t>(selectedEntity_)];ImGui::Separator();if(static_cast<std::size_t>(selectedEntity_)<static_cast<std::size_t>(z.cameraCount)){ImGui::Text("Camera [%d]-%d",ent.hasCameraIndex?ent.cameraIndex:selectedEntity_,ent.hasCameraSubIndex?ent.cameraSubIndex:0);ImGui::Separator();ImGui::Text("Camera Index: %s",ent.hasCameraIndex?std::to_string(ent.cameraIndex).c_str():"Not present");ImGui::Text("Camera SubIndex: %s",ent.hasCameraSubIndex?std::to_string(ent.cameraSubIndex).c_str():"Not present");ImGui::TextDisabled("Camera entities are the first CameraCount entities in the zone.");}else{if(!ent.name.empty()&&ent.hasId)ImGui::Text("%s [ID %d]",ent.name.c_str(),ent.id);else if(ent.hasId)ImGui::Text("Entity [ID %d]",ent.id);else ImGui::Text("Entity %d",selectedEntity_-z.cameraCount);ImGui::Separator();if(ent.hasGoolType&&ent.hasSubtype){const auto goolEid=static_cast<std::uint32_t>(ent.goolType);if(Crash::isValidEid(goolEid))ImGui::Text("GOOL: %s (%08X) | Subtype: %d",Crash::eidToEName(goolEid).c_str(),goolEid,ent.subtype);else ImGui::Text("GOOL: %08X | Subtype: %d",goolEid,ent.subtype);}else ImGui::TextDisabled("GOOL Type/Subtype not defined");if(ent.hasId){int v=ent.id;if(ImGui::InputInt("Object ID",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x009fU,0,v))entityDataChanged_=true;}}if(ent.hasGoolType){int v=ent.goolType;if(ImGui::InputInt("GOOL Type / EID",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a9U,0,v))entityDataChanged_=true;}}if(ent.hasSubtype){int v=ent.subtype;if(ImGui::InputInt("GOOL Subtype",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00aaU,0,v))entityDataChanged_=true;}}if(ent.hasAlternateId)ImGui::Text("Alternate ID: %d",ent.alternateId);if(!ent.positions.empty()){ImGui::Text("Positions: %zu",ent.positions.size());for(std::size_t pi=0;pi<ent.positions.size();++pi){const auto&pos=ent.positions[pi];ImGui::BulletText("[%zu] X %d  Y %d  Z %d",pi,pos.x,pos.y,pos.z);}}if(!ent.settings.empty()){ImGui::Separator();ImGui::Text("GOOL Arguments / Settings (%zu)",ent.settings.size());if(ImGui::BeginTable("GoolSettings",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)){ImGui::TableSetupColumn("#");ImGui::TableSetupColumn("A (8-bit)");ImGui::TableSetupColumn("B (signed 24-bit)");ImGui::TableHeadersRow();for(std::size_t si=0;si<ent.settings.size();++si){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",si);ImGui::TableSetColumnIndex(1);int argA=ent.settings[si].valueA;if(ImGui::InputInt(("##argA"+std::to_string(si)).c_str(),&argA)){argA=std::clamp(argA,0,255);const auto raw=(static_cast<std::uint32_t>(ent.settings[si].valueB)<<8)|(static_cast<std::uint32_t>(argA)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a4U,si,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}ImGui::TableSetColumnIndex(2);int argB=ent.settings[si].valueB;if(ImGui::InputInt(("##argB"+std::to_string(si)).c_str(),&argB)){argB=std::clamp(argB,-8388608,8388607);const auto raw=(static_cast<std::uint32_t>(argB)<<8)|(static_cast<std::uint32_t>(ent.settings[si].valueA)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a4U,si,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}ImGui::EndTable();}}if(!ent.victims.empty()){ImGui::Separator();ImGui::Text("Victim List (%zu)",ent.victims.size());for(std::size_t vi=0;vi<ent.victims.size();++vi){int victim=ent.victims[vi];ImGui::PushID(static_cast<int>(vi));if(ImGui::InputInt("Object ID",&victim)){victim=std::clamp(victim,-32768,32767);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0287U,vi,victim))entityDataChanged_=true;}ImGui::PopID();}}if(ent.hasBoxCount){int v=ent.boxCount;if(ImGui::InputInt("Box Count (Willy)",&v)){const auto raw=(static_cast<std::uint32_t>(v)<<8)|(static_cast<std::uint32_t>(ent.properties.empty()?0:0)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x028bU,0,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}if(ent.hasDdaSettings){int v=ent.ddaDeathCount;if(ImGui::InputInt("DDA Death / 1-Up Count",&v)){const auto raw=static_cast<std::uint32_t>(v)<<8;if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0277U,0,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}if(ent.hasDdaSection){int v=ent.ddaSection;if(ImGui::InputInt("DDA Section",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0288U,0,v))entityDataChanged_=true;}}}ImGui::Text("Declared length: %u | Properties: %zu",ent.declaredLength,ent.properties.size());if(!ent.error.empty())ImGui::TextWrapped("%s",ent.error.c_str());if(ImGui::BeginTable("EntityPropertyTable",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp)){ImGui::TableSetupColumn("Property");ImGui::TableSetupColumn("ID");ImGui::TableSetupColumn("Type");ImGui::TableSetupColumn("Rows");ImGui::TableSetupColumn("Decoded values");ImGui::TableHeadersRow();for(const auto&prop:ent.properties){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(entityPropertyName(static_cast<std::uint16_t>(prop.id)));ImGui::TableSetColumnIndex(1);ImGui::Text("0x%03X",static_cast<unsigned>(static_cast<std::uint16_t>(prop.id)));ImGui::TableSetColumnIndex(2);ImGui::Text("%u / %u B",prop.type,prop.elementSize);ImGui::TableSetColumnIndex(3);ImGui::Text("%d",prop.rowCount);ImGui::TableSetColumnIndex(4);if(!prop.error.empty())ImGui::TextDisabled("%s",prop.error.c_str());else if(!prop.positions.empty()){for(const auto&v:prop.positions)ImGui::Text("%d, %d, %d",v.x,v.y,v.z);}else if(!prop.intValues.empty()){std::string vals;for(std::size_t vi=0;vi<std::min<std::size_t>(prop.intValues.size(),8);++vi){if(vi)vals+=", ";vals+=std::to_string(prop.intValues[vi]);}if(prop.intValues.size()>8)vals+=" ...";ImGui::TextWrapped("%s",vals.c_str());}else ImGui::TextDisabled("%zu bytes (raw)",prop.dataSize);}ImGui::EndTable();}}}}else ImGui::TextWrapped("Entry could not be decoded: %s",e.error.c_str());}}else ImGui::TextDisabled("Specialized resource decoding is pending.");}else ImGui::TextDisabled("Select an NSD, spawn point, or NSF chunk.");ImGui::End();}if(entityDataChanged_){nsf_.decodeZones(gameVersion_==GameVersion::Crash3);entityDataChanged_=false;}}
}
