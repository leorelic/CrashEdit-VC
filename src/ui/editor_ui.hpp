#pragma once
#include "core/workspace.hpp"
#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/nsd/nsd_file.hpp"
#include <string>
namespace CrashEdit {
enum class GameVersion { Crash1Beta1995, Crash1BetaMAR08, Crash1BetaMAY11, Crash1, Crash2, Crash3BetaMAY14, Crash3 };
class EditorUI {
public:
 explicit EditorUI(Workspace& w); void draw();
 void handleDroppedFiles(int count,const char** paths);
private:
 void drawMenu(); void drawToolbar(); void drawHierarchy(); void drawResourceActionDialog(); void drawViewport(); void drawBottom(); void drawGoolDataWindows();
 void drawNode(const ResourceNode&); void drawSearchWindow(); void drawGoolInspector(); void drawGoolGraph(); void drawNsdGoolMap(); void drawTextureViewer(); void drawClutEditor(); void drawSpawnPointsEditor(); void openVersionDialog(); bool loadLevel(const std::filesystem::path&,std::string&); bool saveFiles();
 Workspace& workspace_;
 bool openDialog_=false,searchWindow_=false,nsdGoolMapWindow_=false,versionDialog_=false,centerOpenDialog_=false,centerVersionDialog_=false,about_=false,assets_=true,textureViewer_=true,clutEditor_=true,spawnPointsEditor_=true,entityDataChanged_=false,openSelectedHierarchy_=false,restoredLastNsfPath_=false;
 bool showGoolDataPool_=false,showGoolStateMap_=false,showGoolStateDescriptions_=false,showGoolFrameGroups_=false,focusGoolDataPool_=false;
 int activeGoolChunk_=-1,activeGoolEntry_=-1;
 float sceneYaw_=0.0f,scenePitch_=0.261799f,sceneDistance_=20.0f;
 float sceneTargetX_=0.0f,sceneTargetY_=0.0f,sceneTargetZ_=0.0f;
 bool sceneCameraInitialized_=false;
 int textureFormat_=1,textureColorMode_=0,textureBlendMode_=0;
 int textureX_=0,textureY_=0,textureW_=64,textureH_=64,textureClutX_=0,textureClutY_=0;
 float textureZoom_=1.0f;
 int sceneCameraChunk_=-1,sceneCameraEntry_=-1;
 char path_[1024]{}; char searchQuery_[256]{}; int searchTypeFilter_=-1;
 std::filesystem::path nsfPath_,nsdPath_;
 int selectedChunk_=-1,selectedSpawn_=-1,selectedEntry_=-1,selectedEntity_=-1,selectedItem_=-1;
 int resourceAction_=0,resourceTargetKind_=0,resourceTargetChunk_=-1,resourceTargetEntry_=-1,resourceTargetItem_=-1;
 char resourceActionPath_[1024]{};
 int viewerChunk_=-1,viewerEntry_=-1;
 std::string status_="Ready — open a Crash 2 level archive.";
 GameVersion pendingVersion_=GameVersion::Crash2,gameVersion_=GameVersion::Crash2;
 Crash::NsfArchive nsf_; Crash::NsdFile nsd_; bool nsfValid_=false,nsdValid_=false;
};
}
