#pragma once
#include "core/workspace.hpp"
#include "crash/formats/nsf/nsf_archive.hpp"
#include "crash/formats/nsd/nsd_file.hpp"
#include <string>
namespace CrashEdit {
enum class GameVersion { Crash1Beta1995, Crash1BetaMAR08, Crash1BetaMAY11, Crash1, Crash2, Crash3BetaMAY14, Crash3 };
class EditorUI {
public:
 explicit EditorUI(Workspace& w):workspace_(w){} void draw();
private:
 void drawMenu(); void drawToolbar(); void drawHierarchy(); void drawViewport(); void drawBottom();
 void drawNode(const ResourceNode&); void openVersionDialog(); bool loadLevel(const std::filesystem::path&,std::string&);
 Workspace& workspace_;
 bool openDialog_=false,versionDialog_=false,about_=false,properties_=true,console_=true,assets_=true,entityDataChanged_=false;
 float sceneYaw_=0.65f,scenePitch_=0.55f,sceneZoom_=1.0f;
 char path_[1024]{};
 std::filesystem::path nsfPath_,nsdPath_;
 int selectedChunk_=-1,selectedSpawn_=-1,selectedEntry_=-1,selectedEntity_=-1;
 std::string status_="Ready — open a Crash 2 level archive.";
 GameVersion pendingVersion_=GameVersion::Crash2,gameVersion_=GameVersion::Crash2;
 Crash::NsfArchive nsf_; Crash::NsdFile nsd_; bool nsfValid_=false,nsdValid_=false;
};
}
