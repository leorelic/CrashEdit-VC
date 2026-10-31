#include "ui/editor_ui.hpp"
#include "crash/formats/eid.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cfloat>
#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif
namespace CrashEdit {
static void grid(ImDrawList*d,ImVec2 a,ImVec2 b){ImVec2 c((a.x+b.x)/2,a.y+(b.y-a.y)*.58f);float h=a.y+(b.y-a.y)*.3f;d->AddRectFilled(a,b,IM_COL32(25,30,38,255));d->AddRectFilled(a,ImVec2(b.x,h),IM_COL32(29,36,46,255));for(int i=-12;i<=12;++i)d->AddLine(ImVec2(c.x+i*42,h),ImVec2(c.x+i*115,b.y),IM_COL32(86,105,126,95));for(int i=1;i<=12;++i){float t=float(i)/12;float y=h+(b.y-h)*t*t;d->AddLine(ImVec2(a.x,y),ImVec2(b.x,y),IM_COL32(86,105,126,95));}}
static const char* versionName(GameVersion v){switch(v){case GameVersion::Crash1Beta1995:return "Crash 1 — 1995 Beta";case GameVersion::Crash1BetaMAR08:return "Crash 1 — March 8 Beta";case GameVersion::Crash1BetaMAY11:return "Crash 1 — May 11 Beta";case GameVersion::Crash1:return "Crash 1";case GameVersion::Crash2:return "Crash 2";case GameVersion::Crash3BetaMAY14:return "Crash 3 — May 14 Beta";case GameVersion::Crash3:return "Crash 3";}return "Unknown";}
static Crash::NsdVariant nsdVariant(GameVersion v){if(v==GameVersion::Crash3||v==GameVersion::Crash3BetaMAY14)return Crash::NsdVariant::Crash3;if(v==GameVersion::Crash1Beta1995||v==GameVersion::Crash1BetaMAR08||v==GameVersion::Crash1BetaMAY11)return Crash::NsdVariant::Proto;if(v==GameVersion::Crash1)return Crash::NsdVariant::Old;return Crash::NsdVariant::Standard;}
static const char* chunkTypeName(std::uint16_t t){switch(t){case 0:return "Entry Chunk";case 1:return "Texture Chunk";case 2:return "Old Sound Chunk";case 3:return "Sound Chunk";case 4:return "Wavebank Chunk";case 5:return "Speech Chunk";default:return "Unknown Chunk";}}
static const char* entityPropertyName(std::uint16_t id){switch(id){case 0x2C:return "Name";case 0x32:return "Z modifier";case 0x4B:return "Positions";case 0x9F:return "Entity ID";case 0xA4:return "GOOL arguments / Settings";case 0xA9:return "GOOL Type";case 0xAA:return "GOOL Subtype";case 0x277:return "DDA Death / 1-Up Count";case 0x287:return "Victim Object IDs";case 0x288:return "DDA Section";case 0x28B:return "Box Count (Willy)";case 0x119:return "Camera panning";case 0x130:return "Field of view";case 0x142:return "Camera distance";case 0x162:return "Camera field 0x162";case 0x16D:return "Camera setting 0x16D";case 0x173:return "Camera index";case 0x174:return "Camera sub-index";case 0x176:return "Camera path links";case 0x185:return "Camera flags";case 0x186:return "Camera water settings";case 0x198:return "Camera event sender";case 0x1A8:return "Camera transitions";case 0x1B5:return "Camera particles";case 0x1B8:return "Camera FX control";case 0x1DE:return "Camera fog distance";case 0x1FA:return "Camera backgrounds";case 0x27F:return "Camera scenery updates";case 0x297:return "Camera mirrors";case 0x2AA:return "Camera stars";default:return "Unknown / game-specific";}}
bool EditorUI::loadLevel(const std::filesystem::path& file,std::string& error){
 Crash::NsfArchive archive;if(!archive.load(file,error))return false;if(pendingVersion_==GameVersion::Crash2||pendingVersion_==GameVersion::Crash3BetaMAY14||pendingVersion_==GameVersion::Crash3)archive.decodeZones(pendingVersion_==GameVersion::Crash3);
 auto sidecar=file;sidecar.replace_extension(".nsd");Crash::NsdFile level;std::string nsdError;
 bool haveNsd=level.load(sidecar,nsdVariant(pendingVersion_),nsdError);
 if(!workspace_.open(file.parent_path(),error))return false;
 nsf_=std::move(archive);sceneCameraInitialized_=false;nsfPath_=file;nsdPath_=sidecar;nsdValid_=haveNsd;if(haveNsd)nsd_=std::move(level);
 nsfValid_=true;selectedChunk_=-1;selectedSpawn_=-1;selectedEntry_=-1;selectedEntity_=-1;gameVersion_=pendingVersion_;workspace_.selectResource(file.string());
 status_=std::string("Loaded ")+versionName(gameVersion_)+" NSF ("+std::to_string(nsf_.chunks().size())+" chunks).";
 if(!haveNsd)status_+=" Matching NSD unavailable: "+nsdError;
 return true;
}
void EditorUI::draw(){
 auto*v=ImGui::GetMainViewport();ImGui::SetNextWindowPos(v->WorkPos);ImGui::SetNextWindowSize(v->WorkSize);ImGui::SetNextWindowViewport(v->ID);
 ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoDocking;
 ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::Begin("CrashEditHost",nullptr,f);ImGui::PopStyleVar(3);drawMenu();drawToolbar();auto id=ImGui::GetID("CrashEditDockSpace");ImGui::DockSpace(id,ImVec2(0,-24),ImGuiDockNodeFlags_PassthruCentralNode);ImGui::Separator();ImGui::TextUnformatted(status_.c_str());ImGui::End();
 static bool built=false;if(!built){built=true;ImGui::DockBuilderRemoveNode(id);ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace|ImGuiDockNodeFlags_PassthruCentralNode);ImGui::DockBuilderSetNodeSize(id,v->WorkSize);ImGuiID center=id;auto right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.28f,nullptr,&center);auto rightBottom=ImGui::DockBuilderSplitNode(right,ImGuiDir_Down,.42f,nullptr,&right);auto bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.27f,nullptr,&center);ImGui::DockBuilderDockWindow("3D Zone Editor",center);ImGui::DockBuilderDockWindow("Resource Hierarchy",right);ImGui::DockBuilderDockWindow("Properties",rightBottom);ImGui::DockBuilderDockWindow("Console / Diagnostics",bottom);ImGui::DockBuilderDockWindow("Asset Browser",bottom);ImGui::DockBuilderFinish(id);}
 drawHierarchy();drawViewport();drawBottom();
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
void EditorUI::drawMenu(){if(!ImGui::BeginMenuBar())return;if(ImGui::BeginMenu("File")){if(ImGui::MenuItem("Open NSF...","Ctrl+O"))openDialog_=true;if(ImGui::MenuItem("Export Edited NSF...","Ctrl+Shift+S",false,nsfValid_)){auto out=nsfPath_.parent_path()/(nsfPath_.stem().string()+"_edited.nsf");std::string saveError;if(nsf_.saveUncompressed(out,saveError))status_="Exported edited NSF: "+out.string();else status_="NSF export failed: "+saveError;}if(ImGui::MenuItem("Close Workspace",nullptr,false,workspace_.isOpen())){workspace_.close();nsfValid_=nsdValid_=false;status_="Workspace closed.";}ImGui::Separator();if(ImGui::MenuItem("Exit"))std::exit(0);ImGui::EndMenu();}if(ImGui::BeginMenu("Edit")){ImGui::MenuItem("Undo","Ctrl+Z",false,false);ImGui::MenuItem("Redo","Ctrl+Y",false,false);ImGui::EndMenu();}if(ImGui::BeginMenu("View")){ImGui::MenuItem("Properties",nullptr,&properties_);ImGui::MenuItem("Console / Diagnostics",nullptr,&console_);ImGui::MenuItem("Asset Browser",nullptr,&assets_);ImGui::EndMenu();}if(ImGui::BeginMenu("Help")){if(ImGui::MenuItem("About"))about_=true;ImGui::EndMenu();}ImGui::EndMenuBar();}
void EditorUI::drawToolbar(){ImGui::BeginChild("Toolbar",ImVec2(0,34),ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar);if(ImGui::Button("Open NSF"))openDialog_=true;ImGui::SameLine();ImGui::BeginDisabled(!nsfValid_);ImGui::Button("Save");ImGui::SameLine();ImGui::Button("Import");ImGui::SameLine();ImGui::Button("Export");ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("| Game: %s",nsfValid_?versionName(gameVersion_):"None");ImGui::EndChild();}
void EditorUI::drawHierarchy(){ImGui::Begin("Resource Hierarchy");if(ImGui::Button("Open NSF..."))openDialog_=true;ImGui::Separator();if(nsfValid_){if(nsdValid_){if(ImGui::Selectable(("NSD — "+nsdPath_.filename().string()).c_str(),selectedChunk_==-2,ImGuiSelectableFlags_SpanAvailWidth)){selectedChunk_=-2;selectedSpawn_=-1;selectedEntry_=-1;}if(ImGui::TreeNode("Spawn Points")){for(std::size_t i=0;i<nsd_.spawns.size();++i){char b[96];std::snprintf(b,sizeof(b),"Spawn %03zu | Zone %s",i,Crash::eidToEName(static_cast<std::uint32_t>(nsd_.spawns[i].zoneEid)).c_str());if(ImGui::Selectable(b,selectedSpawn_==static_cast<int>(i))){selectedChunk_=-2;selectedSpawn_=static_cast<int>(i);}}ImGui::TreePop();}}else ImGui::TextDisabled("NSD missing or unreadable");if(ImGui::TreeNode((nsfPath_.filename().string()+"##nsf").c_str())){for(std::size_t i=0;i<nsf_.chunks().size();++i){const auto&c=nsf_.chunks()[i];char label[160];std::snprintf(label,sizeof(label),"Chunk %03zu | ID %04X | %s",i,static_cast<unsigned>(c.chunkId),chunkTypeName(c.type));bool opened=ImGui::TreeNodeEx(label,ImGuiTreeNodeFlags_SpanAvailWidth);if(ImGui::IsItemClicked()){selectedChunk_=static_cast<int>(i);selectedEntry_=-1;selectedSpawn_=-1;}if(opened){if(c.type==0){if(!c.entryTableValid)ImGui::TextDisabled("%s",c.entryTableError.empty()?"Entry table unavailable":c.entryTableError.c_str());else for(std::size_t j=0;j<c.entries.size();++j){const auto&e=c.entries[j];char entryLabel[192];if(e.valid)std::snprintf(entryLabel,sizeof(entryLabel),"Entry %03zu | %s | Type %u | Items %u",j,Crash::eidToEName(e.eid).c_str(),e.type,e.itemCount);else std::snprintf(entryLabel,sizeof(entryLabel),"Entry %03zu | Invalid",j);bool entryOpen=ImGui::TreeNodeEx(entryLabel,ImGuiTreeNodeFlags_SpanAvailWidth);if(ImGui::IsItemClicked()){selectedChunk_=static_cast<int>(i);selectedEntry_=static_cast<int>(j);selectedEntity_=-1;selectedSpawn_=-1;}if(entryOpen){if(e.type==7){if(e.zone.decoded){ImGui::BulletText("Zone Header | %d entities | %d cameras",e.zone.entityCount,e.zone.cameraCount);ImGui::BulletText("Layout | %d x %d x %d",e.zone.width,e.zone.height,e.zone.depth);for(std::size_t k=0;k<e.zone.entities.size();++k){const auto&ent=e.zone.entities[k];const bool camera=k<static_cast<std::size_t>(e.zone.cameraCount);char entLabel[128];if(camera){const int cameraIndex=static_cast<int>(k/3);const int cameraSubIndex=static_cast<int>(k%3);std::snprintf(entLabel,sizeof(entLabel),"Camera [%d]-%d",cameraIndex,cameraSubIndex);}else if(!ent.name.empty()&&ent.hasId)std::snprintf(entLabel,sizeof(entLabel),"%s [ID %d]",ent.name.c_str(),ent.id);else if(ent.hasId)std::snprintf(entLabel,sizeof(entLabel),"Entity [ID %d]",ent.id);else std::snprintf(entLabel,sizeof(entLabel),"Entity %03zu",k-static_cast<std::size_t>(e.zone.cameraCount));if(ImGui::Selectable(entLabel,selectedChunk_==static_cast<int>(i)&&selectedEntry_==static_cast<int>(j)&&selectedEntity_==static_cast<int>(k))){selectedChunk_=static_cast<int>(i);selectedEntry_=static_cast<int>(j);selectedEntity_=static_cast<int>(k);selectedSpawn_=-1;}}}else ImGui::TextDisabled("Zone not decoded: %s",e.zone.error.c_str());}else{for(std::size_t k=0;k<e.items.size();++k)ImGui::BulletText("Item %zu | %zu bytes",k,e.items[k].size);}ImGui::TreePop();}}}ImGui::TreePop();}}ImGui::TreePop();}}else if(!workspace_.isOpen())ImGui::TextDisabled("No NSF archive loaded.");else for(const auto&n:workspace_.resources())drawNode(n);ImGui::End();}
void EditorUI::drawNode(const ResourceNode&n){std::string label=(n.isDirectory?"[+] ":"    ")+n.name;if(n.isDirectory){bool open=ImGui::TreeNodeEx(label.c_str(),ImGuiTreeNodeFlags_SpanAvailWidth|(n.children.empty()?ImGuiTreeNodeFlags_Leaf:0));if(ImGui::IsItemClicked())workspace_.selectResource(n.path.string());if(open){for(const auto&c:n.children)drawNode(c);ImGui::TreePop();}}else if(ImGui::Selectable(label.c_str(),workspace_.selectedResource()==n.path.string(),ImGuiSelectableFlags_SpanAvailWidth)){workspace_.selectResource(n.path.string());if(n.path.extension()==".nsf"||n.path.extension()==".NSF"){std::snprintf(path_,sizeof(path_),"%s",n.path.string().c_str());openDialog_=true;}else status_="Selected: "+n.name+" ("+n.kind+")";}}
void EditorUI::drawViewport(){
 ImGui::Begin("3D Zone Editor");
 ImGui::TextDisabled("Scenery / Zone viewer | Left drag: orbit | Wheel: zoom | WASD/QE: move | Shift: world-aligned | Right click: movement speed");
 const ImVec2 canvasMin=ImGui::GetCursorScreenPos();
 ImVec2 canvasSize=ImGui::GetContentRegionAvail();
 canvasSize.x=std::max(canvasSize.x,100.0f);canvasSize.y=std::max(canvasSize.y,100.0f);
 ImGui::InvisibleButton("ZoneViewportCanvas",canvasSize,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
 const bool canvasHovered=ImGui::IsItemHovered();
 ImGui::OpenPopupOnItemClick("ViewportMovementSpeed",ImGuiPopupFlags_MouseButtonRight);
 if(ImGui::BeginPopup("ViewportMovementSpeed")){
  ImGui::TextUnformatted("Camera movement speed");
  ImGui::SetNextItemWidth(180.0f);
  ImGui::SliderInt("##MovementSpeed",&sceneMoveSpeed_,100,150,"%d%%");
  ImGui::EndPopup();
 }
 const bool canvasActive=ImGui::IsItemActive();
 ImDrawList*dl=ImGui::GetWindowDrawList();
 const ImVec2 canvasMax(canvasMin.x+canvasSize.x,canvasMin.y+canvasSize.y);
 dl->AddRectFilled(canvasMin,canvasMax,IM_COL32(20,25,33,255));
 const Crash::NsfZone*zone=nullptr;const Crash::NsfEntry*zoneEntry=nullptr;
 const Crash::NsfScenery*standaloneScenery=nullptr;const Crash::NsfEntry*sceneryEntry=nullptr;
 if(nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){
  const auto&ch=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];
  if(ch.type==0&&selectedEntry_>=0&&static_cast<std::size_t>(selectedEntry_)<ch.entries.size()){
   const auto&e=ch.entries[static_cast<std::size_t>(selectedEntry_)];
   if(e.valid&&e.type==7&&e.zone.decoded){zone=&e.zone;zoneEntry=&e;}
   else if(e.valid&&e.type==3&&e.scenery.decoded){standaloneScenery=&e.scenery;sceneryEntry=&e;}
  }
 }
 std::vector<const Crash::NsfEntry*> sceneryEntries;
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
 struct Face{Projected p[3];ImU32 colors[3]{};float depth=0;};
 std::vector<Face>faces;
 std::size_t sceneryCount=0,vertexCount=0,faceCount=0;
 V3 boundsMin{FLT_MAX,FLT_MAX,FLT_MAX},boundsMax{-FLT_MAX,-FLT_MAX,-FLT_MAX};
 V3 frameMin{FLT_MAX,FLT_MAX,FLT_MAX},frameMax{-FLT_MAX,-FLT_MAX,-FLT_MAX};
 auto includePoint=[&](V3 p){boundsMin.x=std::min(boundsMin.x,p.x);boundsMin.y=std::min(boundsMin.y,p.y);boundsMin.z=std::min(boundsMin.z,p.z);boundsMax.x=std::max(boundsMax.x,p.x);boundsMax.y=std::max(boundsMax.y,p.y);boundsMax.z=std::max(boundsMax.z,p.z);};
 auto includeFramePoint=[&](V3 p){frameMin.x=std::min(frameMin.x,p.x);frameMin.y=std::min(frameMin.y,p.y);frameMin.z=std::min(frameMin.z,p.z);frameMax.x=std::max(frameMax.x,p.x);frameMax.y=std::max(frameMax.y,p.y);frameMax.z=std::max(frameMax.z,p.z);};
 for(const auto*entry:sceneryEntries){
  const auto&sc=entry->scenery;++sceneryCount;vertexCount+=sc.vertices.size();
  // Scenery offsets are already world-space offsets; do not add zone XYZ.
  for(const auto&v:sc.vertices)includePoint({(float(v.x)*16.0f+float(sc.xOffset))/400.0f,(float(v.y)*16.0f+float(sc.yOffset))/400.0f,(float(v.z)*16.0f+float(sc.zOffset))/400.0f});
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
     auto vertexPosition=[&](std::uint16_t index)->V3{const auto&v=sc.vertices[index];return {(float(v.x)*16.0f+float(sc.xOffset))/400.0f,(float(v.y)*16.0f+float(sc.yOffset))/400.0f,(float(v.z)*16.0f+float(sc.zOffset))/400.0f};};
     auto vertexColor=[&](std::uint16_t index)->ImU32{
      const auto&v=sc.vertices[index];
      if(v.colorIndex<sc.colors.size()){const auto&c=sc.colors[v.colorIndex];return IM_COL32(c.r,c.g,c.b,255);}
      return IM_COL32(180,0,180,255);
     };
     auto addFace=[&](std::uint16_t ia,std::uint16_t ib,std::uint16_t ic){
      if(ia>=sc.vertices.size()||ib>=sc.vertices.size()||ic>=sc.vertices.size())return;
      const auto pa=project(vertexPosition(ia)),pb=project(vertexPosition(ib)),pc=project(vertexPosition(ic));
      if(pa.depth<=0.05f||pb.depth<=0.05f||pc.depth<=0.05f)return;
      const float minX=std::min({pa.x,pb.x,pc.x}),maxX=std::max({pa.x,pb.x,pc.x});
      const float minY=std::min({pa.y,pb.y,pc.y}),maxY=std::max({pa.y,pb.y,pc.y});
      if(maxX<canvasMin.x||minX>canvasMax.x||maxY<canvasMin.y||minY>canvasMax.y)return;
      Face f{};f.p[0]=pa;f.p[1]=pb;f.p[2]=pc;f.colors[0]=vertexColor(ia);f.colors[1]=vertexColor(ib);f.colors[2]=vertexColor(ic);f.depth=(pa.depth+pb.depth+pc.depth)/3.0f;faces.push_back(f);
     };
     for(const auto&t:sc.triangles)addFace(t.a,t.b,t.c);
     for(const auto&q:sc.quads){addFace(q.a,q.b,q.c);addFace(q.a,q.c,q.d);}
  }
  std::sort(faces.begin(),faces.end(),[](const Face&a,const Face&b){return a.depth>b.depth;});
  auto channel=[](ImU32 c,int shift){return float((c>>shift)&255U);};
  auto lerpColor=[&](const Face&f,float wa,float wb,float wc){const auto mix=[&](int shift){return static_cast<unsigned char>(std::clamp(channel(f.colors[0],shift)*wa+channel(f.colors[1],shift)*wb+channel(f.colors[2],shift)*wc,0.0f,255.0f));};return IM_COL32(mix(IM_COL32_R_SHIFT),mix(IM_COL32_G_SHIFT),mix(IM_COL32_B_SHIFT),255);};
  for(const auto&f:faces){
   constexpr int subdivisions=3;
   auto point=[&](float a,float b,float c){return ImVec2(f.p[0].x*a+f.p[1].x*b+f.p[2].x*c,f.p[0].y*a+f.p[1].y*b+f.p[2].y*c);};
   for(int i=0;i<subdivisions;++i)for(int j=0;j<subdivisions-i;++j){
    const float n=float(subdivisions),a=float(i)/n,b=float(j)/n,c=1.0f-a-b;
    const float a1=float(i+1)/n,b1=float(j)/n,c1=1.0f-a1-b1;
    const float a2=float(i)/n,b2=float(j+1)/n,c2=1.0f-a2-b2;
    dl->AddTriangleFilled(point(c,a,b),point(c1,a1,b1),point(c2,a2,b2),lerpColor(f,(c+c1+c2)/3.0f,(a+a1+a2)/3.0f,(b+b1+b2)/3.0f));
    if(i+j<subdivisions-1){const float a3=float(i+1)/n,b3=float(j+1)/n,c3=1.0f-a3-b3;dl->AddTriangleFilled(point(c1,a1,b1),point(c3,a3,b3),point(c2,a2,b2),lerpColor(f,(c1+c3+c2)/3.0f,(a1+a3+a2)/3.0f,(b1+b3+b2)/3.0f));}
   }
   ++faceCount;
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

  }
  char stats[192];std::snprintf(stats,sizeof(stats),"Scenery: %zu | Vertices: %zu | Faces: %zu | Distance: %.2f",sceneryCount,vertexCount,faceCount,sceneDistance_);
  const char* resourceLabel=zoneEntry?Crash::eidToEName(zoneEntry->eid).c_str():(sceneryEntry?Crash::eidToEName(sceneryEntry->eid).c_str():"No scenery selected");
  dl->AddText(ImVec2(canvasMin.x+12,canvasMin.y+10),IM_COL32(230,235,242,255),resourceLabel);
  dl->AddText(ImVec2(canvasMin.x+12,canvasMax.y-22),IM_COL32(180,194,210,255),stats);
  if(faceCount==0)dl->AddText(ImVec2(canvasMin.x+16,canvasMin.y+34),IM_COL32(245,190,105,255),"No visible scenery faces at this camera position");
 }else{
  dl->AddText(ImVec2(canvasMin.x+16,canvasMin.y+16),IM_COL32(205,211,220,255),nsfValid_?"Select a decoded zone entry to view its scenery":"No zone loaded — open an NSF");
 }
 ImGui::End();
}
void EditorUI::drawBottom(){if(console_){ImGui::Begin("Console / Diagnostics",&console_);ImGui::TextUnformatted("[info] Native editor ready.");if(nsfValid_)ImGui::Text("NSF: %zu chunks | compressed: %zu",nsf_.chunks().size(),nsf_.compressedChunkCount());if(nsfValid_&&!nsdValid_)ImGui::TextWrapped("[warning] %s",status_.c_str());ImGui::End();}if(properties_){ImGui::Begin("Properties",&properties_);if(nsfValid_&&selectedChunk_==-2&&nsdValid_){ImGui::Text("NSD: %s",nsdPath_.filename().string().c_str());ImGui::Text("Game: %s",versionName(gameVersion_));ImGui::Text("Level ID: %08X",static_cast<unsigned>(nsd_.id));ImGui::Text("Total entities: %d",nsd_.entityCount);ImGui::Text("Chunks: %d | Index entries: %zu",nsd_.chunkCount,nsd_.index.size());ImGui::Text("Spawn points: %zu",nsd_.spawns.size());if(selectedSpawn_>=0&&static_cast<std::size_t>(selectedSpawn_)<nsd_.spawns.size()){const auto&p=nsd_.spawns[static_cast<std::size_t>(selectedSpawn_)];ImGui::Separator();ImGui::Text("Spawn %d",selectedSpawn_);ImGui::Text("Zone EID: %s (%08X)",Crash::eidToEName(static_cast<std::uint32_t>(p.zoneEid)).c_str(),static_cast<unsigned>(p.zoneEid));ImGui::Text("Camera: %d",p.camera);ImGui::Text("Point: %d",p.point);ImGui::Text("X: %08X  Y: %08X  Z: %08X",static_cast<std::uint32_t>(p.x),static_cast<std::uint32_t>(p.y),static_cast<std::uint32_t>(p.z));if(ImGui::Button("Open matching NSF zone")){bool found=false;for(std::size_t ci=0;ci<nsf_.chunks().size()&&!found;++ci){const auto&chunk=nsf_.chunks()[ci];for(std::size_t ei=0;ei<chunk.entries.size();++ei){const auto&entry=chunk.entries[ei];if(entry.valid&&entry.type==7&&entry.eid==static_cast<std::uint32_t>(p.zoneEid)){selectedChunk_=static_cast<int>(ci);selectedEntry_=static_cast<int>(ei);selectedEntity_=-1;selectedSpawn_=-1;found=true;break;}}}if(!found)status_="No matching zone entry was found in this NSF.";}}}else if(nsfValid_&&selectedChunk_>=0&&static_cast<std::size_t>(selectedChunk_)<nsf_.chunks().size()){const auto&c=nsf_.chunks()[static_cast<std::size_t>(selectedChunk_)];ImGui::Text("Chunk %d",selectedChunk_);ImGui::Text("Type: %s (%u)",chunkTypeName(c.type),static_cast<unsigned>(c.type));ImGui::Text("Chunk ID: %04X",static_cast<unsigned>(c.chunkId));ImGui::Text("Size: %zu bytes",c.bytes.size());ImGui::Text("Compressed in NSF: %s",c.wasCompressed?"Yes":"No");if(c.type==0){ImGui::Text("Entry count: %zu",c.entries.size());ImGui::Text("Entry table: %s",c.entryTableValid?"Valid":"Invalid / unavailable");if(!c.entryTableError.empty())ImGui::TextWrapped("%s",c.entryTableError.c_str());if(selectedEntry_>=0&&static_cast<std::size_t>(selectedEntry_)<c.entries.size()){const auto&e=c.entries[static_cast<std::size_t>(selectedEntry_)];ImGui::Separator();ImGui::Text("Entry %d",selectedEntry_);if(e.valid){ImGui::Text("EID: %s (%08X)",Crash::eidToEName(e.eid).c_str(),e.eid);ImGui::Text("Type: %u",e.type);ImGui::Text("Items: %u",e.itemCount);ImGui::Text("Offset: 0x%zX | Size: %zu",e.offset,e.size);if(e.type==7&&e.zone.decoded){const auto&z=e.zone;ImGui::Separator();ImGui::Text("Zone Header: %zu bytes",z.headerSize);ImGui::Text("Worlds: %d | Zones: %d",z.worldCount,z.zoneCount);ImGui::Text("Cameras: %d | Entities: %d | Info: %d",z.cameraCount,z.entityCount,z.infoCount);ImGui::Text("Music: %d | Flags: %08X",z.music,static_cast<unsigned>(z.zoneFlags));ImGui::Separator();ImGui::Text("Layout position: %d, %d, %d",z.x,z.y,z.z);ImGui::Text("Dimensions: %d x %d x %d",z.width,z.height,z.depth);ImGui::Text("Collision depth: %u, %u, %u",z.collisionDepthX,z.collisionDepthY,z.collisionDepthZ);if(selectedEntity_>=0&&static_cast<std::size_t>(selectedEntity_)<z.entities.size()){const auto&ent=z.entities[static_cast<std::size_t>(selectedEntity_)];ImGui::Separator();if(static_cast<std::size_t>(selectedEntity_)<static_cast<std::size_t>(z.cameraCount)){ImGui::Text("Camera [%d]-%d",ent.hasCameraIndex?ent.cameraIndex:selectedEntity_,ent.hasCameraSubIndex?ent.cameraSubIndex:0);ImGui::Separator();ImGui::Text("Camera Index: %s",ent.hasCameraIndex?std::to_string(ent.cameraIndex).c_str():"Not present");ImGui::Text("Camera SubIndex: %s",ent.hasCameraSubIndex?std::to_string(ent.cameraSubIndex).c_str():"Not present");ImGui::TextDisabled("Camera entities are the first CameraCount entities in the zone.");}else{if(!ent.name.empty()&&ent.hasId)ImGui::Text("%s [ID %d]",ent.name.c_str(),ent.id);else if(ent.hasId)ImGui::Text("Entity [ID %d]",ent.id);else ImGui::Text("Entity %d",selectedEntity_-z.cameraCount);ImGui::Separator();if(ent.hasGoolType&&ent.hasSubtype){const auto goolEid=static_cast<std::uint32_t>(ent.goolType);if(Crash::isValidEid(goolEid))ImGui::Text("GOOL: %s (%08X) | Subtype: %d",Crash::eidToEName(goolEid).c_str(),goolEid,ent.subtype);else ImGui::Text("GOOL: %08X | Subtype: %d",goolEid,ent.subtype);}else ImGui::TextDisabled("GOOL Type/Subtype not defined");if(ent.hasId){int v=ent.id;if(ImGui::InputInt("Object ID",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x009fU,0,v))entityDataChanged_=true;}}if(ent.hasGoolType){int v=ent.goolType;if(ImGui::InputInt("GOOL Type / EID",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a9U,0,v))entityDataChanged_=true;}}if(ent.hasSubtype){int v=ent.subtype;if(ImGui::InputInt("GOOL Subtype",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00aaU,0,v))entityDataChanged_=true;}}if(ent.hasAlternateId)ImGui::Text("Alternate ID: %d",ent.alternateId);if(!ent.positions.empty()){ImGui::Text("Positions: %zu",ent.positions.size());for(std::size_t pi=0;pi<ent.positions.size();++pi){const auto&pos=ent.positions[pi];ImGui::BulletText("[%zu] X %d  Y %d  Z %d",pi,pos.x,pos.y,pos.z);}}if(!ent.settings.empty()){ImGui::Separator();ImGui::Text("GOOL Arguments / Settings (%zu)",ent.settings.size());if(ImGui::BeginTable("GoolSettings",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)){ImGui::TableSetupColumn("#");ImGui::TableSetupColumn("A (8-bit)");ImGui::TableSetupColumn("B (signed 24-bit)");ImGui::TableHeadersRow();for(std::size_t si=0;si<ent.settings.size();++si){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%zu",si);ImGui::TableSetColumnIndex(1);int argA=ent.settings[si].valueA;if(ImGui::InputInt(("##argA"+std::to_string(si)).c_str(),&argA)){argA=std::clamp(argA,0,255);const auto raw=(static_cast<std::uint32_t>(ent.settings[si].valueB)<<8)|(static_cast<std::uint32_t>(argA)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a4U,si,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}ImGui::TableSetColumnIndex(2);int argB=ent.settings[si].valueB;if(ImGui::InputInt(("##argB"+std::to_string(si)).c_str(),&argB)){argB=std::clamp(argB,-8388608,8388607);const auto raw=(static_cast<std::uint32_t>(argB)<<8)|(static_cast<std::uint32_t>(ent.settings[si].valueA)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x00a4U,si,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}ImGui::EndTable();}}if(!ent.victims.empty()){ImGui::Separator();ImGui::Text("Victim List (%zu)",ent.victims.size());for(std::size_t vi=0;vi<ent.victims.size();++vi){int victim=ent.victims[vi];ImGui::PushID(static_cast<int>(vi));if(ImGui::InputInt("Object ID",&victim)){victim=std::clamp(victim,-32768,32767);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0287U,vi,victim))entityDataChanged_=true;}ImGui::PopID();}}if(ent.hasBoxCount){int v=ent.boxCount;if(ImGui::InputInt("Box Count (Willy)",&v)){const auto raw=(static_cast<std::uint32_t>(v)<<8)|(static_cast<std::uint32_t>(ent.properties.empty()?0:0)&0xffU);if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x028bU,0,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}if(ent.hasDdaSettings){int v=ent.ddaDeathCount;if(ImGui::InputInt("DDA Death / 1-Up Count",&v)){const auto raw=static_cast<std::uint32_t>(v)<<8;if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0277U,0,static_cast<std::int32_t>(raw)))entityDataChanged_=true;}}if(ent.hasDdaSection){int v=ent.ddaSection;if(ImGui::InputInt("DDA Section",&v)){if(nsf_.setEntityPropertyInt(static_cast<std::size_t>(selectedChunk_),static_cast<std::size_t>(selectedEntry_),static_cast<std::size_t>(selectedEntity_),0x0288U,0,v))entityDataChanged_=true;}}}ImGui::Text("Declared length: %u | Properties: %zu",ent.declaredLength,ent.properties.size());if(!ent.error.empty())ImGui::TextWrapped("%s",ent.error.c_str());if(ImGui::BeginTable("EntityPropertyTable",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp)){ImGui::TableSetupColumn("Property");ImGui::TableSetupColumn("ID");ImGui::TableSetupColumn("Type");ImGui::TableSetupColumn("Rows");ImGui::TableSetupColumn("Decoded values");ImGui::TableHeadersRow();for(const auto&prop:ent.properties){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(entityPropertyName(static_cast<std::uint16_t>(prop.id)));ImGui::TableSetColumnIndex(1);ImGui::Text("0x%03X",static_cast<unsigned>(static_cast<std::uint16_t>(prop.id)));ImGui::TableSetColumnIndex(2);ImGui::Text("%u / %u B",prop.type,prop.elementSize);ImGui::TableSetColumnIndex(3);ImGui::Text("%d",prop.rowCount);ImGui::TableSetColumnIndex(4);if(!prop.error.empty())ImGui::TextDisabled("%s",prop.error.c_str());else if(!prop.positions.empty()){for(const auto&v:prop.positions)ImGui::Text("%d, %d, %d",v.x,v.y,v.z);}else if(!prop.intValues.empty()){std::string vals;for(std::size_t vi=0;vi<std::min<std::size_t>(prop.intValues.size(),8);++vi){if(vi)vals+=", ";vals+=std::to_string(prop.intValues[vi]);}if(prop.intValues.size()>8)vals+=" ...";ImGui::TextWrapped("%s",vals.c_str());}else ImGui::TextDisabled("%zu bytes (raw)",prop.dataSize);}ImGui::EndTable();}}}}else ImGui::TextWrapped("Entry could not be decoded: %s",e.error.c_str());}}else ImGui::TextDisabled("Specialized resource decoding is pending.");}else ImGui::TextDisabled("Select an NSD, spawn point, or NSF chunk.");ImGui::End();}if(entityDataChanged_){nsf_.decodeZones(gameVersion_==GameVersion::Crash3);entityDataChanged_=false;}if(assets_){ImGui::Begin("Asset Browser",&assets_);ImGui::TextDisabled("Texture previews, sound entries, and wavebank instruments will appear here after typed resource decoding.");ImGui::End();}}
}
